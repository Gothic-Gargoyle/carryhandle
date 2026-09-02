#include <carryhandle/ch_input_physical.h>

#include <stdlib.h>


static int CH_InputPhysicalAxisDeadzone(
    CH_PhysicalInput negative,
    CH_PhysicalInput positive)
{
    switch (negative)
    {
        case CH_PHYSICAL_STICK_LEFT:
            if (positive == CH_PHYSICAL_STICK_RIGHT)
                return CH_INPUT_STICK_DEADZONE;
            break;

        case CH_PHYSICAL_STICK_DOWN:
            if (positive == CH_PHYSICAL_STICK_UP)
                return CH_INPUT_STICK_DEADZONE;
            break;

        case CH_PHYSICAL_CSTICK_LEFT:
            if (positive == CH_PHYSICAL_CSTICK_RIGHT)
                return CH_INPUT_CSTICK_DEADZONE;
            break;

        case CH_PHYSICAL_CSTICK_DOWN:
            if (positive == CH_PHYSICAL_CSTICK_UP)
                return CH_INPUT_CSTICK_DEADZONE;
            break;

        default:
            break;
    }

    return 0;
}


static int CH_InputPhysicalAxisMaximum(
    CH_PhysicalInput negative,
    CH_PhysicalInput positive)
{
    switch (negative)
    {
        case CH_PHYSICAL_STICK_LEFT:
            if (positive == CH_PHYSICAL_STICK_RIGHT)
                return CH_INPUT_STICK_FULL_SCALE;
            break;

        case CH_PHYSICAL_STICK_DOWN:
            if (positive == CH_PHYSICAL_STICK_UP)
                return CH_INPUT_STICK_FULL_SCALE;
            break;

        case CH_PHYSICAL_CSTICK_LEFT:
            if (positive == CH_PHYSICAL_CSTICK_RIGHT)
                return CH_INPUT_CSTICK_FULL_SCALE;
            break;

        case CH_PHYSICAL_CSTICK_DOWN:
            if (positive == CH_PHYSICAL_CSTICK_UP)
                return CH_INPUT_CSTICK_FULL_SCALE;
            break;

        default:
            break;
    }

    return 0;
}


bool CH_InputPhysicalHeld(
    const CH_PadState *pad,
    CH_PhysicalInput input)
{
    if (!pad ||
        !pad->connected)
    {
        return false;
    }

    switch (input)
    {
        case CH_PHYSICAL_A:
            return
                (pad->buttons_held &
                 CH_PAD_BUTTON_A) != 0;

        case CH_PHYSICAL_B:
            return
                (pad->buttons_held &
                 CH_PAD_BUTTON_B) != 0;

        case CH_PHYSICAL_X:
            return
                (pad->buttons_held &
                 CH_PAD_BUTTON_X) != 0;

        case CH_PHYSICAL_Y:
            return
                (pad->buttons_held &
                 CH_PAD_BUTTON_Y) != 0;

        case CH_PHYSICAL_L:
            return
                (pad->buttons_held &
                 CH_PAD_BUTTON_L) != 0;

        case CH_PHYSICAL_R:
            return
                (pad->buttons_held &
                 CH_PAD_BUTTON_R) != 0;

        case CH_PHYSICAL_Z:
            return
                (pad->buttons_held &
                 CH_PAD_BUTTON_Z) != 0;

        case CH_PHYSICAL_START:
            return
                (pad->buttons_held &
                 CH_PAD_BUTTON_START) != 0;

        case CH_PHYSICAL_DPAD_UP:
            return
                (pad->buttons_held &
                 CH_PAD_BUTTON_UP) != 0;

        case CH_PHYSICAL_DPAD_DOWN:
            return
                (pad->buttons_held &
                 CH_PAD_BUTTON_DOWN) != 0;

        case CH_PHYSICAL_DPAD_LEFT:
            return
                (pad->buttons_held &
                 CH_PAD_BUTTON_LEFT) != 0;

        case CH_PHYSICAL_DPAD_RIGHT:
            return
                (pad->buttons_held &
                 CH_PAD_BUTTON_RIGHT) != 0;

        case CH_PHYSICAL_STICK_UP:
            return
                pad->stick_y >
                CH_INPUT_STICK_DEADZONE;

        case CH_PHYSICAL_STICK_DOWN:
            return
                pad->stick_y <
                -CH_INPUT_STICK_DEADZONE;

        case CH_PHYSICAL_STICK_LEFT:
            return
                pad->stick_x <
                -CH_INPUT_STICK_DEADZONE;

        case CH_PHYSICAL_STICK_RIGHT:
            return
                pad->stick_x >
                CH_INPUT_STICK_DEADZONE;

        case CH_PHYSICAL_CSTICK_UP:
            return
                pad->cstick_y >
                CH_INPUT_CSTICK_DEADZONE;

        case CH_PHYSICAL_CSTICK_DOWN:
            return
                pad->cstick_y <
                -CH_INPUT_CSTICK_DEADZONE;

        case CH_PHYSICAL_CSTICK_LEFT:
            return
                pad->cstick_x <
                -CH_INPUT_CSTICK_DEADZONE;

        case CH_PHYSICAL_CSTICK_RIGHT:
            return
                pad->cstick_x >
                CH_INPUT_CSTICK_DEADZONE;

        case CH_PHYSICAL_NONE:
        case CH_PHYSICAL_COUNT:
        default:
            return false;
    }
}


const char *CH_InputPhysicalName(
    CH_PhysicalInput input)
{
    switch (input)
    {
        case CH_PHYSICAL_A:
            return "A";

        case CH_PHYSICAL_B:
            return "B";

        case CH_PHYSICAL_X:
            return "X";

        case CH_PHYSICAL_Y:
            return "Y";

        case CH_PHYSICAL_L:
            return "L";

        case CH_PHYSICAL_R:
            return "R";

        case CH_PHYSICAL_Z:
            return "Z";

        case CH_PHYSICAL_START:
            return "START";

        case CH_PHYSICAL_DPAD_UP:
            return "D-PAD UP";

        case CH_PHYSICAL_DPAD_DOWN:
            return "D-PAD DOWN";

        case CH_PHYSICAL_DPAD_LEFT:
            return "D-PAD LEFT";

        case CH_PHYSICAL_DPAD_RIGHT:
            return "D-PAD RIGHT";

        case CH_PHYSICAL_STICK_UP:
            return "STICK UP";

        case CH_PHYSICAL_STICK_DOWN:
            return "STICK DOWN";

        case CH_PHYSICAL_STICK_LEFT:
            return "STICK LEFT";

        case CH_PHYSICAL_STICK_RIGHT:
            return "STICK RIGHT";

        case CH_PHYSICAL_CSTICK_UP:
            return "C UP";

        case CH_PHYSICAL_CSTICK_DOWN:
            return "C DOWN";

        case CH_PHYSICAL_CSTICK_LEFT:
            return "C LEFT";

        case CH_PHYSICAL_CSTICK_RIGHT:
            return "C RIGHT";

        case CH_PHYSICAL_NONE:
        case CH_PHYSICAL_COUNT:
        default:
            return "NONE";
    }
}


CH_PhysicalInput CH_InputFirstPhysicalHeld(
    const CH_PadState *pad)
{
    CH_PhysicalInput input;

    if (!pad ||
        !pad->connected)
    {
        return CH_PHYSICAL_NONE;
    }

    for (input = CH_PHYSICAL_A;
         input < CH_PHYSICAL_COUNT;
         ++input)
    {
        if (CH_InputPhysicalHeld(
                pad,
                input))
        {
            return input;
        }
    }

    return CH_PHYSICAL_NONE;
}


bool CH_InputPhysicalPairIsAnalogAxis(
    CH_PhysicalInput negative,
    CH_PhysicalInput positive)
{
    if (negative == CH_PHYSICAL_STICK_LEFT &&
        positive == CH_PHYSICAL_STICK_RIGHT)
    {
        return true;
    }

    if (negative == CH_PHYSICAL_STICK_DOWN &&
        positive == CH_PHYSICAL_STICK_UP)
    {
        return true;
    }

    if (negative == CH_PHYSICAL_CSTICK_LEFT &&
        positive == CH_PHYSICAL_CSTICK_RIGHT)
    {
        return true;
    }

    if (negative == CH_PHYSICAL_CSTICK_DOWN &&
        positive == CH_PHYSICAL_CSTICK_UP)
    {
        return true;
    }

    return false;
}


int CH_InputAnalogAxisRaw(
    const CH_PadState *pad,
    CH_PhysicalInput negative,
    CH_PhysicalInput positive)
{
    if (!pad ||
        !pad->connected)
    {
        return 0;
    }

    if (negative == CH_PHYSICAL_STICK_LEFT &&
        positive == CH_PHYSICAL_STICK_RIGHT)
    {
        return
            (int)pad->stick_x;
    }

    if (negative == CH_PHYSICAL_STICK_DOWN &&
        positive == CH_PHYSICAL_STICK_UP)
    {
        return
            (int)pad->stick_y;
    }

    if (negative == CH_PHYSICAL_CSTICK_LEFT &&
        positive == CH_PHYSICAL_CSTICK_RIGHT)
    {
        return
            (int)pad->cstick_x;
    }

    if (negative == CH_PHYSICAL_CSTICK_DOWN &&
        positive == CH_PHYSICAL_CSTICK_UP)
    {
        return
            (int)pad->cstick_y;
    }

    return 0;
}


int CH_InputAnalogAxisNormalized(
    const CH_PadState *pad,
    CH_PhysicalInput negative,
    CH_PhysicalInput positive)
{
    int value;
    int magnitude;
    int deadzone;
    int maximum;

    if (!CH_InputPhysicalPairIsAnalogAxis(
            negative,
            positive))
    {
        return 0;
    }

    value =
        CH_InputAnalogAxisRaw(
            pad,
            negative,
            positive);

    deadzone =
        CH_InputPhysicalAxisDeadzone(
            negative,
            positive);

    maximum =
        CH_InputPhysicalAxisMaximum(
            negative,
            positive);

    if (maximum <= deadzone)
    {
        return 0;
    }

    magnitude =
        abs(value);

    if (magnitude <= deadzone)
    {
        return 0;
    }

    /*
     * DoomCube-proven axis shaping:
     *
     *     deadzone edge -> 0
     *     practical max -> 127
     */
    if (magnitude > maximum)
    {
        magnitude =
            maximum;
    }

    magnitude =
        (magnitude - deadzone) *
        127 /
        (maximum - deadzone);

    if (value < 0)
    {
        magnitude =
            -magnitude;
    }

    return magnitude;
}


float CH_InputAnalogAxisFloat(
    const CH_PadState *pad,
    CH_PhysicalInput negative,
    CH_PhysicalInput positive)
{
    return
        (float)CH_InputAnalogAxisNormalized(
            pad,
            negative,
            positive) /
        127.0f;
}
