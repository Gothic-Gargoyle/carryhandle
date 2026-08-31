#!/usr/bin/env python3

"""
Encode Nintendo GameCube Memory Card presentation assets.

CarryHandle CARD presentation v1 uses:

    banner:
        96 x 32
        CI8
        3072-byte GX tiled image
        512-byte RGB5A3 palette

    icon:
        32 x 32
        CI8
        1024-byte GX tiled image
        512-byte RGB5A3 palette

    comments:
        two 32-byte CARD comment fields

Binary output layout:

    0x0000  banner CI8
    0x0c00  banner RGB5A3 palette
    0x0e00  icon CI8
    0x1200  icon RGB5A3 palette
    0x1400  comment line 1
    0x1420  comment line 2
    0x1440  end

Total size: 5184 bytes.

This tool deliberately does not decide where the blob lives inside a
Memory Card file. That is runtime CARD presentation policy.
"""

from pathlib import Path
from PIL import Image

import argparse
import hashlib
import struct
import sys


BANNER_WIDTH = 96
BANNER_HEIGHT = 32

ICON_WIDTH = 32
ICON_HEIGHT = 32

BANNER_IMAGE_SIZE = 3072
ICON_IMAGE_SIZE = 1024
PALETTE_SIZE = 512

COMMENT_LINE_SIZE = 32
COMMENT_SIZE = 64

PRESENTATION_SIZE = (
    BANNER_IMAGE_SIZE
    + PALETTE_SIZE
    + ICON_IMAGE_SIZE
    + PALETTE_SIZE
    + COMMENT_SIZE
)


class CardAssetError(ValueError):
    pass


def fail(message):
    raise CardAssetError(message)


def round_bits(value, bits):
    maximum = (1 << bits) - 1

    return (
        value * maximum + 127
    ) // 255


def expand_bits(value, bits):
    maximum = (1 << bits) - 1

    return (
        value * 255 + maximum // 2
    ) // maximum


def encode_rgb5a3(r, g, b, a=255):
    if a >= 224:
        return (
            0x8000
            | (round_bits(r, 5) << 10)
            | (round_bits(g, 5) << 5)
            | round_bits(b, 5)
        )

    return (
        (round_bits(a, 3) << 12)
        | (round_bits(r, 4) << 8)
        | (round_bits(g, 4) << 4)
        | round_bits(b, 4)
    )


def decode_rgb5a3(value):
    if value & 0x8000:
        return (
            expand_bits(
                (value >> 10) & 0x1f,
                5,
            ),
            expand_bits(
                (value >> 5) & 0x1f,
                5,
            ),
            expand_bits(
                value & 0x1f,
                5,
            ),
            255,
        )

    return (
        expand_bits(
            (value >> 8) & 0x0f,
            4,
        ),
        expand_bits(
            (value >> 4) & 0x0f,
            4,
        ),
        expand_bits(
            value & 0x0f,
            4,
        ),
        expand_bits(
            (value >> 12) & 0x07,
            3,
        ),
    )


def require_image_size(
    image,
    width,
    height,
    field,
):
    if image.size != (width, height):
        fail(
            f"{field} must be "
            f"{width}x{height}; "
            f"got {image.width}x{image.height}"
        )


def quantize(image):
    return image.convert("RGB").quantize(
        colors=256,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    )


def palette_values(image):
    raw = list(
        image.getpalette() or []
    )

    raw.extend(
        [0] * max(
            0,
            768 - len(raw),
        )
    )

    result = []

    for index in range(256):
        result.append(
            encode_rgb5a3(
                raw[index * 3 + 0],
                raw[index * 3 + 1],
                raw[index * 3 + 2],
                255,
            )
        )

    return result


def encode_palette(values):
    data = b"".join(
        struct.pack(">H", value)
        for value in values
    )

    if len(data) != PALETTE_SIZE:
        fail(
            "RGB5A3 palette encoded to "
            f"{len(data)} bytes"
        )

    return data


def encode_ci8(image):
    width, height = image.size

    if width % 8 != 0:
        fail(
            f"CI8 width {width} "
            "must be divisible by 8"
        )

    if height % 4 != 0:
        fail(
            f"CI8 height {height} "
            "must be divisible by 4"
        )

    pixels = image.load()
    result = bytearray()

    #
    # Nintendo GX CI8 tile order:
    #
    #     8 pixels wide
    #     4 pixels high
    #
    for tile_y in range(
        0,
        height,
        4,
    ):
        for tile_x in range(
            0,
            width,
            8,
        ):
            for y in range(4):
                for x in range(8):
                    result.append(
                        pixels[
                            tile_x + x,
                            tile_y + y,
                        ]
                    )

    expected = width * height

    if len(result) != expected:
        fail(
            "CI8 output length "
            f"{len(result)} != {expected}"
        )

    return bytes(result)


def decode_ci8(
    data,
    width,
    height,
):
    expected = width * height

    if len(data) != expected:
        fail(
            "CI8 input length "
            f"{len(data)} != {expected}"
        )

    result = [0] * expected
    position = 0

    for tile_y in range(
        0,
        height,
        4,
    ):
        for tile_x in range(
            0,
            width,
            8,
        ):
            for y in range(4):
                for x in range(8):
                    destination = (
                        (tile_y + y) * width
                        + tile_x
                        + x
                    )

                    result[destination] = (
                        data[position]
                    )

                    position += 1

    return result


def encode_asset(
    image,
    expected_size,
):
    indexed = quantize(image)

    texture = encode_ci8(
        indexed
    )

    if len(texture) != expected_size:
        fail(
            f"texture encoded to "
            f"{len(texture)} bytes; "
            f"expected {expected_size}"
        )

    source_indices = list(
        indexed.get_flattened_data()
    )

    decoded_indices = decode_ci8(
        texture,
        indexed.width,
        indexed.height,
    )

    if source_indices != decoded_indices:
        fail(
            "CI8 tile round-trip mismatch"
        )

    values = palette_values(
        indexed
    )

    palette = encode_palette(
        values
    )

    return (
        texture,
        palette,
        indexed,
        values,
    )


def encode_comment_line(
    text,
    field,
):
    try:
        encoded = text.encode(
            "ascii"
        )
    except UnicodeEncodeError:
        fail(
            f"{field} must be ASCII"
        )

    if len(encoded) > 31:
        fail(
            f"{field} exceeds "
            "31 ASCII bytes"
        )

    return encoded + bytes(
        COMMENT_LINE_SIZE
        - len(encoded)
    )


def preview_image(
    indexed,
    values,
):
    image = Image.new(
        "RGB",
        indexed.size,
    )

    source = indexed.load()
    destination = image.load()

    for y in range(
        indexed.height
    ):
        for x in range(
            indexed.width
        ):
            r, g, b, _a = (
                decode_rgb5a3(
                    values[
                        source[x, y]
                    ]
                )
            )

            destination[x, y] = (
                r,
                g,
                b,
            )

    return image


def write_if_changed(
    path,
    data,
):
    path = Path(path)

    path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    try:
        old = path.read_bytes()
    except FileNotFoundError:
        old = None

    if old == data:
        return False

    path.write_bytes(data)

    return True


def build_presentation(
    banner_path,
    icon_path,
    title,
    comment,
):
    try:
        with Image.open(
            banner_path
        ) as source:
            banner = source.convert(
                "RGB"
            )
    except OSError as exc:
        fail(
            f"could not read banner: {exc}"
        )

    try:
        with Image.open(
            icon_path
        ) as source:
            icon = source.convert(
                "RGB"
            )
    except OSError as exc:
        fail(
            f"could not read icon: {exc}"
        )

    require_image_size(
        banner,
        BANNER_WIDTH,
        BANNER_HEIGHT,
        "banner",
    )

    require_image_size(
        icon,
        ICON_WIDTH,
        ICON_HEIGHT,
        "icon",
    )

    (
        banner_texture,
        banner_palette,
        banner_indexed,
        banner_palette_values,
    ) = encode_asset(
        banner,
        BANNER_IMAGE_SIZE,
    )

    (
        icon_texture,
        icon_palette,
        icon_indexed,
        icon_palette_values,
    ) = encode_asset(
        icon,
        ICON_IMAGE_SIZE,
    )

    comments = (
        encode_comment_line(
            title,
            "title",
        )
        + encode_comment_line(
            comment,
            "comment",
        )
    )

    data = (
        banner_texture
        + banner_palette
        + icon_texture
        + icon_palette
        + comments
    )

    if len(data) != PRESENTATION_SIZE:
        fail(
            "CARD presentation encoded to "
            f"{len(data)} bytes; "
            f"expected {PRESENTATION_SIZE}"
        )

    return {
        "data": data,
        "banner_texture": banner_texture,
        "banner_palette": banner_palette,
        "banner_indexed": banner_indexed,
        "banner_palette_values": (
            banner_palette_values
        ),
        "icon_texture": icon_texture,
        "icon_palette": icon_palette,
        "icon_indexed": icon_indexed,
        "icon_palette_values": (
            icon_palette_values
        ),
        "comments": comments,
    }


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Encode GameCube Memory Card "
            "banner/icon/comment presentation data."
        )
    )

    parser.add_argument(
        "--banner",
        required=True,
    )

    parser.add_argument(
        "--icon",
        required=True,
    )

    parser.add_argument(
        "--title",
        required=True,
    )

    parser.add_argument(
        "--comment",
        required=True,
    )

    parser.add_argument(
        "--output",
        required=True,
    )

    parser.add_argument(
        "--preview-dir",
    )

    args = parser.parse_args()

    try:
        presentation = build_presentation(
            args.banner,
            args.icon,
            args.title,
            args.comment,
        )
    except CardAssetError as exc:
        print(
            f"CarryHandle CARD asset error: {exc}",
            file=sys.stderr,
        )

        return 2

    changed = write_if_changed(
        args.output,
        presentation["data"],
    )

    if args.preview_dir:
        preview_dir = Path(
            args.preview_dir
        )

        preview_dir.mkdir(
            parents=True,
            exist_ok=True,
        )

        preview_image(
            presentation[
                "banner_indexed"
            ],
            presentation[
                "banner_palette_values"
            ],
        ).save(
            preview_dir
            / "card-banner-encoded.png"
        )

        preview_image(
            presentation[
                "icon_indexed"
            ],
            presentation[
                "icon_palette_values"
            ],
        ).save(
            preview_dir
            / "card-icon-encoded.png"
        )

    data = presentation["data"]

    print(
        "CarryHandle CARD presentation generated"
    )

    print(
        f"  banner CI8     : "
        f"{len(presentation['banner_texture'])} bytes"
    )

    print(
        f"  banner palette : "
        f"{len(presentation['banner_palette'])} bytes"
    )

    print(
        f"  icon CI8       : "
        f"{len(presentation['icon_texture'])} bytes"
    )

    print(
        f"  icon palette   : "
        f"{len(presentation['icon_palette'])} bytes"
    )

    print(
        f"  comments       : "
        f"{len(presentation['comments'])} bytes"
    )

    print(
        f"  presentation   : "
        f"{len(data)} bytes"
    )

    print(
        f"  sha256         : "
        f"{hashlib.sha256(data).hexdigest()}"
    )

    print(
        f"  output         : "
        f"{args.output} "
        f"({'updated' if changed else 'unchanged'})"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(
        main()
    )
