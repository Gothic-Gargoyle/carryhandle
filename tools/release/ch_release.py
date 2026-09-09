#!/usr/bin/env python3
"""
CarryHandle release publication preflight.

This tool owns generic release safety checks. It intentionally does not build
the consumer, create tags, or publish a release.

Release-specific values are invocation-time inputs:
  - tag
  - asset(s)
  - release notes
  - optional title
  - optional provider override

carryhandle.cfg remains application/build metadata. The application name is
read from it only to derive a default human-facing release title.

Provider v1:
  - auto-detect github.com and gitlab.com remotes
  - explicit --provider github|gitlab for other/self-hosted forge remotes

No provider-specific mutation occurs in this version.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess
import sys
from urllib.parse import urlparse


class ReleaseError(RuntimeError):
    pass


def run_git(repo: Path, *args: str, check: bool = True) -> str:
    proc = subprocess.run(
        ["git", "-C", str(repo), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    if check and proc.returncode != 0:
        detail = proc.stderr.strip() or proc.stdout.strip()
        raise ReleaseError(
            f"git {' '.join(args)} failed"
            + (f": {detail}" if detail else "")
        )

    return proc.stdout.strip()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()

    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)

    return digest.hexdigest()


def resolve_input_path(repo: Path, value: str) -> Path:
    path = Path(value).expanduser()

    if not path.is_absolute():
        path = repo / path

    return path.resolve()


def parse_remote_url(url: str) -> tuple[str, str]:
    """
    Return (host, repository path without leading slash/.git).

    Supports normal URL forms and SCP-like Git SSH syntax:
      https://github.com/owner/repo.git
      ssh://git@gitlab.com/group/repo.git
      git@github.com:owner/repo.git
    """

    url = url.strip()

    if not url:
        raise ReleaseError("remote URL is empty")

    if "://" in url:
        parsed = urlparse(url)
        host = (parsed.hostname or "").lower()
        path = parsed.path.lstrip("/")
    else:
        match = re.fullmatch(
            r"(?:[^@/\s]+@)?([^:/\s]+):(.+)",
            url,
        )

        if not match:
            raise ReleaseError(
                f"unsupported Git remote URL syntax: {url}"
            )

        host = match.group(1).lower()
        path = match.group(2).lstrip("/")

    if not host or not path:
        raise ReleaseError(
            f"could not resolve host/repository from remote URL: {url}"
        )

    if path.endswith(".git"):
        path = path[:-4]

    path = path.rstrip("/")

    if not path:
        raise ReleaseError(
            f"remote URL has no repository path: {url}"
        )

    return host, path


def detect_provider(host: str, override: str) -> str:
    if override != "auto":
        return override

    if host == "github.com":
        return "github"

    if host == "gitlab.com":
        return "gitlab"

    raise ReleaseError(
        "cannot auto-detect release provider for "
        f"{host!r}; use --provider github or --provider gitlab"
    )


def load_application_name(
    manifest_path: Path,
    manifest_tool: Path,
) -> str:
    if not manifest_path.is_file():
        raise ReleaseError(
            f"CarryHandle manifest not found: {manifest_path}"
        )

    if not manifest_tool.is_file():
        raise ReleaseError(
            f"CarryHandle manifest tool not found: {manifest_tool}"
        )

    try:
        namespace = runpy.run_path(str(manifest_tool))
        manifest = namespace["load_manifest"](manifest_path)
    except Exception as exc:
        raise ReleaseError(
            f"could not load CarryHandle manifest: {exc}"
        ) from exc

    name = manifest.get("name")

    if not isinstance(name, str) or not name.strip():
        raise ReleaseError(
            "CarryHandle manifest has no usable application name"
        )

    return name.strip()


def unstaged_tracked_paths(repo: Path) -> list[str]:
    output = run_git(repo, "diff", "--name-only")

    if not output:
        return []

    return sorted(
        line for line in output.splitlines()
        if line
    )


def staged_paths(repo: Path) -> list[str]:
    output = run_git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    )

    if not output:
        return []

    return sorted(
        line for line in output.splitlines()
        if line
    )


def validate_source_state(
    repo: Path,
    allow_dirty: list[str],
) -> tuple[list[str], list[str]]:
    staged = staged_paths(repo)

    if staged:
        raise ReleaseError(
            "staged source changes are not allowed: "
            + ", ".join(staged)
        )

    dirty = unstaged_tracked_paths(repo)
    allowed = set(allow_dirty)
    unexpected = [
        path for path in dirty
        if path not in allowed
    ]

    if unexpected:
        raise ReleaseError(
            "unexpected tracked source dirt: "
            + ", ".join(unexpected)
        )

    return dirty, sorted(allowed)


def local_tag_target(repo: Path, tag: str) -> str:
    if not tag or tag.startswith("-"):
        raise ReleaseError(
            f"invalid release tag: {tag!r}"
        )

    ref_check = subprocess.run(
        [
            "git",
            "-C",
            str(repo),
            "check-ref-format",
            f"refs/tags/{tag}",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    if ref_check.returncode != 0:
        raise ReleaseError(
            f"invalid release tag: {tag!r}"
        )

    # `git show-ref --quiet` produces no stdout; use return-code directly.
    proc = subprocess.run(
        [
            "git",
            "-C",
            str(repo),
            "show-ref",
            "--verify",
            "--quiet",
            f"refs/tags/{tag}",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    if proc.returncode != 0:
        raise ReleaseError(
            f"local tag does not exist: {tag}"
        )

    target = run_git(
        repo,
        "rev-list",
        "-n1",
        tag,
        "--",
    )

    if not re.fullmatch(r"[0-9a-fA-F]{40,64}", target):
        raise ReleaseError(
            f"could not resolve local tag target for {tag}"
        )

    return target.lower()


def remote_tag_target(
    repo: Path,
    remote: str,
    tag: str,
) -> tuple[str, str]:
    output = run_git(
        repo,
        "ls-remote",
        "--tags",
        remote,
        f"refs/tags/{tag}",
        f"refs/tags/{tag}^{{}}",
    )

    direct = None
    peeled = None

    for line in output.splitlines():
        parts = line.split()

        if len(parts) != 2:
            continue

        sha, ref = parts

        if ref == f"refs/tags/{tag}":
            direct = sha.lower()
        elif ref == f"refs/tags/{tag}^{{}}":
            peeled = sha.lower()

    if direct is None:
        raise ReleaseError(
            f"remote tag does not exist on {remote}: {tag}"
        )

    if peeled is not None:
        return peeled, "annotated"

    return direct, "lightweight"


def build_plan(args: argparse.Namespace) -> dict:
    repo = Path(args.repo).expanduser().resolve()

    if not (repo / ".git").exists():
        raise ReleaseError(
            f"not a Git working tree: {repo}"
        )

    manifest_tool = Path(
        args.manifest_tool
        or (
            Path(__file__).resolve().parent.parent
            / "ch_manifest.py"
        )
    ).expanduser().resolve()

    manifest = resolve_input_path(
        repo,
        args.manifest,
    )
    notes = resolve_input_path(
        repo,
        args.notes,
    )
    assets = [
        resolve_input_path(repo, value)
        for value in args.asset
    ]

    if not notes.is_file():
        raise ReleaseError(
            f"release notes file not found: {notes}"
        )

    if notes.stat().st_size == 0:
        raise ReleaseError(
            f"release notes file is empty: {notes}"
        )

    if not assets:
        raise ReleaseError(
            "at least one --asset is required"
        )

    basenames = [path.name for path in assets]

    if len(basenames) != len(set(basenames)):
        raise ReleaseError(
            "release assets must have unique basenames"
        )

    missing_assets = [
        str(path)
        for path in assets
        if not path.is_file()
    ]

    if missing_assets:
        raise ReleaseError(
            "release asset(s) missing: "
            + ", ".join(missing_assets)
        )

    dirty, allowed = validate_source_state(
        repo,
        args.allow_dirty,
    )

    head = run_git(repo, "rev-parse", "HEAD").lower()
    local_target = local_tag_target(
        repo,
        args.tag,
    )

    if local_target != head:
        raise ReleaseError(
            f"tag {args.tag} targets {local_target}, "
            f"but HEAD is {head}"
        )

    remote_target, tag_type = remote_tag_target(
        repo,
        args.remote,
        args.tag,
    )

    if remote_target != head:
        raise ReleaseError(
            f"remote tag {args.tag} targets {remote_target}, "
            f"but HEAD is {head}"
        )

    remote_url = run_git(
        repo,
        "remote",
        "get-url",
        args.remote,
    )
    host, repository = parse_remote_url(remote_url)
    provider = detect_provider(
        host,
        args.provider,
    )

    app_name = load_application_name(
        manifest,
        manifest_tool,
    )
    title = args.title or f"{app_name} {args.tag}"

    asset_data = [
        {
            "path": str(path),
            "name": path.name,
            "bytes": path.stat().st_size,
            "sha256": sha256_file(path),
        }
        for path in assets
    ]

    return {
        "status": "PASS",
        "application": app_name,
        "tag": args.tag,
        "tag_type": tag_type,
        "commit": head,
        "remote": args.remote,
        "remote_url": remote_url,
        "remote_host": host,
        "repository": repository,
        "provider": provider,
        "title": title,
        "manifest": str(manifest),
        "notes": str(notes),
        "notes_bytes": notes.stat().st_size,
        "assets": asset_data,
        "tracked_dirty": dirty,
        "allowed_dirty": allowed,
        "publication": "not performed",
    }


def print_human(plan: dict) -> None:
    print("CarryHandle release publication preflight: PASS")
    print()
    print(f"Application : {plan['application']}")
    print(f"Tag         : {plan['tag']} ({plan['tag_type']})")
    print(f"Commit      : {plan['commit']}")
    print(f"Provider    : {plan['provider']}")
    print(f"Repository  : {plan['repository']}")
    print(f"Remote      : {plan['remote']} -> {plan['remote_url']}")
    print(f"Title       : {plan['title']}")
    print(f"Notes       : {plan['notes']} ({plan['notes_bytes']} bytes)")

    if plan["tracked_dirty"]:
        print(
            "Allowed dirt: "
            + ", ".join(plan["tracked_dirty"])
        )
    else:
        print("Allowed dirt: <none>")

    print()
    print("Assets:")

    for asset in plan["assets"]:
        print(f"  {asset['name']}")
        print(f"    path   : {asset['path']}")
        print(f"    bytes  : {asset['bytes']}")
        print(f"    SHA256 : {asset['sha256']}")

    print()
    print("Publication: NOT PERFORMED")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Validate an already-tagged CarryHandle consumer "
            "release before publication."
        )
    )

    sub = parser.add_subparsers(
        dest="command",
        required=True,
    )

    check = sub.add_parser(
        "check",
        help="validate release inputs without publishing",
    )
    check.add_argument(
        "--repo",
        default=".",
        help="consumer Git working tree (default: current directory)",
    )
    check.add_argument(
        "--manifest",
        default="carryhandle.cfg",
        help="CarryHandle manifest path, relative to repo by default",
    )
    check.add_argument(
        "--manifest-tool",
        help=(
            "override ch_manifest.py path; default is the "
            "CarryHandle tool beside this release tool"
        ),
    )
    check.add_argument(
        "--tag",
        required=True,
        help="already-existing release tag",
    )
    check.add_argument(
        "--asset",
        action="append",
        default=[],
        help="release artifact; repeat for multiple assets",
    )
    check.add_argument(
        "--notes",
        required=True,
        help="Markdown/text release notes file",
    )
    check.add_argument(
        "--title",
        help=(
            "release title; default: '<application name> <tag>'"
        ),
    )
    check.add_argument(
        "--provider",
        choices=("auto", "github", "gitlab"),
        default="auto",
        help="forge provider (default: detect github.com/gitlab.com)",
    )
    check.add_argument(
        "--remote",
        default="origin",
        help="Git remote containing the published tag (default: origin)",
    )
    check.add_argument(
        "--allow-dirty",
        action="append",
        default=[],
        help=(
            "explicitly allow one unstaged tracked path; "
            "repeat as needed"
        ),
    )
    check.add_argument(
        "--json",
        action="store_true",
        help="emit the validated release plan as JSON",
    )

    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)

    try:
        if args.command == "check":
            plan = build_plan(args)
        else:
            raise ReleaseError(
                f"unsupported command: {args.command}"
            )
    except ReleaseError as exc:
        print(
            f"CarryHandle release preflight: FAIL: {exc}",
            file=sys.stderr,
        )
        return 1

    if args.json:
        print(
            json.dumps(
                plan,
                indent=2,
                sort_keys=True,
            )
        )
    else:
        print_human(plan)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
