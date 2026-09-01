#ifndef CARRYHANDLE_INTERNAL_CH_PAD_BUS_H
#define CARRYHANDLE_INTERNAL_CH_PAD_BUS_H

#include <stdbool.h>
#include <stdint.h>

bool CH_PadBusInit(void);

bool CH_PadBusScan(
    uint32_t *connected_mask
);

bool CH_PadBusControlMotor(
    unsigned int port,
    unsigned int command
);

#endif
