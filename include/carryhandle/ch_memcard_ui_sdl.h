#ifndef CARRYHANDLE_CH_MEMCARD_UI_SDL_H
#define CARRYHANDLE_CH_MEMCARD_UI_SDL_H

#include <SDL2/SDL.h>

#include <stdbool.h>
#include <stdint.h>

#include <carryhandle/ch_memcard_ui.h>


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Small default SDL presenter for CarryHandle Memory Card screens.
 *
 * This module is optional.  Applications that use another renderer, or
 * applications with their own themed UI, can ignore it completely and
 * implement CH_MemCardUIShowFn themselves.
 */
typedef struct CH_MemCardUISDLPresenter
{
    SDL_Renderer *renderer;

    int width;
    int height;

    uint8_t background_r;
    uint8_t background_g;
    uint8_t background_b;

    uint8_t foreground_r;
    uint8_t foreground_g;
    uint8_t foreground_b;

    uint8_t accent_r;
    uint8_t accent_g;
    uint8_t accent_b;

    const char *glyph_asset_root;
} CH_MemCardUISDLPresenter;


/*
 * Initialize one default presenter.
 *
 * The caller owns renderer and must keep it alive while the presenter is
 * used.  Width/height are logical presentation dimensions.
 */
bool CH_MemCardUISDLInit(
    CH_MemCardUISDLPresenter *presenter,
    SDL_Renderer *renderer,
    int width,
    int height
);


/*
 * Change the three simple RGB roles used by the default presentation.
 */
void CH_MemCardUISDLSetGlyphAssetRoot(
    CH_MemCardUISDLPresenter *presenter,
    const char *asset_root
);


void CH_MemCardUISDLSetColors(
    CH_MemCardUISDLPresenter *presenter,
    uint8_t background_r,
    uint8_t background_g,
    uint8_t background_b,
    uint8_t foreground_r,
    uint8_t foreground_g,
    uint8_t foreground_b,
    uint8_t accent_r,
    uint8_t accent_g,
    uint8_t accent_b
);


/*
 * CH_MemCardUIShowFn-compatible callback.
 *
 * Presents one complete frame and calls SDL_RenderPresent().
 * It does not poll input, delay, or mutate Memory Card state.
 */
bool CH_MemCardUISDLShow(
    void *userdata,
    CH_MemCardUIScreenKind kind,
    const CH_MemCardUIInfo *info
);


/*
 * Convenience adapter for CH_MemCardUIShow().
 */
CH_MemCardUI CH_MemCardUISDLMakeUI(
    CH_MemCardUISDLPresenter *presenter
);


#ifdef __cplusplus
}
#endif

#endif
