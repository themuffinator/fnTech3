"""Elite Force's shipped HUD aspect rules and audio zone sidecars.

The HUD rules are authored from draw groups the retail cgame modules actually
issued (tests/fixtures/stvef_hud_draws.json, captured with cl_hudDump), so the
checks replay those groups through the engine's own matching semantics: the
draw's centre point against each rule in order, first match wins. The audio
zone sidecars cover every official map, split the way the shipped arena
metadata splits them.
"""
# Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
from pathlib import Path
import fnmatch
import json
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from stock_stvef_maps import STOCK_STVEF_MAPS

ROOT = Path(__file__).resolve().parents[1]
DRAWS = json.loads((ROOT / "tests/fixtures/stvef_hud_draws.json").read_text())
PROFILES = ("stvef-sp", "stvef-mp")
GAME_DIRECTORY = "baseEF"
ALIGNMENTS = {"left", "center", "right", "top", "bottom"}


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


def rules(profile):
    return json.loads(read(f"pkg/games/{profile}/{GAME_DIRECTORY}/fntech3-hud.json"))


def find_rule(ruleset, group):
    """The engine's CL_HudFindRule: centre point, in order, first match wins."""
    cx = group["x"] + group["w"] / 2.0
    cy = group["y"] + group["h"] / 2.0
    for rule in ruleset:
        match = rule["match"]
        if "shader" in match and not fnmatch.fnmatch(group["shader"], match["shader"]):
            continue
        if "textLike" in match and bool(match["textLike"]) != bool(group["textLike"]):
            continue
        if "refdef" in match and bool(match["refdef"]) != bool(group["refdef"]):
            continue
        if "region" in match:
            region = match["region"]
            if not (region["x"] <= cx <= region["x"] + region["w"]
                    and region["y"] <= cy <= region["y"] + region["h"]):
                continue
        return rule
    return None


def matches_for(profile):
    ruleset = rules(profile)["rules"]
    resolved = []
    for group in DRAWS["profiles"][profile]["groups"]:
        resolved.append((group, find_rule(ruleset, group)))
    return ruleset, resolved


class STVEFHudRuleTests(unittest.TestCase):
    def test_every_profile_ships_a_well_formed_rule_file(self):
        for profile in PROFILES:
            with self.subTest(profile=profile):
                document = rules(profile)
                self.assertEqual(document["version"], 1)
                names = [rule["name"] for rule in document["rules"]]
                self.assertTrue(names, "a shipped rule file must carry rules")
                self.assertEqual(len(names), len(set(names)), "rule names must be unique")
                for rule in document["rules"]:
                    self.assertEqual(rule["mode"], "uniform")
                    self.assertLessEqual(set(rule["align"]), {"x", "y"})
                    self.assertLessEqual(set(rule["align"].values()), ALIGNMENTS)
                    self.assertTrue(set(rule["match"]) <= {"shader", "textLike", "refdef", "region"})
                    self.assertTrue(rule["match"], "a rule that matches everything is not useful")
                    if "region" in rule["match"]:
                        region = rule["match"]["region"]
                        self.assertGreater(region["w"], 0)
                        self.assertGreater(region["h"], 0)
                        # Rules are authored against the 640x480 virtual screen.
                        self.assertGreaterEqual(region["x"], 0)
                        self.assertGreaterEqual(region["y"], 0)
                        self.assertLessEqual(region["x"] + region["w"], 640)
                        self.assertLessEqual(region["y"] + region["h"], 480)

    def test_no_rule_is_unreachable_behind_an_earlier_one(self):
        # A rule that never wins is either dead or hidden by its predecessor.
        for profile in PROFILES:
            with self.subTest(profile=profile):
                ruleset, resolved = matches_for(profile)
                won = {rule["name"] for _, rule in resolved if rule}
                # The crosshair is absent from the single-player capture, which
                # loads a save into a scene that draws none; the rule is still
                # correct and is exercised by the Holomatch capture.
                expected = {rule["name"] for rule in ruleset}
                if profile == "stvef-sp":
                    expected -= {"crosshair"}
                self.assertEqual(won, expected)

    def test_recorded_status_cluster_is_pinned_to_its_own_corner(self):
        """Health/armour reads bottom-left; Holomatch's ammunition reads bottom-right."""
        for profile in PROFILES:
            with self.subTest(profile=profile):
                _, resolved = matches_for(profile)
                left, right = 0, 0
                for group, rule in resolved:
                    shader = group["shader"]
                    cx = group["x"] + group["w"] / 2.0
                    cy = group["y"] + group["h"] / 2.0
                    status = (shader.startswith("gfx/interface/")
                              or shader.startswith("gfx/2d/numbers/"))
                    if not status or cy < 415:
                        continue
                    self.assertIsNotNone(rule, f"{profile}: {shader} at {cx},{cy} is unruled")
                    self.assertEqual(rule["align"]["y"], "bottom")
                    if cx < 320:
                        left += 1
                        self.assertEqual(rule["align"]["x"], "left")
                    else:
                        right += 1
                        self.assertEqual(rule["align"]["x"], "right"
                                         if profile == "stvef-mp" and cx > 500 else "center")
                self.assertGreater(left, 0)
                if profile == "stvef-mp":
                    self.assertGreater(right, 0)

    def test_crosshair_stays_centred_and_loading_art_is_left_alone(self):
        ruleset, resolved = matches_for("stvef-mp")
        crosshairs = [(g, r) for g, r in resolved if g["shader"].startswith("gfx/2d/crosshair")]
        self.assertTrue(crosshairs)
        for _, rule in crosshairs:
            self.assertEqual(rule["name"], "crosshair")
            self.assertEqual(rule["align"], {"x": "center", "y": "center"})
        # The loading emblem is one composition; pinning its pieces to
        # different edges would pull it apart, so no rule may claim them.
        for profile in PROFILES:
            with self.subTest(profile=profile):
                _, groups = matches_for(profile)
                for group, rule in groups:
                    if group["shader"].startswith("menu/loading/"):
                        self.assertIsNone(rule, f"{profile}: {group['shader']} was captured by "
                                                f"{rule['name'] if rule else None}")


class STVEFPresentationWiringTests(unittest.TestCase):
    def test_the_engine_enables_the_hud_path_for_elite_force(self):
        hud = read("code/client/cl_hud.cpp")
        self.assertIn("hudSupported = Com_GameIsQuake3() || Com_GameIsRTCW() || Com_GameIsSTVEF();", hud)
        # The rules only reach the client if the root archive will serve them.
        files = read("code/qcommon/files.c")
        self.assertIn("( Com_GameIsQuake3() || Com_GameIsRTCW() || Com_GameIsSTVEF() ) &&\n"
                      "\t\t\t\t( !FS_FilenameCompare( qpath, \"fntech3-hud.json\" ) ||", files)

    def test_the_zone_compiler_knows_elite_force_is_a_quake3_fork(self):
        tool = read("code/tools/audiozones/audiozonesc.cpp")
        self.assertIn('profile != "wolfet" && profile != "stvef-sp" && profile != "stvef-mp"', tool)
        # Elite Force ships IBSP 46 and keeps Quake's surface-flag meanings, so
        # it must not be validated against Quake Live's version.
        self.assertIn('const bool profileIsBsp46 = profile == "quake3" || '
                      'profile == "stvef-sp" || profile == "stvef-mp";', tool)
        self.assertIn('map.materialFlags = profile == "rtcw-sp" || profile == "rtcw-mp" '
                      '? MaterialFlags::Rtcw :', tool)

    def test_the_packager_carries_both_profiles_and_their_assets(self):
        archive = read("scripts/root_archive.py")
        self.assertIn('"wolfet", "stvef-sp", "stvef-mp")', archive)
        self.assertIn('"baseEF/fntech3-hud.json",', archive)
        self.assertIn('*(f"baseEF/maps/{name}.azb" for name in names),', archive)


class STVEFAudioZoneTests(unittest.TestCase):
    def test_every_official_map_has_a_sidecar_in_its_own_profile(self):
        self.assertEqual({profile: len(names) for profile, names in STOCK_STVEF_MAPS.items()},
                         {"stvef-sp": 66, "stvef-mp": 33})
        for profile, names in STOCK_STVEF_MAPS.items():
            with self.subTest(profile=profile):
                root = ROOT / "pkg/games" / profile / GAME_DIRECTORY / "maps"
                shipped = {path.relative_to(root).with_suffix("").as_posix()
                           for path in root.rglob("*.azb")}
                self.assertEqual(shipped, set(names))

    def test_sidecars_are_parsable_and_carry_real_zones(self):
        for profile, names in STOCK_STVEF_MAPS.items():
            for name in names:
                path = ROOT / "pkg/games" / profile / GAME_DIRECTORY / "maps" / f"{name}.azb"
                data = path.read_bytes()
                with self.subTest(profile=profile, map=name):
                    self.assertGreater(len(data), 16)
                    # azfmt::kMagic, then the version and zone count.
                    magic, version, zones = struct.unpack_from("<4sII", data, 0)
                    self.assertEqual(magic, b"FQAZ")
                    self.assertIn(version, (1, 2, 3))
                    # An empty sidecar would silently disable the feature.
                    self.assertGreater(zones, 0)
                    # azfmt::kMaxZones; a larger count means a corrupt file.
                    self.assertLessEqual(zones, 1024)

    def test_the_holomatch_split_follows_the_shipped_arena_metadata(self):
        # Every Holomatch map is one the arena metadata declares, and the tour
        # and underscore-prefixed maps stay in single player.
        holomatch = set(STOCK_STVEF_MAPS["stvef-mp"])
        single = set(STOCK_STVEF_MAPS["stvef-sp"])
        self.assertFalse(holomatch & single)
        self.assertTrue(all(name.startswith(("hm_", "ctf_")) for name in holomatch))
        self.assertFalse(any(name.startswith(("hm_", "ctf_")) for name in single))
        self.assertEqual(sum(1 for name in single if name.startswith("tour/")), 10)
        self.assertEqual(sum(1 for name in single if name.startswith("_")), 10)


if __name__ == "__main__":
    unittest.main()
