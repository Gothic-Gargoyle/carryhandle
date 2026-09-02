#ifndef CARRYHANDLE_CH_VIDEO_H
#define CARRYHANDLE_CH_VIDEO_H

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


bool CH_VideoInit(
    unsigned int frame_width,
    unsigned int frame_height,
    const char *title
);


bool CH_VideoPresentRGB32(
    const uint32_t *pixels,
    unsigned int pitch
);


/*
 * Set the 256-entry palette used by CH_VideoPresentIndexed8().
 *
 * Each source entry begins with R, G, B. entry_stride allows consumers
 * with RGB24, RGBA32 or similar palette layouts to use the same API.
 */
bool CH_VideoSetPaletteRGB8(
    const uint8_t *palette,
    unsigned int entry_stride
);


/*
 * Present an 8-bit indexed software framebuffer.
 *
 * The GameCube backend converts indexed pixels directly into the native
 * external framebuffer (XFB), avoiding a second full-size RGB framebuffer.
 */
bool CH_VideoPresentIndexed8(
    const uint8_t *pixels,
    unsigned int pitch
);


void CH_VideoShutdown(void);


#ifdef __cplusplus
}
#endif

#endif
