#include <carryhandle/ch_remote_disc.h>

#include <stddef.h>
#include <ogc/usbgecko.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "proof_expected_fst.h"


#define PROOF_GECKO_CHANNEL 1

/*
 * DoomCube's current FST is only ~7.5 KiB. Keep this proof deliberately
 * simple and bounded. A production bootstrap can allocate exactly the
 * discovered FST size later.
 */
#define PROOF_FST_BUFFER_SIZE (64u * 1024u)


static uint8_t proof_fst[PROOF_FST_BUFFER_SIZE];
static uint8_t proof_prefix[64];


static uint32_t get_be32(
    const uint8_t *src)
{
    return ((uint32_t)src[0] << 24)
        | ((uint32_t)src[1] << 16)
        | ((uint32_t)src[2] << 8)
        | ((uint32_t)src[3]);
}


static void put_be32(
    uint8_t *dst,
    uint32_t value)
{
    dst[0] = (uint8_t)(value >> 24);
    dst[1] = (uint8_t)(value >> 16);
    dst[2] = (uint8_t)(value >> 8);
    dst[3] = (uint8_t)value;
}


static uint32_t crc32_update(
    uint32_t crc,
    const uint8_t *data,
    uint32_t length)
{
    uint32_t i;

    for (i = 0; i < length; ++i) {
        uint32_t bit;

        crc ^= data[i];

        for (bit = 0; bit < 8; ++bit) {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }

    return crc;
}


static bool proof_send_exact(
    const void *buffer,
    uint32_t length)
{
    const uint8_t *bytes =
        (const uint8_t *)buffer;

    uint32_t done = 0;

    while (done < length) {
        int result = usb_sendbuffer_safe(
            PROOF_GECKO_CHANNEL,
            bytes + done,
            (int)(length - done)
        );

        if (result <= 0)
            return false;

        done += (uint32_t)result;
    }

    return true;
}


static void halt_forever(void)
{
    for (;;) {
        __asm__ volatile("nop");
    }
}


static void proof_fail(
    uint32_t stage,
    uint32_t detail)
{
    uint8_t packet[16] = {
        'C','H','F','A','I','L','5','!',
        0,0,0,0,
        0,0,0,0
    };

    put_be32(packet + 8, stage);
    put_be32(packet + 12, detail);

    (void)proof_send_exact(
        packet,
        sizeof(packet)
    );

    halt_forever();
}


static const uint8_t *fst_entry(
    uint32_t index,
    uint32_t entry_count)
{
    if (index >= entry_count)
        return NULL;

    return proof_fst + (index * 12u);
}


static uint32_t fst_type_name(
    const uint8_t *entry)
{
    return get_be32(entry);
}


static uint32_t fst_word1(
    const uint8_t *entry)
{
    return get_be32(entry + 4);
}


static uint32_t fst_word2(
    const uint8_t *entry)
{
    return get_be32(entry + 8);
}


static bool fst_is_dir(
    const uint8_t *entry)
{
    return (
        fst_type_name(entry)
        & 0x01000000u
    ) != 0;
}


static uint32_t fst_name_offset(
    const uint8_t *entry)
{
    return (
        fst_type_name(entry)
        & 0x00FFFFFFu
    );
}


static bool fst_name_matches(
    const uint8_t *entry,
    uint32_t entry_count,
    uint32_t fst_size,
    const char *component,
    size_t component_length)
{
    uint32_t name_base =
        entry_count * 12u;

    uint32_t offset =
        fst_name_offset(entry);

    uint32_t position;

    const char *name;
    const char *nul;

    size_t remaining;
    size_t name_length;

    if (name_base > fst_size)
        return false;

    if (offset > fst_size - name_base)
        return false;

    position = name_base + offset;

    if (position >= fst_size)
        return false;

    name =
        (const char *)(proof_fst + position);

    remaining =
        (size_t)(fst_size - position);

    nul = (const char *)memchr(
        name,
        '\0',
        remaining
    );

    if (nul == NULL)
        return false;

    name_length =
        (size_t)(nul - name);

    return (
        name_length == component_length
        && memcmp(
            name,
            component,
            component_length
        ) == 0
    );
}


/*
 * Resolve one ordinary FST path without using CarryHandle's dvd:/ layer.
 *
 * Directory word2 is the first entry index after that directory's subtree,
 * allowing child-directory subtrees to be skipped while searching only
 * direct children.
 */
static int32_t fst_resolve_path(
    const char *path,
    uint32_t entry_count,
    uint32_t fst_size)
{
    const char *cursor = path;
    uint32_t parent = 0;

    if (path == NULL || *path == '\0')
        return -1;

    while (*cursor != '\0') {
        const char *component;
        size_t component_length;

        const uint8_t *parent_entry;

        uint32_t child;
        uint32_t end;

        int32_t found = -1;

        while (*cursor == '/')
            ++cursor;

        if (*cursor == '\0')
            break;

        component = cursor;

        while (
            *cursor != '\0'
            && *cursor != '/'
        ) {
            ++cursor;
        }

        component_length =
            (size_t)(cursor - component);

        if (component_length == 0)
            return -1;

        parent_entry =
            fst_entry(parent, entry_count);

        if (parent_entry == NULL ||
            !fst_is_dir(parent_entry)) {
            return -1;
        }

        end = fst_word2(parent_entry);

        if (end <= parent ||
            end > entry_count) {
            return -1;
        }

        child = parent + 1u;

        while (child < end) {
            const uint8_t *entry =
                fst_entry(child, entry_count);

            if (entry == NULL)
                return -1;

            if (fst_name_matches(
                    entry,
                    entry_count,
                    fst_size,
                    component,
                    component_length)) {
                found = (int32_t)child;
                break;
            }

            if (fst_is_dir(entry)) {
                uint32_t next =
                    fst_word2(entry);

                if (next <= child ||
                    next > end) {
                    return -1;
                }

                child = next;
            } else {
                ++child;
            }
        }

        if (found < 0)
            return -1;

        while (*cursor == '/')
            ++cursor;

        if (*cursor == '\0')
            return found;

        if (!fst_is_dir(
                fst_entry(
                    (uint32_t)found,
                    entry_count))) {
            return -1;
        }

        parent = (uint32_t)found;
    }

    return -1;
}


int main(void)
{
    CH_RemoteDiscSession session = {0};
    CH_RemoteDiscResult result;

    uint8_t boot_info[16];

    uint32_t dol_offset;
    uint32_t fst_offset;
    uint32_t fst_size;
    uint32_t fst_max_size;

    const uint8_t *root;

    uint32_t entry_count;
    uint32_t name_base;

    int32_t resolved_index;

    const uint8_t *resolved_entry;

    uint32_t resolved_offset;
    uint32_t resolved_size;

    uint32_t prefix_crc;

    /*
     * Preserve the already-proven wiiload -> PC-server handoff window.
     */
    usleep(3000000);

    result = CH_RemoteDiscOpen(
        &session,
        PROOF_GECKO_CHANNEL
    );

    if (result != CH_REMOTE_DISC_RESULT_OK)
        proof_fail(1, (uint32_t)(-result));

    /*
     * GameCube disc header:
     *
     *   0x420 = main DOL offset
     *   0x424 = FST offset
     *   0x428 = FST size
     *   0x42C = FST maximum size
     *
     * Crucially, all four values are discovered from the REMOTE image.
     */
    result = CH_RemoteDiscRead(
        &session,
        0x420u,
        boot_info,
        sizeof(boot_info)
    );

    if (result != CH_REMOTE_DISC_RESULT_OK)
        proof_fail(2, (uint32_t)(-result));

    dol_offset =
        get_be32(boot_info + 0);

    fst_offset =
        get_be32(boot_info + 4);

    fst_size =
        get_be32(boot_info + 8);

    fst_max_size =
        get_be32(boot_info + 12);

    /*
     * Independent local truth check.
     */
    if (dol_offset != PROOF_EXPECTED_DOL_OFFSET)
        proof_fail(3, dol_offset);

    if (fst_offset != PROOF_EXPECTED_FST_OFFSET)
        proof_fail(4, fst_offset);

    if (fst_size != PROOF_EXPECTED_FST_SIZE)
        proof_fail(5, fst_size);

    if (fst_max_size != PROOF_EXPECTED_FST_MAX_SIZE)
        proof_fail(6, fst_max_size);

    if (fst_size < 12u ||
        fst_size > PROOF_FST_BUFFER_SIZE) {
        proof_fail(7, fst_size);
    }

    /*
     * Bootstrap the entire filesystem table directly from the PC-hosted GCM.
     */
    result = CH_RemoteDiscRead(
        &session,
        fst_offset,
        proof_fst,
        fst_size
    );

    if (result != CH_REMOTE_DISC_RESULT_OK)
        proof_fail(8, (uint32_t)(-result));

    root = proof_fst;

    if (!fst_is_dir(root))
        proof_fail(9, fst_type_name(root));

    /*
     * Root entry:
     *
     * word1 = root parent field (normally zero)
     * word2 = total FST entry count
     */
    if (fst_word1(root) != 0u)
        proof_fail(10, fst_word1(root));

    entry_count =
        fst_word2(root);

    if (entry_count != PROOF_EXPECTED_ENTRY_COUNT)
        proof_fail(11, entry_count);

    if (entry_count == 0u)
        proof_fail(12, 0u);

    if (entry_count > UINT32_MAX / 12u)
        proof_fail(13, entry_count);

    name_base =
        entry_count * 12u;

    if (name_base > fst_size)
        proof_fail(14, name_base);

    /*
     * Resolve a real file path using only the REMOTELY LOADED FST.
     */
    resolved_index =
        fst_resolve_path(
            PROOF_TARGET_PATH,
            entry_count,
            fst_size
        );

    if (resolved_index < 0)
        proof_fail(15, 0u);

    if ((uint32_t)resolved_index !=
        PROOF_TARGET_ENTRY_INDEX) {
        proof_fail(
            16,
            (uint32_t)resolved_index
        );
    }

    resolved_entry =
        fst_entry(
            (uint32_t)resolved_index,
            entry_count
        );

    if (resolved_entry == NULL)
        proof_fail(17, 0u);

    if (fst_is_dir(resolved_entry))
        proof_fail(18, (uint32_t)resolved_index);

    resolved_offset =
        fst_word1(resolved_entry);

    resolved_size =
        fst_word2(resolved_entry);

    if (resolved_offset != PROOF_TARGET_OFFSET)
        proof_fail(19, resolved_offset);

    if (resolved_size != PROOF_TARGET_SIZE)
        proof_fail(20, resolved_size);

    if (PROOF_TARGET_PREFIX_LENGTH >
        sizeof(proof_prefix)) {
        proof_fail(
            21,
            PROOF_TARGET_PREFIX_LENGTH
        );
    }

    if (PROOF_TARGET_PREFIX_LENGTH >
        resolved_size) {
        proof_fail(
            22,
            PROOF_TARGET_PREFIX_LENGTH
        );
    }

    /*
     * Final usefulness proof:
     *
     * use the offset obtained FROM THE REMOTE FST to read the corresponding
     * file bytes from the same remote image.
     */
    result = CH_RemoteDiscRead(
        &session,
        resolved_offset,
        proof_prefix,
        PROOF_TARGET_PREFIX_LENGTH
    );

    if (result != CH_REMOTE_DISC_RESULT_OK)
        proof_fail(23, (uint32_t)(-result));

    prefix_crc =
        crc32_update(
            0xFFFFFFFFu,
            proof_prefix,
            PROOF_TARGET_PREFIX_LENGTH
        ) ^ 0xFFFFFFFFu;

    if (prefix_crc != PROOF_TARGET_PREFIX_CRC32)
        proof_fail(24, prefix_crc);

    CH_RemoteDiscClose(&session);

    if (CH_RemoteDiscIsOpen(&session))
        proof_fail(25, 0u);

    {
        static const uint8_t pass[8] = {
            'C','H','P','A','S','S','5','!'
        };

        (void)proof_send_exact(
            pass,
            sizeof(pass)
        );
    }

    halt_forever();

    return 0;
}
