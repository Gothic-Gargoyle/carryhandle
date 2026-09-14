#ifndef CARRYHANDLE_CH_SAVE_BUNDLE_PERSIST_H
#define CARRYHANDLE_CH_SAVE_BUNDLE_PERSIST_H

#include <stddef.h>

#include <carryhandle/ch_persist.h>
#include <carryhandle/ch_save_bundle.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Persist a complete CH_SaveBundle directly through CH_PersistPutStream().
 *
 * The bundle is measured once, then emitted piecewise into CarryHandle's
 * streamed persistence path. No contiguous raw CHB1 allocation is created.
 *
 * Result mapping before persistence begins:
 *
 *   invalid bundle arguments -> CH_PERSIST_RESULT_INVALID_ARGUMENT
 *   encoded-size overflow    -> CH_PERSIST_RESULT_NO_SPACE
 *
 * Once streaming begins, CH_PersistPutStream() owns codec, allocation,
 * transaction, compaction, and durability result semantics.
 */
CH_PersistResult CH_SaveBundlePersistPut(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    const CH_SaveBundleEntry *entries,
    size_t entry_count);

#ifdef __cplusplus
}
#endif

#endif
