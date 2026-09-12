#include <carryhandle/ch_dvd.h>
#include <carryhandle/ch_remote_disc.h>

#include <stddef.h>
#include <ogc/usbgecko.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "proof_expected_dvd.h"


#define PROOF_GECKO_CHANNEL 1

#define PROOF_DVD_PATH \
    "dvd:/" PROOF_TARGET_PATH


static uint8_t proof_prefix[PROOF_TARGET_PREFIX_LENGTH];


static void put_be32(
    uint8_t *dst,
    uint32_t value)
{
    dst[0] = (uint8_t)(value >> 24);
    dst[1] = (uint8_t)(value >> 16);
    dst[2] = (uint8_t)(value >> 8);
    dst[3] = (uint8_t)value;
}


static uint32_t crc32_update(
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


static bool send_exact(
    const void *buffer,
    uint32_t length)
{
    const uint8_t *bytes =
        (const uint8_t *)buffer;

    uint32_t done = 0;

    while (done < length) {
        int result = usb_sendbuffer_safe(
            PROOF_GECKO_CHANNEL,
            bytes + done,
            (int)(length - done)
        );

        if (result <= 0)
            return false;

        done += (uint32_t)result;
    }

    return true;
}


static void halt_forever(void)
{
    for (;;) {
        __asm__ volatile("nop");
    }
}


static void fail(
    uint32_t stage,
    uint32_t detail)
{
    uint8_t packet[16] = {
        'C','H','F','A','I','L','6','!',
        0,0,0,0,
        0,0,0,0
    };

    put_be32(packet + 8, stage);
    put_be32(packet + 12, detail);

    (void)send_exact(
        packet,
        sizeof(packet)
    );

    halt_forever();
}


int main(void)
{
    CH_RemoteDiscSession remote = {0};

    CH_RemoteDiscResult remote_result;

    struct stat st;

    FILE *file;

    size_t got;

    uint32_t crc;

    /*
     * Keep the already-proven wiiload -> host-server handoff delay.
     */
    usleep(3000000);

    remote_result = CH_RemoteDiscOpen(
        &remote,
        PROOF_GECKO_CHANNEL
    );

    if (remote_result != CH_REMOTE_DISC_RESULT_OK)
        fail(1, (uint32_t)(-remote_result));

    /*
     * This is the new integration point.
     *
     * CH_DVDMountRemote() must bootstrap the remote FST and register the
     * SAME ordinary dvd:/ devoptab used by existing CarryHandle applications.
     */
    if (!CH_DVDMountRemote(&remote))
        fail(2, 0);

    /*
     * From here onward, the application knows absolutely nothing about:
     *
     *   USB Gecko
     *   CHR3
     *   FST offsets
     *   GCM offsets
     *
     * It is ordinary filesystem code.
     */
    memset(&st, 0, sizeof(st));

    if (stat(PROOF_DVD_PATH, &st) != 0)
        fail(3, 0);

    if ((uint32_t)st.st_size != PROOF_TARGET_SIZE)
        fail(4, (uint32_t)st.st_size);

    file = fopen(
        PROOF_DVD_PATH,
        "rb"
    );

    if (file == NULL)
        fail(5, 0);

    /*
     * Keep this proof deterministic: ask newlib not to prefetch a larger
     * stdio buffer than the bytes being verified.
     */
    (void)setvbuf(
        file,
        NULL,
        _IONBF,
        0
    );

    memset(
        proof_prefix,
        0,
        sizeof(proof_prefix)
    );

    got = fread(
        proof_prefix,
        1,
        sizeof(proof_prefix),
        file
    );

    if (got != sizeof(proof_prefix)) {
        fclose(file);
        fail(6, (uint32_t)got);
    }

    if (fclose(file) != 0)
        fail(7, 0);

    crc = crc32_update(
        0xFFFFFFFFu,
        proof_prefix,
        sizeof(proof_prefix)
    ) ^ 0xFFFFFFFFu;

    if (crc != PROOF_TARGET_PREFIX_CRC32)
        fail(8, crc);

    CH_DVDUnmount();

    /*
     * CH_DVDUnmount owns only the downloaded FST. The transport session is
     * caller-owned and should still be open here.
     */
    if (!CH_RemoteDiscIsOpen(&remote))
        fail(9, 0);

    CH_RemoteDiscClose(&remote);

    if (CH_RemoteDiscIsOpen(&remote))
        fail(10, 0);

    {
        static const uint8_t pass[8] = {
            'C','H','P','A','S','S','6','!'
        };

        (void)send_exact(
            pass,
            sizeof(pass)
        );
    }

    halt_forever();

    return 0;
}
