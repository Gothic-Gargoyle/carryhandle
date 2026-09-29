# CarryHandle Development

This document is for contributors changing CarryHandle itself. Consumer setup
belongs in [`GETTING_STARTED.md`](GETTING_STARTED.md).

## Core rule

A feature does not belong in CarryHandle merely because one consumer uses it.
It belongs when a second unrelated GameCube application could reasonably use
the same platform mechanism.

## Ownership test

Before adding code, ask:

1. Is this reusable GameCube platform behavior?
2. Can the behavior be expressed without consumer-specific policy?
3. Can it be tested independently?
4. Does CarryHandle add value beyond renaming a libogc2 call?

If the answer is no, keep it in the consumer.

The architectural ownership boundary is defined in
[`docs/architecture.md`](docs/architecture.md).

## Development strategy

CarryHandle is developed from proven needs in real applications:

```text
consumer need
    ↓
identify reusable platform behavior
    ↓
understand dependencies
    ↓
classify
    ↓
remove application policy
    ↓
define/refine CH_* boundary
    ↓
implement and prove in CarryHandle
    ↓
dogfood in independent consumers
    ↓
pin the proven CarryHandle revision
```

Do not begin by copying an entire consumer platform directory.

## Extraction classifications

### EXTRACT

Implementation is already largely generic GameCube functionality and can move
with naming/interface cleanup.

### ADAPT

Implementation contains reusable platform functionality but is coupled to
consumer globals, data structures, lifecycle or policy. Separate those first.

### LEAVE

The behavior is application-specific.

Examples include:

- game menu behavior
- game/content identity policy
- game-specific save-slot semantics
- game input bindings
- game-event-to-rumble mappings
- engine-specific asset/music conversion policy
- game rules and renderer policy

## Provenance

Before copying source, determine whether it is:

- newly written GameCube platform code
- modified upstream engine/game code
- adapted third-party code
- generated code

Preserve appropriate provenance and licensing. Prefer extracting independently
authored reusable logic over carrying unrelated upstream material into
CarryHandle.

## Public API rules

Public declarations live in:

```text
include/carryhandle/
```

Implementation lives in:

```text
source/
```

Public symbols use the `CH_` namespace. Implementation-only helpers remain
private.

Each substantial module should have:

1. a focused public header
2. implementation with minimal unrelated dependencies
3. documented lifecycle and ownership
4. an isolated example or regression proof where practical
5. no consumer-specific policy

## Repository layout

```text
carryhandle/
├── include/carryhandle/   public C API
├── source/                runtime implementation
├── make/                  reusable Make fragments
├── tools/                 host-side tooling
├── examples/              small consumer examples
├── experiments/           focused development proofs
├── docs/                  architecture and subsystem documentation
├── README.md              project front door
├── GETTING_STARTED.md     consumer setup and workflow
├── DEVELOPMENT.md         contributor workflow
└── TECHNICAL.md           technical documentation index
```

Experiments are evidence and development scaffolding; they are not automatically
public API or consumer workflow.

## Build requirements

Core development requirements:

```text
devkitPro
devkitPPC
libogc2
GNU Make
Python 3
```

Low-level modules should not acquire an unrelated dependency merely because one
consumer already links it.

## Build examples

Repository-level examples:

```bash
make
make examples
make hello
```

Direct hello build:

```bash
make -C examples/hello
```

Generated products belong under build directories, not beside source files.
Prefer incremental builds; use `make clean` when a clean rebuild is actually
part of the test.

## Test progression

Prove changes at the narrowest useful level first:

```text
syntax / host-tool validation
        ↓
isolated example or focused proof
        ↓
consumer compile
        ↓
consumer image build
        ↓
Dolphin runtime proof
        ↓
real GameCube hardware where relevant
```

Shared Make rules, native image tooling, manifests, input, video, DVD/FST,
Memory Card and persistence changes should be exercised by more than one
unrelated consumer whenever practical.

Passing in one consumer does not prove that a supposedly generic change is
application-independent.

Hardware-sensitive behavior such as DVD alignment, cache management, video,
controller operation, rumble and Memory Card access requires hardware
validation when the change can affect it.

## Developing against a local checkout

Consumers normally use their pinned `deps/carryhandle` submodule. During
CarryHandle development, point the consumer explicitly at the working checkout:

```bash
make CARRY_ROOT=/path/to/carryhandle test
```

Do not dirty or silently replace the consumer's pinned dependency merely to
perform framework development.

## Examples and regression oracles

Examples are part of API design. If an API cannot be demonstrated cleanly in a
small example, that is evidence the boundary may be too complicated.

Real consumers are regression oracles for already-proven behavior. They help
answer:

- what hardware behavior is actually required?
- which alignment/cache constraints matter?
- which lifecycle survives real application usage?
- which failure cases have already appeared?
- is the API reusable across unrelated engines?

CarryHandle must not remain source-coupled to a consumer after the reusable
mechanism has been extracted.

## Scope discipline

CarryHandle must resist becoming a dumping ground. Prefer functionality that
is already needed, demonstrably reusable, independently testable and narrow
enough to document clearly.

## Documentation rule

Documentation follows the same modularity rule as code:

- `README.md`: short project front door
- `GETTING_STARTED.md`: consumer setup and operational path
- `DEVELOPMENT.md`: work on CarryHandle itself
- `TECHNICAL.md`: index, not an encyclopedia
- `docs/architecture.md`: ownership and design rationale
- `docs/<subsystem>.md`: subsystem-specific details

When one document accumulates several independent subsystems or becomes hard to
navigate, split it rather than growing another wall of text.

## Commits

Prefer small commits corresponding to coherent milestones. Avoid commits that
simultaneously introduce unrelated subsystems.
