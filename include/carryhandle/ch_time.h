#ifndef CARRYHANDLE_CH_TIME_H
#define CARRYHANDLE_CH_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Return monotonic milliseconds from the GameCube hardware timebase.
 */
uint64_t CH_TimeMilliseconds(void);

#ifdef __cplusplus
}
#endif

#endif
