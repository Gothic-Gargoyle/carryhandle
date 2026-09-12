#!/usr/bin/env python3

import argparse
import os
import select
import struct
import termios
import time
import tty
import zlib


TESTS = [
    (0x00002443, 8193),
    (0x00123457, 65537),
    (0x02000003, 262147),
]

CHUNK_SIZE = 1024


def hx(data):
    return " ".join(f"{x:02X}" for x in data)


def read_exact(fd, count, timeout=30.0):
    data = bytearray()
    deadline = time.monotonic() + timeout

    while len(data) < count:
        remaining = deadline - time.monotonic()

        if remaining <= 0:
            raise TimeoutError(
                f"read timeout: {len(data)}/{count}"
            )

        ready, _, _ = select.select(
            [fd], [], [], remaining
        )

        if not ready:
            continue

        try:
            chunk = os.read(fd, count - len(data))
        except BlockingIOError:
            continue

        if chunk:
            data.extend(chunk)

    return bytes(data)


def write_exact(fd, data, timeout=30.0):
    pos = 0
    deadline = time.monotonic() + timeout

    while pos < len(data):
        remaining = deadline - time.monotonic()

        if remaining <= 0:
            raise TimeoutError(
                f"write timeout: {pos}/{len(data)}"
            )

        _, ready, _ = select.select(
            [], [fd], [], remaining
        )

        if not ready:
            continue

        try:
            n = os.write(fd, data[pos:])
        except BlockingIOError:
            continue

        if n > 0:
            pos += n


def configure(fd):
    tty.setraw(fd, termios.TCSANOW)

    a = termios.tcgetattr(fd)

    a[2] &= ~termios.CSIZE
    a[2] |= termios.CS8
    a[2] |= termios.CLOCAL
    a[2] |= termios.CREAD
    a[2] &= ~termios.PARENB
    a[2] &= ~termios.CSTOPB

    if hasattr(termios, "CRTSCTS"):
        a[2] |= termios.CRTSCTS

    if hasattr(termios, "B115200"):
        a[4] = termios.B115200
        a[5] = termios.B115200

    termios.tcsetattr(fd, termios.TCSANOW, a)
    termios.tcflush(fd, termios.TCIOFLUSH)


def main():
    ap = argparse.ArgumentParser()

    ap.add_argument("device")
    ap.add_argument("image")

    args = ap.parse_args()

    print("============================================================")
    print("CARRYHANDLE — LARGE/CHUNKED REMOTE READ SERVER")
    print("============================================================")
    print()
    print(f"Device     : {args.device}")
    print(f"Image      : {args.image}")
    print(f"Chunk size : {CHUNK_SIZE} bytes")
    print()

    local = []

    with open(args.image, "rb") as f:
        for i, (offset, length) in enumerate(TESTS):
            f.seek(offset)
            data = f.read(length)

            if len(data) != length:
                raise RuntimeError(
                    f"short local ISO read #{i}"
                )

            crc = zlib.crc32(data) & 0xffffffff
            local.append((data, crc))

            chunks = (
                length + CHUNK_SIZE - 1
            ) // CHUNK_SIZE

            print(
                f"Test #{i}: "
                f"offset=0x{offset:08X} "
                f"length={length:,} "
                f"chunks={chunks} "
                f"crc32={crc:08X}"
            )

    print()

    fd = os.open(
        args.device,
        os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK,
    )

    try:
        configure(fd)

        print("USB Gecko opened.")
        print("Waiting for GameCube...")
        print()

        for index, (
            (expected_offset, expected_length),
            (data, expected_crc),
        ) in enumerate(zip(TESTS, local)):

            request = read_exact(fd, 16)

            if request[:8] == b"CHFAIL3!":
                failed_index = struct.unpack(
                    ">I", request[8:12]
                )[0]

                stage = struct.unpack(
                    ">I", request[12:16]
                )[0]

                raise RuntimeError(
                    f"GC failure: test={failed_index} "
                    f"stage={stage}"
                )

            if request[:4] != b"CHR3":
                raise RuntimeError(
                    "unexpected request: " + hx(request)
                )

            version = request[4]
            operation = request[5]
            offset = struct.unpack(
                ">I", request[8:12]
            )[0]
            length = struct.unpack(
                ">I", request[12:16]
            )[0]

            print("------------------------------------------------------------")
            print(f"GC -> PC logical read #{index}")
            print("------------------------------------------------------------")
            print()
            print(f"Version : {version}")
            print(f"Op      : {operation}")
            print(f"Offset  : 0x{offset:08X}")
            print(f"Length  : {length:,}")
            print()

            if version != 1 or operation != 1:
                raise RuntimeError(
                    "bad protocol version/op"
                )

            if offset != expected_offset:
                raise RuntimeError(
                    f"offset mismatch: {offset:#x}"
                )

            if length != expected_length:
                raise RuntimeError(
                    f"length mismatch: {length}"
                )

            response = (
                b"CHOK"
                + struct.pack(">I", length)
                + struct.pack(">I", CHUNK_SIZE)
            )

            write_exact(fd, response)

            position = 0
            sequence = 0

            while position < length:
                payload = data[
                    position:
                    position + CHUNK_SIZE
                ]

                header = (
                    b"CHNK"
                    + struct.pack(">I", sequence)
                    + struct.pack(">I", len(payload))
                )

                write_exact(fd, header)
                write_exact(fd, payload)

                position += len(payload)

                if (
                    sequence == 0
                    or position == length
                    or sequence % 64 == 0
                ):
                    print(
                        f"  chunk {sequence:4d}: "
                        f"{position:7,d}/{length:,} bytes"
                    )

                sequence += 1

            write_exact(
                fd,
                b"CHDN"
                + struct.pack(">I", expected_crc)
            )

            ack = read_exact(fd, 16)

            if ack[:8] == b"CHFAIL3!":
                failed_index = struct.unpack(
                    ">I", ack[8:12]
                )[0]

                stage = struct.unpack(
                    ">I", ack[12:16]
                )[0]

                raise RuntimeError(
                    f"GC failure: test={failed_index} "
                    f"stage={stage}"
                )

            if ack[:8] != b"CHACK3!!":
                raise RuntimeError(
                    "bad ACK: " + hx(ack)
                )

            ack_index = struct.unpack(
                ">I", ack[8:12]
            )[0]

            ack_crc = struct.unpack(
                ">I", ack[12:16]
            )[0]

            if ack_index != index:
                raise RuntimeError(
                    f"ACK index mismatch: {ack_index}"
                )

            if ack_crc != expected_crc:
                raise RuntimeError(
                    f"ACK CRC mismatch: "
                    f"{ack_crc:08X} != {expected_crc:08X}"
                )

            print()
            print(
                f"GameCube ACK #{index}: "
                f"CRC32 {ack_crc:08X} PASS"
            )
            print()

        result = read_exact(fd, 8)

        print("------------------------------------------------------------")
        print("FINAL RESULT")
        print("------------------------------------------------------------")
        print()
        print(f"HEX   : {hx(result)}")
        print(f"ASCII : {result!r}")
        print()

        if result != b"CHPASS3!":
            raise RuntimeError(
                f"unexpected final result {result!r}"
            )

        print("============================================================")
        print("CH-USBGECKO-REMOTE-READ-3: PASS")
        print("============================================================")
        print()
        print("Large logical reads : PASS")
        print("1024-byte chunking  : PASS")
        print("Sequence checking   : PASS")
        print("Odd final chunks    : PASS")
        print("End-to-end CRC32    : PASS")
        print("262 KiB read        : PASS")

        return 0

    finally:
        os.close(fd)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print()
        print("============================================================")
        print("CH-USBGECKO-REMOTE-READ-3: ERROR")
        print("============================================================")
        print()
        print(f"{type(exc).__name__}: {exc}")
        raise SystemExit(1)
