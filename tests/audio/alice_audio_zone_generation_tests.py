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
import alice_retail_maps
import generate_stock_alice_audio_zones as generator
from stock_alice_maps import STOCK_ALICE_MAPS

FIXTURE_MAPS = ("garden1", "keep")


def write_pak(path: Path, entries: dict[str, bytes]) -> None:
    with zipfile.ZipFile(path, "w") as archive:
        for name, data in entries.items():
            archive.writestr(name, data)


class AliceAudioZoneGenerationTests(unittest.TestCase):
    def test_official_paks_only_and_the_game_directory_is_found(self) -> None:
        with tempfile.TemporaryDirectory() as tmp, \
             patch.object(alice_retail_maps, "STOCK_ALICE_MAPS", FIXTURE_MAPS):
            game = Path(tmp) / "base"
            game.mkdir()
            write_pak(game / "pak0.pk3", {"maps/garden1.bsp": b"garden1", "maps/keep.bsp": b"keep"})
            write_pak(game / "Pak2.pk3", {"sound/x.wav": b"not a map"})
            # A custom package named like the retail ones is not official.
            write_pak(game / "pak6_custom.pk3", {"maps/custom.bsp": b"custom"})
            write_pak(game / "zz-custom.pk3", {"maps/keep.bsp": b"override"})
            self.assertEqual(alice_retail_maps.resolve_game_directory(Path(tmp)), game)
            self.assertEqual(alice_retail_maps.resolve_game_directory(game), game)
            maps = alice_retail_maps.discover_official_maps(game)
            self.assertEqual(maps["garden1"], (game / "pak0.pk3", "maps/garden1.bsp"))
            self.assertEqual(maps["keep"][0].name, "pak0.pk3")

    def test_inventory_mismatch_and_unsafe_entries_are_rejected(self) -> None:
        for entries, message in (
            ({"maps/../keep.bsp": b"x"}, "unsafe BSP"),
            ({"maps/keep.bsp": b"x", "MAPS/KEEP.BSP": b"y", "maps/garden1.bsp": b"z"}, "case-ambiguous"),
            ({"maps/keep.bsp": b"x"}, "missing="),
            ({"maps/keep.bsp": b"x", "maps/garden1.bsp": b"x", "maps/extra.bsp": b"x"}, "unexpected="),
        ):
            with self.subTest(entries=entries), tempfile.TemporaryDirectory() as tmp:
                game = Path(tmp)
                write_pak(game / "pak0.pk3", entries)
                with patch.object(alice_retail_maps, "STOCK_ALICE_MAPS", FIXTURE_MAPS):
                    with self.assertRaisesRegex(ValueError, message):
                        alice_retail_maps.discover_official_maps(game)

    def test_generation_failure_preserves_existing_sidecars_and_cleans_inputs(self) -> None:
        self._run_publication_probe(fail=True)

    def test_success_publishes_every_map_without_pruning_custom_sidecars(self) -> None:
        self._run_publication_probe(fail=False)

    def _run_publication_probe(self, *, fail: bool) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game = root / "retail" / "base"
            game.mkdir(parents=True)
            write_pak(game / "pak0.pk3", {"maps/garden1.bsp": b"garden1", "maps/keep.bsp": b"keep"})
            tool = root / "compiler.exe"
            tool.write_bytes(b"test compiler")
            material_map = root / "materials.txt"
            material_map.write_text("bark soft weight 1\n", encoding="utf-8")
            output = root / "pkg" / "games"
            old = output / "alice" / "base" / "maps" / "garden1.azb"
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
                    # Alice's own flag rules and its material map are always asked for.
                    self.assertEqual(cmd[cmd.index("--profile") + 1], "alice")
                    self.assertEqual(Path(cmd[cmd.index("--material-map") + 1]), material_map.resolve())
                    generated = Path(cmd[cmd.index("-o") + 1])
                    generated.write_bytes(generated.stem.encode("ascii"))
                    return subprocess.CompletedProcess(cmd, 0, "generated", "")
                self.assertIn("--audit", cmd)
                code = 1 if fail and "keep" in cmd[-1] else 0
                return subprocess.CompletedProcess(cmd, code, "warnings none", "")

            with patch.object(generator, "ROOT", root), \
                 patch.object(generator, "STOCK_ALICE_MAPS", FIXTURE_MAPS), \
                 patch.object(alice_retail_maps, "STOCK_ALICE_MAPS", FIXTURE_MAPS), \
                 patch.object(generator.sweep, "run_command", side_effect=command):
                if fail:
                    with self.assertRaisesRegex(ValueError, "existing sidecars preserved"):
                        generator.generate_sidecars(args)
                    self.assertEqual(old.read_bytes(), b"old sidecar")
                    self.assertFalse((old.parent / "keep.azb").exists())
                else:
                    manifest = generator.generate_sidecars(args)
                    self.assertEqual(old.read_bytes(), b"garden1")
                    self.assertEqual((old.parent / "keep.azb").read_bytes(), b"keep")
                    self.assertEqual(manifest["materialMapSha256"],
                                     hashlib.sha256(material_map.read_bytes()).hexdigest())
            self.assertEqual(custom.read_bytes(), b"custom sidecar")
            self.assertEqual(list(args.work_root.rglob("*.bsp")), [])
            self.assertEqual(list(args.work_root.glob("run-*")), [])
            self.assertTrue((args.work_root / "audio-zone-sweep.json").is_file())

    def test_published_inventory_and_hashes_match_generation_evidence(self) -> None:
        evidence = json.loads((ROOT / "docs/fntech3/2026-09-24-alice-map-sidecars.json").read_text())
        zones = evidence["audioZones"]
        records = {run["map"]: run for run in zones["maps"]}
        self.assertEqual(len(STOCK_ALICE_MAPS), 36)
        self.assertEqual(set(records), set(STOCK_ALICE_MAPS))
        self.assertEqual(zones["materialMapSha256"],
                         hashlib.sha256((ROOT / "scripts/alice-audio-materials.txt").read_bytes()).hexdigest())
        directory = ROOT / "pkg/games/alice/base/maps"
        self.assertEqual({p.stem for p in directory.glob("*.azb")}, set(STOCK_ALICE_MAPS))
        for name in STOCK_ALICE_MAPS:
            data = (directory / f"{name}.azb").read_bytes()
            record = records[name]
            self.assertEqual(record["pak"], "pak0.pk3")
            self.assertEqual(hashlib.sha256(data).hexdigest(), record["azbSha256"])
            self.assertEqual(data[:4], b"FQAZ")
            self.assertEqual(struct.unpack_from("<I", data, 4)[0], 3)
            self.assertEqual(len(data), record["azbBytes"])
            self.assertTrue(0 < record["zones"] <= 512)
        self.assertEqual(zones["reproducibility"], {"runs": 4, "compilerBuilds": 2, "byteIdenticalSidecars": 36})


if __name__ == "__main__":
    unittest.main()
