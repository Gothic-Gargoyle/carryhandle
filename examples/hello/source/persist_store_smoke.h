#ifndef HELLO_PERSIST_STORE_SMOKE_H
#define HELLO_PERSIST_STORE_SMOKE_H

#include <stdbool.h>
#include <stdint.h>

#include <carryhandle/ch_memcard.h>

bool HelloPersistStoreSmokeTest(
    const CH_MemCardSession *session,
    void *sector_buffer,
    uint32_t sector_size
);

#endif
