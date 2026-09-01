#include <carryhandle/ch_input.h>

#include "ch_pad_bus.h"

#include <ogc/pad.h>

#include <string.h>


static CH_PadState chPads[CH_PAD_COUNT];
static bool chInputInitialized;


static void CH_InputClearPads(void)
{
    memset(
        chPads,
        0,
        sizeof(chPads)
    );
}


bool CH_InputInit(void)
{
    if (chInputInitialized)
        return true;

    CH_InputClearPads();

    if (!CH_PadBusInit())
        return false;

    chInputInitialized =
        true;

    return true;
}


void CH_InputPoll(void)
{
    u32 connectedMask;
    unsigned int port;

    if (!chInputInitialized)
    {
        if (!CH_InputInit())
        {
            CH_InputClearPads();
            return;
        }
    }

    if (!CH_PadBusScan(
            &connectedMask))
    {
        CH_InputClearPads();
        return;
    }

    for (port = 0;
         port < CH_PAD_COUNT;
         ++port)
    {
        CH_PadState *state =
            &chPads[port];

        memset(
            state,
            0,
            sizeof(*state)
        );

        state->connected =
            (connectedMask & (1u << port)) != 0;

        if (!state->connected)
            continue;

        state->buttons_held =
            (uint16_t)PAD_ButtonsHeld((s32)port);

        state->buttons_down =
            (uint16_t)PAD_ButtonsDown((s32)port);

        state->buttons_up =
            (uint16_t)PAD_ButtonsUp((s32)port);

        state->stick_x =
            PAD_StickX((s32)port);

        state->stick_y =
            PAD_StickY((s32)port);

        state->cstick_x =
            PAD_SubStickX((s32)port);

        state->cstick_y =
            PAD_SubStickY((s32)port);

        state->trigger_l =
            PAD_TriggerL((s32)port);

        state->trigger_r =
            PAD_TriggerR((s32)port);
    }
}


bool CH_InputGetPad(
    unsigned int port,
    CH_PadState *state
)
{
    if (!state ||
        port >= CH_PAD_COUNT)
    {
        return false;
    }

    *state =
        chPads[port];

    return true;
}
