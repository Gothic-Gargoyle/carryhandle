#ifndef CARRYHANDLE_CH_TX_H
#define CARRYHANDLE_CH_TX_H

#include <stddef.h>
#include <stdint.h>

#include <carryhandle/ch_tx_backend.h>
#include <carryhandle/ch_tx_format.h>


typedef enum CH_TxResult
{
    CH_TX_RESULT_OK = 0,

    /*
     * Normal iterator completion. This is not an error.
     */
    CH_TX_RESULT_END = 1,

    /*
     * Normal lookup completion: no committed record has the requested
     * exact scope + key identity.
     */
    CH_TX_RESULT_NOT_FOUND = 2,

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
 * Cursor over the records visible in one recovered committed state.
 *
 * Opening a cursor snapshots the authoritative log_end. Records appended
 * later are deliberately not visible through that cursor.
 */
typedef struct CH_TxLogCursor
{
    uint32_t generation;
    uint32_t next_sector;
    uint32_t end_sector;

} CH_TxLogCursor;


/*
 * Recover authoritative transaction state and initialize a committed-log
 * cursor at log_start.
 */
CH_TxResult CH_TxOpenLogCursor(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    CH_TxLogCursor *cursor
);


/*
 * Read and validate the next committed record.
 *
 * This performs a validation-only CH_TxReadRecord(): record body bytes are
 * CRC-checked but are not copied out.
 *
 * On CH_TX_RESULT_OK:
 *   record_sector and record are written
 *   cursor advances by record->record_sectors
 *
 * On CH_TX_RESULT_END:
 *   cursor was already at end_sector
 *   outputs and cursor are unchanged
 *
 * On error:
 *   outputs and cursor are unchanged
 */
CH_TxResult CH_TxReadNextRecord(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    CH_TxLogCursor *cursor,
    uint32_t *record_sector,
    CH_TxRecordHeader *record
);


/*
 * Find the physically latest committed record whose scope + key exactly
 * match the requested binary identity.
 *
 * Comparison is byte-exact:
 *
 *   scope length + bytes
 *   key length + bytes
 *
 * No string encoding or case folding is applied.
 *
 * Physical committed-log order determines the winner. Generation is
 * transaction metadata and is deliberately not used to order matches.
 *
 * The entire committed snapshot is validated even after a match has been
 * found. Corruption later in the log therefore cannot silently resurrect
 * an older matching record.
 *
 * On CH_TX_RESULT_OK:
 *   record_sector and record identify the latest matching PUT or DELETE.
 *
 * On CH_TX_RESULT_NOT_FOUND:
 *   no matching committed record exists.
 *
 * On NOT_FOUND or error:
 *   outputs are unchanged.
 */
CH_TxResult CH_TxFindLatestRecord(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    uint32_t *record_sector,
    CH_TxRecordHeader *record
);


/*
 * One record to append transactionally.
 *
 * CarryHandle owns:
 *
 *   generation
 *   record location
 *   scope/key/stored sizes in the encoded header
 *   body CRC
 *   record sector count
 *   A/B superblock publication
 *
 * raw_size and raw_crc32 describe the original uncompressed PUT payload.
 * stored_payload/stored_size describe the bytes actually placed in the
 * transaction log.
 *
 * DELETE carries scope + key with no stored payload.
 */
typedef struct CH_TxAppendRequest
{
    uint32_t operation;
    uint32_t flags;

    const void *scope;
    size_t scope_size;

    const void *key;
    size_t key_size;

    const void *stored_payload;
    size_t stored_size;

    uint32_t raw_size;
    uint32_t raw_crc32;

    uint32_t codec;

} CH_TxAppendRequest;


/*
 * Information about a definitely committed append.
 *
 * This structure is written only when CH_TxAppendRecord() returns
 * CH_TX_RESULT_OK.
 */
typedef struct CH_TxAppendCommit
{
    uint32_t record_sector;
    CH_TxRecordHeader record;

    uint32_t superblock_sector;
    CH_TxSuperblock superblock;

} CH_TxAppendCommit;


/*
 * Atomically append and publish one transaction record.
 *
 * Sequence:
 *
 *   1. recover authoritative A/B state
 *   2. append record at authoritative log_end
 *   3. sync record durability
 *   4. reread and verify record
 *   5. publish the inactive superblock
 *   6. sync publication
 *
 * CH_TX_RESULT_OK means the new record is definitely committed.
 *
 * CH_TX_RESULT_COMMIT_UNCERTAIN means publication I/O began but its
 * durable outcome cannot be known generically. The caller must recover
 * authoritative transaction state before performing another write.
 *
 * commit is optional and is not modified unless the result is OK.
 */
CH_TxResult CH_TxAppendRecord(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const CH_TxAppendRequest *request,
    CH_TxAppendCommit *commit
);


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
