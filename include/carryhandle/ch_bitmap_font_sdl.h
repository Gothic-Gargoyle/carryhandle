#ifndef CARRYHANDLE_CH_BITMAP_FONT_SDL_H
#define CARRYHANDLE_CH_BITMAP_FONT_SDL_H

#include <SDL2/SDL.h>


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Tiny built-in 5x7 uppercase bitmap font for optional SDL UIs.
 *
 * This is intentionally small and asset-free.  It is suitable for
 * diagnostics, bootstrap screens, examples and simple system-style UI.
 *
 * The renderer's current draw color is used for every painted pixel.
 * The caller therefore controls colour with SDL_SetRenderDrawColor().
 *
 * Supported directly:
 *   A-Z, a-z, 0-9, colon, dash, period and slash.
 * Unsupported characters render as blank cells while preserving advance.
 */
#define CH_BITMAP_FONT_SDL_GLYPH_WIDTH    5
#define CH_BITMAP_FONT_SDL_GLYPH_HEIGHT   7
#define CH_BITMAP_FONT_SDL_GLYPH_ADVANCE  6


/*
 * Return the rendered horizontal advance of text at one integer scale.
 *
 * Returns 0 for NULL text or a non-positive scale.
 */
int CH_BitmapFontSDLTextWidth(
    const char *text,
    int scale
);


/*
 * Draw one text string at x/y.
 *
 * This function does not change renderer colour, present the frame,
 * poll input or delay.
 */
void CH_BitmapFontSDLDrawText(
    SDL_Renderer *renderer,
    int x,
    int y,
    const char *text,
    int scale
);


/*
 * Convenience helper that horizontally centers text inside canvas_width.
 */
void CH_BitmapFontSDLDrawCenteredText(
    SDL_Renderer *renderer,
    int canvas_width,
    int y,
    const char *text,
    int scale
);


#ifdef __cplusplus
}
#endif

#endif
