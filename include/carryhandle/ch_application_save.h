#ifndef CARRYHANDLE_CH_APPLICATION_SAVE_H
#define CARRYHANDLE_CH_APPLICATION_SAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <carryhandle/ch_application.h>
#include <carryhandle/ch_memcard.h>
#include <carryhandle/ch_tx.h>
#include <carryhandle/ch_tx_backend.h>
#include <carryhandle/ch_tx_memcard_backend.h>


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Standard sector-zero layout used by CarryHandle application saves.
 *
 *   0..31   CHTX container header
 *   32..63  reserved
 *   64..    GameCube CARD presentation
 *
 * The presentation runtime preserves bytes outside its own range.
 */
#define CH_APPLICATION_SAVE_PRESENTATION_OFFSET 64u


/*
 * Build-time description of one application's physical Memory Card save.
 *
 * Application/disc identity remains CH_ApplicationInfo.
 *
 * This descriptor contains only storage/presentation properties:
 *
 *   filename
 *       Physical GameCube Memory Card filename.
 *
 *   sector_count
 *       Required physical CARD file size in sectors.
 *
 *   presentation_offset
 *       Offset within sector zero at which presentation_data begins.
 *
 *   presentation_data / presentation_size
 *       Optional generated GameCube CARD presentation blob.
 *
 * Byte size is deliberately not stored here:
 *
 *     file bytes = sector_count * mounted CARD sector size
 */
typedef struct CH_ApplicationSaveDescriptor
{
    const char *filename;

    uint32_t sector_count;

    uint32_t presentation_offset;

    const unsigned char *presentation_data;
    uint32_t presentation_size;

} CH_ApplicationSaveDescriptor;


/*
 * Result domain for application-save lifecycle operations.
 *
 * CARD and transaction details remain available in the session so callers
 * can diagnose the underlying subsystem without collapsing libogc2 or
 * transaction error codes into this convenience layer.
 */
typedef enum CH_ApplicationSaveResult
{
    CH_APPLICATION_SAVE_RESULT_OK = 0,

    CH_APPLICATION_SAVE_RESULT_INVALID_ARGUMENT = -1,
    CH_APPLICATION_SAVE_RESULT_NO_MEMORY = -2,

    CH_APPLICATION_SAVE_RESULT_CARD = -3,
    CH_APPLICATION_SAVE_RESULT_GEOMETRY_MISMATCH = -4,

    CH_APPLICATION_SAVE_RESULT_TRANSACTION = -5,
    CH_APPLICATION_SAVE_RESULT_PRESENTATION = -6

} CH_ApplicationSaveResult;


/*
 * One open application-save lifecycle.
 *
 * CH_ApplicationSaveOpen() will own:
 *
 *   - Memory Card mounting
 *   - CARD work-area allocation
 *   - transaction sector-buffer allocation
 *   - manifest geometry validation
 *   - file open/create
 *   - Memory Card transaction backend construction
 *   - transaction-container initialization/recovery
 *   - CARD presentation application
 *
 * CH_ApplicationSaveClose() releases all resources acquired by Open().
 *
 * Do not copy a live session.
 */
typedef struct CH_ApplicationSaveSession
{
    const CH_ApplicationInfo *application;
    const CH_ApplicationSaveDescriptor *descriptor;

    CH_MemCardSession memcard;

    card_file file;

    CH_TxMemCardBackendContext tx_context;
    CH_TxSectorBackend tx_backend;

    void *work_area;
    void *sector_buffer;

    size_t sector_buffer_size;

    uint32_t file_size;

    s32 card_result;
    CH_TxResult tx_result;

    bool mounted;
    bool file_open;
    bool created;
    bool open;

} CH_ApplicationSaveSession;


/*
 * Open this build's application save on one GameCube Memory Card slot.
 *
 * application:
 *     Normally CH_ApplicationGetInfo().
 *
 * descriptor:
 *     Normally generated from this application's [memcard] manifest data.
 *
 * Existing same-named files must exactly match descriptor->sector_count.
 * An incompatible file is never resized, deleted or recreated implicitly.
 *
 * A newly created file is initialized as an empty transaction container.
 * An existing file must recover as a valid transaction container.
 *
 * Presentation is applied after transaction initialization so the CHTX
 * sector-zero header already exists and is preserved by presentation
 * read/modify/write.
 *
 * session must be zero-initialized before its first Open(), or previously
 * returned to the closed state by CH_ApplicationSaveClose().
 *
 * Calling Open() on a live or partially-owned session is rejected with
 * CH_APPLICATION_SAVE_RESULT_INVALID_ARGUMENT. Existing resources are never
 * discarded implicitly.
 *
 * On success:
 *     session->open is true.
 *     session->created reports whether the CARD file was freshly created.
 *
 * On failure:
 *     the function cleans up all resources it acquired during this Open().
 */
CH_ApplicationSaveResult CH_ApplicationSaveOpen(
    CH_ApplicationSaveSession *session,
    const CH_ApplicationInfo *application,
    const CH_ApplicationSaveDescriptor *descriptor,
    s32 slot
);


/*
 * Close an application-save session.
 *
 * Safe to call on a zeroed or already-closed session.
 */
CH_ApplicationSaveResult CH_ApplicationSaveClose(
    CH_ApplicationSaveSession *session
);


/*
 * Persistence plumbing exposed without requiring callers to understand the
 * Memory Card adapter itself.
 *
 * These return NULL/0 when the session is not open.
 */
const CH_TxSectorBackend *CH_ApplicationSaveBackend(
    const CH_ApplicationSaveSession *session
);

void *CH_ApplicationSaveSectorBuffer(
    CH_ApplicationSaveSession *session
);

size_t CH_ApplicationSaveSectorBufferSize(
    const CH_ApplicationSaveSession *session
);


/*
 * True only when Open() created the physical CARD file during this
 * application-save session.
 *
 * This is useful for first-run seeding/migration policy while keeping that
 * policy outside the generic storage lifecycle.
 */
bool CH_ApplicationSaveWasCreated(
    const CH_ApplicationSaveSession *session
);


/*
 * Return this build's immutable generated application-save descriptor.
 *
 * This symbol is provided by the generated Memory Card source for builds
 * whose manifest contains a [memcard] section.
 *
 * The returned descriptor has static storage duration.
 */
const CH_ApplicationSaveDescriptor *
CH_ApplicationSaveGetDescriptor(void);


#ifdef __cplusplus
}
#endif

#endif
