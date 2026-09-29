# Elite Force Holomatch routes to a goal as stvoyHM.exe does

The [goal-link record](2026-09-28-aas-goal-links.md) found that Elite Force
Holomatch's engine starts every area cache from the goal area's own first row
of travel times, where Quake III 1.32 starts from zeros, and left that open:
fnTech3 routed `stvef-mp` as Quake III 1.32 does. Read in full, `stvoyHM.exe`'s
routing update differs from Quake III 1.32's in three ways, and the `stvef-mp`
profile now takes all three. Only the first changes a route on the installed
navigation, where it changes almost every one: of 4,559,687 routes to goals
sampled across all 33 Holomatch maps, 4,520,492 take a different travel time
and 34,723 leave by a different first link. The
[machine-readable record](2026-09-29-stvef-route-seed.json) holds the
addresses, the scans, the measurement of every map and the bot loads.

## Observed: stvoyHM.exe

`stvoyHM.exe`, SHA-256
`5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff`, capstone
disassembly (reference observation only; the addresses are in the JSON):

- **The start.** `AAS_UpdateAreaRoutingCache` (`0x004992d0`) returns, as
  Quake III does, when the goal's number in its cluster is not below the
  cluster's reachability areas (`0x00499352`). It then loads
  `aasworld.areatraveltimes[goal][0]` as the first update's row
  (`0x0049936d`..`0x00499378`). That row holds the times within the goal from
  each link into it to the start of the goal's first reachability. There is
  no zeroed array. Clustering numbers an area without reachabilities past its
  cluster's reachability areas, so a goal without a first reachability is
  returned for before its row would be read.
- **The loop** (`0x004993e9`..`0x0049954a`) tests each reversed link's travel
  type against the travel flags (`0x00499406`), then the contents flags of the
  area it enters (`call 0x00499290`, `0x00499422`), then the cluster. It never
  reads the entered area's `areaflags`, so `AREA_DISABLED` does not stop it,
  where Quake III 1.32 skips a disabled area. The rest matches Quake III
  1.32's loop step for step: the time sum, the better-time test, the
  reachability byte, the next area's row
  `areatraveltimes[next][linknum - firstreachablearea]`, and the list's append.
- **The contents flags** (`0x00499290`) are the older rule: water, then slime,
  then lava, each returning its own flag alone, and any other area `TFL_AIR`
  with `TFL_DONOTENTER` for `AREACONTENTS_DONOTENTER`. Quake III 1.32 also
  gives a liquid `TFL_DONOTENTER`, and adds `TFL_NOTTEAM1`, `TFL_NOTTEAM2` and
  `TFL_BRIDGE`. RTCW's rule is the same older one, with a bit of its own for
  1024.
- Holomatch's game imports 300 to 302 are its own visibility queries, which
  retail's dispatcher refuses, not Quake III 1.32's `AAS_EnableRoutingArea`
  (import 300 there, which Quake III's and Quake Live's games call). No
  Holomatch import can disable an area, and the botlib disables areas on its
  own only for RTCW's and Enemy Territory's blocking entities.

## The installed navigation

- All 33 navigation files Holomatch serves (`BaseEF/pak0.pk3` and `pak3.pk3`)
  were scanned: no area carries `AREA_DISABLED`, and no area's contents flags
  differ between the two rules (0 of 193,465). Nor does any area of the 62
  Quake III and Team Arena or 149 Quake Live files carry `AREA_DISABLED`. The
  last two differences matter only for navigation compiled with Quake III
  1.32's `bspc`. It marks the areas of Team Arena's team-only brushes
  `AREACONTENTS_NOTTEAM1` and `NOTTEAM2`, and lets a do-not-enter brush mark
  a liquid area (`code/bspc/aas_create.c` in id's release). Quake III 1.32's
  flags then keep a bot out of those areas, and `stvoyHM.exe`'s do not. That
  `bspc` never sets `AREA_BRIDGE`.
- [`scripts/measure_stvef_route_seed.py`](../../scripts/measure_stvef_route_seed.py)
  compiles the engine's own loader and routing from source, loads each file
  under `stvef-mp` and routes every area to 128 goal areas spread over those
  with reachabilities, from zeros and from the goal's row.

| | Routes found (both) | Travel time differs | First link differs | Largest row entry |
| --- | ---: | ---: | ---: | ---: |
| `ctf_singularity` | 182,017 | 180,818 | 3,650 | 163 |
| `hm_noon` | 110,750 | 109,345 | 2,087 | 231 |
| `hm_altar` | 224,357 | 223,937 | 2,053 | 210 |
| `hm_temple` | 273,382 | 270,285 | 1,996 | 431 |
| `hm_for2` | 262,168 | 260,680 | 1,828 | 430 |
| all 33 maps | 4,559,687 | 4,520,492 | 34,723 | 835 (`ctf_voy2`) |

No route is found by one start and not the other. A travel time is in
hundredths of a second, so the goal's row adds up to 8.35 s. It lengthens a
route into a goal by the time needed to cross the goal from where the route
enters it to the start of its first reachability. Where two ways into the goal
were close, that can make the other one shorter, and the route leaves by a
different first link. The bots' choice of goal weighs these travel times too.

## Change

`code/botlib/be_aas_route.cpp`, under `stvef-mp` only:

- `AAS_UpdateAreaRoutingCache` starts from `areatraveltimes[goal][0]`, as its
  RTCW branch already did, with that branch's refusal of a goal with no row.
- It follows a reversed link into an area whatever its `AREA_DISABLED` flag.
- `AAS_GetAreaContentsTravelFlags` gives `stvoyHM.exe`'s flags.

Quake III, Team Arena, Quake Live, Enemy Territory, the Jedi multiplayer
profiles and Elite Force single player route as before; `stvoy.exe` has no
bot library to compare with.

## Tests

- `tests/stvef_route_seed_tests.py` compiles the routing from its source over
  the goal-link worlds, under `stvef-mp` and every other profile that routes
  with Quake III's botlib. A filler's link into a goal or portal costs the
  start, the time inside the area it enters from the link's end to that
  area's first reachability (Holomatch) or nothing, and its own time. A
  goal marked `AREA_DISABLED` routes exactly as an open one under Holomatch
  and is reached from nowhere under Quake III. Every combination of the
  content bits and the bridge flag gives each engine's own flags. Nine
  mutations each fail, among them a Holomatch that starts from zeros,
  every profile starting from the row, Elite Force single player routing as
  Holomatch, and each branch of the flags.
- `tests/aas_goal_links_tests.py` takes the time inside the goal for
  Holomatch's fillers, and its routes still match the same worlds without the
  fillers, up to 4,013 links into one area.
- `tests/rtcw_routing_tests.py` expects Holomatch's flags for its mixed area.

## Runtime

[`scripts/probe_retail_bot_loads.py`](../../scripts/probe_retail_bot_loads.py)
loaded all 33 Holomatch maps on the dedicated server, each in a fresh process
with a bot added, once with the change and once without. Both builds came
from `8e345c8` with the
[route cache writer's check](2026-09-29-route-cache-before-routing.md) and
differ by this change alone. Every map loaded its navigation, initialised
routing and listed its bot, with no botlib error, in both, and no map's record
changed.

## What this does not show

- Retail's own travel times were not read from a running `stvoyHM.exe`; the
  retail rule is the disassembly's.
- The bot loads show routing starts and a bot plays; they do not show that
  fnTech3's bots now move as retail's do.
- Only the routing update and the contents flags were compared with
  `stvoyHM.exe`. Its portal routing and route queries were not.

## Found along the way

- **Enemy Territory routes as RTCW does, not as Quake III 1.32.**
  `et.exe`'s `AAS_UpdateAreaRoutingCache` (`0x00481290`) starts from the
  goal's row, skips `AREA_DISABLED` areas, adds 1,000 to the time through an
  area with `areaflags` bit 16 (RTCW's avoid flag), and skips an area without
  travel times. `wolfet` still routes as Quake III 1.32. This is not changed
  here: ET ships no navigation to route over. It remains open.

## Identity

- Tested commit `8e345c8f05bf66b748e85cbe3686cde78f4f3a05` with the route cache
  writer's check, built as a `git archive` export with win32-dev's options,
  strict warnings and every development option, on Windows 11 x64,
  2026-09-29.
- Server without the change `359f5b853e87cdf33603b1e3158ac6100702fa40dca8e28070beb0f18ff0cf0a`,
  with it `693f533f4199c3cf3419808710cb1d6495299765a68d3befbd7cdde21db5dc2c`.
- Installation `E:\Games\Star Trek Voyager Elite Force`, read only.
