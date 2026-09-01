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


#endif
