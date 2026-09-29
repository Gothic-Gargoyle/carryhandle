# Getting Started

This guide is for application developers consuming CarryHandle. If you are
changing CarryHandle itself, use [`DEVELOPMENT.md`](DEVELOPMENT.md).

## 1. Requirements

Core requirements:

- devkitPro
- devkitPPC
- libogc2
- GNU Make
- Python 3
- Git

Optional modules and consumers may add their own dependencies.

## 2. Add CarryHandle as a pinned submodule

The canonical layout is:

```text
mygame/
├── assets/
├── carryhandle.cfg
├── deps/
│   └── carryhandle/
├── source/
└── Makefile
```

Add CarryHandle:

```bash
git submodule add https://github.com/Gothic-Gargoyle/carryhandle deps/carryhandle
git submodule update --init --recursive
```

Clone an existing consumer together with its pinned CarryHandle revision:

```bash
git clone --recurse-submodules <repository>
```

CarryHandle does not need to be installed globally into devkitPro.

## 3. Point the consumer at CarryHandle

A conventional consumer uses:

```make
CARRY_ROOT ?= $(abspath $(PROJECT_DIR)/deps/carryhandle)
```

Select only the modules and Make fragments your application needs. CarryHandle
is intentionally modular; using DVD access does not require adopting
persistence, video, rumble, or presentation helpers.

During CarryHandle development, a consumer may explicitly override the pinned
submodule:

```bash
make CARRY_ROOT=/path/to/carryhandle test
```

Do not make a sibling checkout such as `../carryhandle` the normal build
configuration.

## 4. Create `carryhandle.cfg`

New applications should use manifest version 4:

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

The sections deliberately describe different concepts:

| Section | Purpose |
| --- | --- |
| `[application]` | machine identity and native disc identity |
| `[presentation]` | human-facing disc/application presentation |
| `[persistence]` | persistent-storage namespace |
| `[memcard]` | optional physical Memory Card policy and presentation overrides |

Important identity fields:

| Field | Meaning |
| --- | --- |
| `name` | human-readable application name |
| `game_code` | exactly 4 ASCII bytes; application/Memory Card identity |
| `disc_game_id` | exactly 2 ASCII bytes; native disc game identifier |
| `company_code` | exactly 2 ASCII bytes; maker/company identity |
| `region` | `NTSC-J`, `NTSC-U`, or `PAL` |
| `store_id` | CarryHandle persistence namespace |

The six-byte native GameCube disc ID is:

```text
G + disc_game_id + region character + company_code
```

with:

```text
NTSC-J -> J
NTSC-U -> E
PAL    -> P
```

`game_code`, `disc_game_id`, presentation metadata and `store_id` are separate
on purpose. In particular, existing applications should preserve their Memory
Card `game_code`/`company_code` identity when changing disc or presentation
metadata so existing card data remains accessible.

Validate the manifest with:

```bash
python3 deps/carryhandle/tools/ch_manifest.py carryhandle.cfg
```

Manifest versions 1 through 3 remain supported for existing consumers with
their previous strict behavior.

## 5. Presentation assets

Manifest v4 presentation expects:

```text
banner
    manifest-relative PNG
    exactly 96x32 pixels

company
    human-readable company name
    max 31 ASCII bytes

description
    human-readable description
    max 127 ASCII bytes
```

When `[memcard]` is present, `filename`, `sectors`, and a 32x32 `icon` are
required. Optional Memory Card `title`, `comment`, and `banner` values override
defaults inherited from the application/presentation sections.

A conventional source layout is:

```text
assets/presentation/banner.png
assets/presentation/card-icon.png
```

CarryHandle deliberately does not silently resize these assets.

See [`docs/presentation.md`](docs/presentation.md).

## 6. Build

CarryHandle's shared Make fragments support the normal consumer workflow:

```text
make / make all   incremental compile
make iso          incremental compile + native GameCube image
make dolphin      launch the existing image without rebuilding
make test         incremental compile + image + Dolphin
make run          compile + configured hardware launch
make clean        remove generated output explicitly
make help         show available targets
```

CarryHandle uses available processors automatically unless the caller supplies
another GNU Make job setting.

Override the automatic default with:

```bash
make CH_JOBS=4
```

An explicit GNU Make job setting still wins:

```bash
make -j2
```

Routine development should use incremental builds. `make clean` and forced
rebuilds are diagnostic/destructive operations, not normal prerequisites.

## 7. Run on real hardware

A consumer may expose one or more development paths:

```bash
make run
make run-hardware-sd
make run-hardware-remote
```

Typical meanings are:

- `make run`: consumer's normal USB Gecko/Swiss hardware launch
- `make run-hardware-sd`: DOL launch with an SD directory exposed as `dvd:/`
- `make run-hardware-remote`: DOL launch with a PC-hosted image supplying `dvd:/`

The target names and exact paths beyond CarryHandle's generic primitives are
consumer-owned.

See:

- [`docs/dvd-and-filesystems.md`](docs/dvd-and-filesystems.md)
- [`docs/remote-development.md`](docs/remote-development.md)

## 8. Persistent application data

When a consumer uses the generated application-save descriptor, the
`CH_ApplicationSave*` API can own the common Memory Card lifecycle while the
application owns the logical data stored inside it.

For frequently saved configuration, prefer write-if-changed behavior where
appropriate:

```c
CH_ApplicationSavePutIfChanged(...);
```

This avoids unnecessary physical Memory Card writes when the object already
contains identical bytes.

See [`docs/persistence.md`](docs/persistence.md) and
[`docs/storage.md`](docs/storage.md).

## 9. Update CarryHandle deliberately

Move the submodule to the exact revision being adopted:

```bash
git -C deps/carryhandle fetch origin
git -C deps/carryhandle checkout <commit>
git add deps/carryhandle
```

The parent repository records that exact commit as its submodule gitlink.

## 10. Release a consumer

Release support is opt-in:

```make
include $(CARRY_ROOT)/make/release.mk
```

The consumer provides `release-bundle`; CarryHandle provides the generic outer
lifecycle.

The normal workflow is:

```bash
# Build/test the release candidate, including real hardware where relevant.
make run-hardware-sd VERSION=X.Y.Z

# Build the accepted player package and create the annotated local tag.
make release RELEASE_TAG=vX.Y.Z

# Atomically push current branch + tag.
make release-push RELEASE_TAG=vX.Y.Z

# Validate forge publication without mutation.
make publish-release-check \
    RELEASE_TAG=vX.Y.Z \
    RELEASE_ASSETS="dist/mygame-vX.Y.Z.zip" \
    RELEASE_NOTES="release/vX.Y.Z.md"

# Publish and verify fresh remote downloads.
make publish-release \
    RELEASE_TAG=vX.Y.Z \
    RELEASE_ASSETS="dist/mygame-vX.Y.Z.zip" \
    RELEASE_NOTES="release/vX.Y.Z.md"
```

The hardware-test command is an example consumer workflow, not a generic
CarryHandle requirement. The application decides what acceptance means.

Read [`docs/release-workflow.md`](docs/release-workflow.md) before publishing a
real release.

## Troubleshooting

### The consumer sees the wrong CarryHandle revision

Check the gitlink and initialize the exact pinned submodule:

```bash
git submodule status
git submodule update --init --recursive
```

### A development build shows a `-dev` version

Consumer version policy is consumer-owned. Release orchestration passes
`VERSION` derived from `RELEASE_TAG` to the consumer's `release-bundle` hook,
but ordinary development targets may intentionally use a separate development
version.

### `publish-release-check` refuses the tree

Do not weaken the check first. Verify that:

- the local tag resolves to `HEAD`
- the remote tag resolves to the same commit
- staged tracked changes are absent
- tracked unstaged changes are absent unless explicitly allowed
- release assets and notes exist

See [`docs/release-workflow.md`](docs/release-workflow.md).
