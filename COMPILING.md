# Compiling CarryHandle

CarryHandle targets the Nintendo GameCube using devkitPPC and libogc2.

This document defines the intended build workflow. The build system will
be introduced alongside the first executable CarryHandle example.

## Requirements

The core toolchain is:

- devkitPro
- devkitPPC
- libogc2
- GNU Make
- Python 3 for host-side tooling where required

Additional libraries may be required by optional CarryHandle modules,
but the core framework should avoid unnecessary dependencies.

## Toolchain environment

A normal devkitPro installation provides environment variables such as:

```sh
DEVKITPRO
DEVKITPPC
```

CarryHandle must use the normal GameCube devkitPPC/libogc2 build
environment rather than maintaining a private compiler toolchain.

## Development model

CarryHandle is initially intended to be vendored with the project using
it.

For example:

```text
mygame/
├── carryhandle/
├── source/
└── Makefile
```

Possible ways to obtain it include:

```sh
git clone ...
```

or adding it as a Git submodule.

A global `make install` workflow is not required for the initial
versions.

## Intended CarryHandle build

The root build interface will use ordinary Make targets.

The intended interface is:

```sh
make
```

to build CarryHandle and:

```sh
make examples
```

to build the bundled examples.

Individual examples should also be buildable independently.

For example:

```sh
make -C examples/hello
```

Exact targets will be documented once the first executable example is
implemented.

## Build output

Generated files must not be mixed into the source directories.

Build products belong beneath:

```text
build/
```

Examples may produce GameCube executables such as:

```text
.dol
.elf
```

Native disc-image examples or applications may additionally produce:

```text
.iso
.gcm
```

depending on the tooling interface selected during implementation.

## Native GameCube disc images

CarryHandle is intended to provide reusable host-side tooling for
building native GameCube GCM/FST images.

This tooling will be extracted from the proven native-disc workflow
developed for DoomCube, after its Doom-specific assumptions have been
removed.

Disc-image tooling is a host-side CarryHandle component and must not be
confused with the runtime DVD/FST module.

## Dolphin

Examples should be runnable in Dolphin without changing their source.

The build system may eventually provide a convenience target such as:

```sh
make test
```

but Dolphin must remain a development convenience rather than a runtime
dependency.

## Real hardware

CarryHandle targets real Nintendo GameCube hardware as well as Dolphin.

Code that works only because of emulator behavior is considered a bug
unless explicitly documented otherwise.

## Cleaning

Generated output will be removable through:

```sh
make clean
```

Source files, examples, tools, and documentation must never be removed
by a clean operation.

## Dependencies

Core CarryHandle should remain as close as practical to:

```text
devkitPPC
    +
libogc2
```

Dependencies such as SDL should be module-specific and optional.

An application that only needs low-level CarryHandle facilities should
not be forced to link SDL.

## Current bootstrap state

The repository currently contains the project architecture and initial
version API.

The next implementation milestone is the first GameCube executable and
the concrete Makefile infrastructure supporting it.
