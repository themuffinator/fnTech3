"""Profile catalogue and exports of the approved imagegen icon masters.

Artwork lives in assets/icons/games/sources; generation prompts and reference
provenance are recorded alongside it. This module only resizes those masters.
"""
from __future__ import annotations

from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path


@dataclass(frozen=True)
class GameIcon:
    key: str        # comGameProfile_t::key
    name: str       # display name for launchers
    resource: int | None  # None for artwork without an engine profile
    symbol: str | None    # C++ identifier stem for generated data


def _registry() -> list[GameIcon]:
    # Profile catalogue order. Resource ids are part of the executable's
    # shell-visible layout (shortcuts may name them), so never renumber.
    return [
        GameIcon("quake3", "Quake III Arena", 101, "Quake3"),
        GameIcon("quakelive", "Quake Live", 102, "QuakeLive"),
        GameIcon("rtcw-sp", "Return to Castle Wolfenstein (Single Player)",
                 103, "RtcwSp"),
        GameIcon("rtcw-mp", "Return to Castle Wolfenstein (Multiplayer)",
                 104, "RtcwMp"),
        GameIcon("wolfet", "Wolfenstein: Enemy Territory", 105, "WolfEt"),
        GameIcon("stvef-sp", "Star Trek: Voyager - Elite Force (Single Player)",
                 106, "StvefSp"),
        GameIcon("stvef-mp", "Star Trek: Voyager - Elite Force (Holomatch)",
                 107, "StvefMp"),
        GameIcon("q3-ihv", "Quake III Arena (IHV Test)", 108, "Quake3Ihv"),
        GameIcon("jka-sp", "Star Wars Jedi Knight: Jedi Academy (Single Player)",
                 109, "JkaSp"),
        GameIcon("jka-mp", "Star Wars Jedi Knight: Jedi Academy (Multiplayer)",
                 110, "JkaMp"),
        GameIcon("jk2-sp", "Star Wars Jedi Knight II: Jedi Outcast (Single Player)",
                 111, "Jk2Sp"),
        GameIcon("jk2-mp", "Star Wars Jedi Knight II: Jedi Outcast (Multiplayer)",
                 112, "Jk2Mp"),
        GameIcon("alice", "American McGee's Alice", 113, "Alice"),
        GameIcon("fakk2", "Heavy Metal: F.A.K.K.2", 114, "Fakk2"),
        GameIcon("mohaa", "Medal of Honor: Allied Assault", 115, "Mohaa"),
        GameIcon("q3test", "Quake III Arena (Q3Test)", 116, "Q3Test"),
    ]


_ORDERED = _registry()
_STANDALONE: list[GameIcon] = []
ICONS = {icon.key: icon for icon in _ORDERED + _STANDALONE}


def ordered() -> list[GameIcon]:
    return list(_ORDERED)


def standalone() -> list[GameIcon]:
    """Artwork exports only; these do not declare engine compatibility."""
    return list(_STANDALONE)


@lru_cache(maxsize=len(ICONS))
def source(key: str):
    from PIL import Image

    if key not in ICONS:
        raise KeyError(key)
    path = Path(__file__).resolve().parents[2] / "assets/icons/games/sources" / f"{key}.png"
    with Image.open(path) as image:
        if image.width != image.height or image.width < 1024:
            raise ValueError(f"{path}: expected a square master of at least 1024 px")
        if image.mode not in ("RGB", "RGBA"):
            raise ValueError(f"{path}: expected RGB or RGBA artwork")
        # Several originals intentionally have an opaque square field. Preserve
        # that background, and preserve generated alpha on the cutout designs.
        result = image.convert("RGBA")
        if key == "stvef-mp":
            # This rounded tile is specified as fully opaque inside. Imagegen's
            # cutout master has a near-opaque (251-254) alpha ceiling; normalize
            # that ceiling during export, keeping the antialiased corner mask
            # and the unmodified generated master/provenance intact.
            alpha = result.getchannel("A")
            result.putalpha(alpha.point([255 if value >= 250 else value for value in range(256)]))
        return result


def render(key: str, size: int):
    from PIL import Image

    # Pillow resamples RGBA in premultiplied-alpha space, preserving clean edges.
    return source(key).resize((size, size), Image.Resampling.LANCZOS)
