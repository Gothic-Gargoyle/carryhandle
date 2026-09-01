#include <carryhandle/ch_application_save.h>

#include <carryhandle/ch_card_presentation.h>

#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


static CH_ApplicationSaveResult releaseResources(
    CH_ApplicationSaveSession *session)
{
    bool cleanupOk = true;

    s32 closeResult =
        CARD_ERROR_READY;


    if (!session)
    {
        return
            CH_APPLICATION_SAVE_RESULT_INVALID_ARGUMENT;
    }


    session->open =
        false;


    if (session->file_open)
    {
        closeResult =
            CH_MemCardClose(
                &session->file
            );

        session->file_open =
            false;

        if (closeResult != CARD_ERROR_READY)
        {
            session->card_result =
                closeResult;

            cleanupOk =
                false;
        }
    }


    if (session->mounted)
    {
        if (!CH_MemCardUnmount(
                &session->memcard))
        {
            session->card_result =
                CARD_ERROR_FATAL_ERROR;

            cleanupOk =
                false;
        }

        session->mounted =
            false;
    }


    if (session->sector_buffer)
    {
        free(
            session->sector_buffer
        );

        session->sector_buffer =
            NULL;
    }


    if (session->work_area)
    {
        free(
            session->work_area
        );

        session->work_area =
            NULL;
    }


    session->sector_buffer_size =
        0u;

    session->file_size =
        0u;


    memset(
        &session->file,
        0,
        sizeof(session->file)
    );

    memset(
        &session->tx_context,
        0,
        sizeof(session->tx_context)
    );

    memset(
        &session->tx_backend,
        0,
        sizeof(session->tx_backend)
    );


    return
        cleanupOk
            ? CH_APPLICATION_SAVE_RESULT_OK
            : CH_APPLICATION_SAVE_RESULT_CARD;
}


static CH_ApplicationSaveResult failOpen(
    CH_ApplicationSaveSession *session,
    CH_ApplicationSaveResult result)
{
    CH_ApplicationSaveResult cleanupResult;


    cleanupResult =
        releaseResources(
            session
        );


    if (cleanupResult !=
        CH_APPLICATION_SAVE_RESULT_OK)
    {
        return
            cleanupResult;
    }


    return result;
}


static bool descriptorValid(
    const CH_ApplicationInfo *application,
    const CH_ApplicationSaveDescriptor *descriptor)
{
    if (
        !application ||
        !descriptor ||
        !application->game_code ||
        !application->company_code ||
        !descriptor->filename ||
        descriptor->filename[0] == '\0' ||
        descriptor->sector_count <
            CH_TX_DATA_START_SECTOR
    )
    {
        return false;
    }


    if (
        descriptor->presentation_size == 0u
    )
    {
        return
            descriptor->presentation_data ==
                NULL;
    }


    if (
        !descriptor->presentation_data ||
        descriptor->presentation_offset >=
            CARD_READSIZE ||
        descriptor->presentation_size >
            UINT32_MAX -
                descriptor->presentation_offset
    )
    {
        return false;
    }


    return true;
}


CH_ApplicationSaveResult CH_ApplicationSaveOpen(
    CH_ApplicationSaveSession *session,
    const CH_ApplicationInfo *application,
    const CH_ApplicationSaveDescriptor *descriptor,
    s32 slot)
{
    card_stat status;

    CH_TxResult txResult;

    s32 result;

    uint32_t sectorSize;
    uint32_t fileSize;


    if (
        !session ||
        !descriptorValid(
            application,
            descriptor)
    )
    {
        return
            CH_APPLICATION_SAVE_RESULT_INVALID_ARGUMENT;
    }


    /*
     * Never destroy ownership state from an already-live or partially-owned
     * session. Callers initialize a session to zero before first use and
     * Close() returns it to a reusable closed state.
     */
    if (
        session->open ||
        session->mounted ||
        session->file_open ||
        session->work_area ||
        session->sector_buffer
    )
    {
        return
            CH_APPLICATION_SAVE_RESULT_INVALID_ARGUMENT;
    }


    memset(
        session,
        0,
        sizeof(*session)
    );


    session->application =
        application;

    session->descriptor =
        descriptor;

    session->card_result =
        CARD_ERROR_READY;

    session->tx_result =
        CH_TX_RESULT_OK;


    session->work_area =
        memalign(
            32,
            CARD_WORKAREA
        );

    if (!session->work_area)
    {
        return
            CH_APPLICATION_SAVE_RESULT_NO_MEMORY;
    }


    if (!CH_MemCardMount(
            &session->memcard,
            slot,
            application->game_code,
            application->company_code,
            session->work_area))
    {
        session->card_result =
            CARD_ERROR_FATAL_ERROR;

        return
            failOpen(
                session,
                CH_APPLICATION_SAVE_RESULT_CARD
            );
    }


    session->mounted =
        true;


    if (session->memcard.sector_size <= 0)
    {
        return
            failOpen(
                session,
                CH_APPLICATION_SAVE_RESULT_CARD
            );
    }


    sectorSize =
        (uint32_t)
            session->memcard.sector_size;


    if (
        descriptor->sector_count >
            UINT32_MAX / sectorSize
    )
    {
        return
            failOpen(
                session,
                CH_APPLICATION_SAVE_RESULT_INVALID_ARGUMENT
            );
    }


    fileSize =
        descriptor->sector_count *
        sectorSize;


    if (
        descriptor->presentation_size != 0u &&
        (
            descriptor->presentation_offset +
                descriptor->presentation_size >
            sectorSize
        )
    )
    {
        return
            failOpen(
                session,
                CH_APPLICATION_SAVE_RESULT_INVALID_ARGUMENT
            );
    }


    session->sector_buffer =
        memalign(
            32,
            (size_t)sectorSize
        );

    if (!session->sector_buffer)
    {
        return
            failOpen(
                session,
                CH_APPLICATION_SAVE_RESULT_NO_MEMORY
            );
    }


    session->sector_buffer_size =
        (size_t)sectorSize;

    session->file_size =
        fileSize;


    result =
        CH_MemCardOpen(
            &session->memcard,
            descriptor->filename,
            &session->file
        );

    session->card_result =
        result;


    if (result == CARD_ERROR_NOFILE)
    {
        result =
            CH_MemCardCreate(
                &session->memcard,
                descriptor->filename,
                fileSize,
                &session->file
            );

        session->card_result =
            result;

        if (result != CARD_ERROR_READY)
        {
            return
                failOpen(
                    session,
                    CH_APPLICATION_SAVE_RESULT_CARD
                );
        }


        session->file_open =
            true;

        session->created =
            true;
    }
    else if (result == CARD_ERROR_READY)
    {
        session->file_open =
            true;
    }
    else
    {
        return
            failOpen(
                session,
                CH_APPLICATION_SAVE_RESULT_CARD
            );
    }


    /*
     * CARD_GetStatus() is authoritative for physical file geometry.
     *
     * Never resize, delete or recreate a same-named incompatible save.
     */
    memset(
        &status,
        0,
        sizeof(status)
    );


    result =
        CARD_GetStatus(
            session->file.chn,
            session->file.filenum,
            &status
        );

    session->card_result =
        result;


    if (result != CARD_ERROR_READY)
    {
        return
            failOpen(
                session,
                CH_APPLICATION_SAVE_RESULT_CARD
            );
    }


    if (status.len != fileSize)
    {
        return
            failOpen(
                session,
                CH_APPLICATION_SAVE_RESULT_GEOMETRY_MISMATCH
            );
    }


    /*
     * Never depend on CARD_Create() returning zero-filled physical blocks.
     *
     * CH_TxInitialize() is deliberately idempotent and will accept an
     * already-valid CHTX header. A freshly allocated CARD file could
     * therefore be mistaken for an existing transaction container if old
     * physical block contents happened to contain valid metadata.
     *
     * For a file created by this Open(), explicitly invalidate sector zero
     * before transaction initialization. CH_TxInitialize() will then
     * deterministically take its fresh-container path.
     *
     * The file has not yet been exposed to the caller, so if this initial
     * write fails we attempt to remove the just-created file rather than
     * leave an ambiguous same-named save behind.
     */
    if (session->created)
    {
        s32 zeroResult;
        s32 closeResult;
        s32 deleteResult;


        memset(
            session->sector_buffer,
            0,
            session->sector_buffer_size
        );


        zeroResult =
            CH_MemCardWrite(
                &session->file,
                session->sector_buffer,
                sectorSize,
                0u
            );


        if (zeroResult != CARD_ERROR_READY)
        {
            session->card_result =
                zeroResult;


            closeResult =
                CH_MemCardClose(
                    &session->file
                );


            if (closeResult == CARD_ERROR_READY)
            {
                session->file_open =
                    false;


                deleteResult =
                    CARD_Delete(
                        session->memcard.slot,
                        descriptor->filename
                    );


                if (deleteResult == CARD_ERROR_READY)
                {
                    session->created =
                        false;

                    /*
                     * Preserve the original zeroing failure as the
                     * diagnostic returned to the caller.
                     */
                    session->card_result =
                        zeroResult;
                }
                else
                {
                    session->card_result =
                        deleteResult;
                }
            }
            else
            {
                session->card_result =
                    closeResult;
            }


            return
                failOpen(
                    session,
                    CH_APPLICATION_SAVE_RESULT_CARD
                );
        }


        session->card_result =
            CARD_ERROR_READY;
    }


    if (!CH_TxMemCardBackendInit(
            &session->tx_context,
            &session->memcard,
            &session->file,
            &session->tx_backend))
    {
        return
            failOpen(
                session,
                CH_APPLICATION_SAVE_RESULT_TRANSACTION
            );
    }


    txResult =
        CH_TxInitialize(
            &session->tx_backend,
            session->sector_buffer,
            session->sector_buffer_size
        );

    session->tx_result =
        txResult;


    if (txResult != CH_TX_RESULT_OK)
    {
        return
            failOpen(
                session,
                CH_APPLICATION_SAVE_RESULT_TRANSACTION
            );
    }


    /*
     * Presentation is optional.
     *
     * When present it is applied only after the CHTX container exists.
     * CH_CardPresentationApply() preserves all sector-zero bytes outside
     * the presentation range, including the transaction header.
     */
    if (descriptor->presentation_size != 0u)
    {
        if (!CH_CardPresentationApply(
                &session->file,
                session->memcard.sector_size,
                session->sector_buffer,
                descriptor->presentation_offset,
                descriptor->presentation_data,
                descriptor->presentation_size))
        {
            return
                failOpen(
                    session,
                    CH_APPLICATION_SAVE_RESULT_PRESENTATION
                );
        }
    }


    session->card_result =
        CARD_ERROR_READY;

    session->tx_result =
        CH_TX_RESULT_OK;

    session->open =
        true;


    return
        CH_APPLICATION_SAVE_RESULT_OK;
}


CH_ApplicationSaveResult CH_ApplicationSaveClose(
    CH_ApplicationSaveSession *session)
{
    if (!session)
    {
        return
            CH_APPLICATION_SAVE_RESULT_INVALID_ARGUMENT;
    }


    return
        releaseResources(
            session
        );
}


const CH_TxSectorBackend *CH_ApplicationSaveBackend(
    const CH_ApplicationSaveSession *session)
{
    if (
        !session ||
        !session->open
    )
    {
        return NULL;
    }


    return
        &session->tx_backend;
}


void *CH_ApplicationSaveSectorBuffer(
    CH_ApplicationSaveSession *session)
{
    if (
        !session ||
        !session->open
    )
    {
        return NULL;
    }


    return
        session->sector_buffer;
}


size_t CH_ApplicationSaveSectorBufferSize(
    const CH_ApplicationSaveSession *session)
{
    if (
        !session ||
        !session->open
    )
    {
        return 0u;
    }


    return
        session->sector_buffer_size;
}


bool CH_ApplicationSaveWasCreated(
    const CH_ApplicationSaveSession *session)
{
    return
        session &&
        session->open &&
        session->created;
}

CH_PersistResult CH_ApplicationSaveGet(
    CH_ApplicationSaveSession *session,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    void *output,
    size_t output_capacity,
    size_t *output_size)
{
    if (
        !session ||
        !session->open
    )
    {
        return
            CH_PERSIST_RESULT_INVALID_ARGUMENT;
    }


    return
        CH_PersistGet(
            &session->tx_backend,
            session->sector_buffer,
            session->sector_buffer_size,
            scope,
            scope_size,
            key,
            key_size,
            output,
            output_capacity,
            output_size
        );
}


CH_PersistResult CH_ApplicationSavePut(
    CH_ApplicationSaveSession *session,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size,
    const void *payload,
    size_t payload_size)
{
    if (
        !session ||
        !session->open
    )
    {
        return
            CH_PERSIST_RESULT_INVALID_ARGUMENT;
    }


    return
        CH_PersistPut(
            &session->tx_backend,
            session->sector_buffer,
            session->sector_buffer_size,
            scope,
            scope_size,
            key,
            key_size,
            payload,
            payload_size
        );
}


CH_PersistResult CH_ApplicationSaveDelete(
    CH_ApplicationSaveSession *session,
    const void *scope,
    size_t scope_size,
    const void *key,
    size_t key_size)
{
    if (
        !session ||
        !session->open
    )
    {
        return
            CH_PERSIST_RESULT_INVALID_ARGUMENT;
    }


    return
        CH_PersistDelete(
            &session->tx_backend,
            session->sector_buffer,
            session->sector_buffer_size,
            scope,
            scope_size,
            key,
            key_size
        );
}
