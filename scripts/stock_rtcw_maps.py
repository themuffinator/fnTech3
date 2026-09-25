"""Official RTCW BSP inventory observed in the retail Steam installation.

Includes SP cinematics/ending and all seven official MP map packs. Observed
2026-09-17; source identities are recorded in docs/fntech3/2026-09-17-rtcw-audio-zones.json.
"""

STOCK_RTCW_MAPS = {
    "rtcw-sp": (
        "assault", "baseout", "boss1", "boss2", "castle", "chateau", "church",
        "crypt1", "crypt2", "cutscene1", "cutscene11", "cutscene14", "cutscene19",
        "cutscene6", "cutscene9", "dam", "dark", "dig", "end", "escape1", "escape2",
        "factory", "forest", "norway", "rocket", "sfm", "swf", "trainyard", "tram",
        "village1", "village2", "xlabs",
    ),
    "rtcw-mp": (
        "mp_assault", "mp_base", "mp_beach", "mp_castle", "mp_chateau", "mp_dam",
        "mp_depot", "mp_destruction", "mp_ice", "mp_keep", "mp_rocket", "mp_sub",
        "mp_tram", "mp_trenchtoast", "mp_village",
    ),
}


# Each map ships an audio zone file and a static light file.
SIDECAR_SUFFIXES = (".azb", ".lights.json")


if __name__ == "__main__":
    for profile, names in STOCK_RTCW_MAPS.items():
        for name in names:
            for suffix in SIDECAR_SUFFIXES:
                print(f"{profile}/main/maps/{name}{suffix}")
