"""Find Medal of Honor: Allied Assault's official maps in a retail installation.

Both MoHAA sidecar generators read their input here. The installation is only
ever read: callers extract what they need into their own working directories.
The 1.00 installation keeps six packages, `main/Pak0.pk3` to `Pak5.pk3`, their
names capitalised; only Pak5 carries maps, some of them in folders under
maps/ (`DM/`, `obj/`, `briefing/`).
"""
from __future__ import annotations

import re
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from stock_mohaa_maps import STOCK_MOHAA_MAPS

PROFILE = "mohaa"
GAME_DIRECTORY = "main"
# The retail release's own packages, by name: a pattern would admit a custom
# pak named like them. Only Pak5 carries maps; the rest must not add any.
OFFICIAL_PAKS = frozenset(f"pak{index}.pk3" for index in range(6))
# A map's path under maps/: at most one folder, each part a plain name.
MAP_ENTRY = re.compile(r"maps/((?:[a-z0-9][a-z0-9_-]*/)?[a-z0-9][a-z0-9_-]*)\.bsp", re.I)


def resolve_game_directory(path: Path) -> Path:
    root = path.expanduser().resolve()
    if root.is_dir() and root.name.lower() == GAME_DIRECTORY:
        return root
    candidates = [entry for entry in root.iterdir()
                  if entry.is_dir() and entry.name.lower() == GAME_DIRECTORY]
    if len(candidates) != 1:
        raise ValueError(f"expected one Medal of Honor {GAME_DIRECTORY} directory under {root}")
    return candidates[0]


def discover_official_maps(game: Path) -> dict[str, tuple[Path, str]]:
    """Every BSP in the official paks, by its inventory name, refusing a changed or ambiguous inventory."""
    paks: dict[str, Path] = {}
    for pak in game.iterdir():
        key = pak.name.lower()
        if not pak.is_file() or key not in OFFICIAL_PAKS:
            continue
        if key in paks:
            raise ValueError(f"case-ambiguous official packages: {paks[key]} and {pak}")
        paks[key] = pak
    found: dict[str, tuple[Path, str]] = {}
    for _, pak in sorted(paks.items()):
        with zipfile.ZipFile(pak) as archive:
            for entry in archive.namelist():
                key = entry.lower()
                if not key.startswith("maps/") or not key.endswith(".bsp"):
                    continue
                match = MAP_ENTRY.fullmatch(entry)
                if not match:
                    raise ValueError(f"unsafe BSP entry in {pak.name}: {entry!r}")
                if match[1].lower() in found:
                    raise ValueError(f"duplicate or case-ambiguous BSP: {entry!r} in {pak.name}")
                found[match[1].lower()] = (pak, entry)
    expected = {name.lower(): name for name in STOCK_MOHAA_MAPS}
    missing, unexpected = expected.keys() - found.keys(), found.keys() - expected.keys()
    if missing or unexpected:
        raise ValueError(f"official Medal of Honor map inventory mismatch: "
                         f"missing={sorted(missing)}, unexpected={sorted(unexpected)}")
    maps = {expected[key]: value for key, value in found.items()}
    for name, (pak, entry) in maps.items():
        # The inventory spells each map as the pak does, folders included.
        if entry != f"maps/{name}.bsp":
            raise ValueError(f"{pak.name} spells {name!r} as {entry!r}")
    return maps


def read_map(pak: Path, entry: str) -> tuple[bytes, int]:
    """A map's bytes and the CRC-32 its package records for them."""
    with zipfile.ZipFile(pak) as archive:
        info = archive.getinfo(entry)
        return archive.read(info), info.CRC
