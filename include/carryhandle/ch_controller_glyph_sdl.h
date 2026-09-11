#ifndef CARRYHANDLE_CH_CONTROLLER_GLYPH_SDL_H
#define CARRYHANDLE_CH_CONTROLLER_GLYPH_SDL_H

#include <SDL2/SDL.h>

#include <carryhandle/ch_controller_glyph.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Load one controller glyph as an SDL texture.
 *
 * asset_root is prepended to CH_ControllerGlyphAssetPath(). For a native
 * GameCube image whose FST mirrors CarryHandle's assets tree, pass "dvd:/".
 *
 * SVG assets are rasterized at size x size by SDL2_image/NanoSVG.
 * Pre-rasterized bitmap controller prompts are decoded with SDL's built-in
 * BMP loader and scaled to size x size before texture creation.
 *
 * Original colour and alpha are preserved. The returned texture uses
 * SDL_BLENDMODE_BLEND and is owned by the caller.
 *
 * Returns NULL and sets the SDL error string on failure.
 */
SDL_Texture *CH_ControllerGlyphSDLLoadTexture(
    SDL_Renderer *renderer,
    const char *asset_root,
    CH_ControllerGlyph glyph,
    int size);

#ifdef __cplusplus
}
#endif

#endif
