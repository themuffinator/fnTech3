"""Official American McGee's Alice BSP inventory observed in the retail installation.

All thirty-six ship in base/pak0.pk3; no other retail pak carries a BSP.
Observed 2026-09-24; source identities are recorded in
docs/fntech3/2026-09-24-alice-map-sidecars.json.
"""

STOCK_ALICE_MAPS = (
    "centipede1", "centipede2", "facade", "fortress1", "fortress2", "funhouse",
    "garden1", "garden2", "garden3", "garden4", "grounds1", "grounds2",
    "gvillage", "hatter1", "hatter2", "hedge1", "hedge2", "hedge3",
    "jlair1", "jlair2", "keep", "pandemonium", "potears1", "potears2",
    "potears3", "qlair", "rchess1", "skool1", "skool2", "tower1",
    "tower2", "tower3", "utemple", "wchess1", "wchess2", "wforest",
)

# Each map ships an audio zone file and a static light file.
SIDECAR_SUFFIXES = (".azb", ".lights.json")


if __name__ == "__main__":
    for name in STOCK_ALICE_MAPS:
        for suffix in SIDECAR_SUFFIXES:
            print(f"alice/base/maps/{name}{suffix}")
