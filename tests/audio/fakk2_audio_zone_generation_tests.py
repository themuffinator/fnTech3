"""Heavy Metal: F.A.K.K.2's audio zone files: the inventory, the generator's
publication rules, and the shipped files against their generation record. No
retail map is read; the compiler's FAKK 12 handling is in
audio_zone_material_map_tests.py's fakk2 cases."""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import fakk2_retail_maps
import generate_stock_fakk2_audio_zones as generator
from stock_fakk2_maps import STOCK_FAKK2_MAPS

FIXTURE_MAPS = ("fakkhouse", "zoo")


def write_pak(path: Path, entries: dict[str, bytes]) -> None:
    with zipfile.ZipFile(path, "w") as archive:
        for name, data in entries.items():
            archive.writestr(name, data)


def lf_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


class Fakk2AudioZoneGenerationTests(unittest.TestCase):
    def test_official_paks_only_and_the_game_directory_is_found(self) -> None:
        with tempfile.TemporaryDirectory() as tmp, \
             patch.object(fakk2_retail_maps, "STOCK_FAKK2_MAPS", FIXTURE_MAPS):
            game = Path(tmp).resolve() / "fakk"
            game.mkdir()
            write_pak(game / "pak0.pk3", {"maps/fakkhouse.bsp": b"fakkhouse", "maps/zoo.bsp": b"zoo"})
            write_pak(game / "PAK1.pk3", {"ui/main.urc": b"not a map"})
            # A custom package named like the retail ones is not official.
            write_pak(game / "pak2.pk3", {"maps/custom.bsp": b"custom"})
            write_pak(game / "zz-custom.pk3", {"maps/zoo.bsp": b"override"})
            self.assertEqual(fakk2_retail_maps.resolve_game_directory(Path(tmp).resolve()), game)
            self.assertEqual(fakk2_retail_maps.resolve_game_directory(game), game)
            maps = fakk2_retail_maps.discover_official_maps(game)
            self.assertEqual(maps["fakkhouse"], (game / "pak0.pk3", "maps/fakkhouse.bsp"))
            self.assertEqual(maps["zoo"][0].name, "pak0.pk3")

    def test_inventory_mismatch_and_unsafe_entries_are_rejected(self) -> None:
        for entries, message in (
            ({"maps/../zoo.bsp": b"x"}, "unsafe BSP"),
            ({"maps/zoo.bsp": b"x", "MAPS/ZOO.BSP": b"y", "maps/fakkhouse.bsp": b"z"}, "case-ambiguous"),
            ({"maps/zoo.bsp": b"x"}, "missing="),
            ({"maps/zoo.bsp": b"x", "maps/fakkhouse.bsp": b"x", "maps/extra.bsp": b"x"}, "unexpected="),
        ):
            with self.subTest(entries=entries), tempfile.TemporaryDirectory() as tmp:
                game = Path(tmp).resolve()
                write_pak(game / "pak0.pk3", entries)
                with patch.object(fakk2_retail_maps, "STOCK_FAKK2_MAPS", FIXTURE_MAPS):
                    with self.assertRaisesRegex(ValueError, message):
                        fakk2_retail_maps.discover_official_maps(game)

    def test_generation_failure_preserves_existing_sidecars_and_cleans_inputs(self) -> None:
        self._run_publication_probe(fail=True)

    def test_success_publishes_every_map_without_pruning_custom_sidecars(self) -> None:
        self._run_publication_probe(fail=False)

    def _run_publication_probe(self, *, fail: bool) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            game = root / "retail" / "fakk"
            game.mkdir(parents=True)
            write_pak(game / "pak0.pk3", {"maps/fakkhouse.bsp": b"fakkhouse", "maps/zoo.bsp": b"zoo"})
            tool = root / "compiler.exe"
            tool.write_bytes(b"test compiler")
            material_map = root / "materials.txt"
            material_map.write_text("cemwall stone weight 1\n", encoding="utf-8")
            output = root / "pkg" / "games"
            old = output / "fakk2" / "fakk" / "maps" / "fakkhouse.azb"
            old.parent.mkdir(parents=True)
            old.write_bytes(b"old sidecar")
            custom = old.with_name("custom.azb")
            custom.write_bytes(b"custom sidecar")
            args = argparse.Namespace(
                retail_path=game, tool=tool, material_map=material_map,
                work_root=root / ".tmp" / "work", output_root=output,
                samples=16, max_zones=512, strict=False,
            )

            def command(cmd: list[str]) -> subprocess.CompletedProcess[str]:
                if "--from-bsp" in cmd:
                    # F.A.K.K.2's own profile and its material map are always asked for.
                    self.assertEqual(cmd[cmd.index("--profile") + 1], "fakk2")
                    self.assertEqual(Path(cmd[cmd.index("--material-map") + 1]), material_map.resolve())
                    generated = Path(cmd[cmd.index("-o") + 1])
                    generated.write_bytes(generated.stem.encode("ascii"))
                    return subprocess.CompletedProcess(cmd, 0, "generated", "")
                self.assertIn("--audit", cmd)
                code = 1 if fail and "zoo" in cmd[-1] else 0
                return subprocess.CompletedProcess(cmd, code, "warnings none", "")

            with patch.object(generator, "ROOT", root), \
                 patch.object(generator, "STOCK_FAKK2_MAPS", FIXTURE_MAPS), \
                 patch.object(fakk2_retail_maps, "STOCK_FAKK2_MAPS", FIXTURE_MAPS), \
                 patch.object(generator.sweep, "run_command", side_effect=command):
                if fail:
                    with self.assertRaisesRegex(ValueError, "existing sidecars preserved"):
                        generator.generate_sidecars(args)
                    self.assertEqual(old.read_bytes(), b"old sidecar")
                    self.assertFalse((old.parent / "zoo.azb").exists())
                else:
                    manifest = generator.generate_sidecars(args)
                    self.assertEqual(old.read_bytes(), b"fakkhouse")
                    self.assertEqual((old.parent / "zoo.azb").read_bytes(), b"zoo")
                    self.assertEqual(manifest["profile"], "fakk2")
            self.assertEqual(custom.read_bytes(), b"custom sidecar")
            self.assertEqual(list(args.work_root.rglob("*.bsp")), [])
            self.assertEqual(list(args.work_root.glob("run-*")), [])
            self.assertTrue((args.work_root / "audio-zone-sweep.json").is_file())

    def test_published_inventory_and_hashes_match_generation_evidence(self) -> None:
        evidence = json.loads((ROOT / "docs/fntech3/2026-09-24-fakk2-map-sidecars.json").read_text())
        zones = evidence["audioZones"]
        records = {run["map"]: run for run in zones["maps"]}
        self.assertEqual(len(STOCK_FAKK2_MAPS), 30)
        self.assertEqual(set(records), set(STOCK_FAKK2_MAPS))
        self.assertEqual(zones["profile"], "fakk2")
        self.assertEqual(zones["materialMapSha256"], lf_sha256(ROOT / "scripts/fakk2-audio-materials.txt"))
        directory = ROOT / "pkg/games/fakk2/fakk/maps"
        self.assertEqual({p.stem for p in directory.glob("*.azb")}, set(STOCK_FAKK2_MAPS))
        for name in STOCK_FAKK2_MAPS:
            data = (directory / f"{name}.azb").read_bytes()
            record = records[name]
            self.assertEqual(record["pak"], "pak0.pk3")
            self.assertEqual(hashlib.sha256(data).hexdigest(), record["azbSha256"])
            self.assertEqual(data[:4], b"FQAZ")
            self.assertEqual(struct.unpack_from("<I", data, 4)[0], 3)
            self.assertEqual(len(data), record["azbBytes"])
            self.assertTrue(0 < record["zones"] <= 512)
        self.assertEqual(zones["reproducibility"]["byteIdenticalSidecars"], 30)
        self.assertEqual(zones["summary"]["passed"], 30)


if __name__ == "__main__":
    unittest.main()
