#include <carryhandle/ch_rumble.h>

#include "ch_pad_bus.h"

#include <ogc/cond.h>
#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <ogc/pad.h>
#include <ogc/timesupp.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>


#define CH_RUMBLE_PORT_COUNT 4u
#define CH_RUMBLE_THREAD_PRIORITY 80u


typedef struct
{
    bool active;
    bool on_phase;
    bool motor_on;
    bool hard_stop_at_end;

    uint64_t deadline_ms;
    uint32_t on_ms;
    uint32_t off_ms;

    unsigned int pulses_left;
} CH_RumblePortState;


static mutex_t chRumbleMutex;
static cond_t chRumbleCond;
static lwp_t chRumbleThread;

static bool chRumbleInitialized;
static bool chRumbleStopRequested;

static CH_RumblePortState
    chRumblePorts[CH_RUMBLE_PORT_COUNT];


static uint64_t CH_RumbleNowMs(void)
{
    return
        (uint64_t)ticks_to_millisecs(
            gettime()
        );
}


static struct timespec CH_RumbleRelativeTime(
    uint64_t milliseconds
)
{
    struct timespec result;

    result.tv_sec =
        (time_t)(milliseconds / 1000u);

    result.tv_nsec =
        (long)(
            (milliseconds % 1000u)
            * 1000000u
        );

    return result;
}


static void CH_RumbleSetMotorLocked(
    unsigned int port,
    bool on,
    bool hard_stop
)
{
    CH_RumblePortState *state =
        &chRumblePorts[port];

    if (on)
    {
        if (state->motor_on)
            return;

        if (!CH_PadBusControlMotor(
                port,
                PAD_MOTOR_RUMBLE))
        {
            return;
        }

        state->motor_on =
            true;

        return;
    }

    if (!state->motor_on)
        return;

    CH_PadBusControlMotor(
        port,
        hard_stop
            ? PAD_MOTOR_STOP_HARD
            : PAD_MOTOR_STOP
    );

    state->motor_on =
        false;
}


static void CH_RumbleCancelLocked(
    unsigned int port,
    bool hard_stop
)
{
    CH_RumblePortState *state =
        &chRumblePorts[port];

    state->active =
        false;

    state->on_phase =
        false;

    state->pulses_left =
        0;

    CH_RumbleSetMotorLocked(
        port,
        false,
        hard_stop
    );
}


static void CH_RumbleAdvanceLocked(
    unsigned int port,
    uint64_t now
)
{
    CH_RumblePortState *state =
        &chRumblePorts[port];

    if (!state->active ||
        now < state->deadline_ms)
    {
        return;
    }

    if (state->on_phase)
    {
        if (state->pulses_left > 0)
            state->pulses_left--;

        if (state->pulses_left == 0)
        {
            CH_RumbleCancelLocked(
                port,
                state->hard_stop_at_end
            );

            return;
        }

        if (state->off_ms > 0)
        {
            CH_RumbleSetMotorLocked(
                port,
                false,
                false
            );

            state->on_phase =
                false;

            state->deadline_ms =
                now + state->off_ms;
        }
        else
        {
            /*
             * No physical OFF phase:
             * keep one continuous vibration running.
             */
            state->deadline_ms =
                now + state->on_ms;
        }

        return;
    }

    CH_RumbleSetMotorLocked(
        port,
        true,
        false
    );

    state->on_phase =
        true;

    state->deadline_ms =
        now + state->on_ms;
}


static bool CH_RumbleAnyActiveLocked(
    uint64_t *nearest_deadline
)
{
    bool found =
        false;

    uint64_t nearest =
        UINT64_MAX;

    unsigned int port;

    for (port = 0;
         port < CH_RUMBLE_PORT_COUNT;
         ++port)
    {
        CH_RumblePortState *state =
            &chRumblePorts[port];

        if (!state->active)
            continue;

        found =
            true;

        if (state->deadline_ms < nearest)
        {
            nearest =
                state->deadline_ms;
        }
    }

    if (found &&
        nearest_deadline)
    {
        *nearest_deadline =
            nearest;
    }

    return found;
}


static void *CH_RumbleWorker(
    void *unused
)
{
    (void)unused;

    if (LWP_MutexLock(
            chRumbleMutex) != 0)
    {
        return NULL;
    }

    while (!chRumbleStopRequested)
    {
        uint64_t now;
        uint64_t nearest;
        unsigned int port;

        if (!CH_RumbleAnyActiveLocked(
                &nearest))
        {
            LWP_CondWait(
                chRumbleCond,
                chRumbleMutex
            );

            continue;
        }

        now =
            CH_RumbleNowMs();

        for (port = 0;
             port < CH_RUMBLE_PORT_COUNT;
             ++port)
        {
            CH_RumbleAdvanceLocked(
                port,
                now
            );
        }

        if (chRumbleStopRequested)
            break;

        if (!CH_RumbleAnyActiveLocked(
                &nearest))
        {
            continue;
        }

        now =
            CH_RumbleNowMs();

        if (nearest <= now)
            continue;

        {
            struct timespec wait =
                CH_RumbleRelativeTime(
                    nearest - now
                );

            /*
             * Wake either when the next transition is due or when
             * a caller changes the active effect set.
             *
             * Timeout/error return values need no special handling:
             * the loop simply reevaluates current state and time.
             */
            LWP_CondTimedWait(
                chRumbleCond,
                chRumbleMutex,
                &wait
            );
        }
    }

    for (unsigned int port = 0;
         port < CH_RUMBLE_PORT_COUNT;
         ++port)
    {
        CH_RumbleCancelLocked(
            port,
            true
        );
    }

    LWP_MutexUnlock(
        chRumbleMutex
    );

    return NULL;
}


bool CH_RumbleInit(void)
{
    if (chRumbleInitialized)
        return true;

    if (!CH_PadBusInit())
        return false;

    memset(
        chRumblePorts,
        0,
        sizeof(chRumblePorts)
    );

    if (LWP_MutexInit(
            &chRumbleMutex,
            false) != 0)
    {
        return false;
    }

    if (LWP_CondInit(
            &chRumbleCond) != 0)
    {
        LWP_MutexDestroy(
            chRumbleMutex
        );

        return false;
    }

    chRumbleStopRequested =
        false;

    if (LWP_CreateThread(
            &chRumbleThread,
            CH_RumbleWorker,
            NULL,
            NULL,
            0,
            CH_RUMBLE_THREAD_PRIORITY) != 0)
    {
        LWP_CondDestroy(
            chRumbleCond
        );

        LWP_MutexDestroy(
            chRumbleMutex
        );

        return false;
    }

    chRumbleInitialized =
        true;

    return true;
}


bool CH_RumblePulse(
    unsigned int port,
    uint32_t duration_ms
)
{
    CH_RumblePortState *state;
    uint64_t now;

    if (port >= CH_RUMBLE_PORT_COUNT ||
        duration_ms == 0 ||
        !CH_RumbleInit())
    {
        return false;
    }

    if (LWP_MutexLock(
            chRumbleMutex) != 0)
    {
        return false;
    }

    if (chRumbleStopRequested)
    {
        LWP_MutexUnlock(
            chRumbleMutex
        );

        return false;
    }

    state =
        &chRumblePorts[port];

    now =
        CH_RumbleNowMs();

    /*
     * Preserve the proven single-pulse merge behavior:
     * shorter requests do not truncate; longer requests extend
     * from the current moment.
     */
    if (state->active &&
        state->on_phase &&
        state->pulses_left == 1 &&
        state->off_ms == 0)
    {
        uint64_t requested_deadline =
            now + duration_ms;

        if (requested_deadline >
            state->deadline_ms)
        {
            state->deadline_ms =
                requested_deadline;
        }

        CH_RumbleSetMotorLocked(
            port,
            true,
            false
        );

        LWP_CondSignal(
            chRumbleCond
        );

        LWP_MutexUnlock(
            chRumbleMutex
        );

        return true;
    }

    state->on_ms =
        duration_ms;

    state->off_ms =
        0;

    state->pulses_left =
        1;

    state->hard_stop_at_end =
        true;

    state->active =
        true;

    state->on_phase =
        true;

    state->deadline_ms =
        now + duration_ms;

    CH_RumbleSetMotorLocked(
        port,
        true,
        false
    );

    LWP_CondSignal(
        chRumbleCond
    );

    LWP_MutexUnlock(
        chRumbleMutex
    );

    return true;
}


bool CH_RumblePattern(
    unsigned int port,
    uint32_t on_ms,
    uint32_t off_ms,
    unsigned int pulses,
    bool hard_stop
)
{
    CH_RumblePortState *state;
    uint64_t now;

    if (port >= CH_RUMBLE_PORT_COUNT ||
        on_ms == 0 ||
        pulses == 0 ||
        !CH_RumbleInit())
    {
        return false;
    }

    if (LWP_MutexLock(
            chRumbleMutex) != 0)
    {
        return false;
    }

    if (chRumbleStopRequested)
    {
        LWP_MutexUnlock(
            chRumbleMutex
        );

        return false;
    }

    state =
        &chRumblePorts[port];

    now =
        CH_RumbleNowMs();

    state->on_ms =
        on_ms;

    state->off_ms =
        off_ms;

    state->pulses_left =
        pulses;

    state->hard_stop_at_end =
        hard_stop;

    state->active =
        true;

    state->on_phase =
        true;

    state->deadline_ms =
        now + on_ms;

    CH_RumbleSetMotorLocked(
        port,
        true,
        false
    );

    LWP_CondSignal(
        chRumbleCond
    );

    LWP_MutexUnlock(
        chRumbleMutex
    );

    return true;
}


void CH_RumbleStop(
    unsigned int port,
    bool hard_stop
)
{
    if (port >= CH_RUMBLE_PORT_COUNT ||
        !chRumbleInitialized)
    {
        return;
    }

    if (LWP_MutexLock(
            chRumbleMutex) != 0)
    {
        return;
    }

    CH_RumbleCancelLocked(
        port,
        hard_stop
    );

    LWP_CondSignal(
        chRumbleCond
    );

    LWP_MutexUnlock(
        chRumbleMutex
    );
}


void CH_RumbleShutdown(void)
{
    lwp_t thread;

    if (!chRumbleInitialized)
        return;

    if (LWP_MutexLock(
            chRumbleMutex) != 0)
    {
        return;
    }

    chRumbleStopRequested =
        true;

    for (unsigned int port = 0;
         port < CH_RUMBLE_PORT_COUNT;
         ++port)
    {
        CH_RumbleCancelLocked(
            port,
            true
        );
    }

    LWP_CondBroadcast(
        chRumbleCond
    );

    thread =
        chRumbleThread;

    LWP_MutexUnlock(
        chRumbleMutex
    );

    LWP_JoinThread(
        thread,
        NULL
    );

    LWP_CondDestroy(
        chRumbleCond
    );

    LWP_MutexDestroy(
        chRumbleMutex
    );

    memset(
        chRumblePorts,
        0,
        sizeof(chRumblePorts)
    );

    chRumbleInitialized =
        false;

    chRumbleStopRequested =
        false;

    chRumbleThread =
        LWP_THREAD_NULL;
}
