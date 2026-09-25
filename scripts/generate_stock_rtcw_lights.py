"""Generate a static light file for every official RTCW map, for its own mode.

Each map's `maps/<name>.lights.json` feeds the renderers' static map lights
(`r_staticLights`), which promote the nearest visible ones into per-pixel
dynamic lights and shadow the strongest. RTCW's IBSP 47 maps keep their
compiled `light` entities in the entity lump, so the lights are read from
there and converted under the light compiler's own conventions:

- the entity keys and spawnflags are the ones RTCW's game source documents for
  `light` (its `QUAKED` comment in g_misc.c): `light` (default 300),
  `_color`, `target` and `radius` for a spotlight, `fade` for a linear light,
  and flags nonlinear (1), angle (2), negative spot (4), negative point (8)
  and q3map_non-dynamic (16);
- the falloff is the one q3map2's Wolf lighting model implements for RTCW:
  a light is linear by default, `0.9375 * light - fade * d`, which reaches
  zero at `0.9375 * light / fade`; spawnflag 1 makes it inverse-square,
  `7500 * light / d^2`; a light aimed at a target is an inverse-square
  spotlight whose cone's radius at a distance d is `(radius + 16) / dist * d`.

Each light's radius and brightness are the least-squares fit of the
renderers' own falloff -- a smoothstep of `1 - d^2 / r^2`, drawn along a
segment for a spot -- to the curve retail baked, in lightmap levels as RTCW
draws them, weighted by the area each distance covers. The engine's per-pixel
lighting, shadows and budgets then present it. Lights RTCW's cgame already
draws (`dlight` entities, and a `light` at one of them), grid-only
`lightJunior` lights and lights retail keeps off models are left out. The
installation is only read from, and the files are published only once every
map has converted.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
import sys
import tempfile
import zipfile
from collections import Counter, defaultdict
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
from typing import Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))

from generate_stock_rtcw_audio_zones import discover_official_maps, resolve_main_directory
from stock_rtcw_maps import STOCK_RTCW_MAPS

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK_ROOT = ROOT / ".tmp" / "rtcw-lights"
DEFAULT_OUTPUT_ROOT = ROOT / "pkg" / "games"
GAME_DIRECTORY = "main"
GENERATOR = "fnTech3 scripts/generate_stock_rtcw_lights.py"

IBSP_IDENT = b"IBSP"
IBSP_VERSION_RTCW = 47
IBSP_LUMPS = 17
IBSP_HEADER_BYTES = 8 + IBSP_LUMPS * 8
LUMP_ENTITIES, LUMP_PLANES, LUMP_NODES, LUMP_LEAFS = 0, 2, 3, 4
PLANE = struct.Struct("<4f")
NODE = struct.Struct("<3i6i")
LEAF_BYTES = 48

# The `light` spawnflags RTCW's game source documents (g_misc.c, QUAKED light).
FLAG_NONLINEAR = 1
FLAG_ANGLE = 2
FLAG_NEGATIVE_SPOT = 4
FLAG_NEGATIVE_POINT = 8
FLAG_NON_DYNAMIC = 16

# The light compiler's constants (q3map, and q3map2's Wolf model): a light's
# photons are its value times 7500; a linear light gives photons / 8000 less
# `fade` per unit of distance; every distance is at least 16; a spotlight's
# cone has `radius` (default 64) plus 16 at its target.
DEFAULT_INTENSITY = 300.0
POINT_SCALE = 7500.0
LINEAR_SCALE = 1.0 / 8000.0
MIN_DISTANCE = 16.0
SPOT_DEFAULT_RADIUS = 64.0
SPOT_DEFAULT_DISTANCE = 64.0
SPOT_RADIUS_PAD = 16.0

# The renderers keep this many static lights per map and skip the rest
# (MAX_STATIC_MAP_LIGHTS in every renderer's tr_local.h), and clamp a light's
# radius to this (their static light parsers).
MAX_STATIC_MAP_LIGHTS = 128
MAX_RADIUS = 8192.0
# r_dlightScale's default in each mode (every renderer's tr_init.c); it scales
# a static light's radius as it does every dynamic light's.
DLIGHT_SCALE = {"rtcw-sp": 1.0, "rtcw-mp": 0.5}
# RTCW draws its maps with r_mapOverBrightBits 2 over r_overBrightBits 1, so a
# lightmap value is shifted left once: 128 lights a surface as a dynamic light
# of colour 1 does, and anything brighter is clamped.
LIGHTMAP_UNIT = 128.0
# Retail's curve is fitted out to where it gives one lightmap level; a light
# that never gives one is left out.
FAINTEST_LEVEL = 1.0
# The fit samples retail's curve this often, and tries drawn reaches over this
# span of retail's extent, coarsely and then finely around the best.
FIT_SAMPLES = 200
FIT_COARSE = [0.02 * step for step in range(1, 151)]
FIT_FINE = [0.0005 * step for step in range(-40, 41)]
# A `light` this close to a `dlight` entity is that fixture's baked light: the
# cgame already draws a light there, 300 units across at its style's middle
# value (cg_ents.c CG_Dlight), usually in the same colour.
CGAME_LIGHT_DISTANCE = 64.0

NUMBER = re.compile(r"\s*[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?")


def atof(text: str | None) -> float:
    """C's atof: the leading number, or 0 (so `,25` is 0, as the compiler reads it)."""
    match = NUMBER.match(text or "")
    value = float(match.group(0)) if match else 0.0
    return value if math.isfinite(value) else 0.0


def atoi(text: str | None) -> int:
    match = re.match(r"\s*[+-]?\d+", text or "")
    return int(match.group(0)) if match else 0


def vector(text: str | None) -> tuple[float, float, float]:
    """Three numbers as sscanf("%f %f %f") reads them; missing ones are 0."""
    values = [atof(part) for part in (text or "").split()[:3]]
    return tuple(values + [0.0] * (3 - len(values)))  # type: ignore[return-value]


@dataclass(frozen=True)
class Light:
    entity: int
    origin: tuple[float, float, float]
    intensity: float
    color: tuple[float, float, float]
    flags: int
    fade: float
    spot_direction: tuple[float, float, float] | None
    spot_radius_by_distance: float
    styled: bool
    target_missing: bool
    sun: bool = False

    @property
    def spot(self) -> bool:
        return self.spot_direction is not None

    @property
    def linear(self) -> bool:
        # Spotlights are always inverse-square under the Wolf model.
        return not self.spot and not self.flags & FLAG_NONLINEAR

    @property
    def kind(self) -> str:
        return "spot" if self.spot else "linear" if self.linear else "inverseSquare"


def ibsp_lumps(data: bytes) -> list[tuple[int, int]]:
    if len(data) < IBSP_HEADER_BYTES:
        raise ValueError("truncated IBSP header")
    if data[:4] != IBSP_IDENT:
        raise ValueError("not an IBSP map")
    version = struct.unpack_from("<i", data, 4)[0]
    if version != IBSP_VERSION_RTCW:
        raise ValueError(f"expected RTCW's IBSP 47, got version {version}")
    values = struct.unpack_from(f"<{IBSP_LUMPS * 2}i", data, 8)
    lumps = [(values[i], values[i + 1]) for i in range(0, len(values), 2)]
    for offset, length in lumps:
        if length < 0 or (length and (offset < IBSP_HEADER_BYTES or offset > len(data) - length)):
            raise ValueError("IBSP lump range is outside the file")
    return lumps


def parse_entities(text: str) -> list[list[tuple[str, str]]]:
    """The entity lump's key/value blocks, refusing anything malformed."""
    entities: list[list[tuple[str, str]]] = []
    position, end = 0, len(text)
    current: list[tuple[str, str]] | None = None
    pending: str | None = None
    while True:
        while position < end and text[position] in " \t\r\n":
            position += 1
        if position >= end:
            break
        char = text[position]
        if char == "{":
            if current is not None:
                raise ValueError("nested entity block")
            current = []
            position += 1
        elif char == "}":
            if current is None or pending is not None:
                raise ValueError("unbalanced entity block")
            entities.append(current)
            current = None
            position += 1
        elif char == '"':
            if current is None:
                raise ValueError("entity key outside a block")
            close = text.find('"', position + 1)
            if close < 0:
                raise ValueError("unterminated entity string")
            token = text[position + 1:close]
            if "\n" in token:
                raise ValueError("entity string spans a line")
            position = close + 1
            if pending is None:
                pending = token
            else:
                current.append((pending, token))
                pending = None
        else:
            raise ValueError(f"unexpected {char!r} in the entity lump")
    if current is not None:
        raise ValueError("unterminated entity block")
    if not entities:
        raise ValueError("the entity lump holds no entity")
    return entities


class ClusterIndex:
    """Point to PVS cluster through the map's BSP tree, as the renderers walk it."""

    def __init__(self, data: bytes, lumps: Sequence[tuple[int, int]]) -> None:
        plane_offset, plane_length = lumps[LUMP_PLANES]
        node_offset, node_length = lumps[LUMP_NODES]
        leaf_offset, leaf_length = lumps[LUMP_LEAFS]
        for name, length, size in (("plane", plane_length, PLANE.size), ("node", node_length, NODE.size),
                                   ("leaf", leaf_length, LEAF_BYTES)):
            if length % size:
                raise ValueError(f"IBSP {name} lump is not a whole number of records")
        self.planes = [PLANE.unpack_from(data, plane_offset + i * PLANE.size) for i in range(plane_length // PLANE.size)]
        self.nodes = [NODE.unpack_from(data, node_offset + i * NODE.size)[:3] for i in range(node_length // NODE.size)]
        self.clusters = [struct.unpack_from("<i", data, leaf_offset + i * LEAF_BYTES)[0]
                         for i in range(leaf_length // LEAF_BYTES)]
        if not self.nodes or not self.clusters:
            raise ValueError("IBSP map has no BSP tree")
        for plane in self.planes:
            if not all(math.isfinite(value) for value in plane):
                raise ValueError("IBSP plane is not finite")
        for plane, front, back in self.nodes:
            if not 0 <= plane < len(self.planes):
                raise ValueError("IBSP node names a plane out of range")
            for child in (front, back):
                if not (0 <= child < len(self.nodes) or 0 <= -1 - child < len(self.clusters)):
                    raise ValueError("IBSP node names a child out of range")

    def cluster(self, point: Sequence[float]) -> int:
        node = 0
        for _ in range(len(self.nodes) + 1):
            plane, front, back = self.nodes[node]
            nx, ny, nz, dist = self.planes[plane]
            node = front if point[0] * nx + point[1] * ny + point[2] * nz - dist > 0 else back
            if node < 0:
                return self.clusters[-1 - node]
        raise ValueError("IBSP tree does not end in a leaf")


def read_lights(entities: Sequence[Sequence[tuple[str, str]]]) -> tuple[list[Light], dict[str, int], list]:
    """Every light the compiler lit the world with, and the entity classes around it."""
    classes: Counter[str] = Counter()
    targets: dict[str, tuple[float, float, float]] = {}
    for pairs in entities:
        values = dict(pairs)
        classes[values.get("classname", "")] += 1
        # The compiler aims a spotlight at the first entity with the name.
        name = values.get("targetname")
        if name is not None and name not in targets:
            targets[name] = vector(values.get("origin"))
    lights: list[Light] = []
    cgame_lights = []
    for index, pairs in enumerate(entities):
        keys = [key for key, _ in pairs]
        values = dict(pairs)
        classname = values.get("classname", "")
        if classname == "dlight":
            cgame_lights.append(vector(values.get("origin")))
        # q3map2 takes every class named light*, lightJunior* as grid-only.
        if not classname.lower().startswith("light") or classname.lower().startswith("lightjunior"):
            continue
        if len(keys) != len(set(keys)):
            raise ValueError(f"light entity {index} repeats a key")
        origin = vector(values.get("origin"))
        intensity = atof(values.get("_light")) or atof(values.get("light")) or DEFAULT_INTENSITY
        raw_color = vector(values.get("_color")) if values.get("_color") else (1.0, 1.0, 1.0)
        peak = max(raw_color)
        color = tuple(c / peak for c in raw_color) if peak > 0 else (1.0, 1.0, 1.0)
        flags = atoi(values.get("spawnflags"))
        fade = atof(values.get("fade")) or 1.0
        direction = None
        radius_by_distance = 0.0
        target = values.get("target", "")
        target_missing = bool(target) and target not in targets
        if target and not target_missing:
            delta = tuple(t - o for t, o in zip(targets[target], origin))
            distance = math.sqrt(sum(d * d for d in delta))
            direction = tuple(d / distance for d in delta) if distance > 0 else (0.0, 0.0, 0.0)
            radius = atof(values.get("radius")) or SPOT_DEFAULT_RADIUS
            radius_by_distance = (radius + SPOT_RADIUS_PAD) / (distance or SPOT_DEFAULT_DISTANCE)
            fade = 1.0
        if not all(math.isfinite(v) for v in (*origin, intensity, *color, fade, radius_by_distance)):
            raise ValueError(f"light entity {index} is not finite")
        styled = bool(atoi(values.get("_style")) or atoi(values.get("style")))
        # A targeted light with _sun 1 becomes the compiler's sun, not a light.
        sun = direction is not None and values.get("_sun", "").startswith("1")
        lights.append(Light(index, origin, intensity, color, flags, fade, direction, radius_by_distance,
                            styled, target_missing, sun))
    return lights, dict(sorted(classes.items())), cgame_lights


def exclusion(light: Light, cluster: int, cgame_lights: Sequence[Sequence[float]]) -> str | None:
    """Why a compiled light does not become a static light, if it does not."""
    if light.sun:
        return "sun"
    if light.flags & (FLAG_NEGATIVE_SPOT | FLAG_NEGATIVE_POINT):
        return "negative"
    # Kept out of the light grid, so retail lights no model with it.
    if light.flags & FLAG_NON_DYNAMIC:
        return "nonDynamic"
    # A switchable light style; what the compiler baked for one is not established.
    if light.styled:
        return "styled"
    if light.intensity <= 0.0:
        return "nonPositiveIntensity"
    if light.spot and math.sqrt(sum(d * d for d in light.spot_direction)) < 0.5:
        return "spotWithoutDirection"
    if cluster < 0:
        return "outsideClusters"
    for origin in cgame_lights:
        if math.dist(origin, light.origin) <= CGAME_LIGHT_DISTANCE:
            return "atCgameLight"
    if retail_level(light.linear, light.intensity, light.fade, 0.0) < FAINTEST_LEVEL:
        return "unlit"
    return None


def retail_level(linear: bool, intensity: float, fade: float, distance: float) -> float:
    """What retail baked a distance from a light, on a surface facing it, in lightmap
    levels as RTCW draws them (clamped at LIGHTMAP_UNIT)."""
    distance = max(distance, MIN_DISTANCE)
    photons = intensity * POINT_SCALE
    level = photons * LINEAR_SCALE - fade * distance if linear else photons / (distance * distance)
    return min(LIGHTMAP_UNIT, max(0.0, level))


def retail_extent(linear: bool, intensity: float, fade: float) -> float:
    """Where retail's light ends: a linear light's zero, an inverse-square one's faintest level."""
    photons = intensity * POINT_SCALE
    if linear:
        return max(MIN_DISTANCE, photons * LINEAR_SCALE / fade)
    return max(MIN_DISTANCE, math.sqrt(photons / FAINTEST_LEVEL))


def smoothstep(value: float) -> float:
    value = min(1.0, max(0.0, value))
    return value * value * (3.0 - 2.0 * value)


def drawn_falloff(distance: float, radius: float, spot: bool, scale: float) -> float:
    """The renderers' per-pixel falloff at a distance from a light (along a spot's
    direction), for a file radius drawn at a mode's r_dlightScale.

    A point light is lit to its scaled radius. A spot is drawn as a segment of
    the file radius along its direction, lit to the scaled radius around it.
    """
    beyond = max(0.0, distance - radius) if spot else distance
    return smoothstep(1.0 - (beyond / (radius * scale)) ** 2)


@lru_cache(maxsize=None)
def fitted(linear: bool, intensity: float, fade: float, spot: bool, scale: float) -> tuple[float, float]:
    """The brightness (0-1) and file radius whose drawn falloff best matches retail's.

    Least squares over retail's extent, each distance weighted by the area it
    covers on a surface through the light; the brightness has a closed form for
    each reach tried. A spot is fitted along its direction.
    """
    extent = retail_extent(linear, intensity, fade)
    distances = [extent * (index + 0.5) / FIT_SAMPLES for index in range(FIT_SAMPLES)]
    wanted = [retail_level(linear, intensity, fade, distance) / LIGHTMAP_UNIT for distance in distances]

    def score(fraction: float):
        # The drawn reach: a point's scaled radius, a spot's segment and scaled radius.
        radius = extent * fraction / ((1.0 + scale) if spot else scale)
        drawn = [drawn_falloff(distance, radius, spot, scale) for distance in distances]
        norm = sum(d * f * f for d, f in zip(distances, drawn))
        if norm <= 0.0:
            return None
        brightness = min(1.0, max(0.0, sum(d * f * w for d, f, w in zip(distances, drawn, wanted)) / norm))
        error = sum(d * (w - brightness * f) ** 2 for d, f, w in zip(distances, drawn, wanted))
        return error, fraction, brightness, radius

    coarse = min(filter(None, (score(fraction) for fraction in FIT_COARSE)))
    fine = min(filter(None, (score(coarse[1] + step) for step in FIT_FINE if coarse[1] + step > 0.0)))
    return fine[2], min(MAX_RADIUS, fine[3])


def fit(light: Light, profile: str) -> tuple[float, float]:
    return fitted(light.linear, light.intensity, light.fade, light.spot, DLIGHT_SCALE[profile])


def brightness(light: Light, profile: str) -> float:
    return fit(light, profile)[0]


def file_radius(light: Light, profile: str) -> float:
    return fit(light, profile)[1]


def drawn_reach(light: Light, profile: str) -> float:
    scale = DLIGHT_SCALE[profile]
    return file_radius(light, profile) * ((1.0 + scale) if light.spot else scale)


def static_color(light: Light, profile: str) -> tuple[float, float, float]:
    scale = brightness(light, profile)
    return tuple(component * scale for component in light.color)  # type: ignore[return-value]


def significance(light: Light, profile: str) -> float:
    # The renderers rank a visible static light by brightness times intensity.
    return brightness(light, profile) * drawn_reach(light, profile)


def outer_angle(light: Light) -> float:
    # radius-by-distance is the tangent of the cone's half angle, which is what
    # the renderers call the outer angle.
    return math.degrees(math.atan(light.spot_radius_by_distance))


def select_lights(lights: Sequence[Light], clusters: dict[int, int], profile: str,
                  limit: int = MAX_STATIC_MAP_LIGHTS) -> list[Light]:
    """Up to `limit` lights, spread across the map's PVS clusters.

    Each round takes the strongest remaining light of every cluster, strongest
    first, so no area loses its main light to a brighter area's fifth.
    """
    groups: dict[int, list[Light]] = defaultdict(list)
    for light in lights:
        groups[clusters[light.entity]].append(light)
    order = lambda light: (-significance(light, profile), light.entity)
    for group in groups.values():
        group.sort(key=order)
    selected: list[Light] = []
    depth = 0
    while len(selected) < limit:
        tier = [group[depth] for group in groups.values() if len(group) > depth]
        if not tier:
            break
        tier.sort(key=order)
        selected.extend(tier[:limit - len(selected)])
        depth += 1
    return sorted(selected, key=lambda light: light.entity)


def number(value: float, places: int) -> str:
    # Fixed point only: the engine's number parser is not trusted with exponents.
    text = f"{value:.{places}f}"
    if "." in text:
        text = text.rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


def vector_text(values: Sequence[float], places: int) -> str:
    return "[" + ", ".join(number(value, places) for value in values) + "]"


def light_entry(name: str, light: Light, profile: str) -> str:
    radius = number(file_radius(light, profile), 1)
    fields = [
        ("name", json.dumps(f"{name}-light-{light.entity:04d}")),
        ("type", json.dumps("spot" if light.spot else "point")),
        ("origin", vector_text(light.origin, 3)),
    ]
    if light.spot:
        fields.append(("direction", vector_text(light.spot_direction, 5)))
    fields += [
        ("color", vector_text(static_color(light, profile), 4)),
        # Equal to the radius, the parser's own default: every light's shadow
        # priority is then neutral in both modes.
        ("intensity", radius),
        ("radius", radius),
    ]
    if light.spot:
        fields.append(("outerAngle", number(outer_angle(light), 2)))
    fields.append(("castsShadows", "true"))
    return "{" + ", ".join(f"{json.dumps(key)}: {value}" for key, value in fields) + "}"


def light_document(name: str, profile: str, lights: Sequence[Light]) -> bytes:
    header = [
        ("version", "1"),
        ("generator", json.dumps(GENERATOR)),
        ("map", json.dumps(name)),
        ("profile", json.dumps(profile)),
        ("source", json.dumps("IBSP 47 entity lump, the map's compiled light entities")),
        ("dlightScale", number(DLIGHT_SCALE[profile], 2)),
    ]
    lines = ["{"] + [f"  {json.dumps(key)}: {value}," for key, value in header]
    entries = [light_entry(name, light, profile) for light in lights]
    if entries:
        lines.append('  "lights": [')
        lines += [f"    {entry}{',' if index + 1 < len(entries) else ''}" for index, entry in enumerate(entries)]
        lines.append("  ]")
    else:
        lines.append('  "lights": []')
    lines.append("}")
    return ("\n".join(lines) + "\n").encode("ascii")


def convert_map(name: str, profile: str, data: bytes) -> tuple[bytes, dict[str, object]]:
    lumps = ibsp_lumps(data)
    offset, length = lumps[LUMP_ENTITIES]
    text = data[offset:offset + length].split(b"\0", 1)[0].decode("latin-1")
    entities = parse_entities(text)
    lights, classes, cgame_lights = read_lights(entities)
    index = ClusterIndex(data, lumps)
    clusters = {light.entity: index.cluster(light.origin) for light in lights}
    excluded: Counter[str] = Counter()
    usable = []
    for light in lights:
        reason = exclusion(light, clusters[light.entity], cgame_lights)
        if reason:
            excluded[reason] += 1
        else:
            usable.append(light)
    selected = select_lights(usable, clusters, profile)
    payload = light_document(name, profile, selected)
    summary = {
        "classes": {key: classes[key] for key in ("light", "lightJunior", "dlight", "corona") if key in classes},
        "compiledLights": len(lights),
        "excluded": dict(sorted(excluded.items())),
        "usable": len(usable),
        "usableKinds": dict(sorted(Counter(light.kind for light in usable).items())),
        "usableClusters": len({clusters[light.entity] for light in usable}),
        "missingTargets": sum(1 for light in lights if light.target_missing),
        "selected": len(selected),
        "selectedKinds": dict(sorted(Counter(light.kind for light in selected).items())),
        "selectedClusters": len({clusters[light.entity] for light in selected}),
        "dropped": len(usable) - len(selected),
    }
    return payload, summary


def read_map(pak: Path, entry: str) -> tuple[bytes, int]:
    with zipfile.ZipFile(pak) as archive:
        info = archive.getinfo(entry)
        return archive.read(info), info.CRC


def generate_sidecars(args: argparse.Namespace) -> dict[str, object]:
    main = resolve_main_directory(args.retail_path)
    maps = discover_official_maps(main)
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
            destination = output / profile / GAME_DIRECTORY / "maps" / f"{name}.lights.json"
            if not destination.resolve().is_relative_to(output):
                raise ValueError(f"sidecar destination escapes --output-root: {destination}")
    work.mkdir(parents=True, exist_ok=True)
    runs: list[dict[str, object]] = []
    manifest: dict[str, object] = {
        "generator": GENERATOR,
        "installation": str(main.parent),
        "outputRoot": str(output),
        "runs": runs,
    }
    staged: dict[Path, bytes] = {}
    for profile, names in STOCK_RTCW_MAPS.items():
        for name in names:
            pak, entry = maps[profile, name]
            data, crc = read_map(pak, entry)
            relative = Path(profile) / GAME_DIRECTORY / "maps" / f"{name}.lights.json"
            run: dict[str, object] = {
                "map": name, "profile": profile, "output": relative.as_posix(), "pak": pak.name,
                "entry": entry, "bspBytes": len(data), "bspCrc32": f"{crc:08x}",
                "bspSha256": hashlib.sha256(data).hexdigest(), "status": "failed",
            }
            try:
                payload, summary = convert_map(name, profile, data)
            except ValueError as exc:
                run["error"] = str(exc)
            else:
                staged[relative] = payload
                run.update(summary, status="passed", lightsBytes=len(payload),
                           lightsSha256=hashlib.sha256(payload).hexdigest())
            runs.append(run)
            print(f"{profile}/{name}: {run['status']}", flush=True)
    manifest["summary"] = {
        "maps": len(runs),
        "passed": sum(1 for run in runs if run["status"] == "passed"),
        "selected": sum(int(run.get("selected", 0)) for run in runs),
    }
    (work / "rtcw-lights.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if any(run["status"] != "passed" for run in runs):
        raise ValueError(f"conversion failed; existing sidecars preserved; see {work / 'rtcw-lights.json'}")
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
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("retail_path", type=Path, help="RTCW installation root or its Main directory.")
    parser.add_argument("--work-root", type=Path, default=DEFAULT_WORK_ROOT)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT,
                        help="Root containing rtcw-sp/ and rtcw-mp/.")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        manifest = generate_sidecars(args)
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print(f"generate_stock_rtcw_lights.py: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(manifest["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
