# CarryHandle Technical Architecture

## Purpose

CarryHandle provides reusable GameCube platform components above
devkitPPC and libogc2 without becoming a game engine.

It exists to package patterns that multiple unrelated GameCube
applications can reasonably reuse.

A feature does not belong in CarryHandle merely because DoomCube uses
it.

It belongs in CarryHandle when the functionality makes sense for a
second unrelated GameCube application.

## Design principles

### 1. GameCube-first

The initial target is the Nintendo GameCube.

CarryHandle should expose useful GameCube concepts rather than hiding
them behind abstractions designed for hypothetical platforms.

### 2. C-first

The public API is written in C.

The framework should remain usable from ordinary devkitPPC C projects.

### 3. Small independent modules

Applications should be able to select the parts they need.

There must not be a requirement to initialize or link unrelated modules.

### 4. Explicit lifecycle

Resource ownership and initialization should be visible to the caller.

Hidden initialization and invisible global framework state should be
avoided.

### 5. Application-owned main loop

CarryHandle does not own the application's main loop.

No callback-driven framework lifecycle is required.

### 6. No mandatory renderer

CarryHandle is not a rendering engine.

Where video helpers are useful, they should expose GameCube-specific
setup or presentation functionality without inventing a cross-platform
graphics abstraction.

### 7. No mandatory SDL dependency

SDL may be useful to individual modules or applications.

It must not become an unconditional dependency of low-level CarryHandle
functionality.

### 8. Do not wrap libogc2 without adding value

A `CH_` function should not exist solely to rename a perfectly adequate
one-line libogc2 call.

Wrappers are justified when CarryHandle provides one or more of:

- safer semantics
- useful lifecycle management
- reusable policy
- portability across GameCube application structures
- otherwise repetitive setup
- proven workarounds
- meaningful higher-level functionality

### 9. Proven behavior beats speculative abstraction

CarryHandle originates from code exercised by a real application.

Extraction should preserve proven behavior first and generalize only
where the application-specific dependency is understood.

## Architectural layers

The intended structure is:

```text
Application
    │
    ▼
CarryHandle convenience / higher-level modules
    │
    ▼
CarryHandle low-level GameCube modules
    │
    ▼
libogc2 and optional module-specific libraries
    │
    ▼
Nintendo GameCube hardware
```

Host-side tooling is separate:

```text
Application assets
    │
    ▼
CarryHandle host tools
    │
    ▼
GameCube disc image / generated artifacts
```

Host tools are not runtime library modules.

## Application metadata and shared build tooling

`carryhandle.cfg` is build-time application metadata. The GameCube executable
does not parse the INI file at runtime.

`tools/ch_manifest.py` validates the manifest and can generate immutable
application metadata consumed by `CH_ApplicationInfo`.

Manifest version 4 separates four concepts:

```text
[application]
    machine identity and native disc identity

[presentation]
    human-facing application/disc presentation

[persistence]
    persistent-storage namespace

[memcard]
    optional physical Memory Card policy and presentation overrides
```

The machine-identity fields remain distinct:

```text
game_code
    exactly 4 ASCII bytes
    application / Memory Card game code

disc_game_id
    exactly 2 ASCII bytes
    GameCube disc game identifier

company_code
    exactly 2 ASCII bytes
    maker / company identity

region
    NTSC-J, NTSC-U, or PAL
```

The native six-byte GameCube disc ID is derived as:

```text
"G"
+ disc_game_id
+ region character
+ company_code
```

with the region character:

```text
NTSC-J -> J
NTSC-U -> E
PAL    -> P
```

This separation is intentional. Memory Card identity, disc identity,
presentation, and persistence are different concepts and must not be conflated.

### Presentation metadata

Manifest v4 adds build-time `[presentation]` metadata:

```text
banner
    manifest-relative 96x32 PNG source

company
    human-facing company name

description
    human-facing application description
```

Presentation metadata is host/build metadata rather than generated runtime C
state.

For native GameCube images, `make/image.mk` invokes
`tools/native-gcm/ch_bnr.py` before the GCM builder walks the staged disc root.
The generated file is:

```text
opening.bnr
```

at the root of the native GameCube image.

The current disc encoder is deliberately strict and deterministic:

```text
format          BNR1
banner source   exactly 96x32
resizing        none
image encoding  tiled opaque RGB5A3
metadata        fixed-size NUL-padded ASCII fields
```

BNR2/localized metadata is deferred.

The same 96x32 source banner may also be used for Memory Card presentation, but
disc and Memory Card encoders remain separate because the on-card banner uses a
different CI8 + RGB5A3-palette representation.

Manifest v4 makes `[memcard]` optional. When it is present, `filename`,
`sectors`, and a 32x32 `icon` are required. Optional card-specific `title`,
`comment`, and `banner` values override defaults inherited from
`application.name`, `presentation.description`, and `presentation.banner`
respectively.

Manifest versions 1 through 3 retain their previous strict compatibility
behavior.

### Shared Make fragments

CarryHandle provides reusable build mechanics in:

```text
make/gamecube.mk
make/image.mk
make/dolphin.mk
```

Their ownership is:

```text
make/gamecube.mk
    shared GameCube compile mechanics
    selected CH_* source modules
    incremental/default parallel build policy

make/image.mk
    native GameCube GCM/FST packaging
    manifest-driven disc identity
    manifest-driven BNR1 presentation generation

make/dolphin.mk
    launch of an existing image
    incremental build/package/test workflow
```

Applications own their engine sources, assets, staging rules, release policy,
and application-specific test semantics.

CarryHandle owns reusable platform and build mechanics.

### Native image tooling

Native GameCube image construction is host-side CarryHandle tooling rather
than a runtime library service.

The application provides:

```text
compiled DOL
disc-root staging tree
carryhandle.cfg
```

CarryHandle provides:

```text
apploader
native GCM/FST builder
manifest validation
BNR1 presentation encoder
shared image Make rules
```

The native image builder writes the GameCube boot header, BI2 region data, DOL
location, FST, and staged file data.

Runtime DVD/FST access is a separate concern from host-side image creation.

### Dependency pinning

The canonical consumer layout keeps CarryHandle as a Git submodule at:

```text
deps/carryhandle
```

The parent repository records a `160000` gitlink to the exact CarryHandle
commit proven by that application.

This makes the dependency reproducible while still allowing a developer to
override `CARRY_ROOT` explicitly when testing a separate CarryHandle checkout.

A consumer must not depend on a sibling `../carryhandle` checkout as its normal
build configuration.

## Public API

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

Public declarations live below:

```text
include/carryhandle/
```

Private headers remain in the implementation tree and must not become
part of the supported API accidentally.

## Umbrella header

CarryHandle may provide:

```c
#include <carryhandle/carryhandle.h>
```

for convenience.

Individual modules should also remain directly includable.

For example:

```c
#include <carryhandle/ch_rumble.h>
```

An application should not be forced to include the entire framework.

## Initial module map

The following are candidate modules rather than promises of final API.

### Version

```text
CH_Version
```

Framework version information.

This is the first bootstrap module.

### System

Possible responsibilities:

- genuinely reusable application/platform initialization
- shutdown helpers
- platform information

This module must not become a mandatory global framework manager.

### Log

Possible responsibilities:

- GameCube/Dolphin diagnostic output
- consistent optional logging helpers

### Input

Possible responsibilities:

- controller polling helpers
- normalized controller state where CarryHandle adds value

Raw libogc2 input must remain usable directly.

### Rumble

Possible responsibilities:

- safe motor state management
- timed effects
- wall-clock sequencing

Game-specific decisions such as "shotgun means this rumble pattern" do
not belong here.

### DVD

Possible responsibilities:

- native GameCube FST file lookup
- aligned physical reads
- reusable file access semantics

This module is a strong extraction candidate because native DVD
alignment requirements are platform behavior rather than Doom behavior.

### Card

Possible responsibilities:

- Memory Card mounting
- file operations
- metadata helpers
- safer reusable primitives

### Storage

Potential higher-level module built above Card.

Possible responsibilities:

- transactional persistence
- redundant records
- generation handling
- recovery

This should be considered separately from raw Memory Card access.

Doom save-slot identity and IWAD/PWAD identity do not belong here.

### Video

Scope remains intentionally narrow.

Potential responsibilities include common GameCube display setup or
presentation helpers where they remove meaningful repeated work.

CarryHandle will not implement a universal renderer abstraction.

### Disc tooling

Host-side tooling for:

- apploader handling
- FST generation
- native GCM image construction

This is not a runtime C module.

## Module dependency rules

Dependencies should flow downward.

For example:

```text
CH_Storage
    ↓
CH_Card
    ↓
libogc2
```

Low-level modules must not depend on higher-level modules.

Circular module dependencies are not acceptable.

## Global state

Global state should be minimized.

Where a module inherently represents global GameCube hardware state,
that fact should be explicit rather than hidden behind artificial object
models.

CarryHandle will not create a giant framework context solely for the
appearance of abstraction.

## Error handling

The final error model is not yet frozen.

Initial direction:

- simple success/failure where sufficient
- explicit CarryHandle error/status enums where callers need meaningful
  diagnostics
- no process termination from reusable library code for recoverable
  errors

Host-side tools may use conventional command-line exit codes.

## Memory allocation

Modules should document ownership.

An API returning or accepting memory must make clear:

- who allocates it
- who frees it
- required alignment
- required lifetime

GameCube cache and DMA alignment constraints must never be hidden when
they matter to correctness.

## Threading

CarryHandle should not assume the application is single-threaded when a
module itself requires a worker.

Modules creating threads or workers must:

- document them
- own their shutdown
- avoid leaking application policy into the worker

## Compatibility

Before the first stable CarryHandle release, APIs may change as modules
are extracted and tested.

Once a stable API is published, breaking changes should require an
appropriate version change.

## Versioning

CarryHandle starts development at:

```text
0.0.0-dev
```

Version information is exposed through the `CH_` API.

The public release/versioning policy will be finalized before the first
tagged release.
