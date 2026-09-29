# Release Workflow

CarryHandle provides generic release orchestration while leaving the player
package and acceptance decision in the consuming application.

## Ownership boundary

### Consumer owns

- `release-bundle` implementation
- artifact contents
- application-specific build/package variables
- decision that the produced artifact is accepted
- real-hardware/emulator acceptance procedure
- release notes content
- optional release title
- bundle README and other application-specific prose

### CarryHandle owns

- prospective tag validation
- clean tracked-source preflight
- clean package orchestration
- `VERSION` derivation from `RELEASE_TAG`
- annotated local tag creation after successful packaging
- branch/tag push preflight
- atomic branch + tag push
- publication preflight
- source/local-tag/remote-tag consistency checks
- provider selection
- existing-release refusal
- upload of explicitly supplied assets
- remote metadata verification
- fresh download and SHA-256 verification of every asset

## Trust boundaries

The lifecycle deliberately contains separate trust boundaries:

```text
local package + local tag
        ↓
human / emulator / hardware acceptance
        ↓
remote branch + tag push
        ↓
forge publication
```

`make release` does not push or publish.

`make release-push` does not create a forge release.

`make publish-release` does not build the consumer, create a tag or decide
whether the artifact passed testing.

## Consumer contract

Release support is opt-in:

```make
include $(CARRY_ROOT)/make/release.mk
```

Every consumer provides:

```make
release-bundle:
    # application-specific player packaging
```

The default CarryHandle package target is `release-bundle` and the conventional
consumer version variable is `VERSION`.

## Normal release workflow

### 1. Start from the intended release branch

Example:

```bash
git switch master
git pull --ff-only
git submodule update --init --recursive
git status
```

Use the branch name appropriate to the consumer.

### 2. Perform consumer acceptance

Build and test the release candidate before publication. For a consumer with an
SD-backed hardware target this may look like:

```bash
make run-hardware-sd VERSION=X.Y.Z
```

This command is consumer-specific. The application decides what acceptance
requires.

### 3. Build the release and create the local tag

```bash
make release RELEASE_TAG=vX.Y.Z
```

CarryHandle:

1. validates the prospective tag and clean tracked source;
2. optionally runs the configured clean target;
3. invokes the consumer package target with `VERSION=X.Y.Z`;
4. creates the annotated local tag only after packaging succeeds.

A failed package therefore does not leave a release tag behind.

### 4. Push branch and tag atomically

```bash
make release-push RELEASE_TAG=vX.Y.Z
```

Preflight requires:

- the local tag exists
- the tag resolves to current `HEAD`
- `HEAD` is on a named branch
- the configured remote exists
- staged/unstaged tracked source is clean

CarryHandle then pushes the branch and tag in one atomic remote update.

### 5. Publication preflight

```bash
make publish-release-check \
    RELEASE_TAG=vX.Y.Z \
    RELEASE_ASSETS="dist/mygame-vX.Y.Z.zip" \
    RELEASE_NOTES="release/vX.Y.Z.md" \
    RELEASE_TITLE="My Game vX.Y.Z"
```

This is read-only and performs no forge mutation.

### 6. Publish

```bash
make publish-release \
    RELEASE_TAG=vX.Y.Z \
    RELEASE_ASSETS="dist/mygame-vX.Y.Z.zip" \
    RELEASE_NOTES="release/vX.Y.Z.md" \
    RELEASE_TITLE="My Game vX.Y.Z"
```

A successful run ends only after remote metadata and freshly downloaded assets
have been verified.

## Release variables

| Variable | Required | Meaning |
| --- | --- | --- |
| `RELEASE_TAG` | yes | prospective/local/remote release tag depending on stage |
| `RELEASE_ASSETS` | publication | space-separated local artifact paths |
| `RELEASE_NOTES` | publication | non-empty Markdown/text notes file |
| `RELEASE_TITLE` | no | title override |
| `RELEASE_PROVIDER` | no | `auto`, `github`, or `gitlab`; default `auto` |
| `RELEASE_REMOTE` | no | Git remote; default `origin` |
| `RELEASE_ALLOW_DIRTY` | no | explicitly permitted tracked unstaged paths |

Release-operation state does not belong in `carryhandle.cfg`. The manifest
contains persistent application/build identity, not a particular release tag or
notes path.

When no title is supplied, publication defaults to:

```text
<application.name> <tag>
```

## Publication invariants

`publish-release-check` requires:

```text
consumer path is a Git working tree
release notes exist and are non-empty
at least one asset exists
asset basenames are unique
no staged tracked changes
no unapproved tracked-unstaged changes
local tag exists and resolves to HEAD
remote tag exists and resolves to the same HEAD
remote resolves to a supported/selected provider
```

The central source invariant is:

```text
HEAD
  == local release tag target
  == remote release tag target
```

Annotated and lightweight tags are accepted; annotated tags are compared by
their peeled commit target.

## Working-tree policy

```text
staged tracked changes
    always forbidden

unstaged tracked changes
    forbidden by default
    allowed only path-by-path through RELEASE_ALLOW_DIRTY

untracked files
    do not block publication
```

The tag must still resolve to `HEAD` even when a tracked path is explicitly
allowed dirty.

## Provider selection

With `RELEASE_PROVIDER=auto`:

```text
github.com -> github
gitlab.com -> gitlab
```

Unknown/self-hosted hosts are not guessed.

Publication requires the matching authenticated CLI:

```text
GitHub -> gh
GitLab -> glab
```

The read-only publication check does not require mutation authentication.

## Existing releases are immutable to this workflow

If a release already exists for the tag, CarryHandle refuses to publish.

It does not automatically:

- update notes
- replace assets
- delete/recreate the release

If an operator intentionally wants to change an already-published release,
that is a manual recovery/administrative operation outside this one-shot flow.

## Explicit-assets-only rule

Only paths explicitly passed through `RELEASE_ASSETS` are published.
CarryHandle does not scan `dist/`, invent archive names, add source archives or
upload nearby files automatically.

Asset basenames must be unique.

## GitHub publication

GitHub creation uses the equivalent of:

```text
gh release create <tag> <explicit assets> \
    --title <title> \
    --notes-file <notes> \
    --verify-tag
```

`--verify-tag` prevents the publication step from manufacturing a missing tag.
After creation CarryHandle verifies release metadata and assets.

## GitLab publication

GitLab creation uses a Releases API POST through `glab api` and deliberately
omits a `ref` field. This prevents publication from silently creating a missing
tag after preflight.

After creation CarryHandle verifies metadata and uploaded assets.

## Fresh-download verification

Before publication CarryHandle records for each asset:

```text
basename
byte length
SHA-256
```

After publication each asset is downloaded into a fresh temporary directory.
Acceptance requires exact name, byte length and SHA-256 equality.

A successful publication therefore means:

```text
Publication: PERFORMED AND VERIFIED
```

not merely that an upload command exited successfully.

## Partial failures

CarryHandle does not perform destructive automatic rollback after a partial
remote mutation. A failure can occur after release creation during upload,
metadata verification, download or hashing.

On such a failure:

1. CarryHandle reports failure;
2. remote state is left for human inspection;
3. do not blindly rerun publication, because existing releases are refused.

## What publication does not do

`publish-release` does not:

```text
build the consumer
run make release
run make clean
force a rebuild
accept/test an artifact
create a Git commit
create or move a Git tag
push a Git tag
modify carryhandle.cfg
invent release notes
invent consumer-specific release prose
overwrite an existing release
```

## Validation status

The local release lifecycle has been proven for:

```text
failed release-bundle -> no release tag
successful release-bundle -> annotated tag created afterward
RELEASE_TAG=vX.Y.Z -> VERSION=X.Y.Z reaches consumer hook
release-push -> atomic branch + tag push
remote branch/tag -> exact release HEAD
```

Provider-level fake/disposable tests cover GitHub and GitLab creation,
metadata checks, existing-release refusal and fresh-download hash verification.

The full GitHub consumer workflow is also live-proven: DoomCube v1.2.0 was
published through `make publish-release`, then freshly downloaded and verified
byte-for-byte/SHA-256 against the accepted local release asset.

Real GitLab mutation remains a separate proof requirement until deliberately
validated on a disposable real release.
