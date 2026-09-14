#include <carryhandle/ch_persist.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include <carryhandle/ch_tx.h>


static CH_PersistResult persistResultFromTx(
    CH_TxResult result)
{
    switch (result)
    {
        case CH_TX_RESULT_OK:
            return
                CH_PERSIST_RESULT_OK;

        case CH_TX_RESULT_NOT_FOUND:
            return
                CH_PERSIST_RESULT_NOT_FOUND;

        case CH_TX_RESULT_INVALID_ARGUMENT:
            return
                CH_PERSIST_RESULT_INVALID_ARGUMENT;

        case CH_TX_RESULT_IO:
            return
                CH_PERSIST_RESULT_IO;

        case CH_TX_RESULT_CORRUPT:
            return
                CH_PERSIST_RESULT_CORRUPT;

        case CH_TX_RESULT_AMBIGUOUS:
            return
                CH_PERSIST_RESULT_AMBIGUOUS;

        case CH_TX_RESULT_BUFFER_TOO_SMALL:
            return
                CH_PERSIST_RESULT_BUFFER_TOO_SMALL;

        case CH_TX_RESULT_COMMIT_UNCERTAIN:
            return
                CH_PERSIST_RESULT_COMMIT_UNCERTAIN;

        case CH_TX_RESULT_NO_SPACE:
            return
                CH_PERSIST_RESULT_NO_SPACE;

        case CH_TX_RESULT_NO_MEMORY:
            return
                CH_PERSIST_RESULT_NO_MEMORY;

        case CH_TX_RESULT_END:
        default:
            /*
             * END is meaningful only to transaction-log iteration.
             * Receiving it here indicates an internal contract violation.
             */
            return
                CH_PERSIST_RESULT_CORRUPT;
    }
}


CH_PersistResult CH_PersistGet(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    void *output,
    size_t output_capacity,
    size_t *object_size)
{
    CH_TxRecordHeader record;

    CH_TxResult txResult;

    uint32_t recordSector;

    size_t requiredSize;


    if (
        !object_size ||
        (!output &&
            output_capacity != 0u)
    )
    {
        return
            CH_PERSIST_RESULT_INVALID_ARGUMENT;
    }


    txResult =
        CH_TxFindLatestRecord(
            backend,
            sector_buffer,
            sector_buffer_size,
            scope,
            scope_size,
            key,
            key_size,
            &recordSector,
            &record
        );


    if (txResult ==
        CH_TX_RESULT_NOT_FOUND)
    {
        return
            CH_PERSIST_RESULT_NOT_FOUND;
    }


    if (txResult !=
        CH_TX_RESULT_OK)
    {
        return
            persistResultFromTx(
                txResult
            );
    }


    if (record.operation ==
        CH_TX_OPERATION_DELETE)
    {
        return
            CH_PERSIST_RESULT_NOT_FOUND;
    }


    if (record.operation !=
        CH_TX_OPERATION_PUT)
    {
        return
            CH_PERSIST_RESULT_CORRUPT;
    }


    requiredSize =
        (size_t)record.raw_size;


    /*
     * Determine capacity before asking the transaction layer to decode.
     * This guarantees BUFFER_TOO_SMALL does not touch the payload buffer.
     */
    if (output_capacity <
        requiredSize)
    {
        *object_size =
            requiredSize;

        return
            CH_PERSIST_RESULT_BUFFER_TOO_SMALL;
    }


    if (
        requiredSize != 0u &&
        !output
    )
    {
        /*
         * This should normally have been caught by the capacity check,
         * but keep the public pointer contract explicit.
         */
        return
            CH_PERSIST_RESULT_INVALID_ARGUMENT;
    }


    txResult =
        CH_TxReadRawPayload(
            backend,
            sector_buffer,
            sector_buffer_size,
            recordSector,
            &record,
            output,
            output_capacity
        );


    if (txResult !=
        CH_TX_RESULT_OK)
    {
        return
            persistResultFromTx(
                txResult
            );
    }


    *object_size =
        requiredSize;


    return
        CH_PERSIST_RESULT_OK;
}


/* ------------------------------------------------------------------------- */
/* Append with transparent arena compaction                                  */
/* ------------------------------------------------------------------------- */

static CH_TxResult persistAppendWithCompaction(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const CH_TxAppendRequest *request)
{
    CH_TxResult result;


    result =
        CH_TxAppendRecord(
            backend,
            sector_buffer,
            sector_buffer_size,
            request,
            NULL
        );


    if (result !=
        CH_TX_RESULT_NO_SPACE)
    {
        return result;
    }


    result =
        CH_TxCompact(
            backend,
            sector_buffer,
            sector_buffer_size
        );


    if (result !=
        CH_TX_RESULT_OK)
    {
        return result;
    }


    /*
     * Retry exactly once.
     *
     * If the compacted live set plus this record still cannot fit in one
     * arena, the second append correctly returns NO_SPACE.
     */
    return
        CH_TxAppendRecord(
            backend,
            sector_buffer,
            sector_buffer_size,
            request,
            NULL
        );
}


/* ------------------------------------------------------------------------- */
/* Persistent object PUT                                                     */
/* ------------------------------------------------------------------------- */

static uint32_t persistCrc32(
    const void *data,
    size_t size)
{
    const uint8_t *bytes =
        (const uint8_t *)data;

    uint32_t crc =
        0xffffffffu;

    size_t i;
    unsigned bit;

    for (i = 0u; i < size; ++i)
    {
        crc ^=
            bytes[i];

        for (bit = 0u; bit < 8u; ++bit)
        {
            uint32_t mask =
                0u - (crc & 1u);

            crc =
                (crc >> 1) ^
                (0xedb88320u & mask);
        }
    }

    return
        crc ^ 0xffffffffu;
}



/* ------------------------------------------------------------------------- */
/* Streamed persistent-object PUT                                             */
/* ------------------------------------------------------------------------- */

#define CH_PERSIST_STREAM_INITIAL_CAPACITY (64u * 1024u)

typedef struct PersistStreamDeflate
{
    z_stream stream;

    unsigned char *stored;

    size_t stored_capacity;
    size_t stored_limit;

    size_t raw_expected;
    size_t raw_written;

    uLong raw_crc;

    CH_PersistResult failure;

} PersistStreamDeflate;


static CH_PersistResult persistZlibResult(
    int z_result)
{
    if (z_result == Z_MEM_ERROR)
    {
        return
            CH_PERSIST_RESULT_NO_MEMORY;
    }

    return
        CH_PERSIST_RESULT_CODEC;
}


static bool persistStreamGrowStored(
    PersistStreamDeflate *state)
{
    size_t nextCapacity;
    size_t nextOffset;

    unsigned char *grown;

    if (!state ||
        state->failure !=
            CH_PERSIST_RESULT_OK)
    {
        return false;
    }

    if (state->stored_capacity >=
        state->stored_limit)
    {
        state->failure =
            CH_PERSIST_RESULT_NO_SPACE;

        return false;
    }

    nextOffset =
        state->stored
            ? (size_t)(
                state->stream.next_out -
                (Bytef *)state->stored)
            : 0u;

    if (state->stored_capacity == 0u)
    {
        nextCapacity =
            CH_PERSIST_STREAM_INITIAL_CAPACITY;
    }
    else if (state->stored_capacity >
        SIZE_MAX / 2u)
    {
        nextCapacity =
            state->stored_limit;
    }
    else
    {
        nextCapacity =
            state->stored_capacity * 2u;
    }

    if (nextCapacity >
        state->stored_limit)
    {
        nextCapacity =
            state->stored_limit;
    }

    if (nextCapacity <=
            state->stored_capacity ||
        nextOffset >
            nextCapacity)
    {
        state->failure =
            CH_PERSIST_RESULT_NO_SPACE;

        return false;
    }

    grown =
        (unsigned char *)realloc(
            state->stored,
            nextCapacity
        );

    if (!grown)
    {
        state->failure =
            CH_PERSIST_RESULT_NO_MEMORY;

        return false;
    }

    state->stored =
        grown;

    state->stored_capacity =
        nextCapacity;

    state->stream.next_out =
        (Bytef *)state->stored +
        nextOffset;

    state->stream.avail_out =
        (uInt)(
            state->stored_capacity -
            nextOffset
        );

    return true;
}


static bool persistStreamDeflateSink(
    void *context,
    const void *data,
    size_t size)
{
    PersistStreamDeflate *state =
        (PersistStreamDeflate *)context;

    const Bytef *bytes =
        (const Bytef *)data;

    size_t remaining =
        size;

    if (!state ||
        !data ||
        size == 0u ||
        state->failure !=
            CH_PERSIST_RESULT_OK)
    {
        return false;
    }

    if (state->raw_written >
            state->raw_expected ||
        size >
            state->raw_expected -
                state->raw_written)
    {
        state->failure =
            CH_PERSIST_RESULT_SOURCE_FAILED;

        return false;
    }

    while (remaining > 0u)
    {
        uInt amount =
            remaining > (size_t)UINT_MAX
                ? UINT_MAX
                : (uInt)remaining;

        int zResult;

        state->raw_crc =
            crc32(
                state->raw_crc,
                bytes,
                amount
            );

        state->raw_written +=
            (size_t)amount;

        state->stream.next_in =
            (Bytef *)(void *)bytes;

        state->stream.avail_in =
            amount;

        while (state->stream.avail_in)
        {
            if (state->stream.avail_out == 0u &&
                !persistStreamGrowStored(
                    state))
            {
                return false;
            }

            zResult =
                deflate(
                    &state->stream,
                    Z_NO_FLUSH
                );

            if (zResult !=
                Z_OK)
            {
                state->failure =
                    persistZlibResult(
                        zResult
                    );

                return false;
            }
        }

        bytes +=
            amount;

        remaining -=
            (size_t)amount;
    }

    return true;
}


static bool persistStreamFinish(
    PersistStreamDeflate *state)
{
    for (;;)
    {
        int zResult;

        if (state->stream.avail_out == 0u &&
            !persistStreamGrowStored(
                state))
        {
            return false;
        }

        zResult =
            deflate(
                &state->stream,
                Z_FINISH
            );

        if (zResult ==
            Z_STREAM_END)
        {
            return true;
        }

        if (zResult !=
            Z_OK)
        {
            state->failure =
                persistZlibResult(
                    zResult
                );

            return false;
        }
    }
}


CH_PersistResult CH_PersistPutStream(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    size_t raw_size,
    CH_PersistStreamProducerFn producer,
    void *producer_context)
{
    CH_TxAppendRequest request = {0};

    PersistStreamDeflate state;

    CH_TxResult txResult;

    size_t containerBytes;

    uLong zBound;

    int zResult;
    bool initialized =
        false;

    bool producerOk;

    CH_PersistResult result;

    if (!backend ||
        !sector_buffer ||
        !producer ||
        (!scope && scope_size != 0u) ||
        !key ||
        key_size == 0u ||
        raw_size > UINT32_MAX ||
        backend->sector_size == 0u ||
        backend->sector_count == 0u ||
        (size_t)backend->sector_count >
            SIZE_MAX /
                (size_t)backend->sector_size)
    {
        return
            CH_PERSIST_RESULT_INVALID_ARGUMENT;
    }

    containerBytes =
        (size_t)backend->sector_count *
        (size_t)backend->sector_size;

    memset(
        &state,
        0,
        sizeof(state)
    );

    state.raw_expected =
        raw_size;

    state.raw_crc =
        crc32(
            0L,
            Z_NULL,
            0
        );

    state.failure =
        CH_PERSIST_RESULT_OK;

    zResult =
        deflateInit2(
            &state.stream,
            Z_BEST_SPEED,
            Z_DEFLATED,
            MAX_WBITS,
            6,
            Z_DEFAULT_STRATEGY
        );

    if (zResult !=
        Z_OK)
    {
        return
            persistZlibResult(
                zResult
            );
    }

    initialized =
        true;

    zBound =
        deflateBound(
            &state.stream,
            (uLong)raw_size
        );

    state.stored_limit =
        (size_t)zBound;

    if (state.stored_limit >
        containerBytes)
    {
        state.stored_limit =
            containerBytes;
    }

    if (state.stored_limit >
        (size_t)UINT_MAX)
    {
        state.stored_limit =
            (size_t)UINT_MAX;
    }

    if (state.stored_limit == 0u ||
        !persistStreamGrowStored(
            &state))
    {
        result =
            state.failure !=
                CH_PERSIST_RESULT_OK
                ? state.failure
                : CH_PERSIST_RESULT_NO_SPACE;

        goto done;
    }

    producerOk =
        producer(
            producer_context,
            persistStreamDeflateSink,
            &state
        );

    if (!producerOk)
    {
        result =
            state.failure !=
                CH_PERSIST_RESULT_OK
                ? state.failure
                : CH_PERSIST_RESULT_SOURCE_FAILED;

        goto done;
    }

    if (state.failure !=
            CH_PERSIST_RESULT_OK ||
        state.raw_written !=
            state.raw_expected)
    {
        result =
            state.failure !=
                CH_PERSIST_RESULT_OK
                ? state.failure
                : CH_PERSIST_RESULT_SOURCE_FAILED;

        goto done;
    }

    if (!persistStreamFinish(
            &state))
    {
        result =
            state.failure !=
                CH_PERSIST_RESULT_OK
                ? state.failure
                : CH_PERSIST_RESULT_CODEC;

        goto done;
    }

    request.operation =
        CH_TX_OPERATION_PUT;

    request.scope =
        scope;

    request.scope_size =
        scope_size;

    request.key =
        key;

    request.key_size =
        key_size;

    request.stored_payload =
        state.stored;

    request.stored_size =
        (size_t)state.stream.total_out;

    request.raw_size =
        (uint32_t)raw_size;

    request.raw_crc32 =
        (uint32_t)state.raw_crc;

    request.codec =
        CH_TX_CODEC_ZLIB;

    txResult =
        persistAppendWithCompaction(
            backend,
            sector_buffer,
            sector_buffer_size,
            &request
        );

    result =
        persistResultFromTx(
            txResult
        );

done:
    if (initialized)
    {
        (void)deflateEnd(
            &state.stream
        );
    }

    free(
        state.stored
    );

    return result;
}


CH_PersistResult CH_PersistPut(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    const void *data,
    size_t data_size)
{
    CH_TxAppendRequest request = {0};

    CH_TxResult result;

    if (
        (!data && data_size != 0u) ||
        data_size > UINT32_MAX
    )
    {
        return
            CH_PERSIST_RESULT_INVALID_ARGUMENT;
    }

    request.operation =
        CH_TX_OPERATION_PUT;

    request.scope =
        scope;

    request.scope_size =
        scope_size;

    request.key =
        key;

    request.key_size =
        key_size;

    /*
     * Initial persistent-object writer stores raw bytes directly.
     * Codec policy remains hidden behind this API and can change later.
     */
    request.stored_payload =
        data;

    request.stored_size =
        data_size;

    request.raw_size =
        (uint32_t)data_size;

    request.raw_crc32 =
        persistCrc32(
            data,
            data_size
        );

    request.codec =
        CH_TX_CODEC_NONE;

    result =
        persistAppendWithCompaction(
            backend,
            sector_buffer,
            sector_buffer_size,
            &request
        );

    return
        persistResultFromTx(
            result
        );
}

/* ------------------------------------------------------------------------- */
/* Persistent object DELETE                                                  */
/* ------------------------------------------------------------------------- */

CH_PersistResult CH_PersistDelete(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size)
{
    CH_TxAppendRequest request = {0};

    CH_TxResult result;

    request.operation =
        CH_TX_OPERATION_DELETE;

    request.scope =
        scope;

    request.scope_size =
        scope_size;

    request.key =
        key;

    request.key_size =
        key_size;

    request.stored_payload =
        NULL;

    request.stored_size =
        0u;

    request.raw_size =
        0u;

    request.raw_crc32 =
        0u;

    request.codec =
        CH_TX_CODEC_NONE;

    result =
        persistAppendWithCompaction(
            backend,
            sector_buffer,
            sector_buffer_size,
            &request
        );

    return
        persistResultFromTx(
            result
        );
}
