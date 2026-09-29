#define SDL_MAIN_HANDLED

#ifdef HW_DOL
#include <ogc/gx.h>
#endif
#include <carryhandle/ch_video.h>

#include <SDL2/SDL.h>
#define CH_VIDEO_CONVERT_ROWS 16u


static SDL_Window *chVideoWindow;
static SDL_Renderer *chVideoRenderer;
static SDL_Texture *chVideoTexture;
static SDL_PixelFormat *chVideoPixelFormat;

static uint32_t *chVideoConvertBuffer;

static uint32_t chVideoPalette[256];



static unsigned int chVideoWidth;
static unsigned int chVideoHeight;

static bool chVideoPaletteValid;




static void CH_VideoDestroyResources(void)
{
    if (chVideoConvertBuffer)
    {
        SDL_free(chVideoConvertBuffer);
        chVideoConvertBuffer = NULL;
    }

    if (chVideoTexture)
    {
        SDL_DestroyTexture(chVideoTexture);
        chVideoTexture = NULL;
    }

    if (chVideoRenderer)
    {
        SDL_DestroyRenderer(chVideoRenderer);
        chVideoRenderer = NULL;
    }

    if (chVideoWindow)
    {
        SDL_DestroyWindow(chVideoWindow);
        chVideoWindow = NULL;
    }

    if (chVideoPixelFormat)
    {
        SDL_FreeFormat(chVideoPixelFormat);
        chVideoPixelFormat = NULL;
    }

    chVideoWidth = 0;
    chVideoHeight = 0;
    chVideoPaletteValid = false;
}


static bool CH_VideoRender(void)
{
    if (SDL_RenderClear(chVideoRenderer) < 0)
        return false;

    if (SDL_RenderCopy(
            chVideoRenderer,
            chVideoTexture,
            NULL,
            NULL) < 0)
    {
        return false;
    }


    SDL_RenderPresent(chVideoRenderer);


    return true;
}


bool CH_VideoInit(
    unsigned int frame_width,
    unsigned int frame_height,
    const char *title
)
{
    if (frame_width == 0 ||
        frame_height == 0)
    {
        return false;
    }

    if (chVideoWindow &&
        chVideoRenderer &&
        chVideoTexture &&
        chVideoWidth == frame_width &&
        chVideoHeight == frame_height)
    {
        return true;
    }

    CH_VideoDestroyResources();

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
        return false;

    chVideoPixelFormat =
        SDL_AllocFormat(SDL_PIXELFORMAT_RGB888);

    if (!chVideoPixelFormat)
        goto fail;

    chVideoWindow = SDL_CreateWindow(
        title ? title : "CarryHandle",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640,
        480,
        SDL_WINDOW_SHOWN
    );

    if (!chVideoWindow)
        goto fail;

    chVideoRenderer = SDL_CreateRenderer(
        chVideoWindow,
        -1,
        0
    );

    if (!chVideoRenderer)
        goto fail;

    chVideoTexture = SDL_CreateTexture(
        chVideoRenderer,
        SDL_PIXELFORMAT_RGB888,
        SDL_TEXTUREACCESS_STREAMING,
        (int)frame_width,
        (int)frame_height
    );

    if (!chVideoTexture)
        goto fail;

    chVideoConvertBuffer = SDL_malloc(
        (size_t)frame_width *
        CH_VIDEO_CONVERT_ROWS *
        sizeof(uint32_t)
    );

    if (!chVideoConvertBuffer)
        goto fail;

    chVideoWidth = frame_width;
    chVideoHeight = frame_height;

    SDL_RenderClear(chVideoRenderer);
    SDL_RenderPresent(chVideoRenderer);


    return true;

fail:
    CH_VideoDestroyResources();
    return false;
}


bool CH_VideoPresentRGB32(
    const uint32_t *pixels,
    unsigned int pitch
)
{
    if (!pixels ||
        !chVideoRenderer ||
        !chVideoTexture)
    {
        return false;
    }

    if (SDL_UpdateTexture(
            chVideoTexture,
            NULL,
            pixels,
            (int)pitch) < 0)
    {
        return false;
    }

    return CH_VideoRender();
}


bool CH_VideoSetPaletteRGB8(
    const uint8_t *palette,
    unsigned int entry_stride
)
{
    unsigned int i;

    if (!palette ||
        !chVideoPixelFormat ||
        entry_stride < 3)
    {
        return false;
    }

    for (i = 0;
         i < 256;
         ++i)
    {
        const uint8_t *entry =
            palette + i * entry_stride;

        chVideoPalette[i] =
            SDL_MapRGB(
                chVideoPixelFormat,
                entry[0],
                entry[1],
                entry[2]
            );


    }

    chVideoPaletteValid = true;

    return true;
}


bool CH_VideoPresentIndexed8(
    const uint8_t *pixels,
    unsigned int pitch
)
{


    unsigned int y;

    if (!pixels ||
        !chVideoRenderer ||
        !chVideoTexture ||
        !chVideoConvertBuffer ||
        !chVideoPaletteValid)
    {
        return false;
    }

    /*
     * Avoid direct streaming-texture locking on the GameCube backend.
     *
     * Convert a small band of indexed pixels into RGB32 and upload that
     * band with SDL_UpdateTexture(). This follows the DoomCube-proven
     * SDL presentation API without requiring another full RGB framebuffer.
     */
    for (y = 0;
         y < chVideoHeight;
         y += CH_VIDEO_CONVERT_ROWS)
    {
        SDL_Rect rect;
        unsigned int rows;
        unsigned int row;

        rows = chVideoHeight - y;

        if (rows > CH_VIDEO_CONVERT_ROWS)
            rows = CH_VIDEO_CONVERT_ROWS;

        for (row = 0;
             row < rows;
             ++row)
        {
            const uint8_t *src =
                pixels +
                (y + row) * pitch;

            uint32_t *dst =
                chVideoConvertBuffer +
                row * chVideoWidth;

            unsigned int x;

            for (x = 0;
                 x < chVideoWidth;
                 ++x)
            {
                dst[x] =
                    chVideoPalette[src[x]];
            }
        }

        rect.x = 0;
        rect.y = (int)y;
        rect.w = (int)chVideoWidth;
        rect.h = (int)rows;

        if (SDL_UpdateTexture(
                chVideoTexture,
                &rect,
                chVideoConvertBuffer,
                (int)(chVideoWidth * sizeof(uint32_t))) < 0)
        {
            return false;
        }
    }

    return CH_VideoRender();
}


void CH_VideoShutdown(void)
{
    CH_VideoDestroyResources();
}
