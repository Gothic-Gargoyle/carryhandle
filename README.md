# CarryHandle

CarryHandle is a lightweight C framework for Nintendo GameCube homebrew
built around devkitPPC and libogc2.

It provides reusable GameCube platform components and build tooling for
applications that need more than raw hardware APIs, without requiring
them to adopt a game engine or monolithic framework architecture.

CarryHandle grew out of reusable platform work developed while porting
[DoomCube](https://github.com/Gothic-Gargoyle/doomcube).

## Goals

CarryHandle aims to make starting a GameCube homebrew project easier by
providing small, reusable modules for common platform tasks.

Planned areas include:

- controller input
- controller rumble
- native DVD/FST file access
- Memory Card access
- persistent storage helpers
- common platform and video setup where useful
- logging and diagnostics
- native GameCube disc-image tooling
- reusable build infrastructure
- small examples demonstrating each module

CarryHandle is GameCube-first and intentionally focused.

## Non-goals

CarryHandle is not:

- a game engine
- a scene graph
- an entity-component system
- a physics engine
- a cross-platform rendering abstraction
- a replacement for SDL
- a replacement for libogc2
- a Doom compatibility layer
- a framework-owned main loop

Applications remain in control of their own program structure and main
loop.

CarryHandle should wrap or extend libogc2 only where doing so provides
real reusable value.

## Design philosophy

CarryHandle modules should be:

- small
- explicit
- independently usable
- C-first
- GameCube-specific
- easy to vendor into another project
- understandable without framework magic

Using one CarryHandle module should not require adopting every other
module.

For example, an application should eventually be able to use DVD access
without also using CarryHandle video, persistence, or rumble support.

## API naming

Public functions use the `CH_` prefix.

Examples:

```c
CH_RumbleSetEnabled(true);
CH_DVDOpen(...);
CH_CardMount(...);
```

Public types follow the same convention:

```c
CH_PadState
CH_DVDFile
CH_CardInfo
```

Constants are also namespaced:

```c
CH_CARD_SLOT_A
CH_PAD_BUTTON_A
```

Internal implementation symbols must not be exposed merely because an
application happened to need them during development.

## Project layout

```text
carryhandle/
├── include/
│   └── carryhandle/
├── source/
├── examples/
├── tools/
├── docs/
├── README.md
├── COMPILING.md
├── TECHNICAL.md
└── DEVELOPMENT.md
```

The public API lives under:

```text
include/carryhandle/
```

Implementations live under:

```text
source/
```

Each major module should eventually have a corresponding example.

## Using CarryHandle

The initial distribution model is **vendored source/library usage**.

A project may keep CarryHandle beside or inside its own source tree,
including through a Git submodule if desired.

For example:

```text
mygame/
├── carryhandle/
├── source/
├── data/
└── Makefile
```

Installing CarryHandle globally into the devkitPro environment is not a
requirement.

A conventional installation mechanism may be added later.

## Platform

Initial target:

- Nintendo GameCube
- devkitPPC
- libogc2

Wii support and other platforms are outside the initial project scope.

## Current status

CarryHandle is in early development.

The initial API and architecture are being extracted from platform code
proven in DoomCube, but Doom-specific behavior is deliberately excluded.

See:

- [COMPILING.md](COMPILING.md)
- [TECHNICAL.md](TECHNICAL.md)
- [DEVELOPMENT.md](DEVELOPMENT.md)
