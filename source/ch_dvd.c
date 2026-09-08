/*
 * CarryHandle native GameCube DVD/FST filesystem.
 *
 * Extracted and generalized from a production-proven GameCube
 * implementation. The filesystem behavior remains intentionally
 * GameCube-specific while application policy stays with the consumer.
 */

#include <carryhandle/ch_dvd.h>

#include <gccore.h>
#include <ogc/dvd.h>
#include <fat.h>
#include <gctypes.h>
#include <sdcard/gcsd.h>
#include <sys/iosupport.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>


#define CH_DVD_FST_ENTRY_SIZE 12u


typedef struct
{
    uint32_t offset;
    uint32_t size;
    uint32_t pos;
} gc_dvd_file_t;


typedef struct
{
    uint32_t type_name;
    uint32_t word1;
    uint32_t word2;
} gc_fst_entry_t;


static const gc_fst_entry_t *gc_fst;
static const char *gc_fst_strings;
static uint32_t gc_fst_count;
static bool gc_fst_mounted;


/*
 * Unaligned DVD reads must not depend on transient heap availability.
 * Keep a small permanently aligned bounce buffer and process larger
 * requests in chunks.
 */
#define CH_DVD_BOUNCE_SIZE 8192u

static uint8_t gc_dvd_bounce[CH_DVD_BOUNCE_SIZE]
    __attribute__((aligned(32)));

/*
 * CH_DVD_IMAGE_BACKING
 *
 * Swiss supplies libogc2 applications an external launch path in argv[0].
 * If it names a GameCube image on a native SD interface, use that image as
 * the backing store for the in-memory FST instead of the optical drive.
 */
static FILE *gc_dvd_image;
static const char *gc_dvd_image_mount_name;
static bool gc_dvd_image_mount_owned;

enum {
    CH_DVD_BACKING_PHYSICAL = 0,
    CH_DVD_BACKING_IMAGE = 1,
    CH_DVD_BACKING_ERROR = -1
};

static bool CH_DVD_PathLooksLikeImage(const char *path)
{
    const char *ext;

    if (path == NULL)
        return false;

    ext = strrchr(path, '.');
    if (ext == NULL)
        return false;

    return strcasecmp(ext, ".iso") == 0 ||
        strcasecmp(ext, ".gcm") == 0;
}

static DISC_INTERFACE *CH_DVD_InterfaceForPath(
    const char *path,
    const char **mount_name)
{
    if (path == NULL || mount_name == NULL)
        return NULL;

    if (strncmp(path, "sd:/", 4) == 0) {
        *mount_name = "sd";
        return get_io_gcsd2();
    }

    if (strncmp(path, "carda:/", 7) == 0) {
        *mount_name = "carda";
        return get_io_gcsda();
    }

    if (strncmp(path, "cardb:/", 7) == 0) {
        *mount_name = "cardb";
        return get_io_gcsdb();
    }

    return NULL;
}

static void CH_DVD_CloseImageBacking(void)
{
    if (gc_dvd_image != NULL) {
        fclose(gc_dvd_image);
        gc_dvd_image = NULL;
    }

    if (gc_dvd_image_mount_owned &&
        gc_dvd_image_mount_name != NULL) {
        fatUnmount(gc_dvd_image_mount_name);
    }

    gc_dvd_image_mount_name = NULL;
    gc_dvd_image_mount_owned = false;
}

static int CH_DVD_TryLaunchImage(void)
{
    const char *path;
    const char *mount_name = NULL;
    DISC_INTERFACE *iface;
    char device_name[10];
    uint8_t header[32];

    if (__system_argv == NULL ||
        __system_argv->argvMagic != ARGV_MAGIC ||
        __system_argv->argc < 1 ||
        __system_argv->argv == NULL ||
        __system_argv->argv[0] == NULL) {
        return CH_DVD_BACKING_PHYSICAL;
    }

    path = __system_argv->argv[0];

    if (!CH_DVD_PathLooksLikeImage(path))
        return CH_DVD_BACKING_PHYSICAL;

    iface = CH_DVD_InterfaceForPath(path, &mount_name);

    /*
     * An image path on an unsupported source must fail closed. Falling
     * through to physical DI would recreate the mixed-FST corruption bug.
     */
    if (iface == NULL || mount_name == NULL)
        return CH_DVD_BACKING_ERROR;

    snprintf(device_name, sizeof(device_name), "%s:", mount_name);

    if (FindDevice(device_name) < 0) {
        if (!fatMountSimple(mount_name, iface))
            return CH_DVD_BACKING_ERROR;
        gc_dvd_image_mount_owned = true;
    }

    gc_dvd_image_mount_name = mount_name;
    gc_dvd_image = fopen(path, "rb");

    if (gc_dvd_image == NULL) {
        CH_DVD_CloseImageBacking();
        return CH_DVD_BACKING_ERROR;
    }

    if (fread(header, 1, sizeof(header), gc_dvd_image) != sizeof(header)) {
        CH_DVD_CloseImageBacking();
        return CH_DVD_BACKING_ERROR;
    }

    /*
     * Swiss copies the launched game's header to 0x80000000. Require the
     * image's six-byte Game ID to match it, plus the GameCube disc magic.
     */
    if (memcmp(header, (const void *)(uintptr_t)0x80000000, 6) != 0 ||
        header[0x1c] != 0xc2 ||
        header[0x1d] != 0x33 ||
        header[0x1e] != 0x9f ||
        header[0x1f] != 0x3d) {
        CH_DVD_CloseImageBacking();
        return CH_DVD_BACKING_ERROR;
    }

    rewind(gc_dvd_image);
    return CH_DVD_BACKING_IMAGE;
}

static s32 CH_DVD_ReadAbsBacking(
    dvdcmdblk *block,
    void *buf,
    u32 len,
    s64 offset,
    s32 prio)
{
    size_t got;

    if (gc_dvd_image == NULL)
        return DVD_ReadAbsPrio(block, buf, len, offset, prio);

    (void)block;
    (void)prio;

    if (offset < 0 || offset > LONG_MAX)
        return -1;

    if (fseek(gc_dvd_image, (long)offset, SEEK_SET) != 0)
        return -1;

    got = fread(buf, 1, len, gc_dvd_image);
    if (got != len)
        return -1;

    return (s32)got;
}




/*
 * GameCube low memory:
 *
 * 0x80000038 = FST pointer
 * 0x8000003c = FST maximum size
 *
 * The GameCube apploader fills these before jumping to the application.
 */
static const gc_fst_entry_t *CH_DVD_GetFST(void)
{
    volatile uint32_t *fst_ptr =
        (volatile uint32_t *)0x80000038;

    return (const gc_fst_entry_t *)(uintptr_t)(*fst_ptr);
}


static uint32_t CH_DVD_FST_TypeName(
    const gc_fst_entry_t *e)
{
    return e->type_name;
}


static bool CH_DVD_FST_IsDir(
    const gc_fst_entry_t *e)
{
    return (CH_DVD_FST_TypeName(e) & 0x01000000u) != 0;
}


static uint32_t CH_DVD_FST_NameOffset(
    const gc_fst_entry_t *e)
{
    return CH_DVD_FST_TypeName(e) & 0x00ffffffu;
}


static const char *CH_DVD_FST_Name(
    const gc_fst_entry_t *e)
{
    return gc_fst_strings +
        CH_DVD_FST_NameOffset(e);
}


static const char *CH_DVD_SkipDevicePrefix(
    const char *path)
{
    if (path == NULL)
        return NULL;

    if (strncmp(path, "dvd:", 4) == 0)
        path += 4;

    while (*path == '/')
        ++path;

    return path;
}


static bool CH_DVD_ComponentMatches(
    const char *component,
    size_t component_len,
    const char *name)
{
    size_t name_len;

    if (name == NULL)
        return false;

    name_len = strlen(name);

    if (name_len != component_len)
        return false;

    return strncasecmp(
        component,
        name,
        component_len) == 0;
}


static int CH_DVD_FindChild(
    uint32_t parent_index,
    const char *component,
    size_t component_len)
{
    const gc_fst_entry_t *parent;
    uint32_t i;
    uint32_t end;

    if (parent_index >= gc_fst_count)
        return -1;

    parent = &gc_fst[parent_index];

    if (!CH_DVD_FST_IsDir(parent))
        return -1;

    end = parent->word2;

    i = parent_index + 1;

    while (i < end && i < gc_fst_count)
    {
        const gc_fst_entry_t *e =
            &gc_fst[i];

        if (CH_DVD_ComponentMatches(
                component,
                component_len,
                CH_DVD_FST_Name(e)))
        {
            return (int)i;
        }

        /*
         * If this is a directory, word2 points to the
         * entry immediately following its descendants.
         */
        if (CH_DVD_FST_IsDir(e))
            i = e->word2;
        else
            ++i;
    }

    return -1;
}


static int CH_DVD_ResolvePath(
    const char *path)
{
    const char *p;
    uint32_t current = 0;

    p = CH_DVD_SkipDevicePrefix(path);

    if (p == NULL)
        return -1;

    if (*p == '\0')
        return 0;

    while (*p != '\0')
    {
        const char *slash;
        size_t len;
        int next;

        slash = strchr(p, '/');

        if (slash != NULL)
            len = (size_t)(slash - p);
        else
            len = strlen(p);

        if (len == 0)
        {
            if (slash == NULL)
                break;

            p = slash + 1;
            continue;
        }

        next = CH_DVD_FindChild(
            current,
            p,
            len);

        if (next < 0)
            return -1;

        current = (uint32_t)next;

        if (slash == NULL)
            break;

        p = slash + 1;
    }

    return (int)current;
}


static int CH_DVD_Open(
    struct _reent *r,
    void *fileStruct,
    const char *path,
    int flags,
    int mode)
{
    gc_dvd_file_t *f =
        (gc_dvd_file_t *)fileStruct;

    int index;

    (void)mode;

    if ((flags & O_ACCMODE) != O_RDONLY)
    {
        r->_errno = EROFS;
        return -1;
    }

    index = CH_DVD_ResolvePath(path);

    if (index < 0)
    {
        r->_errno = ENOENT;
        return -1;
    }

    if (CH_DVD_FST_IsDir(&gc_fst[index]))
    {
        r->_errno = EISDIR;
        return -1;
    }

    f->offset =
        gc_fst[index].word1;

    f->size =
        gc_fst[index].word2;

    f->pos = 0;

    return 0;
}


static int CH_DVD_Close(
    struct _reent *r,
    void *fd)
{
    (void)r;
    (void)fd;

    return 0;
}


static ssize_t CH_DVD_Read(
    struct _reent *r,
    void *fd,
    char *ptr,
    size_t len)
{
    gc_dvd_file_t *f =
        (gc_dvd_file_t *)fd;

    dvdcmdblk block;

    uint32_t remaining;
    uint32_t amount;

    s32 rc;

    if (f->pos >= f->size)
        return 0;

    remaining =
        f->size - f->pos;

    amount =
        len < remaining
            ? (uint32_t)len
            : remaining;

    if (amount == 0)
        return 0;

    /*
     * DVD DMA requires 32-byte alignment for the destination,
     * transfer length, AND physical disc offset.
     *
     * stdio may turn a tiny fread() into an aligned internal-buffer
     * refill. Therefore destination/length alignment alone is not
     * sufficient: an unaligned logical seek must still use the
     * bounce-buffer path.
     */
    if ((((uintptr_t)ptr) & 31u) == 0 &&
        (amount & 31u) == 0 &&
        (((f->offset + f->pos) & 31u) == 0))
    {
        rc = CH_DVD_ReadAbsBacking(
            &block,
            ptr,
            amount,
            (s64)f->offset + f->pos,
            2);

        if (rc < 0)
        {
            r->_errno = EIO;
            return -1;
        }
    }
    else
    {
        uint32_t copied = 0;

        while (copied < amount)
        {
            uint32_t absolute =
                f->offset + f->pos + copied;

            uint32_t first =
                absolute & ~31u;

            uint32_t skip =
                absolute - first;

            uint32_t chunk =
                amount - copied;

            uint32_t capacity =
                CH_DVD_BOUNCE_SIZE - skip;

            uint32_t read_len;

            if (chunk > capacity)
                chunk = capacity;

            read_len =
                (skip + chunk + 31u) & ~31u;

            rc = CH_DVD_ReadAbsBacking(
                &block,
                gc_dvd_bounce,
                read_len,
                first,
                2);

            if (rc < 0)
            {
                r->_errno = EIO;
                return -1;
            }

            memcpy(
                ptr + copied,
                gc_dvd_bounce + skip,
                chunk);

            copied += chunk;
        }
    }

    f->pos += amount;

    return (ssize_t)amount;
}


static off_t CH_DVD_Seek(
    struct _reent *r,
    void *fd,
    off_t pos,
    int dir)
{
    gc_dvd_file_t *f =
        (gc_dvd_file_t *)fd;

    int64_t next;

    switch (dir)
    {
        case SEEK_SET:
            next = pos;
            break;

        case SEEK_CUR:
            next =
                (int64_t)f->pos + pos;
            break;

        case SEEK_END:
            next =
                (int64_t)f->size + pos;
            break;

        default:
            r->_errno = EINVAL;
            return (off_t)-1;
    }

    if (next < 0 ||
        next > (int64_t)f->size)
    {
        r->_errno = EINVAL;
        return (off_t)-1;
    }

    f->pos = (uint32_t)next;

    return (off_t)f->pos;
}


static int CH_DVD_Fstat(
    struct _reent *r,
    void *fd,
    struct stat *st)
{
    gc_dvd_file_t *f =
        (gc_dvd_file_t *)fd;

    (void)r;

    memset(st, 0, sizeof(*st));

    st->st_mode =
        S_IFREG | 0444;

    st->st_size =
        f->size;

    return 0;
}


static int CH_DVD_Stat(
    struct _reent *r,
    const char *path,
    struct stat *st)
{
    int index =
        CH_DVD_ResolvePath(path);

    if (index < 0)
    {
        r->_errno = ENOENT;
        return -1;
    }

    memset(st, 0, sizeof(*st));

    if (CH_DVD_FST_IsDir(&gc_fst[index]))
    {
        st->st_mode =
            S_IFDIR | 0555;

        st->st_size = 0;
    }
    else
    {
        st->st_mode =
            S_IFREG | 0444;

        st->st_size =
            gc_fst[index].word2;
    }

    return 0;
}


static const devoptab_t gc_dvd_devoptab =
{
    .name       = "dvd",
    .structSize = sizeof(gc_dvd_file_t),

    .open_r     = CH_DVD_Open,
    .close_r    = CH_DVD_Close,
    .read_r     = CH_DVD_Read,
    .seek_r     = CH_DVD_Seek,

    .fstat_r    = CH_DVD_Fstat,
    .stat_r     = CH_DVD_Stat,
};


bool CH_DVDMount(void)
{
    const gc_fst_entry_t *root;
    uint32_t count;

    if (gc_fst_mounted)
        return true;

    gc_fst = CH_DVD_GetFST();

    if (gc_fst == NULL)
        return false;

    root = &gc_fst[0];

    if (!CH_DVD_FST_IsDir(root))
        return false;

    count = root->word2;

    if (count == 0 ||
        count > 100000u)
    {
        return false;
    }

    gc_fst_count = count;

    gc_fst_strings =
        (const char *)gc_fst +
        ((size_t)gc_fst_count *
         CH_DVD_FST_ENTRY_SIZE);

    {
        int backing = CH_DVD_TryLaunchImage();

        if (backing == CH_DVD_BACKING_ERROR)
            return false;

        if (backing == CH_DVD_BACKING_PHYSICAL) {
            DVD_Init();
            DVD_Mount();
        }
    }

    if (AddDevice(&gc_dvd_devoptab) < 0)
        return false;

    gc_fst_mounted = true;

    return true;
}


void CH_DVDUnmount(void)
{
    if (!gc_fst_mounted)
        return;

    RemoveDevice("dvd:");

    CH_DVD_CloseImageBacking();

    gc_fst = NULL;
    gc_fst_strings = NULL;
    gc_fst_count = 0;

    gc_fst_mounted = false;
}
