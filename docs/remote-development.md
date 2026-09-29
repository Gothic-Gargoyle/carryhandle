# Remote Development

CarryHandle supports real-hardware development paths that reduce SD-card
shuffling while keeping application paths close to release behavior.

## Development modes

A consumer may expose:

```bash
make run
make run-hardware-sd
make run-hardware-remote
```

These target names are conventional consumer interfaces, not mandatory generic
CarryHandle targets.

## SD-backed hardware development

The application DOL is launched through the consumer's normal hardware runner,
while an SD directory is mounted as `dvd:/` using
`CH_DVDMountSDDirectory()`.

This is useful for fast real-hardware smoke testing with the same staged tree
that would otherwise be packaged into a GameCube image.

## USB Gecko remote-disc transport

`CH_RemoteDisc*` is a development transport for reading arbitrary byte ranges
from a disc image hosted on a PC over USB Gecko.

It does not itself mount `dvd:/` or parse/install an FST. Those responsibilities
live in the DVD/filesystem layer.

Current transport properties include:

- 32-bit byte offsets
- 32-bit logical read lengths
- negotiated chunks up to `CH_REMOTE_DISC_MAX_CHUNK_SIZE`
- ordered chunk sequence numbers
- end-to-end CRC32

Open a session:

```c
CH_RemoteDiscSession session = {0};
CH_RemoteDiscResult result = CH_RemoteDiscOpen(&session, 1);
```

GameCube Memory Card slots map to EXI channels:

```text
Slot A -> 0
Slot B -> 1
```

The proven development setup uses Slot B.

Read arbitrary image bytes:

```c
CH_RemoteDiscRead(&session, offset, buffer, length);
```

Unlike physical optical reads, this transport deliberately permits unaligned
offsets and odd byte lengths.

Close with:

```c
CH_RemoteDiscClose(&session);
```

## Remote `dvd:/`

Once the session is open, the DVD layer can mount the hosted image:

```c
CH_DVDMountRemote(&session);
```

The image's FST is downloaded and ordinary application file accesses continue
through `dvd:/`.

## What this mode is for

Remote-disc mode is a development convenience. It is useful for:

- fast iteration without rebuilding/copying a full image for every asset change
- proving that application file access remains compatible with release paths
- hardware diagnostics where PC-side image access is useful

It is not the normal release runtime path.

## Logging and transport interference

USB Gecko can serve both deployment/debug output and data transport. Consumers
should avoid treating high-volume runtime logging as free: EXI traffic and host
consumption behavior can affect timing and can interfere with a transport that
also depends on the device.

For performance-sensitive tests, prefer sparse diagnostic output and compare
against an SD/native-image control when the transport itself could influence
the result.

## Ownership boundary

CarryHandle owns the generic transport and filesystem mechanisms. The consumer
owns:

- which image/file tree is hosted
- runner target names
- logging policy
- acceptance criteria
- whether remote mode is appropriate for a particular test
