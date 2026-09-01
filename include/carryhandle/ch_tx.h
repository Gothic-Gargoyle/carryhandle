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
    CH_TX_RESULT_AMBIGUOUS = -4,
    CH_TX_RESULT_BUFFER_TOO_SMALL = -5,

    /*
     * An I/O failure occurred after publication of new authoritative
     * state began. The caller must not assume whether the transaction
     * committed; recover transaction state before performing more writes.
     */
    CH_TX_RESULT_COMMIT_UNCERTAIN = -6

} CH_TxResult;


/*
 * Read and validate one record from the committed log.
 *
 * read_limit_sector is one sector past the last sector visible to the
 * caller. For committed records this should normally be the authoritative
 * superblock's log_end_sector.
 *
 * Validation includes:
 *
 *   - encoded record-header CRC and semantics
 *   - record sector geometry
 *   - record containment within read_limit_sector
 *   - exact body CRC over scope || key || stored payload
 *
 * scope_output, key_output and stored_output are independently optional.
 * A NULL output requires a zero capacity. A non-NULL output must have
 * enough capacity for the corresponding size reported in the header.
 *
 * Payload decompression and raw_crc32 validation are deliberately outside
 * this primitive.
 */
CH_TxResult CH_TxReadRecord(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    uint32_t read_limit_sector,
    uint32_t record_sector,
    CH_TxRecordHeader *record,
    void *scope_output,
    size_t scope_capacity,
    void *key_output,
    size_t key_capacity,
    void *stored_output,
    size_t stored_capacity
);


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
