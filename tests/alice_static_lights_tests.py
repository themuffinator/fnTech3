"""American McGee's Alice's static light files, from its maps' compiled lights.

Synthetic FAKK 42 maps only; no retail map is read. The shipped files are
checked against their generation record and the renderers' parser limits.
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
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import alice_retail_maps
import generate_stock_alice_lights as lights
from stock_alice_maps import STOCK_ALICE_MAPS

LEAF_LUMP = 8
LIGHT_LUMP = 17


def light_record(origin=(0.0, 0.0, 0.0), color=(1.0, 1.0, 1.0), intensity=300.0, leaf=0,
                 needs_trace=0, spot=0, spot_dir=(0.0, 0.0, 0.0), radius_by_distance=0.0,
                 cap=0.0) -> bytes:
    return struct.pack("<3f3ffiii3fff", *origin, *color, intensity, leaf, needs_trace, spot,
                       *spot_dir, radius_by_distance, cap)


def fakk_map(records: list[bytes], clusters=(0,), *, version: int = 42) -> bytes:
    leafs = b"".join(struct.pack("<12i", cluster, 0, 0, 0, 0, 64, 64, 64, 0, 0, 0, 0)
                     for cluster in clusters)
    lumps = {LEAF_LUMP: leafs, LIGHT_LUMP: b"".join(records)}
    header = bytearray(b"FAKK" + struct.pack("<ii", version, 0))
    body = bytearray()
    offset = lights.FAKK_HEADER_BYTES
    for index in range(lights.FAKK_HEADER_LUMPS):
        payload = lumps.get(index, b"")
        header += struct.pack("<ii", offset, len(payload))
        body += payload
        offset += len(payload)
    return bytes(header + body)


def decode(payload: bytes) -> dict:
    def refuse_duplicates(pairs):
        keys = [key for key, _ in pairs]
        assert len(keys) == len(set(keys)), f"duplicate key in {keys}"
        return dict(pairs)
    return json.loads(payload, object_pairs_hook=refuse_duplicates)


class CompiledLightTests(unittest.TestCase):
    def test_record_is_retail_layout(self) -> None:
        self.assertEqual(lights.LIGHT_RECORD.size, 60)
        data = fakk_map([light_record((1.0, 2.0, 3.0), (0.5, 0.25, 1.0), 200.0, 1, 1, 1,
                                      (0.0, 0.0, -1.0), 0.5, 32.0)], clusters=(7, 9))
        compiled, clusters = lights.read_compiled_lights(data)
        self.assertEqual(clusters, [7, 9])
        light = compiled[0]
        self.assertEqual((light.origin, light.color, light.intensity, light.leaf), ((1, 2, 3), (0.5, 0.25, 1), 200, 1))
        self.assertEqual((light.needs_trace, light.spot, light.spot_dir), (1, 1, (0, 0, -1)))
        self.assertEqual((light.spot_radius_by_distance, light.cap), (0.5, 32))

    def test_malformed_maps_are_refused(self) -> None:
        good = fakk_map([light_record()])
        directory = 12 + LIGHT_LUMP * 8
        cases = [
            ("not a FAKK map", b"IBSP" + good[4:]),
            ("expected Alice's FAKK 42", fakk_map([light_record()], version=12)),
            ("truncated FAKK header", good[:100]),
            ("whole number of records", good[:directory + 4] + struct.pack("<i", 56) + good[directory + 8:]),
            ("outside the file", good[:directory] + struct.pack("<i", len(good)) + good[directory + 4:]),
            ("names leaf 3", fakk_map([light_record(leaf=3)])),
            ("names leaf -1", fakk_map([light_record(leaf=-1)])),
            ("not finite", fakk_map([light_record(intensity=math.nan)])),
            ("flag other than 0 or 1", fakk_map([light_record(spot=2)])),
            ("retail refuses", fakk_map([light_record()] * (lights.RETAIL_MAX_LIGHTS + 1))),
        ]
        for message, data in cases:
            with self.subTest(message):
                with self.assertRaisesRegex(ValueError, re.escape(message)):
                    lights.read_compiled_lights(data)

    def test_exclusions(self) -> None:
        compiled, _ = lights.read_compiled_lights(fakk_map([
            light_record(),
            light_record(spot=1, spot_dir=(0.0, 0.0, 1.0), radius_by_distance=-90.51),
            light_record(intensity=-100.0),
            light_record(color=(0.0, 0.0, 0.0)),
            light_record(spot=1, spot_dir=(0.0, 0.0, 0.0), radius_by_distance=0.3),
            light_record(spot=1, spot_dir=(0.0, 0.0, -1.0), radius_by_distance=0.3),
        ]))
        self.assertEqual([lights.exclusion(light) for light in compiled], [
            None, "surfaceEmitter", "nonPositiveIntensity", "black", "spotWithoutDirection", None])

    def test_conversion_follows_retail_model_light(self) -> None:
        compiled, clusters = lights.read_compiled_lights(fakk_map([
            light_record((8.0, -16.5, 0.25), (1.0, 0.5, 0.25), 300.0),
            light_record(color=(1.0, 0.5, 0.0), intensity=50.0, cap=51.0),
            light_record(spot=1, spot_dir=(0.6, 0.0, -0.8), radius_by_distance=math.tan(math.radians(20.0))),
        ]))
        point, capped, spot = compiled
        # 16 * r^2 / d^2 == 30000 * I / d^2
        self.assertAlmostEqual(lights.static_radius(point), 750.0)
        self.assertAlmostEqual(lights.DLIGHT_AT_RADIUS * lights.static_radius(capped) ** 2,
                               lights.RETAIL_ENTITY_LIGHT_SCALE * 50.0, places=3)
        self.assertEqual(lights.static_color(point), (1.0, 0.5, 0.25))
        self.assertEqual(lights.static_color(capped), (0.2, 0.1, 0.0))
        document = decode(lights.light_document("test", compiled))
        entries = document["lights"]
        self.assertEqual(entries[0], {
            "name": "test-light-0000", "type": "point", "origin": [8, -16.5, 0.25],
            "color": [1, 0.5, 0.25], "intensity": 300, "radius": 750, "castsShadows": True})
        self.assertEqual(entries[1]["color"], [0.2, 0.1, 0])
        self.assertEqual(entries[2]["type"], "spot")
        self.assertEqual(entries[2]["direction"], [0.6, 0, -0.8])
        self.assertEqual(entries[2]["outerAngle"], 20)
        self.assertNotIn("direction", entries[0])
        self.assertNotIn("outerAngle", entries[0])

    def test_overlapping_lights_are_balanced(self) -> None:
        full = lambda origin=(0.0, 0.0, 0.0): light_record(origin, (1.0, 1.0, 1.0), 300.0)
        scales = lambda records: lights.balance_colors(lights.read_compiled_lights(fakk_map(records))[0])
        # One full-colour light adds at most half the surface's brightness.
        self.assertEqual(scales([full()]), {0: 0.5})
        # Two out of each other's reach are balanced alone.
        self.assertEqual(scales([full(), full((5000.0, 0.0, 0.0))]), {0: 0.5, 1: 0.5})
        # Eight at one point share it, and only the renderers' eight count.
        self.assertEqual(set(scales([full()] * 8).values()), {0.5 / 8})
        self.assertEqual(set(scales([full()] * 20).values()), {0.5 / 8})
        # A dim light is not raised to the peak.
        self.assertEqual(scales([light_record(color=(0.1, 0.1, 0.1))]), {0: 1.0})
        # A light at the edge of another's default reach is barely counted
        # (the renderers halve each radius by default).
        reach = lights.static_radius(lights.read_compiled_lights(fakk_map([full()]))[0][0]) * lights.RENDERER_DLIGHT_SCALE
        edge = scales([full(), full((reach * 0.99, 0.0, 0.0))])
        self.assertTrue(0.49 < edge[0] < 0.5)
        # The files carry the balance, and the summary says how many it moved.
        payload, summary = lights.convert_map("stack", fakk_map([full()] * 4))
        self.assertEqual({tuple(entry["color"]) for entry in decode(payload)["lights"]}, {(0.125, 0.125, 0.125)})
        self.assertEqual((summary["balanced"], summary["smallestBalance"]), (4, 0.125))

    def test_numbers_are_fixed_point(self) -> None:
        self.assertEqual(lights.number(0.000004, 5), "0")
        self.assertEqual(lights.number(-0.000004, 5), "0")
        self.assertEqual(lights.number(0.00001, 5), "0.00001")
        self.assertEqual(lights.number(1e-7, 4), "0")
        self.assertEqual(lights.number(12000.0, 1), "12000")
        self.assertEqual(lights.number(-1.25, 3), "-1.25")
        self.assertEqual(lights.number(100.0, 0), "100")
        self.assertEqual(lights.number(1e20, 1), "100000000000000000000")

    def test_selection_spreads_across_clusters(self) -> None:
        records = [light_record(intensity=value, leaf=0) for value in (900.0, 800.0, 700.0, 600.0)]
        records += [light_record(intensity=20.0, leaf=1), light_record(intensity=10.0, leaf=2)]
        compiled, clusters = lights.read_compiled_lights(fakk_map(records, clusters=(4, 5, -1)))
        pick = lambda limit: [light.record for light in lights.select_lights(compiled, clusters, limit)]
        # One per cluster first, strongest first, then each cluster's second.
        self.assertEqual(pick(1), [0])
        self.assertEqual(pick(3), [0, 4, 5])
        self.assertEqual(pick(4), [0, 1, 4, 5])
        self.assertEqual(pick(10), [0, 1, 2, 3, 4, 5])

    def test_the_renderers_limit_is_never_exceeded(self) -> None:
        records = [light_record((float(i), 0.0, 0.0), intensity=100.0 + i, leaf=i % 200)
                   for i in range(400)]
        payload, summary = lights.convert_map("big", fakk_map(records, clusters=tuple(range(200))))
        document = decode(payload)
        self.assertEqual(len(document["lights"]), lights.MAX_STATIC_MAP_LIGHTS)
        self.assertEqual(summary["selectedClusters"], lights.MAX_STATIC_MAP_LIGHTS)
        self.assertEqual(summary["dropped"], 400 - lights.MAX_STATIC_MAP_LIGHTS)
        self.assertEqual(payload, lights.convert_map("big", fakk_map(records, clusters=tuple(range(200))))[0])


class PublicationTests(unittest.TestCase):
    def _probe(self, *, fail: bool) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game = root / "retail" / "base"
            game.mkdir(parents=True)
            maps = {"maps/garden1.bsp": fakk_map([light_record()]),
                    "maps/keep.bsp": b"broken" if fail else fakk_map([light_record(intensity=50.0)])}
            with zipfile.ZipFile(game / "pak0.pk3", "w") as archive:
                for name, data in maps.items():
                    archive.writestr(name, data)
            output = root / "pkg" / "games"
            old = output / "alice" / "base" / "maps" / "garden1.lights.json"
            old.parent.mkdir(parents=True)
            old.write_bytes(b"old sidecar")
            custom = old.with_name("custom.lights.json")
            custom.write_bytes(b"custom sidecar")
            args = argparse.Namespace(retail_path=game, work_root=root / ".tmp" / "work", output_root=output)
            with patch.object(lights, "ROOT", root), \
                 patch.object(lights, "STOCK_ALICE_MAPS", ("garden1", "keep")), \
                 patch.object(alice_retail_maps, "STOCK_ALICE_MAPS", ("garden1", "keep")):
                if fail:
                    with self.assertRaisesRegex(ValueError, "existing sidecars preserved"):
                        lights.generate_sidecars(args)
                    self.assertEqual(old.read_bytes(), b"old sidecar")
                    self.assertFalse((old.parent / "keep.lights.json").exists())
                else:
                    lights.generate_sidecars(args)
                    self.assertEqual(decode(old.read_bytes())["map"], "garden1")
                    self.assertEqual(decode((old.parent / "keep.lights.json").read_bytes())["lights"][0]["intensity"], 50)
            self.assertEqual(custom.read_bytes(), b"custom sidecar")
            self.assertEqual(list(output.rglob(".lights-*")), [])
            self.assertTrue((args.work_root / "alice-lights.json").is_file())

    def test_failure_preserves_existing_files(self) -> None:
        self._probe(fail=True)

    def test_success_publishes_every_map_without_pruning(self) -> None:
        self._probe(fail=False)


class ShippedLightTests(unittest.TestCase):
    """The shipped files, against their record and what every renderer's parser accepts."""

    # rendererrtx/tr_bsp.c STATIC_MAP_LIGHTS_SIDECAR_MAX_BYTES and _MAX_TOKENS
    MAX_BYTES = 256 * 1024
    MAX_TOKENS = 65536

    def test_shipped_files_match_their_record_and_the_parsers(self) -> None:
        evidence = json.loads((ROOT / "docs/fntech3/2026-09-24-alice-map-sidecars.json").read_text())
        records = {run["map"]: run for run in evidence["staticLights"]["maps"]}
        self.assertEqual(set(records), set(STOCK_ALICE_MAPS))
        directory = ROOT / "pkg/games/alice/base/maps"
        self.assertEqual({p.name[:-len(".lights.json")] for p in directory.glob("*.lights.json")},
                         set(STOCK_ALICE_MAPS))
        total = 0
        for name in STOCK_ALICE_MAPS:
            with self.subTest(name):
                payload = (directory / f"{name}.lights.json").read_bytes()
                record = records[name]
                self.assertEqual(hashlib.sha256(payload).hexdigest(), record["lightsSha256"])
                self.assertLessEqual(len(payload), self.MAX_BYTES)
                self.assertNotIn(b"\0", payload)
                self.assertNotIn(b"\r", payload)
                self.assertLess(len(re.findall(rb'"[^"]*"|[{}\[\],:]|[^\s{}\[\],:"]+', payload)), self.MAX_TOKENS)
                document = decode(payload)
                self.assertEqual(document["version"], 1)
                self.assertEqual(document["map"], name)
                entries = document["lights"]
                self.assertEqual(len(entries), record["selected"])
                self.assertLessEqual(len(entries), lights.MAX_STATIC_MAP_LIGHTS)
                self.assertEqual(len({entry["name"] for entry in entries}), len(entries))
                for entry in entries:
                    spot = entry["type"] == "spot"
                    self.assertEqual(set(entry), {"name", "type", "origin", "color", "intensity", "radius",
                                                  "castsShadows"} | ({"direction", "outerAngle"} if spot else set()))
                    self.assertLess(len(entry["name"]), 64)
                    self.assertTrue(0 < max(entry["color"]) <= 1)
                    self.assertTrue(1 <= entry["radius"] <= 8192 and entry["intensity"] > 0)
                    if spot:
                        self.assertAlmostEqual(math.hypot(*entry["direction"]), 1.0, places=3)
                        self.assertTrue(0 < entry["outerAngle"] < 90)
                # Fixed point only: no exponent anywhere.
                self.assertIsNone(re.search(rb"\d[eE][-+]?\d", payload))
                total += len(entries)
        self.assertEqual(total, evidence["staticLights"]["summary"]["selected"])
        self.assertEqual(evidence["staticLights"]["reproducibility"], {"runs": 3, "byteIdenticalSidecars": 36})


if __name__ == "__main__":
    unittest.main()
