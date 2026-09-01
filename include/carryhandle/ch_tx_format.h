#ifndef CARRYHANDLE_CH_TX_FORMAT_H
#define CARRYHANDLE_CH_TX_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/*
 * CarryHandle transactional store format.
 *
 * Physical layout:
 *
 *   sector 0    container metadata
 *   sector 1    superblock A
 *   sector 2    superblock B
 *   sector 3+   append-only records
 *
 * The physical backend is deliberately not part of this interface.
 *
 * All persistent multi-byte integers are encoded explicitly as
 * unsigned 32-bit big-endian values. Native structure layout and host
 * byte order are never part of the storage ABI.
 */


#define CH_TX_FORMAT_VERSION 1u

#define CH_TX_METADATA_SECTOR     0u
#define CH_TX_SUPERBLOCK_A_SECTOR 1u
#define CH_TX_SUPERBLOCK_B_SECTOR 2u
#define CH_TX_DATA_START_SECTOR   3u


/*
 * ASCII:
 *
 *   CHTX  container
 *   CHSB  superblock
 *   CHTR  record
 */
#define CH_TX_CONTAINER_MAGIC  0x43485458u
#define CH_TX_SUPERBLOCK_MAGIC 0x43485342u
#define CH_TX_RECORD_MAGIC     0x43485452u


/*
 * Persistent encoded sizes.
 *
 * These are part of the storage ABI. Never substitute sizeof(struct).
 */
#define CH_TX_CONTAINER_HEADER_ENCODED_SIZE 32u
#define CH_TX_SUPERBLOCK_ENCODED_SIZE       36u
#define CH_TX_RECORD_HEADER_ENCODED_SIZE    64u


typedef enum CH_TxOperation
{
    CH_TX_OPERATION_INVALID = 0,

    CH_TX_OPERATION_PUT = 1,
    CH_TX_OPERATION_DELETE = 2

} CH_TxOperation;


typedef enum CH_TxCodec
{
    CH_TX_CODEC_NONE = 0,

    /*
     * zlib-wrapped DEFLATE.
     */
    CH_TX_CODEC_ZLIB = 1

} CH_TxCodec;


/*
 * Logical contents of sector 0.
 *
 * replica_index identifies the physical A/B container:
 *
 *   0 -> replica A
 *   1 -> replica B
 */
typedef struct CH_TxContainerHeader
{
    uint32_t magic;
    uint32_t version;

    uint32_t header_size;
    uint32_t sector_size;

    uint32_t container_sectors;
    uint32_t replica_index;

    uint32_t flags;
    uint32_t header_crc32;

} CH_TxContainerHeader;


/*
 * Authoritative committed state.
 *
 * log_end_sector points one sector past the last committed record.
 * Anything physically present beyond log_end_sector is invisible.
 */
typedef struct CH_TxSuperblock
{
    uint32_t magic;
    uint32_t version;

    uint32_t generation;

    uint32_t sector_size;
    uint32_t container_sectors;

    uint32_t log_start_sector;
    uint32_t log_end_sector;

    uint32_t flags;
    uint32_t superblock_crc32;

} CH_TxSuperblock;


/*
 * Fixed record framing.
 *
 * On disk:
 *
 *   encoded header
 *   scope bytes
 *   key bytes
 *   stored payload bytes
 *   zero padding to sector boundary
 *
 * scope is optional.
 * key is required.
 *
 * Scope and key are opaque byte sequences. CarryHandle assigns no string
 * encoding and comparisons are exact byte-for-byte comparisons.
 *
 * raw_crc32 protects the uncompressed PUT payload.
 *
 * body_crc32 protects the complete variable-length stored body:
 *
 *   scope || key || stored payload
 *
 * This means corruption of logical identity bytes is detectable before
 * a record can be treated as belonging to another object.
 *
 * DELETE carries scope + key but no payload. Its body CRC therefore
 * protects scope || key.
 */
typedef struct CH_TxRecordHeader
{
    uint32_t magic;
    uint32_t version;

    uint32_t header_size;

    uint32_t operation;
    uint32_t generation;

    uint32_t flags;

    uint32_t scope_size;
    uint32_t key_size;

    uint32_t raw_size;
    uint32_t stored_size;

    uint32_t raw_crc32;
    uint32_t body_crc32;

    uint32_t record_sectors;

    uint32_t codec;

    uint32_t reserved;

    uint32_t header_crc32;

} CH_TxRecordHeader;


/*
 * Calculate the physical sector count for:
 *
 *   header + scope + key + stored payload
 *
 * Returns zero for invalid geometry or arithmetic overflow.
 */
static inline uint32_t CH_TxRecordSectorCount(
    size_t scope_size,
    size_t key_size,
    size_t stored_size,
    uint32_t sector_size)
{
    size_t total;

    if (sector_size == 0u)
    {
        return 0u;
    }

    total =
        CH_TX_RECORD_HEADER_ENCODED_SIZE;

    if (scope_size > SIZE_MAX - total)
    {
        return 0u;
    }

    total += scope_size;

    if (key_size > SIZE_MAX - total)
    {
        return 0u;
    }

    total += key_size;

    if (stored_size > SIZE_MAX - total)
    {
        return 0u;
    }

    total += stored_size;

    if (
        total >
        SIZE_MAX - ((size_t)sector_size - 1u)
    )
    {
        return 0u;
    }

    total =
        (
            total
            + (size_t)sector_size
            - 1u
        )
        / (size_t)sector_size;

    if (total > UINT32_MAX)
    {
        return 0u;
    }

    return (uint32_t)total;
}


/*
 * Explicit binary codecs.
 *
 * The encoded representation, not the C structure layout, is persistent.
 */
bool CH_TxEncodeContainerHeader(
    uint8_t *buffer,
    size_t buffer_size,
    const CH_TxContainerHeader *header
);

bool CH_TxDecodeContainerHeader(
    CH_TxContainerHeader *header,
    const uint8_t *buffer,
    size_t buffer_size
);

bool CH_TxEncodeSuperblock(
    uint8_t *buffer,
    size_t buffer_size,
    const CH_TxSuperblock *superblock
);

bool CH_TxDecodeSuperblock(
    CH_TxSuperblock *superblock,
    const uint8_t *buffer,
    size_t buffer_size
);

bool CH_TxEncodeRecordHeader(
    uint8_t *buffer,
    size_t buffer_size,
    const CH_TxRecordHeader *record
);

bool CH_TxDecodeRecordHeader(
    CH_TxRecordHeader *record,
    const uint8_t *buffer,
    size_t buffer_size
);


#endif
