#include "ch_pad_bus.h"

#include <ogc/mutex.h>
#include <ogc/pad.h>


static mutex_t chPadBusMutex;
static bool chPadBusInitialized;


bool CH_PadBusInit(void)
{
    if (chPadBusInitialized)
        return true;

    if (LWP_MutexInit(
            &chPadBusMutex,
            false) != 0)
    {
        return false;
    }

    if (PAD_Init() == 0)
    {
        LWP_MutexDestroy(
            chPadBusMutex);

        return false;
    }

    chPadBusInitialized =
        true;

    return true;
}


bool CH_PadBusScan(
    uint32_t *connected_mask
)
{
    if (!connected_mask ||
        !CH_PadBusInit())
    {
        return false;
    }

    if (LWP_MutexLock(
            chPadBusMutex) != 0)
    {
        return false;
    }

    *connected_mask =
        PAD_ScanPads();

    if (LWP_MutexUnlock(
            chPadBusMutex) != 0)
    {
        return false;
    }

    return true;
}


bool CH_PadBusControlMotor(
    unsigned int port,
    unsigned int command
)
{
    if (port >= PAD_CHANMAX ||
        !CH_PadBusInit())
    {
        return false;
    }

    if (LWP_MutexLock(
            chPadBusMutex) != 0)
    {
        return false;
    }

    PAD_ControlMotor(
        (s32)port,
        (u32)command);

    if (LWP_MutexUnlock(
            chPadBusMutex) != 0)
    {
        return false;
    }

    return true;
}
