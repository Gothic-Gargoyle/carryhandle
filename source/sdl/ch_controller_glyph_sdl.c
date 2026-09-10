#include <carryhandle/ch_controller_glyph_sdl.h>

#include <SDL2/SDL_image.h>

#include <stdio.h>
#include <string.h>

#define CH_CONTROLLER_GLYPH_PATH_MAX 512

static int CH_ControllerGlyphSDLBuildPath(
    char *buffer,
    size_t buffer_size,
    const char *asset_root,
    const char *asset_path)
{
    size_t root_length;
    int written;

    if (buffer == NULL ||
        buffer_size == 0 ||
        asset_path == NULL)
    {
        return SDL_SetError(
            "CarryHandle controller glyph: invalid path arguments");
    }

    if (asset_root == NULL || asset_root[0] == '\0')
    {
        written =
            snprintf(
                buffer,
                buffer_size,
                "%s",
                asset_path);
    }
    else
    {
        root_length = strlen(asset_root);

        written =
            snprintf(
                buffer,
                buffer_size,
                asset_root[root_length - 1] == '/'
                    ? "%s%s"
                    : "%s/%s",
                asset_root,
                asset_path);
    }

    if (written < 0 ||
        (size_t)written >= buffer_size)
    {
        return SDL_SetError(
            "CarryHandle controller glyph: asset path is too long");
    }

    return 0;
}

SDL_Texture *CH_ControllerGlyphSDLLoadTexture(
    SDL_Renderer *renderer,
    const char *asset_root,
    CH_ControllerGlyph glyph,
    int size)
{
    const char *asset_path;
    char full_path[CH_CONTROLLER_GLYPH_PATH_MAX];
    SDL_RWops *rw;
    SDL_Surface *surface;
    SDL_Texture *texture;

    if (renderer == NULL)
    {
        SDL_SetError(
            "CarryHandle controller glyph: renderer is NULL");
        return NULL;
    }

    if (size <= 0)
    {
        SDL_SetError(
            "CarryHandle controller glyph: size must be positive");
        return NULL;
    }

    asset_path =
        CH_ControllerGlyphAssetPath(glyph);

    if (asset_path == NULL)
    {
        SDL_SetError(
            "CarryHandle controller glyph: no SVG asset for glyph");
        return NULL;
    }

    if (CH_ControllerGlyphSDLBuildPath(
            full_path,
            sizeof(full_path),
            asset_root,
            asset_path) != 0)
    {
        return NULL;
    }

    rw =
        SDL_RWFromFile(
            full_path,
            "rb");

    if (rw == NULL)
        return NULL;

    surface =
        IMG_LoadSizedSVG_RW(
            rw,
            size,
            size);

    SDL_RWclose(rw);

    if (surface == NULL)
        return NULL;

    texture =
        SDL_CreateTextureFromSurface(
            renderer,
            surface);

    SDL_FreeSurface(surface);

    if (texture == NULL)
        return NULL;

    if (SDL_SetTextureBlendMode(
            texture,
            SDL_BLENDMODE_BLEND) != 0)
    {
        SDL_DestroyTexture(texture);
        return NULL;
    }

    return texture;
}
