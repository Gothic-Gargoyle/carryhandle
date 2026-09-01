#include <carryhandle/ch_tx_format.h>

#include <string.h>


static void putU32(
    uint8_t *buffer,
    size_t offset,
    uint32_t value)
{
    buffer[offset + 0u] =
        (uint8_t)(value >> 24);

    buffer[offset + 1u] =
        (uint8_t)(value >> 16);

    buffer[offset + 2u] =
        (uint8_t)(value >> 8);

    buffer[offset + 3u] =
        (uint8_t)value;
}


static uint32_t getU32(
    const uint8_t *buffer,
    size_t offset)
{
    return
        ((uint32_t)buffer[offset + 0u] << 24) |
        ((uint32_t)buffer[offset + 1u] << 16) |
        ((uint32_t)buffer[offset + 2u] << 8) |
        ((uint32_t)buffer[offset + 3u]);
}


/*
 * Standard CRC-32/ISO-HDLC using the reflected 0xedb88320 polynomial.
 *
 * The CRC field itself is logically zero while calculating the checksum.
 */
static uint32_t encodedCrc32(
    const uint8_t *buffer,
    size_t size,
    size_t crc_offset)
{
    uint32_t crc =
        0xffffffffu;

    size_t i;
    unsigned int bit;

    for (i = 0; i < size; ++i)
    {
        uint8_t value =
            (
                i >= crc_offset &&
                i < crc_offset + 4u
            )
            ? 0u
            : buffer[i];

        crc ^=
            (uint32_t)value;

        for (bit = 0; bit < 8u; ++bit)
        {
            uint32_t mask =
                0u -
                (crc & 1u);

            crc =
                (crc >> 1) ^
                (0xedb88320u & mask);
        }
    }

    return
        crc ^ 0xffffffffu;
}


static bool validSectorSize(
    uint32_t sector_size)
{
    return
        sector_size >=
        CH_TX_RECORD_HEADER_ENCODED_SIZE;
}


/* ------------------------------------------------------------------------- */
/* Container header                                                          */
/* ------------------------------------------------------------------------- */

enum
{
    CONTAINER_MAGIC_OFFSET = 0,
    CONTAINER_VERSION_OFFSET = 4,
    CONTAINER_HEADER_SIZE_OFFSET = 8,
    CONTAINER_SECTOR_SIZE_OFFSET = 12,
    CONTAINER_SECTORS_OFFSET = 16,
    CONTAINER_REPLICA_INDEX_OFFSET = 20,
    CONTAINER_FLAGS_OFFSET = 24,
    CONTAINER_CRC_OFFSET = 28
};


static bool validContainerHeader(
    const CH_TxContainerHeader *header)
{
    if (!header)
    {
        return false;
    }

    return
        validSectorSize(
            header->sector_size
        ) &&
        header->container_sectors >=
            CH_TX_DATA_START_SECTOR &&
        header->replica_index <= 1u;
}


bool CH_TxEncodeContainerHeader(
    uint8_t *buffer,
    size_t buffer_size,
    const CH_TxContainerHeader *header)
{
    uint32_t crc;

    if (
        !buffer ||
        !validContainerHeader(header) ||
        buffer_size <
            CH_TX_CONTAINER_HEADER_ENCODED_SIZE
    )
    {
        return false;
    }

    memset(
        buffer,
        0,
        CH_TX_CONTAINER_HEADER_ENCODED_SIZE
    );

    putU32(
        buffer,
        CONTAINER_MAGIC_OFFSET,
        CH_TX_CONTAINER_MAGIC
    );

    putU32(
        buffer,
        CONTAINER_VERSION_OFFSET,
        CH_TX_FORMAT_VERSION
    );

    putU32(
        buffer,
        CONTAINER_HEADER_SIZE_OFFSET,
        CH_TX_CONTAINER_HEADER_ENCODED_SIZE
    );

    putU32(
        buffer,
        CONTAINER_SECTOR_SIZE_OFFSET,
        header->sector_size
    );

    putU32(
        buffer,
        CONTAINER_SECTORS_OFFSET,
        header->container_sectors
    );

    putU32(
        buffer,
        CONTAINER_REPLICA_INDEX_OFFSET,
        header->replica_index
    );

    putU32(
        buffer,
        CONTAINER_FLAGS_OFFSET,
        header->flags
    );

    crc =
        encodedCrc32(
            buffer,
            CH_TX_CONTAINER_HEADER_ENCODED_SIZE,
            CONTAINER_CRC_OFFSET
        );

    putU32(
        buffer,
        CONTAINER_CRC_OFFSET,
        crc
    );

    return true;
}


bool CH_TxDecodeContainerHeader(
    CH_TxContainerHeader *header,
    const uint8_t *buffer,
    size_t buffer_size)
{
    CH_TxContainerHeader decoded;

    uint32_t storedCrc;
    uint32_t calculatedCrc;

    if (
        !header ||
        !buffer ||
        buffer_size <
            CH_TX_CONTAINER_HEADER_ENCODED_SIZE
    )
    {
        return false;
    }

    storedCrc =
        getU32(
            buffer,
            CONTAINER_CRC_OFFSET
        );

    calculatedCrc =
        encodedCrc32(
            buffer,
            CH_TX_CONTAINER_HEADER_ENCODED_SIZE,
            CONTAINER_CRC_OFFSET
        );

    if (storedCrc != calculatedCrc)
    {
        return false;
    }

    memset(
        &decoded,
        0,
        sizeof(decoded)
    );

    decoded.magic =
        getU32(
            buffer,
            CONTAINER_MAGIC_OFFSET
        );

    decoded.version =
        getU32(
            buffer,
            CONTAINER_VERSION_OFFSET
        );

    decoded.header_size =
        getU32(
            buffer,
            CONTAINER_HEADER_SIZE_OFFSET
        );

    decoded.sector_size =
        getU32(
            buffer,
            CONTAINER_SECTOR_SIZE_OFFSET
        );

    decoded.container_sectors =
        getU32(
            buffer,
            CONTAINER_SECTORS_OFFSET
        );

    decoded.replica_index =
        getU32(
            buffer,
            CONTAINER_REPLICA_INDEX_OFFSET
        );

    decoded.flags =
        getU32(
            buffer,
            CONTAINER_FLAGS_OFFSET
        );

    decoded.header_crc32 =
        storedCrc;

    if (
        decoded.magic !=
            CH_TX_CONTAINER_MAGIC ||
        decoded.version !=
            CH_TX_FORMAT_VERSION ||
        decoded.header_size !=
            CH_TX_CONTAINER_HEADER_ENCODED_SIZE ||
        !validContainerHeader(&decoded)
    )
    {
        return false;
    }

    *header =
        decoded;

    return true;
}


/* ------------------------------------------------------------------------- */
/* Superblock                                                                */
/* ------------------------------------------------------------------------- */

enum
{
    SUPER_MAGIC_OFFSET = 0,
    SUPER_VERSION_OFFSET = 4,
    SUPER_GENERATION_OFFSET = 8,
    SUPER_SECTOR_SIZE_OFFSET = 12,
    SUPER_CONTAINER_SECTORS_OFFSET = 16,
    SUPER_LOG_START_OFFSET = 20,
    SUPER_LOG_END_OFFSET = 24,
    SUPER_FLAGS_OFFSET = 28,
    SUPER_CRC_OFFSET = 32
};


static bool validSuperblock(
    const CH_TxSuperblock *superblock)
{
    if (!superblock)
    {
        return false;
    }

    return
        validSectorSize(
            superblock->sector_size
        ) &&
        superblock->container_sectors >=
            CH_TX_DATA_START_SECTOR &&
        superblock->log_start_sector ==
            CH_TX_DATA_START_SECTOR &&
        superblock->log_end_sector >=
            superblock->log_start_sector &&
        superblock->log_end_sector <=
            superblock->container_sectors;
}


bool CH_TxEncodeSuperblock(
    uint8_t *buffer,
    size_t buffer_size,
    const CH_TxSuperblock *superblock)
{
    uint32_t crc;

    if (
        !buffer ||
        !validSuperblock(superblock) ||
        buffer_size <
            CH_TX_SUPERBLOCK_ENCODED_SIZE
    )
    {
        return false;
    }

    memset(
        buffer,
        0,
        CH_TX_SUPERBLOCK_ENCODED_SIZE
    );

    putU32(
        buffer,
        SUPER_MAGIC_OFFSET,
        CH_TX_SUPERBLOCK_MAGIC
    );

    putU32(
        buffer,
        SUPER_VERSION_OFFSET,
        CH_TX_FORMAT_VERSION
    );

    putU32(
        buffer,
        SUPER_GENERATION_OFFSET,
        superblock->generation
    );

    putU32(
        buffer,
        SUPER_SECTOR_SIZE_OFFSET,
        superblock->sector_size
    );

    putU32(
        buffer,
        SUPER_CONTAINER_SECTORS_OFFSET,
        superblock->container_sectors
    );

    putU32(
        buffer,
        SUPER_LOG_START_OFFSET,
        superblock->log_start_sector
    );

    putU32(
        buffer,
        SUPER_LOG_END_OFFSET,
        superblock->log_end_sector
    );

    putU32(
        buffer,
        SUPER_FLAGS_OFFSET,
        superblock->flags
    );

    crc =
        encodedCrc32(
            buffer,
            CH_TX_SUPERBLOCK_ENCODED_SIZE,
            SUPER_CRC_OFFSET
        );

    putU32(
        buffer,
        SUPER_CRC_OFFSET,
        crc
    );

    return true;
}


bool CH_TxDecodeSuperblock(
    CH_TxSuperblock *superblock,
    const uint8_t *buffer,
    size_t buffer_size)
{
    CH_TxSuperblock decoded;

    uint32_t storedCrc;
    uint32_t calculatedCrc;

    if (
        !superblock ||
        !buffer ||
        buffer_size <
            CH_TX_SUPERBLOCK_ENCODED_SIZE
    )
    {
        return false;
    }

    storedCrc =
        getU32(
            buffer,
            SUPER_CRC_OFFSET
        );

    calculatedCrc =
        encodedCrc32(
            buffer,
            CH_TX_SUPERBLOCK_ENCODED_SIZE,
            SUPER_CRC_OFFSET
        );

    if (storedCrc != calculatedCrc)
    {
        return false;
    }

    memset(
        &decoded,
        0,
        sizeof(decoded)
    );

    decoded.magic =
        getU32(
            buffer,
            SUPER_MAGIC_OFFSET
        );

    decoded.version =
        getU32(
            buffer,
            SUPER_VERSION_OFFSET
        );

    decoded.generation =
        getU32(
            buffer,
            SUPER_GENERATION_OFFSET
        );

    decoded.sector_size =
        getU32(
            buffer,
            SUPER_SECTOR_SIZE_OFFSET
        );

    decoded.container_sectors =
        getU32(
            buffer,
            SUPER_CONTAINER_SECTORS_OFFSET
        );

    decoded.log_start_sector =
        getU32(
            buffer,
            SUPER_LOG_START_OFFSET
        );

    decoded.log_end_sector =
        getU32(
            buffer,
            SUPER_LOG_END_OFFSET
        );

    decoded.flags =
        getU32(
            buffer,
            SUPER_FLAGS_OFFSET
        );

    decoded.superblock_crc32 =
        storedCrc;

    if (
        decoded.magic !=
            CH_TX_SUPERBLOCK_MAGIC ||
        decoded.version !=
            CH_TX_FORMAT_VERSION ||
        !validSuperblock(&decoded)
    )
    {
        return false;
    }

    *superblock =
        decoded;

    return true;
}
