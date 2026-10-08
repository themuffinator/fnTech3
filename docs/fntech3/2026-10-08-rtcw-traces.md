# RTCW collision against the retail servers

RTCW's collision evidence was a stock `escape1` probe and fixtures with
synthetic backends. This record asks the retail `WolfSP.exe` 1.32 and
`WolfMP.exe` 1.41 dedicated servers and fnTech3's the same questions through a
synthetic native game module, one build per mode, on eight single-player and
six multiplayer maps. Seven rules in which RTCW's engines differ from Quake
III's, all read from the retail executables, were missing or wrong in
fnTech3, and one inherited test made fnTech3 miss what retail hits. With them
fixed, 202,860 traces agree with retail but for nine, each a difference of
floating-point precision; 39,060 of them run around linked brush models and
8,400 at an entity flagged as a capsule. 21,000 point contents, 2,800 more
beside that entity, 434 links and 2,800 box queries agree as well. The
[machine-readable record](2026-10-08-rtcw-traces.json) holds identities,
bounds, seeds, every tally and each remaining difference.

## Method

The module (C, MSVC x86, kept in `.tmp` and not committed) is the one the
[Quake III trace record](2026-10-08-q3-traces.md) describes, built with each
mode's import numbers (`code/compat/rtcw_module_calls.def`) and entity record:
680 bytes in SP and 684 in MP, `r.linked` at 576 and `r.svFlags` at 584. At
`GAME_INIT` it asks:

- **World:** four hulls -- a point, the player box, a random box up to 32
  units, and the player box traced as a capsule -- with three masks, 300 each
  of a long trace, a vertical drop through the world and a short trace; then
  1,500 point contents.
- **Brush models:** up to 32 of the map's brush models set on entities, moved
  and every second one turned, then linked; their link flags and bounds; 90
  traces around each; 200 box queries around them.
- **A box entity:** a player-sized entity with contents body, traced 300 times
  with box and capsule hulls.
- **A capsule entity:** the same entity flagged `SVF_CAPSULE`, as RTCW SP
  flags its players, traced 600 times with point, box and capsule hulls, one
  in four a position test, then 200 point contents around it. RTCW chooses an
  entity's capsule for every trace shape.

Inputs come from a seeded generator and the module's own arithmetic, and
every answer is printed as raw float bits. Both retail servers run from the
Steam installation, read only, with `fs_homepath` in a scratch folder.
`WolfMP.exe`'s `Sys_LoadDll` (`0x445240`) looks under `fs_basepath` only, so a
scratch `fs_basepath` holds the module and the installation is the CD path.
`WolfSP.exe`'s (`0x441c10`) first loads the bare file name, which finds the
installation's own `qagamex86.dll` beside the executable; under cdb that one
result is set to none as it returns (`0x441cb4`), nothing patched, and the
engine goes on to `fs_cdpath`, where the module waits. fnTech3's dedicated
server loads it from its home. All run `sv_pure 0` and `bot_enable 0` on
loopback on a private desktop. No installation file changed.

## What RTCW's engines do

Observed in the retail executables:

- **Contents inside a solid.** A sweep that starts and ends inside a brush
  writes the solid flags and fraction but no contents (`CM_TraceThroughBrush`,
  `WolfSP.exe` `0x4194bd..0x4194f1`, `WolfMP.exe` `0x41bfdd..0x41c011`), and a
  start found inside a curve sets no contents either (`CM_TestInLeaf`,
  `0x418b8d..0x418ba2` and `0x41b6ad..0x41b6c2`). Quake III writes the brush's
  and the curve's.
- **Curve facets, MP.** `BaseWindingForPlane` starts a facet's winding 8192
  units out from the plane's point nearest the origin (`0x419ae8..0x419b30`),
  `WindingBounds` starts from +-99999 (`0x4198e9`), and `CM_ValidateFacet`
  keeps a facet no wider than 4096 units on any axis, checking nothing else
  (`0x4185d7..0x4185f0`). Quake III starts at 65535 and checks its bounds
  there.
- **Curve facets, SP.** The winding and its bounds start at 131072 units
  (`0x416fd8..0x417020`, `0x416dd9`), and `CM_ValidateFacet` makes Quake
  III's three checks at that bound (`0x415987..0x4159bf`).
- **Inside a curve, MP.** `CM_PositionTestInPatchCollide` (`0x418f00`) tests
  the bounds, places the start against every plane of the curve, widened by
  the box's extent -- in front, behind or straddling -- and finds it inside a
  facet whose surface plane it straddles and each of whose borders it
  straddles or lies inside. It tests a capsule as its box. SP keeps Quake
  III's capsule-aware test (`0x4162d0`). The MP test is Q3Test's from 1.05.
- **A point against a curve, MP.** `CM_TracePointThroughPatchCollide` traces
  a point whatever `cm_playerCurveClip` says (`0x41860a..0x418629`); SP and
  Quake III skip it when the cvar is 0.
- **Capsules, SP.** `CM_Trace` turns the capsule off for both temporary
  models, box and capsule, and traces the box (`0x41a77b..0x41a7ae`,
  `0x41a86a..0x41a89a`); `CM_TempBoxModel` builds the box for a capsule too
  and returns the capsule handle (`0x41386e..0x41387b`). So SP's players,
  which its game flags `SVF_CAPSULE`, are boxes to every trace. MP keeps the
  capsule routines (`0x41d291..0x41d2c7`).

fnTech3 applied Quake III's rule in each case. It now applies RTCW's, by mode
(`code/qcommon/cm_patch.c`, `cm_polylib.c`, `cm_trace.c`, `cm_load.c`).

One more difference was fnTech3's own. Quake3e's `CM_TraceThroughLeaf` passes
over a brush outside the trace's bounds, a test no original engine makes.
When a point or box is traced against a capsule, the engine sweeps the
capsule against a box of the trace's size instead, but the bounds stayed the
point's: a point beside a capsule entity's centre passed over the box it
should have hit. The bounds now hold the capsule's sweep. RTCW MP and Enemy
Territory reach this path, the only profiles that trace a flagged entity's
capsule for point and box traces. Retail's position test keeps its own
pretest on those stale bounds (`CM_TestBoxInBrush`), and so does fnTech3's.

## Results

`WolfSP.exe` 1.32, seed 20261008:

| Map | Brush models linked | Traces | Bit-exact | Within 0.01 unit | Beyond | Point contents equal | Links: same flags | Box queries: same entities |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| escape1 | 32 of 229 | 14,580 | 11,369 | 3,211 | 0 | 1,500 | 32 of 32 | 200 of 200 |
| village1 | 32 of 167 | 14,580 | 12,308 | 2,272 | 0 | 1,500 | 32 of 32 | 200 of 200 |
| forest | 32 of 131 | 14,580 | 12,477 | 2,100 | 3 | 1,500 | 32 of 32 | 200 of 200 |
| rocket | 32 of 196 | 14,580 | 11,519 | 3,061 | 0 | 1,500 | 32 of 32 | 200 of 200 |
| factory | 32 of 61 | 14,580 | 12,302 | 2,278 | 0 | 1,500 | 32 of 32 | 200 of 200 |
| norway | 32 of 88 | 14,580 | 11,757 | 2,821 | 2 | 1,500 | 32 of 32 | 200 of 200 |
| dam | 32 of 149 | 14,580 | 12,376 | 2,204 | 0 | 1,500 | 32 of 32 | 200 of 200 |
| chateau | 32 of 215 | 14,580 | 12,107 | 2,473 | 0 | 1,500 | 32 of 32 | 200 of 200 |

`WolfMP.exe` 1.41, seed 20261008:

| Map | Brush models linked | Traces | Bit-exact | Within 0.01 unit | Beyond | Point contents equal | Links: same flags | Box queries: same entities |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| mp_beach | 32 of 38 | 14,580 | 12,234 | 2,345 | 1 | 1,500 | 32 of 32 | 200 of 200 |
| mp_assault | 32 of 86 | 14,580 | 11,603 | 2,977 | 0 | 1,500 | 32 of 32 | 200 of 200 |
| mp_village | 32 of 86 | 14,580 | 12,139 | 2,441 | 0 | 1,500 | 32 of 32 | 200 of 200 |
| mp_castle | 18 of 18 | 13,320 | 10,400 | 2,920 | 0 | 1,500 | 18 of 18 | 200 of 200 |
| mp_depot | 32 of 38 | 14,580 | 12,661 | 1,919 | 0 | 1,500 | 32 of 32 | 200 of 200 |
| mp_ice | 32 of 50 | 14,580 | 12,798 | 1,779 | 3 | 1,500 | 32 of 32 | 200 of 200 |

"Within 0.01 unit" means every flag and contents value agrees and the end
position is that close. Every box query returned the same entities in the
same order, every link the same flags; 380 of the 434 links have
bit-identical bounds and the rest, turned models, are within 0.001 units.
Each mode's capsule entity also answered its 600 traces and 200 point
contents as retail did. The nine traces beyond:

- **A brush and a curve at once (2).** On `mp_beach` (player capsule) and
  `norway` (player box) a long trace starting in solid stops at the same
  fraction on a 45-degree wall where a brush and a curve facet meet. One
  engine names the brush (surface flags 160), the other the curve (525472):
  retail builds the facet's plane in float, fnTech3 in double, so the tie
  breaks one way on one map and the other way on the other.
- **A curve's edge (1).** On `mp_ice` a trace stops on a curve's axial bevel
  that retail places at -1026.002 and fnTech3 at -1025.975: the bevel comes
  from the facet's winding, which fnTech3 cuts in double. The end positions
  are 0.11 units apart.
- **Turned brush models (6).** Around models the module turned, the end
  positions are 0.0002 to 0.018 units apart (`forest` three, `norway` one,
  `mp_ice` two); on `forest` two models touch, and the slightly nearer one
  wins in each engine. Retail's rotation of a model turned 360 degrees keeps
  a normal component of -4.9e-08 where fnTech3's keeps 1.7e-07.

These are the precision differences the [Quake III record](2026-10-08-q3-traces.md)
leaves as a decision: fnTech3's collision arithmetic from Quake3e is double
where the retail engines' is float.

Before each change, on the same seed:

- **Contents inside a solid.** fnTech3 at `cac46a13` answered 1,533 of 3,900
  `mp_beach` traces with the brush's contents where retail had none, every one
  starting and ending inside a brush.
- **MP curve facets.** With only that rule, five `mp_depot` traces differed,
  one by 256 units: fnTech3 stopped on a curve facet retail never builds, at
  x = -8578. Every one stopped beyond x = -8192, out of reach of MP's
  starting winding.
- **SP capsules.** The final build with SP's players traced as capsules
  answered 71 of escape1's 600 capsule-entity traces and 2 of its 200 point
  contents otherwise than `WolfSP.exe`.
- **The capsule's bounds.** Without them, 9 to 13 point traces on each MP map
  passed through the capsule entity where retail stopped on it.

The fixtures `tests/rtcw_curve_collision_tests.py` (facets cut and starts
found inside curves, mode by mode), `tests/collision_capsule_trace_tests.c`
(SP's boxes and the capsule's bounds) and `tests/bsp_profile_loader_tests.py`
(the rules each RTCW map sets) cover the changes.

## Limits

- Client-side traces (the client game's prediction) are not compared; they
  run the same collision code.
- Eight of the 32 single-player maps and six of the 15 multiplayer maps.
- Enemy Territory reaches the capsule-bounds change but its collision is not
  compared here.
