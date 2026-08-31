#ifndef CARRYHANDLE_CH_MEMCARD_H
#define CARRYHANDLE_CH_MEMCARD_H

#include <stdbool.h>

#include <ogc/card.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct CH_MemCardSession
{
    s32 slot;
    s32 memory_size;
    s32 sector_size;
    bool mounted;
} CH_MemCardSession;


/*
 * Initialise libogc2 CARD identity and mount one Memory Card.
 *
 * work_area must:
 *   - be at least CARD_WORKAREA bytes
 *   - be 32-byte aligned
 *
 * game_code and company_code are normally supplied by
 * CH_ApplicationInfo.
 */
bool CH_MemCardMount(
    CH_MemCardSession *session,
    s32 slot,
    const char *game_code,
    const char *company_code,
    void *work_area
);


/*
 * CarryHandle-specific errors occupy a range outside libogc2 CARD
 * error codes. Otherwise CH_MemCardOpen/Create return CARD_* results
 * unchanged.
 */
#define CH_MEMCARD_ERROR_INVALID_ARGUMENT ((s32)-1000)
#define CH_MEMCARD_ERROR_NOT_MOUNTED      ((s32)-1001)


/*
 * Open an existing file on a mounted Memory Card.
 *
 * Returns CARD_ERROR_READY on success, CARD_ERROR_NOFILE when the
 * file does not exist, another CARD_* error from libogc2, or one of
 * the CH_MEMCARD_ERROR_* values above.
 */
s32 CH_MemCardOpen(
    const CH_MemCardSession *session,
    const char *filename,
    card_file *file
);


/*
 * Create a new file and return it opened for immediate use.
 *
 * size must be a non-zero multiple of session->sector_size.
 */
s32 CH_MemCardCreate(
    const CH_MemCardSession *session,
    const char *filename,
    u32 size,
    card_file *file
);


/*
 * Close an opened Memory Card file.
 *
 * Returns CARD_ERROR_READY on success or the underlying CARD_* error.
 */
s32 CH_MemCardClose(
    card_file *file
);


/*
 * Unmount a session previously opened by CH_MemCardMount().
 *
 * Safe to call on an already-unmounted session.
 */
bool CH_MemCardUnmount(
    CH_MemCardSession *session
);


#ifdef __cplusplus
}
#endif

#endif
