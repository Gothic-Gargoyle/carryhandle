#ifndef CARRYHANDLE_CH_TX_MEMCARD_BACKEND_H
#define CARRYHANDLE_CH_TX_MEMCARD_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

#include <carryhandle/ch_memcard.h>
#include <carryhandle/ch_tx_backend.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Adapter context joining an opened GameCube Memory Card file to the
 * generic transaction-sector backend.
 *
 * session and file must remain valid and mounted/open for the complete
 * lifetime of any CH_TxSectorBackend produced from this context.
 */
typedef struct CH_TxMemCardBackendContext
{
    const CH_MemCardSession *session;
    card_file *file;

    uint32_t sector_size;
    uint32_t sector_count;

} CH_TxMemCardBackendContext;


/*
 * Build a transaction-sector backend over an already-open Memory Card file.
 *
 * Requirements:
 *   - session is mounted;
 *   - file belongs to the mounted slot;
 *   - session sector size is positive;
 *   - file length is a non-zero exact multiple of the sector size.
 *
 * The resulting backend performs complete physical Memory Card sector I/O
 * and requires 32-byte-aligned transaction buffers.
 *
 * libogc2 CARD_Write() is synchronous. Therefore a successful
 * write_sector() has already completed its physical CARD operation;
 * sync() is the transaction layer's logical durability barrier and does
 * not issue another CARD operation.
 */
bool CH_TxMemCardBackendInit(
    CH_TxMemCardBackendContext *context,
    const CH_MemCardSession *session,
    card_file *file,
    CH_TxSectorBackend *backend
);


#ifdef __cplusplus
}
#endif

#endif
