#include <carryhandle/ch_persist.h>

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
