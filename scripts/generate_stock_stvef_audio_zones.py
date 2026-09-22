"""Generate and audit an audio zone file for every official Elite Force map.

The installation is only read from: each BSP is extracted into a fresh working
directory under .tmp, compiled with the engine's zone compiler, audited, and
published only once every map has passed. Elite Force is a Quake III fork, so
its IBSP 46 maps keep Quake's surface-flag meanings; the profile selects which
package the sidecar belongs to and pins the expected BSP version.
"""
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
from stock_stvef_maps import STOCK_STVEF_MAPS

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK_ROOT = ROOT / ".tmp" / "stvef-audio-zones"
DEFAULT_OUTPUT_ROOT = ROOT / "pkg" / "games"
GAME_DIRECTORY = "baseef"
PUBLISHED_GAME_DIRECTORY = "baseEF"
# pak0 ships the base game and pak1/pak3 the Expansion Pack; a texture or mod
# package sitting beside them is not official and is never read.
OFFICIAL_PAK = re.compile(r"pak[0-3]\.pk3", re.I)
# Ten official maps start with an underscore and ten live in maps/tour/, so one
# optional directory level and a leading underscore are allowed, nothing else.
MAP_ENTRY = re.compile(r"maps/((?:[a-z0-9_][a-z0-9_-]*/)?[a-z0-9_][a-z0-9_-]*)\.bsp", re.I)
ARENA_ENTRIES = ("scripts/arenas.txt", "scripts/xpack.arena")
ARENA_MAP = re.compile(r'map\s*"([^"]+)"', re.I)


def resolve_game_directory(path: Path) -> Path:
    root = path.expanduser().resolve()
    if root.is_dir() and root.name.lower() == GAME_DIRECTORY:
        return root
    candidates = [entry for entry in root.iterdir()
                  if entry.is_dir() and entry.name.lower() == GAME_DIRECTORY]
    if len(candidates) != 1:
        raise ValueError(f"expected one Elite Force {PUBLISHED_GAME_DIRECTORY} directory under {root}")
    return candidates[0]


def official_packages(game: Path) -> dict[str, Path]:
    paks: dict[str, Path] = {}
    for pak in game.iterdir():
        if not pak.is_file() or not OFFICIAL_PAK.fullmatch(pak.name):
            continue
        key = pak.name.lower()
        if key in paks:
            raise ValueError(f"case-ambiguous official packages: {paks[key]} and {pak}")
        paks[key] = pak
    if not paks:
        raise ValueError(f"no official pak0..pak3 packages under {game}")
    return paks


def discover_official_maps(game: Path) -> tuple[dict[str, tuple[Path, str]], set[str]]:
    """Every BSP in the official paks, and the maps the arena metadata declares."""
    paks = official_packages(game)
    maps: dict[str, tuple[Path, str]] = {}
    arenas: set[str] = set()
    for _, pak in sorted(paks.items()):
        with zipfile.ZipFile(pak) as archive:
            for entry in archive.namelist():
                key = entry.lower()
                if key in ARENA_ENTRIES:
                    text = archive.read(entry).decode("latin-1")
                    arenas.update(match[1].lower() for match in ARENA_MAP.finditer(text))
                    continue
                if not key.startswith("maps/") or not key.endswith(".bsp"):
                    continue
                match = MAP_ENTRY.fullmatch(key)
                if not match:
                    raise ValueError(f"unsafe BSP entry in {pak.name}: {entry!r}")
                # A later package legitimately replaces an earlier map.
                maps[match[1]] = (pak, entry)
    return maps, arenas


def check_inventory(maps: dict[str, tuple[Path, str]], arenas: set[str]) -> None:
    expected = {name for names in STOCK_STVEF_MAPS.values() for name in names}
    missing, unexpected = expected - maps.keys(), maps.keys() - expected
    if missing or unexpected:
        raise ValueError(f"official Elite Force map inventory mismatch: "
                         f"missing={sorted(missing)}, unexpected={sorted(unexpected)}")
    # The shipped arena metadata, not a name prefix, decides which maps are
    # Holomatch's; a changed installation must not silently repartition them.
    declared = {name for name in arenas if name in maps}
    if declared != set(STOCK_STVEF_MAPS["stvef-mp"]):
        raise ValueError(f"Holomatch arena metadata mismatch: "
                         f"declared={sorted(declared)}, expected={sorted(STOCK_STVEF_MAPS['stvef-mp'])}")
    undeclared = arenas - maps.keys()
    if undeclared:
        raise ValueError(f"arena metadata names maps with no BSP: {sorted(undeclared)}")


def generate_sidecars(args: argparse.Namespace) -> dict[str, object]:
    game = resolve_game_directory(args.retail_path)
    maps, arenas = discover_official_maps(game)
    check_inventory(maps, arenas)
    tool = sweep.resolve_tool(args.tool, dry_run=False)
    work = args.work_root.expanduser().resolve()
    output = args.output_root.expanduser().resolve()
    allowed_work = (ROOT / ".tmp").resolve()
    if work == allowed_work or not work.is_relative_to(allowed_work):
        raise ValueError(f"--work-root must be a child of {allowed_work}")
    if output.is_relative_to(game.parent) or game.parent.is_relative_to(output):
        raise ValueError("--output-root must not overlap the retail installation")
    if work.is_relative_to(output) or output.is_relative_to(work):
        raise ValueError("--work-root and --output-root must not overlap")
    for profile, names in STOCK_STVEF_MAPS.items():
        for name in names:
            destination = output / profile / PUBLISHED_GAME_DIRECTORY / "maps" / f"{name}.azb"
            if not destination.resolve().is_relative_to(output):
                raise ValueError(f"sidecar destination escapes --output-root: {destination}")
    work.mkdir(parents=True, exist_ok=True)
    runs: list[dict[str, object]] = []
    manifest: dict[str, object] = {
        "tool": tool,
        "toolSha256": hashlib.sha256(Path(tool).read_bytes()).hexdigest(),
        "profiles": sorted(STOCK_STVEF_MAPS),
        "samples": args.samples,
        "maxZones": args.max_zones,
        "strict": args.strict,
        "installation": str(game.parent),
        "outputRoot": str(output),
        "runs": runs,
    }
    # The fresh, owned directory is the only place retail files are extracted.
    with tempfile.TemporaryDirectory(prefix="run-", dir=work) as temporary:
        scratch = Path(temporary)
        stage = scratch / "stage"
        for profile, names in STOCK_STVEF_MAPS.items():
            for name in names:
                pak, entry = maps[name]
                with zipfile.ZipFile(pak) as archive:
                    info = archive.getinfo(entry)
                    data = archive.read(info)
                relative = Path(profile) / PUBLISHED_GAME_DIRECTORY / "maps" / f"{name}.azb"
                bsp = scratch / f"{name.replace('/', '_')}.bsp"
                bsp.write_bytes(data)
                azb = stage / relative
                azb.parent.mkdir(parents=True, exist_ok=True)
                command = sweep.build_generate_command(tool, bsp, azb, None,
                                                       args.material_map, args.max_zones)
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
                    audited = sweep.run_command(
                        sweep.build_audit_command(tool, azb, args.strict, args.samples))
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
            raise ValueError(f"generation/audit failed; existing sidecars preserved; "
                             f"see {work / 'audio-zone-sweep.json'}")
        # Publish only after every map passes; each file replaces atomically and
        # nothing unrelated is pruned.
        for run in runs:
            relative = Path(str(run["output"]))
            destination = output / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile(prefix=".azb-", suffix=".tmp", dir=destination.parent,
                                             delete=False) as handle:
                pending = Path(handle.name)
            try:
                shutil.copyfile(stage / relative, pending)
                pending.replace(destination)
            finally:
                pending.unlink(missing_ok=True)
    return manifest


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("retail_path", type=Path,
                        help=f"Elite Force installation root or its {PUBLISHED_GAME_DIRECTORY} directory.")
    parser.add_argument("--tool", type=Path, default=sweep.default_tool_path())
    parser.add_argument("--work-root", type=Path, default=DEFAULT_WORK_ROOT)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT,
                        help="Root containing stvef-sp/ and stvef-mp/.")
    parser.add_argument("--material-map", type=Path, default=None,
                        help="Optional shader-name material overrides.")
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
        print(f"generate_stock_stvef_audio_zones.py: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(manifest["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
