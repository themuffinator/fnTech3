"""Enemy Territory's HUD aspect correction and audio zone sidecars.

ET first took the centred uniform 4:3 correction alone, while its client
reached no further than the limbo panel. Once it reached ordinary play, its
in-game HUD was captured and given per-element rules
(docs/fntech3/2026-09-25-widescreen-hud-and-fov.md); the limbo panel stays one
centred composition. The checks pin that the correction is enabled, that the
shipped rule file is well formed, and that every official map has a sidecar.
"""
# Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
from pathlib import Path
import json
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from stock_wolfet_maps import STOCK_WOLFET_MAPS

ROOT = Path(__file__).resolve().parents[1]
GAME_DIRECTORY = "etmain"


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


class WolfETHudTests(unittest.TestCase):
    def test_the_engine_enables_the_hud_path_for_enemy_territory(self):
        hud = read("code/client/cl_hud.cpp")
        start = hud.index("hudSupported = Com_GameIs")
        enable = hud[start:hud.index(";", start)]
        self.assertIn("Com_GameIsWolfET()", enable)
        # The rules only reach the client if the root archive will serve them.
        files = read("code/qcommon/files.c")
        allowed = files[files.index("FS_RootArchiveAllowsFile"):]
        allowed = allowed[:allowed.index("FS_RootArchiveAllowsMapSidecar( qpath )")]
        self.assertIn("Com_GameIsWolfET()", allowed)
        self.assertIn('!FS_FilenameCompare( qpath, "fntech3-hud.json" )', allowed)

    def test_et_draws_already_reach_the_hud_path(self):
        # Enabling the correction is only meaningful because ET's own compat
        # layer already routes its 2D draws through it.
        compat = read("code/client/client_wolfet_render_compat.hpp")
        self.assertIn("CL_HudDrawStretchPic( VMF(1), VMF(2), VMF(3), VMF(4)", compat)
        self.assertIn("CL_HudAdjustPic( &x, &y, &w, &h, args[9] )", compat)

    def test_the_shipped_rule_file_is_well_formed(self):
        document = json.loads(read(f"pkg/games/wolfet/{GAME_DIRECTORY}/fntech3-hud.json"))
        self.assertEqual(document["version"], 1)
        names = [rule["name"] for rule in document["rules"]]
        self.assertTrue(names, "the in-game HUD has rules")
        self.assertEqual(len(names), len(set(names)), "rule names must be unique")
        for rule in document["rules"]:
            with self.subTest(rule=rule["name"]):
                self.assertIn(rule["mode"], {"uniform", "extend"})
                self.assertLessEqual(set(rule["align"]), {"x", "y"})
                self.assertTrue(rule["match"], "a rule that matches everything is not useful")
                self.assertTrue(set(rule["match"]) <= {"shader", "textLike", "refdef", "region"})


class WolfETAudioZoneTests(unittest.TestCase):
    def test_every_official_map_has_a_sidecar(self):
        self.assertEqual(len(STOCK_WOLFET_MAPS), 6)
        root = ROOT / "pkg/games/wolfet" / GAME_DIRECTORY / "maps"
        shipped = {path.stem for path in root.glob("*.azb")}
        self.assertEqual(shipped, set(STOCK_WOLFET_MAPS))

    def test_sidecars_are_parsable_and_carry_real_zones(self):
        for name in STOCK_WOLFET_MAPS:
            path = ROOT / "pkg/games/wolfet" / GAME_DIRECTORY / "maps" / f"{name}.azb"
            data = path.read_bytes()
            with self.subTest(map=name):
                self.assertGreater(len(data), 16)
                # azfmt::kMagic, then the version and zone count.
                magic, version, zones = struct.unpack_from("<4sII", data, 0)
                self.assertEqual(magic, b"FQAZ")
                self.assertIn(version, (1, 2, 3))
                self.assertGreater(zones, 0)
                # azfmt::kMaxZones; a larger count means a corrupt file.
                self.assertLessEqual(zones, 1024)

    def test_enemy_territory_keeps_its_own_material_interpretation(self):
        # ET shares RTCW's material bits except 0x40, which is a water splash
        # rather than ceramic. Elite Force's profiles must not disturb that.
        tool = read("code/tools/audiozones/audiozonesc.cpp")
        self.assertIn('profile == "wolfet" ? MaterialFlags::WolfET : ritualProfile ? MaterialFlags::Alice :', tool)
        self.assertIn("constexpr int splashSurfaces = 0x40;", tool)
        self.assertNotIn('profile == "stvef-sp" ? MaterialFlags', tool)

    def test_the_packager_requires_the_rule_file_and_every_sidecar(self):
        import root_archive
        self.assertEqual(set(root_archive.PROFILE_REQUIRED_ASSETS["wolfet"]),
                         {f"{GAME_DIRECTORY}/fntech3-hud.json"}
                         | {f"{GAME_DIRECTORY}/maps/{name}.azb" for name in STOCK_WOLFET_MAPS})


if __name__ == "__main__":
    unittest.main()
