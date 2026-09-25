"""Find American McGee's Alice's official maps in a retail installation.

Both Alice sidecar generators read their input here. The installation is only
ever read: callers extract what they need into their own working directories.
"""
from __future__ import annotations

import re
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from stock_alice_maps import STOCK_ALICE_MAPS

PROFILE = "alice"
GAME_DIRECTORY = "base"
# The retail release's own packages, by name: a pattern would admit a custom
# pak named like them. Only pak0 carries maps; the rest must not add any.
OFFICIAL_PAKS = frozenset((
    "pak0.pk3", "pak1_large.pk3", "pak1_small.pk3", "pak2.pk3", "pak3.pk3",
    "pak4_english.pk3", "pak4_french.pk3", "pak4_german.pk3", "pak4_spanish.pk3",
    "pak5_dutch.pk3",
))
MAP_ENTRY = re.compile(r"maps/([a-z0-9][a-z0-9_-]*)\.bsp", re.I)


def resolve_game_directory(path: Path) -> Path:
    root = path.expanduser().resolve()
    if root.is_dir() and root.name.lower() == GAME_DIRECTORY:
        return root
    candidates = [entry for entry in root.iterdir()
                  if entry.is_dir() and entry.name.lower() == GAME_DIRECTORY]
    if len(candidates) != 1:
        raise ValueError(f"expected one Alice {GAME_DIRECTORY} directory under {root}")
    return candidates[0]


def discover_official_maps(game: Path) -> dict[str, tuple[Path, str]]:
    """Every BSP in the official paks, refusing a changed or ambiguous inventory."""
    paks: dict[str, Path] = {}
    for pak in game.iterdir():
        key = pak.name.lower()
        if not pak.is_file() or key not in OFFICIAL_PAKS:
            continue
        if key in paks:
            raise ValueError(f"case-ambiguous official packages: {paks[key]} and {pak}")
        paks[key] = pak
    maps: dict[str, tuple[Path, str]] = {}
    for _, pak in sorted(paks.items()):
        with zipfile.ZipFile(pak) as archive:
            for entry in archive.namelist():
                key = entry.lower()
                if not key.startswith("maps/") or not key.endswith(".bsp"):
                    continue
                match = MAP_ENTRY.fullmatch(key)
                if not match:
                    raise ValueError(f"unsafe BSP entry in {pak.name}: {entry!r}")
                if match[1] in maps:
                    raise ValueError(f"duplicate or case-ambiguous BSP: {entry!r} in {pak.name}")
                maps[match[1]] = (pak, entry)
    expected = set(STOCK_ALICE_MAPS)
    missing, unexpected = expected - maps.keys(), maps.keys() - expected
    if missing or unexpected:
        raise ValueError(f"official Alice map inventory mismatch: "
                         f"missing={sorted(missing)}, unexpected={sorted(unexpected)}")
    return maps


def read_map(pak: Path, entry: str) -> tuple[bytes, int]:
    """A map's bytes and the CRC-32 its package records for them."""
    with zipfile.ZipFile(pak) as archive:
        info = archive.getinfo(entry)
        return archive.read(info), info.CRC
