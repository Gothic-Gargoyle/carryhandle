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
