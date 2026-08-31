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
