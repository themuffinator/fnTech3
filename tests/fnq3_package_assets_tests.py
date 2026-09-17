"""Keep the reviewed FnQuake3 package import complete and byte-identifiable."""
import hashlib
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from root_archive import DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS
from stock_q3_assets import Q3_PACKAGE_ASSETS


class FnQuake3PackageAssetsTests(unittest.TestCase):
    def test_complete_reference_package_is_preserved(self):
        evidence = json.loads((ROOT / "docs/fntech3/2026-09-17-fnq3-package-assets.json").read_text())
        self.assertEqual(len(evidence["assets"]), 92)
        self.assertEqual(len(Q3_PACKAGE_ASSETS), 92)
        self.assertEqual(tuple(sorted(set(Q3_PACKAGE_ASSETS))), Q3_PACKAGE_ASSETS)
        expected = {asset["packagePath"] for asset in evidence["assets"]}
        actual = {p.relative_to(ROOT / "pkg").as_posix()
                  for p in (ROOT / "pkg/games/quake3").rglob("*") if p.is_file()}
        self.assertEqual(actual, expected)
        required = {dest.as_posix() for _source, dest in DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS}
        self.assertTrue(expected.issubset(required))
        for asset in evidence["assets"]:
            with self.subTest(path=asset["packagePath"]):
                payload = (ROOT / "pkg" / asset["packagePath"]).read_bytes()
                if asset["hashEncoding"] == "LF-normalized UTF-8":
                    payload = payload.replace(b"\r\n", b"\n")
                self.assertEqual(hashlib.sha256(payload).hexdigest(), asset["sha256"])

    def test_game_and_missionpack_inventory_is_explicit(self):
        self.assertEqual(sum(name.endswith(".azb") for name in Q3_PACKAGE_ASSETS), 35)
        self.assertEqual(sum(name.endswith(".fog") for name in Q3_PACKAGE_ASSETS), 54)
        self.assertEqual(sum(name.startswith("missionpack/maps/") for name in Q3_PACKAGE_ASSETS), 20)
        self.assertIn("baseq3/sound/fnql-weapon-sounds.sndshd", Q3_PACKAGE_ASSETS)
        self.assertIn("missionpack/sound/fnql-weapon-sounds.sndshd", Q3_PACKAGE_ASSETS)
        self.assertIn("baseq3/fnq3-hud.json", Q3_PACKAGE_ASSETS)


if __name__ == "__main__":
    unittest.main()
