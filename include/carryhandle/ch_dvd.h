#ifndef CARRYHANDLE_CH_DVD_H
#define CARRYHANDLE_CH_DVD_H

#include <stdbool.h>

#include <carryhandle/ch_remote_disc.h>


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Mount the native GameCube disc FST as the standard:
 *
 *     dvd:/
 *
 * newlib filesystem.
 *
 * After a successful mount, ordinary stdio/POSIX-style operations such as
 * fopen(), open(), stat(), fread() and fseek() can access files contained
 * in the GameCube disc filesystem.
 *
 * The implementation performs physical DVD reads with the alignment
 * required by the GameCube DVD interface.
 *
 * Repeated mounting of an already-mounted filesystem is accepted.
 */
bool CH_DVDMount(void);


/*
 * Mount dvd:/ using an already-open CarryHandle remote-disc session.
 *
 * The remote GameCube image is queried for its own FST location and size;
 * the FST is then downloaded into CarryHandle-owned RAM and used by the
 * same dvd:/ devoptab as a normal physical disc.
 *
 * File reads are serviced through CH_RemoteDiscRead().
 *
 * The caller retains ownership of session. CH_DVDUnmount() releases only
 * the downloaded FST and dvd:/ state; it does not close the session.
 *
 * session must remain open for the complete mounted lifetime.
 */
bool CH_DVDMountRemote(
    CH_RemoteDiscSession *session
);

/*
 * Physical SD interfaces available for development dvd:/ backing.
 *
 * This mode deliberately exposes one selected directory on the FAT filesystem
 * as dvd:/. Applications continue using their normal release paths such as:
 *
 *     dvd:/launcher/foo.bmp
 *     dvd:/data/wad/game.wad
 *
 * The same staged filesystem tree can therefore be used for both an SD
 * development run and a native GCM/FST image.
 */
typedef enum CH_DVD_SDDevice
{
    CH_DVD_SD_SLOT_A = 0,
    CH_DVD_SD_SLOT_B = 1,
    CH_DVD_SD_SP2 = 2
} CH_DVD_SDDevice;


/*
 * Mount one directory on the selected FAT-formatted SD device as dvd:/.
 *
 * For example:
 *
 *     CH_DVDMountSDDirectory(
 *         CH_DVD_SD_SLOT_A,
 *         "/doomcube-files");
 *
 * makes:
 *
 *     dvd:/launcher/foo.bmp
 *
 * resolve to:
 *
 *     <SD>:/doomcube-files/launcher/foo.bmp
 *
 * The directory must already exist.
 *
 * CH_DVDUnmount() releases both the dvd:/ alias and CarryHandle's private
 * FAT mount.
 */
bool CH_DVDMountSDDirectory(
    CH_DVD_SDDevice device,
    const char *directory
);



/*
 * Remove CarryHandle's dvd:/ filesystem registration and release any
 * runtime state owned by CH_DVDMount().
 *
 * Safe to call when the filesystem is not mounted.
 */
void CH_DVDUnmount(void);


#ifdef __cplusplus
}
#endif

#endif
