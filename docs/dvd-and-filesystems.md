# DVD and Filesystems

CarryHandle presents application data through the standard `dvd:/` path while
allowing different backing mechanisms for release and development.

The consumer can therefore keep paths such as:

```text
dvd:/launcher/foo.bmp
dvd:/data/wad/game.wad
```

stable across several launch modes.

## Native disc/FST backing

```c
bool CH_DVDMount(void);
```

`CH_DVDMount()` mounts the native GameCube disc FST as `dvd:/`. Ordinary
stdio/POSIX-style operations such as `fopen`, `open`, `stat`, `fread` and
`fseek` can then access files in the image.

CarryHandle handles the physical DVD read alignment required by the GameCube
DVD interface. Repeated mounting of an already-mounted filesystem is accepted.

## SD-directory backing

```c
bool CH_DVDMountSDDirectory(
    CH_DVD_SDDevice device,
    const char *directory);
```

This development mode maps one directory on a FAT-formatted SD device to
`dvd:/`.

Example:

```c
CH_DVDMountSDDirectory(
    CH_DVD_SD_SLOT_A,
    "/doomcube-files");
```

Then:

```text
dvd:/launcher/foo.bmp
```

resolves to:

```text
<SD>:/doomcube-files/launcher/foo.bmp
```

The directory must already exist. `CH_DVDUnmount()` removes the alias and
releases CarryHandle's private FAT mount.

This lets a consumer stage the same filesystem tree for an SD-backed hardware
run and for a native GCM/FST release image.

## Remote-image backing

A PC-hosted GameCube image can supply the same `dvd:/` interface through a
live `CH_RemoteDiscSession`:

```c
bool CH_DVDMountRemote(CH_RemoteDiscSession *session);
```

CarryHandle queries the image's own FST location and size, downloads the FST to
CarryHandle-owned RAM, and services subsequent file reads through
`CH_RemoteDiscRead()`.

The caller retains ownership of the remote session, which must remain open for
the mounted lifetime. `CH_DVDUnmount()` releases the downloaded FST and
filesystem state but does not close the transport session.

See [`remote-development.md`](remote-development.md).

## Unmounting

```c
void CH_DVDUnmount(void);
```

Safe when nothing is mounted.

## Release vs development

The backing mechanism is intentionally separable from the application's path
policy:

```text
release image       -> native FST -> dvd:/
SD hardware dev     -> SD folder  -> dvd:/
USB Gecko remote    -> PC image   -> dvd:/
```

Consumers should avoid adding separate asset-path policy merely because the
development transport is different.

## Ownership boundary

CarryHandle owns the reusable filesystem/backing mechanism. The consumer owns:

- which assets belong in its staged tree
- the SD directory name
- target names such as `run-hardware-sd`
- which transport is appropriate for a particular test
