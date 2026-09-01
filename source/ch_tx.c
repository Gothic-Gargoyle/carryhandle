#include <carryhandle/ch_tx.h>

#include <string.h>


static bool superblockGeometryMatches(
    const CH_TxSectorBackend *backend,
    const CH_TxSuperblock *superblock)
{
    if (
        !backend ||
        !superblock
    )
    {
        return false;
    }

    return
        superblock->sector_size ==
            backend->sector_size &&
        superblock->container_sectors ==
            backend->sector_count;
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


CH_TxResult CH_TxReadAuthoritativeSuperblock(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    CH_TxSuperblock *superblock,
    uint32_t *superblock_sector)
{
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
