#include <carryhandle/ch_controller_glyph.h>
#include <string.h>


const char *CH_ControllerGlyphName(
    CH_ControllerGlyph glyph)
{
    switch (glyph)
    {
        case CH_CONTROLLER_GLYPH_A:
            return "A";

        case CH_CONTROLLER_GLYPH_B:
            return "B";

        case CH_CONTROLLER_GLYPH_X:
            return "X";

        case CH_CONTROLLER_GLYPH_Y:
            return "Y";

        case CH_CONTROLLER_GLYPH_L_ANALOG:
            return "L ANALOG";

        case CH_CONTROLLER_GLYPH_L_DIGITAL:
            return "L CLICK";

        case CH_CONTROLLER_GLYPH_R_ANALOG:
            return "R ANALOG";

        case CH_CONTROLLER_GLYPH_R_DIGITAL:
            return "R CLICK";

        case CH_CONTROLLER_GLYPH_Z:
            return "Z";

        case CH_CONTROLLER_GLYPH_START:
            return "START";

        case CH_CONTROLLER_GLYPH_DPAD:
            return "D-PAD";

        case CH_CONTROLLER_GLYPH_DPAD_UP:
            return "D-PAD UP";

        case CH_CONTROLLER_GLYPH_DPAD_DOWN:
            return "D-PAD DOWN";

        case CH_CONTROLLER_GLYPH_DPAD_LEFT:
            return "D-PAD LEFT";

        case CH_CONTROLLER_GLYPH_DPAD_RIGHT:
            return "D-PAD RIGHT";

        case CH_CONTROLLER_GLYPH_DPAD_LEFT_RIGHT:
            return "D-PAD LEFT-RIGHT";

        case CH_CONTROLLER_GLYPH_DPAD_UP_DOWN:
            return "D-PAD UP-DOWN";

        case CH_CONTROLLER_GLYPH_STICK:
            return "CONTROL STICK";

        case CH_CONTROLLER_GLYPH_STICK_UP:
            return "CONTROL STICK UP";

        case CH_CONTROLLER_GLYPH_STICK_DOWN:
            return "CONTROL STICK DOWN";

        case CH_CONTROLLER_GLYPH_STICK_LEFT:
            return "CONTROL STICK LEFT";

        case CH_CONTROLLER_GLYPH_STICK_RIGHT:
            return "CONTROL STICK RIGHT";

        case CH_CONTROLLER_GLYPH_STICK_LEFT_RIGHT:
            return "CONTROL STICK LEFT-RIGHT";

        case CH_CONTROLLER_GLYPH_STICK_UP_DOWN:
            return "CONTROL STICK UP-DOWN";

        case CH_CONTROLLER_GLYPH_STICK_ALL:
            return "CONTROL STICK ALL";

        case CH_CONTROLLER_GLYPH_STICK_CLOCKWISE:
            return "CONTROL STICK CLOCKWISE";

        case CH_CONTROLLER_GLYPH_STICK_COUNTER_CLOCKWISE:
            return "CONTROL STICK COUNTER-CLOCKWISE";

        case CH_CONTROLLER_GLYPH_CSTICK:
            return "C-STICK";

        case CH_CONTROLLER_GLYPH_CSTICK_UP:
            return "C-STICK UP";

        case CH_CONTROLLER_GLYPH_CSTICK_DOWN:
            return "C-STICK DOWN";

        case CH_CONTROLLER_GLYPH_CSTICK_LEFT:
            return "C-STICK LEFT";

        case CH_CONTROLLER_GLYPH_CSTICK_RIGHT:
            return "C-STICK RIGHT";

        case CH_CONTROLLER_GLYPH_CSTICK_LEFT_RIGHT:
            return "C-STICK LEFT-RIGHT";

        case CH_CONTROLLER_GLYPH_CSTICK_UP_DOWN:
            return "C-STICK UP-DOWN";

        case CH_CONTROLLER_GLYPH_CSTICK_ALL:
            return "C-STICK ALL";

        case CH_CONTROLLER_GLYPH_CSTICK_CLOCKWISE:
            return "C-STICK CLOCKWISE";

        case CH_CONTROLLER_GLYPH_CSTICK_COUNTER_CLOCKWISE:
            return "C-STICK COUNTER-CLOCKWISE";

        case CH_CONTROLLER_GLYPH_NONE:
        case CH_CONTROLLER_GLYPH_COUNT:
        default:
            return "NONE";
    }
}


const char *CH_ControllerGlyphAssetKey(
    CH_ControllerGlyph glyph)
{
    switch (glyph)
    {
        case CH_CONTROLLER_GLYPH_A:
            return "a";

        case CH_CONTROLLER_GLYPH_B:
            return "b";

        case CH_CONTROLLER_GLYPH_X:
            return "x";

        case CH_CONTROLLER_GLYPH_Y:
            return "y";

        case CH_CONTROLLER_GLYPH_L_ANALOG:
            return "l-analog";

        case CH_CONTROLLER_GLYPH_L_DIGITAL:
            return "l-digital";

        case CH_CONTROLLER_GLYPH_R_ANALOG:
            return "r-analog";

        case CH_CONTROLLER_GLYPH_R_DIGITAL:
            return "r-digital";

        case CH_CONTROLLER_GLYPH_Z:
            return "z";

        case CH_CONTROLLER_GLYPH_START:
            return "start";

        case CH_CONTROLLER_GLYPH_DPAD:
            return "dpad";

        case CH_CONTROLLER_GLYPH_DPAD_UP:
            return "dpad-up";

        case CH_CONTROLLER_GLYPH_DPAD_DOWN:
            return "dpad-down";

        case CH_CONTROLLER_GLYPH_DPAD_LEFT:
            return "dpad-left";

        case CH_CONTROLLER_GLYPH_DPAD_RIGHT:
            return "dpad-right";

        case CH_CONTROLLER_GLYPH_DPAD_LEFT_RIGHT:
            return "dpad-left-right";

        case CH_CONTROLLER_GLYPH_DPAD_UP_DOWN:
            return "dpad-up-down";

        case CH_CONTROLLER_GLYPH_STICK:
            return "stick";

        case CH_CONTROLLER_GLYPH_STICK_UP:
            return "stick-up";

        case CH_CONTROLLER_GLYPH_STICK_DOWN:
            return "stick-down";

        case CH_CONTROLLER_GLYPH_STICK_LEFT:
            return "stick-left";

        case CH_CONTROLLER_GLYPH_STICK_RIGHT:
            return "stick-right";

        case CH_CONTROLLER_GLYPH_STICK_LEFT_RIGHT:
            return "stick-left-right";

        case CH_CONTROLLER_GLYPH_STICK_UP_DOWN:
            return "stick-up-down";

        case CH_CONTROLLER_GLYPH_STICK_ALL:
            return "stick-all";

        case CH_CONTROLLER_GLYPH_STICK_CLOCKWISE:
            return "stick-clockwise";

        case CH_CONTROLLER_GLYPH_STICK_COUNTER_CLOCKWISE:
            return "stick-counter-clockwise";

        case CH_CONTROLLER_GLYPH_CSTICK:
            return "cstick";

        case CH_CONTROLLER_GLYPH_CSTICK_UP:
            return "cstick-up";

        case CH_CONTROLLER_GLYPH_CSTICK_DOWN:
            return "cstick-down";

        case CH_CONTROLLER_GLYPH_CSTICK_LEFT:
            return "cstick-left";

        case CH_CONTROLLER_GLYPH_CSTICK_RIGHT:
            return "cstick-right";

        case CH_CONTROLLER_GLYPH_CSTICK_LEFT_RIGHT:
            return "cstick-left-right";

        case CH_CONTROLLER_GLYPH_CSTICK_UP_DOWN:
            return "cstick-up-down";

        case CH_CONTROLLER_GLYPH_CSTICK_ALL:
            return "cstick-all";

        case CH_CONTROLLER_GLYPH_CSTICK_CLOCKWISE:
            return "cstick-clockwise";

        case CH_CONTROLLER_GLYPH_CSTICK_COUNTER_CLOCKWISE:
            return "cstick-counter-clockwise";

        case CH_CONTROLLER_GLYPH_NONE:
        case CH_CONTROLLER_GLYPH_COUNT:
        default:
            return "none";
    }
}


CH_ControllerGlyph CH_ControllerGlyphForPhysical(
    CH_PhysicalInput input)
{
    switch (input)
    {
        case CH_PHYSICAL_A:
            return CH_CONTROLLER_GLYPH_A;

        case CH_PHYSICAL_B:
            return CH_CONTROLLER_GLYPH_B;

        case CH_PHYSICAL_X:
            return CH_CONTROLLER_GLYPH_X;

        case CH_PHYSICAL_Y:
            return CH_CONTROLLER_GLYPH_Y;

        /*
         * Existing CH_PHYSICAL_L/R are the DIGITAL bottom-click switches.
         * Analogue travel remains independent in CH_PadState.trigger_l/r.
         */
        case CH_PHYSICAL_L:
            return CH_CONTROLLER_GLYPH_L_DIGITAL;

        case CH_PHYSICAL_R:
            return CH_CONTROLLER_GLYPH_R_DIGITAL;

        case CH_PHYSICAL_Z:
            return CH_CONTROLLER_GLYPH_Z;

        case CH_PHYSICAL_START:
            return CH_CONTROLLER_GLYPH_START;

        case CH_PHYSICAL_DPAD_UP:
            return CH_CONTROLLER_GLYPH_DPAD_UP;

        case CH_PHYSICAL_DPAD_DOWN:
            return CH_CONTROLLER_GLYPH_DPAD_DOWN;

        case CH_PHYSICAL_DPAD_LEFT:
            return CH_CONTROLLER_GLYPH_DPAD_LEFT;

        case CH_PHYSICAL_DPAD_RIGHT:
            return CH_CONTROLLER_GLYPH_DPAD_RIGHT;

        case CH_PHYSICAL_STICK_UP:
            return CH_CONTROLLER_GLYPH_STICK_UP;

        case CH_PHYSICAL_STICK_DOWN:
            return CH_CONTROLLER_GLYPH_STICK_DOWN;

        case CH_PHYSICAL_STICK_LEFT:
            return CH_CONTROLLER_GLYPH_STICK_LEFT;

        case CH_PHYSICAL_STICK_RIGHT:
            return CH_CONTROLLER_GLYPH_STICK_RIGHT;

        case CH_PHYSICAL_CSTICK_UP:
            return CH_CONTROLLER_GLYPH_CSTICK_UP;

        case CH_PHYSICAL_CSTICK_DOWN:
            return CH_CONTROLLER_GLYPH_CSTICK_DOWN;

        case CH_PHYSICAL_CSTICK_LEFT:
            return CH_CONTROLLER_GLYPH_CSTICK_LEFT;

        case CH_PHYSICAL_CSTICK_RIGHT:
            return CH_CONTROLLER_GLYPH_CSTICK_RIGHT;

        case CH_PHYSICAL_NONE:
        case CH_PHYSICAL_COUNT:
        default:
            return CH_CONTROLLER_GLYPH_NONE;
    }
}

typedef struct
{
    const char *key;
    const char *path;
} CH_ControllerGlyphAssetEntry;

static const CH_ControllerGlyphAssetEntry
chControllerGlyphAssets[] =
{
    { "a", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-A.bmp" },
    { "b", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-B.bmp" },
    { "x", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-X.bmp" },
    { "y", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Y.bmp" },

    /*
     * Toomai provides separate trigger states:
     *   -P = pressed analogue travel
     *   -C = clicked digital bottom switch
     */
    { "l-analog", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-L.bmp" },
    { "l-digital", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-L.bmp" },
    { "r-analog", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-R.bmp" },
    { "r-digital", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-R.bmp" },

    { "z", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Z.bmp" },
    { "start", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Start-Pause.bmp" },

    { "dpad", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-D-Pad.bmp" },
    { "dpad-up", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-D-Pad-U.bmp" },
    { "dpad-down", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-D-Pad-D.bmp" },
    { "dpad-left", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-D-Pad-L.bmp" },
    { "dpad-right", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-D-Pad-R.bmp" },
    { "dpad-left-right", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-D-Pad.bmp" },
    { "dpad-up-down", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-D-Pad.bmp" },

    { "stick", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Control Stick.bmp" },
    { "stick-up", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Control Stick-U.bmp" },
    { "stick-down", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Control Stick-D.bmp" },
    { "stick-left", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Control Stick-L.bmp" },
    { "stick-right", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Control Stick-R.bmp" },
    { "stick-left-right", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Control Stick.bmp" },
    { "stick-up-down", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Control Stick.bmp" },
    { "stick-all", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Control Stick.bmp" },
    { "stick-clockwise", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Control Stick.bmp" },
    { "stick-counter-clockwise", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-Control Stick.bmp" },

    { "cstick", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-C-Stick.bmp" },
    { "cstick-up", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-C-Stick-U.bmp" },
    { "cstick-down", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-C-Stick-D.bmp" },
    { "cstick-left", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-C-Stick-L.bmp" },
    { "cstick-right", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-C-Stick-R.bmp" },
    { "cstick-left-right", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-C-Stick.bmp" },
    { "cstick-up-down", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-C-Stick.bmp" },
    { "cstick-all", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-C-Stick.bmp" },
    { "cstick-clockwise", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-C-Stick.bmp" },
    { "cstick-counter-clockwise", "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-C-Stick.bmp" }
};

const char *CH_ControllerGlyphAssetPath(
    CH_ControllerGlyph glyph)
{
    const char *key =
        CH_ControllerGlyphAssetKey(glyph);
    size_t i;

    if (key == NULL || key[0] == '\0')
        return NULL;

    for (i = 0;
         i < sizeof(chControllerGlyphAssets) /
             sizeof(chControllerGlyphAssets[0]);
         ++i)
    {
        if (strcmp(
                key,
                chControllerGlyphAssets[i].key) == 0)
        {
            return chControllerGlyphAssets[i].path;
        }
    }

    return NULL;
}
