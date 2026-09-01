#include "tx_backend_smoke.h"

#include <carryhandle/carryhandle.h>

#include <stdio.h>
#include <string.h>


#define HELLO_TX_SMOKE_FILENAME "CHTXSMOKE"


bool HelloTxBackendSmokeTest(
    const CH_MemCardSession *session,
    void *sector_buffer,
    uint32_t sector_size)
{
    CH_TxMemCardBackendContext context;
    CH_TxSectorBackend backend;
    card_file file;

    s32 result;
    s32 closeResult;

    bool open = false;
    bool created = false;

    uint32_t i;


    memset(&file, 0, sizeof(file));
    memset(&context, 0, sizeof(context));
    memset(&backend, 0, sizeof(backend));


    result =
        CH_MemCardOpen(
            session,
            HELLO_TX_SMOKE_FILENAME,
            &file
        );

    if (result == CARD_ERROR_NOFILE)
    {
        result =
            CH_MemCardCreate(
                session,
                HELLO_TX_SMOKE_FILENAME,
                sector_size,
                &file
            );

        created = true;
    }

    if (result != CARD_ERROR_READY)
    {
        printf(
            "TX file       : FAIL (%ld)\n",
            (long)result
        );

        return false;
    }

    open = true;

    printf(
        "TX file       : %s\n",
        created ? "created" : "opened"
    );


    if (!CH_TxMemCardBackendInit(
            &context,
            session,
            &file,
            &backend))
    {
        printf("TX backend    : init FAIL\n");
        goto fail;
    }


    if (
        backend.sector_size != sector_size ||
        backend.sector_count < 1u ||
        backend.buffer_alignment != 32u ||
        !CH_TxSectorBufferValid(
            &backend,
            sector_buffer)
    )
    {
        printf("TX backend    : geometry FAIL\n");
        goto fail;
    }

    printf(
        "TX backend    : PASS (%lu sectors)\n",
        (unsigned long)backend.sector_count
    );


    for (i = 0u; i < sector_size; ++i)
    {
        ((unsigned char *)sector_buffer)[i] =
            (unsigned char)(
                (i * 37u + 0x5au) & 0xffu
            );
    }


    if (!backend.write_sector(
            backend.context,
            0u,
            sector_buffer))
    {
        printf("TX write      : FAIL\n");
        goto fail;
    }


    if (!backend.sync(
            backend.context))
    {
        printf("TX sync       : FAIL\n");
        goto fail;
    }


    memset(
        sector_buffer,
        0,
        sector_size
    );


    if (!backend.read_sector(
            backend.context,
            0u,
            sector_buffer))
    {
        printf("TX read       : FAIL\n");
        goto fail;
    }


    for (i = 0u; i < sector_size; ++i)
    {
        unsigned char expected =
            (unsigned char)(
                (i * 37u + 0x5au) & 0xffu
            );

        if (
            ((unsigned char *)sector_buffer)[i]
            != expected
        )
        {
            printf(
                "TX verify     : FAIL @ %lu\n",
                (unsigned long)i
            );

            goto fail;
        }
    }


    printf("TX sector I/O : PASS\n");

    closeResult =
        CH_MemCardClose(&file);

    if (closeResult != CARD_ERROR_READY)
    {
        printf(
            "TX close      : FAIL (%ld)\n",
            (long)closeResult
        );

        return false;
    }

    return true;


fail:

    if (open)
    {
        CH_MemCardClose(&file);
    }

    return false;
}
