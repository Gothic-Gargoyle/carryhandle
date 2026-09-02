#ifndef CARRYHANDLE_CH_INPUT_PHYSICAL_H
#define CARRYHANDLE_CH_INPUT_PHYSICAL_H

#include <stdbool.h>

#include <carryhandle/ch_input.h>


/*
 * CarryHandle physical-input vocabulary.
 *
 * These values identify actual GameCube controls.
 *
 * They are intentionally NOT game actions such as:
 *
 *     forward
 *     attack
 *     jump
 *
 * Games build their own action bindings on top.
 *
 * The distinction between:
 *
 *     D-pad left
 *     main-stick left
 *     C-stick left
 *
 * is preserved all the way through this layer.
 */
typedef enum
{
    CH_PHYSICAL_NONE = 0,

    CH_PHYSICAL_A,
    CH_PHYSICAL_B,
    CH_PHYSICAL_X,
    CH_PHYSICAL_Y,

    CH_PHYSICAL_L,
    CH_PHYSICAL_R,
    CH_PHYSICAL_Z,
    CH_PHYSICAL_START,

    CH_PHYSICAL_DPAD_UP,
    CH_PHYSICAL_DPAD_DOWN,
    CH_PHYSICAL_DPAD_LEFT,
    CH_PHYSICAL_DPAD_RIGHT,

    CH_PHYSICAL_STICK_UP,
    CH_PHYSICAL_STICK_DOWN,
    CH_PHYSICAL_STICK_LEFT,
    CH_PHYSICAL_STICK_RIGHT,

    CH_PHYSICAL_CSTICK_UP,
    CH_PHYSICAL_CSTICK_DOWN,
    CH_PHYSICAL_CSTICK_LEFT,
    CH_PHYSICAL_CSTICK_RIGHT,

    CH_PHYSICAL_COUNT
} CH_PhysicalInput;


/*
 * DoomCube-proven GameCube axis characteristics.
 *
 * Main stick:
 *
 *     deadzone       16
 *     intended max  100
 *
 * Real GameCube main sticks commonly reach cardinal maxima around
 * 97..112 rather than the theoretical signed-byte maximum of 127.
 *
 * C-stick:
 *
 *     deadzone       24
 *     intended max  127
 */
#define CH_INPUT_STICK_DEADZONE       16
#define CH_INPUT_CSTICK_DEADZONE      24

#define CH_INPUT_STICK_FULL_SCALE    100
#define CH_INPUT_CSTICK_FULL_SCALE   127


/*
 * Test one physical control.
 *
 * Analogue directions use their appropriate physical deadzone.
 *
 * This does NOT merge D-pad and analogue-stick directions.
 */
bool CH_InputPhysicalHeld(
    const CH_PadState *pad,
    CH_PhysicalInput input);


/*
 * Return a stable human-readable control name suitable for:
 *
 *     controls menus
 *     binding displays
 *     debugging
 */
const char *CH_InputPhysicalName(
    CH_PhysicalInput input);


/*
 * Return the first currently-held physical input.
 *
 * Useful as the primitive beneath a game's "press a button/control"
 * binding-capture UI.
 *
 * Games remain responsible for capture arming/release policy.
 */
CH_PhysicalInput CH_InputFirstPhysicalHeld(
    const CH_PadState *pad);


/*
 * True only when the supplied negative/positive physical inputs are
 * opposite directions of ONE analogue axis.
 *
 * Valid examples:
 *
 *     STICK_LEFT  + STICK_RIGHT
 *     STICK_DOWN  + STICK_UP
 *     CSTICK_LEFT + CSTICK_RIGHT
 *     CSTICK_DOWN + CSTICK_UP
 *
 * D-pad pairs deliberately return false.
 */
bool CH_InputPhysicalPairIsAnalogAxis(
    CH_PhysicalInput negative,
    CH_PhysicalInput positive);


/*
 * Return the native signed value for an analogue input pair.
 *
 * No deadzone or scaling is applied.
 *
 * Main/C-stick axes therefore retain their native signed PAD values.
 *
 * Returns zero when the pair is not a valid analogue axis.
 */
int CH_InputAnalogAxisRaw(
    const CH_PadState *pad,
    CH_PhysicalInput negative,
    CH_PhysicalInput positive);


/*
 * DoomCube-style movement normalization.
 *
 * Returns:
 *
 *     -127 .. 127
 *
 * after:
 *
 *     1. selecting the native physical axis
 *     2. applying its correct deadzone
 *     3. clamping to its practical full scale
 *     4. removing the deadzone
 *     5. rescaling the remaining travel back to full range
 *
 * This is suitable for software-rendered PC game ports that want
 * genuine proportional controller movement.
 */
int CH_InputAnalogAxisNormalized(
    const CH_PadState *pad,
    CH_PhysicalInput negative,
    CH_PhysicalInput positive);


/*
 * Convenience floating-point form of the normalized axis:
 *
 *     -1.0f .. +1.0f
 */
float CH_InputAnalogAxisFloat(
    const CH_PadState *pad,
    CH_PhysicalInput negative,
    CH_PhysicalInput positive);


#endif
