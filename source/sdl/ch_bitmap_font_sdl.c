#include <carryhandle/ch_bitmap_font_sdl.h>

#include <stdint.h>
#include <string.h>


static const uint8_t CH_BitmapFontSDLLetters[26][7] =
{
    {14,17,17,31,17,17,17}, /* A */
    {30,17,17,30,17,17,30}, /* B */
    {14,17,16,16,16,17,14}, /* C */
    {30,17,17,17,17,17,30}, /* D */
    {31,16,16,30,16,16,31}, /* E */
    {31,16,16,30,16,16,16}, /* F */
    {14,17,16,23,17,17,15}, /* G */
    {17,17,17,31,17,17,17}, /* H */
    {14,4,4,4,4,4,14},      /* I */
    {7,2,2,2,18,18,12},     /* J */
    {17,18,20,24,20,18,17}, /* K */
    {16,16,16,16,16,16,31}, /* L */
    {17,27,21,21,17,17,17}, /* M */
    {17,25,21,19,17,17,17}, /* N */
    {14,17,17,17,17,17,14}, /* O */
    {30,17,17,30,16,16,16}, /* P */
    {14,17,17,17,21,18,13}, /* Q */
    {30,17,17,30,20,18,17}, /* R */
    {15,16,16,14,1,1,30},   /* S */
    {31,4,4,4,4,4,4},       /* T */
    {17,17,17,17,17,17,14}, /* U */
    {17,17,17,17,17,10,4},  /* V */
    {17,17,17,21,21,21,10}, /* W */
    {17,17,10,4,10,17,17},  /* X */
    {17,17,10,4,4,4,4},     /* Y */
    {31,1,2,4,8,16,31}      /* Z */
};


static const uint8_t CH_BitmapFontSDLDigits[10][7] =
{
    {14,17,19,21,25,17,14}, /* 0 */
    {4,12,4,4,4,4,14},      /* 1 */
    {14,17,1,2,4,8,31},     /* 2 */
    {30,1,1,14,1,1,30},     /* 3 */
    {2,6,10,18,31,2,2},     /* 4 */
    {31,16,16,30,1,1,30},   /* 5 */
    {14,16,16,30,17,17,14}, /* 6 */
    {31,1,2,4,8,8,8},       /* 7 */
    {14,17,17,14,17,17,14}, /* 8 */
    {14,17,17,15,1,1,14}    /* 9 */
};


static const uint8_t CH_BitmapFontSDLBlank[7] =
{
    0,0,0,0,0,0,0
};


static const uint8_t CH_BitmapFontSDLColon[7] =
{
    0,4,4,0,4,4,0
};


static const uint8_t CH_BitmapFontSDLDash[7] =
{
    0,0,0,31,0,0,0
};


static const uint8_t CH_BitmapFontSDLPeriod[7] =
{
    0,0,0,0,0,4,4
};


static const uint8_t CH_BitmapFontSDLSlash[7] =
{
    1,2,2,4,8,8,16
};


static const uint8_t *CH_BitmapFontSDLGlyph(
    char c
)
{
    unsigned char uc =
        (unsigned char)c;

    if (uc >= 'a' && uc <= 'z')
    {
        uc =
            (unsigned char)(
                uc - 'a' + 'A'
            );
    }

    if (uc >= 'A' && uc <= 'Z')
    {
        return CH_BitmapFontSDLLetters[
            uc - 'A'
        ];
    }

    if (uc >= '0' && uc <= '9')
    {
        return CH_BitmapFontSDLDigits[
            uc - '0'
        ];
    }

    switch (uc)
    {
        case ':':
            return CH_BitmapFontSDLColon;

        case '-':
            return CH_BitmapFontSDLDash;

        case '.':
            return CH_BitmapFontSDLPeriod;

        case '/':
            return CH_BitmapFontSDLSlash;

        default:
            return CH_BitmapFontSDLBlank;
    }
}


static void CH_BitmapFontSDLDrawChar(
    SDL_Renderer *renderer,
    int x,
    int y,
    char c,
    int scale
)
{
    const uint8_t *glyph =
        CH_BitmapFontSDLGlyph(c);

    SDL_Rect pixel =
    {
        0,
        0,
        scale,
        scale
    };

    int row;
    int col;

    for (
        row = 0;
        row < CH_BITMAP_FONT_SDL_GLYPH_HEIGHT;
        ++row
    )
    {
        for (
            col = 0;
            col < CH_BITMAP_FONT_SDL_GLYPH_WIDTH;
            ++col
        )
        {
            if (
                (
                    glyph[row]
                    & (1u << (4 - col))
                ) == 0
            )
            {
                continue;
            }

            pixel.x =
                x + col * scale;

            pixel.y =
                y + row * scale;

            SDL_RenderFillRect(
                renderer,
                &pixel
            );
        }
    }
}


int CH_BitmapFontSDLTextWidth(
    const char *text,
    int scale
)
{
    if (text == NULL || scale <= 0)
    {
        return 0;
    }

    return
        (int)strlen(text)
        * CH_BITMAP_FONT_SDL_GLYPH_ADVANCE
        * scale;
}


void CH_BitmapFontSDLDrawText(
    SDL_Renderer *renderer,
    int x,
    int y,
    const char *text,
    int scale
)
{
    size_t i;

    if (
        renderer == NULL
        || text == NULL
        || scale <= 0
    )
    {
        return;
    }

    for (
        i = 0;
        text[i] != '\0';
        ++i
    )
    {
        CH_BitmapFontSDLDrawChar(
            renderer,
            x
                + (int)i
                * CH_BITMAP_FONT_SDL_GLYPH_ADVANCE
                * scale,
            y,
            text[i],
            scale
        );
    }
}


void CH_BitmapFontSDLDrawCenteredText(
    SDL_Renderer *renderer,
    int canvas_width,
    int y,
    const char *text,
    int scale
)
{
    int width;

    if (
        renderer == NULL
        || text == NULL
        || canvas_width <= 0
        || scale <= 0
    )
    {
        return;
    }

    width =
        CH_BitmapFontSDLTextWidth(
            text,
            scale
        );

    CH_BitmapFontSDLDrawText(
        renderer,
        (canvas_width - width) / 2,
        y,
        text,
        scale
    );
}
