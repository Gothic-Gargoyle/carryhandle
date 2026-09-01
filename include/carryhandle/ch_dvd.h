#ifndef CARRYHANDLE_CH_DVD_H
#define CARRYHANDLE_CH_DVD_H

#include <stdbool.h>


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
