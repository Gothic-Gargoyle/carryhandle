#include <carryhandle/ch_tx_memcard_backend.h>
#include <carryhandle/ch_tx_format.h>

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
    s32 result;

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

    result =
        CH_MemCardWrite(
            context->file,
            sector_buffer,
            context->sector_size,
            offset
        );

    if (result != CARD_ERROR_READY)
    {
        return false;
    }

    /*
     * Only publishing one of CHTX's A/B superblocks can make a new
     * transaction authoritative. Preceding data/record writes therefore
     * do not change user-visible Memory Card modification metadata.
     */
    if (
        sector_index == CH_TX_SUPERBLOCK_A_SECTOR ||
        sector_index == CH_TX_SUPERBLOCK_B_SECTOR
    )
    {
        context->timestamp_pending =
            true;
    }

    return true;
}


static bool memCardSync(
    void *opaque)
{
    CH_TxMemCardBackendContext *context =
        (CH_TxMemCardBackendContext *)opaque;

    if (
        !context ||
        !context->session ||
        !context->file ||
        !context->session->mounted
    )
    {
        return false;
    }

    /*
     * CARD_Write() is synchronous, so there is no additional payload flush.
     *
     * If this barrier follows a successfully written CHTX superblock, the new
     * transaction is now authoritative. Refresh the native Memory Card
     * modification timestamp at that exact framework-wide commit boundary.
     *
     * Timestamp metadata is best-effort. A failure here must not turn an
     * already durable CHTX commit into COMMIT_UNCERTAIN.
     */
    if (context->timestamp_pending)
    {
        context->timestamp_pending =
            false;

        (void)CH_MemCardTouch(
            context->file);
    }

    return true;
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

    context->timestamp_pending =
        false;

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
