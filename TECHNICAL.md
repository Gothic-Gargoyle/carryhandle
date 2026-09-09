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


### Release publication

Release publication is a host-side service. It is deliberately separated from
consumer build/package logic and from `carryhandle.cfg`.

The intended flow is:

```text
consumer build/package
        ↓
artifact acceptance
        ↓
tag creation + push
        ↓
CarryHandle publication preflight
        ↓
CarryHandle forge publication
        ↓
published-asset verification
```

The corresponding conventional commands are:

```text
make release
[human / emulator / hardware acceptance]
[create and push release tag]
make publish-release-check
make publish-release
```

`make release` is not defined by CarryHandle's publication layer. It remains an
application-owned target because different consumers package different runtime
files, assets, documentation and player tooling.

#### Ownership boundary

The consumer owns:

```text
release build/package procedure
artifact contents
decision that the artifact is accepted
release notes content
optional release title override
release tag creation
release tag push
bundle README and other application-specific prose
```

CarryHandle owns:

```text
publication preflight
source / local-tag / remote-tag consistency checks
forge provider selection
provider authentication/access checks at publication time
existing-release refusal
upload of explicitly supplied assets
post-publication metadata verification
fresh download of every published asset
SHA-256 comparison against the accepted local assets
```

CarryHandle publication never treats "build", "tag" and "publish" as one
automatic transaction. Those are intentionally separate trust boundaries.

#### Release metadata is invocation-time state

Release-publication values do **not** belong in `carryhandle.cfg`.

The application manifest describes persistent application/build identity such
as disc identity, presentation metadata and persistence identity. A particular
release tag, release-notes file or distribution archive is temporary
release-operation state.

The Make interface accepts:

| Variable | Required | Meaning |
| --- | --- | --- |
| `RELEASE_TAG` | yes | Already-existing local and remote release tag |
| `RELEASE_ASSETS` | yes | Space-separated local artifact paths |
| `RELEASE_NOTES` | yes | Non-empty Markdown/text release-notes file |
| `RELEASE_TITLE` | no | Release title override |
| `RELEASE_PROVIDER` | no | `auto`, `github`, or `gitlab`; default `auto` |
| `RELEASE_REMOTE` | no | Git remote containing the release tag; default `origin` |
| `RELEASE_ALLOW_DIRTY` | no | Space-separated tracked paths explicitly permitted to be unstaged and dirty |

The direct host tool exposes the same values through `ch_release.py` options.
Assets and allowed-dirty paths are repeatable command-line options.

When no release title is supplied, the default is:

```text
<application.name> <tag>
```

where the application name is read from the already-existing
`carryhandle.cfg`.

#### Publication preflight

`make publish-release-check` invokes:

```text
ch_release.py check
```

and performs no forge mutation.

The preflight verifies all release inputs before publication is allowed.

The important invariants are:

```text
consumer path is a Git working tree

release notes exist and are non-empty

at least one release asset exists
asset basenames are unique

staged source changes are forbidden

unstaged tracked changes are forbidden
unless each path was explicitly allowed

local release tag exists
local release tag resolves to HEAD

remote release tag exists
remote release tag resolves to the same HEAD

remote URL resolves to a repository and forge host

provider is known or was explicitly selected
```

Both annotated and lightweight Git tags are accepted. For an annotated tag,
CarryHandle compares the peeled commit target rather than the tag object's own
object ID.

The central source invariant is:

```text
HEAD
  ==
local release tag target
  ==
remote release tag target
```

A consumer may continue development after a release. In that case the current
development branch will no longer pass preflight for the older release tag.
To inspect or reproduce the older release, check out that release commit/tag
rather than weakening the invariant.

#### Working-tree policy

Publication source checks distinguish staged, tracked-unstaged and untracked
state.

```text
staged tracked changes
    always forbidden

unstaged tracked changes
    forbidden by default
    may be allowed path-by-path with RELEASE_ALLOW_DIRTY

untracked files
    do not block publication
```

The allow-list exists for deliberate local tracked differences whose presence
does not change the accepted tagged artifact. It is explicit rather than
pattern-based so accidental source dirt remains fail-closed.

The release tag still must target `HEAD` even when an allowed dirty path is
present.

#### Provider selection

With:

```text
RELEASE_PROVIDER=auto
```

CarryHandle auto-detects the public hosted forges:

```text
github.com -> github
gitlab.com -> gitlab
```

Unknown/self-hosted hosts are not guessed. They require an explicit provider
selection.

Explicit selection chooses the provider implementation; it does not waive
source/tag/asset checks.

Publication requires the matching authenticated forge CLI:

```text
GitHub -> gh
GitLab -> glab
```

The read-only `publish-release-check` command does not require provider
publication authentication because it does not call the mutation provider.

#### Existing releases are immutable to this workflow

Before creating anything remotely, the provider checks whether a release for
the requested tag already exists.

If it exists:

```text
REFUSE
```

CarryHandle does not:

```text
update it
replace its notes
replace an asset
delete it
recreate it
```

This makes publication intentionally one-shot. A previously published release
must be handled manually if an operator deliberately wants to change it.

#### GitHub publication

The GitHub provider verifies authenticated access to the exact resolved
repository and checks release absence before creation.

Creation uses the equivalent of:

```text
gh release create <tag> <explicit-assets...> \
    --title <title> \
    --notes-file <notes> \
    --verify-tag
```

`--verify-tag` is a critical invariant. CarryHandle will not allow the GitHub
release command to manufacture a missing tag after preflight.

After creation CarryHandle reads the release back and verifies:

```text
tag name
release title
not draft
not prerelease
release notes
exact asset-name set
asset sizes
```

Every requested asset is then downloaded into a fresh temporary directory and
its byte length and SHA-256 digest are compared with the accepted local asset.

#### GitLab publication

The GitLab provider deliberately does **not** use `glab release create` for the
creation step.

Instead it performs a Releases API `POST` through `glab api` containing:

```text
name
tag_name
description
```

and deliberately omits:

```text
ref
```

That distinction is intentional. A GitLab release create operation can create
a missing tag when given a reference. CarryHandle has already verified that
the requested tag exists remotely and does not grant the provider permission
to create one. If the tag disappears between preflight and release creation,
the API request should therefore fail rather than silently creating another
tag.

After the release exists, only the explicitly supplied files are uploaded with
the GitLab release upload path.

CarryHandle then reads the release back and verifies:

```text
tag name
release title
release notes
exact asset-name set
```

Every requested asset is downloaded freshly and its byte length and SHA-256
digest are compared with the accepted local asset.

#### Explicit-assets-only rule

CarryHandle publishes only paths passed through `RELEASE_ASSETS` / `--asset`.

It does not scan `dist/`, infer archive names, add checksums, add source
archives, or upload other nearby files automatically.

Asset basenames must be unique so the remote asset set can be compared
unambiguously with the accepted local set.

#### Published-asset verification

A successful provider upload is not considered sufficient proof.

For every asset CarryHandle records before publication:

```text
basename
byte length
SHA-256
```

After publication it fetches provider metadata and then downloads each asset
again into a newly created temporary directory.

Acceptance requires:

```text
published asset name == expected asset name
published/downloaded byte length == accepted local byte length
SHA256(freshly downloaded bytes) == SHA256(accepted local bytes)
```

The successful command therefore means:

```text
Publication: PERFORMED AND VERIFIED
```

rather than merely "the upload command exited zero."

#### Partial failures and rollback

Remote publication is irreversible enough that CarryHandle does not attempt
automatic destructive rollback.

A failure can occur after the release has been created, for example during:

```text
asset upload
metadata verification
fresh download
hash verification
```

If that happens, CarryHandle reports failure and leaves the remote state for
human inspection.

It does **not** automatically delete the release or uploaded assets.

Because the normal workflow refuses an existing release, blindly re-running
the publication command after a partial mutation is also intentionally not a
recovery strategy. Inspect the forge state first.

This conservative behavior avoids turning an uncertain partial failure into an
automatic delete/recreate cycle.

#### What publication does not do

`publish-release` does not:

```text
build the consumer
run make release
run make clean
force a rebuild
decide that an artifact passed testing
create a Git commit
create a Git tag
move a Git tag
push a Git tag
modify carryhandle.cfg
invent release notes
invent project-specific release prose
overwrite an existing forge release
```

The release artifact should already have been tested before publication is
attempted.

Hardware-sensitive applications should perform their relevant real-hardware
acceptance before publication, not after it.

#### Current validation status

The publication layer has been exercised at several levels.

Host/fake-provider proof covers:

```text
GitHub successful create path
GitHub metadata verification
GitHub fresh-download SHA-256 verification
GitHub existing-release refusal

GitLab successful create path
GitLab metadata verification
GitLab fresh-download SHA-256 verification
GitLab existing-release refusal
GitLab creation without glab release create
GitLab API creation without ref
```

The user-facing Make integration has also been exercised end to end through:

```text
make publish-release-check
make publish-release
```

using an exact detached DoomCube release checkout and the accepted DoomCube
release ZIP while provider mutation was redirected to a fake forge CLI.

Real GitHub access has proven that an already-existing DoomCube release is
detected and refused without changing its before/after release metadata.

At the time this contract was written:

```text
GitHub creation of a brand-new disposable real release
    not yet live-proven by this workflow

GitLab real mutation
    not yet live-proven by this workflow
```

Those limitations should not be silently promoted into stronger claims until a
deliberate disposable live-publication test proves them.

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
