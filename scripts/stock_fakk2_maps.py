"""Official Heavy Metal: F.A.K.K.2 BSP inventory observed in the retail installation.

All thirty ship in fakk/pak0.pk3; neither language's pak1.pk3 carries a BSP.
Observed 2026-09-24; source identities are recorded in
docs/fntech3/2026-09-24-fakk2-map-sidecars.json.
"""

STOCK_FAKK2_MAPS = (
    "blood", "cemetery", "cliff1", "cliff2", "creeperpens", "end",
    "fakkhouse", "fog", "gruff", "gruff_cinema", "homes1", "homes1_fullvis",
    "homes2evil", "homes2good", "intro", "landersroost", "oracle", "oracleway",
    "otto", "over", "shield", "swamp1", "swamp2", "swamp3",
    "towncenter_evil", "towncenter_good", "training", "under", "water", "zoo",
)

# Each map ships an audio zone file and a static light file.
SIDECAR_SUFFIXES = (".azb", ".lights.json")


if __name__ == "__main__":
    for name in STOCK_FAKK2_MAPS:
        for suffix in SIDECAR_SUFFIXES:
            print(f"fakk2/fakk/maps/{name}{suffix}")
