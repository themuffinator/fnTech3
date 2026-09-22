from __future__ import annotations

import os
import zipfile
from pathlib import Path
from typing import Iterable

from fntech3_meta import ROOT
from stock_q3_assets import Q3_PACKAGE_ASSETS
from stock_ql_maps import STOCK_QL_MAPS
from stock_rtcw_maps import STOCK_RTCW_MAPS
from stock_stvef_maps import STOCK_STVEF_MAPS
from stock_wolfet_maps import STOCK_WOLFET_MAPS


ROOT_ARCHIVE_NAME = "fntech3-pkg.fnz"
PKG_ROOT = ROOT / "pkg"
# Canonical Com_GameProfile()->key values, never startup aliases or title IDs.
GAME_PROFILES = ("quake3", "quakelive", "rtcw-sp", "rtcw-mp", "wolfet", "stvef-sp", "stvef-mp")
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
        )
        for profile, names in STOCK_RTCW_MAPS.items()
    },
    "wolfet": tuple(f"etmain/maps/{name}.azb" for name in STOCK_WOLFET_MAPS),
    # Elite Force shares one baseEF game directory between its two profiles,
    # and ten single-player maps live in the maps/tour/ subdirectory.
    **{
        profile: (
            "baseEF/fntech3-hud.json",
            *(f"baseEF/maps/{name}.azb" for name in names),
        )
        for profile, names in STOCK_STVEF_MAPS.items()
    },
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


def validate_root_archive(archive_path: Path) -> None:
    with zipfile.ZipFile(archive_path) as archive:
        archived_names = [
            info.filename
            for info in archive.infolist()
            if not info.is_dir()
        ]
    validate_root_archive_names(archived_names, archive_name=archive_path.name)


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
