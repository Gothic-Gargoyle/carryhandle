#ifndef CARRYHANDLE_CH_SPLASH_H
#define CARRYHANDLE_CH_SPLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Generic renderer-neutral startup splash sequencing.
 *
 * CarryHandle owns only sequence and timing semantics. Applications remain
 * responsible for loading artwork and presenting each frame.
 *
 * A sequence may contain any number of screens, including zero. Each screen
 * independently defines:
 *
 *     fade in -> hold -> fade out
 *
 * The application callback receives the screen index, its opaque screen_data,
 * and an alpha value from 0 through 255.
 *
 * This module deliberately has no knowledge of SDL, artwork formats,
 * application identity, Memory Cards, or what startup stage follows it.
 */

#define CH_SPLASH_DEFAULT_FRAME_INTERVAL_MS 16u


typedef struct CH_SplashScreen
{
    const void *screen_data;

    uint32_t fade_in_ms;
    uint32_t hold_ms;
    uint32_t fade_out_ms;
} CH_SplashScreen;


/*
 * Present one frame for one splash screen.
 *
 * userdata
 *     Sequence-wide application context, commonly a renderer/presenter.
 *
 * screen_index
 *     Zero-based index within the sequence.
 *
 * screen_data
 *     Opaque per-screen pointer copied from CH_SplashScreen.
 *
 * alpha
 *     Requested visibility from fully transparent (0) to fully opaque (255).
 *
 * Return false to abort the sequence.
 */
typedef bool (*CH_SplashFrameFn)(
    void *userdata,
    size_t screen_index,
    const void *screen_data,
    uint8_t alpha
);


typedef struct CH_SplashSequence
{
    CH_SplashFrameFn frame;
    void *userdata;

    /*
     * Delay between animation frames.
     *
     * Zero selects CH_SPLASH_DEFAULT_FRAME_INTERVAL_MS.
     */
    uint32_t frame_interval_ms;
} CH_SplashSequence;


/*
 * Run screens[0..screen_count-1] in order.
 *
 * screen_count == 0 is a successful no-op; screens may be NULL in that case.
 *
 * For a non-empty sequence, sequence, sequence->frame and screens must all
 * be non-NULL.
 *
 * A zero-duration fade is still presented at its terminal alpha:
 *     fade-in  0 ms -> one frame at alpha 255
 *     fade-out 0 ms -> one frame at alpha 0
 *
 * Returns false on invalid input or when the application frame callback
 * reports failure.
 */
bool CH_SplashSequenceRun(
    const CH_SplashSequence *sequence,
    const CH_SplashScreen *screens,
    size_t screen_count
);


#ifdef __cplusplus
}
#endif

#endif
