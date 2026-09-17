"""Generate and audit every official RTCW map without modifying the installation."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path
from typing import Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))

import audio_zone_sweep as sweep
from stock_rtcw_maps import STOCK_RTCW_MAPS

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK_ROOT = ROOT / ".tmp" / "rtcw-audio-zones"
DEFAULT_OUTPUT_ROOT = ROOT / "pkg" / "games"
OFFICIAL_PAK = re.compile(r"(?:pak0|sp_pak\d+|mp_pak\d+|mp_pakmaps\d+)\.pk3", re.I)
MAP_ENTRY = re.compile(r"maps/([a-z0-9][a-z0-9_-]*)\.bsp", re.I)


def resolve_main_directory(path: Path) -> Path:
    root = path.expanduser().resolve()
    if root.is_dir() and root.name.lower() == "main":
        return root
    candidates = [entry for entry in root.iterdir() if entry.is_dir() and entry.name.lower() == "main"]
    if len(candidates) != 1:
        raise ValueError(f"expected one RTCW Main directory under {root}")
    return candidates[0]


def discover_official_maps(main: Path) -> dict[tuple[str, str], tuple[Path, str]]:
    """Use official pak order, including SP patches, and reject changed inventories."""
    paks: dict[str, Path] = {}
    for pak in main.iterdir():
        if not pak.is_file() or not OFFICIAL_PAK.fullmatch(pak.name):
            continue
        key = pak.name.lower()
        if key in paks:
            raise ValueError(f"case-ambiguous official packages: {paks[key]} and {pak}")
        paks[key] = pak
    maps: dict[tuple[str, str], tuple[Path, str]] = {}
    for pak_name, pak in sorted(paks.items()):
        profile = "rtcw-mp" if pak_name.startswith("mp_") else "rtcw-sp"
        with zipfile.ZipFile(pak) as archive:
            seen: set[str] = set()
            for entry in archive.namelist():
                key = entry.lower()
                if not key.startswith("maps/") or not key.endswith(".bsp"):
                    continue
                match = MAP_ENTRY.fullmatch(key)
                if not match:
                    raise ValueError(f"unsafe BSP entry in {pak.name}: {entry!r}")
                if key in seen:
                    raise ValueError(f"duplicate or case-ambiguous BSP in {pak.name}: {entry!r}")
                seen.add(key)
                maps[profile, match[1]] = (pak, entry)
    expected = {(profile, name) for profile, names in STOCK_RTCW_MAPS.items() for name in names}
    missing, unexpected = expected - maps.keys(), maps.keys() - expected
    if missing or unexpected:
        raise ValueError(f"official RTCW map inventory mismatch: missing={sorted(missing)}, unexpected={sorted(unexpected)}")
    return maps


def generate_sidecars(args: argparse.Namespace) -> dict[str, object]:
    main = resolve_main_directory(args.retail_path)
    maps = discover_official_maps(main)
    tool = sweep.resolve_tool(args.tool, dry_run=False)
    work = args.work_root.expanduser().resolve()
    output = args.output_root.expanduser().resolve()
    allowed_work = (ROOT / ".tmp").resolve()
    if work == allowed_work or not work.is_relative_to(allowed_work):
        raise ValueError(f"--work-root must be a child of {allowed_work}")
    if output.is_relative_to(main.parent) or main.parent.is_relative_to(output):
        raise ValueError("--output-root must not overlap the retail installation")
    if work.is_relative_to(output) or output.is_relative_to(work):
        raise ValueError("--work-root and --output-root must not overlap")
    for profile, names in STOCK_RTCW_MAPS.items():
        for name in names:
            destination = output / profile / "main" / "maps" / f"{name}.azb"
            if not destination.resolve().is_relative_to(output):
                raise ValueError(f"sidecar destination escapes --output-root: {destination}")
    work.mkdir(parents=True, exist_ok=True)
    runs: list[dict[str, object]] = []
    manifest: dict[str, object] = {
        "tool": tool,
        "toolSha256": hashlib.sha256(Path(tool).read_bytes()).hexdigest(),
        "samples": args.samples,
        "maxZones": args.max_zones,
        "strict": args.strict,
        "installation": str(main.parent),
        "outputRoot": str(output),
        "runs": runs,
    }
    # This fresh, owned directory contains the only extracted retail files.
    # No stale directory is recursively removed and no installation file is linked.
    with tempfile.TemporaryDirectory(prefix="run-", dir=work) as temporary:
        scratch = Path(temporary)
        stage = scratch / "stage"
        for profile, names in STOCK_RTCW_MAPS.items():
            for name in names:
                pak, entry = maps[profile, name]
                with zipfile.ZipFile(pak) as archive:
                    info = archive.getinfo(entry)
                    data = archive.read(info)
                relative = Path(profile) / "main" / "maps" / f"{name}.azb"
                bsp = scratch / f"{name}.bsp"
                bsp.write_bytes(data)
                azb = stage / relative
                azb.parent.mkdir(parents=True, exist_ok=True)
                command = sweep.build_generate_command(tool, bsp, azb, None, None, args.max_zones)
                command[2:2] = ["--profile", profile]
                generated = sweep.run_command(command)
                run: dict[str, object] = {
                    "map": name, "profile": profile, "output": relative.as_posix(),
                    "pak": pak.name, "entry": entry, "bspBytes": len(data),
                    "bspCrc32": f"{info.CRC:08x}",
                    "bspSha256": hashlib.sha256(data).hexdigest(),
                    "generateStdout": generated.stdout.strip(),
                    "generateStderr": generated.stderr.strip(),
                    "status": "generate-failed",
                }
                bsp.unlink()
                if generated.returncode == 0:
                    audited = sweep.run_command(sweep.build_audit_command(tool, azb, args.strict, args.samples))
                    run.update(
                        audit=sweep.parse_audit_output(audited.stdout),
                        auditStdout=audited.stdout.strip(), auditStderr=audited.stderr.strip(),
                        status="passed" if audited.returncode == 0 else "audit-failed",
                        azbBytes=azb.stat().st_size,
                        azbSha256=hashlib.sha256(azb.read_bytes()).hexdigest(),
                    )
                runs.append(run)
                manifest["summary"] = sweep.summarize_runs(runs)
                sweep.write_manifest_json(work / "audio-zone-sweep.json", manifest)
                print(f"{profile}/{name}: {run['status']}", flush=True)
        sweep.write_manifest_csv(work / "audio-zone-sweep.csv", manifest)
        if any(run["status"] != "passed" for run in runs):
            raise ValueError(f"generation/audit failed; existing sidecars preserved; see {work / 'audio-zone-sweep.json'}")
        # Publish only after every map passes. Each file replaces atomically;
        # unrelated/custom sidecars are never pruned.
        for run in runs:
            relative = Path(str(run["output"]))
            destination = output / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile(prefix=".azb-", suffix=".tmp", dir=destination.parent, delete=False) as handle:
                pending = Path(handle.name)
            try:
                shutil.copyfile(stage / relative, pending)
                pending.replace(destination)
            finally:
                pending.unlink(missing_ok=True)
    return manifest


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("retail_path", type=Path, help="RTCW installation root or Main directory.")
    parser.add_argument("--tool", type=Path, default=sweep.default_tool_path())
    parser.add_argument("--work-root", type=Path, default=DEFAULT_WORK_ROOT)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT, help="Root containing rtcw-sp/ and rtcw-mp/.")
    parser.add_argument("--samples", type=int, default=32768)
    parser.add_argument("--max-zones", type=int, default=512)
    parser.add_argument("--strict", action="store_true", help="Treat audit warnings as failures.")
    args = parser.parse_args(argv)
    if not 1 <= args.samples <= 1_000_000:
        parser.error("--samples must be between 1 and 1000000")
    if not 1 <= args.max_zones <= 1024:
        parser.error("--max-zones must be between 1 and 1024")
    return args


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        manifest = generate_sidecars(args)
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print(f"generate_stock_rtcw_audio_zones.py: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(manifest["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
