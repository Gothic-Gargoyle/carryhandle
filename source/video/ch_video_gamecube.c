#include <carryhandle/ch_video.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <gccore.h>
#include <ogc/cache.h>
#include <ogc/system.h>
#include <ogc/video.h>


/*
 * Native GameCube software-framebuffer presenter.
 *
 * CarryHandle owns two external framebuffers (XFBs) and presents
 * software-rendered frames directly to the Video Interface.
 *
 * This deliberately bypasses SDL's OGC texture renderer.
 */


static GXRModeObj *chVideoMode;

static void *chVideoFramebufferRaw[2];
static void *chVideoFramebufferVI[2];

static unsigned int chVideoDrawBuffer;

static unsigned int chVideoWidth;
static unsigned int chVideoHeight;

static uint8_t chVideoPaletteY[256];
static uint8_t chVideoPaletteU[256];
static uint8_t chVideoPaletteV[256];


/*
 * CH_VIDEO_PACKED_1TO1
 *
 * Pre-packed GameCube XFB palette entry:
 *
 *     byte 0 = Y
 *     byte 1 = U
 *     byte 2 = Y
 *     byte 3 = V
 *
 * The duplicated Y lets the exact-size indexed presenter
 * produce one Y0 U Y1 V output word from two palette loads
 * and one 32-bit XFB store.
 *
 * 256 entries = 1024 bytes.
 */
static uint32_t chVideoPalettePacked[256];

static bool chVideoPaletteValid;
static bool chVideoInitialized;


/*
 * Convert RGB888 to BT.601 limited-range YUV.
 *
 * This is the same conversion used by the successful native-XFB
 * Quake2Cube proof.
 */
static void CH_VideoRGBToYUV(
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint8_t *y,
    uint8_t *u,
    uint8_t *v
)
{
    int yy;
    int uu;
    int vv;

    yy =
        (
            66 * (int)r +
            129 * (int)g +
            25 * (int)b +
            4096 +
            128
        ) >> 8;

    uu =
        (
            -38 * (int)r -
            74 * (int)g +
            112 * (int)b +
            32768 +
            128
        ) >> 8;

    vv =
        (
            112 * (int)r -
            94 * (int)g -
            18 * (int)b +
            32768 +
            128
        ) >> 8;

    if (yy < 0)
        yy = 0;
    else if (yy > 255)
        yy = 255;

    if (uu < 0)
        uu = 0;
    else if (uu > 255)
        uu = 255;

    if (vv < 0)
        vv = 0;
    else if (vv > 255)
        vv = 255;

    *y = (uint8_t)yy;
    *u = (uint8_t)uu;
    *v = (uint8_t)vv;
}


static void CH_VideoResetState(void)
{
    chVideoMode = NULL;

    chVideoFramebufferRaw[0] = NULL;
    chVideoFramebufferRaw[1] = NULL;

    chVideoFramebufferVI[0] = NULL;
    chVideoFramebufferVI[1] = NULL;

    chVideoDrawBuffer = 0;

    chVideoWidth = 0;
    chVideoHeight = 0;

    chVideoPaletteValid = false;
    chVideoInitialized = false;
}


static bool CH_VideoCanPresent(void)
{
    return
        chVideoInitialized &&
        chVideoMode &&
        chVideoFramebufferRaw[0] &&
        chVideoFramebufferRaw[1] &&
        chVideoFramebufferVI[0] &&
        chVideoFramebufferVI[1] &&
        chVideoWidth > 0 &&
        chVideoHeight > 0;
}


static bool CH_VideoValidateOutputMode(void)
{
    if (!chVideoMode)
        return false;

    if (chVideoMode->fbWidth == 0 ||
        chVideoMode->xfbHeight == 0)
    {
        return false;
    }

    /*
     * GameCube XFB pixels are packed in pairs:
     *
     *     Y0 U Y1 V
     *
     * so an even output width is required.
     */
    if ((chVideoMode->fbWidth & 1u) != 0u)
        return false;

    return true;
}


static void CH_VideoPresentBuffer(void)
{
    VIDEO_SetNextFramebuffer(
        chVideoFramebufferVI[chVideoDrawBuffer]
    );

    VIDEO_Flush();

    /*
     * Wait until the next retrace before reusing the other XFB.
     *
     * This gives us ordinary double-buffered presentation instead
     * of writing into the framebuffer currently being scanned out.
     */
    VIDEO_WaitVSync();

    chVideoDrawBuffer ^= 1u;
}


bool CH_VideoInit(
    unsigned int frame_width,
    unsigned int frame_height,
    const char *title
)
{
    u32 framebufferSize;

    (void)title;

    if (frame_width == 0 ||
        frame_height == 0)
    {
        return false;
    }

    if (CH_VideoCanPresent() &&
        chVideoWidth == frame_width &&
        chVideoHeight == frame_height)
    {
        return true;
    }

    CH_VideoShutdown();

    /*
     * CarryHandle owns GameCube video setup for this backend.
     */
    VIDEO_Init();

    chVideoMode =
        VIDEO_GetPreferredMode(NULL);

    if (!CH_VideoValidateOutputMode())
        goto fail;

    framebufferSize =
        VIDEO_GetFrameBufferSize(
            chVideoMode
        );

    if (framebufferSize == 0)
        goto fail;

    /*
     * SYS_AllocateFramebuffer returns a cache-line-aligned CPU
     * address. Keep that cached address for conversion writes and
     * give VI the uncached alias, following normal libogc usage.
     */
    chVideoFramebufferRaw[0] =
        SYS_AllocateFramebuffer(
            chVideoMode
        );

    chVideoFramebufferRaw[1] =
        SYS_AllocateFramebuffer(
            chVideoMode
        );

    if (!chVideoFramebufferRaw[0] ||
        !chVideoFramebufferRaw[1])
    {
        goto fail;
    }

    chVideoFramebufferVI[0] =
        MEM_K0_TO_K1(
            chVideoFramebufferRaw[0]
        );

    chVideoFramebufferVI[1] =
        MEM_K0_TO_K1(
            chVideoFramebufferRaw[1]
        );

    VIDEO_ClearFrameBuffer(
        chVideoMode,
        chVideoFramebufferVI[0],
        COLOR_BLACK
    );

    VIDEO_ClearFrameBuffer(
        chVideoMode,
        chVideoFramebufferVI[1],
        COLOR_BLACK
    );

    chVideoWidth =
        frame_width;

    chVideoHeight =
        frame_height;

    /*
     * Buffer 0 begins on screen.
     * Buffer 1 is our first draw target.
     */
    chVideoDrawBuffer = 1u;

    VIDEO_Configure(
        chVideoMode
    );

    VIDEO_SetNextFramebuffer(
        chVideoFramebufferVI[0]
    );

    VIDEO_SetBlack(false);

    VIDEO_Flush();
    VIDEO_WaitForFlush();
    VIDEO_WaitVSync();

    /*
     * This mirrors the standard libogc startup sequence for
     * non-interlaced modes.
     */
    if (chVideoMode->viTVMode &
        VI_NON_INTERLACE)
    {
        VIDEO_WaitVSync();
    }

    chVideoInitialized = true;

    return true;


fail:
    CH_VideoShutdown();
    return false;
}


bool CH_VideoSetPaletteRGB8(
    const uint8_t *palette,
    unsigned int entry_stride
)
{
    unsigned int i;

    if (!palette ||
        entry_stride < 3u)
    {
        return false;
    }

    for (i = 0u;
         i < 256u;
         ++i)
    {
        const uint8_t *entry =
            palette +
            (size_t)i * entry_stride;

        CH_VideoRGBToYUV(
            entry[0],
            entry[1],
            entry[2],
            &chVideoPaletteY[i],
            &chVideoPaletteU[i],
            &chVideoPaletteV[i]
        );

        chVideoPalettePacked[i] =
            (
                (uint32_t)
                    chVideoPaletteY[i] << 24
            ) |
            (
                (uint32_t)
                    chVideoPaletteU[i] << 16
            ) |
            (
                (uint32_t)
                    chVideoPaletteY[i] << 8
            ) |
            (uint32_t)
                chVideoPaletteV[i];
    }

    chVideoPaletteValid = true;

    return true;
}


bool CH_VideoPresentIndexed8(
    const uint8_t *pixels,
    unsigned int pitch
)
{
    uint8_t *dstBase;

    unsigned int dstWidth;
    unsigned int dstHeight;

    unsigned int dy;

    u32 framebufferSize;

    if (!pixels ||
        !CH_VideoCanPresent() ||
        !chVideoPaletteValid ||
        pitch < chVideoWidth)
    {
        return false;
    }

    dstWidth =
        (unsigned int)chVideoMode->fbWidth;

    dstHeight =
        (unsigned int)chVideoMode->xfbHeight;

    dstBase =
        (uint8_t *)
        chVideoFramebufferRaw[
            chVideoDrawBuffer
        ];

    /*
     * Scale directly while converting indexed8 -> YUYV.
     *
     * No RGB framebuffer.
     * No texture.
     * No EFB.
     * No GX copy.
     *
     * For Quake2Cube's 640x480 source this degenerates to a
     * straight 1:1 conversion.
     */
    /*
     * Exact-size indexed8 fast path.
     *
     * The generic scaling converter remains completely intact
     * below for mismatched source/output dimensions.
     */
    if (
        dstWidth == chVideoWidth &&
        dstHeight == chVideoHeight
    )
    {
        for (dy = 0u;
             dy < dstHeight;
             ++dy)
        {
            const uint8_t *src;
            uint32_t *dst;

            unsigned int pairs;

            src =
                pixels +
                (size_t)dy * pitch;

            dst =
                (uint32_t *)
                (
                    dstBase +
                    (size_t)dy *
                    (size_t)dstWidth *
                    VI_DISPLAY_PIX_SZ
                );

            pairs =
                dstWidth >> 1;

            while (pairs-- > 0u)
            {
                uint32_t p0;
                uint32_t p1;
                uint32_t uv;

                p0 =
                    chVideoPalettePacked[
                        *src++
                    ];

                p1 =
                    chVideoPalettePacked[
                        *src++
                    ];

                /*
                 * GameCube is big-endian.
                 *
                 * U and V occupy independent 16-bit lanes
                 * inside 0x00ff00ff, so both rounded averages
                 * can be calculated in parallel without
                 * cross-channel carry.
                 */
                uv =
                    (
                        (
                            (
                                p0 &
                                0x00ff00ffu
                            ) +
                            (
                                p1 &
                                0x00ff00ffu
                            ) +
                            0x00010001u
                        ) >> 1
                    ) &
                    0x00ff00ffu;

                *dst++ =
                    (
                        p0 &
                        0xff000000u
                    ) |
                    (
                        p1 &
                        0x0000ff00u
                    ) |
                    uv;
            }
        }
    }
    else
    {
    for (dy = 0u;
         dy < dstHeight;
         ++dy)
    {
        unsigned int sy;

        const uint8_t *src;

        uint8_t *dst;

        unsigned int dx;

        if (dstHeight == chVideoHeight)
        {
            sy = dy;
        }
        else
        {
            sy =
                (unsigned int)(
                    (
                        (uint64_t)dy *
                        (uint64_t)chVideoHeight
                    ) /
                    (uint64_t)dstHeight
                );

            if (sy >= chVideoHeight)
                sy = chVideoHeight - 1u;
        }

        src =
            pixels +
            (size_t)sy * pitch;

        dst =
            dstBase +
            (size_t)dy *
            (size_t)dstWidth *
            VI_DISPLAY_PIX_SZ;

        for (dx = 0u;
             dx < dstWidth;
             dx += 2u)
        {
            unsigned int sx0;
            unsigned int sx1;

            uint8_t i0;
            uint8_t i1;

            unsigned int u;
            unsigned int v;

            if (dstWidth == chVideoWidth)
            {
                sx0 = dx;
                sx1 = dx + 1u;
            }
            else
            {
                sx0 =
                    (unsigned int)(
                        (
                            (uint64_t)dx *
                            (uint64_t)chVideoWidth
                        ) /
                        (uint64_t)dstWidth
                    );

                sx1 =
                    (unsigned int)(
                        (
                            (uint64_t)(dx + 1u) *
                            (uint64_t)chVideoWidth
                        ) /
                        (uint64_t)dstWidth
                    );

                if (sx0 >= chVideoWidth)
                    sx0 = chVideoWidth - 1u;

                if (sx1 >= chVideoWidth)
                    sx1 = chVideoWidth - 1u;
            }

            i0 = src[sx0];
            i1 = src[sx1];

            u =
                (
                    (unsigned int)
                        chVideoPaletteU[i0] +
                    (unsigned int)
                        chVideoPaletteU[i1] +
                    1u
                ) >> 1;

            v =
                (
                    (unsigned int)
                        chVideoPaletteV[i0] +
                    (unsigned int)
                        chVideoPaletteV[i1] +
                    1u
                ) >> 1;

            /*
             * GameCube XFB:
             *
             *     Y0 U Y1 V
             */
            dst[dx * 2u + 0u] =
                chVideoPaletteY[i0];

            dst[dx * 2u + 1u] =
                (uint8_t)u;

            dst[dx * 2u + 2u] =
                chVideoPaletteY[i1];

            dst[dx * 2u + 3u] =
                (uint8_t)v;
        }
    }
    }

    framebufferSize =
        VIDEO_GetFrameBufferSize(
            chVideoMode
        );

    /*
     * CPU wrote via the cached K0 mapping.
     * Push the finished XFB to memory before VI sees it.
     */
    DCStoreRange(
        chVideoFramebufferRaw[
            chVideoDrawBuffer
        ],
        framebufferSize
    );

    CH_VideoPresentBuffer();

    return true;
}


bool CH_VideoPresentRGB32(
    const uint32_t *pixels,
    unsigned int pitch
)
{
    uint8_t *dstBase;

    unsigned int dstWidth;
    unsigned int dstHeight;

    unsigned int dy;

    u32 framebufferSize;

    if (!pixels ||
        !CH_VideoCanPresent() ||
        pitch <
            chVideoWidth *
            sizeof(uint32_t))
    {
        return false;
    }

    dstWidth =
        (unsigned int)chVideoMode->fbWidth;

    dstHeight =
        (unsigned int)chVideoMode->xfbHeight;

    dstBase =
        (uint8_t *)
        chVideoFramebufferRaw[
            chVideoDrawBuffer
        ];

    /*
     * CH_VideoPresentRGB32 historically fed SDL_PIXELFORMAT_RGB888.
     * Treat source words as:
     *
     *     0x00RRGGBB
     */
    for (dy = 0u;
         dy < dstHeight;
         ++dy)
    {
        unsigned int sy;

        const uint32_t *src;

        uint8_t *dst;

        unsigned int dx;

        sy =
            (unsigned int)(
                (
                    (uint64_t)dy *
                    (uint64_t)chVideoHeight
                ) /
                (uint64_t)dstHeight
            );

        if (sy >= chVideoHeight)
            sy = chVideoHeight - 1u;

        src =
            (const uint32_t *)
            (
                (const uint8_t *)pixels +
                (size_t)sy * pitch
            );

        dst =
            dstBase +
            (size_t)dy *
            (size_t)dstWidth *
            VI_DISPLAY_PIX_SZ;

        for (dx = 0u;
             dx < dstWidth;
             dx += 2u)
        {
            unsigned int sx0;
            unsigned int sx1;

            uint32_t p0;
            uint32_t p1;

            uint8_t y0;
            uint8_t u0;
            uint8_t v0;

            uint8_t y1;
            uint8_t u1;
            uint8_t v1;

            unsigned int u;
            unsigned int v;

            sx0 =
                (unsigned int)(
                    (
                        (uint64_t)dx *
                        (uint64_t)chVideoWidth
                    ) /
                    (uint64_t)dstWidth
                );

            sx1 =
                (unsigned int)(
                    (
                        (uint64_t)(dx + 1u) *
                        (uint64_t)chVideoWidth
                    ) /
                    (uint64_t)dstWidth
                );

            if (sx0 >= chVideoWidth)
                sx0 = chVideoWidth - 1u;

            if (sx1 >= chVideoWidth)
                sx1 = chVideoWidth - 1u;

            p0 = src[sx0];
            p1 = src[sx1];

            CH_VideoRGBToYUV(
                (uint8_t)((p0 >> 16) & 0xffu),
                (uint8_t)((p0 >> 8) & 0xffu),
                (uint8_t)(p0 & 0xffu),
                &y0,
                &u0,
                &v0
            );

            CH_VideoRGBToYUV(
                (uint8_t)((p1 >> 16) & 0xffu),
                (uint8_t)((p1 >> 8) & 0xffu),
                (uint8_t)(p1 & 0xffu),
                &y1,
                &u1,
                &v1
            );

            u =
                (
                    (unsigned int)u0 +
                    (unsigned int)u1 +
                    1u
                ) >> 1;

            v =
                (
                    (unsigned int)v0 +
                    (unsigned int)v1 +
                    1u
                ) >> 1;

            dst[dx * 2u + 0u] = y0;
            dst[dx * 2u + 1u] = (uint8_t)u;
            dst[dx * 2u + 2u] = y1;
            dst[dx * 2u + 3u] = (uint8_t)v;
        }
    }

    framebufferSize =
        VIDEO_GetFrameBufferSize(
            chVideoMode
        );

    DCStoreRange(
        chVideoFramebufferRaw[
            chVideoDrawBuffer
        ],
        framebufferSize
    );

    CH_VideoPresentBuffer();

    return true;
}


void CH_VideoShutdown(void)
{
    if (chVideoInitialized)
    {
        /*
         * Stop displaying application framebuffer contents before
         * releasing their allocations.
         */
        VIDEO_SetBlack(true);
        VIDEO_Flush();
        VIDEO_WaitForFlush();
        VIDEO_WaitVSync();
    }

    if (chVideoFramebufferRaw[0])
    {
        free(
            chVideoFramebufferRaw[0]
        );
    }

    if (chVideoFramebufferRaw[1])
    {
        free(
            chVideoFramebufferRaw[1]
        );
    }

    CH_VideoResetState();
}
