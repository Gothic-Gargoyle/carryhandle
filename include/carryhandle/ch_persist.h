#ifndef CARRYHANDLE_CH_PERSIST_H
#define CARRYHANDLE_CH_PERSIST_H

#include <stddef.h>

#include <carryhandle/ch_tx_backend.h>


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Persistent-object result model.
 *
 * END is deliberately absent: iteration is a transaction-layer concern,
 * not an object-store concern.
 */
typedef enum CH_PersistResult
{
    CH_PERSIST_RESULT_OK = 0,
    CH_PERSIST_RESULT_NOT_FOUND = 1,

    CH_PERSIST_RESULT_INVALID_ARGUMENT = -1,
    CH_PERSIST_RESULT_IO = -2,
    CH_PERSIST_RESULT_CORRUPT = -3,
    CH_PERSIST_RESULT_AMBIGUOUS = -4,
    CH_PERSIST_RESULT_BUFFER_TOO_SMALL = -5,
    CH_PERSIST_RESULT_COMMIT_UNCERTAIN = -6

} CH_PersistResult;


/*
 * Read the current committed object identified by exact binary
 * scope + key.
 *
 * scope:
 *   optional; NULL is valid only when scope_size == 0.
 *
 * key:
 *   required and non-empty.
 *
 * Identity comparison is byte-exact and case-sensitive.
 *
 * The newest committed transaction record determines state:
 *
 *   PUT    -> return the verified original blob
 *   DELETE -> CH_PERSIST_RESULT_NOT_FOUND
 *   absent -> CH_PERSIST_RESULT_NOT_FOUND
 *
 * output may be NULL only when output_capacity == 0.
 *
 * On CH_PERSIST_RESULT_OK:
 *   *object_size is the exact blob size and output contains that blob.
 *
 * For a zero-byte object:
 *   output == NULL and output_capacity == 0 are valid and return OK.
 *
 * On CH_PERSIST_RESULT_BUFFER_TOO_SMALL:
 *   *object_size is set to the required blob size.
 *   output is unchanged.
 *
 * On NOT_FOUND or any other error:
 *   *object_size is unchanged.
 *
 * On errors that occur after payload decoding begins, output contents
 * are unspecified. Callers must use output only when the result is OK.
 */
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
    size_t *object_size
);


/*
 * Atomically replace the persistent object identified by exact binary
 * scope + key with the supplied blob.
 *
 * scope is optional; key is required and non-empty.
 * data may be NULL only when data_size == 0.
 *
 * A zero-byte PUT creates an existing zero-byte object; it is distinct
 * from DELETE.
 *
 * Codec selection is an implementation detail. Callers always supply the
 * original uncompressed blob.
 *
 * CH_PERSIST_RESULT_COMMIT_UNCERTAIN means the publication boundary may
 * have become durable. Recover storage state before attempting another
 * write.
 */
CH_PersistResult CH_PersistPut(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    const void *data,
    size_t data_size
);



/*
 * Atomically delete the persistent object identified by exact binary
 * scope + key.
 *
 * Deletion is represented by a committed tombstone. Deleting an object
 * that is already absent is valid and returns OK.
 *
 * CH_PERSIST_RESULT_COMMIT_UNCERTAIN means the publication boundary may
 * have become durable. Recover storage state before another write.
 */
CH_PersistResult CH_PersistDelete(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size
);



#ifdef __cplusplus
}
#endif

#endif
