#include "card_smoke.h"

#include <carryhandle/carryhandle.h>

#include "ch_card_presentation_data.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>


/*
 * This offset belongs only to the hello smoke-test file layout.
 *
 * CarryHandle's generic presentation runtime deliberately does not
 * prescribe where an application's CARD presentation data must live.
 *
 * Leaving 64 bytes free also exercises the runtime's promise to
 * preserve caller-owned sector-zero data outside the presentation.
 */
#define HELLO_CARD_PRESENTATION_OFFSET 64u


static unsigned char cardWorkArea[CARD_WORKAREA]
    __attribute__((aligned(32)));


static void cardRemoved(
    s32 channel,
    s32 result
)
{
    (void)result;

    CARD_Unmount(channel);
}


bool HelloCardSmokeTest(void)
{
    const CH_ApplicationInfo *app;

    card_file file;
    card_stat status;

    void *sectorBuffer = NULL;

    s32 result;
    s32 memorySize = 0;
    s32 sectorSize = 0;

    s32 closeResult = CARD_ERROR_READY;
    s32 unmountResult;

    bool mounted = false;
    bool fileOpen = false;
    bool created = false;
    bool presentationOk = false;

    app = CH_ApplicationGetInfo();

    /*
     * Application identity comes from carryhandle.cfg through the
     * generated CH_ApplicationInfo descriptor.
     */
    result =
        CARD_Init(
            app->game_code,
            app->company_code
        );

    if (result < CARD_ERROR_READY)
    {
        printf(
            "CARD init    : FAIL (%ld)\n",
            (long)result
        );

        return false;
    }

    /*
     * Follow the installed libogc2 example: ProbeEx may briefly report
     * BUSY while the card is being detected.
     */
    do
    {
        result =
            CARD_ProbeEx(
                CARD_SLOTA,
                &memorySize,
                &sectorSize
            );
    }
    while (result == CARD_ERROR_BUSY);

    if (result < CARD_ERROR_READY)
    {
        printf(
            "CARD A probe : FAIL (%ld)\n",
            (long)result
        );

        return false;
    }

    printf(
        "CARD A        : %ld Mbit / %ld byte sector\n",
        (long)memorySize,
        (long)sectorSize
    );

    if (
        sectorSize <= 0
        || (u32)sectorSize
            < HELLO_CARD_PRESENTATION_OFFSET
                + CH_CARD_PRESENTATION_DATA_SIZE
    )
    {
        printf(
            "CARD sector  : too small\n"
        );

        return false;
    }

    result =
        CARD_Mount(
            CARD_SLOTA,
            cardWorkArea,
            cardRemoved
        );

    /*
     * CARD_ERROR_UNLOCKED is positive, so the installed example treats
     * any non-negative mount result as success.
     */
    if (result < CARD_ERROR_READY)
    {
        printf(
            "CARD A mount : FAIL (%ld)\n",
            (long)result
        );

        return false;
    }

    mounted = true;

    sectorBuffer =
        memalign(
            32,
            (size_t)sectorSize
        );

    if (!sectorBuffer)
    {
        printf(
            "CARD buffer  : allocation failed\n"
        );

        goto cleanup;
    }

    memset(
        &file,
        0,
        sizeof(file)
    );

    /*
     * The physical CARD filename also comes from carryhandle.cfg,
     * via ch_card_build.py's generated header.
     */
    result =
        CARD_Open(
            CARD_SLOTA,
            CH_CARD_PRESENTATION_FILENAME,
            &file
        );

    if (result == CARD_ERROR_NOFILE)
    {
        /*
         * CARD_Create requires a whole number of physical sectors and
         * returns the opened card_file directly.
         */
        result =
            CARD_Create(
                CARD_SLOTA,
                CH_CARD_PRESENTATION_FILENAME,
                (u32)sectorSize,
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

    /*
     * Refuse to reinterpret an existing same-named file that is too
     * short. The smoke test never deletes or silently recreates it.
     */
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

    if (
        status.len
        < (u32)sectorSize
    )
    {
        printf(
            "%-13s: existing file too small\n",
            CH_CARD_PRESENTATION_FILENAME
        );

        goto cleanup;
    }

    printf(
        "%-13s: %s\n",
        CH_CARD_PRESENTATION_FILENAME,
        created ? "created" : "opened"
    );

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
        "Presentation : %s\n",
        presentationOk
            ? "PASS"
            : "FAIL"
    );


cleanup:

    if (fileOpen)
    {
        closeResult =
            CARD_Close(
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
            CARD_Unmount(
                CARD_SLOTA
            );

        mounted = false;
    }
    else
    {
        unmountResult =
            CARD_ERROR_READY;
    }

    if (
        closeResult != CARD_ERROR_READY
        || unmountResult < CARD_ERROR_READY
    )
    {
        printf(
            "CARD cleanup : FAIL (%ld/%ld)\n",
            (long)closeResult,
            (long)unmountResult
        );

        return false;
    }

    printf(
        "CARD cleanup  : PASS\n"
    );

    return presentationOk;
}
