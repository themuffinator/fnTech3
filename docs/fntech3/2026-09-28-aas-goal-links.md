# Routing to an area entered by more than 128 links

Quake III's routing starts every area cache in its goal area, where no link
into the area costs any travel time, and walks the area's reversed links from
there (`AAS_UpdateAreaRoutingCache`). It kept those zeros in a stack array of
128 (`startareatraveltimes`, "not more than 128 reachabilities per area
allowed") and read it by the index of each link into the goal. But an area
takes a reversed link from each of the first 128 reachabilities of *every
other* area, so it can be entered by more than 128, and routing to such an
area read the stack past the array as travel times. The
[navigation-check record](2026-09-28-aas-index-checks.md) counted the links
into every area of every installed file and found seven such areas in four
files, entered by up to 355.
The row is now allocated with the reversed links, as long as the most links
into any area, and freed with the other routing caches; RTCW, which starts
from the goal area's own travel times, is unchanged. Wherever no area is
entered more than 128 times, routing reads the same zeros as before.

Retail `quake3.exe` 1.32 and `quakelive_steam.exe` carry the same array and
read past it the same way, so their travel times through those links came
from whatever their stacks held there. Measured with the engine's own routing,
the words past the array set the travel time of up to 2,705 of the 2,848
routes into one Quake Live area. Fixtures that route such worlds fail before
the change and pass after it, and every affected map loads, routes and takes
a bot the same before and after on the stable `meson/build/win32-dev`
binaries. The
[machine-readable record](2026-09-28-aas-goal-links.json) holds every
observation, measurement, load and test run.

## What the routing read

For a goal area G, the routing seeds its update with a row of travel times
within G, one for each link into G, and costs the area that link leaves as
`start + row[i] + the link's own time`, where `i` is the link's place in G's
reversed list. Only the first 128 reachabilities of each area are reversed
(`AAS_CreateReversedReachability` warns past that), but nothing bounds how
many areas lead into one. How each retail engine seeds that row, from its
own executable (capstone disassembly, reference observation only; the
addresses are in the JSON):

| Engine | `AAS_UpdateAreaRoutingCache` | The goal area's row | Past its 128th entry |
| --- | --- | --- | --- |
| `quake3.exe` 1.32 (Quake III, Team Arena) | 0x0049EC80 | 128 zeros on the stack (`Com_Memset` of 0x100 bytes), read at 0x0049EE7A by link, with no bound | the return address into `AAS_GetAreaRoutingCache` (entries 128 and 129: 0xF066 and 0x0049), the cache pointer passed in (130 and 131), then the caller's frame |
| `quakelive_steam.exe` | 0x00493BA0 | the same, at `[ebp-0x104]`, read at 0x00493E36 | the `/GS` cookie (128 and 129, drawn afresh at each start), the saved `ebp`, the return address, the cache pointer, then the caller's frame |
| `jamp.exe` 1.01, `jk2mp.exe` 1.02a | 0x0050CF50, 0x004C55D0 | the same, read at 0x0050D164 and 0x004C57E2 | not traced |
| `stvoyHM.exe` (Elite Force Holomatch) | 0x004992D0 | the goal area's own first row of travel times, `areatraveltimes[goal][0]`; no array | nothing: that row has an entry for each link |
| `et.exe`, `etded.exe` 2.60b | 0x00481290, 0x0044BBF0 | the goal area's first row, as Elite Force | nothing |
| `WolfSP.exe`, `WolfMP.exe` | 0x004A22B0, 0x004B5340 | the goal area's first row, as fnTech3's RTCW branch does | nothing |

Of these engines, only Quake III 1.32 and those built on its code after it
(Quake Live and the Jedi multiplayer engines) zero the goal's row, and every
one of them reads past it. Which words those are is observed from the frame
layout; what they held on any call is not, and in Quake Live the first two
change with every start of the process.

## The change

[`AAS_CreateReversedReachability`](../../code/botlib/be_aas_route.cpp), once
it has counted the links into every area, allocates `startareatraveltimes`
(a new field of `aas_t` in [`be_aas_def.h`](../../code/botlib/be_aas_def.h)),
zeroed and as long as the most links into any one area (at least one entry).
`AAS_FreeRoutingCaches` frees it with the other routing caches, and
`AAS_UpdateAreaRoutingCache` seeds the goal area there for every profile but
RTCW's, instead of from its own 128-entry array. The routing reads entries 0
to one less than the goal's link count, and those are zero in the old array
and the new row alike, so where no area has more than 128 links nothing
changes. RTCW's branch still starts from the goal area's first row, and
RTCW's routing initialisation never builds Quake III's reversed links, so it
never allocates the row. The row costs two bytes a link, 710 bytes for the
largest installed count.

Seven profiles route this way: `q3`, `ql`, `wolfet`, `stvef-sp`, `stvef-mp`,
`jka-mp` and `jk2-mp`. Retail Elite Force and Enemy Territory do not (see
[Found along the way](#found-along-the-way)).

## The installed files

[`scripts/measure_aas_goal_links.py`](../../scripts/measure_aas_goal_links.py)
counts the reversed links into every area of every navigation file in the
Quake III, Team Arena, Quake Live and Elite Force paks, 274 files, as the
routing makes them. For each file with an area entered more than 128 times,
it compiles the engine's own loader and routing from source, loads the file
under its profile and routes every area to that goal with the default travel
flags twice: with the goal's row all zeros, as the engine does now, and with
every entry past the 128th costing 16,384. A route whose travel time or first
link differs between the two ends through one of those links, so before the
change its time was whatever the stack held.

| File | Goal area | Links in | Past the 128th | Areas routed to it | Routes whose time depends on them | Whose first link does |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `missionpack/pak0.pk3` `mpq3tourney6` | 1887 | 134 | 6 | 781 | 6 | 0 |
| Quake Live `pak00.pk3` `blackcathedral` | 802 | 190 | 62 | 1,774 | 844 | 73 |
| | 3004 | 144 | 16 | 1,774 | 1,163 | 88 |
| | 4300 | 198 | 70 | 1,774 | 618 | 12 |
| Quake Live `pak00.pk3` `reflux` | 3297 | 138 | 10 | 2,848 | 10 | 3 |
| | 10172 | 136 | 8 | 2,848 | 2,705 | 247 |
| Elite Force `BaseEF/pak0.pk3` `hm_cam` | 3323 | 355 | 227 | 1,781 | 59 | 0 |

No route is lost either way. None of these areas is a cluster portal, so only
routes to them read past their 128th link, and every link past the 128th
comes from an area of the goal's own cluster. `mpq3tourney6`'s area is a jump
pad's; the others are ordinary floor. The other 270 files have no area entered
more than 128 times; the most is 123, in Elite Force's `ctf_kln2`. Elite
Force's own engine never read past a row here (see the table above), so for
`hm_cam` the change removes a read fnTech3 alone made.

## Bot loads before and after

[`scripts/probe_retail_bot_loads.py`](../../scripts/probe_retail_bot_loads.py),
from the navigation-check work, loaded the four affected maps and three
controls whose busiest area is entered fewer than 128 times, each in a fresh
dedicated server with bots on under its own profile, adding one bot, on
`meson/build/win32-dev` before and after the change.

| Map | Profile | Most links into an area | Before | After |
| --- | --- | ---: | --- | --- |
| `mpq3tourney6` (Team Arena) | `q3` | 134 | loaded, routing, bot listed | the same |
| `blackcathedral` | `ql` | 198 | loaded, routing, bot listed | the same |
| `reflux` | `ql` | 138 | loaded, routing, bot listed | the same |
| `hm_cam` | `stvef-mp` | 355 | loaded, routing, bot listed | the same |
| `ctf_kln2` | `stvef-mp` | 123 | loaded, routing, bot listed | the same |
| `terminus` | `ql` | 110 | loaded, routing, bot listed | the same |
| `q3dm11` | `q3` | 86 | loaded, routing, bot listed | the same |

No map's record changed: outcome, navigation files loaded, botlib errors
(none), routing initialised and the bot listed. The "before" binaries were
linked by another session's build of win32-dev from 8e8de10 and the
uncommitted work then in the tree, before this change was made; the "after"
binaries were rebuilt in place from 484d02d with this change and other
sessions' uncommitted botlib work (the navigation checks and Quake III's
route-cache validation). The pair therefore differs by more than this change;
the fixtures below attribute the routing to it alone.

## Tests

[`tests/aas_goal_links_tests.py`](../../tests/aas_goal_links_tests.py)
compiles the routing from its source over worlds in which a goal, or the
portal between two clusters, is entered by more than 128 links: filler areas
numbered after the rest lead into it alone, so its reversed list takes theirs
first and pushes every other link into it past the 128th. The named areas'
costs, travel types, postures and contents vary with a seed alone.

- **Routes match the world without the fillers.** For each of the seven
  profiles, in worlds whose goal or portal is entered by 128 or 129 links, or
  takes 200 or 1,000 fillers, every named area's route to the goal and to the
  portal, with and without an origin, under three sets of travel flags, equals
  the same world's route without its fillers, and each filler's single link
  costs the cache's start plus its own time: 9,240 routes compared.
- **Under a guard.** The same comparison with 300, 1,000 and 4,000 fillers,
  every query made below a frame holding 8,192 words of 0x5A5A, so a row read
  past its end reads a travel time no route here comes to.

Before the change (HEAD 484d02d with only the new test), both fail: the first
at 129 links, having matched every route at 128, and the second at its first
world. A rerun printing what they read shows each guarded route costing
23,130 (0x5A5A) more, for 149 of 300 fillers and 3,849 of 4,000; the other 23
filler links past the 128th read words that were zero, in the frames between
the row and the guard. Unguarded, 129 links added 1 to two routes, and 1,000
fillers made 557 filler routes wrong by stack words such as 1,200. With the
change, both pass on HEAD with only these hunks and in the shared tree
(Clang 20.1.7, i686 Windows), and as i686 Linux under GCC 13.3 and Clang
18.1.3 (WSL).

The 46 test files at HEAD that name `aasworld`, `AAS_FreeRoutingCaches` or the
bot library (146 tests and 12 subtests) pass on HEAD with the change, and the
48 in the shared tree pass beside other sessions' uncommitted botlib work.
Among them are the fixtures that compile the changed functions from their
source: `rtcw_route_query_tests.py` and `rtcw_goal_route_tests.py` slice
`AAS_CreateReversedReachability`, `AAS_CalculateAreaTravelTimes` and
`AAS_UpdateAreaRoutingCache`, and every fixture built on
`rtcw_navigation_tests.py` slices `AAS_FreeRoutingCaches`. The RTCW probe
scripts that slice routing take those functions from the original RTCW and
iortcw sources, or `rtcw_route_query_tests.py`'s, and were not rerun.
`be_aas_route.cpp`, `be_aas_main.cpp` and `be_aas_file.cpp` compile clean with
win32-dev's own commands (MSVC 19.44, `/W4 /WX`), and win32-dev builds.

The whole Python suite (`python -m pytest tests`, run in the x86 developer
shell) passes on a full export of 484d02d with the change: 2,330 passed, 129
skipped and 57,687 subtests, in 23 minutes. The navigation checks landed
meanwhile (c8d3bbb); on a full export of that commit with the change, the 47
botlib test files it holds (its own `aas_index_checks_tests.py` among them),
the packaging and documentation tests and `aas_goal_links_tests.py` pass:
204 passed, 1 skipped and 34 subtests.

## What this does not show

- Retail's own travel times through those links are not reproduced: they came
  from stack words that differ by build and call, and in Quake Live by
  process.
- No bot was made to route to one of these areas at runtime. The loads show
  that navigation, routing and the bot come up as before; the routes are
  shown by the fixtures and the measurement.
- Windows only for the runs; the fixtures also ran on Linux.

## Found along the way

- **Elite Force and Enemy Territory start routing from the goal area's first
  row.** `stvoyHM.exe` and `et.exe`/`etded.exe` 2.60b seed every area cache
  from `areatraveltimes[goal][0]`, the travel times within the goal from each
  link's end to the start of the goal's first reachability, as RTCW does;
  only Quake III 1.32's line made them zero. fnTech3 routes `stvef-mp` and
  `wolfet` as Quake III 1.32 does, so their travel time to a goal differs
  from their retail engines' by that term, and so can the link a route enters
  it by. This is not changed here. (`stvoy.exe`, Elite
  Force's single-player engine, has no bot library to compare with.)
  Holomatch starts from the goal's row since 2026-09-29
  ([route seed record](2026-09-29-stvef-route-seed.md)); Enemy Territory
  remains open.
- `jamp.exe` and `jk2mp.exe` carry the same 128-entry array. Their profiles
  route this way too, but neither game installs navigation files.
