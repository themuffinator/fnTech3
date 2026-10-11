"""Heavy Metal: F.A.K.K.2's static light files, from its maps' compiled lights.

Synthetic FAKK 12 maps only; no retail map is read. The shipped files are
checked against their generation record and the renderers' parser limits. The
selection, the file format and the radius are the Alice generator's
(tests/alice_static_lights_tests.py covers them), since at its defaults
fakk2.exe 1.02 lights a model with alice.exe's law; what differs is
F.A.K.K.2's 56-byte record, which has no `cap`.
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
import fakk2_retail_maps
import generate_stock_alice_lights as alice_lights
import generate_stock_fakk2_lights as lights
from stock_fakk2_maps import STOCK_FAKK2_MAPS

LEAF_LUMP = 8
LIGHT_LUMP = 17


def light_record(origin=(0.0, 0.0, 0.0), color=(1.0, 1.0, 1.0), intensity=300.0, leaf=0,
                 needs_trace=0, spot=0, spot_dir=(0.0, 0.0, 0.0), radius_by_distance=0.0) -> bytes:
    return struct.pack("<3f3ffiii3ff", *origin, *color, intensity, leaf, needs_trace, spot,
                       *spot_dir, radius_by_distance)


def fakk_map(records: list[bytes], clusters=(0,), *, version: int = 12) -> bytes:
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
    def test_record_is_ritual_mapspherel(self) -> None:
        self.assertEqual(lights.LIGHT_RECORD.size, 56)
        data = fakk_map([light_record((1.0, 2.0, 3.0), (0.5, 0.25, 1.0), 200.0, 1, 1, 1,
                                      (0.0, 0.0, -1.0), 0.5)], clusters=(7, 9))
        compiled, clusters = lights.read_compiled_lights(data)
        self.assertEqual(clusters, [7, 9])
        light = compiled[0]
        self.assertEqual((light.origin, light.color, light.intensity, light.leaf), ((1, 2, 3), (0.5, 0.25, 1), 200, 1))
        self.assertEqual((light.needs_trace, light.spot, light.spot_dir), (1, 1, (0, 0, -1)))
        # No cap in F.A.K.K.2's record: every light is uncapped.
        self.assertEqual((light.spot_radius_by_distance, light.cap), (0.5, 0.0))

    def test_malformed_maps_are_refused(self) -> None:
        good = fakk_map([light_record()])
        directory = 12 + LIGHT_LUMP * 8
        cases = [
            ("not a FAKK map", b"IBSP" + good[4:]),
            ("expected F.A.K.K.2's FAKK 12", fakk_map([light_record()], version=42)),
            ("truncated FAKK header", good[:100]),
            ("whole number of records", good[:directory + 4] + struct.pack("<i", 52) + good[directory + 8:]),
            ("outside the file", good[:directory] + struct.pack("<i", len(good)) + good[directory + 4:]),
            ("names leaf 3", fakk_map([light_record(leaf=3)])),
            ("not finite", fakk_map([light_record(intensity=math.nan)])),
            ("flag other than 0 or 1", fakk_map([light_record(spot=2)])),
            ("retail refuses", fakk_map([light_record()] * (lights.RETAIL_MAX_LIGHTS + 1))),
        ]
        for message, data in cases:
            with self.subTest(message):
                with self.assertRaisesRegex(ValueError, re.escape(message)):
                    lights.read_compiled_lights(data)

    def test_conversion_keeps_alices_rules(self) -> None:
        payload, summary = lights.convert_map("test", fakk_map([
            light_record((8.0, -16.5, 0.25), (1.0, 0.5, 0.25), 300.0),
            light_record(spot=1, spot_dir=(0.0, 0.0, 1.0), radius_by_distance=-90.51),
            light_record(intensity=-100.0),
        ]))
        document = decode(payload)
        self.assertEqual(document["source"], "FAKK 12 lump 17, the map's compiled entity lights")
        self.assertEqual(document["generator"], lights.GENERATOR)
        # The balance for the per-pixel pass halves a lone full-colour light
        # (generate_stock_alice_lights.balance_colors, SUPPLEMENT_PEAK 0.5).
        self.assertEqual(document["lights"], [{
            "name": "test-light-0000", "type": "point", "origin": [8, -16.5, 0.25],
            "color": [0.5, 0.25, 0.125], "intensity": 300, "radius": 750, "castsShadows": True}])
        self.assertEqual(summary["excluded"], {"nonPositiveIntensity": 1, "surfaceEmitter": 1})
        self.assertEqual((summary["balanced"], summary["smallestBalance"]), (1, 0.5))

    def test_the_radius_is_fakk2s_law_at_its_defaults(self) -> None:
        # 7500 a light (0x004d6050), and the sum times 1 << (r_mapOverBrightBits
        # "2" - r_overBrightBits "0") (0x0048e090): alice.exe's 30000, which the
        # Alice rules' radius and balance use.
        self.assertEqual((lights.RETAIL_LIGHT_SCALE, lights.RETAIL_MAP_OVERBRIGHT_BITS, lights.RETAIL_OVERBRIGHT_BITS),
                         (7500.0, 2, 0))
        self.assertEqual(lights.RETAIL_ENTITY_LIGHT_SCALE, 30000.0)
        self.assertEqual(lights.RETAIL_ENTITY_LIGHT_SCALE, alice_lights.RETAIL_ENTITY_LIGHT_SCALE)

    def test_overlapping_lights_share_the_balance(self) -> None:
        # Two coincident lights split the half between them; a far one keeps its own half.
        payload, summary = lights.convert_map("test", fakk_map([
            light_record((0.0, 0.0, 0.0), (1.0, 1.0, 1.0), 300.0),
            light_record((0.0, 0.0, 0.0), (1.0, 1.0, 1.0), 300.0),
            light_record((100000.0, 0.0, 0.0), (1.0, 1.0, 1.0), 300.0),
        ]))
        colors = [light["color"][0] for light in decode(payload)["lights"]]
        self.assertEqual(colors, [0.25, 0.25, 0.5])
        self.assertEqual((summary["balanced"], summary["smallestBalance"]), (3, 0.25))


class PublicationTests(unittest.TestCase):
    def _probe(self, *, fail: bool) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            game = root / "retail" / "fakk"
            game.mkdir(parents=True)
            maps = {"maps/fakkhouse.bsp": fakk_map([light_record()]),
                    "maps/zoo.bsp": b"broken" if fail else fakk_map([light_record(intensity=50.0)])}
            with zipfile.ZipFile(game / "pak0.pk3", "w") as archive:
                for name, data in maps.items():
                    archive.writestr(name, data)
            output = root / "pkg" / "games"
            old = output / "fakk2" / "fakk" / "maps" / "fakkhouse.lights.json"
            old.parent.mkdir(parents=True)
            old.write_bytes(b"old sidecar")
            custom = old.with_name("custom.lights.json")
            custom.write_bytes(b"custom sidecar")
            args = argparse.Namespace(retail_path=game, work_root=root / ".tmp" / "work", output_root=output)
            with patch.object(lights, "ROOT", root), \
                 patch.object(lights, "STOCK_FAKK2_MAPS", ("fakkhouse", "zoo")), \
                 patch.object(fakk2_retail_maps, "STOCK_FAKK2_MAPS", ("fakkhouse", "zoo")):
                if fail:
                    with self.assertRaisesRegex(ValueError, "existing sidecars preserved"):
                        lights.generate_sidecars(args)
                    self.assertEqual(old.read_bytes(), b"old sidecar")
                    self.assertFalse((old.parent / "zoo.lights.json").exists())
                else:
                    lights.generate_sidecars(args)
                    self.assertEqual(decode(old.read_bytes())["map"], "fakkhouse")
                    self.assertEqual(decode((old.parent / "zoo.lights.json").read_bytes())["lights"][0]["intensity"], 50)
            self.assertEqual(custom.read_bytes(), b"custom sidecar")
            self.assertEqual(list(output.rglob(".lights-*")), [])
            self.assertTrue((args.work_root / "fakk2-lights.json").is_file())

    def test_failure_preserves_existing_files(self) -> None:
        self._probe(fail=True)

    def test_success_publishes_every_map_without_pruning(self) -> None:
        self._probe(fail=False)

    def test_the_installation_is_found_from_its_root_or_its_game_folder(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp).resolve() / "fakk"
            game.mkdir()
            (game / "enu").mkdir()
            self.assertEqual(fakk2_retail_maps.resolve_game_directory(Path(tmp).resolve()), game)
            self.assertEqual(fakk2_retail_maps.resolve_game_directory(game), game)


class ShippedLightTests(unittest.TestCase):
    """The shipped files, against their record and what every renderer's parser accepts."""

    # rendererrtx/tr_bsp.c STATIC_MAP_LIGHTS_SIDECAR_MAX_BYTES and _MAX_TOKENS
    MAX_BYTES = 256 * 1024
    MAX_TOKENS = 65536

    def test_shipped_files_match_their_record_and_the_parsers(self) -> None:
        evidence = json.loads((ROOT / "docs/fntech3/2026-09-24-fakk2-map-sidecars.json").read_text())
        records = {run["map"]: run for run in evidence["staticLights"]["maps"]}
        self.assertEqual(len(STOCK_FAKK2_MAPS), 30)
        self.assertEqual(set(records), set(STOCK_FAKK2_MAPS))
        directory = ROOT / "pkg/games/fakk2/fakk/maps"
        self.assertEqual({p.name[:-len(".lights.json")] for p in directory.glob("*.lights.json")},
                         set(STOCK_FAKK2_MAPS))
        total = 0
        oversized: list[str] = []
        for name in STOCK_FAKK2_MAPS:
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
                    self.assertTrue(0 < max(entry["color"]) <= 1)
                    # Three lights -- blood's two of intensity 99999 and one of
                    # oracle's -- reach past the 8192 units every renderer clamps
                    # a radius to (tr_bsp.c); the file keeps retail's law.
                    self.assertTrue(entry["radius"] >= 1 and entry["intensity"] > 0)
                    if entry["radius"] > 8192:
                        oversized.append(entry["name"])
                    if spot:
                        self.assertAlmostEqual(math.hypot(*entry["direction"]), 1.0, places=3)
                        self.assertTrue(0 < entry["outerAngle"] < 90)
                self.assertIsNone(re.search(rb"\d[eE][-+]?\d", payload))
                total += len(entries)
        self.assertEqual(total, evidence["staticLights"]["summary"]["selected"])
        self.assertEqual(sorted(oversized), ["blood-light-0012", "blood-light-0014", "oracle-light-0212"])
        self.assertEqual(evidence["staticLights"]["reproducibility"], {"runs": 3, "byteIdenticalSidecars": 30})
        self.assertEqual(evidence["retailFacts"]["entityLightPower"]["scale"], lights.RETAIL_ENTITY_LIGHT_SCALE)
        # Every retail map's compiled lights are F.A.K.K.2's 56-byte records.
        self.assertEqual(evidence["retailFacts"]["entityLightLump"]["recordBytes"], 56)
        self.assertEqual(evidence["retailFacts"]["entityLightLump"]["mapsDividingBy56"], 30)


if __name__ == "__main__":
    unittest.main()
