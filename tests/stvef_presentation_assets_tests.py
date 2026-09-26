"""Elite Force's shipped HUD aspect rules and audio zone sidecars.

The shipped HUD presets (docs/fntech3/2026-09-25-widescreen-hud-and-fov.md)
are checked against draw groups the retail cgame modules actually issued
(tests/fixtures/stvef_hud_draws.json, captured with cl_hudDump): the checks
replay those groups through the engine's own matching semantics, the draw's
centre point against each rule in order, first match wins. The audio zone
sidecars cover every official map, split the way the shipped arena metadata
splits them.
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
# `extend` stretches a bar across the screen, as the cutscene letterbox does.
MODES = {"uniform", "extend"}


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
                    self.assertIn(rule["mode"], MODES)
                    self.assertLessEqual(set(rule["align"]), {"x", "y"})
                    self.assertLessEqual(set(rule["align"].values()), ALIGNMENTS)
                    self.assertTrue(set(rule["match"]) <= {"shader", "textLike", "refdef", "region"})
                    self.assertTrue(rule["match"], "a rule that matches everything is not useful")
                    if "region" in rule["match"]:
                        region = rule["match"]["region"]
                        self.assertGreater(region["w"], 0)
                        self.assertGreater(region["h"], 0)
                        # Rules are authored against the 640x480 virtual screen;
                        # a letterbox bar's region may reach just past its edge.
                        self.assertLess(region["x"], 640)
                        self.assertGreater(region["x"] + region["w"], 0)
                        self.assertLess(region["y"], 480)
                        self.assertGreater(region["y"] + region["h"], 0)

    def test_no_rule_is_shadowed_by_an_earlier_one(self):
        # A draw made to each rule's own description must reach that rule; one
        # an earlier rule takes instead is dead. The capture does not exercise
        # every rule (single player's save shows no ammunition or cutscene), so
        # the draws are made from the rules themselves.
        for profile in PROFILES:
            with self.subTest(profile=profile):
                ruleset = rules(profile)["rules"]
                for rule in ruleset:
                    match = rule["match"]
                    region = match.get("region", {"x": 320, "y": 240, "w": 0, "h": 0})
                    group = {"shader": match.get("shader", "white").replace("*", "x"),
                             "textLike": match.get("textLike", 0), "refdef": match.get("refdef", 0),
                             "x": region["x"], "y": region["y"], "w": region["w"], "h": region["h"]}
                    found = find_rule(ruleset, group)
                    self.assertIsNotNone(found, rule["name"])
                    self.assertEqual(found["name"], rule["name"])

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
                    if rule is None:
                        # The default centred correction keeps a draw in the
                        # middle of the screen where it is; a corner's needs a rule.
                        self.assertTrue(213 <= cx <= 427, f"{profile}: {shader} at {cx},{cy} is unruled")
                        continue
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
            # Unruled, the default correction keeps it centred as well.
            if rule is not None:
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
        # The predicate is shared with the other corrected titles, so this pins
        # Elite Force's place in it rather than the whole line.
        hud = read("code/client/cl_hud.cpp")
        start = hud.index("hudSupported = Com_GameIs")
        enable = hud[start:hud.index(";", start)]
        self.assertIn("Com_GameIsSTVEF()", enable)
        self.assertNotIn("Com_GameIsQuakeLive", enable)
        # The rules only reach the client if the root archive will serve them.
        files = read("code/qcommon/files.c")
        allowed = files[files.index("FS_RootArchiveAllowsFile"):]
        allowed = allowed[:allowed.index("FS_RootArchiveAllowsMapSidecar( qpath )")]
        self.assertIn("Com_GameIsSTVEF()", allowed)
        self.assertIn('!FS_FilenameCompare( qpath, "fntech3-hud.json" )', allowed)

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
        import root_archive
        for profile, names in STOCK_STVEF_MAPS.items():
            with self.subTest(profile=profile):
                self.assertIn(profile, root_archive.GAME_PROFILES)
                self.assertEqual(set(root_archive.PROFILE_REQUIRED_ASSETS[profile]),
                                 {f"{GAME_DIRECTORY}/fntech3-hud.json"}
                                 | {f"{GAME_DIRECTORY}/maps/{name}.azb" for name in names})


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
