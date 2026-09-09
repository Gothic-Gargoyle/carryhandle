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
import os
import shlex
import shutil
import tempfile
import urllib.parse


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



# CARRYHANDLE_RELEASE_PROVIDERS_V1
#
# Publication deliberately remains separate from build/tag creation.
# build_plan() is always run first, so publication inherits the exact same
# source/tag/asset checks as the read-only `check` command.


def run_cli(
    command: list[str],
    *,
    check: bool = True,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    merged_env = None

    if env is not None:
        merged_env = dict(os.environ)
        merged_env.update(env)

    proc = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=merged_env,
    )

    if check and proc.returncode != 0:
        detail = (
            proc.stderr.strip()
            or proc.stdout.strip()
            or f"exit status {proc.returncode}"
        )

        raise ReleaseError(
            "command failed: "
            + shlex.join(command)
            + "\n"
            + detail
        )

    return proc


def parse_json_output(
    proc: subprocess.CompletedProcess[str],
    context: str,
) -> dict:
    try:
        value = json.loads(proc.stdout)
    except json.JSONDecodeError as exc:
        raise ReleaseError(
            f"{context} did not return valid JSON"
        ) from exc

    if not isinstance(value, dict):
        raise ReleaseError(
            f"{context} returned unexpected JSON"
        )

    return value


def plan_notes_path(
    args: argparse.Namespace,
) -> Path:
    repo = Path(args.repo).expanduser().resolve()
    return resolve_input_path(repo, args.notes)


def expected_asset_map(plan: dict) -> dict[str, dict]:
    return {
        asset["name"]: asset
        for asset in plan["assets"]
    }


class ReleaseProvider:
    cli_name = ""

    def __init__(
        self,
        plan: dict,
    ) -> None:
        self.plan = plan
        self.host = plan["remote_host"]
        self.repository = plan["repository"]
        self.tag = plan["tag"]
        self.title = plan["title"]

    def require_cli(self) -> None:
        if not shutil.which(self.cli_name):
            raise ReleaseError(
                f"required provider CLI is not installed: "
                f"{self.cli_name}"
            )

    def verify_auth(self) -> None:
        raise NotImplementedError

    def verify_project_access(self) -> None:
        raise NotImplementedError

    def release_exists(self) -> bool:
        raise NotImplementedError

    def create_release(
        self,
        notes: Path,
    ) -> None:
        raise NotImplementedError

    def verify_release(
        self,
        notes: Path,
    ) -> dict:
        raise NotImplementedError

    def download_asset(
        self,
        asset: dict,
        destination: Path,
    ) -> Path:
        raise NotImplementedError

    def publish(
        self,
        notes: Path,
    ) -> dict:
        self.require_cli()
        self.verify_auth()
        self.verify_project_access()

        if self.release_exists():
            raise ReleaseError(
                f"{self.plan['provider']} release already exists "
                f"for tag {self.tag}; refusing to update or overwrite it"
            )

        self.create_release(notes)

        metadata = self.verify_release(notes)

        verified_assets = []

        with tempfile.TemporaryDirectory(
            prefix="carryhandle-release-verify-"
        ) as temp_name:
            temp = Path(temp_name)

            for asset in self.plan["assets"]:
                asset_dir = temp / asset["name"]
                asset_dir.mkdir()

                downloaded = self.download_asset(
                    asset,
                    asset_dir,
                )

                if downloaded.name != asset["name"]:
                    raise ReleaseError(
                        "downloaded asset basename mismatch: "
                        f"expected {asset['name']}, "
                        f"got {downloaded.name}"
                    )

                actual_size = downloaded.stat().st_size

                if actual_size != asset["bytes"]:
                    raise ReleaseError(
                        f"published asset size mismatch for "
                        f"{asset['name']}: expected "
                        f"{asset['bytes']}, got {actual_size}"
                    )

                actual_sha = sha256_file(downloaded)

                if actual_sha != asset["sha256"]:
                    raise ReleaseError(
                        f"published asset SHA256 mismatch for "
                        f"{asset['name']}:\n"
                        f"  expected: {asset['sha256']}\n"
                        f"  actual:   {actual_sha}"
                    )

                verified_assets.append(
                    {
                        "name": asset["name"],
                        "bytes": actual_size,
                        "sha256": actual_sha,
                    }
                )

        return {
            "provider": self.plan["provider"],
            "repository": self.repository,
            "tag": self.tag,
            "title": self.title,
            "url": metadata.get("url", ""),
            "assets": verified_assets,
        }


class GitHubProvider(ReleaseProvider):
    cli_name = "gh"

    @property
    def repo_target(self) -> str:
        if self.host == "github.com":
            return self.repository

        return f"{self.host}/{self.repository}"

    def verify_auth(self) -> None:
        run_cli(
            [
                "gh",
                "auth",
                "status",
                "--hostname",
                self.host,
            ]
        )

    def verify_project_access(self) -> None:
        proc = run_cli(
            [
                "gh",
                "repo",
                "view",
                self.repo_target,
                "--json",
                "nameWithOwner",
            ]
        )

        data = parse_json_output(
            proc,
            "GitHub repository lookup",
        )

        actual = data.get("nameWithOwner")

        if (
            not isinstance(actual, str)
            or actual.casefold()
            != self.repository.casefold()
        ):
            raise ReleaseError(
                "GitHub repository lookup resolved an unexpected "
                f"repository: {actual!r}"
            )

    def release_exists(self) -> bool:
        encoded_tag = urllib.parse.quote(
            self.tag,
            safe="",
        )

        proc = run_cli(
            [
                "gh",
                "api",
                "--hostname",
                self.host,
                f"repos/{self.repository}/releases/tags/"
                f"{encoded_tag}",
            ],
            check=False,
        )

        if proc.returncode == 0:
            return True

        detail = (
            proc.stderr
            + "\n"
            + proc.stdout
        )

        if "HTTP 404" in detail:
            return False

        raise ReleaseError(
            "could not determine whether the GitHub release "
            f"already exists:\n{detail.strip()}"
        )

    def create_release(
        self,
        notes: Path,
    ) -> None:
        command = [
            "gh",
            "release",
            "create",
            self.tag,
        ]

        command.extend(
            asset["path"]
            for asset in self.plan["assets"]
        )

        command.extend(
            [
                "--repo",
                self.repo_target,
                "--title",
                self.title,
                "--notes-file",
                str(notes),
                "--verify-tag",
            ]
        )

        run_cli(command)

    def verify_release(
        self,
        notes: Path,
    ) -> dict:
        proc = run_cli(
            [
                "gh",
                "release",
                "view",
                self.tag,
                "--repo",
                self.repo_target,
                "--json",
                (
                    "tagName,name,isDraft,isPrerelease,"
                    "body,assets,url"
                ),
            ]
        )

        data = parse_json_output(
            proc,
            "GitHub release lookup",
        )

        if data.get("tagName") != self.tag:
            raise ReleaseError(
                "GitHub release tag does not match the requested tag"
            )

        if data.get("name") != self.title:
            raise ReleaseError(
                "GitHub release title does not match the requested title"
            )

        if data.get("isDraft") is not False:
            raise ReleaseError(
                "GitHub release unexpectedly remains a draft"
            )

        if data.get("isPrerelease") is not False:
            raise ReleaseError(
                "GitHub release unexpectedly became a prerelease"
            )

        expected_notes = notes.read_text(
            encoding="utf-8"
        ).rstrip("\r\n")
        actual_notes = str(
            data.get("body", "")
        ).rstrip("\r\n")

        if actual_notes != expected_notes:
            raise ReleaseError(
                "GitHub release notes do not match the requested notes file"
            )

        remote_assets = data.get("assets")

        if not isinstance(remote_assets, list):
            raise ReleaseError(
                "GitHub release returned invalid asset metadata"
            )

        actual = {}

        for item in remote_assets:
            if not isinstance(item, dict):
                raise ReleaseError(
                    "GitHub release returned invalid asset metadata"
                )

            name = item.get("name")
            size = item.get("size")

            if not isinstance(name, str):
                raise ReleaseError(
                    "GitHub release asset has no valid name"
                )

            actual[name] = size

        expected = expected_asset_map(self.plan)

        if set(actual) != set(expected):
            raise ReleaseError(
                "GitHub release asset names differ from the "
                "explicitly supplied asset set"
            )

        for name, asset in expected.items():
            if actual[name] != asset["bytes"]:
                raise ReleaseError(
                    f"GitHub release asset size mismatch for {name}"
                )

        return data

    def download_asset(
        self,
        asset: dict,
        destination: Path,
    ) -> Path:
        run_cli(
            [
                "gh",
                "release",
                "download",
                self.tag,
                "--repo",
                self.repo_target,
                "--dir",
                str(destination),
                "--pattern",
                asset["name"],
            ]
        )

        files = [
            candidate
            for candidate in destination.iterdir()
            if candidate.is_file()
        ]

        if (
            len(files) != 1
            or files[0].name != asset["name"]
        ):
            raise ReleaseError(
                "GitHub release download did not produce exactly "
                f"the requested asset {asset['name']}"
            )

        return files[0]


class GitLabProvider(ReleaseProvider):
    cli_name = "glab"

    @property
    def environment(self) -> dict[str, str]:
        return {
            "GITLAB_HOST": self.host,
        }

    @property
    def encoded_project(self) -> str:
        return urllib.parse.quote(
            self.repository,
            safe="",
        )

    @property
    def encoded_tag(self) -> str:
        return urllib.parse.quote(
            self.tag,
            safe="",
        )

    @property
    def release_endpoint(self) -> str:
        return (
            f"projects/{self.encoded_project}/releases/"
            f"{self.encoded_tag}"
        )

    def verify_auth(self) -> None:
        run_cli(
            [
                "glab",
                "auth",
                "status",
                "--hostname",
                self.host,
            ],
            env=self.environment,
        )

    def verify_project_access(self) -> None:
        proc = run_cli(
            [
                "glab",
                "api",
                f"projects/{self.encoded_project}",
            ],
            env=self.environment,
        )

        data = parse_json_output(
            proc,
            "GitLab repository lookup",
        )

        actual = data.get("path_with_namespace")

        if actual != self.repository:
            raise ReleaseError(
                "GitLab repository lookup resolved an unexpected "
                f"repository: {actual!r}"
            )

    def release_exists(self) -> bool:
        proc = run_cli(
            [
                "glab",
                "api",
                self.release_endpoint,
            ],
            check=False,
            env=self.environment,
        )

        if proc.returncode == 0:
            return True

        detail = (
            proc.stderr
            + "\n"
            + proc.stdout
        )

        if "404" in detail:
            return False

        raise ReleaseError(
            "could not determine whether the GitLab release "
            f"already exists:\n{detail.strip()}"
        )

    def create_release(
        self,
        notes: Path,
    ) -> None:
        payload = {
            "name": self.title,
            "tag_name": self.tag,
            "description": notes.read_text(
                encoding="utf-8"
            ),
        }

        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            prefix="carryhandle-gitlab-release-",
            suffix=".json",
            delete=False,
        ) as handle:
            json.dump(payload, handle)
            payload_path = Path(handle.name)

        try:
            run_cli(
                [
                    "glab",
                    "api",
                    "--method",
                    "POST",
                    f"projects/{self.encoded_project}/releases",
                    "--input",
                    str(payload_path),
                ],
                env=self.environment,
            )
        finally:
            payload_path.unlink(
                missing_ok=True
            )

        try:
            command = [
                "glab",
                "release",
                "upload",
                self.tag,
            ]

            command.extend(
                asset["path"]
                for asset in self.plan["assets"]
            )

            command.extend(
                [
                    "--repo",
                    self.repository,
                ]
            )

            run_cli(
                command,
                env=self.environment,
            )
        except ReleaseError as exc:
            raise ReleaseError(
                "GitLab release was created, but asset upload failed. "
                "Automatic rollback is intentionally disabled; inspect "
                "the release manually before taking further action.\n"
                f"{exc}"
            ) from exc

    def verify_release(
        self,
        notes: Path,
    ) -> dict:
        proc = run_cli(
            [
                "glab",
                "api",
                self.release_endpoint,
            ],
            env=self.environment,
        )

        data = parse_json_output(
            proc,
            "GitLab release lookup",
        )

        if data.get("tag_name") != self.tag:
            raise ReleaseError(
                "GitLab release tag does not match the requested tag"
            )

        if data.get("name") != self.title:
            raise ReleaseError(
                "GitLab release title does not match the requested title"
            )

        expected_notes = notes.read_text(
            encoding="utf-8"
        ).rstrip("\r\n")
        actual_notes = str(
            data.get("description", "")
        ).rstrip("\r\n")

        if actual_notes != expected_notes:
            raise ReleaseError(
                "GitLab release notes do not match the requested notes file"
            )

        assets = data.get("assets")

        if not isinstance(assets, dict):
            raise ReleaseError(
                "GitLab release returned invalid asset metadata"
            )

        links = assets.get("links")

        if not isinstance(links, list):
            raise ReleaseError(
                "GitLab release returned invalid asset links"
            )

        actual_names = {
            item.get("name")
            for item in links
            if isinstance(item, dict)
            and isinstance(item.get("name"), str)
        }

        expected_names = set(
            expected_asset_map(self.plan)
        )

        if actual_names != expected_names:
            raise ReleaseError(
                "GitLab release asset names differ from the "
                "explicitly supplied asset set"
            )

        links_data = data.get("_links")
        url = ""

        if isinstance(links_data, dict):
            value = links_data.get("self")

            if isinstance(value, str):
                url = value

        data["url"] = url
        return data

    def download_asset(
        self,
        asset: dict,
        destination: Path,
    ) -> Path:
        run_cli(
            [
                "glab",
                "release",
                "download",
                self.tag,
                "--repo",
                self.repository,
                "--dir",
                str(destination),
                "--asset-name",
                asset["name"],
            ],
            env=self.environment,
        )

        files = [
            candidate
            for candidate in destination.iterdir()
            if candidate.is_file()
        ]

        if (
            len(files) != 1
            or files[0].name != asset["name"]
        ):
            raise ReleaseError(
                "GitLab release download did not produce exactly "
                f"the requested asset {asset['name']}"
            )

        return files[0]


def provider_for_plan(
    plan: dict,
) -> ReleaseProvider:
    provider = plan["provider"]

    if provider == "github":
        return GitHubProvider(plan)

    if provider == "gitlab":
        return GitLabProvider(plan)

    raise ReleaseError(
        f"unsupported release provider: {provider!r}"
    )


def publish_release(
    plan: dict,
    args: argparse.Namespace,
) -> dict:
    notes = plan_notes_path(args)
    provider = provider_for_plan(plan)
    return provider.publish(notes)


def print_publish_human(
    plan: dict,
    publication: dict,
) -> None:
    print("CarryHandle release publication: PASS")
    print()
    print(f"Application : {plan['application']}")
    print(f"Tag         : {plan['tag']} ({plan['tag_type']})")
    print(f"Commit      : {plan['commit']}")
    print(f"Provider    : {publication['provider']}")
    print(f"Repository  : {publication['repository']}")
    print(f"Title       : {publication['title']}")

    if publication["url"]:
        print(f"Release URL : {publication['url']}")

    print()
    print("Fresh-download verification:")

    for asset in publication["assets"]:
        print(f"  {asset['name']}")
        print(f"    bytes  : {asset['bytes']}")
        print(f"    SHA256 : {asset['sha256']}")

    print()
    print("Publication: PERFORMED AND VERIFIED")

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
    requested_command = (
        argv[0]
        if argv
        else None
    )
    parse_argv = list(argv)

    if requested_command == "publish":
        parse_argv[0] = "check"
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

    sub.add_parser(
        "publish",
        help=(
            "publish an already-tagged release and verify "
            "freshly downloaded assets"
        ),
    )

    args = parser.parse_args(parse_argv)

    if requested_command == "publish":
        args.command = "publish"

    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)

    try:
        plan = build_plan(args)

        if args.command == "check":
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

        if args.command == "publish":
            publication = publish_release(
                plan,
                args,
            )

            if args.json:
                print(
                    json.dumps(
                        {
                            "plan": plan,
                            "publication": publication,
                        },
                        indent=2,
                        sort_keys=True,
                    )
                )
            else:
                print_publish_human(
                    plan,
                    publication,
                )

            return 0

        raise ReleaseError(
            f"unsupported command: {args.command!r}"
        )

    except ReleaseError as exc:
        label = (
            "CarryHandle release publication"
            if args.command == "publish"
            else "CarryHandle release preflight"
        )

        print(
            f"{label}: FAIL: {exc}",
            file=sys.stderr,
        )
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
