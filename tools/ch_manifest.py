#!/usr/bin/env python3

"""
CarryHandle build-time application manifest parser.

carryhandle.cfg is host/build metadata.
It is not intended to be parsed by the GameCube executable.
"""

from pathlib import Path, PurePosixPath
import argparse
import configparser
import re
import sys


MANIFEST_VERSION = 3

SUPPORTED_MANIFEST_VERSIONS = frozenset(
    (1, 2, 3)
)

REGIONS = {
    "NTSC-J": 0,
    "NTSC-U": 1,
    "PAL": 2,
}

REGION_C_NAMES = {
    "NTSC-J": "CH_APPLICATION_REGION_NTSC_J",
    "NTSC-U": "CH_APPLICATION_REGION_NTSC_U",
    "PAL": "CH_APPLICATION_REGION_PAL",
}

STORE_ID_RE = re.compile(
    r"^[A-Za-z0-9._-]+$"
)


class ManifestError(ValueError):
    pass


def fail(message):
    raise ManifestError(message)


def require_ascii(
    value,
    field,
    exact=None,
    maximum=None,
):
    try:
        encoded = value.encode("ascii")
    except UnicodeEncodeError:
        fail(f"{field} must be ASCII")

    if not encoded:
        fail(f"{field} must not be empty")

    if exact is not None and len(encoded) != exact:
        fail(
            f"{field} must be exactly "
            f"{exact} ASCII bytes"
        )

    if maximum is not None and len(encoded) > maximum:
        fail(
            f"{field} exceeds "
            f"{maximum} ASCII bytes"
        )

    if any(
        byte < 0x20 or byte > 0x7e
        for byte in encoded
    ):
        fail(
            f"{field} must contain printable ASCII"
        )

    return value


def require_section(
    parser,
    section,
    fields,
):
    if section not in parser:
        fail(
            f"missing section [{section}]"
        )

    actual = set(parser[section].keys())
    expected = set(fields)

    missing = sorted(
        expected - actual
    )

    extra = sorted(
        actual - expected
    )

    if missing:
        fail(
            f"[{section}] missing: "
            + ", ".join(missing)
        )

    if extra:
        fail(
            f"[{section}] unknown field(s): "
            + ", ".join(extra)
        )

    return parser[section]


def require_asset_path(
    value,
    field,
):
    require_ascii(
        value,
        field,
        maximum=255,
    )

    if "\\" in value:
        fail(
            f"{field} must use '/' path separators"
        )

    parsed = PurePosixPath(value)

    if parsed.is_absolute():
        fail(
            f"{field} must be relative to carryhandle.cfg"
        )

    if any(
        part in ("", ".", "..")
        for part in parsed.parts
    ):
        fail(
            f"{field} contains an invalid path component"
        )

    return value


def load_manifest(path):
    parser = configparser.ConfigParser(
        interpolation=None,
        strict=True,
    )

    #
    # Manifest keys are deliberately case-sensitive.
    #
    parser.optionxform = str

    try:
        with Path(path).open(
            "r",
            encoding="utf-8",
        ) as handle:
            parser.read_file(handle)

    except (
        OSError,
        configparser.Error,
    ) as exc:
        fail(str(exc))

    allowed_sections = {
        "carryhandle",
        "application",
        "persistence",
        "memcard",
    }

    unknown = (
        set(parser.sections())
        - allowed_sections
    )

    if unknown:
        fail(
            "unknown section(s): "
            + ", ".join(sorted(unknown))
        )

    ch = require_section(
        parser,
        "carryhandle",
        ("manifest_version",),
    )

    try:
        version = int(
            ch["manifest_version"]
        )
    except ValueError:
        fail(
            "manifest_version must be an integer"
        )

    if version not in SUPPORTED_MANIFEST_VERSIONS:
        fail(
            f"unsupported manifest_version {version}"
        )

    application_fields = (
        "name",
        "game_code",
        "company_code",
        "region",
    )

    if version >= 3:
        application_fields = (
            "name",
            "game_code",
            "disc_game_id",
            "company_code",
            "region",
        )

    app = require_section(
        parser,
        "application",
        application_fields,
    )

    name = require_ascii(
        app["name"],
        "application.name",
        maximum=64,
    )

    game_code = require_ascii(
        app["game_code"],
        "application.game_code",
        exact=4,
    )

    disc_game_id = None

    if version >= 3:
        disc_game_id = require_ascii(
            app["disc_game_id"],
            "application.disc_game_id",
            exact=2,
        )

    company_code = require_ascii(
        app["company_code"],
        "application.company_code",
        exact=2,
    )

    region = app["region"]

    if region not in REGIONS:
        fail(
            "application.region must be "
            "NTSC-J, NTSC-U or PAL"
        )

    persistence = require_section(
        parser,
        "persistence",
        ("store_id",),
    )

    store_id = require_ascii(
        persistence["store_id"],
        "persistence.store_id",
        maximum=63,
    )

    if not STORE_ID_RE.fullmatch(
        store_id
    ):
        fail(
            "persistence.store_id may contain only "
            "letters, digits, '.', '_' and '-'"
        )

    memcard = None

    if "memcard" in parser:
        fields = (
            "filename",
            "title",
            "comment",
            "banner",
            "icon",
        )

        if version >= 2:
            fields = (
                "filename",
                "sectors",
                "title",
                "comment",
                "banner",
                "icon",
            )

        card = require_section(
            parser,
            "memcard",
            fields,
        )

        #
        # Manifest v1 predates explicit CARD file geometry.
        # Its historical behaviour was one physical CARD sector.
        #
        sectors = 1

        if version >= 2:
            try:
                sectors = int(
                    card["sectors"],
                    10,
                )
            except ValueError:
                fail(
                    "memcard.sectors must be an integer"
                )

            if (
                sectors < 1
                or sectors > 0xffffffff
            ):
                fail(
                    "memcard.sectors must be between "
                    "1 and 4294967295"
                )

        memcard = {
            "filename": require_ascii(
                card["filename"],
                "memcard.filename",
                maximum=32,
            ),
            "sectors": sectors,
            "title": require_ascii(
                card["title"],
                "memcard.title",
                maximum=31,
            ),
            "comment": require_ascii(
                card["comment"],
                "memcard.comment",
                maximum=31,
            ),
            "banner": require_asset_path(
                card["banner"],
                "memcard.banner",
            ),
            "icon": require_asset_path(
                card["icon"],
                "memcard.icon",
            ),
        }

    return {
        "manifest_version": version,
        "name": name,
        "game_code": game_code,
        "disc_game_id": disc_game_id,
        "company_code": company_code,
        "region": region,
        "region_bi2": REGIONS[region],
        "store_id": store_id,
        "has_memcard": memcard is not None,
        "memcard": memcard,
    }




def c_string(value):
    return (
        '"'
        + value.replace("\\", "\\\\").replace('"', '\\"')
        + '"'
    )


def render_application_header(manifest):
    return f"""\
/*
 * GENERATED FILE.
 *
 * Generated by tools/ch_manifest.py.
 * Do not hand-edit.
 */

#ifndef CARRYHANDLE_GENERATED_CH_APPLICATION_CONFIG_H
#define CARRYHANDLE_GENERATED_CH_APPLICATION_CONFIG_H

#define CH_GENERATED_MANIFEST_VERSION \\
    {manifest["manifest_version"]}u

#define CH_GENERATED_APPLICATION_NAME \\
    {c_string(manifest["name"])}

#define CH_GENERATED_GAME_CODE \\
    {c_string(manifest["game_code"])}

#define CH_GENERATED_DISC_GAME_ID \\
    {c_string(manifest["disc_game_id"] or "")}

#define CH_GENERATED_COMPANY_CODE \\
    {c_string(manifest["company_code"])}

#define CH_GENERATED_APPLICATION_REGION \\
    {REGION_C_NAMES[manifest["region"]]}

#define CH_GENERATED_APPLICATION_REGION_BI2 \\
    {manifest["region_bi2"]}u

#define CH_GENERATED_PERSISTENCE_STORE_ID \\
    {c_string(manifest["store_id"])}

#endif
"""


def write_if_changed(path, text):
    path = Path(path)

    path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    data = text.encode("ascii")

    try:
        old = path.read_bytes()
    except FileNotFoundError:
        old = None

    if old == data:
        return False

    path.write_bytes(data)
    return True


def main():
    ap = argparse.ArgumentParser()

    ap.add_argument(
        "manifest",
        help="path to carryhandle.cfg",
    )

    ap.add_argument(
        "--output-header",
        help="write generated ch_application_config.h",
    )

    args = ap.parse_args()

    try:
        manifest = load_manifest(
            args.manifest
        )
    except ManifestError as exc:
        print(
            f"CarryHandle manifest error: {exc}",
            file=sys.stderr,
        )
        return 2

    if args.output_header:
        changed = write_if_changed(
            args.output_header,
            render_application_header(
                manifest
            ),
        )

        print(
            "Generated application header "
            + (
                "updated"
                if changed
                else "unchanged"
            )
        )

        print(
            f"  header       : "
            f"{args.output_header}"
        )

    print("CarryHandle manifest valid")
    print(
        f"  version      : "
        f"{manifest['manifest_version']}"
    )
    print(
        f"  application  : "
        f"{manifest['name']}"
    )
    print(
        f"  game code    : "
        f"{manifest['game_code']}"
    )
    print(
        f"  disc game id : "
        f"{manifest['disc_game_id'] or '(none)'}"
    )
    print(
        f"  company code : "
        f"{manifest['company_code']}"
    )
    print(
        f"  region       : "
        f"{manifest['region']} "
        f"(BI2={manifest['region_bi2']})"
    )
    print(
        f"  store id     : "
        f"{manifest['store_id']}"
    )
    print(
        f"  memcard      : "
        f"{'yes' if manifest['has_memcard'] else 'no'}"
    )

    if manifest["memcard"] is not None:
        card = manifest["memcard"]

        print(
            f"  card filename: "
            f"{card['filename']}"
        )

        print(
            f"  card title   : "
            f"{card['title']}"
        )

        print(
            f"  card comment : "
            f"{card['comment']}"
        )

        print(
            f"  card banner  : "
            f"{card['banner']}"
        )

        print(
            f"  card icon    : "
            f"{card['icon']}"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
