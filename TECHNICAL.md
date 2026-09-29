# CarryHandle Technical Reference

This file is the technical map of CarryHandle. Detailed subsystem behavior
lives in focused documents instead of one monolithic architecture file.

## Architecture

Start with [`docs/architecture.md`](docs/architecture.md).

It defines:

- CarryHandle vs consumer ownership
- deliberate non-goals
- design principles
- runtime and host-tool layers
- dependency direction
- lifecycle/global-state rules

## Application metadata

Build-time application identity comes from `carryhandle.cfg` and is validated
by `tools/ch_manifest.py`. Runtime immutable metadata is exposed through
`CH_ApplicationInfo`.

See:

- [`GETTING_STARTED.md`](GETTING_STARTED.md#4-create-carryhandlecfg)
- [`docs/architecture.md`](docs/architecture.md#application-metadata-boundary)
- `include/carryhandle/ch_application.h`

## DVD and filesystems

Native disc FST access, SD-directory backing and remote-image-backed `dvd:/`:

- [`docs/dvd-and-filesystems.md`](docs/dvd-and-filesystems.md)
- [`docs/remote-development.md`](docs/remote-development.md)
- `include/carryhandle/ch_dvd.h`
- `include/carryhandle/ch_remote_disc.h`

## Persistence and storage

Application-facing object persistence, save bundles and the application-save
lifecycle:

- [`docs/persistence.md`](docs/persistence.md)

Lower-level Memory Card, transactional storage, A/B metadata and durability:

- [`docs/storage.md`](docs/storage.md)

Primary headers:

- `ch_application_save.h`
- `ch_persist.h`
- `ch_save_bundle.h`
- `ch_save_bundle_persist.h`
- `ch_memcard.h`
- `ch_tx*.h`

## Input and controller presentation

- [`docs/input.md`](docs/input.md)
- `ch_input.h`
- `ch_input_physical.h`
- `ch_input_bindings.h`
- `ch_rumble.h`
- `ch_controller_glyph.h`

## Presentation

Disc/card presentation metadata, BNR generation, splash helpers and optional
SDL UI helpers:

- [`docs/presentation.md`](docs/presentation.md)

## Build and image tooling

Shared Make fragments live under `make/`. Native image construction is
host-side tooling rather than a runtime library service.

The consumer provides its compiled DOL, staged disc root and
`carryhandle.cfg`; CarryHandle provides reusable image/FST/BNR tooling.

See:

- [`GETTING_STARTED.md`](GETTING_STARTED.md#6-build)
- [`docs/architecture.md`](docs/architecture.md#host-tooling-boundary)

## Release lifecycle and publication

- [`docs/release-workflow.md`](docs/release-workflow.md)
- `make/release.mk`
- `tools/release/ch_release.py`

The release documentation contains the exact ownership split, tag lifecycle,
publication preflight, provider behavior, immutable-release policy and
fresh-download verification contract.

## Public API conventions

All public CarryHandle symbols use the `CH_` namespace.

Functions:

```c
CH_Foo();
```

Types:

```c
CH_Foo
CH_FooState
CH_FooConfig
```

Constants:

```c
CH_FOO_BAR
```

Public declarations live under `include/carryhandle/`. Private headers remain
implementation details.

## Umbrella header

Applications may use:

```c
#include <carryhandle/carryhandle.h>
```

Individual modules remain directly includable:

```c
#include <carryhandle/ch_rumble.h>
```

No consumer is required to include or initialize the complete framework.

## Error handling

Reusable library code should not terminate the process for recoverable errors.
Modules use simple success/failure where sufficient and explicit result/status
domains where callers need diagnostics.

Host-side tools use conventional command-line exit codes.

## Memory ownership

APIs that allocate, return or accept memory must make clear:

- who allocates it
- who frees it
- required alignment
- required lifetime

GameCube cache/DMA alignment requirements must remain explicit when correctness
depends on them.

## Threading

CarryHandle does not create a framework-wide threading model. A module that
creates a worker/thread must document it, own its shutdown, and keep consumer
policy out of the worker.

## Compatibility and versioning

CarryHandle remains in active development. Public API compatibility should be
considered deliberately as modules mature; breaking changes require an
appropriate version change once stable API versions are published.
