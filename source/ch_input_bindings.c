#include <carryhandle/ch_input_bindings.h>

static bool CH_ActionValid(
    const CH_ActionBindings *bindings,
    size_t action)
{
    return
        bindings != NULL &&
        bindings->inputs != NULL &&
        action < bindings->action_count;
}

bool CH_ActionBindingsInit(
    CH_ActionBindings *bindings,
    CH_PhysicalInput *storage,
    size_t action_count)
{
    if (bindings == NULL ||
        storage == NULL ||
        action_count == 0)
    {
        return false;
    }

    bindings->inputs = storage;
    bindings->action_count = action_count;

    return true;
}

bool CH_ActionBindingsSet(
    CH_ActionBindings *bindings,
    size_t action,
    CH_PhysicalInput input)
{
    if (!CH_ActionValid(bindings, action))
        return false;

    if (input < CH_PHYSICAL_NONE ||
        input >= CH_PHYSICAL_COUNT)
    {
        return false;
    }

    /*
     * Duplicates are intentional.
     *
     * Two or more actions may share one physical input.
     */
    bindings->inputs[action] = input;

    return true;
}

CH_PhysicalInput CH_ActionBindingsGet(
    const CH_ActionBindings *bindings,
    size_t action)
{
    if (!CH_ActionValid(bindings, action))
        return CH_PHYSICAL_NONE;

    return bindings->inputs[action];
}

bool CH_ActionBindingsHeld(
    const CH_ActionBindings *bindings,
    const CH_PadState *pad,
    size_t action)
{
    CH_PhysicalInput input;

    input =
        CH_ActionBindingsGet(
            bindings,
            action);

    if (input == CH_PHYSICAL_NONE)
        return false;

    return CH_InputPhysicalHeld(
        pad,
        input);
}

bool CH_ActionBindingsHasAnalogAxis(
    const CH_ActionBindings *bindings,
    size_t negative_action,
    size_t positive_action)
{
    return CH_InputPhysicalPairIsAnalogAxis(
        CH_ActionBindingsGet(
            bindings,
            negative_action),
        CH_ActionBindingsGet(
            bindings,
            positive_action));
}

int CH_ActionBindingsAnalogAxisRaw(
    const CH_ActionBindings *bindings,
    const CH_PadState *pad,
    size_t negative_action,
    size_t positive_action)
{
    CH_PhysicalInput negative;
    CH_PhysicalInput positive;

    if (pad == NULL)
        return 0;

    negative =
        CH_ActionBindingsGet(
            bindings,
            negative_action);

    positive =
        CH_ActionBindingsGet(
            bindings,
            positive_action);

    if (!CH_InputPhysicalPairIsAnalogAxis(
            negative,
            positive))
    {
        return 0;
    }

    return CH_InputAnalogAxisRaw(
        pad,
        negative,
        positive);
}


float CH_ActionBindingsAnalogAxisFloat(
    const CH_ActionBindings *bindings,
    const CH_PadState *pad,
    size_t negative_action,
    size_t positive_action)
{
    CH_PhysicalInput negative;
    CH_PhysicalInput positive;

    if (pad == NULL)
        return 0.0f;

    negative =
        CH_ActionBindingsGet(
            bindings,
            negative_action);

    positive =
        CH_ActionBindingsGet(
            bindings,
            positive_action);

    if (!CH_InputPhysicalPairIsAnalogAxis(
            negative,
            positive))
    {
        return 0.0f;
    }

    return CH_InputAnalogAxisFloat(
        pad,
        negative,
        positive);
}
