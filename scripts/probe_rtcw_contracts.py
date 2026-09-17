#!/usr/bin/env python3
"""Compile every frozen RTCW field against user-supplied id Software headers.

This is source/ABI evidence only. It loads no game module or asset and does not
establish gameplay, networking or renderer compatibility.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BOUNDARY = ROOT / "code/compat/rtcw_game_abi.h"
MODULE_CALLS = ROOT / "code/compat/rtcw_module_calls.def"
KEY_CODES = ROOT / "code/compat/rtcw_keycodes.def"
MENUS = ROOT / "code/compat/rtcw_menus.def"
BSP_LAYOUT = ROOT / "code/compat/rtcw_bsp_layout.def"
MDC_LAYOUT = ROOT / "code/compat/rtcw_mdc_layout.def"
MDS_LAYOUT = ROOT / "code/compat/rtcw_mds_layout.def"
TRAJECTORIES = ROOT / "code/compat/rtcw_trajectory_types.def"
HEADERS = ("game/q_shared.h", "cgame/tr_types.h", "cgame/cg_public.h",
           "game/g_public.h", "ui/ui_public.h", "game/bg_public.h", "ui/keycodes.h",
           "qcommon/qfiles.h", "qcommon/cm_local.h", "client/cl_cgame.c", "client/cl_ui.c",
           "game/botlib.h", "game/be_ai_goal.h", "game/be_ai_move.h", "game/be_aas.h")


def records() -> dict[str, list[tuple[str, str, bool]]]:
    """Read the independently authored grouped records, not reference code."""
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", BOUNDARY.read_text(), flags=re.S)
    result = {}
    for body, name in re.findall(r"typedef\s+struct\s*\{([^{}]*)\}\s*(\w+)\s*;", text):
        fields = []
        for declaration in body.split(";"):
            if not declaration.strip():
                continue
            scalar, declarators = declaration.strip().split(maxsplit=1)
            for declarator in declarators.split(","):
                match = re.fullmatch(r"\s*(\w+)\s*((?:\[[^]]+\])*)\s*", declarator)
                if not match:
                    raise ValueError(f"unsupported declaration in {name}: {declaration}")
                fields.append((scalar, match[1], bool(match[2])))
        result[name] = fields
    return result


def fields_for(types: dict, name: str, prefix: str = "") -> list[str]:
    result = []
    for scalar, field, array in types[name]:
        path = prefix + field
        if scalar in types and not array:
            result.extend(fields_for(types, scalar, path + "."))
        else:
            result.append(path)
    return result


def reference_field(field: str, iortcw: bool = False) -> str:
    parts = [part for part in field.split(".") if part not in ("common", "appearance")]
    if parts[-1] == "padding":
        parts[-1] = "pad"
    if parts[-1] == "worldflags":
        parts.insert(-1, "r")
    if parts[-1] == "fogDirty":
        parts[-1:] = ["glfog", "dirty"]
    if iortcw and parts[:2] == ["r", "s"]:
        parts[1] = "unused"
    if iortcw and parts == ["maxActiveTextures"]:
        parts[0] = "numTextureUnits"
    return ".".join(parts)


def translation_unit(mode: str, types: dict, iortcw: bool = False) -> tuple[str, int]:
    lines = []
    if iortcw:
        # iortcw's legacy MSVC header redeclares int8_t as __int8 (char under
        # Clang); isolate that spelling from the standard signed-char typedef.
        # Its Windows build also supplies __WIN64__ for the platform header.
        lines += ['#include <stdint.h>', '#if defined(_MSC_VER)',
                  '#define int8_t fntech3_probe_reference_int8_t', '#endif',
                  '#if defined(_WIN64) && !defined(__WIN64__)', '#define __WIN64__ 1', '#endif']
    lines += ['#include "q_shared.h"', '#undef int8_t', '#include "tr_types.h"',
             '#include "cg_public.h"', '#include "g_public.h"',
             '#include "ui_public.h"', '#include "bg_public.h"', '#include "keycodes.h"',
             '#include "botlib.h"', '#include "be_ai_goal.h"', '#include "be_ai_move.h"',
             '#include "be_aas.h"', '#undef SE_NONE', '#include <stddef.h>',
             f'#include "{BOUNDARY.as_posix()}"']
    pairs = [(f"fntech3_rtcw_{name}", original) for name, original in (
        ("trajectory_t", "trajectory_t"), ("plane_t", "cplane_t"),
        ("bot_entitystate_t", "bot_entitystate_t"),
        ("bot_initmove_t", "bot_initmove_t"),
        ("bot_goal_t", "bot_goal_t"),
        ("bot_moveresult_t", "bot_moveresult_t"),
        ("bot_input_t", "bot_input_t"),
        ("aas_trace_t", "aas_trace_t"), ("aas_clientmove_t", "aas_clientmove_t"),
        ("trace_t", "trace_t"), ("gameState_t", "gameState_t"),
        ("entityState_t", "entityState_t"), ("vmCvar_t", "vmCvar_t"),
        ("uiClientState_t", "uiClientState_t"), ("glconfig_t", "glconfig_t"),
        ("pc_token_t", "pc_token_t"), ("qtime_t", "qtime_t"),
        ("glyphInfo_t", "glyphInfo_t"), ("fontInfo_t", "fontInfo_t"),
        ("markFragment_t", "markFragment_t"), ("polyVert_t", "polyVert_t"), ("orientation_t", "orientation_t"))]
    pairs += [(f"fntech3_rtcw_{mode}_{name}", name) for name in (
        "usercmd_t", "playerState_t", "sharedEntity_t", "snapshot_t", "refEntity_t", "refdef_t")]
    lines.append('_Static_assert(sizeof(fntech3_rtcw_markPoint_t) == sizeof(vec5_t), "RTCW mark point stride");')
    lines.append('_Static_assert(offsetof(fntech3_rtcw_markPoint_t, st) == 3 * sizeof(float), "RTCW mark ST offset");')
    checks = 2
    lines.append('_Static_assert(_Generic(((aas_clientmove_t*)0)->endcontents, float: 1, default: 0), "RTCW prediction contents are numeric float");')
    checks += 1
    lines.append('_Static_assert(FNTECH3_RTCW_GENTITYNUM_BITS == GENTITYNUM_BITS, "RTCW game entity-number limit");')
    checks += 1
    for own, original in pairs:
        lines.append(f'_Static_assert(sizeof({own}) == sizeof({original}), "{original} size");')
        lines.append(f'_Static_assert(_Alignof({own}) == _Alignof({original}), "{original} alignment");')
        checks += 2
        for field in fields_for(types, own):
            other = reference_field(field, iortcw)
            label = f"{original}.{other}"
            lines.append(f'_Static_assert(offsetof({own},{field}) == offsetof({original},{other}), "{label} offset");')
            lines.append(f'_Static_assert(sizeof((({own}*)0)->{field}) == sizeof((({original}*)0)->{other}), "{label} size");')
            checks += 2
    lines.append(f'#include "{(ROOT / "code/compat/rtcw_module_abi.h").as_posix()}"')
    for name in re.findall(r"FNTECH3_RTCW_((?:SVF|CVAR|CA|TC|GLDRV|GLHW|TT|CGAME_EVENT|STEREO|KEYCATCH|K|RT|RF|RDF|REFLAG|MFL|ACTION)_\w+)\s*=", BOUNDARY.read_text()):
        prefix = "IORTCW" if iortcw and name == "TC_EXT_COMP_S3TC" else "RTCW"
        lines.append(f'_Static_assert(FNTECH3_{prefix}_{name} == {name}, "{name} encoding");')
        checks += 1
    if mode == "sp":
        for name in re.findall(r"FNTECH3_RTCW_SP_((?:RDF|REFLAG)_\w+)\s*=", BOUNDARY.read_text()):
            lines.append(f'_Static_assert(FNTECH3_RTCW_SP_{name} == {name}, "SP {name} encoding");')
            checks += 1
    for name in re.findall(r"FNTECH3_RTCW_KEY\((K_\w+), \d+\)", KEY_CODES.read_text()):
        lines.append(f'_Static_assert(FNTECH3_RTCW_{name} == {name}, "{name} key number");')
        checks += 1
    for name, ordinal in re.findall(r"^FNTECH3_RTCW_TRAJECTORY\((\w+), \w+, (\d+)\)", TRAJECTORIES.read_text(), re.M):
        lines.append(f'_Static_assert({name} == {ordinal}, "{name} trajectory number");')
        checks += 1
    for name, sp, mp in re.findall(r"FNTECH3_RTCW_MENU\((\w+), (-?\d+), (-?\d+)\)", MENUS.read_text()):
        if int(sp if mode == "sp" else mp) >= 0:
            lines.append(f'_Static_assert(FNTECH3_RTCW_{mode.upper()}_UIMENU_{name} == UIMENU_{name}, "{name} menu");')
            checks += 1
    if mode == "mp":
        for name in re.findall(r"FNTECH3_RTCW_MP_(GS_\w+)\s*=", BOUNDARY.read_text()):
            lines.append(f'_Static_assert(FNTECH3_RTCW_MP_{name} == {name}, "MP {name} state");')
            checks += 1
        prefix = "IORTCW" if iortcw else "RTCW"
        lines.append(f'_Static_assert(FNTECH3_{prefix}_MP_CVAR_WOLFINFO == CVAR_WOLFINFO, "MP WOLFINFO encoding");')
        lines.append('_Static_assert(FNTECH3_RTCW_MP_CS_WOLFINFO == CS_WOLFINFO, "MP WOLFINFO channel");')
        checks += 2
    for own, reference in (("UI", "UI_API_VERSION"), ("CGAME", "CGAME_IMPORT_API_VERSION"),
                           ("GAME", "GAME_API_VERSION")):
        lines.append(f'_Static_assert(FNTECH3_RTCW_{own}_API_VERSION == {reference}, "{reference}");')
        checks += 1
    for name, sp, mp in re.findall(
            r"FNTECH3_RTCW_MODULE_CALL\(\w+, \w+, (\w+), (-?\d+), (-?\d+)\)",
            MODULE_CALLS.read_text()):
        if int(sp if mode == "sp" else mp) >= 0:
            lines.append(f'_Static_assert(FNTECH3_RTCW_{mode.upper()}_{name} == {name}, "{name} ordinal");')
            checks += 1
    lines += ['#include "qfiles.h"',
              '#define FNTECH3_RTCW_BSP_RECORD(type, bytes, alignment) _Static_assert(sizeof(type) == bytes && _Alignof(type) == alignment, #type " layout");',
              '#define FNTECH3_RTCW_BSP_FIELD(type, field, offset, bytes) _Static_assert(offsetof(type, field) == offset && sizeof(((type *)0)->field) == bytes, #type "." #field " layout");',
              '#define FNTECH3_RTCW_BSP_CONSTANT(name, value) _Static_assert(name == value, #name " encoding");',
              f'#include "{BSP_LAYOUT.as_posix()}"',
              '_Static_assert(BSP_VERSION == 47, "RTCW BSP version");']
    checks += len(re.findall(r'^FNTECH3_RTCW_BSP_', BSP_LAYOUT.read_text(), re.M)) + 1
    lines += ['#define FNTECH3_RTCW_MDC_RECORD(type, bytes, alignment) _Static_assert(sizeof(type) == bytes && _Alignof(type) == alignment, #type " layout");',
              '#define FNTECH3_RTCW_MDC_FIELD(type, field, offset, bytes) _Static_assert(offsetof(type, field) == offset && sizeof(((type *)0)->field) == bytes, #type "." #field " layout");',
              '#define FNTECH3_RTCW_MDC_CONSTANT(name, value) _Static_assert(name == value, #name " encoding");',
              f'#include "{MDC_LAYOUT.as_posix()}"']
    checks += len(re.findall(r'^FNTECH3_RTCW_MDC_', MDC_LAYOUT.read_text(), re.M))
    lines += ['#define FNTECH3_RTCW_MDS_RECORD(type, bytes, alignment) _Static_assert(sizeof(type) == bytes && _Alignof(type) == alignment, #type " layout");',
              '#define FNTECH3_RTCW_MDS_FIELD(type, field, offset, bytes) _Static_assert(offsetof(type, field) == offset && sizeof(((type *)0)->field) == bytes, #type "." #field " layout");',
              '#define FNTECH3_RTCW_MDS_CONSTANT(name, value) _Static_assert(name == value, #name " encoding");',
              f'#include "{MDS_LAYOUT.as_posix()}"']
    checks += len(re.findall(r'^FNTECH3_RTCW_MDS_', MDS_LAYOUT.read_text(), re.M))
    lines += ['#include "cm_local.h"',
              '_Static_assert(MAX_SUBMODELS == 512, "RTCW collision handle range");',
              '_Static_assert(CAPSULE_MODEL_HANDLE == 510, "RTCW capsule handle");',
              '_Static_assert(BOX_MODEL_HANDLE == 511, "RTCW box handle");',
              '_Static_assert(CONTENTS_BODY == 0x02000000, "RTCW temporary hull contents");']
    checks += 4
    return "\n".join(lines) + "\n", checks


def probe(compiler: str, target: str | None, mode: str, source: Path,
          scratch: Path, types: dict, iortcw: bool = False) -> dict:
    reference = source.resolve(strict=True) / ("code" if iortcw else "src")
    hashes = {}
    for header in HEADERS:
        if iortcw:
            header = {"game/q_shared.h": "qcommon/q_shared.h",
                      "cgame/tr_types.h": "renderer/tr_types.h",
                      "ui/keycodes.h": "client/keycodes.h",
                      "game/botlib.h": "botlib/botlib.h",
                      "game/be_ai_goal.h": "botlib/be_ai_goal.h",
                      "game/be_ai_move.h": "botlib/be_ai_move.h",
                      "game/be_aas.h": "botlib/be_aas.h"}.get(header, header)
        hashes[header] = hashlib.sha256((reference / header).read_bytes()).hexdigest()
    unit, checks = translation_unit(mode, types, iortcw)
    path = scratch / f"{mode}.c"
    path.write_text(unit, encoding="utf-8")
    command = [compiler, "-std=c11", "-fsyntax-only", "-Wno-unknown-pragmas",
               "-Wno-microsoft", "-Wno-comment"]
    if target:
        command += ["--target=" + target]
    for directory in ("game", "cgame", "ui", "qcommon", "renderer", "client", "botlib"):
        command += ["-I", str(reference / directory)]
    run = subprocess.run([*command, str(path)], cwd=ROOT, capture_output=True,
                         text=True, timeout=60)
    return {"profile": f"rtcw-{mode}", "reference": str(source.resolve()),
            "referenceVariant": "iortcw" if iortcw else "original",
            "sourceSha256": hashes, "staticAssertions": checks,
            "passed": run.returncode == 0, "diagnostics": run.stdout + run.stderr}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sp-source", type=Path, required=True)
    parser.add_argument("--mp-source", type=Path, required=True)
    parser.add_argument("--iortcw-source", type=Path, help="Also check SP/MP contracts in an iortcw tree")
    parser.add_argument("--compiler", default="clang")
    parser.add_argument("--target", help="Clang target, e.g. i686-pc-windows-msvc")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error(f"compiler not found: {args.compiler}")
    scratch = ROOT / ".tmp"
    scratch.mkdir(exist_ok=True)
    types = records()
    with tempfile.TemporaryDirectory(prefix="rtcw-contracts-", dir=scratch) as directory:
        results = [probe(compiler, args.target, mode, source, Path(directory), types)
                   for mode, source in (("sp", args.sp_source), ("mp", args.mp_source))]
        if args.iortcw_source:
            results += [probe(compiler, args.target, mode, args.iortcw_source / mode.upper(),
                              Path(directory), types, True) for mode in ("sp", "mp")]
    report = {"schemaVersion": 1, "evidence": "Source/Unit", "compiler": compiler,
              "target": args.target or "compiler default",
              "boundarySha256": hashlib.sha256(BOUNDARY.read_bytes()).hexdigest(),
              "moduleCallsSha256": hashlib.sha256(MODULE_CALLS.read_bytes()).hexdigest(),
              "keyCodesSha256": hashlib.sha256(KEY_CODES.read_bytes()).hexdigest(),
              "menusSha256": hashlib.sha256(MENUS.read_bytes()).hexdigest(),
              "bspLayoutSha256": hashlib.sha256(BSP_LAYOUT.read_bytes()).hexdigest(),
              "mdcLayoutSha256": hashlib.sha256(MDC_LAYOUT.read_bytes()).hexdigest(),
              "mdsLayoutSha256": hashlib.sha256(MDS_LAYOUT.read_bytes()).hexdigest(),
              "trajectoryTypesSha256": hashlib.sha256(TRAJECTORIES.read_bytes()).hexdigest(),
              "results": results}
    output = args.report.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for result in results:
        print(f"{result['profile']} ({result['referenceVariant']}): {'PASS' if result['passed'] else 'FAIL'} "
              f"({result['staticAssertions']} static assertions)")
        if result["diagnostics"]:
            print(result["diagnostics"])
    return 0 if all(result["passed"] for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
