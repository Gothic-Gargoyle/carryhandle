#include <carryhandle/ch_bitmap_font_sdl.h>
#include <carryhandle/ch_controller_glyph_sdl.h>
#include <carryhandle/ch_memcard_ui_sdl.h>

#include <stdio.h>


#define CH_MEMCARD_UI_TITLE_SCALE 4
#define CH_MEMCARD_UI_BODY_SCALE  3
#define CH_MEMCARD_UI_SMALL_SCALE 2

#define CH_MEMCARD_UI_HINT_GLYPH_SIZE 36
#define CH_MEMCARD_UI_HINT_GLYPH_GAP   8
#define CH_MEMCARD_UI_HINT_PAIR_GAP   28
#define CH_MEMCARD_UI_HINT_TEXT_SCALE  2


static void CH_MemCardUISetForeground(
    const CH_MemCardUISDLPresenter *presenter
)
{
    SDL_SetRenderDrawColor(
        presenter->renderer,
        presenter->foreground_r,
        presenter->foreground_g,
        presenter->foreground_b,
        255
    );
}


static void CH_MemCardUISetAccent(
    const CH_MemCardUISDLPresenter *presenter
)
{
    SDL_SetRenderDrawColor(
        presenter->renderer,
        presenter->accent_r,
        presenter->accent_g,
        presenter->accent_b,
        255
    );
}


static const char *CH_MemCardUITitle(
    CH_MemCardUIScreenKind kind
)
{
    switch (kind)
    {
        case CH_MEMCARD_UI_CHECKING:
            return "CHECKING MEMORY CARD";

        case CH_MEMCARD_UI_READY:
            return "MEMORY CARD READY";

        case CH_MEMCARD_UI_NO_CARD:
            return "NO MEMORY CARD";

        case CH_MEMCARD_UI_CREATE_PROMPT:
            return "CREATE SAVE FILE";

        case CH_MEMCARD_UI_CREATED:
            return "SAVE FILE CREATED";

        case CH_MEMCARD_UI_TOO_SMALL:
            return "MEMORY CARD TOO SMALL";

        case CH_MEMCARD_UI_INSUFFICIENT_SPACE:
            return "NOT ENOUGH SPACE";

        case CH_MEMCARD_UI_DISABLED:
            return "SAVING DISABLED";

        case CH_MEMCARD_UI_ERROR:
            return "MEMORY CARD ERROR";

        case CH_MEMCARD_UI_COUNT:
        default:
            return "MEMORY CARD";
    }
}


static void CH_MemCardUIDrawKnownBlocks(
    CH_MemCardUISDLPresenter *presenter,
    const CH_MemCardUIInfo *info,
    int *y
)
{
    char line[64];

    if (
        presenter == NULL
        || info == NULL
        || y == NULL
    )
    {
        return;
    }

    if (
        info->card_blocks
        != CH_MEMCARD_UI_U32_UNKNOWN
    )
    {
        snprintf(
            line,
            sizeof(line),
            "CAPACITY %u BLOCKS",
            (unsigned int)
                info->card_blocks
        );

        CH_BitmapFontSDLDrawCenteredText(
            presenter->renderer,
            presenter->width,
            *y,
            line,
            CH_MEMCARD_UI_SMALL_SCALE
        );

        *y += 28;
    }

    if (
        info->required_blocks
        != CH_MEMCARD_UI_U32_UNKNOWN
    )
    {
        snprintf(
            line,
            sizeof(line),
            "REQUIRED %u BLOCKS",
            (unsigned int)
                info->required_blocks
        );

        CH_BitmapFontSDLDrawCenteredText(
            presenter->renderer,
            presenter->width,
            *y,
            line,
            CH_MEMCARD_UI_SMALL_SCALE
        );

        *y += 28;
    }

    if (
        info->free_blocks
        != CH_MEMCARD_UI_U32_UNKNOWN
    )
    {
        snprintf(
            line,
            sizeof(line),
            "FREE %u BLOCKS",
            (unsigned int)
                info->free_blocks
        );

        CH_BitmapFontSDLDrawCenteredText(
            presenter->renderer,
            presenter->width,
            *y,
            line,
            CH_MEMCARD_UI_SMALL_SCALE
        );

        *y += 28;
    }

    if (
        info->initial_blocks
        != CH_MEMCARD_UI_U32_UNKNOWN
    )
    {
        snprintf(
            line,
            sizeof(line),
            "INITIAL %u BLOCKS",
            (unsigned int)
                info->initial_blocks
        );

        CH_BitmapFontSDLDrawCenteredText(
            presenter->renderer,
            presenter->width,
            *y,
            line,
            CH_MEMCARD_UI_SMALL_SCALE
        );

        *y += 28;
    }

    if (
        info->maximum_blocks
        != CH_MEMCARD_UI_U32_UNKNOWN
    )
    {
        snprintf(
            line,
            sizeof(line),
            "MAXIMUM %u BLOCKS",
            (unsigned int)
                info->maximum_blocks
        );

        CH_BitmapFontSDLDrawCenteredText(
            presenter->renderer,
            presenter->width,
            *y,
            line,
            CH_MEMCARD_UI_SMALL_SCALE
        );

        *y += 28;
    }
}



static int CH_MemCardUIActionHintWidth(
    const CH_MemCardUIActionHint *hint)
{
    int width = 0;

    if (hint == NULL)
        return 0;

    if (hint->glyph != CH_CONTROLLER_GLYPH_NONE)
        width += CH_MEMCARD_UI_HINT_GLYPH_SIZE
            + CH_MEMCARD_UI_HINT_GLYPH_GAP;

    width += CH_BitmapFontSDLTextWidth(
        hint->label,
        CH_MEMCARD_UI_HINT_TEXT_SCALE);

    return width;
}


static void CH_MemCardUIDrawActionHint(
    CH_MemCardUISDLPresenter *presenter,
    const CH_MemCardUIActionHint *hint,
    int x,
    int y)
{
    int text_x = x;

    if (presenter == NULL || hint == NULL)
        return;

    if (hint->glyph != CH_CONTROLLER_GLYPH_NONE)
    {
        SDL_Texture *texture = NULL;

        if (presenter->glyph_asset_root != NULL)
        {
            texture = CH_ControllerGlyphSDLLoadTexture(
                presenter->renderer,
                presenter->glyph_asset_root,
                hint->glyph,
                CH_MEMCARD_UI_HINT_GLYPH_SIZE);
        }

        if (texture != NULL)
        {
            SDL_Rect dst = {
                x, y,
                CH_MEMCARD_UI_HINT_GLYPH_SIZE,
                CH_MEMCARD_UI_HINT_GLYPH_SIZE
            };
            SDL_RenderCopy(presenter->renderer, texture, NULL, &dst);
            SDL_DestroyTexture(texture);
        }
        else
        {
            const char *fallback = CH_ControllerGlyphName(hint->glyph);

            CH_MemCardUISetAccent(presenter);
            CH_BitmapFontSDLDrawText(
                presenter->renderer,
                x + 8,
                y + 11,
                fallback,
                2);
        }

        text_x += CH_MEMCARD_UI_HINT_GLYPH_SIZE
            + CH_MEMCARD_UI_HINT_GLYPH_GAP;
    }

    CH_MemCardUISetForeground(presenter);
    CH_BitmapFontSDLDrawText(
        presenter->renderer,
        text_x,
        y + 11,
        hint->label,
        CH_MEMCARD_UI_HINT_TEXT_SCALE);
}


static void CH_MemCardUIDrawActionHints(
    CH_MemCardUISDLPresenter *presenter,
    const CH_MemCardUIInfo *info)
{
    unsigned int count;
    unsigned int i;
    int total_width = 0;
    int x;
    int y;

    if (presenter == NULL || info == NULL)
        return;

    count = info->action_hint_count;
    if (count > CH_MEMCARD_UI_MAX_ACTION_HINTS)
        count = CH_MEMCARD_UI_MAX_ACTION_HINTS;
    if (count == 0)
        return;

    for (i = 0; i < count; ++i)
    {
        if (i != 0)
            total_width += CH_MEMCARD_UI_HINT_PAIR_GAP;
        total_width += CH_MemCardUIActionHintWidth(&info->action_hints[i]);
    }

    x = (presenter->width - total_width) / 2;
    y = presenter->height - CH_MEMCARD_UI_HINT_GLYPH_SIZE - 24;

    for (i = 0; i < count; ++i)
    {
        if (i != 0)
            x += CH_MEMCARD_UI_HINT_PAIR_GAP;

        CH_MemCardUIDrawActionHint(
            presenter,
            &info->action_hints[i],
            x,
            y);

        x += CH_MemCardUIActionHintWidth(&info->action_hints[i]);
    }
}


bool CH_MemCardUISDLInit(
    CH_MemCardUISDLPresenter *presenter,
    SDL_Renderer *renderer,
    int width,
    int height
)
{
    if (
        presenter == NULL
        || renderer == NULL
        || width <= 0
        || height <= 0
    )
    {
        return false;
    }

    presenter->renderer = renderer;
    presenter->width = width;
    presenter->height = height;
    presenter->glyph_asset_root = NULL;

    CH_MemCardUISDLSetColors(
        presenter,
        12,
        12,
        16,
        232,
        232,
        232,
        132,
        132,
        148
    );

    return true;
}


void CH_MemCardUISDLSetGlyphAssetRoot(
    CH_MemCardUISDLPresenter *presenter,
    const char *asset_root)
{
    if (presenter != NULL)
        presenter->glyph_asset_root = asset_root;
}


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
)
{
    if (presenter == NULL)
    {
        return;
    }

    presenter->background_r =
        background_r;

    presenter->background_g =
        background_g;

    presenter->background_b =
        background_b;

    presenter->foreground_r =
        foreground_r;

    presenter->foreground_g =
        foreground_g;

    presenter->foreground_b =
        foreground_b;

    presenter->accent_r =
        accent_r;

    presenter->accent_g =
        accent_g;

    presenter->accent_b =
        accent_b;
}


bool CH_MemCardUISDLShow(
    void *userdata,
    CH_MemCardUIScreenKind kind,
    const CH_MemCardUIInfo *info
)
{
    CH_MemCardUISDLPresenter *presenter =
        (CH_MemCardUISDLPresenter *)userdata;

    SDL_Rect accent;
    const char *title;
    int y;

    if (
        presenter == NULL
        || presenter->renderer == NULL
        || info == NULL
        || kind < CH_MEMCARD_UI_CHECKING
        || kind >= CH_MEMCARD_UI_COUNT
    )
    {
        return false;
    }

    SDL_SetRenderDrawColor(
        presenter->renderer,
        presenter->background_r,
        presenter->background_g,
        presenter->background_b,
        255
    );

    if (
        SDL_RenderClear(
            presenter->renderer
        ) != 0
    )
    {
        return false;
    }

    accent.x = 48;
    accent.y = 82;
    accent.w = presenter->width - 96;
    accent.h = 4;

    CH_MemCardUISetAccent(
        presenter
    );

    SDL_RenderFillRect(
        presenter->renderer,
        &accent
    );

    CH_MemCardUISetForeground(
        presenter
    );

    title =
        CH_MemCardUITitle(
            kind
        );

    CH_BitmapFontSDLDrawCenteredText(
        presenter->renderer,
        presenter->width,
        110,
        title,
        CH_MEMCARD_UI_TITLE_SCALE
    );

    y = 205;

    if (
        info->application_name != NULL
        && info->application_name[0] != '\0'
    )
    {
        CH_BitmapFontSDLDrawCenteredText(
            presenter->renderer,
            presenter->width,
            y,
            info->application_name,
            CH_MEMCARD_UI_BODY_SCALE
        );

        y += 44;
    }

    if (
        info->detail != NULL
        && info->detail[0] != '\0'
    )
    {
        CH_BitmapFontSDLDrawCenteredText(
            presenter->renderer,
            presenter->width,
            y,
            info->detail,
            CH_MEMCARD_UI_SMALL_SCALE
        );

        y += 38;
    }

    CH_MemCardUIDrawKnownBlocks(
        presenter,
        info,
        &y
    );

    if (info->action_hint_count != 0)
    {
        CH_MemCardUIDrawActionHints(presenter, info);
    }
    else if (info->prompt != NULL && info->prompt[0] != '\0')
    {
        CH_MemCardUISetAccent(presenter);
        CH_BitmapFontSDLDrawCenteredText(
            presenter->renderer,
            presenter->width,
            presenter->height - 42,
            info->prompt,
            CH_MEMCARD_UI_SMALL_SCALE);
    }

    SDL_RenderPresent(
        presenter->renderer
    );

    return true;
}


CH_MemCardUI CH_MemCardUISDLMakeUI(
    CH_MemCardUISDLPresenter *presenter
)
{
    CH_MemCardUI ui;

    ui.show =
        CH_MemCardUISDLShow;

    ui.userdata =
        presenter;

    return ui;
}
