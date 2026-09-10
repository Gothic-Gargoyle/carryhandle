#ifndef CARRYHANDLE_CH_CONTROLLER_GLYPH_H
#define CARRYHANDLE_CH_CONTROLLER_GLYPH_H

#include <carryhandle/ch_input_physical.h>


/*
 * Renderer-neutral GameCube controller-glyph vocabulary.
 *
 * These identifiers describe presentation assets, not renderer objects.
 * CarryHandle deliberately does not expose SDL_Texture, GXTexObj or any
 * other renderer-specific type here.
 *
 * The canonical asset key returned by CH_ControllerGlyphAssetKey() is a
 * stable logical name.  Consumers or build tools may map that key to
 * whatever raster/texture representation they need.
 *
 * Important trigger distinction:
 *
 *     CH_PHYSICAL_L / CH_PHYSICAL_R
 *
 * currently mean the physical DIGITAL bottom-click switches.  Their glyph
 * mapping therefore resolves to L_DIGITAL / R_DIGITAL.
 *
 * The independent analogue trigger travel is already preserved in
 * CH_PadState.trigger_l / trigger_r.  It is intentionally NOT collapsed
 * into the digital mapping below.  L_ANALOG / R_ANALOG glyphs exist so a
 * consumer can explicitly present that analogue control.
 */
typedef enum
{
    CH_CONTROLLER_GLYPH_NONE = 0,

    CH_CONTROLLER_GLYPH_A,
    CH_CONTROLLER_GLYPH_B,
    CH_CONTROLLER_GLYPH_X,
    CH_CONTROLLER_GLYPH_Y,

    CH_CONTROLLER_GLYPH_L_ANALOG,
    CH_CONTROLLER_GLYPH_L_DIGITAL,
    CH_CONTROLLER_GLYPH_R_ANALOG,
    CH_CONTROLLER_GLYPH_R_DIGITAL,
    CH_CONTROLLER_GLYPH_Z,
    CH_CONTROLLER_GLYPH_START,

    CH_CONTROLLER_GLYPH_DPAD,
    CH_CONTROLLER_GLYPH_DPAD_UP,
    CH_CONTROLLER_GLYPH_DPAD_DOWN,
    CH_CONTROLLER_GLYPH_DPAD_LEFT,
    CH_CONTROLLER_GLYPH_DPAD_RIGHT,
    CH_CONTROLLER_GLYPH_DPAD_LEFT_RIGHT,
    CH_CONTROLLER_GLYPH_DPAD_UP_DOWN,

    CH_CONTROLLER_GLYPH_STICK,
    CH_CONTROLLER_GLYPH_STICK_UP,
    CH_CONTROLLER_GLYPH_STICK_DOWN,
    CH_CONTROLLER_GLYPH_STICK_LEFT,
    CH_CONTROLLER_GLYPH_STICK_RIGHT,
    CH_CONTROLLER_GLYPH_STICK_LEFT_RIGHT,
    CH_CONTROLLER_GLYPH_STICK_UP_DOWN,
    CH_CONTROLLER_GLYPH_STICK_ALL,
    CH_CONTROLLER_GLYPH_STICK_CLOCKWISE,
    CH_CONTROLLER_GLYPH_STICK_COUNTER_CLOCKWISE,

    CH_CONTROLLER_GLYPH_CSTICK,
    CH_CONTROLLER_GLYPH_CSTICK_UP,
    CH_CONTROLLER_GLYPH_CSTICK_DOWN,
    CH_CONTROLLER_GLYPH_CSTICK_LEFT,
    CH_CONTROLLER_GLYPH_CSTICK_RIGHT,
    CH_CONTROLLER_GLYPH_CSTICK_LEFT_RIGHT,
    CH_CONTROLLER_GLYPH_CSTICK_UP_DOWN,
    CH_CONTROLLER_GLYPH_CSTICK_ALL,
    CH_CONTROLLER_GLYPH_CSTICK_CLOCKWISE,
    CH_CONTROLLER_GLYPH_CSTICK_COUNTER_CLOCKWISE,

    CH_CONTROLLER_GLYPH_COUNT
} CH_ControllerGlyph;


/*
 * Return a stable human-readable glyph name suitable for diagnostics and
 * generic controls UI.
 */
const char *CH_ControllerGlyphName(
    CH_ControllerGlyph glyph);


/*
 * Return a stable renderer-neutral asset key.
 *
 * Examples:
 *
 *     "a"
 *     "r-digital"
 *     "dpad-left-right"
 *     "cstick-clockwise"
 *
 * Returns "none" for NONE, COUNT or an invalid value.
 */
const char *CH_ControllerGlyphAssetKey(
    CH_ControllerGlyph glyph);

/*
 * Return the canonical CarryHandle-relative SVG asset path for a glyph.
 *
 * The returned path is renderer-neutral and has no device prefix. A
 * consumer that mirrors CarryHandle's assets tree into a GameCube FST can,
 * for example, prepend "dvd:/" before opening it.
 *
 * Returns NULL for NONE/COUNT/invalid values or unavailable artwork.
 */
const char *CH_ControllerGlyphAssetPath(
    CH_ControllerGlyph glyph);


/*
 * Map one existing CarryHandle physical input to its canonical prompt glyph.
 *
 * L/R deliberately map to the DIGITAL click glyphs because CH_PHYSICAL_L
 * and CH_PHYSICAL_R test CH_PAD_BUTTON_L / CH_PAD_BUTTON_R.
 *
 * Composite presentation glyphs such as STICK_ALL and DPAD_LEFT_RIGHT do
 * not correspond to one CH_PhysicalInput and are requested directly.
 */
CH_ControllerGlyph CH_ControllerGlyphForPhysical(
    CH_PhysicalInput input);


#endif
