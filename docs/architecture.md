# CarryHandle Architecture

CarryHandle provides reusable GameCube platform mechanisms above devkitPPC and
libogc2 without becoming a game engine.

The most important architectural rule is ownership.

## Ownership boundaries

### CarryHandle owns

CarryHandle owns generic mechanisms that make sense across unrelated GameCube
applications, including where implemented:

- reusable GameCube platform primitives
- shared build mechanics
- manifest parsing and generated immutable application metadata
- native image/FST/BNR tooling
- controller/input primitives, rumble helpers and generic glyph vocabulary
- DVD/filesystem mechanisms
- Memory Card primitives and generic persistence machinery
- generic presentation helpers
- remote-development transport primitives
- generic release orchestration and publication verification

### The consumer owns

The consuming application owns policy and application meaning, including:

- game/application logic
- main-loop structure
- renderer policy
- application-specific UX
- game-specific input bindings and semantics
- game-event-to-rumble policy
- save schema/content and migration policy
- asset/content policy
- exact release-bundle contents
- release notes and release prose
- acceptance criteria and the decision that a release is ready

### CarryHandle deliberately does not own

CarryHandle deliberately does not become:

- a game engine
- a scene graph or ECS
- a physics engine
- a framework-owned main loop
- a universal renderer abstraction
- an SDL replacement
- a libogc2 replacement
- a dumping ground for one consumer's platform directory
- an authority that decides whether a consumer release passed acceptance

This boundary should be checked before adding every new subsystem or helper.

## Design principles

### GameCube-first

Expose useful GameCube concepts instead of hiding them behind abstractions for
hypothetical platforms.

### C-first

The public API is C and remains usable by ordinary devkitPPC C projects.

### Small independent modules

Consumers select the modules they need. Using one subsystem must not require
initializing unrelated ones.

### Explicit lifecycle

Initialization, shutdown, ownership and resource lifetime remain visible to the
caller. Hidden global framework initialization is avoided.

### Application-owned main loop

CarryHandle does not require a callback-driven lifecycle or take control of the
application's main loop.

### No mandatory renderer

Video/presentation helpers solve specific GameCube problems; they do not invent
a generic rendering engine.

### No mandatory SDL dependency

SDL may support optional modules. It must not become an unconditional
dependency of low-level CarryHandle functionality.

### Do not wrap libogc2 without adding value

A `CH_` wrapper is justified when it contributes real reusable behavior such
as safer semantics, lifecycle management, meaningful policy, repetitive setup,
proven workarounds or a higher-level operation.

### Proven behavior beats speculative abstraction

Start from behavior proven by real applications/hardware. Generalize only when
the application-specific dependency is understood.

## Runtime layers

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

Dependencies should flow downward. Circular dependencies are not acceptable.

Example:

```text
Application-save convenience
    ↓
Object persistence
    ↓
Transaction store
    ↓
Memory Card backend
    ↓
libogc2 CARD
```

## Host-tooling boundary

Host-side tooling is separate from the runtime C library:

```text
Application assets / metadata / executable
    │
    ▼
CarryHandle host tools and Make fragments
    │
    ▼
Generated GameCube image / release artifact
```

Host tools are not runtime modules and runtime APIs do not parse the project
manifest from disk.

## Application metadata boundary

`carryhandle.cfg` is build-time metadata. `tools/ch_manifest.py` validates it
and may generate immutable runtime metadata consumed by `CH_ApplicationInfo`.

Manifest v4 keeps distinct concepts distinct:

```text
[application]   machine identity / disc identity
[presentation]  human-facing presentation
[persistence]   logical persistent-storage namespace
[memcard]       optional physical Memory Card policy
```

These values should not be collapsed merely because one consumer happens to
use matching strings.

## Build ownership

CarryHandle Make fragments own reusable mechanics such as GameCube compilation,
image construction and generic release orchestration.

Consumers own:

- engine/application source selection
- game assets
- staging layout
- consumer-specific target semantics
- player package contents
- application acceptance/testing policy

## Release ownership

The release system follows the same boundary:

```text
consumer release-bundle
        ↓
CarryHandle generic release lifecycle
        ↓
human/emulator/hardware acceptance
        ↓
CarryHandle atomic branch+tag push
        ↓
CarryHandle forge publication + verification
```

See [`release-workflow.md`](release-workflow.md).

## Global state

Minimize global state. When a module represents inherently global GameCube
hardware state, make that explicit rather than hiding it behind artificial
object models.

CarryHandle does not create a giant framework context merely for the appearance
of abstraction.

## Memory and alignment

Module APIs must expose ownership, alignment and lifetime constraints when they
matter. GameCube cache, DMA, DVD and Memory Card alignment requirements are
correctness constraints, not implementation trivia.

## Error boundaries

Reusable modules return status to the caller rather than terminating the
application on recoverable failures. Higher-level convenience layers may map
lower-level result domains, but should retain enough diagnostic information for
callers that need it.

## Threading

No global threading model is imposed. Modules that create workers must own
their lifecycle and avoid consumer policy inside those workers.

## How to decide where new code belongs

A new feature belongs in CarryHandle when all of the following are reasonably
true:

1. an unrelated GameCube application could use it;
2. its API can be expressed without consumer-specific policy;
3. its dependencies fit the downward module graph;
4. it can be documented and tested as an independent mechanism;
5. CarryHandle adds meaningful value beyond a one-line wrapper.

Otherwise, keep it in the consumer until a reusable boundary becomes clear.
