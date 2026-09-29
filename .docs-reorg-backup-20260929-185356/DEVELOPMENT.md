# CarryHandle Development

## Core rule

A feature does not belong in CarryHandle merely because DoomCube uses
it.

A feature belongs in CarryHandle when a second unrelated GameCube
application could reasonably use it.

## Development strategy

CarryHandle is being developed by extracting reusable GameCube platform
work proven during DoomCube development.

The extraction process is:

```text
identify
    ↓
understand dependencies
    ↓
classify
    ↓
remove application policy
    ↓
define CH_* API
    ↓
extract
    ↓
build isolated example
    ↓
test
```

Do not begin by copying the entire DoomCube platform directory.

## Extraction classifications

Every DoomCube candidate should be classified as one of:

### EXTRACT

The implementation is already largely generic GameCube functionality.

It may be moved into CarryHandle with naming and interface cleanup.

### ADAPT

The implementation contains reusable GameCube functionality but is
coupled to DoomCube policy, globals, data structures, or lifecycle.

The generic behavior should be separated before entering CarryHandle.

### LEAVE

The functionality is application-specific and does not belong in the
framework.

Examples include:

- Doom menu behavior
- IWAD/PWAD identity policy
- Doom save-slot semantics
- Doom input bindings
- game-event-to-rumble mappings
- Doom music conversion policy

## Provenance

Code extracted from DoomCube must retain appropriate provenance and
licensing information.

Before copying a source file, determine whether it is:

- newly written GameCube platform code
- modified upstream Doom/DoomGeneric code
- adapted third-party code
- generated code

Do not copy a mixed source file wholesale merely because part of it is
useful.

Where practical, extract the independently authored reusable logic
rather than carrying unrelated upstream material into CarryHandle.

## Scope discipline

CarryHandle must resist becoming a dumping ground.

Do not add functionality because it might hypothetically be useful.

Prefer functionality that is:

- already needed by a real GameCube application
- demonstrably reusable
- testable independently
- narrow enough to document clearly

## Public API rules

Public API:

```text
include/carryhandle/
```

Implementation:

```text
source/
```

Public symbols use:

```text
CH_
```

Implementation-only helpers should remain private.

Do not expose internal functions in a public header simply to make
development convenient.

## Module rules

Each substantial module should have:

1. a focused public header
2. an implementation with minimal external dependencies
3. documented lifecycle and ownership
4. an isolated example or regression test
5. no application-specific policy

## Building and testing CarryHandle

CarryHandle uses the normal devkitPro GameCube toolchain rather than a
private compiler environment.

Core development requirements are:

```text
devkitPro
devkitPPC
libogc2
GNU Make
Python 3
```

Optional modules or consumers may add their own dependencies. A low-level
CarryHandle module should not acquire an unrelated dependency merely because
one consumer already links it.

### Building examples

The repository root provides ordinary Make orchestration:

```bash
make
make examples
make hello
```

The hello example can also be built directly:

```bash
make -C examples/hello
```

Generated build products belong below build directories rather than alongside
source files.

Normal development should use incremental builds. `make clean` and forced
rebuilds are diagnostic or destructive operations and should not be routine
prerequisites.

### Testing shared changes

Framework changes should be proven at the narrowest useful level first and
then through real consumers when they affect shared behavior.

A useful progression is:

```text
syntax / host-tool validation
        ↓
isolated example
        ↓
consumer compile
        ↓
consumer image build
        ↓
Dolphin runtime proof
        ↓
real GameCube hardware where relevant
```

Changes to shared Make fragments, native GCM tooling, manifests, input,
video, DVD/FST, Memory Card, persistence, or other cross-application services
should be exercised by more than one unrelated consumer whenever practical.

DoomCube and Quake2Cube are current dogfood consumers. Passing in one does not
by itself establish that a supposedly generic change is application
independent.

Real-hardware-sensitive behavior such as DVD alignment, cache management,
video presentation, controller operation, rumble, and Memory Card access
requires hardware validation when a change can affect it.

CarryHandle's Swiss-launched image-backed DVD path and native disc
presentation generation have been validated through DoomCube on a real
Nintendo GameCube using:

```text
PicoLoader -> Swiss -> SD Gecko -> DoomCube .iso
```

That proof covers the Swiss-launched `.iso` path from SD, including image-backed
game-data reads and generated disc presentation. It does not by itself establish
support for burned optical discs, other ODEs/loaders, other Swiss storage
devices, direct-DOL launch behavior, or real-hardware Memory Card writes.

### Developing against a local CarryHandle checkout

A consumer normally uses its pinned `deps/carryhandle` submodule.

During CarryHandle development, point the consumer explicitly at the working
CarryHandle checkout rather than rewriting or dirtying the submodule. The
canonical shared variable is:

```bash
make CARRY_ROOT=/path/to/carryhandle test
```

A consumer may expose a project-specific wrapper variable which ultimately
sets `CARRY_ROOT`.

## Examples

Examples are part of the framework design process.

An API that cannot be demonstrated cleanly in a tiny example is a sign
that the API may be too complicated.

Planned examples include:

```text
hello
input
rumble
dvd
card
storage
```

Examples should demonstrate CarryHandle rather than reimplementing large
applications.

## Consumer ports as regression oracles

CarryHandle is developed against real applications rather than speculative
framework use cases.

DoomCube and Quake2Cube are current regression oracles for behavior that has
already been proven in application development.

Consumer ports can answer questions such as:

- what hardware behavior is actually required?
- which alignment or cache constraints matter?
- which lifecycle survives real application usage?
- which failure cases have already been encountered?
- whether an API is genuinely reusable across unrelated engines

CarryHandle should not remain source-coupled to any consumer after reusable
behavior has been extracted.

Application policy remains in the application repository. CarryHandle owns
only the reusable GameCube platform mechanism.

## Commits

Prefer small commits corresponding to coherent milestones.

Examples:

```text
Bootstrap CarryHandle architecture
Add GameCube hello example
Add controller input module
Add rumble sequencer
Add native DVD access
```

Avoid giant commits that simultaneously extract multiple subsystems.

## Current development direction

CarryHandle has moved beyond its bootstrap/extraction-only phase and is now
consumed by multiple independent GameCube ports.

New framework work should therefore be driven by demonstrated reusable needs
across consumers rather than by the historical bootstrap milestone order.

The preferred cycle is:

```text
consumer need
    ↓
identify reusable platform behavior
    ↓
define or refine the CH_* boundary
    ↓
prove the CarryHandle implementation
    ↓
dogfood it in independent consumers
    ↓
pin the proven CarryHandle revision
```

Application-specific renderer policy, game rules, asset policy, save
serialization, and release semantics remain in the application repositories.
