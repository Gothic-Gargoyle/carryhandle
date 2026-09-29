# CarryHandle

CarryHandle is a lightweight C framework and build-tooling layer for Nintendo
GameCube homebrew built around devkitPPC and libogc2.

It packages reusable GameCube platform mechanisms without becoming a game
engine or taking ownership of an application's main loop, game logic, renderer,
or project-specific policy.

CarryHandle grew out of reusable platform work proven in real GameCube ports,
including [DoomCube](https://github.com/Gothic-Gargoyle/doomcube).

## What CarryHandle provides

Current areas include:

- application metadata generated from `carryhandle.cfg`
- controller input, physical-input helpers, rumble and controller glyphs
- native `dvd:/` access
- SD-directory and USB Gecko remote-disc development backing for `dvd:/`
- Memory Card helpers, presentation and application-save lifecycle support
- transactional persistence and save bundles
- splash/presentation helpers
- native GameCube image and BNR tooling
- shared GameCube build rules
- generic release orchestration and verified forge publication

## What CarryHandle is not

CarryHandle is not a game engine, scene graph, ECS, physics engine, universal
renderer, SDL replacement, libogc2 replacement, or framework-owned main loop.

Applications remain in control of their own structure and policy. CarryHandle
wraps or extends libogc2 only where doing so provides reusable value.

For the exact ownership boundary, start with
[`docs/architecture.md`](docs/architecture.md).

## Quick start

CarryHandle is normally pinned as a Git submodule inside the consumer:

```bash
git submodule add https://github.com/Gothic-Gargoyle/carryhandle deps/carryhandle
git submodule update --init --recursive
```

A conventional project points its Makefile at the submodule:

```make
CARRY_ROOT ?= $(abspath $(PROJECT_DIR)/deps/carryhandle)
```

Consumer projects then select the CarryHandle modules and shared Make fragments
they need.

Common development commands are:

```bash
make
make run
```

Consumers may additionally provide hardware-development targets such as:

```bash
make run-hardware-sd
make run-hardware-remote
```

The exact SD path, runtime assets and consumer-specific targets remain owned by
the consuming project.

For a complete setup walkthrough, see
[`GETTING_STARTED.md`](GETTING_STARTED.md).

## Documentation

| Document | Use it when you want to... |
| --- | --- |
| [`GETTING_STARTED.md`](GETTING_STARTED.md) | add CarryHandle to an application and get from clone to build, hardware test and release |
| [`DEVELOPMENT.md`](DEVELOPMENT.md) | work on CarryHandle itself |
| [`TECHNICAL.md`](TECHNICAL.md) | find the technical reference for a subsystem |
| [`docs/architecture.md`](docs/architecture.md) | understand ownership boundaries and architectural decisions |
| [`docs/persistence.md`](docs/persistence.md) | use application saves, object persistence and save bundles |
| [`docs/storage.md`](docs/storage.md) | understand Memory Card, transaction storage and durability layers |
| [`docs/dvd-and-filesystems.md`](docs/dvd-and-filesystems.md) | use native, SD-backed or remote `dvd:/` |
| [`docs/input.md`](docs/input.md) | use controller input, physical mappings, rumble and glyphs |
| [`docs/presentation.md`](docs/presentation.md) | use disc/card presentation, splash helpers and presentation assets |
| [`docs/remote-development.md`](docs/remote-development.md) | develop on real hardware through USB Gecko/Swiss and SD backing |
| [`docs/release-workflow.md`](docs/release-workflow.md) | build, accept, tag, push and publish a release |

## Platform

Primary target:

- Nintendo GameCube
- devkitPPC
- libogc2

Other platforms are outside the initial project scope unless deliberately added.

## Status

CarryHandle is in active development and is dogfooded by independent GameCube
ports. Reusable platform behavior belongs in CarryHandle; engine- and
application-specific policy stays in the consuming repository.
