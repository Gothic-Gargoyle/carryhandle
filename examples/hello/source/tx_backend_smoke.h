#ifndef HELLO_TX_BACKEND_SMOKE_H
#define HELLO_TX_BACKEND_SMOKE_H

#include <stdbool.h>
#include <stdint.h>

#include <carryhandle/ch_memcard.h>

bool HelloTxBackendSmokeTest(
    const CH_MemCardSession *session,
    void *sector_buffer,
    uint32_t sector_size
);

#endif
