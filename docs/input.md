# Input, Rumble and Controller Glyphs

CarryHandle separates physical GameCube controls from consumer/game semantics.

## Input ownership

CarryHandle may represent physical buttons, sticks, triggers and controller
state. The consumer decides what those controls mean in the application.

Examples of consumer-owned policy:

- A means jump/use/fire
- a particular stick threshold opens a menu
- shotgun firing triggers a particular rumble effect

Those mappings do not belong in generic CarryHandle modules.

## Physical input

Use the input and physical-input APIs when a consumer needs a stable vocabulary
for GameCube controls without embedding game-action semantics.

Relevant headers:

```text
ch_input.h
ch_input_physical.h
ch_input_bindings.h
```

## Rumble

Rumble helpers manage GameCube motor behavior and reusable sequencing/lifecycle.
Game-event-to-effect mapping remains consumer policy.

Relevant header:

```text
ch_rumble.h
```

## Controller glyph vocabulary

`ch_controller_glyph.h` exposes renderer-neutral presentation identifiers.
CarryHandle deliberately does not expose `SDL_Texture`, `GXTexObj`, or another
renderer-specific object as the generic glyph API.

A glyph can provide:

- a stable human-readable name
- a stable logical asset key
- a canonical CarryHandle-relative asset path
- a mapping from a physical input to an appropriate prompt glyph

## Trigger distinction

The physical L/R button mappings refer to the digital bottom-click switches.
Analogue trigger travel remains separately available in pad state and has
separate glyphs when a consumer explicitly wants to present it.

This avoids collapsing two physically different controls merely because the
same trigger assembly contains both.

## SDL glyph loading

Optional SDL-specific helpers can load/rasterize CarryHandle controller assets
for SDL consumers. This remains an optional presentation layer and does not
make SDL a dependency of the renderer-neutral glyph vocabulary.

## Asset ownership

CarryHandle provides reusable controller artwork and logical asset mapping.
The consumer owns:

- where and when prompts are shown
- prompt layout
- game/action labels
- renderer integration beyond the provided optional adapter
