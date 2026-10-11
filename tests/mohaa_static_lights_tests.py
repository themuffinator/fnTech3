"""Medal of Honor: Allied Assault's static light files, from its maps' sphere lights.

Synthetic 2015 maps only; no retail map is read. The shipped files are
checked against their generation record and the renderers' parser limits. The
selection and the file format are the Alice generator's
(tests/alice_static_lights_tests.py covers them); what differs is 2015's
header and lumps, its maps in folders, and its model-light law, a quarter of
Alice's.
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
import generate_stock_mohaa_lights as lights
import mohaa_retail_maps
from stock_mohaa_maps import STOCK_MOHAA_MAPS

LEAF_LUMP = 8
LIGHT_LUMP = 19


def light_record(origin=(0.0, 0.0, 0.0), color=(1.0, 1.0, 1.0), intensity=120.0, leaf=0,
                 needs_trace=0, spot=0, spot_dir=(0.0, 0.0, 0.0), radius_by_distance=0.0) -> bytes:
    return struct.pack("<3f3ffiii3ff", *origin, *color, intensity, leaf, needs_trace, spot,
                       *spot_dir, radius_by_distance)


def mohaa_map(records: list[bytes], clusters=(0,), *, version: int = 19) -> bytes:
    leafs = b"".join(struct.pack("<16i", cluster, 0, 0, 0, 0, 64, 64, 64, 0, 0, 0, 0, 0, 0, 0, 0)
                     for cluster in clusters)
    lumps = {LEAF_LUMP: leafs, LIGHT_LUMP: b"".join(records)}
    header = bytearray(b"2015" + struct.pack("<ii", version, 0))
    body = bytearray()
    offset = lights.MOHAA_HEADER_BYTES
    for index in range(lights.MOHAA_HEADER_LUMPS):
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


class SphereLightTests(unittest.TestCase):
    def test_record_is_mapspherel_in_lump_19(self) -> None:
        self.assertEqual(lights.LIGHT_RECORD.size, 56)
        self.assertEqual(lights.MOHAA_HEADER_BYTES, 12 + 28 * 8)
        data = mohaa_map([light_record((1.0, 2.0, 3.0), (0.5, 0.25, 1.0), 200.0, 1, 1, 1,
                                       (0.0, 0.0, -1.0), 0.5)], clusters=(7, 9))
        compiled, clusters = lights.read_compiled_lights(data)
        self.assertEqual(clusters, [7, 9])
        light = compiled[0]
        self.assertEqual((light.origin, light.color, light.intensity, light.leaf), ((1, 2, 3), (0.5, 0.25, 1), 200, 1))
        self.assertEqual((light.needs_trace, light.spot, light.spot_dir), (1, 1, (0, 0, -1)))
        self.assertEqual((light.spot_radius_by_distance, light.cap), (0.5, 0.0))

    def test_malformed_maps_are_refused(self) -> None:
        good = mohaa_map([light_record()])
        directory = 12 + LIGHT_LUMP * 8
        cases = [
            ("not a 2015 map", b"FAKK" + good[4:]),
            ("expected Medal of Honor's 2015 19", mohaa_map([light_record()], version=17)),
            ("truncated 2015 header", good[:100]),
            ("whole number of records", good[:directory + 4] + struct.pack("<i", 52) + good[directory + 8:]),
            ("outside the file", good[:directory] + struct.pack("<i", len(good)) + good[directory + 4:]),
            ("names leaf 3", mohaa_map([light_record(leaf=3)])),
            ("not finite", mohaa_map([light_record(intensity=math.inf)])),
            ("flag other than 0 or 1", mohaa_map([light_record(needs_trace=2)])),
            ("retail refuses", mohaa_map([light_record()] * (lights.RETAIL_MAX_LIGHTS + 1))),
        ]
        for message, data in cases:
            with self.subTest(message):
                with self.assertRaisesRegex(ValueError, re.escape(message)):
                    lights.read_compiled_lights(data)
        # 1,531 is the most 2015's loader keeps (it drops a map at 1,532).
        self.assertEqual(len(lights.read_compiled_lights(mohaa_map([light_record()] * 1531))[0]), 1531)

    def test_conversion_takes_2015s_law(self) -> None:
        payload, summary = lights.convert_map("DM/test", mohaa_map([
            light_record((8.0, -16.5, 0.25), (1.0, 0.5, 0.25), 120.0),
            light_record((0.0, 0.0, 64.0), (1.0, 1.0, 1.0), 300.0, spot=1, spot_dir=(0.0, 0.0, -1.0),
                         radius_by_distance=1.0),
            # A disc spot, which 2015 never lets pass its cone test.
            light_record(spot=1, spot_dir=(0.0, 0.0, 1.0), radius_by_distance=-32.0),
        ]))
        document = decode(payload)
        self.assertEqual(document["source"], "2015 19 lump 19, the map's sphere lights")
        self.assertEqual(document["generator"], lights.GENERATOR)
        self.assertEqual(document["map"], "DM/test")
        # |I| x 7500 / d^2 falls to 16 at sqrt(7500 x 120 / 16) = 237.2 units.
        point, spot = document["lights"]
        self.assertEqual(point["name"], "test-light-0000")
        self.assertEqual((point["type"], point["origin"], point["intensity"], point["radius"]),
                         ("point", [8, -16.5, 0.25], 120, 237.2))
        self.assertEqual((spot["type"], spot["direction"], spot["radius"], spot["outerAngle"]),
                         ("spot", [0, 0, -1], 375, 45))
        self.assertEqual(summary["excluded"], {"surfaceEmitter": 1})

    def test_a_lone_light_keeps_half_its_colour(self) -> None:
        payload, summary = lights.convert_map("test", mohaa_map([light_record(color=(1.0, 0.5, 0.25))]))
        self.assertEqual(decode(payload)["lights"][0]["color"], [0.5, 0.25, 0.125])
        self.assertEqual((summary["balanced"], summary["smallestBalance"]), (1, 0.5))

    def test_overlap_follows_2015s_shorter_reach(self) -> None:
        # 300 units apart: inside Alice's law's reach (0.5 x 750), outside
        # 2015's (0.5 x 375), so under 2015's law the two keep their halves.
        records = [light_record((0.0, 0.0, 0.0), intensity=300.0), light_record((300.0, 0.0, 0.0), intensity=300.0)]
        payload, _ = lights.convert_map("test", mohaa_map(records))
        self.assertEqual([light["color"][0] for light in decode(payload)["lights"]], [0.5, 0.5])
        payload, _ = lights.convert_map("test", mohaa_map([light_record(intensity=300.0)] * 2))
        self.assertEqual([light["color"][0] for light in decode(payload)["lights"]], [0.25, 0.25])

    def test_a_map_without_sphere_lights_ships_an_empty_file(self) -> None:
        payload, summary = lights.convert_map("briefing/test", mohaa_map([]))
        self.assertEqual(decode(payload)["lights"], [])
        self.assertEqual(summary["selected"], 0)


class PublicationTests(unittest.TestCase):
    def _probe(self, *, fail: bool) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            game = root / "retail" / "main"
            game.mkdir(parents=True)
            maps = {"maps/DM/mohdm1.bsp": mohaa_map([light_record()]),
                    "maps/m1l1.bsp": b"broken" if fail else mohaa_map([light_record(intensity=50.0)])}
            with zipfile.ZipFile(game / "Pak5.pk3", "w") as archive:
                for name, data in maps.items():
                    archive.writestr(name, data)
            with zipfile.ZipFile(game / "Pak0.pk3", "w") as archive:
                archive.writestr("default.cfg", "")
            output = root / "pkg" / "games"
            old = output / "mohaa" / "main" / "maps" / "DM" / "mohdm1.lights.json"
            old.parent.mkdir(parents=True)
            old.write_bytes(b"old sidecar")
            custom = old.with_name("custom.lights.json")
            custom.write_bytes(b"custom sidecar")
            args = argparse.Namespace(retail_path=game.parent, work_root=root / ".tmp" / "work", output_root=output)
            stock = ("DM/mohdm1", "m1l1")
            with patch.object(lights, "ROOT", root), patch.object(lights, "STOCK_MOHAA_MAPS", stock), \
                 patch.object(mohaa_retail_maps, "STOCK_MOHAA_MAPS", stock):
                if fail:
                    with self.assertRaisesRegex(ValueError, "existing sidecars preserved"):
                        lights.generate_sidecars(args)
                    self.assertEqual(old.read_bytes(), b"old sidecar")
                    self.assertFalse((output / "mohaa/main/maps/m1l1.lights.json").exists())
                else:
                    lights.generate_sidecars(args)
                    self.assertEqual(decode(old.read_bytes())["map"], "DM/mohdm1")
                    published = decode((output / "mohaa/main/maps/m1l1.lights.json").read_bytes())
                    self.assertEqual(published["lights"][0]["intensity"], 50)
            self.assertEqual(custom.read_bytes(), b"custom sidecar")
            self.assertEqual(list(output.rglob(".lights-*")), [])
            self.assertTrue((args.work_root / "mohaa-lights.json").is_file())

    def test_failure_preserves_existing_files(self) -> None:
        self._probe(fail=True)

    def test_success_publishes_every_map_in_its_folder_without_pruning(self) -> None:
        self._probe(fail=False)

    def test_the_inventory_is_the_paks_own_spelling(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp).resolve() / "main"
            game.mkdir()
            stock = ("DM/mohdm1",)
            for entries, message in ((["maps/dm/mohdm1.bsp"], "spells"),
                                     (["maps/DM/mohdm1.bsp", "maps/extra.bsp"], "unexpected=['extra']"),
                                     ([], "missing=['dm/mohdm1']"),
                                     (["maps/DM/../mohdm1.bsp"], "unsafe BSP entry")):
                with self.subTest(entries=entries):
                    with zipfile.ZipFile(game / "Pak5.pk3", "w") as archive:
                        for entry in entries:
                            archive.writestr(entry, b"")
                    with patch.object(mohaa_retail_maps, "STOCK_MOHAA_MAPS", stock):
                        with self.assertRaisesRegex(ValueError, re.escape(message)):
                            mohaa_retail_maps.discover_official_maps(game)
            # A pak named like an official one but not one of the six is not read.
            with zipfile.ZipFile(game / "Pak5.pk3", "w") as archive:
                archive.writestr("maps/DM/mohdm1.bsp", b"")
            with zipfile.ZipFile(game / "pak6.pk3", "w") as archive:
                archive.writestr("maps/custom.bsp", b"")
            with patch.object(mohaa_retail_maps, "STOCK_MOHAA_MAPS", stock):
                self.assertEqual(set(mohaa_retail_maps.discover_official_maps(game)), {"DM/mohdm1"})
            self.assertEqual(mohaa_retail_maps.resolve_game_directory(Path(tmp).resolve()), game)
            self.assertEqual(mohaa_retail_maps.resolve_game_directory(game), game)


class ShippedLightTests(unittest.TestCase):
    """The shipped files, against their record and what every renderer's parser accepts."""

    # rendererrtx/tr_bsp.c STATIC_MAP_LIGHTS_SIDECAR_MAX_BYTES and _MAX_TOKENS
    MAX_BYTES = 256 * 1024
    MAX_TOKENS = 65536

    def test_shipped_files_match_their_record_and_the_parsers(self) -> None:
        evidence = json.loads((ROOT / "docs/fntech3/2026-09-25-mohaa-map-sidecars.json").read_text())
        records = {run["map"]: run for run in evidence["staticLights"]["maps"]}
        self.assertEqual(len(STOCK_MOHAA_MAPS), 54)
        self.assertEqual(set(records), set(STOCK_MOHAA_MAPS))
        directory = ROOT / "pkg/games/mohaa/main/maps"
        shipped = {path.relative_to(directory).as_posix()[:-len(".lights.json")]
                   for path in directory.rglob("*.lights.json")}
        self.assertEqual(shipped, set(STOCK_MOHAA_MAPS))
        total = 0
        for name in STOCK_MOHAA_MAPS:
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
                self.assertLessEqual(len(entries), 128)
                self.assertEqual(len({entry["name"] for entry in entries}), len(entries))
                for entry in entries:
                    spot = entry["type"] == "spot"
                    self.assertEqual(set(entry), {"name", "type", "origin", "color", "intensity", "radius",
                                                  "castsShadows"} | ({"direction", "outerAngle"} if spot else set()))
                    self.assertLess(len(entry["name"]), 64)
                    self.assertNotIn("/", entry["name"])
                    self.assertTrue(0 < max(entry["color"]) <= 0.5)
                    self.assertTrue(1 <= entry["radius"] <= 8192 and entry["intensity"] > 0)
                    self.assertAlmostEqual(entry["radius"], math.sqrt(7500 * entry["intensity"] / 16), delta=0.06)
                    if spot:
                        self.assertAlmostEqual(math.hypot(*entry["direction"]), 1.0, places=3)
                        self.assertTrue(0 < entry["outerAngle"] < 90)
                self.assertIsNone(re.search(rb"\d[eE][-+]?\d", payload))
                total += len(entries)
        self.assertEqual(total, evidence["staticLights"]["summary"]["selected"])
        self.assertEqual(evidence["staticLights"]["reproducibility"], {"runs": 2, "byteIdenticalSidecars": 54})
        facts = evidence["retailFacts"]["sphereLightLump"]
        self.assertEqual((facts["lump"], facts["recordBytes"], facts["mapsDividingBy56"]), (19, 56, 54))
        # Every negative-radius spot, 2015's disc emitters, is left out and nothing else.
        self.assertEqual(evidence["staticLights"]["summary"]["excluded"], {"surfaceEmitter": facts["negativeRadiusSpots"]})

    def test_shipped_zones_match_their_record(self) -> None:
        evidence = json.loads((ROOT / "docs/fntech3/2026-09-25-mohaa-map-sidecars.json").read_text())
        records = {run["map"]: run for run in evidence["audioZones"]["maps"]}
        self.assertEqual(set(records), set(STOCK_MOHAA_MAPS))
        directory = ROOT / "pkg/games/mohaa/main/maps"
        self.assertEqual({path.relative_to(directory).as_posix()[:-len(".azb")] for path in directory.rglob("*.azb")},
                         set(STOCK_MOHAA_MAPS))
        for name in STOCK_MOHAA_MAPS:
            with self.subTest(name):
                payload = (directory / f"{name}.azb").read_bytes()
                self.assertEqual(hashlib.sha256(payload).hexdigest(), records[name]["azbSha256"])
                self.assertEqual(len(payload), records[name]["azbBytes"])
        self.assertEqual(evidence["audioZones"]["summary"]["passed"], 54)
        self.assertEqual(evidence["audioZones"]["reproducibility"], {"runs": 2, "byteIdenticalSidecars": 54})


if __name__ == "__main__":
    unittest.main()
