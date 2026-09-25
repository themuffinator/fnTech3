"""Generate a static light file for every official Medal of Honor: Allied Assault map.

Each map's `maps/<name>.lights.json` feeds the renderers' static map lights
(`r_staticLights`), as Alice's and F.A.K.K.2's do. The lights come from the
map's own sphere lights, 2015 lump 19: the lights its compiler kept for
lighting models, in Ritual's 56-byte mapspherel_t, which every retail map's
lump divides into. The selection, the exclusions and the file are the Alice
generator's (generate_stock_alice_lights.py). The model-light law is 2015's,
`|I| x 7500 / d^2` (OpenMoHAA's tr_sphere_shade.cpp RB_Sphere_AddLight,
reference only), a quarter of Alice's, and the radius and the colour balance
follow it; MOHAA.exe is encrypted, so the law is OpenMoHAA's reading [O].

The installation is only read from, and the files are published only once
every map has converted.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import sys
import tempfile
import zipfile
from collections import defaultdict
from pathlib import Path
from typing import Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))

from generate_stock_alice_lights import (DLIGHT_AT_RADIUS, RENDERER_DLIGHT_SCALE, RENDERER_STATIC_LIGHT_BUDGET,
                                         SUPPLEMENT_PEAK, CompiledLight, exclusion, finite, number, select_lights,
                                         static_color, vector)
from mohaa_retail_maps import GAME_DIRECTORY, PROFILE, discover_official_maps, read_map, resolve_game_directory
from stock_mohaa_maps import STOCK_MOHAA_MAPS

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK_ROOT = ROOT / ".tmp" / "mohaa-lights"
DEFAULT_OUTPUT_ROOT = ROOT / "pkg" / "games"
GENERATOR = "fnTech3 scripts/generate_stock_mohaa_lights.py"

MOHAA_IDENT = b"2015"
MOHAA_VERSION = 19
MOHAA_HEADER_LUMPS = 28
MOHAA_HEADER_BYTES = 12 + MOHAA_HEADER_LUMPS * 8
LUMP_LEAFS = 8
LUMP_SPHERELIGHTS = 19
LEAF_BYTES = 64

# One sphere light: origin, colour, intensity, leaf, needs-trace, spot, spot
# direction, spot radius by distance (code/qcommon/bsp_mohaa.h).
LIGHT_RECORD = struct.Struct("<3f3ffiii3ff")
assert LIGHT_RECORD.size == 56
# 2015's loader refuses a map with more: it drops one whose count reaches
# MAX_MAP_SPHERE_L_SIZE, 1532 [O].
RETAIL_MAX_LIGHTS = 1531

# 2015 lights a model with power = |intensity| * 7500 / d^2, and leaves out a
# light whose power there is under 5 [O]; the renderers' own MoHAA light grid
# reads the same law (MOHAA_LIGHT_SCALE in tr_mohaa_world.c). The radius
# written is where that power falls to 16, as Alice's is.
RETAIL_SPHERE_LIGHT_SCALE = 7500.0


def mohaa_lumps(data: bytes) -> list[tuple[int, int]]:
    if len(data) < MOHAA_HEADER_BYTES:
        raise ValueError("truncated 2015 header")
    if data[:4] != MOHAA_IDENT:
        raise ValueError("not a 2015 map")
    version = struct.unpack_from("<i", data, 4)[0]
    if version != MOHAA_VERSION:
        raise ValueError(f"expected Medal of Honor's 2015 19, got version {version}")
    values = struct.unpack_from(f"<{MOHAA_HEADER_LUMPS * 2}i", data, 12)
    lumps = [(values[i], values[i + 1]) for i in range(0, len(values), 2)]
    for offset, length in lumps:
        if length and (offset < MOHAA_HEADER_BYTES or length < 0 or offset > len(data) - length):
            raise ValueError("2015 lump range is outside the file")
    return lumps


def read_compiled_lights(data: bytes) -> tuple[list[CompiledLight], list[int]]:
    """The map's sphere lights and each leaf's cluster, refusing anything malformed."""
    lumps = mohaa_lumps(data)
    leaf_offset, leaf_length = lumps[LUMP_LEAFS]
    if leaf_length % LEAF_BYTES:
        raise ValueError("2015 leaf lump is not a whole number of records")
    clusters = [struct.unpack_from("<i", data, leaf_offset + index * LEAF_BYTES)[0]
                for index in range(leaf_length // LEAF_BYTES)]
    light_offset, light_length = lumps[LUMP_SPHERELIGHTS]
    if light_length % LIGHT_RECORD.size:
        raise ValueError("2015 sphere light lump is not a whole number of records")
    count = light_length // LIGHT_RECORD.size
    if count > RETAIL_MAX_LIGHTS:
        raise ValueError(f"{count} sphere lights; retail refuses more than {RETAIL_MAX_LIGHTS}")
    lights = []
    for index in range(count):
        fields = LIGHT_RECORD.unpack_from(data, light_offset + index * LIGHT_RECORD.size)
        light = CompiledLight(index, fields[0:3], fields[3:6], fields[6], fields[7], fields[8],
                              fields[9], fields[10:13], fields[13], 0.0)
        if not finite((*light.origin, *light.color, light.intensity, *light.spot_dir,
                       light.spot_radius_by_distance)):
            raise ValueError(f"sphere light {index} is not finite")
        if not 0 <= light.leaf < len(clusters):
            raise ValueError(f"sphere light {index} names leaf {light.leaf} of {len(clusters)}")
        if light.needs_trace not in (0, 1) or light.spot not in (0, 1):
            raise ValueError(f"sphere light {index} has a flag other than 0 or 1")
        lights.append(light)
    return lights, clusters


def static_radius(light: CompiledLight) -> float:
    return math.sqrt(RETAIL_SPHERE_LIGHT_SCALE * light.intensity / DLIGHT_AT_RADIUS)


def per_pixel_share(light: CompiledLight, at: Sequence[float]) -> float:
    """The fraction of its colour a light adds at a point by default."""
    reach = static_radius(light) * RENDERER_DLIGHT_SCALE
    if reach <= 0.0:
        return 0.0
    distance_squared = sum((a - b) ** 2 for a, b in zip(light.origin, at))
    x = 1.0 - distance_squared / (reach * reach)
    return x * x * (3.0 - 2.0 * x) if x > 0.0 else 0.0


def balance_colors(lights: Sequence[CompiledLight]) -> dict[int, float]:
    """Each light's colour scale, by record, as Alice's balance_colors, over 2015's radii."""
    scales: dict[int, float] = {}
    for light in lights:
        shares = sorted((max(static_color(other)) * per_pixel_share(other, light.origin) for other in lights),
                        reverse=True)[:RENDERER_STATIC_LIGHT_BUDGET]
        total = sum(shares)
        scales[light.record] = min(1.0, SUPPLEMENT_PEAK / total) if total > 0.0 else 1.0
    return scales


def light_entry(name: str, light: CompiledLight, scale: float = 1.0) -> str:
    """One light's line, as Alice's light_entry writes it, with 2015's radius."""
    fields = [
        ("name", json.dumps(f"{name.rsplit('/', 1)[-1]}-light-{light.record:04d}")),
        ("type", json.dumps("spot" if light.spot else "point")),
        ("origin", vector(light.origin, 3)),
    ]
    if light.spot:
        fields.append(("direction", vector(light.spot_dir, 5)))
    fields += [
        ("color", vector([component * scale for component in static_color(light)], 4)),
        ("intensity", number(light.intensity, 3)),
        ("radius", number(static_radius(light), 1)),
    ]
    if light.spot:
        fields.append(("outerAngle", number(math.degrees(math.atan(light.spot_radius_by_distance)), 2)))
    fields.append(("castsShadows", "true"))
    return "{" + ", ".join(f"{json.dumps(key)}: {value}" for key, value in fields) + "}"


def light_document(name: str, lights: Sequence[CompiledLight], scales: dict[int, float] | None = None) -> bytes:
    header = [
        ("version", "1"),
        ("generator", json.dumps(GENERATOR)),
        ("map", json.dumps(name)),
        ("source", json.dumps("2015 19 lump 19, the map's sphere lights")),
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
        # A negative radius by distance is a spot that 2015 moves across a disc
        # and never passes its cone test [O]; Alice's rule calls it an emitter.
        reason = exclusion(light)
        if reason:
            excluded[reason] += 1
        else:
            usable.append(light)
    selected = select_lights(usable, clusters)
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
        "smallestBalance": round(min(scales.values(), default=1.0), 4),
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
    for name in STOCK_MOHAA_MAPS:
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
    for name in STOCK_MOHAA_MAPS:
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
    (work / "mohaa-lights.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                                            encoding="utf-8")
    if any(run["status"] != "passed" for run in runs):
        raise ValueError(f"conversion failed; existing sidecars preserved; see {work / 'mohaa-lights.json'}")
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
                        help="Medal of Honor installation root or its main directory.")
    parser.add_argument("--work-root", type=Path, default=DEFAULT_WORK_ROOT)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT,
                        help="Root containing mohaa/.")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        manifest = generate_sidecars(args)
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print(f"generate_stock_mohaa_lights.py: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(manifest["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
