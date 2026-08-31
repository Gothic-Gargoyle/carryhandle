#ifndef CARRYHANDLE_CH_APPLICATION_H
#define CARRYHANDLE_CH_APPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Values deliberately match the GameCube BI2 region field.
 *
 * This is application/disc identity, not runtime video mode.
 */
typedef enum CH_ApplicationRegion
{
    CH_APPLICATION_REGION_NTSC_J = 0,
    CH_APPLICATION_REGION_NTSC_U = 1,
    CH_APPLICATION_REGION_PAL = 2
} CH_ApplicationRegion;


/*
 * Immutable metadata generated from carryhandle.cfg at build time.
 *
 * Returned strings have static storage duration.
 *
 * game_code    : exactly 4 ASCII bytes
 * company_code : exactly 2 ASCII bytes
 *
 * store_id is deliberately independent from game_code.
 */
typedef struct CH_ApplicationInfo
{
    unsigned int manifest_version;

    const char *name;

    const char *game_code;
    const char *company_code;

    CH_ApplicationRegion region;

    const char *store_id;
} CH_ApplicationInfo;


/*
 * Return this build's immutable application descriptor.
 */
const CH_ApplicationInfo *CH_ApplicationGetInfo(void);


/*
 * Return NTSC-J, NTSC-U, PAL, or UNKNOWN.
 */
const char *CH_ApplicationRegionName(
    CH_ApplicationRegion region
);


#ifdef __cplusplus
}
#endif

#endif
