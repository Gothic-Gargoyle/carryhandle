#include <carryhandle/ch_save_bundle.h>

#include <limits.h>
#include <string.h>

typedef struct CH_SaveBundleBufferSink
{
    uint8_t *output;
    size_t capacity;
    size_t offset;
} CH_SaveBundleBufferSink;

static uint32_t readBe32(
    const uint8_t *p)
{
    return
        ((uint32_t)p[0] << 24) |
        ((uint32_t)p[1] << 16) |
        ((uint32_t)p[2] << 8) |
        ((uint32_t)p[3]);
}

static void writeBe32(
    uint8_t *p,
    uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static bool addSize(
    size_t *value,
    size_t amount)
{
    if (!value ||
        amount > SIZE_MAX - *value)
    {
        return false;
    }

    *value += amount;

    return true;
}

static CH_SaveBundleResult validateEntryForEncode(
    const CH_SaveBundleEntry *entry)
{
    if (!entry ||
        !entry->name ||
        entry->name_size == 0u ||
        entry->name_size > UINT32_MAX ||
        entry->data_size > UINT32_MAX ||
        (!entry->data && entry->data_size))
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_ARGUMENT;
    }

    return CH_SAVE_BUNDLE_RESULT_OK;
}

CH_SaveBundleResult CH_SaveBundleMeasure(
    const CH_SaveBundleEntry *entries,
    size_t entry_count,
    size_t *encoded_size)
{
    size_t total =
        CH_SAVE_BUNDLE_HEADER_SIZE;

    size_t i;

    if (!entries ||
        !encoded_size ||
        entry_count == 0u)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_ARGUMENT;
    }

    if (entry_count > UINT32_MAX)
    {
        return CH_SAVE_BUNDLE_RESULT_OVERFLOW;
    }

    for (i = 0u;
         i < entry_count;
         ++i)
    {
        CH_SaveBundleResult result =
            validateEntryForEncode(
                &entries[i]
            );

        if (result !=
            CH_SAVE_BUNDLE_RESULT_OK)
        {
            return result;
        }

        if (!addSize(
                &total,
                CH_SAVE_BUNDLE_ENTRY_HEADER_SIZE) ||
            !addSize(
                &total,
                entries[i].name_size) ||
            !addSize(
                &total,
                entries[i].data_size))
        {
            return CH_SAVE_BUNDLE_RESULT_OVERFLOW;
        }
    }

    if (total > UINT32_MAX)
    {
        return CH_SAVE_BUNDLE_RESULT_OVERFLOW;
    }

    *encoded_size =
        total;

    return CH_SAVE_BUNDLE_RESULT_OK;
}

CH_SaveBundleResult CH_SaveBundleEmit(
    const CH_SaveBundleEntry *entries,
    size_t entry_count,
    CH_SaveBundleSinkFn sink,
    void *sink_context,
    size_t *encoded_size)
{
    uint8_t bundleHeader[
        CH_SAVE_BUNDLE_HEADER_SIZE
    ];

    uint8_t entryHeader[
        CH_SAVE_BUNDLE_ENTRY_HEADER_SIZE
    ];

    size_t required;
    size_t i;

    CH_SaveBundleResult result;

    if (!sink ||
        !encoded_size)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_ARGUMENT;
    }

    result =
        CH_SaveBundleMeasure(
            entries,
            entry_count,
            &required
        );

    if (result !=
        CH_SAVE_BUNDLE_RESULT_OK)
    {
        return result;
    }

    *encoded_size =
        required;

    writeBe32(
        bundleHeader + 0u,
        CH_SAVE_BUNDLE_MAGIC
    );

    writeBe32(
        bundleHeader + 4u,
        CH_SAVE_BUNDLE_VERSION
    );

    writeBe32(
        bundleHeader + 8u,
        (uint32_t)entry_count
    );

    writeBe32(
        bundleHeader + 12u,
        (uint32_t)required
    );

    if (!sink(
            sink_context,
            bundleHeader,
            sizeof(bundleHeader)))
    {
        return CH_SAVE_BUNDLE_RESULT_SINK_FAILED;
    }

    for (i = 0u;
         i < entry_count;
         ++i)
    {
        writeBe32(
            entryHeader + 0u,
            (uint32_t)entries[i].name_size
        );

        writeBe32(
            entryHeader + 4u,
            (uint32_t)entries[i].data_size
        );

        if (!sink(
                sink_context,
                entryHeader,
                sizeof(entryHeader)) ||
            !sink(
                sink_context,
                entries[i].name,
                entries[i].name_size))
        {
            return CH_SAVE_BUNDLE_RESULT_SINK_FAILED;
        }

        if (entries[i].data_size &&
            !sink(
                sink_context,
                entries[i].data,
                entries[i].data_size))
        {
            return CH_SAVE_BUNDLE_RESULT_SINK_FAILED;
        }
    }

    return CH_SAVE_BUNDLE_RESULT_OK;
}

static bool bufferSink(
    void *context,
    const void *data,
    size_t size)
{
    CH_SaveBundleBufferSink *sink =
        (CH_SaveBundleBufferSink *)context;

    if (!sink ||
        !data ||
        size == 0u ||
        sink->offset > sink->capacity ||
        size > sink->capacity - sink->offset)
    {
        return false;
    }

    memcpy(
        sink->output + sink->offset,
        data,
        size
    );

    sink->offset +=
        size;

    return true;
}

CH_SaveBundleResult CH_SaveBundleEncode(
    const CH_SaveBundleEntry *entries,
    size_t entry_count,
    void *output,
    size_t output_capacity,
    size_t *encoded_size)
{
    CH_SaveBundleBufferSink sink;

    size_t required;

    CH_SaveBundleResult result;

    if (!encoded_size)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_ARGUMENT;
    }

    result =
        CH_SaveBundleMeasure(
            entries,
            entry_count,
            &required
        );

    if (result !=
        CH_SAVE_BUNDLE_RESULT_OK)
    {
        return result;
    }

    *encoded_size =
        required;

    if (!output ||
        output_capacity < required)
    {
        return CH_SAVE_BUNDLE_RESULT_BUFFER_TOO_SMALL;
    }

    sink.output =
        (uint8_t *)output;

    sink.capacity =
        output_capacity;

    sink.offset =
        0u;

    result =
        CH_SaveBundleEmit(
            entries,
            entry_count,
            bufferSink,
            &sink,
            encoded_size
        );

    if (result !=
        CH_SAVE_BUNDLE_RESULT_OK)
    {
        return result;
    }

    if (sink.offset !=
        required)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
    }

    return CH_SAVE_BUNDLE_RESULT_OK;
}

static CH_SaveBundleResult validateEncoded(
    const uint8_t *encoded,
    size_t encoded_size,
    uint32_t *entry_count_out)
{
    uint32_t entry_count;
    uint32_t total_size;

    size_t offset =
        CH_SAVE_BUNDLE_HEADER_SIZE;

    uint32_t i;

    if (!encoded ||
        !entry_count_out)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_ARGUMENT;
    }

    if (encoded_size <
        CH_SAVE_BUNDLE_HEADER_SIZE)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
    }

    if (readBe32(
            encoded + 0u) !=
            CH_SAVE_BUNDLE_MAGIC ||
        readBe32(
            encoded + 4u) !=
            CH_SAVE_BUNDLE_VERSION)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
    }

    entry_count =
        readBe32(
            encoded + 8u
        );

    total_size =
        readBe32(
            encoded + 12u
        );

    if (entry_count == 0u ||
        (size_t)total_size !=
            encoded_size)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
    }

    /*
     * Before walking, prove that even the fixed headers can fit.
     * This also rejects hostile gigantic entry_count values immediately.
     */
    if ((size_t)entry_count >
        (encoded_size -
            CH_SAVE_BUNDLE_HEADER_SIZE) /
            CH_SAVE_BUNDLE_ENTRY_HEADER_SIZE)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
    }

    for (i = 0u;
         i < entry_count;
         ++i)
    {
        uint32_t name_size;
        uint32_t data_size;

        size_t remaining;

        if (offset >
                encoded_size ||
            encoded_size - offset <
                CH_SAVE_BUNDLE_ENTRY_HEADER_SIZE)
        {
            return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
        }

        name_size =
            readBe32(
                encoded + offset + 0u
            );

        data_size =
            readBe32(
                encoded + offset + 4u
            );

        offset +=
            CH_SAVE_BUNDLE_ENTRY_HEADER_SIZE;

        if (name_size == 0u)
        {
            return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
        }

        remaining =
            encoded_size - offset;

        if ((size_t)name_size >
            remaining)
        {
            return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
        }

        offset +=
            (size_t)name_size;

        remaining =
            encoded_size - offset;

        if ((size_t)data_size >
            remaining)
        {
            return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
        }

        offset +=
            (size_t)data_size;
    }

    if (offset !=
        encoded_size)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
    }

    *entry_count_out =
        entry_count;

    return CH_SAVE_BUNDLE_RESULT_OK;
}

CH_SaveBundleResult CH_SaveBundleDecodeBegin(
    CH_SaveBundleDecoder *decoder,
    const void *encoded,
    size_t encoded_size)
{
    uint32_t entry_count;

    CH_SaveBundleResult result;

    if (!decoder)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_ARGUMENT;
    }

    memset(
        decoder,
        0,
        sizeof(*decoder)
    );

    result =
        validateEncoded(
            (const uint8_t *)encoded,
            encoded_size,
            &entry_count
        );

    if (result !=
        CH_SAVE_BUNDLE_RESULT_OK)
    {
        return result;
    }

    decoder->encoded =
        (const uint8_t *)encoded;

    decoder->encoded_size =
        encoded_size;

    decoder->entry_count =
        entry_count;

    decoder->entry_index =
        0u;

    decoder->offset =
        CH_SAVE_BUNDLE_HEADER_SIZE;

    return CH_SAVE_BUNDLE_RESULT_OK;
}

CH_SaveBundleResult CH_SaveBundleDecodeNext(
    CH_SaveBundleDecoder *decoder,
    CH_SaveBundleView *entry)
{
    uint32_t name_size;
    uint32_t data_size;

    const uint8_t *encoded;

    size_t offset;

    if (!decoder ||
        !entry ||
        !decoder->encoded)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_ARGUMENT;
    }

    if (decoder->entry_index >=
        decoder->entry_count)
    {
        return CH_SAVE_BUNDLE_RESULT_DONE;
    }

    encoded =
        decoder->encoded;

    offset =
        decoder->offset;

    /*
     * DecodeBegin already performed complete structural validation.
     * Keep these cheap local checks anyway so a damaged decoder state
     * cannot turn into an out-of-bounds view.
     */
    if (offset >
            decoder->encoded_size ||
        decoder->encoded_size - offset <
            CH_SAVE_BUNDLE_ENTRY_HEADER_SIZE)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
    }

    name_size =
        readBe32(
            encoded + offset + 0u
        );

    data_size =
        readBe32(
            encoded + offset + 4u
        );

    offset +=
        CH_SAVE_BUNDLE_ENTRY_HEADER_SIZE;

    if (name_size == 0u ||
        (size_t)name_size >
            decoder->encoded_size - offset)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
    }

    entry->name =
        encoded + offset;

    entry->name_size =
        (size_t)name_size;

    offset +=
        (size_t)name_size;

    if ((size_t)data_size >
        decoder->encoded_size - offset)
    {
        return CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT;
    }

    entry->data =
        encoded + offset;

    entry->data_size =
        (size_t)data_size;

    offset +=
        (size_t)data_size;

    decoder->offset =
        offset;

    decoder->entry_index++;

    return CH_SAVE_BUNDLE_RESULT_OK;
}
