# Persistence

This document covers the application-facing persistence layers. For the
transaction format, Memory Card backend and durability model, see
[`storage.md`](storage.md).

## Layering

```text
Application
    ↓
CH_ApplicationSave* convenience lifecycle
    ↓
CH_Persist* object store
    ↓
CH_Tx* transactional store
    ↓
sector backend (for example Memory Card)
```

Save bundles provide an additional structured application-data layer above
persistent objects.

## Application-save lifecycle

`CH_ApplicationSaveOpen()` can own the common physical lifecycle for one
application save:

- Memory Card mount
- CARD work-area allocation
- transaction sector-buffer allocation
- manifest geometry validation
- file open/create
- Memory Card transaction-backend construction
- transaction-container initialization/recovery
- CARD presentation application

`CH_ApplicationSaveClose()` releases resources acquired by Open.

A live session must not be copied. Opening an already-live session is rejected
rather than implicitly discarding existing resources.

Existing same-named files must match the configured sector count exactly; an
incompatible file is not silently resized, deleted or recreated.

## Generated save descriptor

The generated `CH_ApplicationSaveDescriptor` describes physical
storage/presentation properties such as:

- filename
- sector count
- presentation offset
- generated presentation bytes

Application/disc identity remains in `CH_ApplicationInfo` rather than being
duplicated into the descriptor.

## Object persistence

`CH_PersistGet`, `CH_PersistPut` and `CH_PersistDelete` operate on exact binary
`scope + key` identities.

Identity comparison is byte-exact and case-sensitive.

The newest committed transaction record determines the object's visible state:

```text
PUT    -> current verified object
DELETE -> not found
absent -> not found
```

The object layer intentionally hides transaction-log iteration from normal
application code.

## Application-save convenience wrappers

An open application-save session exposes convenience wrappers around the
object store so consumers do not normally need to wire the transaction backend
and scratch sector buffer manually:

```text
CH_ApplicationSaveGet
CH_ApplicationSavePut
CH_ApplicationSavePutIfChanged
CH_ApplicationSaveDelete
```

## Write-if-changed

`CH_ApplicationSavePutIfChanged()` performs a read-only comparison first. A
physical/logical PUT occurs only when the exact bytes differ or the object does
not already exist.

This is useful for frequently persisted configuration where rewriting an
identical object would only create unnecessary Memory Card activity.

The helper uses caller-owned comparison scratch; it does not hide a heap
allocation for the comparison.

## Streamed objects

`CH_PersistPutStream()` accepts a synchronous producer for large objects. The
producer emits the original uncompressed bytes; CarryHandle handles compression
and transactional append.

The producer must emit exactly the declared raw size. Too many/few bytes or a
producer failure aborts the operation without appending a transaction record.

## Save bundles

Save bundles package multiple logical files/objects into a defined binary
container suitable for persistence as one object while allowing streaming
construction and extraction.

Relevant headers:

```text
ch_save_bundle.h
ch_save_bundle_persist.h
```

Use bundles when application policy naturally treats a group of files as one
atomic persisted unit. The application still owns the meaning, names and
migration policy of bundle contents.

## Failure model

Persistence distinguishes normal absence from errors and preserves important
failure states such as:

- I/O failure
- corrupt store
- buffer too small
- commit uncertain
- no space
- no memory
- source/codec failure

A `COMMIT_UNCERTAIN` result means a publication I/O operation began but generic
code cannot prove the durable outcome. Recover authoritative transaction state
before attempting another write.

## What persistence does not own

CarryHandle does not decide:

- save-slot semantics
- game/content identity policy
- application object schemas
- migration rules
- when the application should save
- whether a cosmetic presentation failure is fatal
