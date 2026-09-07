"""Validate installation evidence matching without launching a game."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("profile_probe", ROOT / "scripts/probe_game_profiles.py")
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class ProfileProbeEvidenceTests(unittest.TestCase):
    def test_requires_the_exact_mounted_base_pak(self):
        marker = ROOT / "synthetic-install/baseq3/pak0.pk3"
        self.assertTrue(probe.installation_observed(f"Current search path:\n{marker} (3539 files)\n", marker))
        self.assertFalse(probe.installation_observed(f"{marker}.backup (3539 files)\n", marker))
        self.assertFalse(probe.installation_observed(f"Missing file: {marker}\n", marker))
        self.assertFalse(probe.installation_observed(f"{marker.parent / 'pak00.pk3'} (3539 files)\n", marker))
        self.assertFalse(probe.installation_observed("", marker))


if __name__ == "__main__":
    unittest.main()
