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

## Testing

Whenever practical, test on:

1. build/toolchain
2. Dolphin
3. real GameCube hardware

Emulator success alone does not prove correct hardware behavior.

Known hardware requirements such as DVD alignment and cache management
must receive explicit tests.

## DoomCube as regression oracle

The released DoomCube implementation is a reference for behavior that
was already proven during development.

CarryHandle extraction may use DoomCube to answer questions such as:

- what hardware behavior was required?
- which alignment restrictions mattered?
- which lifecycle survived real application usage?
- which failure cases were discovered?

CarryHandle should not remain source-coupled to DoomCube after
extraction.

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

## First milestones

### Milestone 0 — Bootstrap

- project documentation
- source/include layout
- namespace
- version API

### Milestone 1 — Hello GameCube

- concrete Makefile infrastructure
- native GameCube executable
- simplest useful CarryHandle initialization
- controller-driven clean exit or idle loop
- Dolphin test

### Milestone 2 — DoomCube quarry map

Inventory GameCube-specific DoomCube code and classify each component as:

```text
EXTRACT
ADAPT
LEAVE
```

### Later milestones

Extract only one coherent subsystem at a time.

Current likely order:

1. input
2. rumble
3. DVD/FST
4. platform/video helpers
5. Memory Card primitives
6. transactional storage
7. native disc tooling

The order may change after the quarry analysis.
