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
import generate_stock_wolfet_audio_zones as generator
from stock_wolfet_maps import STOCK_WOLFET_MAPS

FIXTURE_MAPS = ("oasis", "radar")


def write_pak(path: Path, entries: dict[str, bytes]) -> None:
    with zipfile.ZipFile(path, "w") as archive:
        for name, data in entries.items():
            archive.writestr(name, data)


class WolfETAudioZoneGenerationTests(unittest.TestCase):
    def test_official_paks_only_and_the_game_directory_is_found(self) -> None:
        with tempfile.TemporaryDirectory() as tmp, patch.object(generator, "STOCK_WOLFET_MAPS", FIXTURE_MAPS):
            game = Path(tmp).resolve() / "etmain"
            game.mkdir()
            write_pak(game / "pak0.pk3", {"maps/oasis.bsp": b"oasis", "maps/radar.bsp": b"radar"})
            write_pak(game / "mp_bin.pk3", {"maps/custom.bsp": b"not official"})
            write_pak(game / "z-custom.pk3", {"maps/oasis.bsp": b"custom"})
            self.assertEqual(generator.resolve_game_directory(Path(tmp).resolve()), game)
            self.assertEqual(generator.resolve_game_directory(game), game)
            maps = generator.discover_official_maps(game)
            self.assertEqual(maps["oasis"], (game / "pak0.pk3", "maps/oasis.bsp"))
            self.assertEqual(maps["radar"][0].name, "pak0.pk3")

    def test_inventory_mismatch_and_unsafe_entries_are_rejected(self) -> None:
        for entries, message in (
            ({"maps/../oasis.bsp": b"x"}, "unsafe BSP"),
            ({"maps/oasis.bsp": b"x", "MAPS/OASIS.BSP": b"y", "maps/radar.bsp": b"z"}, "case-ambiguous"),
            ({"maps/oasis.bsp": b"x"}, "missing="),
            ({"maps/oasis.bsp": b"x", "maps/radar.bsp": b"x", "maps/extra.bsp": b"x"}, "unexpected="),
        ):
            with self.subTest(entries=entries), tempfile.TemporaryDirectory() as tmp:
                game = Path(tmp).resolve()
                write_pak(game / "pak0.pk3", entries)
                with patch.object(generator, "STOCK_WOLFET_MAPS", FIXTURE_MAPS):
                    with self.assertRaisesRegex(ValueError, message):
                        generator.discover_official_maps(game)

    def test_generation_failure_preserves_existing_sidecars_and_cleans_inputs(self) -> None:
        self._run_publication_probe(fail=True)

    def test_success_publishes_every_map_without_pruning_custom_sidecars(self) -> None:
        self._run_publication_probe(fail=False)

    def _run_publication_probe(self, *, fail: bool) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            game = root / "retail" / "etmain"
            game.mkdir(parents=True)
            write_pak(game / "pak0.pk3", {"maps/oasis.bsp": b"oasis", "maps/radar.bsp": b"radar"})
            tool = root / "compiler.exe"
            tool.write_bytes(b"test compiler")
            output = root / "pkg" / "games"
            old = output / "wolfet" / "etmain" / "maps" / "oasis.azb"
            old.parent.mkdir(parents=True)
            old.write_bytes(b"old sidecar")
            custom = old.with_name("custom.azb")
            custom.write_bytes(b"custom sidecar")
            args = argparse.Namespace(
                retail_path=game, tool=tool, work_root=root / ".tmp" / "work",
                output_root=output, samples=16, max_zones=512, strict=False,
            )

            def command(cmd: list[str]) -> subprocess.CompletedProcess[str]:
                if "--from-bsp" in cmd:
                    # ET's own material rules are always asked for.
                    self.assertEqual(cmd[cmd.index("--profile") + 1], "wolfet")
                    generated = Path(cmd[cmd.index("-o") + 1])
                    generated.write_bytes(generated.stem.encode("ascii"))
                    return subprocess.CompletedProcess(cmd, 0, "generated", "")
                self.assertIn("--audit", cmd)
                code = 1 if fail and "radar" in cmd[-1] else 0
                return subprocess.CompletedProcess(cmd, code, "warnings none", "")

            with patch.object(generator, "ROOT", root), \
                 patch.object(generator, "STOCK_WOLFET_MAPS", FIXTURE_MAPS), \
                 patch.object(generator.sweep, "run_command", side_effect=command):
                if fail:
                    with self.assertRaisesRegex(ValueError, "existing sidecars preserved"):
                        generator.generate_sidecars(args)
                    self.assertEqual(old.read_bytes(), b"old sidecar")
                    self.assertFalse((old.parent / "radar.azb").exists())
                else:
                    generator.generate_sidecars(args)
                    self.assertEqual(old.read_bytes(), b"oasis")
                    self.assertEqual((old.parent / "radar.azb").read_bytes(), b"radar")
            self.assertEqual(custom.read_bytes(), b"custom sidecar")
            self.assertEqual(list(args.work_root.rglob("*.bsp")), [])
            self.assertEqual(list(args.work_root.glob("run-*")), [])
            self.assertTrue((args.work_root / "audio-zone-sweep.json").is_file())

    def test_published_inventory_and_hashes_match_generation_evidence(self) -> None:
        evidence = json.loads((ROOT / "docs/fntech3/2026-09-18-wolfet-audio-zones.json").read_text())
        records = {run["map"]: run for run in evidence["maps"]}
        self.assertEqual(STOCK_WOLFET_MAPS, ("battery", "fueldump", "goldrush", "oasis", "radar", "railgun"))
        self.assertEqual(set(records), set(STOCK_WOLFET_MAPS))
        directory = ROOT / "pkg/games/wolfet/etmain/maps"
        self.assertEqual({p.stem for p in directory.glob("*.azb")}, set(STOCK_WOLFET_MAPS))
        for name in STOCK_WOLFET_MAPS:
            data = (directory / f"{name}.azb").read_bytes()
            record = records[name]
            self.assertEqual(record["profile"], "wolfet")
            self.assertEqual(record["pak"], "pak0.pk3")
            self.assertEqual(hashlib.sha256(data).hexdigest(), record["azbSha256"])
            self.assertEqual(data[:4], b"FQAZ")
            self.assertEqual(struct.unpack_from("<I", data, 4)[0], 3)
            self.assertEqual(len(data), record["azbBytes"])
            self.assertTrue(0 < record["audit"]["zones"] <= 512)
        self.assertEqual(evidence["reproducibility"], {"runs": 2, "byteIdenticalSidecars": 6})


if __name__ == "__main__":
    unittest.main()
