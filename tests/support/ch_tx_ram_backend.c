#include "ch_tx_ram_backend.h"

#include <stdint.h>
#include <string.h>


static bool sectorRange(
    const CH_TxRamBackend *ram,
    uint32_t sector,
    size_t *offset)
{
    if (
        !ram ||
        !offset ||
        sector >= ram->sector_count
    )
    {
        return false;
    }

    *offset =
        (size_t)sector *
        (size_t)ram->sector_size;

    return true;
}


static bool ramReadSector(
    void *context,
    uint32_t sector,
    void *buffer)
{
    CH_TxRamBackend *ram =
        (CH_TxRamBackend *)context;

    size_t offset;

    if (
        !ram ||
        !buffer ||
        !sectorRange(
            ram,
            sector,
            &offset)
    )
    {
        return false;
    }

    ++ram->read_calls;

    if (
        ram->fail_read_call != 0u &&
        ram->read_calls ==
            ram->fail_read_call
    )
    {
        return false;
    }

    memcpy(
        buffer,
        ram->working + offset,
        ram->sector_size
    );

    return true;
}


static bool ramWriteSector(
    void *context,
    uint32_t sector,
    const void *buffer)
{
    CH_TxRamBackend *ram =
        (CH_TxRamBackend *)context;

    size_t offset;

    if (
        !ram ||
        !buffer ||
        !sectorRange(
            ram,
            sector,
            &offset)
    )
    {
        return false;
    }

    ++ram->write_calls;

    if (
        ram->fail_write_call != 0u &&
        ram->write_calls ==
            ram->fail_write_call
    )
    {
        return false;
    }

    memcpy(
        ram->working + offset,
        buffer,
        ram->sector_size
    );

    return true;
}


static bool ramSync(
    void *context)
{
    CH_TxRamBackend *ram =
        (CH_TxRamBackend *)context;

    if (!ram)
    {
        return false;
    }

    ++ram->sync_calls;

    if (
        ram->fail_sync_call != 0u &&
        ram->sync_calls ==
            ram->fail_sync_call
    )
    {
        return false;
    }

    memcpy(
        ram->durable,
        ram->working,
        ram->storage_size
    );

    return true;
}


bool CH_TxRamBackendInit(
    CH_TxRamBackend *ram,
    void *working,
    void *durable,
    uint32_t sector_size,
    uint32_t sector_count)
{
    size_t storageSize;

    if (
        !ram ||
        !working ||
        !durable ||
        sector_size == 0u ||
        sector_count == 0u ||
        (size_t)sector_count >
            SIZE_MAX /
            (size_t)sector_size
    )
    {
        return false;
    }

    storageSize =
        (size_t)sector_size *
        (size_t)sector_count;

    memset(
        ram,
        0,
        sizeof(*ram)
    );

    ram->working =
        (uint8_t *)working;

    ram->durable =
        (uint8_t *)durable;

    ram->storage_size =
        storageSize;

    ram->sector_size =
        sector_size;

    ram->sector_count =
        sector_count;

    memset(
        ram->working,
        0,
        storageSize
    );

    memset(
        ram->durable,
        0,
        storageSize
    );

    return true;
}


CH_TxSectorBackend CH_TxRamBackendMake(
    CH_TxRamBackend *ram)
{
    CH_TxSectorBackend backend = {0};

    if (!ram)
    {
        return backend;
    }

    backend.context =
        ram;

    backend.sector_size =
        ram->sector_size;

    backend.sector_count =
        ram->sector_count;

    backend.buffer_alignment =
        1u;

    backend.read_sector =
        ramReadSector;

    backend.write_sector =
        ramWriteSector;

    backend.sync =
        ramSync;

    return backend;
}


void CH_TxRamBackendCrash(
    CH_TxRamBackend *ram)
{
    if (!ram)
    {
        return;
    }

    memcpy(
        ram->working,
        ram->durable,
        ram->storage_size
    );
}


void CH_TxRamBackendClearFailures(
    CH_TxRamBackend *ram)
{
    if (!ram)
    {
        return;
    }

    ram->fail_read_call = 0u;
    ram->fail_write_call = 0u;
    ram->fail_sync_call = 0u;
}
