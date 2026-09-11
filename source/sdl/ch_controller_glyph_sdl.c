#include <carryhandle/ch_controller_glyph_sdl.h>

#include <SDL2/SDL_image.h>

#include <stdio.h>
#include <string.h>

#define CH_CONTROLLER_GLYPH_PATH_MAX 512

static int CH_ControllerGlyphSDLHasSuffix(
    const char *path,
    const char *suffix)
{
    size_t path_length;
    size_t suffix_length;

    if (path == NULL || suffix == NULL)
        return 0;

    path_length = strlen(path);
    suffix_length = strlen(suffix);

    if (suffix_length > path_length)
        return 0;

    return strcmp(
               path + path_length - suffix_length,
               suffix) == 0;
}



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
    SDL_Surface *scaled_surface;
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

    if (CH_ControllerGlyphSDLHasSuffix(
            asset_path,
            ".svg"))
    {
        surface =
            IMG_LoadSizedSVG_RW(
                rw,
                size,
                size);
    }
    else
    {
        /*
         * Pre-rasterized GameCube controller glyphs use SDL's built-in
         * BMP decoder, bypassing both NanoSVG and SDL2_image's generic
         * multi-codec dispatcher.
         */
        surface =
            SDL_LoadBMP_RW(
                rw,
                0);
    }

    SDL_RWclose(rw);

    if (surface == NULL)
        return NULL;

    if (surface->w != size ||
        surface->h != size)
    {
        scaled_surface =
            SDL_CreateRGBSurfaceWithFormat(
                0,
                size,
                size,
                32,
                SDL_PIXELFORMAT_RGBA32);

        if (scaled_surface == NULL)
        {
            SDL_FreeSurface(surface);
            return NULL;
        }

        /*
         * Copy source RGBA into the transparent destination instead of
         * alpha-blending it over the destination during scaling.
         */
        if (SDL_SetSurfaceBlendMode(
                surface,
                SDL_BLENDMODE_NONE) != 0)
        {
            SDL_FreeSurface(surface);
            SDL_FreeSurface(scaled_surface);
            return NULL;
        }

        if (SDL_BlitScaled(
                surface,
                NULL,
                scaled_surface,
                NULL) != 0)
        {
            SDL_FreeSurface(surface);
            SDL_FreeSurface(scaled_surface);
            return NULL;
        }

        SDL_FreeSurface(surface);
        surface = scaled_surface;
    }

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
