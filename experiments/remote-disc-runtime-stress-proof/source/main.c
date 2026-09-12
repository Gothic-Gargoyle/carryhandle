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

#include "proof_expected_runtime.h"


#define PROOF_GECKO_CHANNEL 1

#define PROOF_IO_BUFFER_SIZE 65521u
#define PROOF_MAX_WINDOW_SIZE 32749u
#define PROOF_MAX_PROBE_SIZE 4093u


static uint8_t proof_io_buffer[
    PROOF_IO_BUFFER_SIZE
];

static uint8_t proof_small_buffer[
    PROOF_MAX_WINDOW_SIZE
];


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
                crc = (
                    crc >> 1
                ) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }

    return crc;
}


static uint32_t crc32_buffer(
    const uint8_t *data,
    uint32_t length)
{
    return crc32_update(
        0xFFFFFFFFu,
        data,
        length
    ) ^ 0xFFFFFFFFu;
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


static void send_marker(
    const char marker[8])
{
    (void)send_exact(
        marker,
        8
    );
}


static void send_done(void)
{
    static const char marker[8] = {
        'C','H','D','O','N','E','7','!'
    };

    send_marker(marker);
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
        'C','H','F','A','I','L','7','!',
        0,0,0,0,
        0,0,0,0
    };

    put_be32(
        packet + 8,
        stage
    );

    put_be32(
        packet + 12,
        detail
    );

    (void)send_exact(
        packet,
        sizeof(packet)
    );

    send_done();

    halt_forever();
}


static bool build_dvd_path(
    char *dst,
    size_t dst_size,
    const char *fst_path)
{
    int result;

    result = snprintf(
        dst,
        dst_size,
        "dvd:/%s",
        fst_path
    );

    return (
        result >= 0
        && (size_t)result < dst_size
    );
}


static uint32_t stream_entire_file(
    const char *path,
    uint32_t expected_size,
    uint32_t failure_base)
{
    FILE *file;
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t total = 0;

    file = fopen(
        path,
        "rb"
    );

    if (file == NULL)
        fail(failure_base + 0u, 0);

    /*
     * Do not let stdio add a second large buffering strategy. We want
     * CarryHandle's dvd:/ implementation itself to own the read pattern.
     */
    if (setvbuf(
            file,
            NULL,
            _IONBF,
            0) != 0) {
        fclose(file);
        fail(failure_base + 1u, 0);
    }

    while (total < expected_size) {
        uint32_t wanted =
            expected_size - total;

        size_t got;

        if (wanted > sizeof(proof_io_buffer))
            wanted = sizeof(proof_io_buffer);

        got = fread(
            proof_io_buffer,
            1,
            wanted,
            file
        );

        if (got != wanted) {
            fclose(file);
            fail(
                failure_base + 2u,
                (uint32_t)got
            );
        }

        crc = crc32_update(
            crc,
            proof_io_buffer,
            (uint32_t)got
        );

        total += (uint32_t)got;
    }

    /*
     * We should now be exactly at EOF.
     */
    if (fgetc(file) != EOF) {
        fclose(file);
        fail(
            failure_base + 3u,
            total
        );
    }

    if (fclose(file) != 0)
        fail(
            failure_base + 4u,
            total
        );

    return crc ^ 0xFFFFFFFFu;
}


static void test_random_windows(
    const char *path)
{
    FILE *file;
    uint32_t i;

    file = fopen(
        path,
        "rb"
    );

    if (file == NULL)
        fail(100, 0);

    if (setvbuf(
            file,
            NULL,
            _IONBF,
            0) != 0) {
        fclose(file);
        fail(101, 0);
    }

    for (
        i = 0;
        i < PROOF_WINDOW_COUNT;
        ++i
    ) {
        const ProofWindowExpected *window =
            &proof_windows[i];

        size_t got;
        uint32_t crc;

        if (window->length >
            sizeof(proof_small_buffer)) {
            fclose(file);
            fail(102, i);
        }

        if (fseek(
                file,
                (long)window->offset,
                SEEK_SET) != 0) {
            fclose(file);
            fail(103, i);
        }

        got = fread(
            proof_small_buffer,
            1,
            window->length,
            file
        );

        if (got != window->length) {
            fclose(file);
            fail(104, i);
        }

        crc = crc32_buffer(
            proof_small_buffer,
            window->length
        );

        if (crc != window->crc32) {
            fclose(file);
            fail(105, i);
        }
    }

    if (fclose(file) != 0)
        fail(106, 0);
}


static void test_seek_end(
    const char *path)
{
    FILE *file;
    size_t got;
    uint32_t crc;

    if (PROOF_TAIL_LENGTH >
        sizeof(proof_small_buffer)) {
        fail(120, PROOF_TAIL_LENGTH);
    }

    file = fopen(
        path,
        "rb"
    );

    if (file == NULL)
        fail(121, 0);

    if (setvbuf(
            file,
            NULL,
            _IONBF,
            0) != 0) {
        fclose(file);
        fail(122, 0);
    }

    if (fseek(
            file,
            -(long)PROOF_TAIL_LENGTH,
            SEEK_END) != 0) {
        fclose(file);
        fail(123, 0);
    }

    got = fread(
        proof_small_buffer,
        1,
        PROOF_TAIL_LENGTH,
        file
    );

    if (got != PROOF_TAIL_LENGTH) {
        fclose(file);
        fail(124, (uint32_t)got);
    }

    crc = crc32_buffer(
        proof_small_buffer,
        PROOF_TAIL_LENGTH
    );

    if (crc != PROOF_TAIL_CRC32) {
        fclose(file);
        fail(125, crc);
    }

    if (fclose(file) != 0)
        fail(126, 0);
}


static void test_secondary_file(
    const ProofFileExpected *expected,
    uint32_t index)
{
    char path[1024];

    struct stat st;

    FILE *file;

    size_t got;
    uint32_t crc;

    uint32_t base =
        200u + (index * 20u);

    if (!build_dvd_path(
            path,
            sizeof(path),
            expected->path)) {
        fail(base + 0u, index);
    }

    memset(
        &st,
        0,
        sizeof(st)
    );

    if (stat(path, &st) != 0)
        fail(base + 1u, index);

    if ((uint32_t)st.st_size !=
        expected->size) {
        fail(
            base + 2u,
            (uint32_t)st.st_size
        );
    }

    if (
        expected->prefix_length
            > sizeof(proof_small_buffer)
        ||
        expected->suffix_length
            > sizeof(proof_small_buffer)
    ) {
        fail(base + 3u, index);
    }

    file = fopen(
        path,
        "rb"
    );

    if (file == NULL)
        fail(base + 4u, index);

    if (setvbuf(
            file,
            NULL,
            _IONBF,
            0) != 0) {
        fclose(file);
        fail(base + 5u, index);
    }

    got = fread(
        proof_small_buffer,
        1,
        expected->prefix_length,
        file
    );

    if (got != expected->prefix_length) {
        fclose(file);
        fail(base + 6u, index);
    }

    crc = crc32_buffer(
        proof_small_buffer,
        expected->prefix_length
    );

    if (crc != expected->prefix_crc32) {
        fclose(file);
        fail(base + 7u, index);
    }

    if (fseek(
            file,
            -(long)expected->suffix_length,
            SEEK_END) != 0) {
        fclose(file);
        fail(base + 8u, index);
    }

    got = fread(
        proof_small_buffer,
        1,
        expected->suffix_length,
        file
    );

    if (got != expected->suffix_length) {
        fclose(file);
        fail(base + 9u, index);
    }

    crc = crc32_buffer(
        proof_small_buffer,
        expected->suffix_length
    );

    if (crc != expected->suffix_crc32) {
        fclose(file);
        fail(base + 10u, index);
    }

    if (fclose(file) != 0)
        fail(base + 11u, index);
}


int main(void)
{
    CH_RemoteDiscSession remote = {0};

    CH_RemoteDiscResult result;

    char primary_path[1024];

    struct stat st;

    uint32_t pass;
    uint32_t i;

    usleep(3000000);

    result = CH_RemoteDiscOpen(
        &remote,
        PROOF_GECKO_CHANNEL
    );

    if (result != CH_REMOTE_DISC_RESULT_OK)
        fail(1, (uint32_t)(-result));

    if (!CH_DVDMountRemote(&remote))
        fail(2, 0);

    if (!build_dvd_path(
            primary_path,
            sizeof(primary_path),
            PROOF_PRIMARY_PATH)) {
        fail(3, 0);
    }

    memset(
        &st,
        0,
        sizeof(st)
    );

    if (stat(
            primary_path,
            &st) != 0) {
        fail(4, 0);
    }

    if ((uint32_t)st.st_size !=
        PROOF_PRIMARY_SIZE) {
        fail(
            5,
            (uint32_t)st.st_size
        );
    }

    /*
     * Repeated complete-file streaming.
     *
     * Each pass reopens the file from scratch and validates its complete
     * CRC32 against Linux-side truth generated directly from the ISO.
     */
    for (
        pass = 0;
        pass < PROOF_STREAM_PASSES;
        ++pass
    ) {
        uint32_t crc =
            stream_entire_file(
                primary_path,
                PROOF_PRIMARY_SIZE,
                20u + (pass * 5u)
            );

        if (crc != PROOF_PRIMARY_CRC32)
            fail(80, crc);
    }

    /*
     * Keep one file open and seek all over it.
     */
    test_random_windows(
        primary_path
    );

    /*
     * Explicit SEEK_END coverage.
     */
    test_seek_end(
        primary_path
    );

    /*
     * Resolve/stat/open/read/seek other real FST entries.
     */
    for (
        i = 0;
        i < PROOF_FILE_COUNT;
        ++i
    ) {
        test_secondary_file(
            &proof_files[i],
            i
        );
    }

    CH_DVDUnmount();

    /*
     * dvd:/ unmounting must not steal caller ownership of the transport.
     */
    if (!CH_RemoteDiscIsOpen(&remote))
        fail(400, 0);

    CH_RemoteDiscClose(&remote);

    if (CH_RemoteDiscIsOpen(&remote))
        fail(401, 0);

    {
        static const char pass_marker[8] = {
            'C','H','P','A','S','S','7','!'
        };

        send_marker(
            pass_marker
        );
    }

    send_done();

    halt_forever();

    return 0;
}
