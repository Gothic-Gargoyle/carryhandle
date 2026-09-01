#ifndef CARRYHANDLE_CH_TX_H
#define CARRYHANDLE_CH_TX_H

#include <stddef.h>
#include <stdint.h>

#include <carryhandle/ch_tx_backend.h>
#include <carryhandle/ch_tx_format.h>


typedef enum CH_TxResult
{
    CH_TX_RESULT_OK = 0,

    CH_TX_RESULT_INVALID_ARGUMENT = -1,
    CH_TX_RESULT_IO = -2,
    CH_TX_RESULT_CORRUPT = -3,
    CH_TX_RESULT_AMBIGUOUS = -4

} CH_TxResult;


/*
 * Read both A/B superblocks and select the authoritative committed state.
 *
 * Rules:
 *
 *   one structurally valid copy
 *       -> that copy wins
 *
 *   both valid, different generations
 *       -> newest generation wins using wrap-aware ordering
 *
 *   both valid, same generation, equivalent state
 *       -> A wins deterministically
 *
 *   both valid, same generation, conflicting state
 *       -> CH_TX_RESULT_AMBIGUOUS
 *
 *   generation difference exactly 0x80000000
 *       -> CH_TX_RESULT_AMBIGUOUS
 *
 *   neither copy structurally valid
 *       -> CH_TX_RESULT_CORRUPT
 *
 * A backend read failure is reported as CH_TX_RESULT_IO. It is not
 * treated as merely an invalid copy, because doing so could silently
 * select an older committed generation.
 *
 * sector_buffer must hold one complete backend sector and satisfy the
 * backend's alignment requirement.
 */
CH_TxResult CH_TxReadAuthoritativeSuperblock(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    CH_TxSuperblock *superblock,
    uint32_t *superblock_sector
);


#endif
