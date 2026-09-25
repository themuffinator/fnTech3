"""RTCW's static light files, from its maps' compiled `light` entities.

Synthetic IBSP 47 maps only; no retail map is read. The shipped files are
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
import generate_stock_rtcw_audio_zones
import generate_stock_rtcw_lights as lights
import root_archive
from stock_rtcw_maps import STOCK_RTCW_MAPS

RECORD = ROOT / "docs/fntech3/2026-09-24-rtcw-map-lights.json"


def entity_text(entities) -> bytes:
    blocks = []
    for entity in entities:
        pairs = entity.items() if isinstance(entity, dict) else entity
        blocks.append("{\n" + "".join(f'"{key}" "{value}"\n' for key, value in pairs) + "}\n")
    return "".join(blocks).encode("latin-1") + b"\0"


def ibsp_map(entities, *, clusters=(3, 7), version: int = 47, lumps: dict | None = None) -> bytes:
    """One plane (x = 0) splitting the world into a front leaf and a back leaf."""
    contents = {
        0: entity_text(entities),
        2: struct.pack("<4f", 1.0, 0.0, 0.0, 0.0),
        3: struct.pack("<3i6i", 0, -1, -2, 0, 0, 0, 0, 0, 0),
        4: b"".join(struct.pack("<12i", cluster, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) for cluster in clusters),
    }
    contents.update(lumps or {})
    header = bytearray(b"IBSP" + struct.pack("<i", version))
    body = bytearray()
    offset = lights.IBSP_HEADER_BYTES
    for index in range(lights.IBSP_LUMPS):
        payload = contents.get(index, b"")
        header += struct.pack("<ii", offset, len(payload))
        body += payload
        offset += len(payload)
    return bytes(header + body)


WORLD = {"classname": "worldspawn"}


def light(**keys) -> dict:
    return {"classname": "light", "origin": "8 0 0", **keys}


def read(entities) -> list[lights.Light]:
    return lights.read_lights(lights.parse_entities(entity_text(entities)[:-1].decode("latin-1")))[0]


def decode(payload: bytes) -> dict:
    def refuse_duplicates(pairs):
        keys = [key for key, _ in pairs]
        assert len(keys) == len(set(keys)), f"duplicate key in {keys}"
        return dict(pairs)
    return json.loads(payload, object_pairs_hook=refuse_duplicates)


class MapTests(unittest.TestCase):
    def test_malformed_maps_are_refused(self) -> None:
        good = ibsp_map([WORLD, light()])
        lump = lambda index: 8 + index * 8
        cases = [
            ("not an IBSP map", b"FAKK" + good[4:]),
            ("expected RTCW's IBSP 47, got version 46", ibsp_map([WORLD], version=46)),
            ("truncated IBSP header", good[:100]),
            ("outside the file", good[:lump(3)] + struct.pack("<i", len(good)) + good[lump(3) + 4:]),
            ("node lump is not a whole number", good[:lump(3) + 4] + struct.pack("<i", 35) + good[lump(3) + 8:]),
            ("names a plane out of range", ibsp_map([WORLD], lumps={3: struct.pack("<3i6i", 1, -1, -2, *[0] * 6)})),
            ("names a child out of range", ibsp_map([WORLD], lumps={3: struct.pack("<3i6i", 0, -1, -3, *[0] * 6)})),
            ("has no BSP tree", ibsp_map([WORLD], lumps={3: b""})),
            ("plane is not finite", ibsp_map([WORLD], lumps={2: struct.pack("<4f", math.nan, 0, 0, 0)})),
            ("tree does not end in a leaf", ibsp_map([WORLD, light()], lumps={3: struct.pack("<3i6i", 0, 0, 0, *[0] * 6)})),
            ("repeats a key", ibsp_map([WORLD, [("classname", "light"), ("light", "1"), ("light", "2")]])),
        ]
        for message, data in cases:
            with self.subTest(message):
                with self.assertRaisesRegex(ValueError, re.escape(message)):
                    lights.convert_map("test", "rtcw-sp", data)

    def test_entity_lump_is_parsed_strictly(self) -> None:
        cases = [
            ('{ "a" "b" { }', "nested entity block"),
            ('{ "a" }', "unbalanced entity block"),
            ('}', "unbalanced entity block"),
            ('"a" "b"', "entity key outside a block"),
            ('{ "a" "b', "unterminated entity string"),
            ('{ "a\n" "b" }', "entity string spans a line"),
            ('{ a b }', "unexpected 'a'"),
            ('{ "a" "b"', "unterminated entity block"),
            ('  ', "holds no entity"),
        ]
        for text, message in cases:
            with self.subTest(text):
                with self.assertRaisesRegex(ValueError, re.escape(message)):
                    lights.parse_entities(text)
        self.assertEqual(lights.parse_entities('{\n"classname" "worldspawn"\n}\n{ "a" "" }'),
                         [[("classname", "worldspawn")], [("a", "")]])

    def test_numbers_are_read_as_the_compiler_reads_them(self) -> None:
        self.assertEqual(lights.atof(",25"), 0.0)
        self.assertEqual(lights.atof("0,2"), 0.0)
        self.assertEqual(lights.atof(" .25x"), 0.25)
        self.assertEqual(lights.atof("1e999"), 0.0)
        self.assertEqual(lights.atof(None), 0.0)
        self.assertEqual(lights.atoi("19.5"), 19)
        self.assertEqual(lights.vector("1 2"), (1.0, 2.0, 0.0))


class KeyTests(unittest.TestCase):
    def test_keys_follow_the_light_compiler(self) -> None:
        found = read([
            WORLD,
            light(),
            light(light="20", _light="50", _color="0.5 0.25 0", fade=".2"),
            light(light="0", _color="0 0 0", fade=",25"),
            light(spawnflags="3", fade="0,2"),
            {"classname": "LIGHT_fluoro", "origin": "1 2 3", "spawnflags": "2"},
            {"classname": "lightJunior", "origin": "0 0 0"},
            {"classname": "dlight", "origin": "0 0 0"},
        ])
        self.assertEqual([entry.entity for entry in found], [1, 2, 3, 4, 5])
        default, coloured, black, nonlinear, fluoro = found
        self.assertEqual((default.intensity, default.color, default.fade, default.kind), (300.0, (1.0, 1.0, 1.0), 1.0, "linear"))
        self.assertEqual((coloured.intensity, coloured.color, coloured.fade), (50.0, (1.0, 0.5, 0.0), 0.2))
        self.assertEqual((black.intensity, black.color, black.fade), (300.0, (1.0, 1.0, 1.0), 1.0))
        self.assertEqual((nonlinear.kind, nonlinear.fade), ("inverseSquare", 1.0))
        self.assertEqual((fluoro.origin, fluoro.kind), ((1.0, 2.0, 3.0), "linear"))

    def test_spotlights_aim_at_the_first_entity_named(self) -> None:
        found = read([
            WORLD,
            light(target="t1", fade=".2"),
            light(target="t1", radius="128", spawnflags="2"),
            light(target="none"),
            light(target="here"),
            {"classname": "info_null", "targetname": "t1", "origin": "8 0 -100"},
            {"classname": "info_null", "targetname": "t1", "origin": "8 0 100"},
            {"classname": "info_null", "targetname": "here", "origin": "8 0 0"},
        ])
        aimed, wide, missing, degenerate = found
        self.assertEqual((aimed.kind, aimed.spot_direction, aimed.fade), ("spot", (0.0, 0.0, -1.0), 1.0))
        self.assertAlmostEqual(aimed.spot_radius_by_distance, 80.0 / 100.0)
        self.assertAlmostEqual(wide.spot_radius_by_distance, 144.0 / 100.0)
        self.assertEqual(wide.kind, "spot")
        self.assertEqual((missing.kind, missing.target_missing), ("linear", True))
        self.assertEqual(degenerate.spot_direction, (0.0, 0.0, 0.0))
        self.assertAlmostEqual(degenerate.spot_radius_by_distance, 80.0 / 64.0)

    def test_exclusions(self) -> None:
        entities = [
            WORLD,
            light(),
            light(spawnflags="4"),
            light(spawnflags="8"),
            light(spawnflags="17"),
            light(style="32"),
            light(target="sky", _sun="1"),
            light(target="here"),
            light(origin="-8 0 0"),
            light(origin="72.5 0 0"),
            light(origin="72 0 0"),
            light(light="40", fade="2.5"),
            {"classname": "info_null", "targetname": "sky", "origin": "8 0 -64"},
            {"classname": "info_null", "targetname": "here", "origin": "8 0 0"},
            {"classname": "dlight", "origin": "136.5 0 0"},
        ]
        found = read(entities)
        data = ibsp_map(entities, clusters=(3, -1))
        index = lights.ClusterIndex(data, lights.ibsp_lumps(data))
        cgame = lights.read_lights(lights.parse_entities(entity_text(entities)[:-1].decode("latin-1")))[2]
        self.assertEqual(cgame, [(136.5, 0.0, 0.0)])
        reasons = [lights.exclusion(entry, index.cluster(entry.origin), cgame) for entry in found]
        self.assertEqual(reasons, [None, "negative", "negative", "nonDynamic", "styled", "sun",
                                   "spotWithoutDirection", "outsideClusters", "atCgameLight", None, "unlit"])


class ConversionTests(unittest.TestCase):
    def convert(self, profile, *entities):
        payload, summary = lights.convert_map("test", profile, ibsp_map([WORLD, *entities]))
        return decode(payload), summary

    def test_retail_curves_follow_the_compiler(self) -> None:
        level = lights.retail_level
        # Linear: 0.9375 * light - fade * d, from the 16-unit minimum distance,
        # zero at 0.9375 * light / fade.
        self.assertAlmostEqual(level(True, 30, 0.2, 0.0), 28.125 - 3.2)
        self.assertAlmostEqual(level(True, 30, 0.2, 100.0), 28.125 - 20.0)
        self.assertEqual(level(True, 30, 0.2, 141.0), 0.0)
        self.assertAlmostEqual(lights.retail_extent(True, 30, 0.2), 140.625)
        # Inverse-square: 7500 * light / d^2, ending where it gives one level.
        self.assertAlmostEqual(level(False, 100, 1.0, 250.0), 12.0)
        self.assertAlmostEqual(lights.retail_extent(False, 100, 1.0), math.sqrt(750000))
        # RTCW draws a lightmap level of 128 as bright as it draws anything.
        self.assertEqual(level(True, 300, 1.0, 0.0), 128.0)
        self.assertEqual(level(False, 100, 1.0, 16.0), 128.0)

    def test_the_drawn_falloff_is_the_renderers(self) -> None:
        falloff = lights.drawn_falloff
        # smoothstep(1 - d^2 / r^2) of the scaled radius; a spot is lit along its segment.
        self.assertEqual(falloff(0.0, 100.0, False, 0.5), 1.0)
        self.assertAlmostEqual(falloff(25.0, 100.0, False, 0.5), 0.75 ** 2 * 1.5)
        self.assertEqual(falloff(50.0, 100.0, False, 0.5), 0.0)
        self.assertEqual(falloff(100.0, 100.0, True, 0.5), 1.0)
        self.assertAlmostEqual(falloff(125.0, 100.0, True, 0.5), 0.75 ** 2 * 1.5)
        self.assertEqual(falloff(150.0, 100.0, True, 0.5), 0.0)

    def test_the_fit_matches_retail_in_both_modes(self) -> None:
        cases = [(True, 30.0, 0.2, False), (True, 300.0, 1.0, False), (False, 100.0, 1.0, False),
                 (False, 300.0, 1.0, True)]
        for linear, intensity, fade, spot in cases:
            extent = lights.retail_extent(linear, intensity, fade)
            distances = [extent * (index + 0.5) / 200 for index in range(200)]
            wanted = [lights.retail_level(linear, intensity, fade, d) / 128 for d in distances]
            fits = {}
            for profile, scale in lights.DLIGHT_SCALE.items():
                with self.subTest(linear=linear, intensity=intensity, spot=spot, profile=profile):
                    brightness, radius = lights.fitted(linear, intensity, fade, spot, scale)
                    self.assertTrue(0 < brightness <= 1)
                    drawn = [brightness * lights.drawn_falloff(d, radius, spot, scale) for d in distances]
                    error = sum(d * (w - f) ** 2 for d, w, f in zip(distances, wanted, drawn))
                    total = sum(d * w * w for d, w in zip(distances, wanted))
                    # Points follow retail closely; a spot's segment less so.
                    self.assertLess(error / total, 0.25 if spot else 0.13)
                    # No reach or brightness either side of the fit does better.
                    for other in (radius * 0.97, radius * 1.03):
                        worse = [brightness * lights.drawn_falloff(d, other, spot, scale) for d in distances]
                        self.assertGreaterEqual(sum(d * (w - f) ** 2 for d, w, f in zip(distances, wanted, worse)),
                                                error)
                    fits[profile] = (brightness, radius)
            if not spot:
                # A point is the same light in both modes: MP's file radius is
                # twice SP's, since MP draws radii at half scale.
                self.assertAlmostEqual(fits["rtcw-sp"][0], fits["rtcw-mp"][0], places=9)
                self.assertAlmostEqual(fits["rtcw-mp"][1], 2 * fits["rtcw-sp"][1], places=6)
        # Pinned: a dim linear fill light and the default inverse-square light.
        brightness, radius = lights.fitted(True, 30.0, 0.2, False, 1.0)
        self.assertEqual((round(brightness, 3), round(radius, 1)), (0.147, 138.6))
        brightness, radius = lights.fitted(False, 100.0, 1.0, False, 1.0)
        self.assertEqual((round(brightness, 3), round(radius, 1)), (0.895, 185.3))

    def test_document(self) -> None:
        document, summary = self.convert(
            "rtcw-mp", light(origin="8 -16.5 0.25", _color="1 0.5 0.25"),
            light(light="30", fade=".2", _color="0 0.5 1"),
            light(light="300", target="t", radius="36"),
            {"classname": "info_null", "targetname": "t", "origin": "8 0 -100"},
            {"classname": "lightJunior", "origin": "0 0 0"}, {"classname": "corona", "origin": "0 0 0"})
        self.assertEqual({key: document[key] for key in ("version", "map", "profile", "dlightScale")},
                         {"version": 1, "map": "test", "profile": "rtcw-mp", "dlightScale": 0.5})
        point, dim, spot = document["lights"]
        # MP draws radii at half scale, so a file radius is twice the drawn one.
        self.assertEqual(point, {"name": "test-light-0001", "type": "point", "origin": [8, -16.5, 0.25],
                                 "color": [1, 0.5, 0.25], "intensity": 615.1, "radius": 615.1, "castsShadows": True})
        self.assertEqual((dim["color"], dim["radius"]), ([0, 0.0737, 0.1474], 277.2))
        self.assertEqual((spot["type"], spot["direction"], spot["color"], spot["radius"], spot["intensity"]),
                         ("spot", [0, 0, -1], [0.7914] * 3, 162, 162))
        self.assertEqual(spot["outerAngle"], round(math.degrees(math.atan(52 / 100)), 2))
        self.assertEqual(summary["classes"], {"corona": 1, "light": 3, "lightJunior": 1})
        self.assertEqual(summary["usableKinds"], {"linear": 2, "spot": 1})

    def test_numbers_are_fixed_point(self) -> None:
        self.assertEqual(lights.number(0.000004, 5), "0")
        self.assertEqual(lights.number(-0.000004, 5), "0")
        self.assertEqual(lights.number(12000.0, 1), "12000")
        self.assertEqual(lights.number(1e20, 1), "100000000000000000000")
        payload, _ = lights.convert_map("test", "rtcw-sp", ibsp_map([WORLD, light(light="0.001", fade="1000")] +
                                                                    [light(light="1e6", spawnflags="1")]))
        self.assertIsNone(re.search(rb"\d[eE][-+]?\d", payload))

    def test_selection_spreads_across_clusters(self) -> None:
        entities = [WORLD] + [light(origin=f"8 {i} 0", light=str(value), spawnflags="1")
                              for i, value in enumerate((900, 800, 700, 600))]
        entities += [light(origin="-8 0 0", light="20", spawnflags="1"),
                     light(origin="-8 1 0", light="10", spawnflags="1")]
        found = read(entities)
        clusters = {entry.entity: (4 if entry.origin[0] > 0 else 5) for entry in found}
        pick = lambda limit: [entry.entity for entry in lights.select_lights(found, clusters, "rtcw-sp", limit)]
        # One per cluster first, strongest first, then each cluster's second.
        self.assertEqual(pick(1), [1])
        self.assertEqual(pick(2), [1, 5])
        self.assertEqual(pick(3), [1, 2, 5])
        self.assertEqual(pick(4), [1, 2, 5, 6])
        self.assertEqual(pick(10), [1, 2, 3, 4, 5, 6])

    def test_the_renderers_limit_is_never_exceeded(self) -> None:
        entities = [WORLD] + [light(origin=f"{8 if i % 2 else -8} {i} 0", light=str(100 + i)) for i in range(300)]
        data = ibsp_map(entities)
        payload, summary = lights.convert_map("big", "rtcw-sp", data)
        document = decode(payload)
        self.assertEqual(len(document["lights"]), lights.MAX_STATIC_MAP_LIGHTS)
        self.assertEqual((summary["usable"], summary["dropped"]), (300, 300 - lights.MAX_STATIC_MAP_LIGHTS))
        self.assertEqual(summary["selectedClusters"], 2)
        self.assertEqual(payload, lights.convert_map("big", "rtcw-sp", data)[0])


class PublicationTests(unittest.TestCase):
    MAPS = {"rtcw-sp": ("escape1",), "rtcw-mp": ("mp_ice",)}

    def _probe(self, *, fail: bool) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            main = root / "retail" / "Main"
            main.mkdir(parents=True)
            with zipfile.ZipFile(main / "pak0.pk3", "w") as archive:
                archive.writestr("maps/escape1.bsp", ibsp_map([WORLD, light()]))
            with zipfile.ZipFile(main / "mp_pakmaps1.pk3", "w") as archive:
                archive.writestr("maps/mp_ice.bsp", b"broken" if fail else ibsp_map([WORLD, light(light="50")]))
            output = root / "pkg" / "games"
            old = output / "rtcw-sp" / "main" / "maps" / "escape1.lights.json"
            old.parent.mkdir(parents=True)
            old.write_bytes(b"old sidecar")
            custom = old.with_name("custom.lights.json")
            custom.write_bytes(b"custom sidecar")
            args = argparse.Namespace(retail_path=main.parent, work_root=root / ".tmp" / "work", output_root=output)
            with patch.object(lights, "ROOT", root), patch.object(lights, "STOCK_RTCW_MAPS", self.MAPS), \
                 patch.object(generate_stock_rtcw_audio_zones, "STOCK_RTCW_MAPS", self.MAPS):
                if fail:
                    with self.assertRaisesRegex(ValueError, "existing sidecars preserved"):
                        lights.generate_sidecars(args)
                    self.assertEqual(old.read_bytes(), b"old sidecar")
                    self.assertFalse((output / "rtcw-mp").exists())
                else:
                    lights.generate_sidecars(args)
                    self.assertEqual(decode(old.read_bytes())["profile"], "rtcw-sp")
                    mp = decode((output / "rtcw-mp" / "main" / "maps" / "mp_ice.lights.json").read_bytes())
                    self.assertEqual((mp["profile"], mp["lights"][0]["radius"]), ("rtcw-mp", 94.6))
            self.assertEqual(custom.read_bytes(), b"custom sidecar")
            self.assertEqual(list(output.rglob(".lights-*")), [])
            manifest = json.loads((args.work_root / "rtcw-lights.json").read_text(encoding="utf-8"))
            self.assertEqual([run["status"] for run in manifest["runs"]], ["passed", "failed" if fail else "passed"])

    def test_failure_preserves_existing_files(self) -> None:
        self._probe(fail=True)

    def test_success_publishes_both_modes_without_pruning(self) -> None:
        self._probe(fail=False)


class PackageValidationTests(unittest.TestCase):
    """The release validator reads every packaged light file as the renderers would."""

    GOOD = (b'{\n  "version": 1,\n  "map": "m",\n  "lights": [\n'
            b'    {"name": "a", "type": "point", "origin": [1, 2, 3], "color": [1, 0.5, 0], "radius": 300},\n'
            b'    {"name": "b", "type": "spot", "origin": [0, 0, 0], "direction": [0, 0, -1], "radius": 8192}\n'
            b'  ]\n}\n')

    def test_accepts_a_well_formed_file(self) -> None:
        root_archive.validate_static_light_file("games/rtcw-sp/main/maps/m.lights.json", self.GOOD)

    def test_refuses_what_a_renderer_would_reject_or_cut_short(self) -> None:
        light = '{"type": "point", "origin": [0, 0, 0], "radius": 10}'
        cases = [
            ("over 262144 bytes", self.GOOD + b" " * (256 * 1024)),
            ("contains a NUL", self.GOOD.replace(b"\n}", b"\0}")),
            ("65536 tokens or more", b'{"version": 1, "lights": [], "pad": [' + b"0," * 40000 + b"0]}"),
            ("with an exponent", self.GOOD.replace(b"300", b"3e2")),
            ("repeats a key", self.GOOD.replace(b'"name": "a"', b'"name": "a", "name": "c"')),
            ("non-finite number", self.GOOD.replace(b"300", b"NaN")),
            ("is not JSON", self.GOOD[:-3]),
            ("not a version 1", self.GOOD.replace(b'"version": 1', b'"version": 2')),
            ("at most 128 lights", ('{"version": 1, "lights": [' + ", ".join([light] * 129) + "]}").encode()),
            ("not a point or spot", self.GOOD.replace(b'"point"', b'"area"')),
            ("no origin", self.GOOD.replace(b'"origin": [1, 2, 3], ', b"")),
            ("malformed colour", self.GOOD.replace(b"[1, 0.5, 0]", b"[1, 0.5]")),
            ("spot without a direction", self.GOOD.replace(b"[0, 0, -1]", b"[0, 0, 0]")),
            ("neither a positive radius", self.GOOD.replace(b'"radius": 300', b'"radius": 0')),
            ("neither a positive radius", self.GOOD.replace(b'"radius": 300', b'"radius": "300"')),
        ]
        for message, payload in cases:
            with self.subTest(message):
                with self.assertRaisesRegex(ValueError, re.escape(message)):
                    root_archive.validate_static_light_file("m.lights.json", payload)

    def test_the_release_validator_reads_packaged_light_files(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            archive_path = Path(tmp) / "fntech3-pkg.fnz"
            names = root_archive.required_root_archive_names()
            with zipfile.ZipFile(archive_path, "w") as archive:
                for name in names:
                    archive.writestr(name, self.GOOD if name.endswith(".lights.json") else b"x")
            root_archive.validate_root_archive(archive_path)
            broken = next(name for name in names if name.startswith("games/rtcw-mp/") and name.endswith(".lights.json"))
            with zipfile.ZipFile(archive_path, "w") as archive:
                for name in names:
                    archive.writestr(name, b"{}" if name == broken else self.GOOD if name.endswith(".lights.json") else b"x")
            with self.assertRaisesRegex(ValueError, re.escape(f"{broken}: is not a version 1 light file")):
                root_archive.validate_root_archive(archive_path)


class ShippedLightTests(unittest.TestCase):
    """The shipped files, against their record and what every renderer's parser accepts."""

    # rendererrtx/tr_bsp.c STATIC_MAP_LIGHTS_SIDECAR_MAX_BYTES and _MAX_TOKENS
    MAX_BYTES = 256 * 1024
    MAX_TOKENS = 65536

    def test_shipped_files_match_their_record_and_the_parsers(self) -> None:
        evidence = json.loads(RECORD.read_text(encoding="utf-8"))
        records = {(run["profile"], run["map"]): run for run in evidence["generation"]["runs"]}
        self.assertEqual(set(records), {(profile, name) for profile, names in STOCK_RTCW_MAPS.items()
                                        for name in names})
        total = 0
        for profile, names in STOCK_RTCW_MAPS.items():
            directory = ROOT / "pkg/games" / profile / "main/maps"
            self.assertEqual({p.name[:-len(".lights.json")] for p in directory.glob("*.lights.json")}, set(names))
            for name in names:
                with self.subTest(profile=profile, map=name):
                    payload = (directory / f"{name}.lights.json").read_bytes()
                    record = records[profile, name]
                    self.assertEqual(hashlib.sha256(payload).hexdigest(), record["lightsSha256"])
                    self.assertLessEqual(len(payload), self.MAX_BYTES)
                    self.assertNotIn(b"\0", payload)
                    self.assertNotIn(b"\r", payload)
                    tokens = re.findall(rb'"[^"]*"|[{}\[\],:]|[^\s{}\[\],:"]+', payload)
                    self.assertLess(len(tokens), self.MAX_TOKENS)
                    self.assertIsNone(re.search(rb"\d[eE][-+]?\d", payload))
                    document = decode(payload)
                    self.assertEqual((document["version"], document["map"], document["profile"]), (1, name, profile))
                    self.assertEqual(document["dlightScale"], lights.DLIGHT_SCALE[profile])
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
                        self.assertTrue(1 <= entry["radius"] <= 8192)
                        self.assertEqual(entry["intensity"], entry["radius"])
                        if spot:
                            self.assertAlmostEqual(math.hypot(*entry["direction"]), 1.0, places=3)
                            self.assertTrue(0 < entry["outerAngle"] < 90)
                    total += len(entries)
        self.assertEqual(total, evidence["generation"]["summary"]["selected"])


if __name__ == "__main__":
    unittest.main()
