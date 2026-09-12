#!/usr/bin/env python3

import argparse
import os
import select
import struct
import termios
import time
import tty
import zlib


CHUNK_SIZE = 1024


def read_exact(fd, count, timeout=30.0):
    result = bytearray()
    deadline = time.monotonic() + timeout

    while len(result) < count:
        remaining = deadline - time.monotonic()

        if remaining <= 0:
            raise TimeoutError(
                f"read timeout: {len(result)}/{count}"
            )

        ready, _, _ = select.select(
            [fd],
            [],
            [],
            remaining,
        )

        if not ready:
            continue

        try:
            chunk = os.read(
                fd,
                count - len(result),
            )
        except BlockingIOError:
            continue

        if chunk:
            result.extend(chunk)

    return bytes(result)


def write_exact(fd, data, timeout=30.0):
    position = 0
    deadline = time.monotonic() + timeout

    while position < len(data):
        remaining = deadline - time.monotonic()

        if remaining <= 0:
            raise TimeoutError(
                f"write timeout: "
                f"{position}/{len(data)}"
            )

        _, ready, _ = select.select(
            [],
            [fd],
            [],
            remaining,
        )

        if not ready:
            continue

        try:
            written = os.write(
                fd,
                data[position:],
            )
        except BlockingIOError:
            continue

        if written > 0:
            position += written


def configure(fd):
    tty.setraw(
        fd,
        termios.TCSANOW,
    )

    attrs = termios.tcgetattr(fd)

    attrs[2] &= ~termios.CSIZE
    attrs[2] |= termios.CS8
    attrs[2] |= termios.CLOCAL
    attrs[2] |= termios.CREAD
    attrs[2] &= ~termios.PARENB
    attrs[2] &= ~termios.CSTOPB

    if hasattr(termios, "CRTSCTS"):
        attrs[2] |= termios.CRTSCTS

    if hasattr(termios, "B115200"):
        attrs[4] = termios.B115200
        attrs[5] = termios.B115200

    termios.tcsetattr(
        fd,
        termios.TCSANOW,
        attrs,
    )

    termios.tcflush(
        fd,
        termios.TCIOFLUSH,
    )


def serve_read(
    fd,
    image,
    image_size,
    request_number,
    request,
):
    version = request[4]
    operation = request[5]
    reserved = request[6:8]

    offset = struct.unpack(
        ">I",
        request[8:12],
    )[0]

    length = struct.unpack(
        ">I",
        request[12:16],
    )[0]

    print(
        "------------------------------------------------------------"
    )
    print(
        f"REMOTE READ #{request_number}"
    )
    print(
        "------------------------------------------------------------"
    )
    print()
    print(f"Version : {version}")
    print(f"Op      : {operation}")
    print(f"Offset  : 0x{offset:08X}")
    print(f"Length  : {length:,}")
    print()

    if version != 1:
        raise RuntimeError(
            f"unsupported protocol version {version}"
        )

    if operation != 1:
        raise RuntimeError(
            f"unsupported operation {operation}"
        )

    if reserved != b"\x00\x00":
        raise RuntimeError(
            "reserved request bytes are nonzero"
        )

    if offset + length > image_size:
        raise RuntimeError(
            "request exceeds image bounds"
        )

    image.seek(offset)
    payload = image.read(length)

    if len(payload) != length:
        raise RuntimeError(
            "short image read"
        )

    crc = zlib.crc32(payload) & 0xFFFFFFFF

    write_exact(
        fd,
        b"CHOK"
        + struct.pack(">I", length)
        + struct.pack(">I", CHUNK_SIZE),
    )

    position = 0
    sequence = 0

    while position < length:
        chunk = payload[
            position:
            position + CHUNK_SIZE
        ]

        write_exact(
            fd,
            b"CHNK"
            + struct.pack(">I", sequence)
            + struct.pack(">I", len(chunk)),
        )

        write_exact(
            fd,
            chunk,
        )

        position += len(chunk)
        sequence += 1

    write_exact(
        fd,
        b"CHDN"
        + struct.pack(">I", crc),
    )

    print(
        f"Served  : {length:,} bytes "
        f"in {sequence} chunk(s)"
    )
    print(f"CRC32   : {crc:08X}")
    print()


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("device")
    parser.add_argument("image")

    args = parser.parse_args()

    image_size = os.path.getsize(
        args.image
    )

    print(
        "============================================================"
    )
    print(
        "CARRYHANDLE — GENERIC REMOTE DISC SERVER"
    )
    print(
        "============================================================"
    )
    print()
    print(f"Device : {args.device}")
    print(f"Image  : {args.image}")
    print(f"Size   : {image_size:,} bytes")
    print()

    fd = os.open(
        args.device,
        os.O_RDWR
        | os.O_NOCTTY
        | os.O_NONBLOCK,
    )

    try:
        configure(fd)

        print("USB Gecko opened.")
        print(
            "Serving arbitrary CHR3 reads..."
        )
        print()

        request_number = 0

        with open(
            args.image,
            "rb",
        ) as image:

            while True:
                prefix = read_exact(
                    fd,
                    4,
                )

                if prefix == b"CHR3":
                    rest = read_exact(
                        fd,
                        12,
                    )

                    request = prefix + rest

                    serve_read(
                        fd,
                        image,
                        image_size,
                        request_number,
                        request,
                    )

                    request_number += 1
                    continue

                if prefix == b"CHPA":
                    marker = (
                        prefix
                        + read_exact(fd, 4)
                    )

                    print(
                        "============================================================"
                    )
                    print(
                        "GAMECUBE FINAL RESULT"
                    )
                    print(
                        "============================================================"
                    )
                    print()
                    print(f"ASCII : {marker!r}")
                    print(
                        f"Reads : {request_number}"
                    )
                    print()

                    if marker != b"CHPASS5!":
                        raise RuntimeError(
                            f"unexpected pass marker "
                            f"{marker!r}"
                        )

                    print(
                        "============================================================"
                    )
                    print(
                        "CH-REMOTE-FST-BOOTSTRAP-1: PASS"
                    )
                    print(
                        "============================================================"
                    )
                    print()
                    print(
                        "Remote boot-info read : PASS"
                    )
                    print(
                        "Remote FST discovery  : PASS"
                    )
                    print(
                        "Remote FST fetch      : PASS"
                    )
                    print(
                        "FST root validation   : PASS"
                    )
                    print(
                        "Path resolution       : PASS"
                    )
                    print(
                        "Resolved file read    : PASS"
                    )

                    return 0

                if prefix == b"CHFA":
                    rest = read_exact(
                        fd,
                        12,
                    )

                    marker = (
                        prefix
                        + rest[:4]
                    )

                    stage = struct.unpack(
                        ">I",
                        rest[4:8],
                    )[0]

                    detail = struct.unpack(
                        ">I",
                        rest[8:12],
                    )[0]

                    print(
                        "============================================================"
                    )
                    print(
                        "GAMECUBE FAILURE"
                    )
                    print(
                        "============================================================"
                    )
                    print()
                    print(f"Marker : {marker!r}")
                    print(f"Stage  : {stage}")
                    print(
                        f"Detail : "
                        f"0x{detail:08X} "
                        f"({detail})"
                    )

                    return 2

                raise RuntimeError(
                    "unexpected GameCube prefix: "
                    + " ".join(
                        f"{x:02X}"
                        for x in prefix
                    )
                )

    finally:
        os.close(fd)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print()
        print(
            "============================================================"
        )
        print(
            "CH-REMOTE-FST-BOOTSTRAP-1: ERROR"
        )
        print(
            "============================================================"
        )
        print()
        print(
            f"{type(exc).__name__}: {exc}"
        )

        raise SystemExit(1)
