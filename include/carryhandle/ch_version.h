#ifndef CARRYHANDLE_CH_VERSION_H
#define CARRYHANDLE_CH_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#define CH_VERSION_MAJOR 0
#define CH_VERSION_MINOR 0
#define CH_VERSION_PATCH 0

#define CH_VERSION_STRING "0.0.0-dev"

/*
 * Return the CarryHandle version string.
 *
 * The returned string has static storage duration and must not be freed.
 */
const char *CH_VersionString(void);

#ifdef __cplusplus
}
#endif

#endif
