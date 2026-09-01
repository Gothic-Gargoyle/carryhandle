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
        authoritative->sector_size !=
            backend->sector_size ||
        authoritative->container_sectors !=
            backend->sector_count ||
        authoritative->log_start_sector !=
            CH_TX_DATA_START_SECTOR ||
        authoritative->log_end_sector <
            authoritative->log_start_sector ||
        authoritative->log_end_sector >
            backend->sector_count
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
            authoritative->log_end_sector ||
        new_log_end_sector >
            backend->sector_count
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
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
