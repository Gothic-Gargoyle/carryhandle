# Toomai GameCube controller glyph runtime BMPs

The pristine Toomai SVG source artwork remains in `../buttons/`.

Those SVGs were rendered on the development host with a full desktop SVG
renderer to 64x64 RGBA PNGs. This directory contains a lossless conversion of
those RGBA pixels to 32-bit BMP V4 with explicit red/green/blue/alpha masks.

Why BMP at runtime:

- GameCube SDL2 can decode BMP directly via `SDL_LoadBMP_RW()`.
- No SDL2_image generic format dispatcher is needed.
- No standalone libpng/zlib/JPEG/TIFF/WebP link dependencies are needed.
- NanoSVG is completely bypassed for this controller glyph set.

The BMP conversion does not alter the rendered RGBA pixels.
