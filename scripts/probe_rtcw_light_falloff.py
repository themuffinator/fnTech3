#!/usr/bin/env python3
"""Test RTCW's light-compiler conventions against the maps' own baked lightmaps.

`scripts/generate_stock_rtcw_lights.py` converts each map's `light` entities
under q3map2's Wolf lighting model: linear by default, `0.9375 * light -
fade * d`; inverse-square `7500 * light / d^2` with spawnflag 1; spotlights
inverse-square inside a cone. The lightmaps RTCW ships were baked from those
entities, so they can confirm or refute each reading.

Every lightmap texel of a planar surface gets its world position from the
surface's draw vertices (barycentric in lightmap space). A texel is compared
with a light only when it faces the light and the segment between them
crosses no solid world brush. Patches, models and surface lights are not
modelled, so single texels are noisy; the tests compare medians over many
texels and lights, where the readings differ by far more than that noise:

- fade: lights of 140 with fade 2.5 reach 52 units if fade multiplies the
  distance, 131 without fade and 328 if it divides it;
- fade below 1: lights of 20-50 with fade 0.2 reach 94-234 units if fade
  multiplies the distance, and 19-47 without it;
- spawnflag 1: the ratio of the baked light to 7500 * light / d^2 * cos;
- spotlights: the baked light inside a cone against the same distances
  outside it.

Only the installation's paks are read, in memory.
"""
from __future__ import annotations

import argparse
import json
import math
import struct
import sys
import zipfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import generate_stock_rtcw_lights as lights
from generate_stock_rtcw_audio_zones import discover_official_maps, resolve_main_directory

LIGHTMAP = 128
CONTENTS_SOLID = 1
VERT = np.dtype([("xyz", "<f4", 3), ("st", "<f4", 2), ("lm", "<f4", 2), ("n", "<f4", 3), ("c", "u1", 4)])


def lump(data, index):
    return struct.unpack_from("<ii", data, 8 + 8 * index)


def texels(data):
    """World position, normal and brightest channel of every planar lightmap texel."""
    vo, vl = lump(data, 10)
    verts = np.frombuffer(data, dtype=VERT, count=vl // 44, offset=vo)
    io, il = lump(data, 11)
    indexes = np.frombuffer(data, dtype="<i4", count=il // 4, offset=io)
    so, sl = lump(data, 13)
    surfaces = np.frombuffer(data, dtype="<i4", count=sl // 4, offset=so).reshape(-1, 26)
    lo, ll = lump(data, 14)
    count = ll // (LIGHTMAP * LIGHTMAP * 3)
    maps = np.frombuffer(data, dtype="u1", count=count * LIGHTMAP * LIGHTMAP * 3, offset=lo)
    maps = maps.reshape(count, LIGHTMAP, LIGHTMAP, 3)
    positions, normals, values = [], [], []
    for surface in surfaces:
        kind, first, number, first_index, index_count, lightmap = surface[2:8]
        if kind != 1 or lightmap < 0:
            continue
        vertices = verts[first:first + number]
        for triangle in indexes[first_index:first_index + index_count].reshape(-1, 3):
            a, b, c = (vertices[i] for i in triangle)
            pa, pb, pc = (v["lm"].astype(np.float64) * LIGHTMAP for v in (a, b, c))
            xa, xb, xc = (v["xyz"].astype(np.float64) for v in (a, b, c))
            normal = np.cross(xb - xa, xc - xa)
            length = np.linalg.norm(normal)
            det = (pb[1] - pc[1]) * (pa[0] - pc[0]) + (pc[0] - pb[0]) * (pa[1] - pc[1])
            if length == 0 or abs(det) < 1e-9:
                continue
            u0, u1 = max(int(np.floor(min(pa[0], pb[0], pc[0]))), 0), min(int(np.ceil(max(pa[0], pb[0], pc[0]))), LIGHTMAP)
            v0, v1 = max(int(np.floor(min(pa[1], pb[1], pc[1]))), 0), min(int(np.ceil(max(pa[1], pb[1], pc[1]))), LIGHTMAP)
            uu, vv = np.meshgrid(np.arange(u0, u1), np.arange(v0, v1))
            if not uu.size:
                continue
            px, py = uu.ravel() + 0.5, vv.ravel() + 0.5
            w1 = ((pb[1] - pc[1]) * (px - pc[0]) + (pc[0] - pb[0]) * (py - pc[1])) / det
            w2 = ((pc[1] - pa[1]) * (px - pc[0]) + (pa[0] - pc[0]) * (py - pc[1])) / det
            w3 = 1 - w1 - w2
            inside = (w1 >= -1e-6) & (w2 >= -1e-6) & (w3 >= -1e-6)
            if not inside.any():
                continue
            positions.append(np.outer(w1[inside], xa) + np.outer(w2[inside], xb) + np.outer(w3[inside], xc))
            normals.append(np.repeat((normal / length)[None, :], inside.sum(), axis=0))
            values.append(maps[lightmap, vv.ravel()[inside], uu.ravel()[inside]].max(axis=1))
    return np.concatenate(positions), np.concatenate(normals), np.concatenate(values).astype(np.float64)


class Brushes:
    """Segment tests against the world model's solid brushes."""

    def __init__(self, data):
        so, sl = lump(data, 1)
        contents = [struct.unpack_from("<i", data, so + i * 72 + 68)[0] for i in range(sl // 72)]
        po, pl = lump(data, 2)
        planes = np.frombuffer(data, dtype="<f4", count=pl // 4, offset=po).reshape(-1, 4).astype(np.float64)
        mo, _ = lump(data, 7)
        first_brush, brush_count = struct.unpack_from("<ii", data, mo + 32)
        bo, bl = lump(data, 8)
        brushes = np.frombuffer(data, dtype="<i4", count=bl // 4, offset=bo).reshape(-1, 3)
        do, dl = lump(data, 9)
        sides = np.frombuffer(data, dtype="<i4", count=dl // 4, offset=do).reshape(-1, 2)
        kept, mins, maxs = [], [], []
        for first, count, shader in brushes[first_brush:first_brush + brush_count]:
            if not contents[shader] & CONTENTS_SOLID:
                continue
            p = planes[sides[first:first + count, 0]]
            lo, hi = np.full(3, -1e9), np.full(3, 1e9)
            for nx, ny, nz, dist in p:
                normal = (nx, ny, nz)
                if sum(abs(v) for v in normal) != 1.0:
                    continue
                for axis in range(3):
                    if normal[axis] == 1.0:
                        hi[axis] = min(hi[axis], dist)
                    elif normal[axis] == -1.0:
                        lo[axis] = max(lo[axis], -dist)
            kept.append(p)
            mins.append(lo)
            maxs.append(hi)
        self.mins, self.maxs = np.array(mins), np.array(maxs)
        self.planes = np.zeros((len(kept), max(len(p) for p in kept), 4))
        self.planes[:, :, 3] = 1e12  # padding: every point is behind it
        for index, p in enumerate(kept):
            self.planes[index, :len(p)] = p

    def visible(self, starts, end):
        blocked = np.zeros(len(starts), dtype=bool)
        if not len(starts):
            return ~blocked
        smin, smax = np.minimum(starts, end), np.maximum(starts, end)
        near = np.nonzero(np.all(self.maxs >= smin.min(axis=0), axis=1) & np.all(self.mins <= smax.max(axis=0), axis=1))[0]
        if not len(near):
            return ~blocked
        step = max(1, 200000 // len(near))
        for s0 in range(0, len(starts), step):
            s1 = min(len(starts), s0 + step)
            overlap = (np.all(smax[s0:s1, None, :] >= self.mins[near][None], axis=2) &
                       np.all(smin[s0:s1, None, :] <= self.maxs[near][None], axis=2))
            ti, bi = np.nonzero(overlap)
            if not len(ti):
                continue
            ti += s0
            planes = self.planes[near[bi]]
            ds = np.einsum("pk,psk->ps", starts[ti], planes[:, :, :3]) - planes[:, :, 3]
            de = planes[:, :, :3] @ end - planes[:, :, 3]
            crossing = (ds > 0) != (de > 0)
            with np.errstate(divide="ignore", invalid="ignore"):
                t = np.where(crossing, ds / (ds - de), 0.0)
            enter = np.where(crossing & (ds > 0), t, 0.0).max(axis=1)
            leave = np.where(crossing & (ds <= 0), t, 1.0).min(axis=1)
            hit = ~((ds > 0) & (de > 0)).any(axis=1) & (enter < leave)
            blocked[ti[hit]] = True
        return ~blocked


class MapData:
    def __init__(self, maps, profile, name):
        data, _ = lights.read_map(*maps[profile, name])
        offset, length = lump(data, 0)
        entities = lights.parse_entities(data[offset:offset + length].split(b"\0", 1)[0].decode("latin-1"))
        self.lights, _, _ = lights.read_lights(entities)
        self.ambient = lights.atof(dict(entities[0]).get("_ambient")) or lights.atof(dict(entities[0]).get("ambient"))
        self.positions, self.normals, self.values = texels(data)
        self.brushes = Brushes(data)

    def seen(self, light, low, high):
        """Texels between low and high units that face the light and see it."""
        origin = np.array(light.origin)
        offset = origin[None, :] - self.positions
        distance = np.linalg.norm(offset, axis=1)
        index = np.nonzero((distance >= low) & (distance < high))[0]
        cos = np.einsum("ij,ij->i", self.normals[index], offset[index] / distance[index][:, None])
        index, cos = index[cos > 0.2], cos[cos > 0.2]
        keep = self.brushes.visible(self.positions[index] + self.normals[index], origin)
        return index[keep], distance[index[keep]], cos[keep]


def band_medians(data, light, bands):
    out = []
    for low, high in bands:
        index, _, _ = data.seen(light, low, high)
        out.append(round(float(np.median(data.values[index] - data.ambient)), 1) if len(index) >= 5 else None)
    return out


def fade_test(maps):
    """Lights of 140 with fade 2.5: baked light 50-200 units away."""
    data = MapData(maps, "rtcw-mp", "mp_keep")
    chosen = [light for light in data.lights if light.linear and light.intensity == 140 and light.fade == 2.5]
    bands = ((50, 100), (100, 200))
    rows = []
    for light in chosen:
        medians = band_medians(data, light, bands)
        rows.append(dict(entity=light.entity, bakedAboveAmbient=medians))
    peak = 0.9375 * 140
    predictions = {
        "d*fade (reach 52)": [0.0, 0.0],
        "no fade (reach 131)": [round(max(0.0, peak - 75), 1), round(max(0.0, peak - 150), 1)],
        "d/fade (reach 328)": [round(peak - 75 / 2.5, 1), round(peak - 150 / 2.5, 1)],
    }
    measured = [[row["bakedAboveAmbient"][i] for row in rows if row["bakedAboveAmbient"][i] is not None]
                for i in range(len(bands))]
    return dict(map="rtcw-mp/mp_keep", lights=len(chosen), bands=bands, ambient=data.ambient,
                predictionsAtBandMiddles=predictions,
                medianOfLightMedians=[round(float(np.median(values)), 1) for values in measured],
                lightsMeasured=[len(values) for values in measured], rows=rows)


def small_fade_test(maps, specs):
    """Lights of 20-50 with fade 0.2: baked light between the two readings' reaches."""
    rows = []
    for profile, name in specs:
        data = MapData(maps, profile, name)
        for light in data.lights:
            if not (light.linear and 20 <= light.intensity <= 50 and light.fade == 0.2 and not light.flags & 16):
                continue
            no_fade = 0.9375 * light.intensity
            inner = (no_fade + 8, 0.5 * no_fade / 0.2)
            outer = (1.3 * no_fade / 0.2, 2.0 * no_fade / 0.2)
            if inner[1] - inner[0] < 16:
                continue
            medians = band_medians(data, light, (inner, outer))
            if None in medians:
                continue
            rows.append(dict(map=f"{profile}/{name}", entity=light.entity, intensity=light.intensity,
                             inner=[round(v) for v in inner], outer=[round(v) for v in outer],
                             bakedAboveAmbient=medians))
    inner = np.array([row["bakedAboveAmbient"][0] for row in rows])
    outer = np.array([row["bakedAboveAmbient"][1] for row in rows])
    return dict(lights=len(rows),
                innerBand="from 8 units past the no-fade reach to half the d*fade reach",
                outerBand="1.3 to 2 times the d*fade reach",
                medianInner=round(float(np.median(inner)), 1), medianOuter=round(float(np.median(outer)), 1),
                innerBrighterThanOuter=int((inner > outer).sum()), rows=rows)


def inverse_square_test(maps, specs):
    """Spawnflag 1: baked light over 7500 * light / d^2 * cos, where that is 6-100."""
    per_light = []
    for profile, name in specs:
        data = MapData(maps, profile, name)
        for light in data.lights:
            if light.kind != "inverseSquare" or light.flags & 12:
                continue
            reach = math.sqrt(7500 * light.intensity / 6)
            index, distance, cos = data.seen(light, 24, min(reach, 2048))
            predicted = 7500 * light.intensity / np.maximum(distance, 16) ** 2 * cos * max(light.color)
            keep = (predicted >= 6) & (predicted <= 100)
            if keep.sum() < 10:
                continue
            ratio = (data.values[index][keep] - data.ambient) / predicted[keep]
            per_light.append(float(np.median(ratio)))
    values = np.array(per_light)
    return dict(lights=len(values), medianRatio=round(float(np.median(values)), 3),
                quartiles=[round(float(np.percentile(values, q)), 3) for q in (25, 75)],
                withinFactorTwo=int(((values > 0.5) & (values < 2)).sum()))


def spot_cone_test(maps, specs):
    """Spotlights: baked light inside the cone against outside it, 64-256 units away."""
    rows = []
    for profile, name in specs:
        data = MapData(maps, profile, name)
        for light in data.lights:
            if not light.spot or light.flags & 12 or not any(light.spot_direction):
                continue
            index, distance, _ = data.seen(light, 64, 256)
            if len(index) < 20:
                continue
            axis = np.array(light.spot_direction)
            offset = data.positions[index] - np.array(light.origin)[None, :]
            along = offset @ axis
            radius = np.linalg.norm(offset - along[:, None] * axis[None, :], axis=1)
            edge = light.spot_radius_by_distance * along
            inside = (along > 0) & (radius < edge - 32)
            outside = (along <= 0) | (radius > edge + 32)
            if inside.sum() < 5 or outside.sum() < 5:
                continue
            baked = data.values[index] - data.ambient
            rows.append(dict(map=f"{profile}/{name}", entity=light.entity, inside=float(np.median(baked[inside])),
                             outside=float(np.median(baked[outside]))))
    inside = np.array([row["inside"] for row in rows])
    outside = np.array([row["outside"] for row in rows])
    return dict(lights=len(rows), medianInside=round(float(np.median(inside)), 1),
                medianOutside=round(float(np.median(outside)), 1),
                insideBrighter=int((inside > outside).sum()))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("retail_path", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.relative_to(ROOT / ".tmp")
    maps = discover_official_maps(resolve_main_directory(args.retail_path))
    report = dict(scope=__doc__)
    report["fadeMultipliesDistance"] = fade_test(maps)
    print("fade", json.dumps({k: v for k, v in report["fadeMultipliesDistance"].items() if k != "rows"}), flush=True)
    report["fadeBelowOne"] = small_fade_test(maps, [("rtcw-sp", "escape1"), ("rtcw-mp", "mp_destruction")])
    print("small fade", json.dumps({k: v for k, v in report["fadeBelowOne"].items() if k != "rows"}), flush=True)
    report["inverseSquare"] = inverse_square_test(maps, [("rtcw-sp", "dig"), ("rtcw-sp", "boss2"), ("rtcw-mp", "mp_base")])
    print("inverse square", json.dumps(report["inverseSquare"]), flush=True)
    report["spotCone"] = spot_cone_test(maps, [("rtcw-mp", "mp_depot"), ("rtcw-sp", "village2"), ("rtcw-mp", "mp_trenchtoast")])
    print("spot cone", json.dumps(report["spotCone"]), flush=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=1) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
