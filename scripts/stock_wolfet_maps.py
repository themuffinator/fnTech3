"""Official Enemy Territory BSP inventory observed in the retail Steam installation.

The six campaign maps ship in etmain/pak0.pk3. Observed 2026-09-18; source
identities are recorded in docs/fntech3/2026-09-18-wolfet-audio-zones.json.
"""

STOCK_WOLFET_MAPS = ("battery", "fueldump", "goldrush", "oasis", "radar", "railgun")


if __name__ == "__main__":
    for name in STOCK_WOLFET_MAPS:
        print(f"wolfet/etmain/maps/{name}.azb")
