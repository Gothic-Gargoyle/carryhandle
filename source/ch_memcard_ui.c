#include <carryhandle/ch_memcard_ui.h>

#include <stddef.h>


void CH_MemCardUIInfoInit(
    CH_MemCardUIInfo *info
)
{
    if (info == NULL)
    {
        return;
    }

    info->slot = -1;

    info->card_blocks =
        CH_MEMCARD_UI_U32_UNKNOWN;

    info->free_blocks =
        CH_MEMCARD_UI_U32_UNKNOWN;

    info->required_blocks =
        CH_MEMCARD_UI_U32_UNKNOWN;

    info->initial_blocks =
        CH_MEMCARD_UI_U32_UNKNOWN;

    info->maximum_blocks =
        CH_MEMCARD_UI_U32_UNKNOWN;

    info->application_name = NULL;
    info->detail = NULL;
    info->prompt = NULL;

    info->action_hint_count = 0;
    info->action_hints[0].glyph = CH_CONTROLLER_GLYPH_NONE;
    info->action_hints[0].label = NULL;
    info->action_hints[1].glyph = CH_CONTROLLER_GLYPH_NONE;
    info->action_hints[1].label = NULL;
}


const char *CH_MemCardUIScreenName(
    CH_MemCardUIScreenKind kind
)
{
    switch (kind)
    {
        case CH_MEMCARD_UI_CHECKING:
            return "CHECKING";

        case CH_MEMCARD_UI_READY:
            return "READY";

        case CH_MEMCARD_UI_NO_CARD:
            return "NO_CARD";

        case CH_MEMCARD_UI_CREATE_PROMPT:
            return "CREATE_PROMPT";

        case CH_MEMCARD_UI_CREATED:
            return "CREATED";

        case CH_MEMCARD_UI_TOO_SMALL:
            return "TOO_SMALL";

        case CH_MEMCARD_UI_INSUFFICIENT_SPACE:
            return "INSUFFICIENT_SPACE";

        case CH_MEMCARD_UI_DISABLED:
            return "DISABLED";

        case CH_MEMCARD_UI_ERROR:
            return "ERROR";

        case CH_MEMCARD_UI_COUNT:
        default:
            return "UNKNOWN";
    }
}


bool CH_MemCardUIShow(
    const CH_MemCardUI *ui,
    CH_MemCardUIScreenKind kind,
    const CH_MemCardUIInfo *info
)
{
    if (
        ui == NULL
        || ui->show == NULL
        || info == NULL
        || kind < CH_MEMCARD_UI_CHECKING
        || kind >= CH_MEMCARD_UI_COUNT
    )
    {
        return false;
    }

    return ui->show(
        ui->userdata,
        kind,
        info
    );
}
