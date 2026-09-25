"""Generate a static light file for every official Heavy Metal: F.A.K.K.2 map.

Each map's `maps/<name>.lights.json` feeds the renderers' static map lights
(`r_staticLights`), as Alice's do. The lights come from the map's own compiled
light list, FAKK lump 17. F.A.K.K.2's records are Ritual's 56-byte
mapspherel_t -- every retail map's lump divides by 56, and all but two refuse
Alice's 60 -- without the `cap` Alice's engine added, so no light is capped.
The selection, the exclusions and the file are the Alice generator's
(generate_stock_alice_lights.py), and so is the radius: at its defaults
fakk2.exe 1.02 lights a model with alice.exe's `|I| x 30000 / d^2`, 7500 a
light times its map overbright factor of 4 (RETAIL_ENTITY_LIGHT_SCALE below).

The installation is only read from, and the files are published only once
every map has converted.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import tempfile
import zipfile
from collections import defaultdict
from pathlib import Path
from typing import Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))

from fakk2_retail_maps import GAME_DIRECTORY, PROFILE, discover_official_maps, read_map, resolve_game_directory
from generate_stock_alice_lights import (CompiledLight, FAKK_HEADER_BYTES, FAKK_HEADER_LUMPS, FAKK_IDENT, LEAF_BYTES,
                                         LUMP_ENTLIGHTS, LUMP_LEAFS, RETAIL_ENTITY_LIGHT_SCALE as ALICE_LIGHT_SCALE,
                                         RETAIL_MAX_LIGHTS, balance_colors, exclusion, finite, light_entry,
                                         select_lights)
from stock_fakk2_maps import STOCK_FAKK2_MAPS

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK_ROOT = ROOT / ".tmp" / "fakk2-lights"
DEFAULT_OUTPUT_ROOT = ROOT / "pkg" / "games"
GENERATOR = "fnTech3 scripts/generate_stock_fakk2_lights.py"

FAKK_VERSION_FAKK2 = 12

# One compiled light: origin, colour, intensity, leaf, needs-trace, spot, spot
# direction, spot radius by distance -- Ritual's mapspherel_t, which the SDK's
# qfiles.h declares and every F.A.K.K.2 map's lump 17 divides into.
LIGHT_RECORD = struct.Struct("<3f3ffiii3ff")
assert LIGHT_RECORD.size == 56

# fakk2.exe 1.02 adds |intensity| * 7500 / d^2 of each light to a model and
# leaves out a light whose power there is under 1 (0x0048dd80, the constant at
# 0x004d6050, which its spot, vertex and dynamic-light paths multiply by too);
# no cap clamps a light. Its last step (0x0048e090) multiplies the sum by
# 1 << (r_mapOverBrightBits - tr.overbrightBits), the shift its map lighting
# takes, before clamping it to 255: r_mapOverBrightBits is registered "2"
# (0x00460edb) and r_overBrightBits "0" (tr.overbrightBits, 0x0046ff70).
# So at its defaults a model is lit with alice.exe's law, and the Alice
# generator's radius, where that power falls to 16, is F.A.K.K.2's too.
RETAIL_LIGHT_SCALE = 7500.0
RETAIL_MAP_OVERBRIGHT_BITS = 2
RETAIL_OVERBRIGHT_BITS = 0
RETAIL_ENTITY_LIGHT_SCALE = RETAIL_LIGHT_SCALE * (1 << (RETAIL_MAP_OVERBRIGHT_BITS - RETAIL_OVERBRIGHT_BITS))
assert RETAIL_ENTITY_LIGHT_SCALE == ALICE_LIGHT_SCALE


def fakk_lumps(data: bytes) -> list[tuple[int, int]]:
    if len(data) < FAKK_HEADER_BYTES:
        raise ValueError("truncated FAKK header")
    if data[:4] != FAKK_IDENT:
        raise ValueError("not a FAKK map")
    version = struct.unpack_from("<i", data, 4)[0]
    if version != FAKK_VERSION_FAKK2:
        raise ValueError(f"expected F.A.K.K.2's FAKK 12, got version {version}")
    values = struct.unpack_from(f"<{FAKK_HEADER_LUMPS * 2}i", data, 12)
    lumps = [(values[i], values[i + 1]) for i in range(0, len(values), 2)]
    for offset, length in lumps:
        if length and (offset < FAKK_HEADER_BYTES or length < 0 or offset > len(data) - length):
            raise ValueError("FAKK lump range is outside the file")
    return lumps


def read_compiled_lights(data: bytes) -> tuple[list[CompiledLight], list[int]]:
    """The map's compiled lights and each leaf's cluster, refusing anything malformed."""
    lumps = fakk_lumps(data)
    leaf_offset, leaf_length = lumps[LUMP_LEAFS]
    if leaf_length % LEAF_BYTES:
        raise ValueError("FAKK leaf lump is not a whole number of records")
    clusters = [struct.unpack_from("<i", data, leaf_offset + index * LEAF_BYTES)[0]
                for index in range(leaf_length // LEAF_BYTES)]
    light_offset, light_length = lumps[LUMP_ENTLIGHTS]
    if light_length % LIGHT_RECORD.size:
        raise ValueError("FAKK entity light lump is not a whole number of records")
    count = light_length // LIGHT_RECORD.size
    if count > RETAIL_MAX_LIGHTS:
        raise ValueError(f"{count} compiled lights; retail refuses more than {RETAIL_MAX_LIGHTS}")
    lights = []
    for index in range(count):
        fields = LIGHT_RECORD.unpack_from(data, light_offset + index * LIGHT_RECORD.size)
        light = CompiledLight(index, fields[0:3], fields[3:6], fields[6], fields[7], fields[8],
                              fields[9], fields[10:13], fields[13], 0.0)
        if not finite((*light.origin, *light.color, light.intensity, *light.spot_dir,
                       light.spot_radius_by_distance)):
            raise ValueError(f"compiled light {index} is not finite")
        if not 0 <= light.leaf < len(clusters):
            raise ValueError(f"compiled light {index} names leaf {light.leaf} of {len(clusters)}")
        if light.needs_trace not in (0, 1) or light.spot not in (0, 1):
            raise ValueError(f"compiled light {index} has a flag other than 0 or 1")
        lights.append(light)
    return lights, clusters


def light_document(name: str, lights: Sequence[CompiledLight], scales: dict[int, float] | None = None) -> bytes:
    header = [
        ("version", "1"),
        ("generator", json.dumps(GENERATOR)),
        ("map", json.dumps(name)),
        ("source", json.dumps("FAKK 12 lump 17, the map's compiled entity lights")),
    ]
    lines = ["{"] + [f"  {json.dumps(key)}: {value}," for key, value in header]
    entries = [light_entry(name, light, (scales or {}).get(light.record, 1.0)) for light in lights]
    if entries:
        lines.append('  "lights": [')
        lines += [f"    {entry}{',' if index + 1 < len(entries) else ''}" for index, entry in enumerate(entries)]
        lines.append("  ]")
    else:
        lines.append('  "lights": []')
    lines.append("}")
    return ("\n".join(lines) + "\n").encode("ascii")


def convert_map(name: str, data: bytes) -> tuple[bytes, dict[str, object]]:
    lights, clusters = read_compiled_lights(data)
    excluded: dict[str, int] = defaultdict(int)
    usable = []
    for light in lights:
        reason = exclusion(light)
        if reason:
            excluded[reason] += 1
        else:
            usable.append(light)
    selected = select_lights(usable, clusters)
    # The colours are balanced for the renderers' per-pixel pass, as Alice's are
    # (generate_stock_alice_lights.balance_colors).
    scales = balance_colors(selected)
    payload = light_document(name, selected, scales)
    summary = {
        "compiledLights": len(lights),
        "excluded": dict(sorted(excluded.items())),
        "usable": len(usable),
        "usableClusters": len({clusters[light.leaf] for light in usable}),
        "selected": len(selected),
        "selectedClusters": len({clusters[light.leaf] for light in selected}),
        "selectedSpots": sum(1 for light in selected if light.spot),
        "selectedOutsideClusters": sum(1 for light in selected if clusters[light.leaf] < 0),
        "dropped": len(usable) - len(selected),
        "balanced": sum(1 for scale in scales.values() if scale < 1.0),
        "smallestBalance": round(min(scales.values()), 4) if scales else 1.0,
    }
    return payload, summary


def generate_sidecars(args: argparse.Namespace) -> dict[str, object]:
    game = resolve_game_directory(args.retail_path)
    maps = discover_official_maps(game)
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
        destination = output / PROFILE / GAME_DIRECTORY / "maps" / f"{name}.lights.json"
        if not destination.resolve().is_relative_to(output):
            raise ValueError(f"sidecar destination escapes --output-root: {destination}")
    work.mkdir(parents=True, exist_ok=True)
    runs: list[dict[str, object]] = []
    manifest: dict[str, object] = {
        "generator": GENERATOR,
        "profile": PROFILE,
        "installation": str(game.parent),
        "outputRoot": str(output),
        "runs": runs,
    }
    staged: dict[Path, bytes] = {}
    for name in STOCK_FAKK2_MAPS:
        pak, entry = maps[name]
        data, crc = read_map(pak, entry)
        relative = Path(PROFILE) / GAME_DIRECTORY / "maps" / f"{name}.lights.json"
        run: dict[str, object] = {
            "map": name, "output": relative.as_posix(), "pak": pak.name, "entry": entry,
            "bspBytes": len(data), "bspCrc32": f"{crc:08x}",
            "bspSha256": hashlib.sha256(data).hexdigest(), "status": "failed",
        }
        try:
            payload, summary = convert_map(name, data)
        except ValueError as exc:
            run["error"] = str(exc)
        else:
            staged[relative] = payload
            run.update(summary, status="passed", lightsBytes=len(payload),
                       lightsSha256=hashlib.sha256(payload).hexdigest())
        runs.append(run)
        print(f"{PROFILE}/{name}: {run['status']}", flush=True)
    manifest["summary"] = {
        "maps": len(runs),
        "passed": sum(1 for run in runs if run["status"] == "passed"),
        "selected": sum(int(run.get("selected", 0)) for run in runs),
    }
    (work / "fakk2-lights.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                                            encoding="utf-8")
    if any(run["status"] != "passed" for run in runs):
        raise ValueError(f"conversion failed; existing sidecars preserved; see {work / 'fakk2-lights.json'}")
    # Publish only after every map converts; each file replaces atomically and
    # nothing unrelated is pruned.
    for relative, payload in staged.items():
        destination = output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(prefix=".lights-", suffix=".tmp", dir=destination.parent,
                                         delete=False) as handle:
            pending = Path(handle.name)
            handle.write(payload)
        try:
            pending.replace(destination)
        finally:
            pending.unlink(missing_ok=True)
    return manifest


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("retail_path", type=Path,
                        help="F.A.K.K.2 installation root or its fakk directory.")
    parser.add_argument("--work-root", type=Path, default=DEFAULT_WORK_ROOT)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT,
                        help="Root containing fakk2/.")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        manifest = generate_sidecars(args)
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print(f"generate_stock_fakk2_lights.py: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(manifest["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
