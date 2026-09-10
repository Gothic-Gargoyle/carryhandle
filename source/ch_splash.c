#include <carryhandle/ch_splash.h>

#include <errno.h>
#include <time.h>

#include <carryhandle/ch_time.h>


static void CH_SplashSleepMilliseconds(
    uint32_t milliseconds
)
{
    struct timespec request;
    struct timespec remaining;

    if (milliseconds == 0u)
        return;

    request.tv_sec =
        (time_t)(milliseconds / 1000u);

    request.tv_nsec =
        (long)(
            (milliseconds % 1000u)
            * 1000000u
        );

    while (nanosleep(
               &request,
               &remaining) != 0)
    {
        if (errno != EINTR)
            return;

        request =
            remaining;
    }
}


static bool CH_SplashPresent(
    const CH_SplashSequence *sequence,
    const CH_SplashScreen *screen,
    size_t screen_index,
    uint8_t alpha
)
{
    return sequence->frame(
        sequence->userdata,
        screen_index,
        screen->screen_data,
        alpha
    );
}


static bool CH_SplashRunFade(
    const CH_SplashSequence *sequence,
    const CH_SplashScreen *screen,
    size_t screen_index,
    uint32_t duration_ms,
    bool fade_in,
    uint32_t frame_interval_ms
)
{
    uint64_t start;

    if (duration_ms == 0u)
    {
        return CH_SplashPresent(
            sequence,
            screen,
            screen_index,
            fade_in ? 255u : 0u
        );
    }

    start =
        CH_TimeMilliseconds();

    for (;;)
    {
        uint64_t now =
            CH_TimeMilliseconds();

        uint64_t elapsed =
            now - start;

        uint8_t alpha;

        if (elapsed >= duration_ms)
            break;

        if (fade_in)
        {
            alpha =
                (uint8_t)(
                    elapsed * 255u
                    / duration_ms
                );
        }
        else
        {
            alpha =
                (uint8_t)(
                    255u
                    - elapsed * 255u
                    / duration_ms
                );
        }

        if (!CH_SplashPresent(
                sequence,
                screen,
                screen_index,
                alpha))
        {
            return false;
        }

        CH_SplashSleepMilliseconds(
            frame_interval_ms
        );
    }

    return CH_SplashPresent(
        sequence,
        screen,
        screen_index,
        fade_in ? 255u : 0u
    );
}


bool CH_SplashSequenceRun(
    const CH_SplashSequence *sequence,
    const CH_SplashScreen *screens,
    size_t screen_count
)
{
    uint32_t frame_interval_ms;
    size_t i;

    if (sequence == NULL)
        return false;

    if (screen_count == 0u)
        return true;

    if (sequence->frame == NULL ||
        screens == NULL)
    {
        return false;
    }

    frame_interval_ms =
        sequence->frame_interval_ms;

    if (frame_interval_ms == 0u)
    {
        frame_interval_ms =
            CH_SPLASH_DEFAULT_FRAME_INTERVAL_MS;
    }

    for (i = 0u; i < screen_count; ++i)
    {
        const CH_SplashScreen *screen =
            &screens[i];

        if (!CH_SplashRunFade(
                sequence,
                screen,
                i,
                screen->fade_in_ms,
                true,
                frame_interval_ms))
        {
            return false;
        }

        CH_SplashSleepMilliseconds(
            screen->hold_ms
        );

        if (!CH_SplashRunFade(
                sequence,
                screen,
                i,
                screen->fade_out_ms,
                false,
                frame_interval_ms))
        {
            return false;
        }
    }

    return true;
}
