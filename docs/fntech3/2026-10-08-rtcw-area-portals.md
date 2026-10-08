# RTCW area portals closed while shut

An area portal joins two areas while its count is above zero; a door opening
raises it and closing lowers it. Quake III drops the level when a close takes
the count below zero, and fnTech3 did so for RTCW too. Both RTCW engines skip
such a close instead: the count stays at zero, and the areas are flooded as
after any close. fnTech3 now does the same in both modes. A retail-against-
fnTech3 run on `escape1` shows the change: fnTech3's old rule dropped the
level where `WolfSP.exe` went on. The
[machine-readable record](2026-10-08-rtcw-area-portals.json) holds the runs.

## The rule

Observed in the retail executables: `CM_AdjustAreaPortalState` reads the
count of the portal from the second area to the first and, at zero, skips
both decrements and goes on to flood the areas (`WolfSP.exe`
`0x418489..0x41849a`, `WolfMP.exe` `0x41afa9..0x41afba`). Ritual's engines
and Medal of Honor, whose rule
`code/qcommon/cm_test.c` already followed, let the count go below zero and
only tell a developer; RTCW keeps it at zero, so one later open joins the
areas again.

## Runtime check

The synthetic game module of the [RTCW trace record](2026-10-08-rtcw-traces.md),
built with its portal battery, links each of up to 32 of a map's brush
models back where the map put it, so that a door's model spans its portal.
It draws 16 point pairs around the model, then closes, opens, closes and
closes again the model's portal through `trap_AdjustAreaPortalState`, and
after each step asks `trap_InPVS` of the same pairs: `SV_inPVS` asks whether
the two areas are connected. No entity is spawned, so every portal starts
shut and the first close is of a portal nothing opened. Retail and fnTech3's
dedicated servers ran on loopback on a private desktop, the installation
read only.

| Map | Models | Steps answered alike | Old rule |
| --- | ---: | --- | --- |
| escape1 (SP) | 32 | 128 of 128 | dropped the level at model 20's first close, after 76 steps |
| mp_beach, mp_village (MP) | 32 each | 128 of 128 | went on, 128 of 128 |
| mp_assault, mp_castle, mp_depot, mp_ice (MP) | 32, 18, 32, 32 | all | not run |

On `escape1`, model 20 spans a portal, and model 30 spans one whose open let
some of its pairs see each other: its next close shut them again and the
second close, of a portal now at zero, changed nothing, the same answers in
both engines. No linked model of the six multiplayer maps changed what its
pairs see, so the multiplayer rule rests on `WolfMP.exe` and the fixture.

`tests/area_portal_count_tests.py` runs every catalog profile through the
engine's own area-portal code: Quake III's drop, the Ritual engines' report
and RTCW's ignored close.

## Limits

- Doors the game spawns and opens, and portal state carried through a
  savegame, are not exercised here.
- Enemy Territory's own source keeps the same rule; `et.exe` is not checked,
  and its profile keeps Quake III's drop.
