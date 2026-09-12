#include <carryhandle/ch_remote_disc.h>

#include <stddef.h>
#include <ogc/usbgecko.h>

#include <stdint.h>
#include <string.h>


#define CH_REMOTE_DISC_PROTOCOL_VERSION 1u
#define CH_REMOTE_DISC_OPERATION_READ   1u


static void CH_RemoteDiscPutBE32(
    uint8_t *dst,
    uint32_t value)
{
    dst[0] = (uint8_t)(value >> 24);
    dst[1] = (uint8_t)(value >> 16);
    dst[2] = (uint8_t)(value >> 8);
    dst[3] = (uint8_t)value;
}


static uint32_t CH_RemoteDiscGetBE32(
    const uint8_t *src)
{
    return ((uint32_t)src[0] << 24)
        | ((uint32_t)src[1] << 16)
        | ((uint32_t)src[2] << 8)
        | ((uint32_t)src[3]);
}


static bool CH_RemoteDiscSendExact(
    const CH_RemoteDiscSession *session,
    const void *buffer,
    uint32_t length)
{
    const uint8_t *bytes =
        (const uint8_t *)buffer;

    uint32_t done = 0;

    while (done < length) {
        int result = usb_sendbuffer_safe(
            session->gecko_channel,
            bytes + done,
            (int)(length - done)
        );

        if (result <= 0)
            return false;

        done += (uint32_t)result;
    }

    return true;
}


static bool CH_RemoteDiscRecvExact(
    const CH_RemoteDiscSession *session,
    void *buffer,
    uint32_t length)
{
    uint8_t *bytes =
        (uint8_t *)buffer;

    uint32_t done = 0;

    while (done < length) {
        int result = usb_recvbuffer_safe(
            session->gecko_channel,
            bytes + done,
            (int)(length - done)
        );

        if (result <= 0)
            return false;

        done += (uint32_t)result;
    }

    return true;
}


static uint32_t CH_RemoteDiscCRC32Update(
    uint32_t crc,
    const uint8_t *data,
    uint32_t length)
{
    uint32_t i;

    for (i = 0; i < length; ++i) {
        uint32_t bit;

        crc ^= data[i];

        for (bit = 0; bit < 8; ++bit) {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }

    return crc;
}


CH_RemoteDiscResult CH_RemoteDiscOpen(
    CH_RemoteDiscSession *session,
    int gecko_channel)
{
    if (session == NULL)
        return CH_REMOTE_DISC_RESULT_INVALID_ARGUMENT;

    /*
     * libogc2 USB Gecko supports EXI channel 0 or 1.
     */
    if (gecko_channel != 0 &&
        gecko_channel != 1) {
        return CH_REMOTE_DISC_RESULT_INVALID_ARGUMENT;
    }

    /*
     * Follow CarryHandle's existing session-lifecycle convention:
     * never silently discard a live session.
     */
    if (session->open)
        return CH_REMOTE_DISC_RESULT_INVALID_ARGUMENT;

    memset(session, 0, sizeof(*session));

    if (!usb_isgeckoalive(gecko_channel))
        return CH_REMOTE_DISC_RESULT_NOT_AVAILABLE;

    /*
     * wiiload may have owned the same adapter immediately before us.
     * Discard stale loader transport state before beginning our protocol.
     */
    usb_flush(gecko_channel);

    session->gecko_channel = gecko_channel;
    session->open = true;

    return CH_REMOTE_DISC_RESULT_OK;
}


CH_RemoteDiscResult CH_RemoteDiscRead(
    CH_RemoteDiscSession *session,
    uint32_t offset,
    void *buffer,
    uint32_t length)
{
    uint8_t request[16];
    uint8_t response[12];
    uint8_t chunk_header[12];
    uint8_t done_packet[8];

    uint8_t *output =
        (uint8_t *)buffer;

    uint32_t remaining;
    uint32_t position;
    uint32_t expected_sequence;
    uint32_t negotiated_chunk;
    uint32_t crc;

    if (session == NULL ||
        !session->open) {
        return CH_REMOTE_DISC_RESULT_INVALID_ARGUMENT;
    }

    if (length != 0 && buffer == NULL)
        return CH_REMOTE_DISC_RESULT_INVALID_ARGUMENT;

    if (length > UINT32_MAX - offset)
        return CH_REMOTE_DISC_RESULT_RANGE;

    memset(request, 0, sizeof(request));

    /*
     * Keep the exact physical-hardware-proven proof-3 framing.
     *
     * "CHR3"
     * version
     * operation
     * reserved[2]
     * offset BE32
     * length BE32
     */
    request[0] = 'C';
    request[1] = 'H';
    request[2] = 'R';
    request[3] = '3';

    request[4] = CH_REMOTE_DISC_PROTOCOL_VERSION;
    request[5] = CH_REMOTE_DISC_OPERATION_READ;

    CH_RemoteDiscPutBE32(
        request + 8,
        offset
    );

    CH_RemoteDiscPutBE32(
        request + 12,
        length
    );

    if (!CH_RemoteDiscSendExact(
            session,
            request,
            sizeof(request))) {
        return CH_REMOTE_DISC_RESULT_TRANSPORT;
    }

    /*
     * Server response:
     *
     * "CHOK"
     * total length BE32
     * negotiated chunk size BE32
     */
    if (!CH_RemoteDiscRecvExact(
            session,
            response,
            sizeof(response))) {
        return CH_REMOTE_DISC_RESULT_TRANSPORT;
    }

    if (memcmp(response, "CHOK", 4) != 0)
        return CH_REMOTE_DISC_RESULT_PROTOCOL;

    if (CH_RemoteDiscGetBE32(response + 4) != length)
        return CH_REMOTE_DISC_RESULT_PROTOCOL;

    negotiated_chunk =
        CH_RemoteDiscGetBE32(response + 8);

    if (negotiated_chunk == 0 ||
        negotiated_chunk > CH_REMOTE_DISC_MAX_CHUNK_SIZE) {
        return CH_REMOTE_DISC_RESULT_PROTOCOL;
    }

    remaining = length;
    position = 0;
    expected_sequence = 0;
    crc = 0xFFFFFFFFu;

    while (remaining > 0) {
        uint32_t sequence;
        uint32_t chunk_length;

        /*
         * Per-chunk frame:
         *
         * "CHNK"
         * sequence BE32
         * payload length BE32
         */
        if (!CH_RemoteDiscRecvExact(
                session,
                chunk_header,
                sizeof(chunk_header))) {
            return CH_REMOTE_DISC_RESULT_TRANSPORT;
        }

        if (memcmp(chunk_header, "CHNK", 4) != 0)
            return CH_REMOTE_DISC_RESULT_PROTOCOL;

        sequence =
            CH_RemoteDiscGetBE32(
                chunk_header + 4
            );

        chunk_length =
            CH_RemoteDiscGetBE32(
                chunk_header + 8
            );

        if (sequence != expected_sequence)
            return CH_REMOTE_DISC_RESULT_PROTOCOL;

        if (chunk_length == 0 ||
            chunk_length > negotiated_chunk ||
            chunk_length > remaining) {
            return CH_REMOTE_DISC_RESULT_PROTOCOL;
        }

        if (!CH_RemoteDiscRecvExact(
                session,
                output + position,
                chunk_length)) {
            return CH_REMOTE_DISC_RESULT_TRANSPORT;
        }

        crc = CH_RemoteDiscCRC32Update(
            crc,
            output + position,
            chunk_length
        );

        position += chunk_length;
        remaining -= chunk_length;
        ++expected_sequence;
    }

    /*
     * End frame:
     *
     * "CHDN"
     * CRC32 BE32
     */
    if (!CH_RemoteDiscRecvExact(
            session,
            done_packet,
            sizeof(done_packet))) {
        return CH_REMOTE_DISC_RESULT_TRANSPORT;
    }

    if (memcmp(done_packet, "CHDN", 4) != 0)
        return CH_REMOTE_DISC_RESULT_PROTOCOL;

    crc ^= 0xFFFFFFFFu;

    if (CH_RemoteDiscGetBE32(done_packet + 4) != crc)
        return CH_REMOTE_DISC_RESULT_CHECKSUM;

    return CH_REMOTE_DISC_RESULT_OK;
}


void CH_RemoteDiscClose(
    CH_RemoteDiscSession *session)
{
    if (session == NULL)
        return;

    memset(session, 0, sizeof(*session));
}


bool CH_RemoteDiscIsOpen(
    const CH_RemoteDiscSession *session)
{
    return session != NULL &&
        session->open;
}
