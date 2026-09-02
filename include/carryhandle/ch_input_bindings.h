#ifndef CARRYHANDLE_CH_INPUT_BINDINGS_H
#define CARRYHANDLE_CH_INPUT_BINDINGS_H

#include <stdbool.h>
#include <stddef.h>

#include <carryhandle/ch_input_physical.h>

typedef struct
{
    CH_PhysicalInput *inputs;
    size_t action_count;
} CH_ActionBindings;

bool CH_ActionBindingsInit(
    CH_ActionBindings *bindings,
    CH_PhysicalInput *storage,
    size_t action_count);

bool CH_ActionBindingsSet(
    CH_ActionBindings *bindings,
    size_t action,
    CH_PhysicalInput input);

CH_PhysicalInput CH_ActionBindingsGet(
    const CH_ActionBindings *bindings,
    size_t action);

bool CH_ActionBindingsHeld(
    const CH_ActionBindings *bindings,
    const CH_PadState *pad,
    size_t action);

bool CH_ActionBindingsHasAnalogAxis(
    const CH_ActionBindings *bindings,
    size_t negative_action,
    size_t positive_action);

int CH_ActionBindingsAnalogAxisRaw(
    const CH_ActionBindings *bindings,
    const CH_PadState *pad,
    size_t negative_action,
    size_t positive_action);

float CH_ActionBindingsAnalogAxisFloat(
    const CH_ActionBindings *bindings,
    const CH_PadState *pad,
    size_t negative_action,
    size_t positive_action);

#endif
