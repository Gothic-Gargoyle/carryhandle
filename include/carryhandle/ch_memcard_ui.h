#ifndef CARRYHANDLE_CH_MEMCARD_UI_H
#define CARRYHANDLE_CH_MEMCARD_UI_H

#include <stdbool.h>
#include <stdint.h>

#include <carryhandle/ch_controller_glyph.h>


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Semantic Memory Card screens shared by CarryHandle applications.
 *
 * These values describe WHAT the application needs to communicate.
 * They deliberately say nothing about HOW a renderer presents it.
 *
 * A themed application can therefore provide its own art, fonts and
 * controller prompts while using the same underlying screen contract.
 */
typedef enum CH_MemCardUIScreenKind
{
    CH_MEMCARD_UI_CHECKING = 0,
    CH_MEMCARD_UI_READY,
    CH_MEMCARD_UI_NO_CARD,
    CH_MEMCARD_UI_CREATE_PROMPT,
    CH_MEMCARD_UI_CREATED,
    CH_MEMCARD_UI_TOO_SMALL,
    CH_MEMCARD_UI_INSUFFICIENT_SPACE,
    CH_MEMCARD_UI_DISABLED,
    CH_MEMCARD_UI_ERROR,

    CH_MEMCARD_UI_COUNT
} CH_MemCardUIScreenKind;

typedef struct CH_MemCardUIActionHint
{
    CH_ControllerGlyph glyph;
    const char *label;
} CH_MemCardUIActionHint;

#define CH_MEMCARD_UI_MAX_ACTION_HINTS 2u


/*
 * Numeric fields use this value when a quantity is not known or does
 * not apply to the current screen.
 *
 * Zero remains meaningful, especially for free_blocks.
 */
#define CH_MEMCARD_UI_U32_UNKNOWN UINT32_MAX


/*
 * Renderer-neutral information for one Memory Card screen.
 *
 * slot
 *     Native GameCube Memory Card slot index.  Use -1 when unknown or
 *     when the screen is not associated with one physical slot.
 *
 * card_blocks
 *     Total user-visible card capacity in blocks when known.
 *
 * free_blocks
 *     Currently free blocks when known.  Zero is a valid value.
 *
 * required_blocks
 *     Blocks required for the operation which triggered this screen.
 *
 * initial_blocks / maximum_blocks
 *     Application save-file sizing policy when relevant.
 *
 * application_name
 *     Optional human-readable application/game name.
 *
 * detail
 *     Optional application-provided detail string.  The generic
 *     CarryHandle presenter may show it; themed presenters may replace
 *     it with their own wording while preserving the semantic kind.
 *
 * prompt
 *     Optional player-facing action/help text for the current screen.
 *     This is presentation only: input polling and confirmation remain
 *     explicitly owned by the caller.  A themed presenter may replace
 *     textual button names with controller glyphs.
 */
typedef struct CH_MemCardUIInfo
{
    int slot;

    uint32_t card_blocks;
    uint32_t free_blocks;
    uint32_t required_blocks;
    uint32_t initial_blocks;
    uint32_t maximum_blocks;

    const char *application_name;
    const char *detail;
    const char *prompt;

    unsigned int action_hint_count;
    CH_MemCardUIActionHint action_hints[CH_MEMCARD_UI_MAX_ACTION_HINTS];
} CH_MemCardUIInfo;


/*
 * Application-supplied presentation callback.
 *
 * CarryHandle core intentionally does not own an input loop here.
 * Prompt confirmation/cancellation remains explicit in the caller.
 *
 * Return true when the screen was presented successfully.
 */
typedef bool (*CH_MemCardUIShowFn)(
    void *userdata,
    CH_MemCardUIScreenKind kind,
    const CH_MemCardUIInfo *info
);


typedef struct CH_MemCardUI
{
    CH_MemCardUIShowFn show;
    void *userdata;
} CH_MemCardUI;


/*
 * Initialize an info structure with safe "unknown" defaults.
 */
void CH_MemCardUIInfoInit(
    CH_MemCardUIInfo *info
);


/*
 * Stable diagnostic name for one screen kind.
 *
 * Returns "UNKNOWN" for an invalid value.
 */
const char *CH_MemCardUIScreenName(
    CH_MemCardUIScreenKind kind
);


/*
 * Validate and dispatch one semantic screen to an application presenter.
 *
 * Returns false for an invalid kind, NULL object/callback/info, or when
 * the presentation callback itself reports failure.
 */
bool CH_MemCardUIShow(
    const CH_MemCardUI *ui,
    CH_MemCardUIScreenKind kind,
    const CH_MemCardUIInfo *info
);


#ifdef __cplusplus
}
#endif

#endif
