#!/usr/bin/env python3

"""
CarryHandle PC-hosted GameCube remote-disc server.

The GameCube side issues CHR3 byte-range requests through USB Gecko.
This server responds with ordered CHNK frames followed by an end-to-end
CRC32.

Normal operation is intentionally indefinite. Stop with Ctrl+C.

Non-protocol GameCube -> PC traffic is tolerated. This matters because
libogc2/SYS_Report or loader diagnostics may share the USB Gecko transmit
stream during development. The server scans for CHR3 framing rather than
assuming every received byte belongs to this protocol.
"""

import argparse
import errno
import os
import select
import struct
import sys
import termios
import time
import tty
import zlib


PROTOCOL_MAGIC = b"CHR3"
PROTOCOL_VERSION = 1
OP_READ = 1

CHUNK_SIZE = 1024


def write_exact(fd, data):
    position = 0

    while position < len(data):
        _, writable, _ = select.select(
            [],
            [fd],
            [],
            1.0,
        )

        if not writable:
            continue

        try:
            count = os.write(
                fd,
                data[position:],
            )
        except BlockingIOError:
            continue

        if count <= 0:
            raise RuntimeError(
                "USB Gecko write returned no progress"
            )

        position += count


def configure_serial(fd):
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


def open_serial(path, timeout):
    deadline = time.monotonic() + timeout
    last_error = None

    while True:
        try:
            fd = os.open(
                path,
                os.O_RDWR
                | os.O_NOCTTY
                | os.O_NONBLOCK,
            )

            configure_serial(fd)

            return fd

        except OSError as exc:
            last_error = exc

            if exc.errno not in {
                errno.EBUSY,
                errno.EIO,
                errno.ENOENT,
            }:
                raise

            if time.monotonic() >= deadline:
                raise RuntimeError(
                    "could not acquire USB Gecko "
                    f"within {timeout:.1f}s: {last_error}"
                ) from exc

            time.sleep(0.05)


def format_sideband(data):
    parts = []

    for value in data:
        if value == 0x0A:
            parts.append("\\n")
        elif value == 0x0D:
            parts.append("\\r")
        elif value == 0x09:
            parts.append("\\t")
        elif 0x20 <= value <= 0x7E:
            parts.append(chr(value))
        else:
            parts.append(
                f"\\x{value:02X}"
            )

    return "".join(parts)


class GeckoInput:
    def __init__(
        self,
        fd,
        exit_marker=None,
        show_sideband=False,
    ):
        self.fd = fd
        self.pending = bytearray()

        self.exit_marker = (
            exit_marker.encode("ascii")
            if exit_marker is not None
            else None
        )

        self.show_sideband = show_sideband

    def emit_sideband(self, data):
        if not data:
            return

        if self.show_sideband:
            print(
                "[GC sideband] "
                + format_sideband(data)
            )

    def read_more(self):
        readable, _, _ = select.select(
            [self.fd],
            [],
            [],
            1.0,
        )

        if not readable:
            return

        try:
            data = os.read(
                self.fd,
                4096,
            )
        except BlockingIOError:
            return

        if data:
            self.pending.extend(data)

    def next_event(self):
        markers = [PROTOCOL_MAGIC]

        if self.exit_marker:
            markers.append(self.exit_marker)

        keep = max(len(x) for x in markers) - 1

        while True:
            found = []

            for marker in markers:
                index = self.pending.find(marker)

                if index >= 0:
                    found.append(
                        (index, marker)
                    )

            if found:
                index, marker = min(
                    found,
                    key=lambda item: item[0],
                )

                if index > 0:
                    self.emit_sideband(
                        bytes(self.pending[:index])
                    )

                    del self.pending[:index]

                if marker == PROTOCOL_MAGIC:
                    if len(self.pending) < 16:
                        self.read_more()
                        continue

                    packet = bytes(
                        self.pending[:16]
                    )

                    del self.pending[:16]

                    return (
                        "request",
                        packet,
                    )

                del self.pending[
                    :len(marker)
                ]

                return (
                    "exit",
                    marker,
                )

            if len(self.pending) > keep:
                sideband_length = (
                    len(self.pending) - keep
                )

                self.emit_sideband(
                    bytes(
                        self.pending[
                            :sideband_length
                        ]
                    )
                )

                del self.pending[
                    :sideband_length
                ]

            self.read_more()


def parse_request(packet):
    if len(packet) != 16:
        raise RuntimeError(
            f"invalid request size {len(packet)}"
        )

    if packet[:4] != PROTOCOL_MAGIC:
        raise RuntimeError(
            "invalid request magic"
        )

    version = packet[4]
    operation = packet[5]

    reserved = packet[6:8]

    offset = struct.unpack(
        ">I",
        packet[8:12],
    )[0]

    length = struct.unpack(
        ">I",
        packet[12:16],
    )[0]

    if version != PROTOCOL_VERSION:
        raise RuntimeError(
            f"unsupported protocol version {version}"
        )

    if operation != OP_READ:
        raise RuntimeError(
            f"unsupported operation {operation}"
        )

    if reserved != b"\x00\x00":
        raise RuntimeError(
            "reserved request bytes are nonzero"
        )

    return offset, length


def serve_read(
    fd,
    image,
    image_size,
    number,
    offset,
    length,
):
    if offset + length > image_size:
        raise RuntimeError(
            "remote read exceeds image bounds: "
            f"offset=0x{offset:08X} "
            f"length={length:,} "
            f"image={image_size:,}"
        )

    start = time.monotonic()

    write_exact(
        fd,
        b"CHOK"
        + struct.pack(">I", length)
        + struct.pack(">I", CHUNK_SIZE),
    )

    image.seek(offset)

    remaining = length
    sequence = 0
    crc = 0

    while remaining > 0:
        requested = min(
            CHUNK_SIZE,
            remaining,
        )

        payload = image.read(requested)

        if len(payload) != requested:
            raise RuntimeError(
                "short read from image"
            )

        write_exact(
            fd,
            b"CHNK"
            + struct.pack(">I", sequence)
            + struct.pack(">I", len(payload)),
        )

        write_exact(
            fd,
            payload,
        )

        crc = zlib.crc32(
            payload,
            crc,
        )

        remaining -= len(payload)
        sequence += 1

    crc &= 0xFFFFFFFF

    write_exact(
        fd,
        b"CHDN"
        + struct.pack(">I", crc),
    )

    elapsed = max(
        time.monotonic() - start,
        0.000001,
    )

    mib = length / (1024.0 * 1024.0)
    rate = mib / elapsed

    print(
        f"#{number:06d} "
        f"offset=0x{offset:08X} "
        f"length={length:9,d} "
        f"chunks={sequence:5d} "
        f"crc32={crc:08X} "
        f"{elapsed * 1000.0:8.2f} ms "
        f"{rate:7.2f} MiB/s"
    )

    sys.stdout.flush()


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Serve a GameCube disc image over "
            "CarryHandle's USB Gecko remote-disc protocol."
        )
    )

    parser.add_argument(
        "--device",
        required=True,
        help=(
            "USB Gecko serial device, preferably "
            "/dev/serial/by-id/..."
        ),
    )

    parser.add_argument(
        "--image",
        required=True,
        help="GameCube .iso/.gcm image to serve.",
    )

    parser.add_argument(
        "--open-timeout",
        type=float,
        default=3.0,
        help=(
            "Seconds to wait for wiiload to release "
            "the serial device. Default: 3."
        ),
    )

    parser.add_argument(
        "--show-sideband",
        action="store_true",
        help=(
            "Display non-CHR3 GameCube transmit data "
            "instead of silently discarding it."
        ),
    )

    parser.add_argument(
        "--exit-marker",
        default=None,
        help=(
            "Optional ASCII marker that exits successfully "
            "when observed. Intended for hardware tests only."
        ),
    )

    args = parser.parse_args()

    image_path = os.path.abspath(
        os.path.expanduser(args.image)
    )

    if not os.path.isfile(image_path):
        raise RuntimeError(
            f"image does not exist: {image_path}"
        )

    image_size = os.path.getsize(
        image_path
    )

    print(
        "============================================================"
    )
    print(
        "CARRYHANDLE — REMOTE DISC SERVER"
    )
    print(
        "============================================================"
    )
    print()
    print(f"Device : {args.device}")
    print(f"Image  : {image_path}")
    print(f"Size   : {image_size:,} bytes")
    print(f"Chunk  : {CHUNK_SIZE} bytes")

    if args.exit_marker:
        print(
            f"Test exit marker : {args.exit_marker!r}"
        )
    else:
        print(
            "Mode   : continuous; Ctrl+C to stop"
        )

    print()

    fd = open_serial(
        args.device,
        args.open_timeout,
    )

    try:
        print("USB Gecko opened.")
        print(
            "Serving CHR3 reads..."
        )
        print()

        stream = GeckoInput(
            fd,
            exit_marker=args.exit_marker,
            show_sideband=args.show_sideband,
        )

        request_number = 0

        with open(
            image_path,
            "rb",
            buffering=0,
        ) as image:
            while True:
                kind, value = (
                    stream.next_event()
                )

                if kind == "exit":
                    print()
                    print(
                        "============================================================"
                    )
                    print(
                        "REMOTE DISC TEST MARKER RECEIVED"
                    )
                    print(
                        "============================================================"
                    )
                    print()
                    print(
                        f"Marker : "
                        f"{value.decode('ascii')!r}"
                    )
                    print(
                        f"Reads  : {request_number}"
                    )

                    return 0

                offset, length = (
                    parse_request(value)
                )

                serve_read(
                    fd,
                    image,
                    image_size,
                    request_number,
                    offset,
                    length,
                )

                request_number += 1

    finally:
        os.close(fd)


if __name__ == "__main__":
    try:
        raise SystemExit(main())

    except KeyboardInterrupt:
        print()
        print("Remote-disc server stopped.")
        raise SystemExit(0)

    except Exception as exc:
        print(
            f"ERROR: {type(exc).__name__}: {exc}",
            file=sys.stderr,
        )

        raise SystemExit(1)
