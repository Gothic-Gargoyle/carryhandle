# Presentation

CarryHandle separates machine identity from human-facing presentation and
provides reusable tooling/helpers for native GameCube presentation data.

## Manifest presentation metadata

Manifest v4 uses:

```ini
[presentation]
banner = assets/presentation/banner.png
company = My Studio
description = My Game for Nintendo GameCube
```

The banner source must be exactly 96x32 pixels. CarryHandle does not silently
resize or crop it.

`company` is human-facing text. It is deliberately distinct from the two-byte
`company_code` used in native GameCube identity.

## Native disc BNR

For native GameCube images, CarryHandle generates a root-level:

```text
opening.bnr
```

The current encoder is deterministic BNR1:

```text
format          BNR1
banner source   exactly 96x32
resizing        none
image encoding  tiled opaque RGB5A3
metadata        fixed-size NUL-padded ASCII fields
```

BNR2/localized metadata is not currently generated.

## Memory Card presentation

When `[memcard]` is present, a 32x32 icon is required. Optional card-specific
`title`, `comment`, and `banner` values override defaults inherited from the
application/presentation sections.

Disc and Memory Card encoders remain separate even when they share a source
banner because the on-card format differs.

`CH_CardPresentationApply()` installs generated presentation data into an
already-open Memory Card file while preserving bytes outside its assigned
sector-zero range.

The operation is idempotent: if payload and directory metadata already match,
no write is performed.

Presentation success/failure does not itself define persistence validity;
storage/application policy decides how important a cosmetic failure is.

## Application-save sector-zero layout

CarryHandle's standard application-save layout reserves:

```text
0..31   CHTX container header
32..63  reserved
64..    GameCube CARD presentation
```

The presentation layer uses read/modify/write so transaction metadata outside
its range remains intact.

## Splash and simple UI helpers

CarryHandle also contains reusable splash and optional SDL UI helpers for
bootstrap/presentation use. These are intentionally presentation helpers, not a
framework-wide UI system.

Relevant headers include:

```text
ch_splash.h
ch_bitmap_font_sdl.h
ch_memcard_ui.h
ch_memcard_ui_sdl.h
ch_controller_glyph.h
ch_controller_glyph_sdl.h
```

## Ownership boundary

CarryHandle owns generic encoding/presentation mechanisms and reusable assets.
The consumer owns its branding, text, when screens appear and application UX.
