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
