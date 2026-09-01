#include <carryhandle/ch_memcard.h>

#include <stdint.h>
#include <string.h>


bool CH_MemCardMount(
    CH_MemCardSession *session,
    s32 slot,
    const char *game_code,
    const char *company_code,
    void *work_area
)
{
    s32 result;

    if (
        !session
        || !game_code
        || !company_code
        || !work_area
    )
    {
        return false;
    }

    if (
        ((uintptr_t)work_area & 31u)
        != 0u
    )
    {
        return false;
    }

    memset(
        session,
        0,
        sizeof(*session)
    );

    session->slot = slot;

    result =
        CARD_Init(
            game_code,
            company_code
        );

    if (result < CARD_ERROR_READY)
    {
        return false;
    }

    do
    {
        result =
            CARD_ProbeEx(
                slot,
                &session->memory_size,
                &session->sector_size
            );
    }
    while (result == CARD_ERROR_BUSY);

    if (result < CARD_ERROR_READY)
    {
        return false;
    }

    result =
        CARD_Mount(
            slot,
            work_area,
            NULL
        );

    if (result < CARD_ERROR_READY)
    {
        return false;
    }

    session->mounted = true;

    return true;
}


bool CH_MemCardUnmount(
    CH_MemCardSession *session
)
{
    s32 result;

    if (!session)
    {
        return false;
    }

    if (!session->mounted)
    {
        return true;
    }

    result =
        CARD_Unmount(
            session->slot
        );

    if (result < CARD_ERROR_READY)
    {
        return false;
    }

    session->mounted = false;

    return true;
}

static bool CH_MemCardFilenameValid(
    const char *filename
)
{
    size_t length;

    if (!filename)
    {
        return false;
    }

    length = strlen(filename);

    return (
        length > 0u
        && length <= CARD_FILENAMELEN
    );
}


s32 CH_MemCardOpen(
    const CH_MemCardSession *session,
    const char *filename,
    card_file *file
)
{
    if (
        !session
        || !file
        || !CH_MemCardFilenameValid(filename)
    )
    {
        return CH_MEMCARD_ERROR_INVALID_ARGUMENT;
    }

    if (!session->mounted)
    {
        return CH_MEMCARD_ERROR_NOT_MOUNTED;
    }

    return CARD_Open(
        session->slot,
        filename,
        file
    );
}


s32 CH_MemCardCreate(
    const CH_MemCardSession *session,
    const char *filename,
    u32 size,
    card_file *file
)
{
    if (
        !session
        || !file
        || !CH_MemCardFilenameValid(filename)
        || session->sector_size <= 0
        || size == 0u
        || (
            size
            % (u32)session->sector_size
        ) != 0u
    )
    {
        return CH_MEMCARD_ERROR_INVALID_ARGUMENT;
    }

    if (!session->mounted)
    {
        return CH_MEMCARD_ERROR_NOT_MOUNTED;
    }

    return CARD_Create(
        session->slot,
        filename,
        size,
        file
    );
}

s32 CH_MemCardClose(
    card_file *file
)
{
    if (!file)
    {
        return CH_MEMCARD_ERROR_INVALID_ARGUMENT;
    }

    return CARD_Close(file);
}


s32 CH_MemCardRead(
    card_file *file,
    void *buffer,
    u32 len,
    u32 offset
)
{
    if (
        !file
        || !buffer
        || len == 0u
        || (
            (uintptr_t)buffer
            & 31u
        ) != 0u
    )
    {
        return CH_MEMCARD_ERROR_INVALID_ARGUMENT;
    }

    return CARD_Read(
        file,
        buffer,
        len,
        offset
    );
}
