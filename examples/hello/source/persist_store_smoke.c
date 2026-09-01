#include "persist_store_smoke.h"

#include <carryhandle/carryhandle.h>

#include <stdio.h>
#include <string.h>


#define HELLO_STORE_FILENAME "CH8BSTORE"
#define HELLO_STORE_SECTORS  8u


static const uint8_t helloScope[] =
    {'l','i','v','e'};

static const uint8_t helloKey[] =
    {'b','l','o','b'};

static const uint8_t helloPayload[] =
    {'C','a','r','r','y','H','a','n','d','l','e'};


bool HelloPersistStoreSmokeTest(
    const CH_MemCardSession *session,
    void *sector_buffer,
    uint32_t sector_size)
{
    CH_TxMemCardBackendContext context;
    CH_TxSectorBackend backend;

    CH_TxSuperblock superblock;

    card_file file;

    uint8_t output[sizeof(helloPayload)];

    size_t objectSize = 0u;

    uint32_t fileSize;

    s32 result;
    s32 closeResult;

    bool fileOpen = false;
    bool created = false;


    if (
        !session ||
        !sector_buffer ||
        sector_size == 0u ||
        sector_size >
            UINT32_MAX / HELLO_STORE_SECTORS
    )
    {
        return false;
    }

    fileSize =
        sector_size *
        HELLO_STORE_SECTORS;


    memset(&file, 0, sizeof(file));

    result =
        CH_MemCardOpen(
            session,
            HELLO_STORE_FILENAME,
            &file
        );

    if (result == CARD_ERROR_NOFILE)
    {
        result =
            CH_MemCardCreate(
                session,
                HELLO_STORE_FILENAME,
                fileSize,
                &file
            );

        created = true;
    }

    if (result != CARD_ERROR_READY)
    {
        printf(
            "Store file    : FAIL (%ld)\n",
            (long)result
        );

        return false;
    }

    fileOpen = true;

    printf(
        "Store file    : %s\n",
        created ? "created" : "opened"
    );


    memset(&context, 0, sizeof(context));
    memset(&backend, 0, sizeof(backend));

    if (!CH_TxMemCardBackendInit(
            &context,
            session,
            &file,
            &backend))
    {
        printf("Store backend : FAIL\n");
        goto fail;
    }

    if (backend.sector_count !=
        HELLO_STORE_SECTORS)
    {
        printf(
            "Store geometry: FAIL (%lu)\n",
            (unsigned long)backend.sector_count
        );

        goto fail;
    }

    printf(
        "Store backend : PASS (%lu sectors)\n",
        (unsigned long)backend.sector_count
    );


    if (created)
    {
        if (CH_TxReadAuthoritativeSuperblock(
                &backend,
                sector_buffer,
                sector_size,
                &superblock,
                NULL) != CH_TX_RESULT_CORRUPT)
        {
            printf("Store blank   : FAIL\n");
            goto fail;
        }

        printf("Store blank   : PASS\n");
    }


    if (CH_TxInitialize(
            &backend,
            sector_buffer,
            sector_size) !=
        CH_TX_RESULT_OK)
    {
        printf("Store init    : FAIL\n");
        goto fail;
    }

    printf("Store init    : PASS\n");


    /*
     * Populate a freshly created store exactly once.
     *
     * Existing stores are read-only for this smoke path so repeatedly
     * launching the example cannot consume transaction-log space.
     */
    if (created)
    {
        if (CH_PersistPut(
                &backend,
                sector_buffer,
                sector_size,
                helloScope,
                sizeof(helloScope),
                helloKey,
                sizeof(helloKey),
                helloPayload,
                sizeof(helloPayload)) !=
            CH_PERSIST_RESULT_OK)
        {
            printf("Store PUT     : FAIL\n");
            goto fail;
        }

        printf("Store PUT     : PASS\n");
    }
    else
    {
        printf("Store PUT     : existing\n");
    }


    closeResult =
        CH_MemCardClose(&file);

    if (closeResult != CARD_ERROR_READY)
    {
        printf(
            "Store close   : FAIL (%ld)\n",
            (long)closeResult
        );

        return false;
    }

    fileOpen = false;


    memset(&file, 0, sizeof(file));

    result =
        CH_MemCardOpen(
            session,
            HELLO_STORE_FILENAME,
            &file
        );

    if (result != CARD_ERROR_READY)
    {
        printf(
            "Store reopen  : FAIL (%ld)\n",
            (long)result
        );

        return false;
    }

    fileOpen = true;


    memset(&context, 0, sizeof(context));
    memset(&backend, 0, sizeof(backend));

    if (!CH_TxMemCardBackendInit(
            &context,
            session,
            &file,
            &backend))
    {
        printf("Store reopen  : backend FAIL\n");
        goto fail;
    }


    /*
     * Existing valid containers initialize idempotently.
     */
    if (CH_TxInitialize(
            &backend,
            sector_buffer,
            sector_size) !=
        CH_TX_RESULT_OK)
    {
        printf("Store reopen  : init FAIL\n");
        goto fail;
    }

    printf("Store reopen  : PASS\n");


    memset(
        output,
        0,
        sizeof(output)
    );

    objectSize = 0u;

    if (CH_PersistGet(
            &backend,
            sector_buffer,
            sector_size,
            helloScope,
            sizeof(helloScope),
            helloKey,
            sizeof(helloKey),
            output,
            sizeof(output),
            &objectSize) !=
        CH_PERSIST_RESULT_OK)
    {
        printf("Store GET     : FAIL\n");
        goto fail;
    }

    if (
        objectSize != sizeof(helloPayload) ||
        memcmp(
            output,
            helloPayload,
            sizeof(helloPayload)) != 0
    )
    {
        printf("Store verify  : FAIL\n");
        goto fail;
    }

    printf("Store GET     : PASS\n");
    printf("Store verify  : PASS\n");


    closeResult =
        CH_MemCardClose(&file);

    if (closeResult != CARD_ERROR_READY)
    {
        printf(
            "Store close   : FAIL (%ld)\n",
            (long)closeResult
        );

        return false;
    }

    return true;


fail:

    if (fileOpen)
    {
        CH_MemCardClose(&file);
    }

    return false;
}
