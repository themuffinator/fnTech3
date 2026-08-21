"""Contract tests for the FnQL settings overlay.

The overlay renders rows only for cvars the engine publishes in its WebUI
configuration snapshot, and the retail settings sections read every value from
that same snapshot. These tests keep the three sides in step: the overlay's cvar
references, the engine allowlist, and the cvars the engine actually registers.
"""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SETTINGS_JS = ROOT / "code" / "client" / "webui" / "fnql-settings.js"
SETTINGS_CSS = ROOT / "code" / "client" / "webui" / "css" / "fnql-settings.css"
CL_WEBUI = ROOT / "code" / "client" / "cl_webui.cpp"

# Retail settings routes the overlay knows how to extend.
RETAIL_ROUTES = {
    "binds",
    "gamepad",
    "basic",
    "game",
    "hud",
    "team",
    "weapons",
    "video",
    "sound",
    "spectating",
}

# Sources that register the cvars the overlay may bind rows to. cgame-owned
# cvars are excluded: the retail modules own those names.
CVAR_SOURCE_DIRECTORIES = (
    "code/client",
    "code/qcommon",
    "code/renderer",
    "code/renderercommon",
    "code/renderervk",
    "code/rendererrtx",
    "code/sdl",
    "code/win32",
    "code/unix",
)


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def overlay_source() -> str:
    return read(SETTINGS_JS)


def overlay_cvars() -> list[str]:
    names: list[str] = []
    for name in re.findall(r"name: '([A-Za-z_0-9]+)'", overlay_source()):
        if name not in names:
            names.append(name)
    return names


def allowlist_cvars() -> list[str]:
    source = read(CL_WEBUI)
    block = re.search(
        r"static const char \*const configCvars\[\] = \{(.*?)\n\t\tNULL", source, re.S
    )
    assert block is not None, "configCvars allowlist not found"
    return re.findall(r'"([A-Za-z_0-9]+)"', block.group(1))


def registered_cvars() -> set[str]:
    names: set[str] = set()
    pattern = re.compile(r'Cvar_Get\s*\(\s*"([A-Za-z_0-9]+)"')
    for directory in CVAR_SOURCE_DIRECTORIES:
        for source in (ROOT / directory).rglob("*"):
            if source.suffix not in (".c", ".cpp", ".h", ".hpp", ".inl"):
                continue
            names.update(
                name.lower()
                for name in pattern.findall(source.read_text(encoding="utf-8", errors="replace"))
            )
    return names


CVAR_BOUND_SOURCES = (
    "code/renderer/tr_init.c",
    "code/client/cl_main.cpp",
    "code/client/cl_input.cpp",
    "code/client/cl_console.cpp",
    "code/client/audio/snd_main.cpp",
    "code/client/audio/legacy/snd_dma.cpp",
    "code/client/audio/openal/AudioSystemBackend.inl",
    "code/client/cl_webui.cpp",
    "code/qcommon/common.c",
    "code/sdl/sdl_input.cpp",
    "code/win32/win_input.cpp",
)

NUMBER = re.compile(r"-?\d+(?:\.\d+)?")


def cvar_bounds() -> dict[str, tuple[float | None, float | None]]:
    """min/max per cvar from Cvar_CheckRange. Computed bounds resolve to None."""

    bounds: dict[str, tuple[float | None, float | None]] = {}
    name_re = re.compile(r'(\w+)\s*=\s*(?:ri\.)?Cvar_Get\s*\(\s*"([A-Za-z_0-9]+)"')
    range_re = re.compile(
        r"(?:ri\.)?Cvar_CheckRange\s*\(\s*([A-Za-z_0-9]+)\s*,\s*([^;]*?)\)\s*;"
    )
    for relative in CVAR_BOUND_SOURCES:
        source = read(ROOT / relative)
        names = {m.group(1): m.group(2).lower() for m in name_re.finditer(source)}
        for match in range_re.finditer(source):
            cvar = names.get(match.group(1))
            if cvar is None or cvar in bounds:
                continue
            parts = [part.strip().strip('"') for part in match.group(2).split(",")]
            limits: list[float | None] = []
            for part in parts[:2]:
                limits.append(float(part) if NUMBER.fullmatch(part) else None)
            while len(limits) < 2:
                limits.append(None)
            bounds[cvar] = (limits[0], limits[1])
    return bounds


def overlay_rows() -> list[tuple[str, str]]:
    source = overlay_source()
    return re.findall(
        r"\{ name: '([A-Za-z_0-9]+)',(.*?)\n(?=\s*(?:\{ name:|\]|modeRow|//))",
        source,
        re.S,
    )


class SettingsOverlayBoundsTests(unittest.TestCase):
    def test_row_bounds_stay_inside_the_engine_range(self) -> None:
        bounds = cvar_bounds()
        rows = overlay_rows()
        self.assertGreater(len(rows), 100)

        def number(body: str, key: str) -> float | None:
            found = re.search(key + r":\s*(-?\d+(?:\.\d+)?)", body)
            return float(found.group(1)) if found else None

        for name, body in rows:
            low, high = bounds.get(name.lower(), (None, None))
            with self.subTest(cvar=name):
                if "type: 'range'" in body:
                    row_min = number(body, "min")
                    row_max = number(body, "max")
                    self.assertIsNotNone(row_min, "range row without min")
                    self.assertIsNotNone(row_max, "range row without max")
                    self.assertIsNotNone(number(body, "step"), "range row without step")
                    if low is not None:
                        self.assertGreaterEqual(row_min, low)
                    if high is not None:
                        self.assertLessEqual(row_max, high)
                if "type: 'select'" in body:
                    values = re.findall(r"\['([^']*)',\s*'[^']*'\]", body)
                    self.assertTrue(values, "select row without options")
                    for value in values:
                        if not NUMBER.fullmatch(value):
                            continue
                        if low is not None:
                            self.assertGreaterEqual(float(value), low)
                        if high is not None:
                            self.assertLessEqual(float(value), high)


class SettingsOverlayContractTests(unittest.TestCase):
    def test_overlay_cvars_are_published_in_the_config_snapshot(self) -> None:
        allowlist = {name.lower() for name in allowlist_cvars()}
        missing = [name for name in overlay_cvars() if name.lower() not in allowlist]
        self.assertEqual(
            missing,
            [],
            "settings overlay rows bind to cvars the engine snapshot never "
            "publishes; the rows would never appear",
        )

    def test_overlay_cvars_exist_in_the_engine(self) -> None:
        registered = registered_cvars()
        unknown = [name for name in overlay_cvars() if name.lower() not in registered]
        self.assertEqual(
            unknown, [], "settings overlay rows name cvars no engine source registers"
        )

    def test_allowlist_covers_the_retail_settings_sections(self) -> None:
        # Every engine-owned cvar the retail settings sections bind a row to.
        # Retail seeds its rows from the snapshot alone, so an omission renders
        # the row with a default instead of the player's value.
        retail_rows = (
            "sensitivity",
            "m_pitch",
            "m_cpi",
            "cl_mouseAccel",
            "cl_mouseAccelOffset",
            "cl_mouseSensCap",
            "cl_viewAccel",
            "in_joystick_inverted",
            "in_joyHorizViewSensitivity",
            "in_joyVertViewSensitivity",
            "in_joyHorizViewDeadzone",
            "in_joyVertViewDeadzone",
            "in_joyHorizMoveDeadzone",
            "in_joyVertMoveDeadzone",
            "r_fullscreen",
            "r_gamma",
            "r_swapInterval",
            "r_displayRefresh",
            "r_mapOverBrightBits",
            "r_overBrightBits",
            "r_texturemode",
            "r_picmip",
            "r_vertexLight",
            "r_dynamiclight",
            "r_fastsky",
            "s_volume",
            "s_musicVolume",
            "s_voiceVolume",
            "s_doppler",
            "cl_allowConsoleChat",
            "cl_demoRecordMessage",
        )
        allowlist = {name.lower() for name in allowlist_cvars()}
        missing = [name for name in retail_rows if name.lower() not in allowlist]
        self.assertEqual(missing, [])

    def test_allowlist_omits_the_retail_postprocess_control_surface(self) -> None:
        # FnQL does not implement the retail post-processing pipeline, and the
        # overlay hides those rows instead of publishing dead cvars for them.
        allowlist = {name.lower() for name in allowlist_cvars()}
        for name in (
            "r_enablepostprocess",
            "r_enablecolorcorrect",
            "r_enablebloom",
            "r_bloombrightthreshold",
            "r_bloomsaturation",
            "r_bloomintensity",
            "r_bloomscenesaturation",
            "r_bloomsceneintensity",
            "cg_vignette",
            "r_lightmap",
            "r_fullbright",
            "r_ambientscale",
        ):
            with self.subTest(cvar=name):
                self.assertNotIn(name, allowlist)

    def test_allowlist_has_no_duplicate_entries(self) -> None:
        names = [name.lower() for name in allowlist_cvars()]
        duplicates = sorted({name for name in names if names.count(name) > 1})
        self.assertEqual(duplicates, [])

    def test_config_snapshot_buffer_fits_the_allowlist(self) -> None:
        source = read(CL_WEBUI)
        budget = int(
            re.search(r"#define CL_WEB_CONFIG_CVAR_JSON_LENGTH (\d+)", source).group(1)
        )
        # "name":"value", per entry. Values are bounded by MAX_CVAR_VALUE_STRING
        # but the realistic worst case here is a per-weapon command string.
        estimate = sum(len(name) + 6 + 48 for name in allowlist_cvars())
        self.assertLess(estimate, budget)

    def test_overlay_routes_are_retail_routes(self) -> None:
        source = overlay_source()
        mapping = re.search(r"var SECTION_ROUTES = \{(.*?)\};", source, re.S)
        assert mapping is not None
        routes = set(re.findall(r": '([a-z]+)'", mapping.group(1)))
        self.assertEqual(routes, RETAIL_ROUTES)

        for table in ("RETAIL_UNSUPPORTED", "COLUMN_ROWS", "SECTION_GROUPS"):
            block = re.search(
                r"var %s = \{(.*?)\n  \};" % table, source, re.S
            )
            assert block is not None, table
            for route in re.findall(r"^    ([a-z]+): \[", block.group(1), re.M):
                with self.subTest(table=table, route=route):
                    self.assertIn(route, RETAIL_ROUTES)

    def test_player_highlighting_is_integrated_with_the_team_section(self) -> None:
        source = overlay_source()
        team_columns = re.search(
            r"    team: \[(.*?)\n    \],\n    sound: \[", source, re.S
        )
        assert team_columns is not None, "team column injections not found"
        columns = team_columns.group(1)

        # The retail Team section renders teammate settings in the left column
        # and opponent settings in the right one. The relationship overrides
        # belong beside them.
        teammate, opponent = columns.split("column: 1")
        self.assertIn("cl_playerHighlightTeammateColor", teammate)
        self.assertIn("cl_playerHighlightEnemyColor", opponent)
        self.assertNotIn("cl_playerHighlightEnemyColor", teammate)

        team_groups = re.search(
            r"    team: \[\n      \{\n        title: 'Player Highlighting',(.*?)\n    \],",
            source,
            re.S,
        )
        assert team_groups is not None, "player highlighting group not found"
        for name in (
            "cl_playerHighlight'",
            "cl_playerHighlightRimIntensity",
            "cl_playerHighlightOutlineIntensity",
            "cl_playerHighlightOutlineScale",
            "cl_playerHighlightRedColor",
            "cl_playerHighlightBlueColor",
            "cl_playerHighlightFreeColor",
        ):
            with self.subTest(cvar=name):
                self.assertIn(name, team_groups.group(1))

    def test_framebuffer_dependent_effects_are_present_and_explained(self) -> None:
        source = overlay_source()
        for name in (
            "r_fbo",
            "r_hdr",
            "r_bloom'",
            "r_motionBlur",
            "r_greyscale",
            "r_liquid'",
            "r_globalFog'",
            "r_ext_multisample",
            "r_ext_supersample",
            "r_renderScale",
            "r_crt'",
            "r_depthFade",
        ):
            with self.subTest(cvar=name):
                self.assertIn("'%s" % name.rstrip("'"), source)

        framebuffer_row = re.search(
            r"\{ name: 'r_fbo'.*?\},", source, re.S
        )
        assert framebuffer_row is not None
        self.assertIn("Required by", framebuffer_row.group(0))

    def test_overlay_stylesheet_only_covers_what_retail_lacks(self) -> None:
        style = read(SETTINGS_CSS)
        self.assertIn(".fnql-retail-hidden", style)
        self.assertIn(".fnql-cvar-help", style)
        # Retail's own rules style .cvar, .range and .Select; redefining them
        # would make injected rows drift away from the retail rows beside them.
        for selector in ("\n.cvar", "\n.range", "\n.Select", ".Select-control {"):
            with self.subTest(selector=selector):
                self.assertNotIn(selector, style)

    def test_diagnostics_report_injected_rows(self) -> None:
        source = read(CL_WEBUI)
        self.assertIn('{ "fnqlRows",', source)
        self.assertIn('{ "fnqlGroups",', source)
        self.assertIn('{ "fnqlHiddenRetailRows",', source)
        self.assertIn('{ "fnqlActions",', source)
        self.assertNotIn("fnql-settings-tab", source)


class SettingsCvarPersistenceTests(unittest.TestCase):
    def test_menu_owned_cvars_persist(self) -> None:
        # A settings row whose cvar is never written to a config forgets the
        # player's choice on the next launch. Both of these are Quake Live
        # profile cvars in QLSRP and appear in a retail install's configs.
        common = read(ROOT / "code" / "qcommon" / "common.c")
        self.assertIn(
            'com_maxfps = Cvar_Get( "com_maxfps", "125",\n'
            "\t\tCVAR_ARCHIVE | CVAR_PROTECTED | CVAR_CLOUD );",
            common,
        )
        # Cvar_Restart( qtrue ) unsets CVAR_VM_CREATED cvars on a game-directory
        # change, and com_maxfps is held by an engine pointer read every frame.
        self.assertNotIn("com_maxfps\", \"125\",\n\t\tCVAR_ARCHIVE | CVAR_PROTECTED | CVAR_VM_CREATED", common)

        client = read(ROOT / "code" / "client" / "cl_main.cpp")
        self.assertIn(
            'r_displayRefresh = Cvar_Get( "r_displayRefresh", "0",\n'
            "\t\tCVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_CLOUD );",
            client,
        )


if __name__ == "__main__":
    unittest.main()
