#include "card_smoke.h"

#include <carryhandle/carryhandle.h>

#include "ch_card_presentation_data.h"

#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


/*
 * Sector-zero application layout:
 *
 *   0..31   CarryHandle CHTX container header
 *   32..63  reserved/caller-owned
 *   64..    GameCube CARD presentation
 *
 * Transaction superblocks live in sectors 1/2 and records begin at
 * CH_TX_DATA_START_SECTOR.
 */
#define HELLO_CARD_PRESENTATION_OFFSET 64u


static const uint8_t helloPersistKey[] =
{
    'h', 'e', 'l', 'l', 'o', '-', 's', 'm', 'o', 'k', 'e'
};


static const uint8_t helloPersistPayload[] =
{
    'C', 'a', 'r', 'r', 'y', 'H', 'a', 'n', 'd', 'l', 'e'
};


static unsigned char cardWorkArea[CARD_WORKAREA]
    __attribute__((aligned(32)));


bool HelloCardSmokeTest(void)
{
    const CH_ApplicationInfo *app;

    CH_MemCardSession session;

    CH_TxMemCardBackendContext txContext;
    CH_TxSectorBackend txBackend;

    card_file file;
    card_stat status;

    void *sectorBuffer = NULL;

    uint8_t output[sizeof(helloPersistPayload)];

    size_t objectSize = 0u;

    u32 fileSize = 0u;
    u32 existingSectors = 0u;

    s32 result;
    s32 memorySize = 0;
    s32 sectorSize = 0;

    s32 closeResult = CARD_ERROR_READY;
    s32 unmountResult = CARD_ERROR_READY;

    bool mounted = false;
    bool fileOpen = false;
    bool created = false;

    bool storeOk = false;
    bool presentationOk = false;
    bool persistOk = false;


    app =
        CH_ApplicationGetInfo();


    memset(
        &session,
        0,
        sizeof(session)
    );


    if (!CH_MemCardMount(
            &session,
            CARD_SLOTA,
            app->game_code,
            app->company_code,
            cardWorkArea))
    {
        printf("CARD A mount  : FAIL\n");
        return false;
    }

    mounted = true;

    memorySize =
        session.memory_size;

    sectorSize =
        session.sector_size;


    printf(
        "CARD A        : %ld Mbit / %ld byte sector\n",
        (long)memorySize,
        (long)sectorSize
    );


    if (
        sectorSize <= 0 ||
        CH_CARD_FILE_SECTORS <=
            CH_TX_SUPERBLOCK_B_SECTOR ||
        (u32)sectorSize >
            UINT32_MAX / CH_CARD_FILE_SECTORS ||
        (u32)sectorSize <
            HELLO_CARD_PRESENTATION_OFFSET +
            CH_CARD_PRESENTATION_DATA_SIZE
    )
    {
        printf("CARD geometry : manifest invalid\n");
        goto cleanup;
    }


    fileSize =
        (u32)sectorSize *
        CH_CARD_FILE_SECTORS;


    sectorBuffer =
        memalign(
            32,
            (size_t)sectorSize
        );

    if (!sectorBuffer)
    {
        printf("CARD buffer   : allocation FAIL\n");
        goto cleanup;
    }


    memset(
        &file,
        0,
        sizeof(file)
    );


    result =
        CH_MemCardOpen(
            &session,
            CH_CARD_PRESENTATION_FILENAME,
            &file
        );


    if (result == CARD_ERROR_NOFILE)
    {
        result =
            CH_MemCardCreate(
                &session,
                CH_CARD_PRESENTATION_FILENAME,
                fileSize,
                &file
            );

        if (result != CARD_ERROR_READY)
        {
            printf(
                "%-13s: create FAIL (%ld)\n",
                CH_CARD_PRESENTATION_FILENAME,
                (long)result
            );

            goto cleanup;
        }

        created = true;
        fileOpen = true;
    }
    else if (result == CARD_ERROR_READY)
    {
        fileOpen = true;
    }
    else
    {
        printf(
            "%-13s: open FAIL (%ld)\n",
            CH_CARD_PRESENTATION_FILENAME,
            (long)result
        );

        goto cleanup;
    }


    memset(
        &status,
        0,
        sizeof(status)
    );


    result =
        CARD_GetStatus(
            file.chn,
            file.filenum,
            &status
        );

    if (result != CARD_ERROR_READY)
    {
        printf(
            "%-13s: status FAIL (%ld)\n",
            CH_CARD_PRESENTATION_FILENAME,
            (long)result
        );

        goto cleanup;
    }


    existingSectors =
        status.len /
        (u32)sectorSize;


    /*
     * File geometry is part of the manifest-backed persistence contract.
     *
     * Never silently reinterpret, resize or recreate a same-named save
     * whose geometry belongs to an older application format.
     */
    if (status.len != fileSize)
    {
        printf(
            "%-13s: geometry FAIL\n",
            CH_CARD_PRESENTATION_FILENAME
        );

        printf(
            "Existing      : %lu sectors / %lu bytes\n",
            (unsigned long)existingSectors,
            (unsigned long)status.len
        );

        printf(
            "Manifest      : %lu sectors / %lu bytes\n",
            (unsigned long)CH_CARD_FILE_SECTORS,
            (unsigned long)fileSize
        );

        goto cleanup;
    }


    printf(
        "%-13s: %s\n",
        CH_CARD_PRESENTATION_FILENAME,
        created ? "created" : "opened"
    );

    printf(
        "CARD geometry : PASS (%lu sectors)\n",
        (unsigned long)CH_CARD_FILE_SECTORS
    );


    memset(
        &txContext,
        0,
        sizeof(txContext)
    );

    memset(
        &txBackend,
        0,
        sizeof(txBackend)
    );


    if (!CH_TxMemCardBackendInit(
            &txContext,
            &session,
            &file,
            &txBackend))
    {
        printf("TX backend    : FAIL\n");
        goto cleanup;
    }


    printf(
        "TX backend    : PASS (%lu sectors)\n",
        (unsigned long)txBackend.sector_count
    );


    /*
     * A newly created file becomes a transaction container here.
     *
     * An existing valid file follows CH_TxInitialize()'s idempotent
     * validation path and is not rewritten.
     */
    if (CH_TxInitialize(
            &txBackend,
            sectorBuffer,
            (size_t)sectorSize) !=
        CH_TX_RESULT_OK)
    {
        printf("Store init    : FAIL\n");
        goto cleanup;
    }

    storeOk = true;

    printf("Store init    : PASS\n");


    /*
     * Presentation shares sector zero with the CHTX header. The
     * presentation runtime performs read/modify/write and therefore
     * preserves the transaction header at bytes 0..31.
     */
    presentationOk =
        CH_CardPresentationApply(
            &file,
            sectorSize,
            sectorBuffer,
            HELLO_CARD_PRESENTATION_OFFSET,
            ch_card_presentation_data,
            CH_CARD_PRESENTATION_DATA_SIZE
        );


    printf(
        "Presentation  : %s\n",
        presentationOk
            ? "PASS"
            : "FAIL"
    );

    if (!presentationOk)
    {
        goto cleanup;
    }


    /*
     * Seed one persistent object only when this physical application
     * save file was freshly created.
     */
    if (created)
    {
        if (CH_PersistPut(
                &txBackend,
                sectorBuffer,
                (size_t)sectorSize,
                NULL,
                0u,
                helloPersistKey,
                sizeof(helloPersistKey),
                helloPersistPayload,
                sizeof(helloPersistPayload)) !=
            CH_PERSIST_RESULT_OK)
        {
            printf("Store PUT     : FAIL\n");
            goto cleanup;
        }

        printf("Store PUT     : PASS\n");
    }
    else
    {
        printf("Store PUT     : existing\n");
    }


    /*
     * Close/reopen before the GET so persistence is recovered through a
     * fresh card_file and transaction-backend instance.
     */
    closeResult =
        CH_MemCardClose(
            &file
        );

    if (closeResult != CARD_ERROR_READY)
    {
        printf(
            "Store close   : FAIL (%ld)\n",
            (long)closeResult
        );

        goto cleanup;
    }

    fileOpen = false;


    memset(
        &file,
        0,
        sizeof(file)
    );


    result =
        CH_MemCardOpen(
            &session,
            CH_CARD_PRESENTATION_FILENAME,
            &file
        );

    if (result != CARD_ERROR_READY)
    {
        printf(
            "Store reopen  : FAIL (%ld)\n",
            (long)result
        );

        goto cleanup;
    }

    fileOpen = true;


    memset(
        &txContext,
        0,
        sizeof(txContext)
    );

    memset(
        &txBackend,
        0,
        sizeof(txBackend)
    );


    if (!CH_TxMemCardBackendInit(
            &txContext,
            &session,
            &file,
            &txBackend))
    {
        printf("Store reopen  : backend FAIL\n");
        goto cleanup;
    }


    if (CH_TxInitialize(
            &txBackend,
            sectorBuffer,
            (size_t)sectorSize) !=
        CH_TX_RESULT_OK)
    {
        printf("Store reopen  : init FAIL\n");
        goto cleanup;
    }


    printf("Store reopen  : PASS\n");


    memset(
        output,
        0,
        sizeof(output)
    );

    objectSize = 0u;


    if (CH_PersistGet(
            &txBackend,
            sectorBuffer,
            (size_t)sectorSize,
            NULL,
            0u,
            helloPersistKey,
            sizeof(helloPersistKey),
            output,
            sizeof(output),
            &objectSize) !=
        CH_PERSIST_RESULT_OK)
    {
        printf("Store GET     : FAIL\n");
        goto cleanup;
    }


    if (
        objectSize !=
            sizeof(helloPersistPayload) ||
        memcmp(
            output,
            helloPersistPayload,
            sizeof(helloPersistPayload)) != 0
    )
    {
        printf("Store verify  : FAIL\n");
        goto cleanup;
    }


    persistOk = true;

    printf("Store GET     : PASS\n");
    printf("Store verify  : PASS\n");


cleanup:

    if (fileOpen)
    {
        closeResult =
            CH_MemCardClose(
                &file
            );

        fileOpen = false;
    }


    if (sectorBuffer)
    {
        free(
            sectorBuffer
        );

        sectorBuffer = NULL;
    }


    if (mounted)
    {
        unmountResult =
            CH_MemCardUnmount(
                &session
            )
                ? CARD_ERROR_READY
                : CARD_ERROR_FATAL_ERROR;

        mounted = false;
    }


    if (
        closeResult != CARD_ERROR_READY ||
        unmountResult < CARD_ERROR_READY
    )
    {
        printf(
            "CARD cleanup  : FAIL (%ld/%ld)\n",
            (long)closeResult,
            (long)unmountResult
        );

        return false;
    }


    printf("CARD cleanup  : PASS\n");


    return
        storeOk &&
        presentationOk &&
        persistOk;
}
