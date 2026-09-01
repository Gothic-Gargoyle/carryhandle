#include <carryhandle/ch_tx_memcard_backend.h>

#include <limits.h>
#include <string.h>


static bool memCardSectorOffset(
    const CH_TxMemCardBackendContext *context,
    uint32_t sector_index,
    uint32_t *offset)
{
    if (
        !context ||
        !context->session ||
        !context->file ||
        !offset ||
        !context->session->mounted ||
        sector_index >= context->sector_count
    )
    {
        return false;
    }

    /*
     * sector_count is derived from CARD_GetStatus(), so any valid sector
     * index necessarily maps inside the opened file.
     */
    *offset =
        sector_index *
        context->sector_size;

    return true;
}


static bool memCardReadSector(
    void *opaque,
    uint32_t sector_index,
    void *sector_buffer)
{
    CH_TxMemCardBackendContext *context =
        (CH_TxMemCardBackendContext *)opaque;

    uint32_t offset;

    if (
        !sector_buffer ||
        !memCardSectorOffset(
            context,
            sector_index,
            &offset
        )
    )
    {
        return false;
    }

    return
        CH_MemCardRead(
            context->file,
            sector_buffer,
            context->sector_size,
            offset
        ) == CARD_ERROR_READY;
}


static bool memCardWriteSector(
    void *opaque,
    uint32_t sector_index,
    const void *sector_buffer)
{
    CH_TxMemCardBackendContext *context =
        (CH_TxMemCardBackendContext *)opaque;

    uint32_t offset;

    if (
        !sector_buffer ||
        !memCardSectorOffset(
            context,
            sector_index,
            &offset
        )
    )
    {
        return false;
    }

    return
        CH_MemCardWrite(
            context->file,
            sector_buffer,
            context->sector_size,
            offset
        ) == CARD_ERROR_READY;
}


static bool memCardSync(
    void *opaque)
{
    CH_TxMemCardBackendContext *context =
        (CH_TxMemCardBackendContext *)opaque;

    /*
     * CARD_Write() is synchronous. There is no additional CARD flush
     * operation to perform here; successful write_sector() already
     * completed the physical write.
     */
    return
        context &&
        context->session &&
        context->file &&
        context->session->mounted;
}


bool CH_TxMemCardBackendInit(
    CH_TxMemCardBackendContext *context,
    const CH_MemCardSession *session,
    card_file *file,
    CH_TxSectorBackend *backend)
{
    card_stat status;

    uint32_t sectorSize;
    uint32_t fileSize;
    uint32_t sectorCount;

    if (
        !context ||
        !session ||
        !file ||
        !backend ||
        !session->mounted ||
        session->sector_size <= 0 ||
        file->chn != session->slot
    )
    {
        return false;
    }

    memset(
        &status,
        0,
        sizeof(status)
    );

    if (CARD_GetStatus(
            file->chn,
            file->filenum,
            &status) != CARD_ERROR_READY)
    {
        return false;
    }

    sectorSize =
        (uint32_t)session->sector_size;

    fileSize =
        status.len;

    if (
        fileSize % sectorSize != 0u
    )
    {
        return false;
    }

    sectorCount =
        fileSize / sectorSize;

    if (sectorCount == 0u)
    {
        return false;
    }

    context->session =
        session;

    context->file =
        file;

    context->sector_size =
        sectorSize;

    context->sector_count =
        sectorCount;

    backend->context =
        context;

    backend->sector_size =
        sectorSize;

    backend->sector_count =
        sectorCount;

    backend->buffer_alignment =
        32u;

    backend->read_sector =
        memCardReadSector;

    backend->write_sector =
        memCardWriteSector;

    backend->sync =
        memCardSync;

    return
        CH_TxSectorBackendValid(
            backend
        );
}
