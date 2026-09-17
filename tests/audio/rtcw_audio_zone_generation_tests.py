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
import generate_stock_rtcw_audio_zones as generator
from stock_rtcw_maps import STOCK_RTCW_MAPS

FIXTURE_MAPS = {"rtcw-sp": ("escape1",), "rtcw-mp": ("mp_beach",)}


def write_pak(path: Path, entries: dict[str, bytes]) -> None:
    with zipfile.ZipFile(path, "w") as archive:
        for name, data in entries.items():
            archive.writestr(name, data)


class RtcwAudioZoneGenerationTests(unittest.TestCase):
    def test_patches_win_and_custom_packages_are_excluded(self) -> None:
        with tempfile.TemporaryDirectory() as tmp, patch.object(generator, "STOCK_RTCW_MAPS", FIXTURE_MAPS):
            main = Path(tmp) / "Main"
            main.mkdir()
            write_pak(main / "pak0.pk3", {"maps/escape1.bsp": b"original"})
            write_pak(main / "sp_pak4.pk3", {"MAPS/ESCAPE1.BSP": b"patched"})
            write_pak(main / "mp_pak0.pk3", {"maps/mp_beach.bsp": b"mp"})
            write_pak(main / "z-custom.pk3", {"maps/escape1.bsp": b"custom", "maps/custom.bsp": b"custom"})
            self.assertEqual(generator.resolve_main_directory(Path(tmp)), main)
            self.assertEqual(generator.resolve_main_directory(main), main)
            maps = generator.discover_official_maps(main)
            self.assertEqual(maps["rtcw-sp", "escape1"], (main / "sp_pak4.pk3", "MAPS/ESCAPE1.BSP"))
            self.assertEqual(maps["rtcw-mp", "mp_beach"][0].name, "mp_pak0.pk3")

    def test_inventory_mismatch_and_unsafe_entries_are_rejected(self) -> None:
        for entries, message in (
            ({"maps/../escape1.bsp": b"x"}, "unsafe BSP"),
            ({"maps/escape1.bsp": b"x", "MAPS/ESCAPE1.BSP": b"y"}, "case-ambiguous"),
            ({"maps/escape1.bsp": b"x"}, "missing="),
            ({"maps/escape1.bsp": b"x", "maps/extra.bsp": b"x"}, "unexpected="),
        ):
            with self.subTest(entries=entries), tempfile.TemporaryDirectory() as tmp:
                main = Path(tmp)
                write_pak(main / "pak0.pk3", entries)
                with patch.object(generator, "STOCK_RTCW_MAPS", FIXTURE_MAPS):
                    with self.assertRaisesRegex(ValueError, message):
                        generator.discover_official_maps(main)

    def test_generation_failure_preserves_all_existing_sidecars_and_cleans_inputs(self) -> None:
        self._run_publication_probe(fail=True)

    def test_success_publishes_both_profiles_without_pruning_custom_sidecars(self) -> None:
        self._run_publication_probe(fail=False)

    def _run_publication_probe(self, *, fail: bool) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            main = root / "retail" / "Main"
            main.mkdir(parents=True)
            write_pak(main / "pak0.pk3", {"maps/escape1.bsp": b"sp"})
            write_pak(main / "mp_pak0.pk3", {"maps/mp_beach.bsp": b"mp"})
            tool = root / "compiler.exe"
            tool.write_bytes(b"test compiler")
            output = root / "pkg" / "games"
            old = output / "rtcw-sp" / "main" / "maps" / "escape1.azb"
            old.parent.mkdir(parents=True)
            old.write_bytes(b"old sidecar")
            custom = old.with_name("custom.azb")
            custom.write_bytes(b"custom sidecar")
            args = argparse.Namespace(
                retail_path=main, tool=tool, work_root=root / ".tmp" / "work",
                output_root=output, samples=16, max_zones=512, strict=False,
            )

            def command(cmd: list[str]) -> subprocess.CompletedProcess[str]:
                if "--from-bsp" in cmd:
                    profile = cmd[cmd.index("--profile") + 1]
                    self.assertIn(profile, FIXTURE_MAPS)
                    generated = Path(cmd[cmd.index("-o") + 1])
                    generated.write_bytes(profile.encode("ascii"))
                    return subprocess.CompletedProcess(cmd, 0, "generated", "")
                self.assertIn("--audit", cmd)
                code = 1 if fail and "rtcw-mp" in cmd[-1] else 0
                return subprocess.CompletedProcess(cmd, code, "warnings none", "")

            with patch.object(generator, "ROOT", root), \
                 patch.object(generator, "STOCK_RTCW_MAPS", FIXTURE_MAPS), \
                 patch.object(generator.sweep, "run_command", side_effect=command):
                if fail:
                    with self.assertRaisesRegex(ValueError, "existing sidecars preserved"):
                        generator.generate_sidecars(args)
                    self.assertEqual(old.read_bytes(), b"old sidecar")
                    self.assertFalse((output / "rtcw-mp").exists())
                else:
                    generator.generate_sidecars(args)
                    self.assertEqual(old.read_bytes(), b"rtcw-sp")
                    self.assertEqual((output / "rtcw-mp/main/maps/mp_beach.azb").read_bytes(), b"rtcw-mp")
            self.assertEqual(custom.read_bytes(), b"custom sidecar")
            self.assertEqual(list(args.work_root.rglob("*.bsp")), [])
            self.assertEqual(list(args.work_root.glob("run-*")), [])
            self.assertTrue((args.work_root / "audio-zone-sweep.json").is_file())

    def test_published_inventory_and_hashes_match_generation_evidence(self) -> None:
        evidence = json.loads((ROOT / "docs/fntech3/2026-09-17-rtcw-audio-zones.json").read_text())
        records = {(run["profile"], run["map"]): run for run in evidence["maps"]}
        self.assertEqual([len(STOCK_RTCW_MAPS[p]) for p in ("rtcw-sp", "rtcw-mp")], [32, 15])
        self.assertEqual(set(records), {(p, n) for p, names in STOCK_RTCW_MAPS.items() for n in names})
        for profile, names in STOCK_RTCW_MAPS.items():
            directory = ROOT / "pkg/games" / profile / "main/maps"
            self.assertEqual({p.stem for p in directory.glob("*.azb")}, set(names))
            for name in names:
                data = (directory / f"{name}.azb").read_bytes()
                record = records[profile, name]
                self.assertEqual(hashlib.sha256(data).hexdigest(), record["azbSha256"])
                self.assertEqual(data[:4], b"FQAZ")
                self.assertEqual(struct.unpack_from("<I", data, 4)[0], 3)
                self.assertEqual(len(data), record["azbBytes"])
                self.assertTrue(0 < record["audit"]["zones"] <= 512)
        for name in ("crypt1", "dam", "end"):
            self.assertEqual(records["rtcw-sp", name]["pak"], "sp_pak4.pk3")


if __name__ == "__main__":
    unittest.main()
