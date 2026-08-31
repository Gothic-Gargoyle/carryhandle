#include <carryhandle/ch_application.h>

#include "ch_application_config.h"


static const CH_ApplicationInfo applicationInfo =
{
    CH_GENERATED_MANIFEST_VERSION,

    CH_GENERATED_APPLICATION_NAME,

    CH_GENERATED_GAME_CODE,
    CH_GENERATED_COMPANY_CODE,

    CH_GENERATED_APPLICATION_REGION,

    CH_GENERATED_PERSISTENCE_STORE_ID
};


const CH_ApplicationInfo *CH_ApplicationGetInfo(void)
{
    return &applicationInfo;
}


const char *CH_ApplicationRegionName(
    CH_ApplicationRegion region
)
{
    switch (region)
    {
        case CH_APPLICATION_REGION_NTSC_J:
            return "NTSC-J";

        case CH_APPLICATION_REGION_NTSC_U:
            return "NTSC-U";

        case CH_APPLICATION_REGION_PAL:
            return "PAL";

        default:
            return "UNKNOWN";
    }
}
