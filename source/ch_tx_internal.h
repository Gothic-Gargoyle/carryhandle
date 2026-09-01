#ifndef CARRYHANDLE_CH_TX_INTERNAL_H
#define CARRYHANDLE_CH_TX_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include <carryhandle/ch_tx.h>


/*
 * Internal transaction mechanics.
 *
 * Write one complete record beginning at record_sector, make its bytes
 * durable with backend->sync(), then read the record back through the
 * normal transaction reader and verify its metadata and body CRC.
 *
 * This does NOT update either superblock. The record therefore remains
 * logically invisible until a later publish step advances log_end.
 *
 * record_template supplies operation, generation, flags, codec,
 * raw_size and raw_crc32. Physical body sizes, body CRC and
 * record_sectors are calculated here.
 */
CH_TxResult CH_TxWriteUncommittedRecord(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    uint32_t record_sector,
    const CH_TxRecordHeader *record_template,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    const void *stored_payload,
    size_t stored_size,
    CH_TxRecordHeader *written_record
);


/*
 * Publish a new committed log end through the inactive A/B superblock.
 *
 * authoritative and authoritative_sector describe the currently
 * authoritative state previously selected by
 * CH_TxReadAuthoritativeSuperblock().
 *
 * The new superblock:
 *
 *   - preserves container geometry, log_start and flags
 *   - increments generation modulo uint32_t
 *   - advances log_end_sector
 *   - is written to the inactive A/B sector
 *   - is definitely committed after backend->sync() succeeds
 *
 * Once publication I/O begins, a write or sync failure cannot generically
 * prove whether new authoritative state reached stable media. Such a
 * failure returns CH_TX_RESULT_COMMIT_UNCERTAIN. The caller must recover
 * transaction state before performing another write.
 *
 * No post-sync read is performed here. A successful sync is the definite
 * commit point; returning a later read error would make commit status
 * unnecessarily ambiguous.
 */
CH_TxResult CH_TxPublishSuperblock(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const CH_TxSuperblock *authoritative,
    uint32_t authoritative_sector,
    uint32_t new_log_end_sector,
    CH_TxSuperblock *published_superblock,
    uint32_t *published_sector
);


#endif
