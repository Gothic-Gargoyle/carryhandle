#ifndef CARRYHANDLE_CH_INPUT_H
#define CARRYHANDLE_CH_INPUT_H

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


#define CH_PAD_COUNT 4u


/*
 * CarryHandle button masks intentionally mirror the native GameCube
 * controller bit layout while keeping application code independent of
 * libogc2's PAD_* names.
 */
typedef enum
{
    CH_PAD_BUTTON_LEFT  = 0x0001u,
    CH_PAD_BUTTON_RIGHT = 0x0002u,
    CH_PAD_BUTTON_DOWN  = 0x0004u,
    CH_PAD_BUTTON_UP    = 0x0008u,

    CH_PAD_BUTTON_Z     = 0x0010u,
    CH_PAD_BUTTON_R     = 0x0020u,
    CH_PAD_BUTTON_L     = 0x0040u,

    CH_PAD_BUTTON_A     = 0x0100u,
    CH_PAD_BUTTON_B     = 0x0200u,
    CH_PAD_BUTTON_X     = 0x0400u,
    CH_PAD_BUTTON_Y     = 0x0800u,

    CH_PAD_BUTTON_START = 0x1000u
} CH_PadButton;


/*
 * One controller snapshot produced by CH_InputPoll().
 *
 * Stick axes retain the native signed GameCube range.
 * Trigger values retain the native unsigned analogue range.
 *
 * CarryHandle deliberately applies no game-specific deadzone,
 * sensitivity or action mapping here.
 */
typedef struct
{
    bool connected;

    uint16_t buttons_held;
    uint16_t buttons_down;
    uint16_t buttons_up;

    int8_t stick_x;
    int8_t stick_y;

    int8_t cstick_x;
    int8_t cstick_y;

    uint8_t trigger_l;
    uint8_t trigger_r;
} CH_PadState;


/*
 * Initialize the native GameCube controller subsystem and clear all
 * CarryHandle controller snapshots.
 *
 * Safe to call more than once.
 */
bool CH_InputInit(void);


/*
 * Poll all native GameCube controller ports once and replace the current
 * CarryHandle snapshots.
 */
void CH_InputPoll(void);


/*
 * Copy the most recent snapshot for one controller port.
 *
 * Returns false only for an invalid port or NULL output pointer.
 * A valid but disconnected port returns true with state.connected=false.
 */
bool CH_InputGetPad(
    unsigned int port,
    CH_PadState *state
);


#ifdef __cplusplus
}
#endif

#endif
