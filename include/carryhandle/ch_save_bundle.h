#ifndef CARRYHANDLE_CH_SAVE_BUNDLE_H
#define CARRYHANDLE_CH_SAVE_BUNDLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generic CarryHandle save bundle.
 *
 * CH_SaveBundle deliberately knows nothing about Doom, Quake, GoldSrc,
 * filesystems, Memory Cards, transactions, compression, or catalogs.
 *
 * It only maps a bounded array of named opaque byte blobs to/from one
 * deterministic big-endian byte representation.
 *
 * Format v1:
 *
 *   bundle header:
 *     u32 BE magic       "CHB1"
 *     u32 BE version     1
 *     u32 BE entry_count
 *     u32 BE total_size
 *
 *   repeated entry_count times:
 *     u32 BE name_size
 *     u32 BE data_size
 *     byte   name[name_size]
 *     byte   data[data_size]
 *
 * Names are length-delimited and are not NUL-terminated in the encoded form.
 * Name contents are intentionally opaque to this layer.
 */

#define CH_SAVE_BUNDLE_MAGIC          0x43484231u /* "CHB1" */
#define CH_SAVE_BUNDLE_VERSION        1u
#define CH_SAVE_BUNDLE_HEADER_SIZE    16u
#define CH_SAVE_BUNDLE_ENTRY_HEADER_SIZE 8u

typedef enum CH_SaveBundleResult
{
    CH_SAVE_BUNDLE_RESULT_OK = 0,
    CH_SAVE_BUNDLE_RESULT_DONE,
    CH_SAVE_BUNDLE_RESULT_INVALID_ARGUMENT,
    CH_SAVE_BUNDLE_RESULT_INVALID_FORMAT,
    CH_SAVE_BUNDLE_RESULT_BUFFER_TOO_SMALL,
    CH_SAVE_BUNDLE_RESULT_OVERFLOW,
    CH_SAVE_BUNDLE_RESULT_SINK_FAILED
} CH_SaveBundleResult;

typedef struct CH_SaveBundleEntry
{
    const void *name;
    size_t name_size;

    const void *data;
    size_t data_size;
} CH_SaveBundleEntry;

typedef struct CH_SaveBundleView
{
    const uint8_t *name;
    size_t name_size;

    const uint8_t *data;
    size_t data_size;
} CH_SaveBundleView;

typedef struct CH_SaveBundleDecoder
{
    const uint8_t *encoded;
    size_t encoded_size;

    uint32_t entry_count;
    uint32_t entry_index;

    size_t offset;
} CH_SaveBundleDecoder;

/*
 * Piecewise CHB1 output sink.
 *
 * The pointed-to bytes are borrowed and are valid only for the duration of
 * the call. A non-zero size is always supplied.
 *
 * Return false to abort emission.
 */
typedef bool (*CH_SaveBundleSinkFn)(
    void *context,
    const void *data,
    size_t size);

/*
 * Calculate the exact encoded size for entries.
 *
 * entry_count must be non-zero.
 * Each entry must have a non-empty name.
 * A zero-byte data blob is valid.
 */
CH_SaveBundleResult CH_SaveBundleMeasure(
    const CH_SaveBundleEntry *entries,
    size_t entry_count,
    size_t *encoded_size);

/*
 * Emit one complete CHB1 representation without constructing a contiguous
 * raw bundle.
 *
 * Pieces are emitted in exact encoded order:
 *   bundle header
 *   entry header
 *   entry name
 *   entry data, when non-empty
 *   ...
 *
 * The output is byte-for-byte identical to CH_SaveBundleEncode().
 *
 * *encoded_size receives the exact complete size before the first sink call,
 * so it remains useful even if the sink later fails.
 */
CH_SaveBundleResult CH_SaveBundleEmit(
    const CH_SaveBundleEntry *entries,
    size_t entry_count,
    CH_SaveBundleSinkFn sink,
    void *sink_context,
    size_t *encoded_size);

/*
 * Encode one complete CHB1 bundle into a contiguous caller-owned buffer.
 *
 * This is implemented through CH_SaveBundleEmit(), keeping one authoritative
 * framing path.
 *
 * On BUFFER_TOO_SMALL, *encoded_size still receives the required size.
 */
CH_SaveBundleResult CH_SaveBundleEncode(
    const CH_SaveBundleEntry *entries,
    size_t entry_count,
    void *output,
    size_t output_capacity,
    size_t *encoded_size);

/*
 * Validate an entire CHB1 object and initialize an allocation-free iterator.
 *
 * Validation is complete, not lazy: malformed/truncated entries and trailing
 * bytes are rejected here before the first CH_SaveBundleDecodeNext().
 */
CH_SaveBundleResult CH_SaveBundleDecodeBegin(
    CH_SaveBundleDecoder *decoder,
    const void *encoded,
    size_t encoded_size);

/*
 * Return the next entry as views into the caller-owned encoded buffer.
 *
 * The views remain valid only while that encoded buffer remains unchanged.
 */
CH_SaveBundleResult CH_SaveBundleDecodeNext(
    CH_SaveBundleDecoder *decoder,
    CH_SaveBundleView *entry);

#ifdef __cplusplus
}
#endif

#endif
