"""Official Medal of Honor: Allied Assault BSP inventory observed in the retail installation.

All fifty-four ship in main/Pak5.pk3; no other retail pak carries a BSP. Each
name is the map's path under maps/, as the pak spells it: the deathmatch,
objective and briefing maps sit in folders of their own. Observed 2026-09-25
in the 1.00 US installation; source identities are recorded in
docs/fntech3/2026-09-25-mohaa-map-sidecars.json.
"""

STOCK_MOHAA_MAPS = (
    "DM/mohdm1", "DM/mohdm2", "DM/mohdm3", "DM/mohdm4", "DM/mohdm5", "DM/mohdm6", "DM/mohdm7",
    "briefing/briefing1", "briefing/briefing2", "briefing/briefing3",
    "briefing/briefing4", "briefing/briefing5", "briefing/briefing6",
    "credits",
    "m1l1", "m1l2a", "m1l2b", "m1l3a", "m1l3b", "m1l3c",
    "m2l1", "m2l2a", "m2l2b", "m2l2c", "m2l3",
    "m3l1a", "m3l1b", "m3l2", "m3l3",
    "m4l0", "m4l1", "m4l2", "m4l3",
    "m5l1a", "m5l1b", "m5l2a", "m5l2b", "m5l3",
    "m6l1a", "m6l1b", "m6l1c", "m6l2a", "m6l2b", "m6l3a", "m6l3b", "m6l3c", "m6l3d", "m6l3e",
    "obj/obj_team1", "obj/obj_team2", "obj/obj_team3", "obj/obj_team4",
    "training", "void",
)

# Each map ships an audio zone file and a static light file.
SIDECAR_SUFFIXES = (".azb", ".lights.json")


if __name__ == "__main__":
    for name in STOCK_MOHAA_MAPS:
        for suffix in SIDECAR_SUFFIXES:
            print(f"mohaa/main/maps/{name}{suffix}")
