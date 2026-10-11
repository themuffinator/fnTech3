"""Reviewed version preparation and draft-only fnTech3 release automation."""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from fntech3_meta import ROOT, base_metadata, compose_windows_version
from generate_docs import render

VERSION_PATH = "version/fntech3_version.h"
VERSION_RE = re.compile(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)")
SECTION_RE = re.compile(r"## \[([^\]]+)\](?: - (\d{4}-\d{2}-\d{2}))?")
ARTIFACTS = ("linux-x86.tar.gz", "windows-mingw-x86.zip", "windows-msvc-x86.zip")


def version_tuple(value: str) -> tuple[int, int, int]:
    match = VERSION_RE.fullmatch(value)
    if not match:
        raise ValueError("Version must be MAJOR.MINOR.PATCH without a v prefix or leading zeroes")
    parts = tuple(int(part) for part in match.groups())
    compose_windows_version(*parts)
    return parts


def tag_version(tag: str) -> str:
    if not tag.startswith("v"):
        raise ValueError("Release tag must be vMAJOR.MINOR.PATCH")
    version_tuple(tag[1:])
    return tag[1:]


def meaningful(body: str) -> bool:
    return any(
        line.startswith(("- ", "* ")) and line.strip().lower() not in {
            "- none yet.", "- _none yet._", "- no documented changes.",
            "* none yet.", "* _none yet._", "* no documented changes.",
        }
        for line in body.splitlines()
    )


def sections(text: str) -> list[tuple[str, str | None, str]]:
    entries: list[tuple[str, str | None, str]] = []
    label = date = None
    body: list[str] = []
    for line in text.splitlines():
        if line.startswith("## "):
            match = SECTION_RE.fullmatch(line)
            if not match:
                raise ValueError(f"Malformed changelog heading: {line}")
            if label is not None:
                entries.append((label, date, "\n".join(body).strip()))
            label, date = match.groups()
            body = []
        else:
            body.append(line)
    if label is not None:
        entries.append((label, date, "\n".join(body).strip()))
    if not entries or entries[0][:2] != ("Unreleased", None):
        raise ValueError("Changelog must start with an undated ## [Unreleased] section")
    seen = {"Unreleased"}
    previous_version = None
    previous_date = None
    for label, date, body in entries[1:]:
        if label in seen:
            raise ValueError(f"Duplicate changelog version: {label}")
        seen.add(label)
        current = version_tuple(label)
        if not date:
            raise ValueError(f"Changelog version {label} needs a release date")
        day = dt.date.fromisoformat(date)
        if day > dt.datetime.now(dt.timezone.utc).date():
            raise ValueError(f"Changelog version {label} has a future release date")
        if previous_version is not None and current >= previous_version:
            raise ValueError("Changelog versions must be in descending order")
        if previous_date is not None and day > previous_date:
            raise ValueError("Changelog dates must be in descending order")
        if not meaningful(body):
            raise ValueError(f"Changelog version {label} has no release notes")
        previous_version, previous_date = current, day
    return entries


def check_content(header: str, changelog: str, version: str | None = None) -> dict:
    meta = base_metadata(text=header)
    current = version_tuple(str(meta["version"]))
    if meta["version_tweak"] or meta["tag_prefix"] != "v":
        raise ValueError("Release headers require TWEAK=0 and TAG_PREFIX=\"v\"")
    entries = sections(changelog)
    if len(entries) > 1 and current < version_tuple(entries[1][0]):
        raise ValueError("Version header is older than the newest changelog entry")
    if version is not None:
        version_tuple(version)
        if meta["version"] != version:
            raise ValueError("Release tag does not match the canonical version header")
        if len(entries) < 2 or entries[1][0] != version:
            raise ValueError(f"Prepare the dated [{version}] changelog entry before tagging")
        if meaningful(entries[0][2]):
            raise ValueError("Tagged release still has pending Unreleased changes")
    return meta


def git(*args: str, root: Path = ROOT) -> str:
    result = subprocess.run(["git", *args], cwd=root, text=True, encoding="utf-8",
                            capture_output=True, check=False)
    if result.returncode:
        raise ValueError(result.stderr.strip() or "Git command failed")
    return result.stdout.strip()


def metadata(tag: str | None = None, *, root: Path = ROOT) -> dict:
    if tag:
        version = tag_version(tag)  # Validate before passing a ref to Git.
        sha = git("rev-parse", "--verify", f"refs/tags/{tag}^{{commit}}", root=root)
        header = git("show", f"{sha}:{VERSION_PATH}", root=root)
        changelog = git("show", f"{sha}:CHANGELOG.md", root=root)
    else:
        version = None
        sha = git("rev-parse", "HEAD", root=root)
        header = (root / VERSION_PATH).read_text(encoding="utf-8")
        changelog = (root / "CHANGELOG.md").read_text(encoding="utf-8")
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError("Release source must resolve to a full commit SHA")
    meta = check_content(header, changelog, version)
    return {
        "version": meta["version"],
        "release_tag": tag or f"v{meta['version']}",
        "head_commit": sha,
        "build_date": sections(changelog)[1][1] if tag else dt.datetime.now(dt.timezone.utc).date().isoformat(),
    }


def check(root: Path = ROOT) -> None:
    meta = check_content((root / VERSION_PATH).read_text(encoding="utf-8"),
                         (root / "CHANGELOG.md").read_text(encoding="utf-8"))
    context = {**meta, "release_tag_example": f"v{meta['version']}"}
    for template, output in (("docs/templates/README.md.in", "README.md"),
                             ("docs/templates/install-readme.html.in", ".install/README.html")):
        if render(root / template, context) != (root / output).read_text(encoding="utf-8"):
            raise ValueError(f"{output} is stale; run python scripts/generate_docs.py")


def prepare(version: str, date: str, *, root: Path = ROOT) -> None:
    requested = version_tuple(version)
    header_path, changelog_path = root / VERSION_PATH, root / "CHANGELOG.md"
    header = header_path.read_text(encoding="utf-8")
    changelog = changelog_path.read_text(encoding="utf-8")
    meta = check_content(header, changelog)
    if requested < version_tuple(str(meta["version"])):
        raise ValueError("Cannot decrease the current version")
    entries = sections(changelog)
    if len(entries) > 1 and requested <= version_tuple(entries[1][0]):
        raise ValueError("Release already prepared; choose a newer version")
    if not meaningful(entries[0][2]):
        raise ValueError("Unreleased needs reviewed changelog bullets before preparation")
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", date):
        raise ValueError("Release date must be YYYY-MM-DD")
    new_changelog = changelog.replace("## [Unreleased]", f"## [Unreleased]\n\n_None yet._\n\n## [{version}] - {date}", 1)
    major, minor, patch = requested
    windows = compose_windows_version(major, minor, patch)
    replacements = {
        "FNTECH3_VERSION_MAJOR": str(major), "FNTECH3_VERSION_MINOR": str(minor),
        "FNTECH3_VERSION_PATCH": str(patch), "FNTECH3_VERSION_TWEAK": "0",
        "FNTECH3_VERSION_STRING": f'"{version}"',
        "FNTECH3_WINDOWS_FILE_VERSION": windows, "FNTECH3_WINDOWS_PRODUCT_VERSION": windows,
    }
    for key, value in replacements.items():
        header, count = re.subn(rf"(?m)^#define {key} .+$", f"#define {key} {value}", header)
        if count != 1:
            raise ValueError(f"Expected exactly one {key} definition")
    new_meta = check_content(header, new_changelog, version)
    context = {**new_meta, "release_tag_example": f"v{version}"}
    updates = {
        header_path: header, changelog_path: new_changelog,
        root / "README.md": render(root / "docs/templates/README.md.in", context),
        root / ".install/README.html": render(root / "docs/templates/install-readme.html.in", context),
    }
    # Validate every representation and template before changing the first file.
    for path, content in updates.items():
        path.write_text(content, encoding="utf-8", newline="\n")


def notes(info: dict, *, root: Path = ROOT) -> str:
    changelog = git("show", f"{info['head_commit']}:CHANGELOG.md", root=root)
    body = sections(changelog)[1][2]
    source_url = f"https://github.com/themuffinator/fnTech3/blob/{info['head_commit']}"
    # Keep large first-release histories in the packaged changelog. GitHub's
    # release description is bounded; retain complete sections, not cut bullets.
    if len(body.encode("utf-8")) > 60_000:
        selected = []
        for section in re.split(r"(?m)(?=^### )", body):
            if section.startswith(("### Highlights\n", "### Known limitations\n")):
                selected.append(section.strip())
        body = "\n\n".join(selected)
        if len(body.encode("utf-8")) > 60_000:
            raise ValueError("Release highlights and limitations need shorter reviewed notes")
        body += f"\n\n[Complete versioned changelog]({source_url}/CHANGELOG.md)."
    # Release pages do not resolve repository-relative documentation links.
    body = re.sub(r"\]\(((?:docs/)[^\s)]+|BUILD\.md(?:#[^\s)]+)?)\)",
                  lambda match: f"]({source_url}/{match[1]})", body)
    return (f"# fnTech3 {info['version']}\n\n{body}\n\n"
            "## Release scope\n\n"
            "Development prerelease: Q3 and QL profiles are enabled. All other game "
            "profiles remain experimental and disabled in these default builds. This release does not "
            "establish full unified compatibility or completion of runtime gates. "
            "See the included `docs/fntech3/COMPATIBILITY.md` for evidence and open gates.\n\n"
            "Engine only; supply your own legitimate game installation.\n\n"
            f"## Build details\n\n- Tag: `{info['release_tag']}`\n"
            f"- Source commit: `{info['head_commit']}`\n- Release date: {info['build_date']}\n"
            "- Platforms: Windows x86 (MSVC and MinGW), Linux x86.\n"
            "- Verify downloads against `SHA256SUMS.txt`; retain `release-manifest.json` "
            "for build identity and compatibility scope.\n")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def release_files(manifest_path: Path, info: dict) -> list[Path]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    expected = {"project": "fnTech3", "channel": "candidate", "prerelease": True,
                "release_tag": info["release_tag"], "version": info["version"],
                "source_commit": info["head_commit"], "build_date": info["build_date"]}
    for key, value in expected.items():
        if manifest.get(key) != value:
            raise ValueError(f"Manifest {key} does not match the reviewed release")
    expected_names = {f"fntech3-{info['version']}-{suffix}" for suffix in ARTIFACTS}
    archives = manifest["archives"]
    if len(archives) != len(expected_names) or {item["archive"] for item in archives} != expected_names:
        raise ValueError("Manifest must contain exactly the three supported archives")
    files = [manifest_path, manifest_path.parent / "SHA256SUMS.txt"]
    for item in archives:
        path = manifest_path.parent / "packages" / item["archive"]
        if path.is_symlink() or not path.is_file() or sha256(path) != item["sha256"]:
            raise ValueError(f"Archive is missing or checksum differs: {path.name}")
        files.append(path)
    expected_sums = {f"{sha256(path)}  {path.name}" for path in files if path.name != "SHA256SUMS.txt"}
    sums = files[1].read_text(encoding="utf-8").splitlines()
    if len(sums) != len(expected_sums) or set(sums) != expected_sums:
        raise ValueError("SHA256SUMS.txt does not match the manifest and archives")
    return files


def gh(*args: str) -> str:
    result = subprocess.run(["gh", *args], cwd=ROOT, text=True, encoding="utf-8",
                            capture_output=True, check=False)
    if result.returncode:
        raise ValueError(result.stderr.strip() or "GitHub CLI command failed")
    return result.stdout


def remote_tag_commit(tag: str) -> str:
    # Use authenticated API reads: checkout intentionally does not persist Git credentials.
    obj = json.loads(gh("api", f"repos/{{owner}}/{{repo}}/git/ref/tags/{tag}"))["object"]
    for _ in range(16):
        sha = obj.get("sha", "")
        if not re.fullmatch(r"[0-9a-f]{40}", sha):
            raise ValueError("Remote tag has an invalid object ID")
        if obj.get("type") == "commit":
            return sha
        if obj.get("type") != "tag":
            break
        obj = json.loads(gh("api", f"repos/{{owner}}/{{repo}}/git/tags/{sha}"))["object"]
    raise ValueError("Remote tag does not resolve to a commit")


def find_release(tag: str) -> dict | None:
    # The by-tag REST endpoint only finds published releases. Authenticated
    # listing includes drafts; paginate so an older draft is still repairable.
    pages = json.loads(gh("api", "--paginate", "--slurp", "repos/{owner}/{repo}/releases?per_page=100"))
    matches = [item for page in pages for item in page if item.get("tag_name") == tag]
    if len(matches) > 1:
        raise ValueError("Multiple releases use this tag; inspect them before retrying")
    return matches[0] if matches else None


def validate_draft(existing: dict, info: dict, files: list[Path]) -> None:
    if not existing.get("draft"):
        raise ValueError("Release is already published; published assets are immutable to this workflow")
    if existing.get("target_commitish") != info["head_commit"]:
        raise ValueError("Existing draft targets another commit; inspect it before retrying")
    if existing.get("tag_name") != info["release_tag"] or not existing.get("prerelease"):
        raise ValueError("Existing draft tag or prerelease state differs")
    unexpected = {item["name"] for item in existing.get("assets", [])} - {path.name for path in files}
    if unexpected:
        raise ValueError(f"Existing draft has unexpected assets: {sorted(unexpected)}")


def create_draft(tag: str, manifest: Path, notes_path: Path) -> str:
    info = metadata(tag)
    if git("rev-parse", "HEAD") != info["head_commit"]:
        raise ValueError("Checkout must match the tagged source commit")
    if remote_tag_commit(tag) != info["head_commit"]:
        raise ValueError("Remote release tag moved or is missing; refusing to upload")
    files = release_files(manifest, info)
    if notes_path.read_text(encoding="utf-8") != notes(info):
        raise ValueError("Release notes do not match the tagged changelog")
    found = find_release(tag)
    if found is None:
        gh("release", "create", tag, "--verify-tag", "--draft", "--prerelease",
           "--target", info["head_commit"], "--title", f"fnTech3 {info['version']}",
           "--notes-file", str(notes_path))
    else:
        validate_draft(found, info, files)
        gh("release", "edit", tag, "--title", f"fnTech3 {info['version']}", "--notes-file", str(notes_path))
    # Only drafts can reach this point; a failed upload can be retried in place.
    gh("release", "upload", tag, *(str(path) for path in files), "--clobber")
    result = find_release(tag)
    if result is None:
        raise ValueError("Uploaded draft was not found; inspect the workflow before retrying")
    validate_draft(result, info, files)
    actual = {item["name"]: item for item in result["assets"]}
    if set(actual) != {path.name for path in files}:
        raise ValueError("Draft asset set is incomplete; rerun the workflow")
    for path in files:
        asset = actual[path.name]
        if asset.get("digest") != f"sha256:{sha256(path)}" or asset.get("size") != path.stat().st_size:
            raise ValueError(f"Uploaded asset verification failed: {path.name}")
    return result["html_url"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("check")
    prep = commands.add_parser("prepare")
    prep.add_argument("--version", required=True)
    prep.add_argument("--date", default=dt.datetime.now(dt.timezone.utc).date().isoformat())
    meta = commands.add_parser("metadata")
    meta.add_argument("--tag")
    meta.add_argument("--github-output", action="store_true")
    note = commands.add_parser("notes")
    note.add_argument("--tag", required=True)
    note.add_argument("--output", type=Path, required=True)
    draft = commands.add_parser("draft")
    draft.add_argument("--tag", required=True)
    draft.add_argument("--manifest", type=Path, required=True)
    draft.add_argument("--notes", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "check":
            check()
            print("Version, changelog and generated documentation agree.")
        elif args.command == "prepare":
            prepare(args.version, args.date)
            print(f"Prepared {args.version}. Review and commit the four changed release files before tagging.")
        elif args.command == "metadata":
            info = metadata(args.tag)
            print("\n".join(f"{key}={value}" for key, value in info.items()) if args.github_output else json.dumps(info, indent=2))
        elif args.command == "notes":
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(notes(metadata(args.tag)), encoding="utf-8", newline="\n")
        else:
            print(create_draft(args.tag, args.manifest, args.notes))
    except (ValueError, KeyError, OSError) as exc:
        print(f"release_workflow.py: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
