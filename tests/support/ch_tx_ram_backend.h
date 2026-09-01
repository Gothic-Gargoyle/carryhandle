#ifndef CH_TX_RAM_BACKEND_H
#define CH_TX_RAM_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#include <carryhandle/ch_tx_backend.h>


/*
 * Host-side transaction test backend.
 *
 * working:
 *   state visible to reads during the current run.
 *
 * durable:
 *   state preserved by the most recent successful sync().
 *
 * A simulated crash discards working state and restores durable state.
 *
 * fail_*_call:
 *   zero disables failure injection.
 *   otherwise the matching numbered callback fails with no side effect.
 */
typedef struct CH_TxRamBackend
{
    uint8_t *working;
    uint8_t *durable;

    size_t storage_size;

    uint32_t sector_size;
    uint32_t sector_count;

    uint32_t read_calls;
    uint32_t write_calls;
    uint32_t sync_calls;

    uint32_t fail_read_call;
    uint32_t fail_write_call;
    uint32_t fail_sync_call;

} CH_TxRamBackend;


bool CH_TxRamBackendInit(
    CH_TxRamBackend *ram,
    void *working,
    void *durable,
    uint32_t sector_size,
    uint32_t sector_count
);


CH_TxSectorBackend CH_TxRamBackendMake(
    CH_TxRamBackend *ram
);


void CH_TxRamBackendCrash(
    CH_TxRamBackend *ram
);


void CH_TxRamBackendClearFailures(
    CH_TxRamBackend *ram
);


#endif
