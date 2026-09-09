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

CarryHandle is intended to live inside the consuming application's source
tree as a **pinned Git submodule**.

The canonical layout is:

```text
mygame/
├── assets/
│   └── presentation/
│       ├── banner.png
│       └── card-icon.png
├── carryhandle.cfg
├── deps/
│   └── carryhandle/
├── source/
└── Makefile
```

Clone a consumer together with its pinned CarryHandle revision using:

```bash
git clone --recurse-submodules <repository>
```

For an existing checkout:

```bash
git submodule update --init --recursive
```

Installing CarryHandle globally into the devkitPro environment is not
required.

### Application manifest

Applications using CarryHandle's shared application and image tooling provide
a project-level `carryhandle.cfg`.

Manifest version 4 separates machine identity, human-facing presentation,
persistent-storage identity, and optional Memory Card policy:

```ini
[carryhandle]
manifest_version = 4

[application]
name = My Game
game_code = MYGM
disc_game_id = MG
company_code = CH
region = PAL

[presentation]
banner = assets/presentation/banner.png
company = My Studio
description = My Game for Nintendo GameCube

[persistence]
store_id = my-game

[memcard]
filename = MYGAME
sectors = 8
icon = assets/presentation/card-icon.png
```

The sections have deliberately different ownership:

| Section | Purpose |
| --- | --- |
| `[application]` | Machine identity and disc identity |
| `[presentation]` | Human-facing disc/application presentation |
| `[persistence]` | CarryHandle persistent-storage namespace |
| `[memcard]` | Optional physical Memory Card policy and overrides |

The main identity fields are:

| Field | Purpose |
| --- | --- |
| `name` | Human-readable application name; max 31 ASCII bytes in v4 |
| `game_code` | Exactly 4 ASCII bytes; application/Memory Card game code |
| `disc_game_id` | Exactly 2 ASCII bytes; GameCube disc game identifier |
| `company_code` | Exactly 2 ASCII bytes; maker/company identity |
| `region` | `NTSC-J`, `NTSC-U`, or `PAL` |
| `store_id` | CarryHandle persistence namespace |

The six-byte native GameCube disc ID remains:

```text
G + disc_game_id + region character + company_code
```

with the region character:

```text
NTSC-J -> J
NTSC-U -> E
PAL    -> P
```

`company_code` is the two-byte machine identity used in native GameCube
metadata. It is deliberately separate from `presentation.company`, which is
human-facing text shown in presentation metadata such as `opening.bnr`.

Presentation v4 requires:

```text
banner
    manifest-relative PNG path
    exactly 96x32 pixels
    no automatic resize or crop

company
    human-readable company name
    max 31 ASCII bytes

description
    human-readable description
    max 127 ASCII bytes
```

For native GameCube images CarryHandle converts the presentation banner into a
root-level `opening.bnr`. The current encoder produces deterministic BNR1
presentation data. BNR2/localized metadata is not currently generated.

`[presentation]` does **not** imply Memory Card usage. Applications which do not
use CarryHandle's Memory Card policy may omit `[memcard]` entirely.

When `[memcard]` is present, v4 requires `filename`, `sectors`, and a 32x32
`icon`. Card-specific presentation values are optional overrides:

```text
title
    defaults to application.name

comment
    defaults to presentation.description

banner
    defaults to presentation.banner
```

The canonical source-asset layout is:

```text
assets/presentation/banner.png
assets/presentation/card-icon.png
```

CarryHandle does not silently resize presentation art. A disc/card banner must
be exactly 96x32 pixels and a card icon exactly 32x32 pixels.

The `examples/hello/assets/presentation/` directory contains CarryHandle-branded
96x32 banner and 32x32 card-icon placeholder assets. They can be copied as a
starting point and replaced with application artwork.

`game_code` and `disc_game_id` remain deliberately separate. Existing
applications should preserve their Memory Card game/company identity so that
existing card data remains accessible.

Manifest versions 1 through 3 remain supported for existing consumers with
their previous strict behavior. New consumers should use manifest version 4.

Validate a manifest with:

```bash
python3 deps/carryhandle/tools/ch_manifest.py carryhandle.cfg
```

### Shared GameCube build contract

CarryHandle's shared Make fragments provide the normal consumer workflow:

```text
make / make all   incremental compile
make iso          incremental compile + native GameCube image
make dolphin      launch the existing image without rebuilding
make test         incremental compile + image + Dolphin
make run          compile + configured hardware launch
make clean        explicitly remove generated output
make help         show available targets
```

CarryHandle automatically uses the available processor count when the caller
has not supplied another GNU Make job setting.

Override the automatic default with:

```bash
make CH_JOBS=4
```

An explicit GNU Make job setting still takes precedence:

```bash
make -j2
```

Routine development should use incremental builds. `make clean` and forced
rebuilds are diagnostic or destructive operations rather than normal build
steps.

### Consumer integration

A conventional consumer points CarryHandle at its pinned submodule:

```make
CARRY_ROOT ?= $(abspath $(PROJECT_DIR)/deps/carryhandle)
```

Applications select the CarryHandle modules they need and include the shared
Make fragments from `$(CARRY_ROOT)/make/`.

During CarryHandle development, a consumer can explicitly point at a separate
working checkout:

```bash
make CARRY_ROOT=/path/to/carryhandle test
```

The checked-in submodule should remain pinned to a published, tested
CarryHandle commit.

### Updating CarryHandle in a consumer

Move the submodule deliberately to the CarryHandle revision being adopted:

```bash
git -C deps/carryhandle fetch origin
git -C deps/carryhandle checkout <commit>
git add deps/carryhandle
```

The parent repository then records that exact CarryHandle revision as its
submodule gitlink.


### Release publication

CarryHandle can publish an **already-built, already-tested and already-tagged**
consumer release to GitHub or GitLab. The consumer still owns how the release
artifact is built, when it is accepted, the release notes, and creation/push of
the Git tag.

Publication support is opt-in:

```make
include $(CARRY_ROOT)/make/release.mk
```

GitHub publication requires an authenticated `gh` CLI. GitLab publication
requires an authenticated `glab` CLI.

A normal release flow is:

```bash
make release
# Test the artifact, including real hardware where relevant.
# Create and push the release tag.

make publish-release-check \
    RELEASE_TAG=v1.2.3 \
    RELEASE_ASSETS=dist/mygame-v1.2.3.zip \
    RELEASE_NOTES=release/v1.2.3.md

make publish-release \
    RELEASE_TAG=v1.2.3 \
    RELEASE_ASSETS=dist/mygame-v1.2.3.zip \
    RELEASE_NOTES=release/v1.2.3.md
```

Run the publication steps from the exact commit targeted by the release tag.
`publish-release-check` is read-only. `publish-release` refuses to update an
existing release and verifies the published assets by downloading them again
and comparing their SHA-256 hashes.

See [TECHNICAL.md](TECHNICAL.md) for the complete publication safety contract,
provider behavior, release-time variables, dirty-tree policy, verification
rules and failure modes.

## Platform

Initial target:

- Nintendo GameCube
- devkitPPC
- libogc2

Wii support and other platforms are outside the initial project scope.

## Current status

CarryHandle is in active development and is dogfooded by multiple independent
GameCube ports, including DoomCube and Quake2Cube.

Reusable platform behavior belongs in CarryHandle; engine-specific policy
remains in the consuming application.

See:

- [TECHNICAL.md](TECHNICAL.md)
- [DEVELOPMENT.md](DEVELOPMENT.md)
