#include <carryhandle/ch_remote_disc.h>

#include <stddef.h>
#include <ogc/usbgecko.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>


#define PROOF_GECKO_CHANNEL 1
#define PROOF_MAX_LENGTH    262147u


typedef struct ProofCase
{
    uint32_t offset;
    uint32_t length;
    uint32_t crc32;

} ProofCase;


static const ProofCase proof_cases[] = {
    {
        0x00002443u,
        8193u,
        0x2AAAED20u
    },
    {
        0x00123457u,
        65537u,
        0x7A90D367u
    },
    {
        0x02000003u,
        262147u,
        0x10410EC3u
    }
};


static uint8_t proof_buffer[PROOF_MAX_LENGTH];


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


static bool proof_send_exact(
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


static void proof_fail(
    uint32_t index,
    uint32_t stage)
{
    uint8_t packet[16] = {
        'C','H','F','A','I','L','3','!',
        0,0,0,0,
        0,0,0,0
    };

    put_be32(packet + 8, index);
    put_be32(packet + 12, stage);

    (void)proof_send_exact(
        packet,
        sizeof(packet)
    );

    halt_forever();
}


int main(void)
{
    CH_RemoteDiscSession session = {0};

    CH_RemoteDiscResult result;

    uint32_t index;

    /*
     * Preserve proof #3's successful loader -> server handoff window.
     */
    usleep(3000000);

    result = CH_RemoteDiscOpen(
        &session,
        PROOF_GECKO_CHANNEL
    );

    if (result != CH_REMOTE_DISC_RESULT_OK) {
        proof_fail(
            0,
            100u + (uint32_t)(-result)
        );
    }

    if (!CH_RemoteDiscIsOpen(&session))
        proof_fail(0, 120u);

    for (
        index = 0;
        index < (
            sizeof(proof_cases) /
            sizeof(proof_cases[0])
        );
        ++index
    ) {
        const ProofCase *test =
            &proof_cases[index];

        uint32_t crc;

        memset(
            proof_buffer,
            0,
            test->length
        );

        /*
         * THIS is the actual thing being proven:
         *
         * the reusable CarryHandle module performs the complete
         * remote logical read.
         */
        result = CH_RemoteDiscRead(
            &session,
            test->offset,
            proof_buffer,
            test->length
        );

        if (result != CH_REMOTE_DISC_RESULT_OK) {
            proof_fail(
                index,
                200u + (uint32_t)(-result)
            );
        }

        /*
         * Independent proof-harness verification of the bytes returned
         * through the public CarryHandle API.
         */
        crc = crc32_update(
            0xFFFFFFFFu,
            proof_buffer,
            test->length
        ) ^ 0xFFFFFFFFu;

        if (crc != test->crc32)
            proof_fail(index, 300u);

        /*
         * Preserve proof #3's exact ACK wire contract so the exact same
         * already-proven PC host server can be reused unchanged.
         */
        {
            uint8_t ack[16] = {
                'C','H','A','C','K','3','!','!',
                0,0,0,0,
                0,0,0,0
            };

            put_be32(
                ack + 8,
                index
            );

            put_be32(
                ack + 12,
                crc
            );

            if (!proof_send_exact(
                    ack,
                    sizeof(ack))) {
                halt_forever();
            }
        }
    }

    CH_RemoteDiscClose(&session);

    if (CH_RemoteDiscIsOpen(&session))
        proof_fail(0, 400u);

    /*
     * Exact same hardware milestone marker as proof #3.
     */
    {
        static const uint8_t pass[8] = {
            'C','H','P','A','S','S','3','!'
        };

        (void)proof_send_exact(
            pass,
            sizeof(pass)
        );
    }

    halt_forever();

    return 0;
}
