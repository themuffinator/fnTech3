"""Generate and audit an audio zone file for every official Heavy Metal: F.A.K.K.2 map.

The installation is only read from: each FAKK 12 map is extracted into a fresh
working directory under .tmp, compiled with the engine's zone compiler under
the fakk2 profile's flag rules -- Ritual's, as Alice's are -- and the checked-in
F.A.K.K.2 material map, audited, and published only once every map has passed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path
from typing import Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))

import audio_zone_sweep as sweep
from fakk2_retail_maps import GAME_DIRECTORY, PROFILE, discover_official_maps, read_map, resolve_game_directory
from stock_fakk2_maps import STOCK_FAKK2_MAPS

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK_ROOT = ROOT / ".tmp" / "fakk2-audio-zones"
DEFAULT_OUTPUT_ROOT = ROOT / "pkg" / "games"
DEFAULT_MATERIAL_MAP = ROOT / "scripts" / "fakk2-audio-materials.txt"


def generate_sidecars(args: argparse.Namespace) -> dict[str, object]:
    game = resolve_game_directory(args.retail_path)
    maps = discover_official_maps(game)
    tool = sweep.resolve_tool(args.tool, dry_run=False)
    material_map = args.material_map.expanduser().resolve()
    if not material_map.is_file():
        raise ValueError(f"material map not found: {material_map}")
    work = args.work_root.expanduser().resolve()
    output = args.output_root.expanduser().resolve()
    allowed_work = (ROOT / ".tmp").resolve()
    if work == allowed_work or not work.is_relative_to(allowed_work):
        raise ValueError(f"--work-root must be a child of {allowed_work}")
    if output.is_relative_to(game.parent) or game.parent.is_relative_to(output):
        raise ValueError("--output-root must not overlap the retail installation")
    if work.is_relative_to(output) or output.is_relative_to(work):
        raise ValueError("--work-root and --output-root must not overlap")
    for name in STOCK_FAKK2_MAPS:
        destination = output / PROFILE / GAME_DIRECTORY / "maps" / f"{name}.azb"
        if not destination.resolve().is_relative_to(output):
            raise ValueError(f"sidecar destination escapes --output-root: {destination}")
    work.mkdir(parents=True, exist_ok=True)
    runs: list[dict[str, object]] = []
    manifest: dict[str, object] = {
        "tool": tool,
        "toolSha256": hashlib.sha256(Path(tool).read_bytes()).hexdigest(),
        "profile": PROFILE,
        "materialMap": material_map.name,
        "materialMapSha256": hashlib.sha256(material_map.read_bytes()).hexdigest(),
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
        for name in STOCK_FAKK2_MAPS:
            pak, entry = maps[name]
            data, crc = read_map(pak, entry)
            relative = Path(PROFILE) / GAME_DIRECTORY / "maps" / f"{name}.azb"
            bsp = scratch / f"{name}.bsp"
            bsp.write_bytes(data)
            azb = stage / relative
            azb.parent.mkdir(parents=True, exist_ok=True)
            command = sweep.build_generate_command(tool, bsp, azb, None, material_map, args.max_zones)
            command[2:2] = ["--profile", PROFILE]
            generated = sweep.run_command(command)
            run: dict[str, object] = {
                "map": name, "profile": PROFILE, "output": relative.as_posix(),
                "pak": pak.name, "entry": entry, "bspBytes": len(data),
                "bspCrc32": f"{crc:08x}",
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
            print(f"{PROFILE}/{name}: {run['status']}", flush=True)
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
                        help="F.A.K.K.2 installation root or its fakk directory.")
    parser.add_argument("--tool", type=Path, default=sweep.default_tool_path())
    parser.add_argument("--material-map", type=Path, default=DEFAULT_MATERIAL_MAP)
    parser.add_argument("--work-root", type=Path, default=DEFAULT_WORK_ROOT)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT,
                        help="Root containing fakk2/.")
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
        print(f"generate_stock_fakk2_audio_zones.py: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(manifest["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
