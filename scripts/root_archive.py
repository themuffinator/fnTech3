from __future__ import annotations

import json
import math
import os
import re
import zipfile
from pathlib import Path
from typing import Iterable

from fntech3_meta import ROOT
from stock_alice_maps import SIDECAR_SUFFIXES as ALICE_SIDECAR_SUFFIXES, STOCK_ALICE_MAPS
from stock_fakk2_maps import SIDECAR_SUFFIXES as FAKK2_SIDECAR_SUFFIXES, STOCK_FAKK2_MAPS
from stock_mohaa_maps import SIDECAR_SUFFIXES as MOHAA_SIDECAR_SUFFIXES, STOCK_MOHAA_MAPS
from stock_q3_assets import Q3_PACKAGE_ASSETS
from stock_ql_maps import STOCK_QL_MAPS
from stock_rtcw_maps import STOCK_RTCW_MAPS
from stock_wolfet_maps import STOCK_WOLFET_MAPS


ROOT_ARCHIVE_NAME = "fntech3-pkg.fnz"
PKG_ROOT = ROOT / "pkg"
# Canonical Com_GameProfile()->key values, never startup aliases or title IDs.
GAME_PROFILES = ("quake3", "quakelive", "rtcw-sp", "rtcw-mp", "wolfet", "stvef-sp", "stvef-mp", "alice", "fakk2",
                 "mohaa")
# These paths are relative to games/<canonical profile>/. A title can have
# multiple game directories (for example quake3/baseq3 and quake3/missionpack).
PROFILE_REQUIRED_ASSETS = {
    "quake3": Q3_PACKAGE_ASSETS,
    "quakelive": (
        *(f"baseq3/maps/{name}.azb" for name in STOCK_QL_MAPS),
        *(f"baseq3/maps/{name}.fog" for name in STOCK_QL_MAPS),
        "baseq3/scripts/fnql.shader",
        "baseq3/sound/fnql-weapon-sounds.sndshd",
    ),
    **{
        profile: (
            "main/fntech3-hud.json",
            *(f"main/maps/{name}.azb" for name in names),
            # Each official map's static lights, converted for its own mode.
            *(f"main/maps/{name}.lights.json" for name in names),
        )
        for profile, names in STOCK_RTCW_MAPS.items()
    },
    # Enemy Territory ships a zone file for each official map, and the HUD rules.
    "wolfet": ("etmain/fntech3-hud.json", *(f"etmain/maps/{name}.azb" for name in STOCK_WOLFET_MAPS)),
    # Elite Force ships each mode's HUD rules.
    "stvef-sp": ("baseEF/fntech3-hud.json",),
    "stvef-mp": ("baseEF/fntech3-hud.json",),
    # Alice ships an audio zone file and a static light file for every official map,
    # and the HUD rules that place its HUD.
    "alice": (
        "base/fntech3-hud.json",
        *(f"base/maps/{name}{suffix}" for name in STOCK_ALICE_MAPS
          for suffix in ALICE_SIDECAR_SUFFIXES),
    ),
    # Heavy Metal: F.A.K.K.2 ships the same two files for every official map,
    # and the HUD rules that place its HUD menus.
    "fakk2": (
        "fakk/fntech3-hud.json",
        *(f"fakk/maps/{name}{suffix}" for name in STOCK_FAKK2_MAPS
          for suffix in FAKK2_SIDECAR_SUFFIXES),
    ),
    # Medal of Honor: Allied Assault ships the same two files for every official
    # map, each beside its map in the map's own folder.
    "mohaa": tuple(f"main/maps/{name}{suffix}" for name in STOCK_MOHAA_MAPS
                   for suffix in MOHAA_SIDECAR_SUFFIXES),
}
DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS = [
    (PKG_ROOT / "games" / profile / name, Path("games") / profile / name)
    for profile, names in PROFILE_REQUIRED_ASSETS.items()
    for name in names
]
DEFAULT_AUDIO_ZONE_ASSETS = [asset for asset in DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS if asset[1].suffix == ".azb"]
DEFAULT_GLOBAL_FOG_ASSETS = [asset for asset in DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS if asset[1].suffix == ".fog"]
DEFAULT_WEAPON_SOUND_SHADER_ASSETS = [asset for asset in DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS if asset[1].suffix == ".sndshd"]
DEFAULT_RENDERER_SHADER_ASSETS = [asset for asset in DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS if asset[1].suffix == ".shader"]
DEFAULT_STATIC_LIGHT_ASSETS = [asset for asset in DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS
                               if asset[1].name.endswith(".lights.json")]


def iter_package_assets(package_root: Path = PKG_ROOT) -> list[tuple[Path, Path]]:
    package_root = package_root.expanduser()
    resolved_package_root = package_root.resolve()
    if not package_root.is_dir():
        raise FileNotFoundError(f"Missing root archive source directory: {package_root}")

    assets: list[tuple[Path, Path]] = []
    for source in sorted(package_root.rglob("*"), key=lambda path: path.as_posix().lower()):
        if source.is_symlink():
            raise ValueError(f"Root archive package asset must not be a symbolic link: {source}")
        if not source.is_file():
            continue

        resolved_source = source.resolve()
        if not path_is_relative_to(resolved_source, resolved_package_root):
            raise ValueError(f"Root archive package asset escapes package root: {source}")

        dest_relative = source.relative_to(package_root)
        if dest_relative.name == ".gitkeep":
            continue

        package_member_name(dest_relative)
        assets.append((source, dest_relative))

    if not assets:
        raise FileNotFoundError(f"No root archive assets found under {package_root}")

    return assets


def path_is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
    except ValueError:
        return False
    return True


def archive_member_name(dest_relative: Path | str) -> str:
    archive_name = (
        dest_relative.as_posix()
        if isinstance(dest_relative, Path)
        else str(dest_relative)
    )
    if not archive_name:
        raise ValueError("Root archive entry name must not be empty")
    if "\\" in archive_name:
        raise ValueError(f"Root archive entry must use forward slashes: {archive_name}")
    if archive_name.startswith("/"):
        raise ValueError(f"Root archive entry must be relative: {archive_name}")

    parts = archive_name.split("/")
    if any(part in {"", ".", ".."} for part in parts):
        raise ValueError(f"Root archive entry contains unsafe path component: {archive_name}")
    if any(":" in part for part in parts):
        raise ValueError(f"Root archive entry must not contain a drive prefix or stream name: {archive_name}")
    if any(any(ord(char) < 32 or ord(char) == 127 for char in part) for part in parts):
        raise ValueError(f"Root archive entry contains unsafe control character: {archive_name!r}")
    return archive_name


def package_member_name(dest_relative: Path | str) -> str:
    """Validate the runtime namespace as well as ZIP path safety."""
    name = archive_member_name(dest_relative)
    parts = name.split("/")
    if len(parts) < 4 or parts[0] != "games" or parts[1] not in GAME_PROFILES:
        raise ValueError(
            f"Package asset must use games/<profile>/<gamedir>/<qpath> "
            f"with a canonical game profile ({', '.join(GAME_PROFILES)}): {name}"
        )
    # Keep in sync with MAX_ZPATH: the loader never truncates candidate names.
    if len(name.encode("utf-8")) >= 256:
        raise ValueError(f"Package asset path exceeds the engine's MAX_ZPATH: {name}")
    return name


def validate_archive_member_names(archived_names: Iterable[str], *, archive_name: str) -> None:
    seen: set[str] = set()
    for name in archived_names:
        try:
            safe_name = archive_member_name(name)
        except ValueError as exc:
            raise ValueError(f"{archive_name} contains unsafe package asset path: {name}") from exc
        key = safe_name.lower()
        if key in seen:
            raise ValueError(f"{archive_name} contains duplicate package asset path: {safe_name}")
        seen.add(key)


def required_root_archive_names(
    assets: Iterable[tuple[Path, Path]] = DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS,
) -> list[str]:
    return [archive_member_name(dest_relative) for _source, dest_relative in assets]


def required_audio_zone_archive_names(
    assets: Iterable[tuple[Path, Path]] = DEFAULT_AUDIO_ZONE_ASSETS,
) -> list[str]:
    return required_root_archive_names(assets)


def required_global_fog_archive_names(
    assets: Iterable[tuple[Path, Path]] = DEFAULT_GLOBAL_FOG_ASSETS,
) -> list[str]:
    return required_root_archive_names(assets)


def validate_root_archive_names(
    archived_names: Iterable[str],
    *,
    archive_name: str = ROOT_ARCHIVE_NAME,
    assets: Iterable[tuple[Path, Path]] = DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS,
) -> None:
    archived_name_list = list(archived_names)
    validate_archive_member_names(archived_name_list, archive_name=archive_name)
    for name in archived_name_list:
        package_member_name(name)
    present_names = set(archived_name_list)
    missing_assets = [
        name
        for name in required_root_archive_names(assets)
        if name not in present_names
    ]
    if missing_assets:
        raise ValueError(
            f"{archive_name} is missing required package assets: "
            + ", ".join(missing_assets[:8])
        )


# What every renderer's static light parser accepts: RTX's refuses a file
# over 256 KB or 65,536 tokens and a repeated key, all keep 128 lights a map
# and draw nothing without a positive radius or intensity (each stands in for
# the other), and none is trusted with an exponent.
STATIC_LIGHT_MAX_BYTES = 256 * 1024
STATIC_LIGHT_MAX_TOKENS = 65536
STATIC_LIGHT_MAX_LIGHTS = 128


def validate_static_light_file(name: str, payload: bytes) -> None:
    """Refuse a packaged maps/<map>.lights.json that a renderer would reject or cut short."""
    def fail(reason: str) -> None:
        raise ValueError(f"{name}: {reason}")

    def unique(pairs):
        keys = [key for key, _value in pairs]
        if len(keys) != len(set(keys)):
            fail("repeats a key")
        return dict(pairs)

    def refuse_constant(constant: str):
        fail(f"has a non-finite number {constant}")

    if len(payload) > STATIC_LIGHT_MAX_BYTES:
        fail(f"is over {STATIC_LIGHT_MAX_BYTES} bytes")
    if b"\0" in payload:
        fail("contains a NUL byte")
    if len(re.findall(rb'"[^"]*"|[{}\[\],:]|[^\s{}\[\],:"]+', payload)) >= STATIC_LIGHT_MAX_TOKENS:
        fail(f"has {STATIC_LIGHT_MAX_TOKENS} tokens or more")
    if re.search(rb"\d[eE][-+]?\d", re.sub(rb'"[^"]*"', b'""', payload)):
        fail("writes a number with an exponent")
    try:
        document = json.loads(payload.decode("utf-8"), object_pairs_hook=unique,
                              parse_constant=refuse_constant)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        fail(f"is not JSON ({exc})")
    if not isinstance(document, dict) or document.get("version") != 1:
        fail("is not a version 1 light file")
    entries = document.get("lights")
    if not isinstance(entries, list) or len(entries) > STATIC_LIGHT_MAX_LIGHTS:
        fail(f"does not hold a list of at most {STATIC_LIGHT_MAX_LIGHTS} lights")

    def numbers(value, count: int) -> bool:
        return (isinstance(value, list) and len(value) == count and
                all(isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v) for v in value))

    for index, entry in enumerate(entries):
        if not isinstance(entry, dict) or entry.get("type") not in ("point", "spot"):
            fail(f"light {index} is not a point or spot light")
        if not numbers(entry.get("origin"), 3) or not numbers(entry.get("color", [1, 1, 1]), 3):
            fail(f"light {index} has no origin or a malformed colour")
        if entry["type"] == "spot" and not (numbers(entry.get("direction"), 3) and any(entry["direction"])):
            fail(f"light {index} is a spot without a direction")
        sizes = [entry.get("radius", 0), entry.get("intensity", 0)]
        if not numbers(sizes, 2) or max(sizes) <= 0:
            fail(f"light {index} has neither a positive radius nor a positive intensity")


def validate_root_archive(archive_path: Path) -> None:
    with zipfile.ZipFile(archive_path) as archive:
        archived_names = [
            info.filename
            for info in archive.infolist()
            if not info.is_dir()
        ]
        light_files = {name: archive.read(name) for name in archived_names if name.endswith(".lights.json")}
    validate_root_archive_names(archived_names, archive_name=archive_path.name)
    for name, payload in light_files.items():
        validate_static_light_file(name, payload)


def write_root_archive(
    archive_path: Path,
    *,
    package_root: Path = PKG_ROOT,
    assets: Iterable[tuple[Path, Path]] | None = None,
    required_assets: Iterable[tuple[Path, Path]] | None = None,
) -> None:
    archive_path = archive_path.expanduser()
    package_root = package_root.expanduser()
    resolved_package_root = package_root.resolve()
    if path_is_relative_to(archive_path.resolve(), resolved_package_root):
        raise ValueError(f"Archive output must not be inside package source tree: {archive_path}")

    archive_assets = list(iter_package_assets(package_root) if assets is None else assets)
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    temp_path = archive_path.with_name(f"{archive_path.name}.tmp")

    if temp_path.exists():
        temp_path.unlink()

    try:
        with zipfile.ZipFile(temp_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            archived_names: set[str] = set()
            for source, dest_relative in archive_assets:
                source = source.expanduser()
                if not source.is_file():
                    raise FileNotFoundError(f"Missing root archive asset: {source}")
                if source.is_symlink():
                    raise ValueError(f"Root archive package asset must not be a symbolic link: {source}")
                if not path_is_relative_to(source.resolve(), resolved_package_root):
                    raise ValueError(f"Root archive package asset escapes package root: {source}")

                archive_name = package_member_name(dest_relative)
                archive_key = archive_name.lower()
                if archive_key in archived_names:
                    raise ValueError(f"Duplicate root archive entry: {archive_name}")
                archived_names.add(archive_key)

                info = zipfile.ZipInfo(archive_name)
                info.create_system = 3  # Fixed UNIX metadata, including on Windows.
                info.compress_type = zipfile.ZIP_DEFLATED
                info.date_time = (1980, 1, 1, 0, 0, 0)
                info.external_attr = 0o644 << 16
                archive.writestr(info, source.read_bytes())

        if required_assets is not None:
            validate_root_archive_names(
                (package_member_name(dest) for _source, dest in archive_assets),
                archive_name=archive_path.name,
                assets=required_assets,
            )
        os.replace(temp_path, archive_path)
    finally:
        if temp_path.exists():
            temp_path.unlink()
