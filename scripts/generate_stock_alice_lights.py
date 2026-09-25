"""Generate a static light file for every official American McGee's Alice map.

Each map's `maps/<name>.lights.json` feeds the renderers' static map lights
(`r_staticLights`), which promote the nearest visible ones into dynamic lights
and shadow the strongest. The lights come from the map's own compiled light
list -- FAKK lump 17, the list Alice's engine lights its models from -- not
from entity text, so what the map's compiler resolved (defaults, targets,
spot cones) is what is read.

The installation is only read from, and the files are published only once
every map has converted.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import shutil
import struct
import sys
import tempfile
import zipfile
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))

from alice_retail_maps import GAME_DIRECTORY, PROFILE, discover_official_maps, read_map, resolve_game_directory
from stock_alice_maps import STOCK_ALICE_MAPS

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK_ROOT = ROOT / ".tmp" / "alice-lights"
DEFAULT_OUTPUT_ROOT = ROOT / "pkg" / "games"
GENERATOR = "fnTech3 scripts/generate_stock_alice_lights.py"

FAKK_IDENT = b"FAKK"
FAKK_VERSION_ALICE = 42
FAKK_HEADER_LUMPS = 20
FAKK_HEADER_BYTES = 12 + FAKK_HEADER_LUMPS * 8
LUMP_LEAFS = 8
LUMP_ENTLIGHTS = 17
LEAF_BYTES = 48

# One compiled light, as alice.exe's loader reads it (0x0049a360, which
# refuses a lump that is not a whole number of these): origin, colour,
# intensity, leaf, needs-trace, spot, spot direction, spot radius by distance
# -- Ritual's mapspherel_t -- and one float Alice adds, the light's `cap`.
LIGHT_RECORD = struct.Struct("<3f3ffiii3fff")
assert LIGHT_RECORD.size == 60
# Retail refuses a map with more (0x0049a39c).
RETAIL_MAX_LIGHTS = 1532

# The renderers keep this many static lights per map and skip the rest
# (MAX_STATIC_MAP_LIGHTS in every renderer's tr_local.h).
MAX_STATIC_MAP_LIGHTS = 128

# Retail lights a model with power = |intensity| * 30000 / d^2 (0x004ab9a6,
# the constant at 0x005004a0), clamped to `cap` when that is set. The radius
# written is where that power falls to 16 (the renderers' DLIGHT_AT_RADIUS).
# The renderers scale every dynamic light's radius by r_dlightScale, 0.5 by
# default, so by default a light ends where retail's power has fallen to 64,
# a quarter of full.
RETAIL_ENTITY_LIGHT_SCALE = 30000.0
DLIGHT_AT_RADIUS = 16.0
# A capped light never gives a model more than `cap` of 255; its colour is
# scaled so a light's brightest contribution is no more than retail's.
LIGHT_LEVEL_FULL = 255.0

# The balance. The renderers' per-pixel pass adds colour x smoothstep(1 -
# d^2 / R^2) to a surface, R being the radius times r_dlightScale: nearly the
# whole colour anywhere well inside R, and on top of the baked lighting that
# already holds the same light. Eight overlapping full-colour lights, the
# default r_staticLightMaxLights, wash a surface white. So each light's colour
# is scaled until the strongest eight contributions at its own origin -- its
# own included -- add at most SUPPLEMENT_PEAK of the surface's brightness.
# The balance is computed for the renderers' defaults.
RENDERER_DLIGHT_SCALE = 0.5
RENDERER_STATIC_LIGHT_BUDGET = 8
SUPPLEMENT_PEAK = 0.5


@dataclass(frozen=True)
class CompiledLight:
    record: int
    origin: tuple[float, float, float]
    color: tuple[float, float, float]
    intensity: float
    leaf: int
    needs_trace: int
    spot: int
    spot_dir: tuple[float, float, float]
    spot_radius_by_distance: float
    cap: float


def fakk_lumps(data: bytes) -> list[tuple[int, int]]:
    if len(data) < FAKK_HEADER_BYTES:
        raise ValueError("truncated FAKK header")
    if data[:4] != FAKK_IDENT:
        raise ValueError("not a FAKK map")
    version = struct.unpack_from("<i", data, 4)[0]
    if version != FAKK_VERSION_ALICE:
        raise ValueError(f"expected Alice's FAKK 42, got version {version}")
    values = struct.unpack_from(f"<{FAKK_HEADER_LUMPS * 2}i", data, 12)
    lumps = [(values[i], values[i + 1]) for i in range(0, len(values), 2)]
    for offset, length in lumps:
        if length and (offset < FAKK_HEADER_BYTES or length < 0 or offset > len(data) - length):
            raise ValueError("FAKK lump range is outside the file")
    return lumps


def finite(values: Sequence[float]) -> bool:
    return all(math.isfinite(value) for value in values)


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
                              fields[9], fields[10:13], fields[13], fields[14])
        if not finite((*light.origin, *light.color, light.intensity, *light.spot_dir,
                       light.spot_radius_by_distance, light.cap)):
            raise ValueError(f"compiled light {index} is not finite")
        if not 0 <= light.leaf < len(clusters):
            raise ValueError(f"compiled light {index} names leaf {light.leaf} of {len(clusters)}")
        if light.needs_trace not in (0, 1) or light.spot not in (0, 1):
            raise ValueError(f"compiled light {index} has a flag other than 0 or 1")
        lights.append(light)
    return lights, clusters


def exclusion(light: CompiledLight) -> str | None:
    """Why a compiled light does not become a static light, if it does not."""
    # The compiler's surface-light emitters are spots whose radius-by-distance
    # is negative; an entity spot's is (radius + 16) / distance, always
    # positive. What the negative value encodes is not established.
    if light.spot and light.spot_radius_by_distance < 0.0:
        return "surfaceEmitter"
    # Retail lights only specially flagged models with a negative light.
    if light.intensity <= 0.0:
        return "nonPositiveIntensity"
    if max(light.color) <= 0.0:
        return "black"
    if light.spot and abs(math.sqrt(sum(c * c for c in light.spot_dir)) - 1.0) > 1e-3:
        return "spotWithoutDirection"
    return None


def color_scale(light: CompiledLight) -> float:
    return min(1.0, light.cap / LIGHT_LEVEL_FULL) if light.cap > 0.0 else 1.0


def static_color(light: CompiledLight) -> tuple[float, float, float]:
    scale = color_scale(light)
    return tuple(max(0.0, component) * scale for component in light.color)


def static_radius(light: CompiledLight) -> float:
    return math.sqrt(RETAIL_ENTITY_LIGHT_SCALE * light.intensity / DLIGHT_AT_RADIUS)


def per_pixel_share(light: CompiledLight, at: Sequence[float]) -> float:
    """The fraction of its colour a light adds at a point by default."""
    reach = static_radius(light) * RENDERER_DLIGHT_SCALE
    if reach <= 0.0:
        return 0.0
    distance_squared = sum((a - b) ** 2 for a, b in zip(light.origin, at))
    x = 1.0 - distance_squared / (reach * reach)
    return x * x * (3.0 - 2.0 * x) if x > 0.0 else 0.0


def balance_colors(lights: Sequence[CompiledLight]) -> dict[int, float]:
    """Each light's colour scale, by record, so that overlapping lights cannot stack past SUPPLEMENT_PEAK."""
    scales: dict[int, float] = {}
    for light in lights:
        shares = sorted((max(static_color(other)) * per_pixel_share(other, light.origin) for other in lights),
                        reverse=True)[:RENDERER_STATIC_LIGHT_BUDGET]
        total = sum(shares)
        scales[light.record] = min(1.0, SUPPLEMENT_PEAK / total) if total > 0.0 else 1.0
    return scales


def significance(light: CompiledLight) -> float:
    # The renderers rank a visible static light by brightness times intensity.
    return max(static_color(light)) * light.intensity


def select_lights(lights: Sequence[CompiledLight], clusters: Sequence[int],
                  limit: int = MAX_STATIC_MAP_LIGHTS) -> list[CompiledLight]:
    """Up to `limit` lights, spread across the map's PVS clusters.

    Each round takes the strongest remaining light of every cluster, strongest
    first, so no area loses its main light to a brighter area's fifth.
    """
    groups: dict[int, list[CompiledLight]] = defaultdict(list)
    for light in lights:
        groups[clusters[light.leaf]].append(light)
    for group in groups.values():
        group.sort(key=lambda light: (-significance(light), light.record))
    selected: list[CompiledLight] = []
    depth = 0
    while len(selected) < limit:
        tier = [group[depth] for group in groups.values() if len(group) > depth]
        if not tier:
            break
        tier.sort(key=lambda light: (-significance(light), light.record))
        selected.extend(tier[:limit - len(selected)])
        depth += 1
    return sorted(selected, key=lambda light: light.record)


def number(value: float, places: int) -> str:
    # Fixed point only: the engine's number parser is not trusted with exponents.
    text = f"{value:.{places}f}"
    if "." in text:
        text = text.rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


def vector(values: Sequence[float], places: int) -> str:
    return "[" + ", ".join(number(value, places) for value in values) + "]"


def light_entry(name: str, light: CompiledLight, scale: float = 1.0) -> str:
    """One light's line; `scale` is its balance (see balance_colors)."""
    fields = [
        ("name", json.dumps(f"{name}-light-{light.record:04d}")),
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
        # radius-by-distance is the tangent of the cone's half angle, which is
        # what the renderers call the outer angle.
        fields.append(("outerAngle", number(math.degrees(math.atan(light.spot_radius_by_distance)), 2)))
    fields.append(("castsShadows", "true"))
    return "{" + ", ".join(f"{json.dumps(key)}: {value}" for key, value in fields) + "}"


def light_document(name: str, lights: Sequence[CompiledLight],
                   scales: dict[int, float] | None = None) -> bytes:
    header = [
        ("version", "1"),
        ("generator", json.dumps(GENERATOR)),
        ("map", json.dumps(name)),
        ("source", json.dumps("FAKK 42 lump 17, the map's compiled entity lights")),
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
        "selectedCapped": sum(1 for light in selected if light.cap > 0.0),
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
    for name in STOCK_ALICE_MAPS:
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
    for name in STOCK_ALICE_MAPS:
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
    (work / "alice-lights.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                                            encoding="utf-8")
    if any(run["status"] != "passed" for run in runs):
        raise ValueError(f"conversion failed; existing sidecars preserved; see {work / 'alice-lights.json'}")
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
                        help="Alice installation root or its base directory.")
    parser.add_argument("--work-root", type=Path, default=DEFAULT_WORK_ROOT)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT,
                        help="Root containing alice/.")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        manifest = generate_sidecars(args)
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print(f"generate_stock_alice_lights.py: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(manifest["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
