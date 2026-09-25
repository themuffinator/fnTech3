from __future__ import annotations

import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
import shutil
import os


BSP_HEADER_LUMPS = 17
BSP_HEADER_SIZE = 8 + BSP_HEADER_LUMPS * 8
BSP_LUMP_SHADERS = 1
BSP_LUMP_LEAFS = 4
BSP_LUMP_LEAF_SURFACES = 5
BSP_LUMP_LEAF_BRUSHES = 6
BSP_LUMP_BRUSHES = 8
BSP_LUMP_BRUSH_SIDES = 9
BSP_LUMP_SURFACES = 13


def i32(value: int) -> bytes:
    return struct.pack("<i", value)


def fixed_qpath(name: str) -> bytes:
    encoded = name.encode("ascii")
    if len(encoded) >= 64:
        raise ValueError("qpath is too long for the test BSP")
    return encoded + b"\0" * (64 - len(encoded))


def leaf_record(
    cluster: int,
    area: int,
    mins: tuple[int, int, int],
    maxs: tuple[int, int, int],
    first_leaf_surface: int,
    num_leaf_surfaces: int,
) -> bytes:
    return b"".join(
        [
            i32(cluster),
            i32(area),
            i32(mins[0]),
            i32(mins[1]),
            i32(mins[2]),
            i32(maxs[0]),
            i32(maxs[1]),
            i32(maxs[2]),
            i32(first_leaf_surface),
            i32(num_leaf_surfaces),
            i32(0),
            i32(0),
        ]
    )


def surface_record(shader_index: int) -> bytes:
    surface = bytearray(104)
    surface[0:4] = i32(shader_index)
    surface[8:12] = i32(1)
    return bytes(surface)


def build_bsp(
    shaders: list[str],
    leaves: list[tuple[int, int, tuple[int, int, int], tuple[int, int, int], int]],
) -> bytes:
    shader_lump = b"".join(fixed_qpath(name) + i32(0) + i32(0) for name in shaders)
    leaf_lump = bytearray()
    leaf_surfaces = bytearray()
    surfaces = bytearray()

    for cluster, area, mins, maxs, shader_index in leaves:
        surface_index = len(surfaces) // 104
        first_leaf_surface = len(leaf_surfaces) // 4
        surfaces.extend(surface_record(shader_index))
        leaf_surfaces.extend(i32(surface_index))
        leaf_lump.extend(leaf_record(cluster, area, mins, maxs, first_leaf_surface, 1))

    lump_data: dict[int, bytes] = {
        BSP_LUMP_SHADERS: shader_lump,
        BSP_LUMP_LEAFS: bytes(leaf_lump),
        BSP_LUMP_LEAF_SURFACES: bytes(leaf_surfaces),
        BSP_LUMP_LEAF_BRUSHES: b"",
        BSP_LUMP_BRUSHES: b"",
        BSP_LUMP_BRUSH_SIDES: b"",
        BSP_LUMP_SURFACES: bytes(surfaces),
    }
    lumps: list[tuple[int, int]] = []
    body = bytearray()
    offset = BSP_HEADER_SIZE
    for index in range(BSP_HEADER_LUMPS):
        payload = lump_data.get(index, b"")
        lumps.append((offset, len(payload)))
        body.extend(payload)
        offset += len(payload)

    header = bytearray()
    header.extend(b"IBSP")
    header.extend(i32(46))
    for lump_offset, lump_length in lumps:
        header.extend(i32(lump_offset))
        header.extend(i32(lump_length))
    return bytes(header + body)


def build_minimal_bsp(shader_name: str, *, version: int = 46, flags: int = 0) -> bytes:
    data = bytearray(build_bsp([shader_name], [(0, 0, (0, 0, 0), (128, 128, 128), 0)]))
    struct.pack_into("<i", data, 4, version)
    shader_offset = struct.unpack_from("<i", data, 8 + BSP_LUMP_SHADERS * 8)[0]
    struct.pack_into("<i", data, shader_offset + 64, flags)
    return bytes(data)


# Alice's FAKK 42: a checksum after the version, twenty lumps in F.A.K.K.2's
# order, and shader and surface records one trailing field longer.
FAKK_HEADER_LUMPS = 20
FAKK_HEADER_SIZE = 12 + FAKK_HEADER_LUMPS * 8
FAKK_LUMP = {"shaders": 0, "planes": 1, "surfaces": 3, "leaf_brushes": 6, "leaf_surfaces": 7,
             "leafs": 8, "brush_sides": 10, "brushes": 11}
# Ritual's flags, from alice.exe's surfaceparm table.
ALICE_HINT = 0x1000000


def fakk_shader(name: str, surface_flags: int = 0, contents: int = 1) -> bytes:
    return fixed_qpath(name) + i32(surface_flags) + i32(contents) + i32(16)


def fakk_surface(shader_index: int) -> bytes:
    return surface_record(shader_index) + struct.pack("<f", 0.0)


def build_fakk(lumps: dict[str, bytes], *, version: int = 42) -> bytes:
    """A FAKK map with one leaf (cluster 0) seeing each draw surface and brush given."""
    header = bytearray(b"FAKK" + i32(version) + i32(0))
    body = bytearray()
    offset = FAKK_HEADER_SIZE
    for index in range(FAKK_HEADER_LUMPS):
        name = next((key for key, value in FAKK_LUMP.items() if value == index), None)
        payload = lumps.get(name, b"") if name else b""
        header += i32(offset) + i32(len(payload))
        body += payload
        offset += len(payload)
    return bytes(header + body)


def alice_room(shaders: list[bytes], draw_shader: int, *, brush_shader: int | None = None,
               side_shader: int | None = None) -> bytes:
    """A 512-unit room: one draw surface, and optionally one brush with one side."""
    brush_count = 0 if brush_shader is None else 1
    leaf = b"".join(i32(v) for v in (0, 0, 0, 0, 0, 512, 512, 128, 0, 1, 0, brush_count))
    lumps = {
        "shaders": b"".join(shaders),
        "planes": struct.pack("<4f", 0.0, 0.0, 1.0, 4096.0),
        "surfaces": fakk_surface(draw_shader),
        "leaf_surfaces": i32(0),
        "leafs": leaf,
    }
    if brush_shader is not None:
        lumps["brushes"] = i32(0) + i32(1) + i32(brush_shader)
        lumps["brush_sides"] = i32(0) + i32(side_shader if side_shader is not None else brush_shader)
        lumps["leaf_brushes"] = i32(0)
    return build_fakk(lumps)


# Medal of Honor: Allied Assault's 2015 19: a checksum after the version, 28
# lumps in 2015's order, and shader, surface, leaf and brush side records that
# add fields after Quake III's. A terrain patch names its shader at +40.
MOHAA_HEADER_LUMPS = 28
MOHAA_HEADER_SIZE = 12 + MOHAA_HEADER_LUMPS * 8
MOHAA_LUMP = {"shaders": 0, "planes": 1, "surfaces": 3, "leaf_brushes": 6, "leaf_surfaces": 7,
              "leafs": 8, "brush_sides": 11, "brushes": 12, "terrain": 22, "terrain_indexes": 23}
# 2015's flags (OpenMoHAA's surfaceflags.h), checked against the retail maps.
MOHAA_HINT = 0x40000000
MOHAA_METAL = 0x8000


def mohaa_shader(name: str, surface_flags: int = 0, contents: int = 1) -> bytes:
    return fixed_qpath(name) + i32(surface_flags) + i32(contents) + i32(16) + fixed_qpath("")


def mohaa_surface(shader_index: int) -> bytes:
    return surface_record(shader_index) + struct.pack("<f", 0.0)


def mohaa_terrain_patch(shader_index: int) -> bytes:
    patch = bytearray(388)
    struct.pack_into("<H", patch, 40, shader_index)
    return bytes(patch)


def build_mohaa(lumps: dict[str, bytes], *, version: int = 19) -> bytes:
    """A 2015 map with the lumps given; every other lump is empty."""
    header = bytearray(b"2015" + i32(version) + i32(0))
    body = bytearray()
    offset = MOHAA_HEADER_SIZE
    for index in range(MOHAA_HEADER_LUMPS):
        name = next((key for key, value in MOHAA_LUMP.items() if value == index), None)
        payload = lumps.get(name, b"") if name else b""
        header += i32(offset) + i32(len(payload))
        body += payload
        offset += len(payload)
    return bytes(header + body)


def mohaa_room(shaders: list[bytes], draw_shader: int, *, brush_shader: int | None = None,
               side_shader: int | None = None, terrain_shader: int | None = None) -> bytes:
    """A 512-unit room: one draw surface, optionally one brush with one side, and one terrain patch."""
    brush_count = 0 if brush_shader is None else 1
    terrain_count = 0 if terrain_shader is None else 1
    leaf = b"".join(i32(v) for v in (0, 0, 0, 0, 0, 512, 512, 128, 0, 1, 0, brush_count,
                                     0, terrain_count, 0, 0))
    lumps = {
        "shaders": b"".join(shaders),
        "planes": struct.pack("<4f", 0.0, 0.0, 1.0, 4096.0),
        "surfaces": mohaa_surface(draw_shader),
        "leaf_surfaces": i32(0),
        "leafs": leaf,
    }
    if brush_shader is not None:
        lumps["brushes"] = i32(0) + i32(1) + i32(brush_shader)
        lumps["brush_sides"] = i32(0) + i32(side_shader if side_shader is not None else brush_shader) + i32(-1)
        lumps["leaf_brushes"] = i32(0)
    if terrain_shader is not None:
        lumps["terrain"] = mohaa_terrain_patch(terrain_shader)
        lumps["terrain_indexes"] = struct.pack("<h", 0)
    return build_mohaa(lumps)


def resolve_tool() -> Path:
    configured = os.environ.get("FNQL_AUDIOZONESC", "").strip()
    tool_argument = next(
        (
            argument
            for argument in sys.argv[1:]
            if Path(argument).name.lower()
            in {"fnql-audiozonesc", "fnql-audiozonesc.exe"}
        ),
        "",
    )
    tool = Path(configured or tool_argument or "fnql-audiozonesc")
    if tool.parent == Path("."):
        local_tool = Path.cwd() / tool.name
        if local_tool.exists():
            return local_tool
        found = shutil.which(tool.name)
        if found:
            return Path(found)
    elif tool.is_file():
        return tool

    raise unittest.SkipTest(
        "fnql-audiozonesc is not built; pass its path or set FNQL_AUDIOZONESC"
    )


class AudioZoneMaterialMapTests(unittest.TestCase):
    def generate_and_dump(self, bsp_bytes: bytes, material_map: str | None = None,
                          profile: str | None = None) -> str:
        tool = resolve_tool()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            bsp = root / "generated.bsp"
            bsp.write_bytes(bsp_bytes)
            output = root / "generated.azb"
            command = [str(tool), "--from-bsp", "-o", str(output)]
            if profile is not None:
                command.extend(["--profile", profile])
            if material_map is not None:
                material_map_path = root / "audio-materials.txt"
                material_map_path.write_text(material_map, encoding="utf-8")
                command.extend(["--material-map", str(material_map_path)])
            command.append(str(bsp))

            generated = subprocess.run(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            self.assertEqual(generated.returncode, 0, generated.stderr)

            dumped = subprocess.run(
                [str(tool), "--dump", str(output)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            self.assertEqual(dumped.returncode, 0, dumped.stderr)
            return dumped.stdout

    def test_material_flags_follow_explicit_profile_not_bsp_version(self) -> None:
        for profile, version in ((None, 47), ("quake3", 46), ("quakelive", 47),
                                 ("rtcw-sp", 47), ("rtcw-mp", 47), ("wolfet", 47)):
            rtcw = profile in {"rtcw-sp", "rtcw-mp"}
            # Enemy Territory keeps RTCW's material bits but gives 0x40 to a
            # water splash. That votes liquid, which a dry zone never takes as
            # its own material, so the zone is left neutral rather than stone.
            wolf = rtcw or profile == "wolfet"
            for flag, material in (
                (0x40, "stone" if rtcw else "neutral" if profile == "wolfet" else "soft"),
                (0x1000, "metal"),
                *((flag, "stone" if wolf else "neutral")
                  for flag in (0x200000, 0x800000, 0x1000000)),
                *((flag, "soft" if wolf else "neutral")
                  for flag in (0x40000, 0x80000, 0x100000, 0x400000, 0x2000000)),
            ):
                with self.subTest(profile=profile, flag=hex(flag)):
                    dumped = self.generate_and_dump(
                        build_minimal_bsp("textures/plain/panel", version=version, flags=flag),
                        profile=profile,
                    )
                    self.assertIn(f"material {material}", dumped)

    def test_bsp_profile_errors_leave_existing_output_untouched(self) -> None:
        tool = resolve_tool()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            bsp = root / "q3.bsp"
            bsp.write_bytes(build_minimal_bsp("textures/plain/panel"))
            output = root / "old.azb"
            output.write_bytes(b"previous sidecar")
            for profile in ("rtcw-sp", "rtcw-mp", "wolfet", "quakelive", "unknown"):
                result = subprocess.run(
                    [str(tool), "--from-bsp", "--profile", profile, "-o", str(output), str(bsp)],
                    capture_output=True, text=True, check=False,
                )
                self.assertNotEqual(result.returncode, 0, profile)
                self.assertIn("--profile", result.stderr)
                self.assertEqual(output.read_bytes(), b"previous sidecar")

    def test_alice_reads_fakk_maps_with_ritual_material_flags(self) -> None:
        # Ritual numbers materials from bit 16; Quake III's flesh (0x40) is
        # Ritual's castshadow, its metal steps (0x1000) are unused, and slide
        # (0x2000000) is a movement property.
        for flag, material in (
            (0x20000, "metal"), (0x100000, "metal"), (0x40000, "stone"),
            (0x10000, "soft"), (0x80000, "soft"), (0x200000, "soft"), (0x400000, "soft"),
            (0x40, "neutral"), (0x1000, "neutral"), (0x2000000, "neutral"),
        ):
            with self.subTest(flag=hex(flag)):
                dumped = self.generate_and_dump(
                    alice_room([fakk_shader("textures/plain/panel", flag)], 0), profile="alice")
                self.assertIn(f"material {material}", dumped)

    def test_alice_skips_ritual_hint_sides_but_not_quake_hint_bits(self) -> None:
        for side_flags, material in ((ALICE_HINT, "neutral"), (0x80, "neutral"), (0x100, "metal"), (0x200, "metal")):
            with self.subTest(side_flags=hex(side_flags)):
                shaders = [fakk_shader("textures/plain/panel"), fakk_shader("textures/plain/metalside", side_flags)]
                dumped = self.generate_and_dump(
                    alice_room(shaders, 0, brush_shader=0, side_shader=1), profile="alice")
                self.assertIn(f"material {material}", dumped)

    def test_fakk_maps_and_the_alice_profile_require_each_other(self) -> None:
        tool = resolve_tool()
        fakk = alice_room([fakk_shader("textures/plain/panel")], 0)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            output = root / "old.azb"
            output.write_bytes(b"previous sidecar")
            for data, profile in ((fakk, None), (fakk, "wolfet"), (fakk, "quake3"),
                                  (build_minimal_bsp("textures/plain/panel", version=47), "alice"),
                                  (build_minimal_bsp("textures/plain/panel"), "alice")):
                with self.subTest(fakk=data[:4] == b"FAKK", profile=profile):
                    bsp = root / "input.bsp"
                    bsp.write_bytes(data)
                    command = [str(tool), "--from-bsp", "-o", str(output), str(bsp)]
                    if profile is not None:
                        command[2:2] = ["--profile", profile]
                    result = subprocess.run(command, capture_output=True, text=True, check=False)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("--profile", result.stderr)
                    self.assertEqual(output.read_bytes(), b"previous sidecar")

    def test_malformed_fakk_maps_are_refused(self) -> None:
        tool = resolve_tool()
        good = alice_room([fakk_shader("textures/plain/panel")], 0)
        shader_lump = 12 + FAKK_LUMP["shaders"] * 8
        leaf_lump = 12 + FAKK_LUMP["leafs"] * 8
        cases = []
        cases.append(("F.A.K.K.2's own version", bytearray(good), lambda d: struct.pack_into("<i", d, 4, 12),
                      "unsupported FAKK version 12"))
        cases.append(("truncated header", bytearray(good[:FAKK_HEADER_SIZE - 8]), lambda d: None, "truncated"))
        cases.append(("lump past the end", bytearray(good),
                      lambda d: struct.pack_into("<i", d, leaf_lump, len(good)), "out-of-range lump"))
        cases.append(("Quake III-sized shader", bytearray(good),
                      lambda d: struct.pack_into("<i", d, shader_lump + 4, 72), "shader lump has a malformed size"))
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for label, data, damage, message in cases:
                with self.subTest(label):
                    damage(data)
                    bsp = root / "damaged.bsp"
                    bsp.write_bytes(bytes(data))
                    output = root / "damaged.azb"
                    result = subprocess.run(
                        [str(tool), "--from-bsp", "--profile", "alice", "-o", str(output), str(bsp)],
                        capture_output=True, text=True, check=False)
                    self.assertEqual(result.returncode, 1, result.stdout)
                    self.assertIn(message, result.stderr)
                    self.assertFalse(output.exists())

    def test_fakk2_reads_its_fakk_12_maps_with_ritual_material_flags(self) -> None:
        # F.A.K.K.2's maps are FAKK 12 with Alice's records, and its surface
        # flags are Ritual's (its SDK's surfaceflags.h, and its retail maps'
        # metal and wood shaders).
        for flag, material in ((0x20000, "metal"), (0x40000, "stone"), (0x10000, "soft"), (0x40, "neutral")):
            with self.subTest(flag=hex(flag)):
                data = alice_room([fakk_shader("textures/plain/panel", flag)], 0)
                data = data[:4] + struct.pack("<i", 12) + data[8:]
                dumped = self.generate_and_dump(data, profile="fakk2")
                self.assertIn(f"material {material}", dumped)

    def test_each_ritual_profile_reads_only_its_own_fakk_version(self) -> None:
        tool = resolve_tool()
        alice = alice_room([fakk_shader("textures/plain/panel")], 0)
        fakk2 = alice[:4] + struct.pack("<i", 12) + alice[8:]
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            output = root / "old.azb"
            output.write_bytes(b"previous sidecar")
            for label, data, profile, message in (
                ("FAKK 12 as alice", fakk2, "alice", "unsupported FAKK version 12 for --profile alice"),
                ("FAKK 42 as fakk2", alice, "fakk2", "unsupported FAKK version 42 for --profile fakk2"),
                ("FAKK 12 without a profile", fakk2, None, "a FAKK 12 map needs --profile fakk2"),
                ("FAKK 12 as quake3", fakk2, "quake3", "a FAKK 12 map needs --profile fakk2"),
                ("IBSP as fakk2", build_minimal_bsp("textures/plain/panel"), "fakk2",
                 "BSP version does not match --profile fakk2"),
            ):
                with self.subTest(label):
                    bsp = root / "input.bsp"
                    bsp.write_bytes(data)
                    command = [str(tool), "--from-bsp", "-o", str(output), str(bsp)]
                    if profile is not None:
                        command[2:2] = ["--profile", profile]
                    result = subprocess.run(command, capture_output=True, text=True, check=False)
                    self.assertEqual(result.returncode, 1, result.stdout)
                    self.assertIn(message, result.stderr)
                    self.assertEqual(output.read_bytes(), b"previous sidecar")

    def test_mohaa_reads_2015_maps_with_its_material_flags(self) -> None:
        # 2015 numbers fifteen materials from bit 13; Quake III's flesh (0x40)
        # is its castshadow and Quake III's metal steps (0x1000) its overbright.
        soft = (0x2000, 0x4000, 0x20000, 0x80000, 0x100000, 0x800000, 0x1000000, 0x2000000, 0x4000000, 0x8000000)
        for flag, material in ((0x8000, "metal"), (0x40000, "metal"), (0x10000, "stone"), (0x400000, "stone"),
                               *((flag, "soft") for flag in soft), (0x40, "neutral"), (0x1000, "neutral")):
            with self.subTest(flag=hex(flag)):
                dumped = self.generate_and_dump(
                    mohaa_room([mohaa_shader("textures/plain/panel", flag)], 0), profile="mohaa")
                self.assertIn(f"material {material}", dumped)

    def test_mohaa_material_flags_outrank_shader_name_tokens(self) -> None:
        # Every retail face carries one of the flags, so a shader's name only
        # decides for a face that has none.
        for name, flag, material in (("textures/general/stonewall", MOHAA_METAL, "metal"),
                                     ("textures/snow/forstsnow_rock", 0x4000000, "soft"),
                                     ("textures/general/stonewall", 0, "stone")):
            with self.subTest(name=name, flag=hex(flag)):
                dumped = self.generate_and_dump(mohaa_room([mohaa_shader(name, flag)], 0), profile="mohaa")
                self.assertIn(f"material {material}", dumped)

    def test_mohaa_skips_its_hint_and_nodraw_sides_but_not_quake_hint_bits(self) -> None:
        # 2015's hint is bit 30; its 0x100 and 0x200 are nolightmap and
        # alphashadow, which are drawn.
        for side_flags, material in ((MOHAA_HINT, "neutral"), (0x80, "neutral"), (0x100, "metal"), (0x200, "metal")):
            with self.subTest(side_flags=hex(side_flags)):
                shaders = [mohaa_shader("textures/plain/panel"),
                           mohaa_shader("textures/plain/side", MOHAA_METAL | side_flags)]
                dumped = self.generate_and_dump(
                    mohaa_room(shaders, 0, brush_shader=0, side_shader=1), profile="mohaa")
                self.assertIn(f"material {material}", dumped)

    def test_mohaa_counts_a_leafs_terrain_as_drawn_ground(self) -> None:
        shaders = [mohaa_shader("textures/plain/panel"), mohaa_shader("textures/terrain/ground", 0x80000)]
        self.assertIn("material neutral", self.generate_and_dump(mohaa_room(shaders, 0), profile="mohaa"))
        self.assertIn("material soft",
                      self.generate_and_dump(mohaa_room(shaders, 0, terrain_shader=1), profile="mohaa"))

    def test_mohaa_leaves_invisible_shadow_casters_out(self) -> None:
        # textures/common/caulkshadow compiles to surfaces: nodraw, castshadow,
        # nomarks and nolightmap over the default rock.
        for flags, material in ((0x101e0, "neutral"), (0x10000, "stone")):
            with self.subTest(flags=hex(flags)):
                dumped = self.generate_and_dump(
                    mohaa_room([mohaa_shader("textures/common/caulkshadow", flags)], 0), profile="mohaa")
                self.assertIn(f"material {material}", dumped)

    def test_2015_maps_and_the_mohaa_profile_require_each_other(self) -> None:
        tool = resolve_tool()
        mohaa = mohaa_room([mohaa_shader("textures/plain/panel")], 0)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            output = root / "old.azb"
            output.write_bytes(b"previous sidecar")
            for label, data, profile, message in (
                ("2015 without a profile", mohaa, None, "a 2015 map needs --profile mohaa"),
                ("2015 as quake3", mohaa, "quake3", "a 2015 map needs --profile mohaa"),
                ("2015 as fakk2", mohaa, "fakk2", "a 2015 map needs --profile mohaa"),
                ("IBSP as mohaa", build_minimal_bsp("textures/plain/panel"), "mohaa",
                 "BSP version does not match --profile mohaa"),
                ("FAKK as mohaa", alice_room([fakk_shader("textures/plain/panel")], 0), "mohaa",
                 "BSP version does not match --profile mohaa"),
            ):
                with self.subTest(label):
                    bsp = root / "input.bsp"
                    bsp.write_bytes(data)
                    command = [str(tool), "--from-bsp", "-o", str(output), str(bsp)]
                    if profile is not None:
                        command[2:2] = ["--profile", profile]
                    result = subprocess.run(command, capture_output=True, text=True, check=False)
                    self.assertEqual(result.returncode, 1, result.stdout)
                    self.assertIn(message, result.stderr)
                    self.assertEqual(output.read_bytes(), b"previous sidecar")

    def test_malformed_2015_maps_are_refused(self) -> None:
        tool = resolve_tool()
        good = mohaa_room([mohaa_shader("textures/plain/panel"), mohaa_shader("textures/terrain/ground")], 0,
                          terrain_shader=1)
        leaf_lump = 12 + MOHAA_LUMP["leafs"] * 8
        terrain_lump = 12 + MOHAA_LUMP["terrain"] * 8
        cases = (
            ("an older 2015 version", lambda d: struct.pack_into("<i", d, 4, 17), "unsupported 2015 version 17"),
            ("truncated header", lambda d: d.__delitem__(slice(MOHAA_HEADER_SIZE - 8, None)), "truncated"),
            ("lump past the end", lambda d: struct.pack_into("<i", d, leaf_lump, len(good)), "out-of-range lump"),
            ("Quake III-sized leaf", lambda d: struct.pack_into("<i", d, leaf_lump + 4, 48),
             "leaf lump has a malformed size"),
            ("partial terrain patch", lambda d: struct.pack_into("<i", d, terrain_lump + 4, 100),
             "terrain lump has a malformed size"),
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for label, damage, message in cases:
                with self.subTest(label):
                    data = bytearray(good)
                    damage(data)
                    bsp = root / "damaged.bsp"
                    bsp.write_bytes(bytes(data))
                    output = root / "damaged.azb"
                    result = subprocess.run(
                        [str(tool), "--from-bsp", "--profile", "mohaa", "-o", str(output), str(bsp)],
                        capture_output=True, text=True, check=False)
                    self.assertEqual(result.returncode, 1, result.stdout)
                    self.assertIn(message, result.stderr)
                    self.assertFalse(output.exists())

    def test_material_map_overrides_shader_classification(self) -> None:
        tool = resolve_tool()

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            bsp = root / "material_map.bsp"
            bsp.write_bytes(build_minimal_bsp("textures/custom/mystery_panel"))
            material_map = root / "audio-materials.txt"
            material_map.write_text(
                "textures/custom/* metal preset hallway flag outdoor weight 8\n",
                encoding="utf-8",
            )
            output = root / "material_map.azb"

            generated = subprocess.run(
                [
                    str(tool),
                    "--from-bsp",
                    "--material-map",
                    str(material_map),
                    "-o",
                    str(output),
                    str(bsp),
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            self.assertEqual(generated.returncode, 0, generated.stderr)
            self.assertIn("using 1 material rule", generated.stdout)

            dumped = subprocess.run(
                [str(tool), "--dump", str(output)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            self.assertEqual(dumped.returncode, 0, dumped.stderr)
            self.assertIn("audiozones version 3", dumped.stdout)
            self.assertIn("preset hallway", dumped.stdout)
            self.assertIn("material metal", dumped.stdout)
            self.assertIn("flags 3", dumped.stdout)

    def test_split_leaf_classification_uses_aggregate_room_bounds(self) -> None:
        dumped = self.generate_and_dump(
            build_bsp(
                ["textures/plain/wall"],
                [
                    (0, 0, (0, 0, 0), (64, 512, 128), 0),
                    (0, 0, (64, 0, 0), (512, 512, 128), 0),
                ],
            )
        )

        self.assertIn("audiozones version 3, zones 1", dumped)
        self.assertIn("preset room", dumped)
        self.assertNotIn("preset hallway", dumped)

    def test_adjacent_split_zones_merge_across_clusters(self) -> None:
        dumped = self.generate_and_dump(
            build_bsp(
                ["textures/plain/wall"],
                [
                    (0, 0, (0, 0, 0), (256, 512, 128), 0),
                    (1, 0, (256, 0, 0), (512, 512, 128), 0),
                ],
            )
        )

        self.assertIn("audiozones version 3, zones 1", dumped)
        self.assertIn("preset room", dumped)

    def test_doorway_connected_room_and_hallway_stay_separate_with_portal(self) -> None:
        dumped = self.generate_and_dump(
            build_bsp(
                ["textures/plain/wall"],
                [
                    (0, 0, (0, 0, 0), (512, 512, 128), 0),
                    (1, 0, (512, 192, 0), (896, 320, 128), 0),
                ],
            )
        )

        self.assertIn("audiozones version 3, zones 2", dumped)
        self.assertIn("preset room", dumped)
        self.assertIn("preset hallway", dumped)
        self.assertEqual(dumped.count("  portal target "), 2)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
