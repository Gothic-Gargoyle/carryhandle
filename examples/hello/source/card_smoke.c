#include "card_smoke.h"

#include <carryhandle/carryhandle.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>


static const uint8_t helloPersistKey[] =
{
    'h', 'e', 'l', 'l', 'o', '-', 's', 'm', 'o', 'k', 'e'
};


static const uint8_t helloPersistPayload[] =
{
    'C', 'a', 'r', 'r', 'y', 'H', 'a', 'n', 'd', 'l', 'e'
};


static void printOpenFailure(
    const char *label,
    CH_ApplicationSaveResult result,
    const CH_ApplicationSaveSession *session)
{
    printf(
        "%-14s: FAIL (%d)\n",
        label,
        (int)result
    );

    printf(
        "  CARD result : %ld\n",
        (long)session->card_result
    );

    printf(
        "  TX result   : %d\n",
        (int)session->tx_result
    );
}


bool HelloCardSmokeTest(void)
{
    const CH_ApplicationInfo *application;
    const CH_ApplicationSaveDescriptor *descriptor;

    CH_ApplicationSaveSession save = {0};

    size_t objectSize = 0u;

    uint8_t output[
        sizeof(helloPersistPayload)
    ];

    CH_ApplicationSaveResult saveResult;
    CH_ApplicationSaveResult closeResult;

    bool created;
    bool persistOk = false;
    bool cleanupOk = true;


    application =
        CH_ApplicationGetInfo();

    descriptor =
        CH_ApplicationSaveGetDescriptor();


    if (
        !application ||
        !descriptor
    )
    {
        printf("App save      : descriptor FAIL\n");
        return false;
    }


    /*
     * Application-save Open owns:
     *
     *   CARD work area
     *   mount
     *   physical geometry
     *   open/create
     *   transaction backend
     *   CHTX init/recovery
     *   sector scratch
     *   presentation
     */
    saveResult =
        CH_ApplicationSaveOpen(
            &save,
            application,
            descriptor,
            CARD_SLOTA
        );


    if (
        saveResult !=
        CH_APPLICATION_SAVE_RESULT_OK
    )
    {
        printOpenFailure(
            "App save",
            saveResult,
            &save
        );

        return false;
    }


    created =
        CH_ApplicationSaveWasCreated(
            &save
        );


    printf(
        "%-13s: %s\n",
        descriptor->filename,
        created
            ? "created"
            : "opened"
    );

    printf(
        "CARD geometry : PASS (%lu sectors)\n",
        (unsigned long)
            descriptor->sector_count
    );

    printf(
        "App save open : PASS\n"
    );


    /*
     * Application policy remains outside the generic lifecycle:
     * seed this object only on physical first creation.
     */
    if (created)
    {
        if (CH_ApplicationSavePut(
                &save,
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
     * Deliberately close and reopen through the new high-level API.
     *
     * The same CH_ApplicationSaveSession is reused after Close(), proving
     * the reusable closed-state contract from STEP 9B2A.
     */
    closeResult =
        CH_ApplicationSaveClose(
            &save
        );


    if (
        closeResult !=
        CH_APPLICATION_SAVE_RESULT_OK
    )
    {
        printf(
            "Store close   : FAIL (%d)\n",
            (int)closeResult
        );

        goto cleanup;
    }


    printf("Store close   : PASS\n");


    saveResult =
        CH_ApplicationSaveOpen(
            &save,
            application,
            descriptor,
            CARD_SLOTA
        );


    if (
        saveResult !=
        CH_APPLICATION_SAVE_RESULT_OK
    )
    {
        printOpenFailure(
            "Store reopen",
            saveResult,
            &save
        );

        goto cleanup;
    }


    /*
     * Reopening an already-created application save must never report
     * physical creation.
     */
    if (CH_ApplicationSaveWasCreated(
            &save))
    {
        printf(
            "Store reopen  : recreated FAIL\n"
        );

        goto cleanup;
    }


    printf("Store reopen  : PASS\n");


    memset(
        output,
        0,
        sizeof(output)
    );

    objectSize =
        0u;


    if (CH_ApplicationSaveGet(
            &save,
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


    printf("Store GET     : PASS\n");


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


    printf("Store verify  : PASS\n");

    persistOk =
        true;


cleanup:

    /*
     * Close is safe for both a live session and one already cleaned up by
     * a failed Open().
     */
    closeResult =
        CH_ApplicationSaveClose(
            &save
        );


    if (
        closeResult !=
        CH_APPLICATION_SAVE_RESULT_OK
    )
    {
        cleanupOk =
            false;

        printf(
            "CARD cleanup  : FAIL (%d)\n",
            (int)closeResult
        );
    }
    else
    {
        printf("CARD cleanup  : PASS\n");
    }


    return
        persistOk &&
        cleanupOk;
}
