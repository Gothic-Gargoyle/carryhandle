#ifndef CARRYHANDLE_CH_RUMBLE_H
#define CARRYHANDLE_CH_RUMBLE_H

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Initialize CarryHandle's asynchronous native GameCube rumble worker.
 *
 * Safe to call more than once.
 */
bool CH_RumbleInit(void);


/*
 * Start or extend one continuous rumble pulse.
 *
 * A shorter pulse request never truncates an already-running single pulse.
 * A longer pulse extends the deadline from the current moment.
 *
 * duration_ms must be non-zero.
 */
bool CH_RumblePulse(
    unsigned int port,
    uint32_t duration_ms
);


/*
 * Start a repeated binary-motor pattern.
 *
 * The first ON phase begins immediately. off_ms may be zero, in which case
 * the physical motor remains continuously active between logical pulses.
 *
 * hard_stop controls whether the final motor stop uses the GameCube's
 * hard-stop command.
 */
bool CH_RumblePattern(
    unsigned int port,
    uint32_t on_ms,
    uint32_t off_ms,
    unsigned int pulses,
    bool hard_stop
);


/*
 * Cancel the current effect on one port.
 */
void CH_RumbleStop(
    unsigned int port,
    bool hard_stop
);


/*
 * Stop all motors, terminate the worker and release rumble state.
 *
 * Safe to call when rumble is not initialized.
 */
void CH_RumbleShutdown(void);


#ifdef __cplusplus
}
#endif

#endif
