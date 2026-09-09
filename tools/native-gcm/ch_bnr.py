#!/usr/bin/env python3

"""
Generate a Nintendo GameCube BNR1 opening banner from carryhandle.cfg.

Input:
    manifest v4 [presentation]
    exactly 96x32 source banner image

Output:
    BNR1
        0x0000  32-byte header
        0x0020  96x32 tiled RGB5A3 image (6144 bytes)
        0x1820  metadata block (320 bytes)
        total   0x1960 / 6496 bytes

CarryHandle's first BNR implementation deliberately emits BNR1 only.
The source banner must be fully opaque; CarryHandle does not silently
flatten, resize, crop, or otherwise reinterpret presentation artwork.
"""

from pathlib import Path
from PIL import Image

import argparse
import hashlib
import struct
import sys


TOOLS_DIR = Path(__file__).resolve().parent.parent

if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(
        0,
        str(TOOLS_DIR),
    )

from ch_manifest import (  # noqa: E402
    ManifestError,
    load_manifest,
)


BANNER_WIDTH = 96
BANNER_HEIGHT = 32

TILE_WIDTH = 4
TILE_HEIGHT = 4

HEADER_SIZE = 32

IMAGE_SIZE = (
    BANNER_WIDTH
    * BANNER_HEIGHT
    * 2
)

SHORT_NAME_SIZE = 32
SHORT_COMPANY_SIZE = 32
FULL_NAME_SIZE = 64
FULL_COMPANY_SIZE = 64
DESCRIPTION_SIZE = 128

METADATA_SIZE = (
    SHORT_NAME_SIZE
    + SHORT_COMPANY_SIZE
    + FULL_NAME_SIZE
    + FULL_COMPANY_SIZE
    + DESCRIPTION_SIZE
)

BNR1_SIZE = (
    HEADER_SIZE
    + IMAGE_SIZE
    + METADATA_SIZE
)


class BnrError(ValueError):
    pass


def fail(message):
    raise BnrError(message)


def round_bits(
    value,
    bits,
):
    maximum = (1 << bits) - 1

    return (
        value * maximum + 127
    ) // 255


def encode_opaque_rgb5a3(
    r,
    g,
    b,
):
    return (
        0x8000
        | (round_bits(r, 5) << 10)
        | (round_bits(g, 5) << 5)
        | round_bits(b, 5)
    )


def require_opaque(
    image,
):
    if "A" not in image.getbands():
        return

    alpha = image.getchannel("A")

    low, high = alpha.getextrema()

    if (
        low != 255
        or high != 255
    ):
        fail(
            "presentation banner must be fully opaque"
        )


def encode_image(
    image,
):
    if image.size != (
        BANNER_WIDTH,
        BANNER_HEIGHT,
    ):
        fail(
            "presentation banner must be exactly "
            f"{BANNER_WIDTH}x{BANNER_HEIGHT}; "
            f"got {image.width}x{image.height}"
        )

    require_opaque(
        image
    )

    rgb = image.convert("RGB")
    pixels = rgb.load()

    output = bytearray()

    for tile_y in range(
        0,
        BANNER_HEIGHT,
        TILE_HEIGHT,
    ):
        for tile_x in range(
            0,
            BANNER_WIDTH,
            TILE_WIDTH,
        ):
            for y in range(TILE_HEIGHT):
                for x in range(TILE_WIDTH):
                    r, g, b = pixels[
                        tile_x + x,
                        tile_y + y,
                    ]

                    output.extend(
                        struct.pack(
                            ">H",
                            encode_opaque_rgb5a3(
                                r,
                                g,
                                b,
                            ),
                        )
                    )

    if len(output) != IMAGE_SIZE:
        fail(
            "BNR1 image encoded to "
            f"{len(output)} bytes; "
            f"expected {IMAGE_SIZE}"
        )

    return bytes(
        output
    )


def encode_text(
    value,
    size,
    field,
):
    try:
        encoded = value.encode(
            "ascii"
        )
    except UnicodeEncodeError:
        fail(
            f"{field} must be ASCII"
        )

    if len(encoded) >= size:
        fail(
            f"{field} must fit in "
            f"{size - 1} ASCII bytes"
        )

    return (
        encoded
        + bytes(
            size - len(encoded)
        )
    )


def build_bnr(
    manifest_path,
):
    manifest_path = Path(
        manifest_path
    ).resolve()

    manifest = load_manifest(
        manifest_path
    )

    presentation = manifest[
        "presentation"
    ]

    if presentation is None:
        fail(
            "manifest has no [presentation] section"
        )

    banner_path = (
        manifest_path.parent
        / presentation["banner"]
    )

    try:
        with Image.open(
            banner_path
        ) as source:
            image = source.copy()
    except OSError as exc:
        fail(
            "could not read presentation banner: "
            f"{exc}"
        )

    header = (
        b"BNR1"
        + bytes(28)
    )

    raster = encode_image(
        image
    )

    name = manifest[
        "name"
    ]

    company = presentation[
        "company"
    ]

    description = presentation[
        "description"
    ]

    metadata = (
        encode_text(
            name,
            SHORT_NAME_SIZE,
            "application.name",
        )
        + encode_text(
            company,
            SHORT_COMPANY_SIZE,
            "presentation.company",
        )
        + encode_text(
            name,
            FULL_NAME_SIZE,
            "application.name",
        )
        + encode_text(
            company,
            FULL_COMPANY_SIZE,
            "presentation.company",
        )
        + encode_text(
            description,
            DESCRIPTION_SIZE,
            "presentation.description",
        )
    )

    if len(metadata) != METADATA_SIZE:
        fail(
            "BNR1 metadata encoded to "
            f"{len(metadata)} bytes; "
            f"expected {METADATA_SIZE}"
        )

    data = (
        header
        + raster
        + metadata
    )

    if len(data) != BNR1_SIZE:
        fail(
            "BNR1 encoded to "
            f"{len(data)} bytes; "
            f"expected {BNR1_SIZE}"
        )

    return {
        "data": data,
        "manifest_path": manifest_path,
        "banner_path": banner_path,
        "name": name,
        "company": company,
        "description": description,
    }


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

    path.write_bytes(
        data
    )

    return True


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Generate GameCube opening.bnr "
            "from CarryHandle presentation metadata."
        )
    )

    parser.add_argument(
        "--manifest",
        required=True,
    )

    parser.add_argument(
        "--output",
        required=True,
    )

    args = parser.parse_args()

    try:
        result = build_bnr(
            args.manifest
        )
    except (
        ManifestError,
        BnrError,
    ) as exc:
        print(
            "CarryHandle BNR build error: "
            f"{exc}",
            file=sys.stderr,
        )

        return 2

    output_path = Path(
        args.output
    )

    changed = write_if_changed(
        output_path,
        result["data"],
    )

    print(
        "CarryHandle BNR1 generated"
    )

    print(
        "  manifest    : "
        f"{result['manifest_path']}"
    )

    print(
        "  banner      : "
        f"{result['banner_path']}"
    )

    print(
        "  name        : "
        f"{result['name']}"
    )

    print(
        "  company     : "
        f"{result['company']}"
    )

    print(
        "  description : "
        f"{result['description']}"
    )

    print(
        "  output      : "
        f"{output_path} "
        f"({'updated' if changed else 'unchanged'})"
    )

    print(
        "  bytes       : "
        f"{len(result['data'])}"
    )

    print(
        "  sha256      : "
        f"{hashlib.sha256(result['data']).hexdigest()}"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(
        main()
    )
