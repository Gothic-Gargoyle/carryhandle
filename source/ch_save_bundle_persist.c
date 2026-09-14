#include <carryhandle/ch_save_bundle_persist.h>

typedef struct CH_SaveBundlePersistProducer
{
    const CH_SaveBundleEntry *entries;
    size_t entry_count;
    size_t encoded_size;

} CH_SaveBundlePersistProducer;


static bool saveBundlePersistProducer(
    void *context,
    CH_PersistStreamSinkFn sink,
    void *sink_context)
{
    CH_SaveBundlePersistProducer *producer =
        (CH_SaveBundlePersistProducer *)context;

    size_t emitted_size =
        0u;

    CH_SaveBundleResult bundle_result;

    if (!producer ||
        !sink)
    {
        return false;
    }

    bundle_result =
        CH_SaveBundleEmit(
            producer->entries,
            producer->entry_count,
            sink,
            sink_context,
            &emitted_size
        );

    return
        bundle_result ==
            CH_SAVE_BUNDLE_RESULT_OK &&
        emitted_size ==
            producer->encoded_size;
}


CH_PersistResult CH_SaveBundlePersistPut(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    const CH_SaveBundleEntry *entries,
    size_t entry_count)
{
    CH_SaveBundlePersistProducer producer;

    size_t encoded_size =
        0u;

    CH_SaveBundleResult bundle_result;

    bundle_result =
        CH_SaveBundleMeasure(
            entries,
            entry_count,
            &encoded_size
        );

    if (bundle_result ==
        CH_SAVE_BUNDLE_RESULT_OVERFLOW)
    {
        return
            CH_PERSIST_RESULT_NO_SPACE;
    }

    if (bundle_result !=
        CH_SAVE_BUNDLE_RESULT_OK)
    {
        return
            CH_PERSIST_RESULT_INVALID_ARGUMENT;
    }

    producer.entries =
        entries;

    producer.entry_count =
        entry_count;

    producer.encoded_size =
        encoded_size;

    return
        CH_PersistPutStream(
            backend,
            sector_buffer,
            sector_buffer_size,
            scope,
            scope_size,
            key,
            key_size,
            encoded_size,
            saveBundlePersistProducer,
            &producer
        );
}
