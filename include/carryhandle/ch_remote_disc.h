#ifndef CARRYHANDLE_CH_REMOTE_DISC_H
#define CARRYHANDLE_CH_REMOTE_DISC_H

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


/*
 * CarryHandle remote-disc transport.
 *
 * This is a development transport for reading arbitrary byte ranges from
 * a disc image hosted by a PC over USB Gecko.
 *
 * It does not mount dvd:/ and does not parse or install a GameCube FST.
 * Those are filesystem/disc-bootstrap responsibilities above this layer.
 *
 * The current wire implementation is the transport proven on physical
 * GameCube hardware by CH-USBGECKO-REMOTE-READ-3:
 *
 *   - 32-bit byte offsets
 *   - 32-bit logical read lengths
 *   - negotiated transport chunks
 *   - ordered chunk sequence numbers
 *   - end-to-end CRC32
 */
#define CH_REMOTE_DISC_MAX_CHUNK_SIZE 1024u


typedef enum CH_RemoteDiscResult
{
    CH_REMOTE_DISC_RESULT_OK = 0,

    CH_REMOTE_DISC_RESULT_INVALID_ARGUMENT = -1,
    CH_REMOTE_DISC_RESULT_NOT_AVAILABLE = -2,
    CH_REMOTE_DISC_RESULT_TRANSPORT = -3,
    CH_REMOTE_DISC_RESULT_PROTOCOL = -4,
    CH_REMOTE_DISC_RESULT_RANGE = -5,
    CH_REMOTE_DISC_RESULT_CHECKSUM = -6

} CH_RemoteDiscResult;


/*
 * One live remote-disc transport session.
 *
 * The structure must be zero-initialized before its first Open(), or
 * returned to the closed state by CH_RemoteDiscClose().
 *
 * Do not copy a live session.
 */
typedef struct CH_RemoteDiscSession
{
    int gecko_channel;
    bool open;

} CH_RemoteDiscSession;


/*
 * Open a remote-disc transport over one USB Gecko EXI channel.
 *
 * GameCube memory-card slots map to EXI channels:
 *
 *   Slot A = 0
 *   Slot B = 1
 *
 * The currently proven hardware setup uses Slot B.
 *
 * This function verifies that a USB Gecko is alive and flushes stale
 * transport state left by the previous loader.
 */
CH_RemoteDiscResult CH_RemoteDiscOpen(
    CH_RemoteDiscSession *session,
    int gecko_channel
);


/*
 * Read one arbitrary byte range from the PC-hosted image.
 *
 * offset and length are byte units.
 *
 * On CH_REMOTE_DISC_RESULT_OK:
 *
 *   - exactly length bytes have been written to buffer
 *   - chunk ordering was validated
 *   - the server's final CRC32 matched the bytes received locally
 *
 * Zero-length reads are accepted. buffer may be NULL only when length is 0.
 *
 * This function does not impose GameCube optical-drive alignment rules;
 * unaligned offsets and odd lengths are valid transport operations.
 */
CH_RemoteDiscResult CH_RemoteDiscRead(
    CH_RemoteDiscSession *session,
    uint32_t offset,
    void *buffer,
    uint32_t length
);


/*
 * Close a remote-disc transport session.
 *
 * No persistent resource exists on the PC side yet; this currently returns
 * the local session object to its reusable closed state.
 *
 * Safe to call on a zeroed or already-closed session.
 */
void CH_RemoteDiscClose(
    CH_RemoteDiscSession *session
);


/*
 * True only for a successfully opened transport session.
 */
bool CH_RemoteDiscIsOpen(
    const CH_RemoteDiscSession *session
);


#ifdef __cplusplus
}
#endif

#endif
