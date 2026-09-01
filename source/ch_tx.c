#include <carryhandle/ch_tx.h>

#include <string.h>
#include <zlib.h>


static bool superblockGeometryMatches(
    const CH_TxSectorBackend *backend,
    const CH_TxSuperblock *superblock)
{
    CH_TxArenaBounds arena;

    if (
        !backend ||
        !superblock ||
        superblock->sector_size !=
            backend->sector_size ||
        superblock->container_sectors !=
            backend->sector_count
    )
    {
        return false;
    }

    return
        CH_TxArenaBoundsForLog(
            backend->sector_count,
            superblock->log_start_sector,
            superblock->log_end_sector,
            &arena,
            NULL
        );
}


static bool superblocksEquivalent(
    const CH_TxSuperblock *a,
    const CH_TxSuperblock *b)
{
    if (
        !a ||
        !b
    )
    {
        return false;
    }

    return
        a->magic ==
            b->magic &&
        a->version ==
            b->version &&
        a->generation ==
            b->generation &&
        a->sector_size ==
            b->sector_size &&
        a->container_sectors ==
            b->container_sectors &&
        a->log_start_sector ==
            b->log_start_sector &&
        a->log_end_sector ==
            b->log_end_sector &&
        a->flags ==
            b->flags;
}


static bool readSuperblock(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    uint32_t sector,
    CH_TxSuperblock *superblock,
    bool *valid)
{
    if (
        !backend ||
        !sector_buffer ||
        !superblock ||
        !valid
    )
    {
        return false;
    }

    *valid =
        false;

    if (!backend->read_sector(
            backend->context,
            sector,
            sector_buffer))
    {
        return false;
    }

    if (!CH_TxDecodeSuperblock(
            superblock,
            sector_buffer,
            backend->sector_size))
    {
        return true;
    }

    if (!superblockGeometryMatches(
            backend,
            superblock))
    {
        return true;
    }

    *valid =
        true;

    return true;
}


static bool containerGeometryMatches(
    const CH_TxSectorBackend *backend,
    const CH_TxContainerHeader *container)
{
    return
        backend &&
        container &&
        container->sector_size ==
            backend->sector_size &&
        container->container_sectors ==
            backend->sector_count;
}


CH_TxResult CH_TxReadAuthoritativeSuperblock(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    CH_TxSuperblock *superblock,
    uint32_t *superblock_sector)
{
    CH_TxContainerHeader container;

    CH_TxSuperblock a;
    CH_TxSuperblock b;

    bool aValid;
    bool bValid;

    uint32_t delta;

    if (
        !CH_TxSectorBackendValid(backend) ||
        !sector_buffer ||
        !superblock ||
        sector_buffer_size <
            backend->sector_size ||
        !CH_TxSectorBufferValid(
            backend,
            sector_buffer) ||
        backend->sector_count <=
            CH_TX_SUPERBLOCK_B_SECTOR
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    /*
     * Sector 0 is the container publication marker.
     *
     * Valid superblocks without a valid container header are staged or
     * incomplete initialization state and must never become authoritative.
     */
    if (!backend->read_sector(
            backend->context,
            CH_TX_METADATA_SECTOR,
            sector_buffer))
    {
        return
            CH_TX_RESULT_IO;
    }

    if (
        !CH_TxDecodeContainerHeader(
            &container,
            sector_buffer,
            backend->sector_size) ||
        !containerGeometryMatches(
            backend,
            &container)
    )
    {
        return
            CH_TX_RESULT_CORRUPT;
    }


    if (!readSuperblock(
            backend,
            sector_buffer,
            CH_TX_SUPERBLOCK_A_SECTOR,
            &a,
            &aValid))
    {
        return
            CH_TX_RESULT_IO;
    }

    if (!readSuperblock(
            backend,
            sector_buffer,
            CH_TX_SUPERBLOCK_B_SECTOR,
            &b,
            &bValid))
    {
        return
            CH_TX_RESULT_IO;
    }

    if (!aValid && !bValid)
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    if (aValid && !bValid)
    {
        *superblock =
            a;

        if (superblock_sector)
        {
            *superblock_sector =
                CH_TX_SUPERBLOCK_A_SECTOR;
        }

        return
            CH_TX_RESULT_OK;
    }

    if (!aValid && bValid)
    {
        *superblock =
            b;

        if (superblock_sector)
        {
            *superblock_sector =
                CH_TX_SUPERBLOCK_B_SECTOR;
        }

        return
            CH_TX_RESULT_OK;
    }

    if (a.generation ==
        b.generation)
    {
        if (!superblocksEquivalent(
                &a,
                &b))
        {
            return
                CH_TX_RESULT_AMBIGUOUS;
        }

        *superblock =
            a;

        if (superblock_sector)
        {
            *superblock_sector =
                CH_TX_SUPERBLOCK_A_SECTOR;
        }

        return
            CH_TX_RESULT_OK;
    }

    delta =
        b.generation -
        a.generation;

    /*
     * Exactly half the uint32 generation space apart has no unique
     * wrap-aware ordering.
     */
    if (delta == 0x80000000u)
    {
        return
            CH_TX_RESULT_AMBIGUOUS;
    }

    if (delta < 0x80000000u)
    {
        *superblock =
            b;

        if (superblock_sector)
        {
            *superblock_sector =
                CH_TX_SUPERBLOCK_B_SECTOR;
        }
    }
    else
    {
        *superblock =
            a;

        if (superblock_sector)
        {
            *superblock_sector =
                CH_TX_SUPERBLOCK_A_SECTOR;
        }
    }

    return
        CH_TX_RESULT_OK;
}

/* ------------------------------------------------------------------------- */
/* Transaction container initialization                                      */
/* ------------------------------------------------------------------------- */

CH_TxResult CH_TxInitialize(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size)
{
    CH_TxContainerHeader container = {0};
    CH_TxSuperblock superblock = {0};

    if (
        !CH_TxSectorBackendValid(backend) ||
        !sector_buffer ||
        sector_buffer_size <
            backend->sector_size ||
        !CH_TxSectorBufferValid(
            backend,
            sector_buffer) ||
        backend->sector_size <
            CH_TX_RECORD_HEADER_ENCODED_SIZE ||
        backend->sector_count <=
            CH_TX_SUPERBLOCK_B_SECTOR
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }


    /*
     * First inspect sector 0.
     *
     * If it is already a valid format-v1 container for this backend,
     * initialization is idempotent: validate the authoritative state and
     * return without writing anything.
     */
    if (!backend->read_sector(
            backend->context,
            CH_TX_METADATA_SECTOR,
            sector_buffer))
    {
        return
            CH_TX_RESULT_IO;
    }

    if (CH_TxDecodeContainerHeader(
            &container,
            sector_buffer,
            backend->sector_size))
    {
        if (!containerGeometryMatches(
                backend,
                &container))
        {
            return
                CH_TX_RESULT_CORRUPT;
        }

        return
            CH_TxReadAuthoritativeSuperblock(
                backend,
                sector_buffer,
                sector_buffer_size,
                &superblock,
                NULL
            );
    }


    /*
     * Stage two equivalent empty superblocks.
     *
     * Sector 0 is still invalid at this point, so even physically durable
     * A/B writes cannot make a half-initialized store visible.
     */
    superblock.generation =
        0u;

    superblock.sector_size =
        backend->sector_size;

    superblock.container_sectors =
        backend->sector_count;

    {
        CH_TxArenaBounds arena;

        if (!CH_TxArenaBoundsForIndex(
                backend->sector_count,
                0u,
                &arena))
        {
            return
                CH_TX_RESULT_INVALID_ARGUMENT;
        }

        superblock.log_start_sector =
            arena.start_sector;

        superblock.log_end_sector =
            arena.start_sector;
    }

    superblock.flags =
        0u;


    memset(
        sector_buffer,
        0,
        backend->sector_size
    );

    if (!CH_TxEncodeSuperblock(
            sector_buffer,
            backend->sector_size,
            &superblock))
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }


    if (!backend->write_sector(
            backend->context,
            CH_TX_SUPERBLOCK_A_SECTOR,
            sector_buffer))
    {
        return
            CH_TX_RESULT_IO;
    }


    if (!backend->write_sector(
            backend->context,
            CH_TX_SUPERBLOCK_B_SECTOR,
            sector_buffer))
    {
        return
            CH_TX_RESULT_IO;
    }


    if (!backend->sync(
            backend->context))
    {
        return
            CH_TX_RESULT_IO;
    }


    /*
     * Reload sector 0 after staging so bytes outside the transaction
     * container header are preserved. This is important for callers that
     * store CARD presentation or other application metadata in the same
     * physical sector.
     */
    if (!backend->read_sector(
            backend->context,
            CH_TX_METADATA_SECTOR,
            sector_buffer))
    {
        return
            CH_TX_RESULT_IO;
    }


    memset(
        &container,
        0,
        sizeof(container)
    );

    container.sector_size =
        backend->sector_size;

    container.container_sectors =
        backend->sector_count;

    /*
     * Format-v1 single-backend containers use physical replica A.
     */
    container.replica_index =
        0u;

    container.flags =
        0u;


    /*
     * CH_TxEncodeContainerHeader() touches only the encoded 32-byte header.
     * The remainder of sector 0 stays caller-owned.
     */
    if (!CH_TxEncodeContainerHeader(
            sector_buffer,
            backend->sector_size,
            &container))
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }


    /*
     * Publication begins here.
     *
     * From this point onward a generic backend cannot know whether a failed
     * write or durability barrier became persistent.
     */
    if (!backend->write_sector(
            backend->context,
            CH_TX_METADATA_SECTOR,
            sector_buffer))
    {
        return
            CH_TX_RESULT_COMMIT_UNCERTAIN;
    }


    if (!backend->sync(
            backend->context))
    {
        return
            CH_TX_RESULT_COMMIT_UNCERTAIN;
    }


    return
        CH_TX_RESULT_OK;
}


/* ------------------------------------------------------------------------- */
/* Committed record reader                                                   */
/* ------------------------------------------------------------------------- */

static uint32_t bodyCrcUpdate(
    uint32_t crc,
    const uint8_t *data,
    size_t size)
{
    size_t i;
    unsigned int bit;

    for (i = 0; i < size; ++i)
    {
        crc ^=
            (uint32_t)data[i];

        for (bit = 0; bit < 8u; ++bit)
        {
            uint32_t mask =
                0u - (crc & 1u);

            crc =
                (crc >> 1) ^
                (0xedb88320u & mask);
        }
    }

    return crc;
}


static bool recordBodySize(
    const CH_TxRecordHeader *record,
    size_t *body_size)
{
    size_t total;

    if (
        !record ||
        !body_size
    )
    {
        return false;
    }

    total =
        (size_t)record->scope_size;

    if (
        (size_t)record->key_size >
        SIZE_MAX - total
    )
    {
        return false;
    }

    total +=
        (size_t)record->key_size;

    if (
        (size_t)record->stored_size >
        SIZE_MAX - total
    )
    {
        return false;
    }

    total +=
        (size_t)record->stored_size;

    *body_size =
        total;

    return true;
}


static void copyRecordBodyChunk(
    const CH_TxRecordHeader *record,
    size_t body_offset,
    const uint8_t *data,
    size_t size,
    uint8_t *scope_output,
    uint8_t *key_output,
    uint8_t *stored_output)
{
    size_t scopeEnd;
    size_t keyEnd;

    size_t cursor;
    size_t sourceOffset;
    size_t remaining;
    size_t amount;

    scopeEnd =
        (size_t)record->scope_size;

    keyEnd =
        scopeEnd +
        (size_t)record->key_size;

    cursor =
        body_offset;

    sourceOffset =
        0u;

    remaining =
        size;

    if (
        remaining > 0u &&
        cursor < scopeEnd
    )
    {
        amount =
            scopeEnd - cursor;

        if (amount > remaining)
        {
            amount =
                remaining;
        }

        if (scope_output)
        {
            memcpy(
                scope_output + cursor,
                data + sourceOffset,
                amount
            );
        }

        cursor += amount;
        sourceOffset += amount;
        remaining -= amount;
    }

    if (
        remaining > 0u &&
        cursor < keyEnd
    )
    {
        amount =
            keyEnd - cursor;

        if (amount > remaining)
        {
            amount =
                remaining;
        }

        if (key_output)
        {
            memcpy(
                key_output +
                    (cursor - scopeEnd),
                data + sourceOffset,
                amount
            );
        }

        cursor += amount;
        sourceOffset += amount;
        remaining -= amount;
    }

    if (remaining > 0u)
    {
        if (stored_output)
        {
            memcpy(
                stored_output +
                    (cursor - keyEnd),
                data + sourceOffset,
                remaining
            );
        }
    }
}


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
    size_t stored_capacity)
{
    CH_TxRecordHeader decoded;

    uint8_t *sectorBytes;

    size_t bodySize;
    size_t bodyOffset;
    size_t remaining;
    size_t dataOffset;
    size_t available;
    size_t amount;

    uint32_t expectedSectors;
    uint32_t sectorIndex;
    uint32_t crc;

    if (
        !CH_TxSectorBackendValid(backend) ||
        !sector_buffer ||
        !record ||
        sector_buffer_size <
            backend->sector_size ||
        !CH_TxSectorBufferValid(
            backend,
            sector_buffer) ||
        backend->sector_size <
            CH_TX_RECORD_HEADER_ENCODED_SIZE ||
        read_limit_sector >
            backend->sector_count ||
        read_limit_sector <
            CH_TX_DATA_START_SECTOR ||
        record_sector <
            CH_TX_DATA_START_SECTOR ||
        record_sector >=
            read_limit_sector ||
        (!scope_output &&
            scope_capacity != 0u) ||
        (!key_output &&
            key_capacity != 0u) ||
        (!stored_output &&
            stored_capacity != 0u)
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    sectorBytes =
        (uint8_t *)sector_buffer;

    if (!backend->read_sector(
            backend->context,
            record_sector,
            sector_buffer))
    {
        return
            CH_TX_RESULT_IO;
    }

    if (!CH_TxDecodeRecordHeader(
            &decoded,
            sectorBytes,
            backend->sector_size))
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    if (!recordBodySize(
            &decoded,
            &bodySize))
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    expectedSectors =
        CH_TxRecordSectorCount(
            (size_t)decoded.scope_size,
            (size_t)decoded.key_size,
            (size_t)decoded.stored_size,
            backend->sector_size
        );

    if (
        expectedSectors == 0u ||
        decoded.record_sectors !=
            expectedSectors ||
        decoded.record_sectors >
            read_limit_sector -
            record_sector
    )
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    if (
        (scope_output &&
            scope_capacity <
                (size_t)decoded.scope_size) ||
        (key_output &&
            key_capacity <
                (size_t)decoded.key_size) ||
        (stored_output &&
            stored_capacity <
                (size_t)decoded.stored_size)
    )
    {
        return
            CH_TX_RESULT_BUFFER_TOO_SMALL;
    }

    crc =
        0xffffffffu;

    bodyOffset =
        0u;

    remaining =
        bodySize;

    for (
        sectorIndex = 0u;
        sectorIndex <
            decoded.record_sectors;
        ++sectorIndex
    )
    {
        if (sectorIndex != 0u)
        {
            if (!backend->read_sector(
                    backend->context,
                    record_sector +
                        sectorIndex,
                    sector_buffer))
            {
                return
                    CH_TX_RESULT_IO;
            }
        }

        dataOffset =
            sectorIndex == 0u
            ? CH_TX_RECORD_HEADER_ENCODED_SIZE
            : 0u;

        available =
            (size_t)backend->sector_size -
            dataOffset;

        amount =
            remaining < available
            ? remaining
            : available;

        if (amount > 0u)
        {
            crc =
                bodyCrcUpdate(
                    crc,
                    sectorBytes +
                        dataOffset,
                    amount
                );

            copyRecordBodyChunk(
                &decoded,
                bodyOffset,
                sectorBytes +
                    dataOffset,
                amount,
                (uint8_t *)scope_output,
                (uint8_t *)key_output,
                (uint8_t *)stored_output
            );

            bodyOffset += amount;
            remaining -= amount;
        }
    }

    if (
        remaining != 0u ||
        bodyOffset != bodySize
    )
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    crc ^=
        0xffffffffu;

    if (crc !=
        decoded.body_crc32)
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    *record =
        decoded;

    return
        CH_TX_RESULT_OK;
}

/* ------------------------------------------------------------------------- */
/* Durable uncommitted record writer                                         */
/* ------------------------------------------------------------------------- */

static uint32_t bodyCrcParts(
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    const void *stored_payload,
    size_t stored_size)
{
    uint32_t crc =
        0xffffffffu;

    if (scope_size > 0u)
    {
        crc =
            bodyCrcUpdate(
                crc,
                (const uint8_t *)scope,
                scope_size
            );
    }

    if (key_size > 0u)
    {
        crc =
            bodyCrcUpdate(
                crc,
                (const uint8_t *)key,
                key_size
            );
    }

    if (stored_size > 0u)
    {
        crc =
            bodyCrcUpdate(
                crc,
                (const uint8_t *)stored_payload,
                stored_size
            );
    }

    return
        crc ^ 0xffffffffu;
}


static size_t copyInputBodyChunk(
    uint8_t *destination,
    size_t capacity,
    size_t body_offset,
    const uint8_t *scope,
    size_t scope_size,
    const uint8_t *key,
    size_t key_size,
    const uint8_t *stored_payload,
    size_t stored_size)
{
    size_t scopeEnd =
        scope_size;

    size_t keyEnd =
        scope_size + key_size;

    size_t bodyEnd =
        keyEnd + stored_size;

    size_t cursor =
        body_offset;

    size_t copied =
        0u;

    size_t amount;

    while (
        copied < capacity &&
        cursor < bodyEnd
    )
    {
        if (cursor < scopeEnd)
        {
            amount =
                scopeEnd - cursor;

            if (amount > capacity - copied)
            {
                amount =
                    capacity - copied;
            }

            memcpy(
                destination + copied,
                scope + cursor,
                amount
            );
        }
        else if (cursor < keyEnd)
        {
            amount =
                keyEnd - cursor;

            if (amount > capacity - copied)
            {
                amount =
                    capacity - copied;
            }

            memcpy(
                destination + copied,
                key + (cursor - scopeEnd),
                amount
            );
        }
        else
        {
            amount =
                bodyEnd - cursor;

            if (amount > capacity - copied)
            {
                amount =
                    capacity - copied;
            }

            memcpy(
                destination + copied,
                stored_payload +
                    (cursor - keyEnd),
                amount
            );
        }

        cursor += amount;
        copied += amount;
    }

    return copied;
}


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
    CH_TxRecordHeader *written_record)
{
    CH_TxRecordHeader record;

    uint8_t *sectorBytes;

    size_t bodySize;
    size_t bodyOffset;
    size_t dataOffset;
    size_t available;
    size_t copied;

    uint32_t sectorIndex;

    if (
        !CH_TxSectorBackendValid(backend) ||
        !sector_buffer ||
        !record_template ||
        sector_buffer_size <
            backend->sector_size ||
        !CH_TxSectorBufferValid(
            backend,
            sector_buffer) ||
        backend->sector_size <
            CH_TX_RECORD_HEADER_ENCODED_SIZE ||
        record_sector <
            CH_TX_DATA_START_SECTOR ||
        scope_size > UINT32_MAX ||
        key_size == 0u ||
        key_size > UINT32_MAX ||
        stored_size > UINT32_MAX ||
        (!scope && scope_size != 0u) ||
        !key ||
        (!stored_payload &&
            stored_size != 0u)
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    if (
        scope_size >
            SIZE_MAX - key_size ||
        scope_size + key_size >
            SIZE_MAX - stored_size
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    bodySize =
        scope_size +
        key_size +
        stored_size;

    record =
        *record_template;

    record.scope_size =
        (uint32_t)scope_size;

    record.key_size =
        (uint32_t)key_size;

    record.stored_size =
        (uint32_t)stored_size;

    record.body_crc32 =
        bodyCrcParts(
            scope,
            scope_size,
            key,
            key_size,
            stored_payload,
            stored_size
        );

    record.record_sectors =
        CH_TxRecordSectorCount(
            scope_size,
            key_size,
            stored_size,
            backend->sector_size
        );

    record.reserved =
        0u;

    if (
        record.record_sectors == 0u ||
        record_sector >
            backend->sector_count ||
        record.record_sectors >
            backend->sector_count -
            record_sector
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    sectorBytes =
        (uint8_t *)sector_buffer;

    bodyOffset =
        0u;

    for (
        sectorIndex = 0u;
        sectorIndex <
            record.record_sectors;
        ++sectorIndex
    )
    {
        memset(
            sectorBytes,
            0,
            backend->sector_size
        );

        dataOffset =
            sectorIndex == 0u
            ? CH_TX_RECORD_HEADER_ENCODED_SIZE
            : 0u;

        if (
            sectorIndex == 0u &&
            !CH_TxEncodeRecordHeader(
                sectorBytes,
                backend->sector_size,
                &record)
        )
        {
            return
                CH_TX_RESULT_INVALID_ARGUMENT;
        }

        available =
            (size_t)backend->sector_size -
            dataOffset;

        copied =
            copyInputBodyChunk(
                sectorBytes + dataOffset,
                available,
                bodyOffset,
                (const uint8_t *)scope,
                scope_size,
                (const uint8_t *)key,
                key_size,
                (const uint8_t *)stored_payload,
                stored_size
            );

        bodyOffset +=
            copied;

        if (!backend->write_sector(
                backend->context,
                record_sector +
                    sectorIndex,
                sector_buffer))
        {
            return
                CH_TX_RESULT_IO;
        }
    }

    if (bodyOffset != bodySize)
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    if (!backend->sync(
            backend->context))
    {
        return
            CH_TX_RESULT_IO;
    }

    /*
     * Durability alone is not sufficient. Read the bytes back through
     * the same validator used for committed records before allowing the
     * append stage to succeed.
     */
    {
        CH_TxRecordHeader readback;
        CH_TxResult readResult;

        uint32_t recordEnd =
            record_sector +
            record.record_sectors;

        readResult =
            CH_TxReadRecord(
                backend,
                sector_buffer,
                sector_buffer_size,
                recordEnd,
                record_sector,
                &readback,
                NULL,
                0u,
                NULL,
                0u,
                NULL,
                0u
            );

        if (readResult !=
            CH_TX_RESULT_OK)
        {
            return
                readResult;
        }

        if (
            readback.operation !=
                record.operation ||
            readback.generation !=
                record.generation ||
            readback.flags !=
                record.flags ||
            readback.scope_size !=
                record.scope_size ||
            readback.key_size !=
                record.key_size ||
            readback.raw_size !=
                record.raw_size ||
            readback.stored_size !=
                record.stored_size ||
            readback.raw_crc32 !=
                record.raw_crc32 ||
            readback.body_crc32 !=
                record.body_crc32 ||
            readback.record_sectors !=
                record.record_sectors ||
            readback.codec !=
                record.codec ||
            readback.reserved !=
                record.reserved
        )
        {
            return
                CH_TX_RESULT_CORRUPT;
        }
    }

    if (written_record)
    {
        *written_record =
            record;
    }

    return
        CH_TX_RESULT_OK;
}

/* ------------------------------------------------------------------------- */
/* Inactive-superblock publication                                           */
/* ------------------------------------------------------------------------- */

CH_TxResult CH_TxPublishSuperblock(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const CH_TxSuperblock *authoritative,
    uint32_t authoritative_sector,
    uint32_t new_log_end_sector,
    CH_TxSuperblock *published_superblock,
    uint32_t *published_sector)
{
    CH_TxSuperblock next;

    uint32_t inactiveSector;

    if (
        !CH_TxSectorBackendValid(backend) ||
        !sector_buffer ||
        !authoritative ||
        sector_buffer_size <
            backend->sector_size ||
        !CH_TxSectorBufferValid(
            backend,
            sector_buffer) ||
        backend->sector_size <
            CH_TX_SUPERBLOCK_ENCODED_SIZE
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    /*
     * Transaction format v1 reserves:
     *
     *   sector 1 = superblock A
     *   sector 2 = superblock B
     */
    if (
        authoritative_sector != 1u &&
        authoritative_sector != 2u
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    /*
     * Do not trust a caller-supplied superblock merely because it came
     * through an internal API. Its geometry must still describe this
     * backend and a valid transaction log.
     */
    if (
        !superblockGeometryMatches(
            backend,
            authoritative)
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    /*
     * Publication is specifically an append commit. Every transaction
     * published through this primitive must advance the visible log.
     */
    if (
        new_log_end_sector <=
            authoritative->log_end_sector
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    {
        CH_TxArenaBounds arena;

        if (!CH_TxArenaBoundsForLog(
                backend->sector_count,
                authoritative->log_start_sector,
                authoritative->log_end_sector,
                &arena,
                NULL))
        {
            return
                CH_TX_RESULT_INVALID_ARGUMENT;
        }

        if (new_log_end_sector >
            arena.end_sector)
        {
            return
                CH_TX_RESULT_NO_SPACE;
        }
    }

    next =
        *authoritative;

    /*
     * Unsigned overflow is intentional. Authoritative-state selection
     * already defines uint32 generation wraparound semantics.
     */
    next.generation =
        authoritative->generation + 1u;

    next.log_end_sector =
        new_log_end_sector;

    inactiveSector =
        authoritative_sector == 1u
        ? 2u
        : 1u;

    memset(
        sector_buffer,
        0,
        backend->sector_size
    );

    if (!CH_TxEncodeSuperblock(
            sector_buffer,
            backend->sector_size,
            &next))
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    if (!backend->write_sector(
            backend->context,
            inactiveSector,
            sector_buffer))
    {
        return
            CH_TX_RESULT_COMMIT_UNCERTAIN;
    }

    /*
     * This is the commit point.
     *
     * Before this durability barrier succeeds, the previous
     * authoritative superblock remains the committed state.
     *
     * After it succeeds, the newly written generation is committed.
     */
    if (!backend->sync(
            backend->context))
    {
        return
            CH_TX_RESULT_COMMIT_UNCERTAIN;
    }

    if (published_superblock)
    {
        *published_superblock =
            next;
    }

    if (published_sector)
    {
        *published_sector =
            inactiveSector;
    }

    return
        CH_TX_RESULT_OK;
}

/* ------------------------------------------------------------------------- */
/* Transaction record append orchestration                                   */
/* ------------------------------------------------------------------------- */

CH_TxResult CH_TxAppendRecord(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const CH_TxAppendRequest *request,
    CH_TxAppendCommit *commit)
{
    CH_TxSuperblock authoritative;
    CH_TxSuperblock published;

    CH_TxRecordHeader recordTemplate = {0};
    CH_TxRecordHeader written;

    CH_TxResult result;

    uint32_t authoritativeSector;
    uint32_t publishedSector;
    uint32_t recordSector;
    uint32_t recordEnd;

    if (
        !backend ||
        !sector_buffer ||
        !request
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    /*
     * Recover committed state first.
     *
     * IO, CORRUPT and AMBIGUOUS are propagated directly. Nothing has
     * been written yet at this point.
     */
    result =
        CH_TxReadAuthoritativeSuperblock(
            backend,
            sector_buffer,
            sector_buffer_size,
            &authoritative,
            &authoritativeSector
        );

    if (result != CH_TX_RESULT_OK)
    {
        return result;
    }

    {
        CH_TxArenaBounds arena;

        uint32_t requiredSectors =
            CH_TxRecordSectorCount(
                request->scope_size,
                request->key_size,
                request->stored_size,
                backend->sector_size
            );

        if (
            requiredSectors == 0u ||
            !CH_TxArenaBoundsForLog(
                backend->sector_count,
                authoritative.log_start_sector,
                authoritative.log_end_sector,
                &arena,
                NULL
            )
        )
        {
            return
                CH_TX_RESULT_INVALID_ARGUMENT;
        }

        if (
            requiredSectors >
            arena.end_sector -
                authoritative.log_end_sector
        )
        {
            return
                CH_TX_RESULT_NO_SPACE;
        }
    }

    /*
     * Generation and physical placement are transaction-store state,
     * not caller-controlled record metadata.
     */
    recordTemplate.operation =
        request->operation;

    recordTemplate.generation =
        authoritative.generation + 1u;

    recordTemplate.flags =
        request->flags;

    recordTemplate.raw_size =
        request->raw_size;

    recordTemplate.raw_crc32 =
        request->raw_crc32;

    recordTemplate.codec =
        request->codec;

    recordSector =
        authoritative.log_end_sector;

    /*
     * This performs:
     *
     *   complete record write
     *   -> durability sync
     *   -> normal reader validation
     *   -> exact metadata verification
     *
     * Failure here is still pre-publication and therefore cannot have
     * changed authoritative transaction state.
     */
    result =
        CH_TxWriteUncommittedRecord(
            backend,
            sector_buffer,
            sector_buffer_size,
            recordSector,
            &recordTemplate,
            request->scope,
            request->scope_size,
            request->key,
            request->key_size,
            request->stored_payload,
            request->stored_size,
            &written
        );

    if (result != CH_TX_RESULT_OK)
    {
        return result;
    }

    /*
     * CH_TxWriteUncommittedRecord() guarantees that the complete record
     * fits the backend, so this addition cannot overflow uint32_t.
     */
    recordEnd =
        recordSector +
        written.record_sectors;

    /*
     * Publication is the only commit boundary.
     *
     * In particular, COMMIT_UNCERTAIN must propagate unchanged.
     */
    result =
        CH_TxPublishSuperblock(
            backend,
            sector_buffer,
            sector_buffer_size,
            &authoritative,
            authoritativeSector,
            recordEnd,
            &published,
            &publishedSector
        );

    if (result != CH_TX_RESULT_OK)
    {
        return result;
    }

    /*
     * Do not expose a partially meaningful result structure on any
     * failure path. Reaching here means publication definitely synced.
     */
    if (commit)
    {
        commit->record_sector =
            recordSector;

        commit->record =
            written;

        commit->superblock_sector =
            publishedSector;

        commit->superblock =
            published;
    }

    return
        CH_TX_RESULT_OK;
}

/* ------------------------------------------------------------------------- */
/* Committed-log cursor                                                      */
/* ------------------------------------------------------------------------- */

CH_TxResult CH_TxOpenLogCursor(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    CH_TxLogCursor *cursor)
{
    CH_TxSuperblock authoritative;
    CH_TxLogCursor next;

    CH_TxResult result;

    uint32_t authoritativeSector;

    if (!cursor)
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    result =
        CH_TxReadAuthoritativeSuperblock(
            backend,
            sector_buffer,
            sector_buffer_size,
            &authoritative,
            &authoritativeSector
        );

    if (result != CH_TX_RESULT_OK)
    {
        return result;
    }

    (void)authoritativeSector;

    next.generation =
        authoritative.generation;

    next.next_sector =
        authoritative.log_start_sector;

    next.end_sector =
        authoritative.log_end_sector;

    *cursor =
        next;

    return
        CH_TX_RESULT_OK;
}


CH_TxResult CH_TxReadNextRecord(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    CH_TxLogCursor *cursor,
    uint32_t *record_sector,
    CH_TxRecordHeader *record)
{
    CH_TxRecordHeader nextRecord;

    CH_TxResult result;

    uint32_t currentSector;
    uint32_t nextSector;

    if (
        !cursor ||
        !record_sector ||
        !record
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    if (
        cursor->next_sector >
            cursor->end_sector
    )
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    if (
        cursor->next_sector ==
            cursor->end_sector
    )
    {
        return
            CH_TX_RESULT_END;
    }

    currentSector =
        cursor->next_sector;

    result =
        CH_TxReadRecord(
            backend,
            sector_buffer,
            sector_buffer_size,
            cursor->end_sector,
            currentSector,
            &nextRecord,
            NULL,
            0u,
            NULL,
            0u,
            NULL,
            0u
        );

    if (result != CH_TX_RESULT_OK)
    {
        return result;
    }

    if (
        nextRecord.record_sectors >
            cursor->end_sector -
            currentSector
    )
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    nextSector =
        currentSector +
        nextRecord.record_sectors;

    /*
     * Only publish outputs/cursor advancement after the complete record
     * has validated successfully.
     */
    *record_sector =
        currentSector;

    *record =
        nextRecord;

    cursor->next_sector =
        nextSector;

    return
        CH_TX_RESULT_OK;
}

/* ------------------------------------------------------------------------- */
/* Exact committed-record identity resolution                                */
/* ------------------------------------------------------------------------- */

static bool recordIdentityHeaderMatches(
    const CH_TxRecordHeader *a,
    const CH_TxRecordHeader *b)
{
    return
        a->operation ==
            b->operation &&
        a->generation ==
            b->generation &&
        a->flags ==
            b->flags &&
        a->scope_size ==
            b->scope_size &&
        a->key_size ==
            b->key_size &&
        a->raw_size ==
            b->raw_size &&
        a->stored_size ==
            b->stored_size &&
        a->raw_crc32 ==
            b->raw_crc32 &&
        a->body_crc32 ==
            b->body_crc32 &&
        a->record_sectors ==
            b->record_sectors &&
        a->codec ==
            b->codec &&
        a->reserved ==
            b->reserved;
}


static CH_TxResult recordIdentityMatches(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    uint32_t read_limit_sector,
    uint32_t record_sector,
    const CH_TxRecordHeader *record,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    bool *matches)
{
    CH_TxRecordHeader reread;

    uint8_t *sectorBytes;

    const uint8_t *scopeBytes =
        (const uint8_t *)scope;

    const uint8_t *keyBytes =
        (const uint8_t *)key;

    size_t bodySize;
    size_t bodyOffset;
    size_t identityEnd;
    size_t remaining;
    size_t dataOffset;
    size_t available;
    size_t amount;
    size_t i;
    size_t position;

    uint32_t sectorIndex;
    uint32_t crc;

    bool equal =
        true;

    if (
        !CH_TxSectorBackendValid(backend) ||
        !sector_buffer ||
        !record ||
        !matches ||
        sector_buffer_size <
            backend->sector_size ||
        !CH_TxSectorBufferValid(
            backend,
            sector_buffer) ||
        record->scope_size !=
            scope_size ||
        record->key_size !=
            key_size ||
        (!scope &&
            scope_size != 0u) ||
        !key ||
        key_size == 0u ||
        scope_size >
            SIZE_MAX - key_size
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    if (!recordBodySize(
            record,
            &bodySize))
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    if (
        read_limit_sector >
            backend->sector_count ||
        record_sector <
            CH_TX_DATA_START_SECTOR ||
        record_sector >=
            read_limit_sector ||
        record->record_sectors == 0u ||
        record->record_sectors >
            read_limit_sector -
            record_sector
    )
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    identityEnd =
        scope_size +
        key_size;

    sectorBytes =
        (uint8_t *)sector_buffer;

    bodyOffset =
        0u;

    remaining =
        bodySize;

    crc =
        0xffffffffu;

    for (
        sectorIndex = 0u;
        sectorIndex <
            record->record_sectors;
        ++sectorIndex
    )
    {
        if (!backend->read_sector(
                backend->context,
                record_sector +
                    sectorIndex,
                sector_buffer))
        {
            return
                CH_TX_RESULT_IO;
        }

        if (sectorIndex == 0u)
        {
            if (!CH_TxDecodeRecordHeader(
                    &reread,
                    sectorBytes,
                    backend->sector_size))
            {
                return
                    CH_TX_RESULT_CORRUPT;
            }

            /*
             * The cursor already validated this record once.
             * Require the second identity pass to observe the same
             * metadata rather than comparing identity against bytes
             * from a changed record.
             */
            if (!recordIdentityHeaderMatches(
                    &reread,
                    record))
            {
                return
                    CH_TX_RESULT_CORRUPT;
            }
        }

        dataOffset =
            sectorIndex == 0u
            ? CH_TX_RECORD_HEADER_ENCODED_SIZE
            : 0u;

        available =
            (size_t)backend->sector_size -
            dataOffset;

        amount =
            remaining < available
            ? remaining
            : available;

        if (amount > 0u)
        {
            crc =
                bodyCrcUpdate(
                    crc,
                    sectorBytes +
                        dataOffset,
                    amount
                );

            /*
             * Only scope + key participate in identity comparison.
             * The stored payload is still included in body CRC
             * validation below.
             */
            for (i = 0u; i < amount; ++i)
            {
                position =
                    bodyOffset + i;

                if (position < scope_size)
                {
                    if (
                        sectorBytes[
                            dataOffset + i
                        ] !=
                        scopeBytes[position]
                    )
                    {
                        equal =
                            false;
                    }
                }
                else if (position < identityEnd)
                {
                    if (
                        sectorBytes[
                            dataOffset + i
                        ] !=
                        keyBytes[
                            position -
                            scope_size
                        ]
                    )
                    {
                        equal =
                            false;
                    }
                }
            }

            bodyOffset +=
                amount;

            remaining -=
                amount;
        }
    }

    if (
        remaining != 0u ||
        bodyOffset != bodySize
    )
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    crc ^=
        0xffffffffu;

    if (crc !=
        record->body_crc32)
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    *matches =
        equal;

    return
        CH_TX_RESULT_OK;
}


CH_TxResult CH_TxFindLatestRecord(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    uint32_t *record_sector,
    CH_TxRecordHeader *record)
{
    CH_TxLogCursor cursor;

    CH_TxRecordHeader current;
    CH_TxRecordHeader candidate;

    CH_TxResult result;

    uint32_t currentSector;
    uint32_t candidateSector =
        0u;

    bool found =
        false;

    bool matches;

    if (
        !record_sector ||
        !record ||
        (!scope &&
            scope_size != 0u) ||
        !key ||
        key_size == 0u ||
        scope_size > UINT32_MAX ||
        key_size > UINT32_MAX ||
        scope_size >
            SIZE_MAX - key_size
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    result =
        CH_TxOpenLogCursor(
            backend,
            sector_buffer,
            sector_buffer_size,
            &cursor
        );

    if (result != CH_TX_RESULT_OK)
    {
        return result;
    }

    for (;;)
    {
        result =
            CH_TxReadNextRecord(
                backend,
                sector_buffer,
                sector_buffer_size,
                &cursor,
                &currentSector,
                &current
            );

        if (result == CH_TX_RESULT_END)
        {
            break;
        }

        if (result != CH_TX_RESULT_OK)
        {
            return result;
        }

        /*
         * Different lengths can never be the same binary identity.
         * CH_TxReadNextRecord() has still fully validated the record,
         * including its body CRC.
         */
        if (
            current.scope_size !=
                scope_size ||
            current.key_size !=
                key_size
        )
        {
            continue;
        }

        result =
            recordIdentityMatches(
                backend,
                sector_buffer,
                sector_buffer_size,
                cursor.end_sector,
                currentSector,
                &current,
                scope,
                scope_size,
                key,
                key_size,
                &matches
            );

        if (result != CH_TX_RESULT_OK)
        {
            return result;
        }

        if (matches)
        {
            /*
             * Physical committed-log order wins. Keep replacing the
             * candidate as matching records are encountered.
             */
            candidateSector =
                currentSector;

            candidate =
                current;

            found =
                true;
        }
    }

    if (!found)
    {
        return
            CH_TX_RESULT_NOT_FOUND;
    }

    *record_sector =
        candidateSector;

    *record =
        candidate;

    return
        CH_TX_RESULT_OK;
}

/* ------------------------------------------------------------------------- */
/* Raw committed PUT payload                                                 */
/* ------------------------------------------------------------------------- */

static uint32_t rawPayloadCrc32(
    const void *data,
    size_t size)
{
    uint32_t crc =
        0xffffffffu;

    if (size != 0u)
    {
        crc =
            bodyCrcUpdate(
                crc,
                (const uint8_t *)data,
                size
            );
    }

    return
        crc ^ 0xffffffffu;
}


static CH_TxResult readZlibRawPayload(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    uint32_t read_limit_sector,
    uint32_t record_sector,
    const CH_TxRecordHeader *record,
    void *raw_output)
{
    z_stream stream;

    CH_TxRecordHeader reread;

    uint8_t inflateBuffer[1024];

    uint8_t *sectorBytes =
        (uint8_t *)sector_buffer;

    uint8_t *rawBytes =
        (uint8_t *)raw_output;

    size_t payloadStart;
    size_t payloadEnd;

    size_t bodyStart;
    size_t dataOffset;
    size_t available;
    size_t bodyEnd;

    size_t inputStart;
    size_t inputEnd;
    size_t inputAmount;

    size_t produced;
    size_t rawWritten =
        0u;

    size_t storedSeen =
        0u;

    uint32_t rawCrc =
        0xffffffffu;

    uint32_t sectorIndex;

    int zResult;

    bool streamEnded =
        false;


    if (
        (size_t)record->scope_size +
            (size_t)record->key_size <
        (size_t)record->scope_size
    )
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    payloadStart =
        (size_t)record->scope_size +
        (size_t)record->key_size;

    if (
        payloadStart +
            (size_t)record->stored_size <
        payloadStart
    )
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    payloadEnd =
        payloadStart +
        (size_t)record->stored_size;


    memset(
        &stream,
        0,
        sizeof(stream)
    );

    zResult =
        inflateInit(
            &stream
        );

    if (zResult != Z_OK)
    {
        return
            CH_TX_RESULT_CORRUPT;
    }


    for (
        sectorIndex = 0u;
        sectorIndex <
            record->record_sectors;
        ++sectorIndex
    )
    {
        if (!backend->read_sector(
                backend->context,
                record_sector +
                    sectorIndex,
                sector_buffer))
        {
            inflateEnd(
                &stream
            );

            return
                CH_TX_RESULT_IO;
        }


        if (sectorIndex == 0u)
        {
            if (!CH_TxDecodeRecordHeader(
                    &reread,
                    sectorBytes,
                    backend->sector_size))
            {
                inflateEnd(
                    &stream
                );

                return
                    CH_TX_RESULT_CORRUPT;
            }

            /*
             * The first pass already validated this committed record.
             * Do not decode payload bytes from a record whose metadata
             * changed between passes.
             */
            if (!recordIdentityHeaderMatches(
                    &reread,
                    record))
            {
                inflateEnd(
                    &stream
                );

                return
                    CH_TX_RESULT_CORRUPT;
            }
        }


        if (sectorIndex == 0u)
        {
            bodyStart =
                0u;

            dataOffset =
                CH_TX_RECORD_HEADER_ENCODED_SIZE;

            available =
                (size_t)backend->sector_size -
                dataOffset;
        }
        else
        {
            bodyStart =
                ((size_t)backend->sector_size -
                    CH_TX_RECORD_HEADER_ENCODED_SIZE) +
                ((size_t)sectorIndex - 1u) *
                    (size_t)backend->sector_size;

            dataOffset =
                0u;

            available =
                backend->sector_size;
        }

        bodyEnd =
            bodyStart +
            available;


        inputStart =
            bodyStart > payloadStart
            ? bodyStart
            : payloadStart;

        inputEnd =
            bodyEnd < payloadEnd
            ? bodyEnd
            : payloadEnd;


        if (inputStart >= inputEnd)
        {
            continue;
        }


        if (streamEnded)
        {
            inflateEnd(
                &stream
            );

            return
                CH_TX_RESULT_CORRUPT;
        }


        inputAmount =
            inputEnd -
            inputStart;

        storedSeen +=
            inputAmount;


        stream.next_in =
            (Bytef *)(
                sectorBytes +
                dataOffset +
                (inputStart - bodyStart)
            );

        stream.avail_in =
            (uInt)inputAmount;


        while (
            stream.avail_in != 0u &&
            !streamEnded
        )
        {
            uInt inputBefore =
                stream.avail_in;

            stream.next_out =
                inflateBuffer;

            stream.avail_out =
                (uInt)sizeof(inflateBuffer);


            zResult =
                inflate(
                    &stream,
                    Z_NO_FLUSH
                );


            produced =
                sizeof(inflateBuffer) -
                stream.avail_out;


            if (produced != 0u)
            {
                if (
                    rawWritten >
                        record->raw_size ||
                    produced >
                        (size_t)record->raw_size -
                        rawWritten
                )
                {
                    inflateEnd(
                        &stream
                    );

                    return
                        CH_TX_RESULT_CORRUPT;
                }

                if (rawBytes)
                {
                    memcpy(
                        rawBytes +
                            rawWritten,
                        inflateBuffer,
                        produced
                    );
                }

                rawCrc =
                    bodyCrcUpdate(
                        rawCrc,
                        inflateBuffer,
                        produced
                    );

                rawWritten +=
                    produced;
            }


            if (zResult == Z_STREAM_END)
            {
                streamEnded =
                    true;

                /*
                 * Format-v1 stored_size is exactly one zlib stream.
                 * Bytes remaining after the end marker are not accepted.
                 */
                if (stream.avail_in != 0u)
                {
                    inflateEnd(
                        &stream
                    );

                    return
                        CH_TX_RESULT_CORRUPT;
                }

                break;
            }


            if (zResult != Z_OK)
            {
                inflateEnd(
                    &stream
                );

                return
                    CH_TX_RESULT_CORRUPT;
            }


            /*
             * Defensive no-progress guard for malformed input.
             */
            if (
                stream.avail_in ==
                    inputBefore &&
                produced == 0u
            )
            {
                inflateEnd(
                    &stream
                );

                return
                    CH_TX_RESULT_CORRUPT;
            }
        }
    }


    inflateEnd(
        &stream
    );


    if (
        storedSeen !=
            record->stored_size ||
        !streamEnded ||
        rawWritten !=
            record->raw_size
    )
    {
        return
            CH_TX_RESULT_CORRUPT;
    }


    rawCrc ^=
        0xffffffffu;

    if (rawCrc !=
        record->raw_crc32)
    {
        return
            CH_TX_RESULT_CORRUPT;
    }


    (void)read_limit_sector;

    return
        CH_TX_RESULT_OK;
}


CH_TxResult CH_TxReadRawPayload(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    uint32_t record_sector,
    const CH_TxRecordHeader *record,
    void *raw_output,
    size_t raw_capacity)
{
    CH_TxSuperblock authoritative;

    CH_TxRecordHeader current;
    CH_TxRecordHeader reread;

    CH_TxResult result;

    uint32_t authoritativeSector;


    if (
        !record ||
        record->operation !=
            CH_TX_OPERATION_PUT ||
        (
            record->raw_size != 0u &&
            !raw_output
        )
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }


    result =
        CH_TxReadAuthoritativeSuperblock(
            backend,
            sector_buffer,
            sector_buffer_size,
            &authoritative,
            &authoritativeSector
        );

    if (result != CH_TX_RESULT_OK)
    {
        return result;
    }

    (void)authoritativeSector;


    /*
     * Revalidate that the supplied record is still inside the current
     * committed log and that the on-media metadata still matches.
     */
    result =
        CH_TxReadRecord(
            backend,
            sector_buffer,
            sector_buffer_size,
            authoritative.log_end_sector,
            record_sector,
            &current,
            NULL,
            0u,
            NULL,
            0u,
            NULL,
            0u
        );

    if (result != CH_TX_RESULT_OK)
    {
        return result;
    }


    if (!recordIdentityHeaderMatches(
            &current,
            record))
    {
        return
            CH_TX_RESULT_CORRUPT;
    }


    if (raw_capacity <
        current.raw_size)
    {
        return
            CH_TX_RESULT_BUFFER_TOO_SMALL;
    }


    if (current.codec ==
        CH_TX_CODEC_NONE)
    {
        /*
         * For NONE, format validation guarantees:
         *
         *   raw_size == stored_size
         *
         * ReadRecord() performs the second complete body validation
         * while copying the stored/raw bytes.
         */
        result =
            CH_TxReadRecord(
                backend,
                sector_buffer,
                sector_buffer_size,
                authoritative.log_end_sector,
                record_sector,
                &reread,
                NULL,
                0u,
                NULL,
                0u,
                raw_output,
                raw_capacity
            );

        if (result != CH_TX_RESULT_OK)
        {
            return result;
        }


        if (!recordIdentityHeaderMatches(
                &reread,
                &current))
        {
            return
                CH_TX_RESULT_CORRUPT;
        }


        if (
            rawPayloadCrc32(
                raw_output,
                current.raw_size
            ) != current.raw_crc32
        )
        {
            return
                CH_TX_RESULT_CORRUPT;
        }


        return
            CH_TX_RESULT_OK;
    }


    if (current.codec ==
        CH_TX_CODEC_ZLIB)
    {
        return
            readZlibRawPayload(
                backend,
                sector_buffer,
                authoritative.log_end_sector,
                record_sector,
                &current,
                raw_output
            );
    }


    /*
     * DecodeRecordHeader() should already have rejected codecs unknown
     * to format v1. Keep this defensive boundary anyway.
     */
    return
        CH_TX_RESULT_CORRUPT;
}
