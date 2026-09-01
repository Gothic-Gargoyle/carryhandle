#include <carryhandle/ch_tx.h>


static bool superblockGeometryMatches(
    const CH_TxSectorBackend *backend,
    const CH_TxSuperblock *superblock)
{
    if (
        !backend ||
        !superblock
    )
    {
        return false;
    }

    return
        superblock->sector_size ==
            backend->sector_size &&
        superblock->container_sectors ==
            backend->sector_count;
}


static bool superblocksEquivalent(
    const CH_TxSuperblock *a,
    const CH_TxSuperblock *b)
{
    if (
        !a ||
        !b
    )
    {
        return false;
    }

    return
        a->magic ==
            b->magic &&
        a->version ==
            b->version &&
        a->generation ==
            b->generation &&
        a->sector_size ==
            b->sector_size &&
        a->container_sectors ==
            b->container_sectors &&
        a->log_start_sector ==
            b->log_start_sector &&
        a->log_end_sector ==
            b->log_end_sector &&
        a->flags ==
            b->flags;
}


static bool readSuperblock(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    uint32_t sector,
    CH_TxSuperblock *superblock,
    bool *valid)
{
    if (
        !backend ||
        !sector_buffer ||
        !superblock ||
        !valid
    )
    {
        return false;
    }

    *valid =
        false;

    if (!backend->read_sector(
            backend->context,
            sector,
            sector_buffer))
    {
        return false;
    }

    if (!CH_TxDecodeSuperblock(
            superblock,
            sector_buffer,
            backend->sector_size))
    {
        return true;
    }

    if (!superblockGeometryMatches(
            backend,
            superblock))
    {
        return true;
    }

    *valid =
        true;

    return true;
}


CH_TxResult CH_TxReadAuthoritativeSuperblock(
    const CH_TxSectorBackend *backend,
    void *sector_buffer,
    size_t sector_buffer_size,
    CH_TxSuperblock *superblock,
    uint32_t *superblock_sector)
{
    CH_TxSuperblock a;
    CH_TxSuperblock b;

    bool aValid;
    bool bValid;

    uint32_t delta;

    if (
        !CH_TxSectorBackendValid(backend) ||
        !sector_buffer ||
        !superblock ||
        sector_buffer_size <
            backend->sector_size ||
        !CH_TxSectorBufferValid(
            backend,
            sector_buffer) ||
        backend->sector_count <=
            CH_TX_SUPERBLOCK_B_SECTOR
    )
    {
        return
            CH_TX_RESULT_INVALID_ARGUMENT;
    }

    if (!readSuperblock(
            backend,
            sector_buffer,
            CH_TX_SUPERBLOCK_A_SECTOR,
            &a,
            &aValid))
    {
        return
            CH_TX_RESULT_IO;
    }

    if (!readSuperblock(
            backend,
            sector_buffer,
            CH_TX_SUPERBLOCK_B_SECTOR,
            &b,
            &bValid))
    {
        return
            CH_TX_RESULT_IO;
    }

    if (!aValid && !bValid)
    {
        return
            CH_TX_RESULT_CORRUPT;
    }

    if (aValid && !bValid)
    {
        *superblock =
            a;

        if (superblock_sector)
        {
            *superblock_sector =
                CH_TX_SUPERBLOCK_A_SECTOR;
        }

        return
            CH_TX_RESULT_OK;
    }

    if (!aValid && bValid)
    {
        *superblock =
            b;

        if (superblock_sector)
        {
            *superblock_sector =
                CH_TX_SUPERBLOCK_B_SECTOR;
        }

        return
            CH_TX_RESULT_OK;
    }

    if (a.generation ==
        b.generation)
    {
        if (!superblocksEquivalent(
                &a,
                &b))
        {
            return
                CH_TX_RESULT_AMBIGUOUS;
        }

        *superblock =
            a;

        if (superblock_sector)
        {
            *superblock_sector =
                CH_TX_SUPERBLOCK_A_SECTOR;
        }

        return
            CH_TX_RESULT_OK;
    }

    delta =
        b.generation -
        a.generation;

    /*
     * Exactly half the uint32 generation space apart has no unique
     * wrap-aware ordering.
     */
    if (delta == 0x80000000u)
    {
        return
            CH_TX_RESULT_AMBIGUOUS;
    }

    if (delta < 0x80000000u)
    {
        *superblock =
            b;

        if (superblock_sector)
        {
            *superblock_sector =
                CH_TX_SUPERBLOCK_B_SECTOR;
        }
    }
    else
    {
        *superblock =
            a;

        if (superblock_sector)
        {
            *superblock_sector =
                CH_TX_SUPERBLOCK_A_SECTOR;
        }
    }

    return
        CH_TX_RESULT_OK;
}
