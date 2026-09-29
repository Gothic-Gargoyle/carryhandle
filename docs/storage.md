# Storage and Transaction Architecture

This document describes the lower-level storage mechanisms beneath
[`persistence.md`](persistence.md).

## Layering

```text
CH_Persist / application-save
        ↓
CH_Tx transactional store
        ↓
CH_TxSectorBackend
        ↓
Memory Card adapter or another sector backend
```

## Sector backend contract

The transaction layer performs complete-sector I/O through
`CH_TxSectorBackend`.

A backend defines:

- sector size/count
- required buffer alignment
- sector read
- sector write
- durability/sync boundary

The required buffer alignment must be explicit and is part of the runtime
backend contract.

## Persistent encoding

The transaction format does not persist native C structure layout or host byte
order. Persistent multi-byte integers are explicitly encoded as unsigned
32-bit big-endian values.

This keeps the on-disk ABI independent from compiler structure packing.

## Format-v2 arena model

Format v2 divides the data region into two equal arenas:

```text
metadata / superblocks
arena A
arena B
```

One arena is authoritative and appendable. The other is reserved for the next
crash-safe compacted snapshot.

If the available data-sector count is odd, the final sector is deliberately
unused so both arenas have equal capacity.

## Records

Transaction records carry exact binary identity:

```text
scope (optional)
key   (required)
operation
codec
payload metadata
CRC information
```

Scope and key are opaque byte sequences. CarryHandle assigns no string encoding
or case folding.

CRC fields protect both logical identity/body bytes and decoded payload
integrity so corruption cannot silently transform a record into a different
object.

## Authoritative state

Recovery validates the container header and both A/B superblocks and selects
the authoritative committed state.

A backend read failure is an I/O error, not merely an invalid copy; silently
falling back to an older generation could resurrect stale state.

## Append and publication

Transactional append separates writing a new record from publishing new
committed metadata.

`CH_TX_RESULT_OK` means the record is definitely committed.

`CH_TX_RESULT_COMMIT_UNCERTAIN` means publication I/O began but generic code
cannot prove whether the metadata became durable. Recover before another write.

## Compaction

Compaction writes the current live snapshot into the inactive arena and only
then publishes the new authoritative superblock.

Before publication, the old arena remains authoritative and untouched.

Only the physically latest PUT for each exact scope+key is retained; a latest
DELETE is represented by omission from the compacted snapshot.

## Memory Card backend

`CH_TxMemCardBackendInit()` adapts an already-open GameCube Memory Card file to
the generic sector backend.

The adapter performs complete physical CARD-sector I/O and requires 32-byte
aligned transaction buffers.

libogc2 `CARD_Write()` is synchronous. The backend's `sync()` therefore serves
as the transaction layer's logical durability boundary rather than issuing a
second CARD operation.

On durable transaction commits, CarryHandle may refresh native Memory Card file
metadata/timestamps as best-effort metadata maintenance. Metadata refresh
failure must not retroactively turn an otherwise durable transaction into a
failed commit.

## Memory Card primitives

The raw Memory Card layer handles reusable mounting/file/metadata operations.
Higher-level persistence policy belongs above it.

Relevant headers:

```text
ch_memcard.h
ch_tx_backend.h
ch_tx_format.h
ch_tx.h
ch_tx_memcard_backend.h
```

## Storage vs presentation

Persistence validity and GameCube Memory Card presentation are intentionally
separate concerns. Presentation code preserves transaction-header bytes and is
idempotent, but a banner/icon update is not itself a transaction commit.

See [`presentation.md`](presentation.md).
