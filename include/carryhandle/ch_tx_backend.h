#ifndef CARRYHANDLE_CH_TX_BACKEND_H
#define CARRYHANDLE_CH_TX_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/*
 * Sector-oriented storage backend used by the CarryHandle
 * transactional store.
 *
 * The transaction layer performs only complete-sector I/O.
 *
 * A successful write_sector() followed by a successful sync()
 * establishes the durability barrier required before publishing
 * transactional metadata.
 *
 * buffer_alignment is the minimum alignment required for sector
 * buffers:
 *
 *   1  -> no special alignment
 *   32 -> suitable for GameCube CARD I/O
 *
 * It must be a non-zero power of two.
 */


typedef bool (*CH_TxSectorReadFn)(
    void *context,
    uint32_t sector_index,
    void *sector_buffer
);


typedef bool (*CH_TxSectorWriteFn)(
    void *context,
    uint32_t sector_index,
    const void *sector_buffer
);


typedef bool (*CH_TxSectorSyncFn)(
    void *context
);


typedef struct CH_TxSectorBackend
{
    void *context;

    uint32_t sector_size;
    uint32_t sector_count;

    size_t buffer_alignment;

    CH_TxSectorReadFn read_sector;
    CH_TxSectorWriteFn write_sector;
    CH_TxSectorSyncFn sync;

} CH_TxSectorBackend;


/*
 * Validate the runtime backend contract.
 *
 * Format-specific geometry is deliberately not checked here.
 */
static inline bool CH_TxSectorBackendValid(
    const CH_TxSectorBackend *backend)
{
    if (!backend)
    {
        return false;
    }

    if (
        backend->sector_size == 0u ||
        backend->sector_count == 0u ||
        backend->buffer_alignment == 0u ||
        !backend->read_sector ||
        !backend->write_sector ||
        !backend->sync
    )
    {
        return false;
    }

    return
        (
            backend->buffer_alignment &
            (backend->buffer_alignment - 1u)
        ) == 0u;
}


/*
 * Check whether a caller-supplied sector buffer satisfies the backend's
 * alignment requirement.
 */
static inline bool CH_TxSectorBufferValid(
    const CH_TxSectorBackend *backend,
    const void *buffer)
{
    if (
        !CH_TxSectorBackendValid(backend) ||
        !buffer
    )
    {
        return false;
    }

    return
        (
            (uintptr_t)buffer &
            (uintptr_t)(
                backend->buffer_alignment - 1u
            )
        ) == 0u;
}


#endif
