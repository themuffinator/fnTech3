# Snapshot portal recursion across profiles, 2026-10-15

The [broadcast and level-time record](2026-10-15-jedi-broadcast-level-time.md)
left one finding open: that fnTech3's shared snapshot walk recurses into a
portal entity's camera only from the viewer's own pass, where retail
`jamp.exe`, `jk2mp.exe` and id's Quake III 1.32 source also recurse from a
portal seen through another portal. That was a misreading. fnTech3 passed
its `portal` flag through unchanged, so every pass from the eye or from a
portal already recursed; only Quake3e's second-view pass stopped. Reading
the same walk in every retail engine that shares it found three real
differences instead, fixed here:

- RTCW's engines send a viewer watching a scripted camera only the portals
  and what their cameras see. fnTech3 had no such rule.
- fnTech3 honoured Quake3e's second view, bit `0x20000` on the viewer's
  entity, in every profile whose flags reach the walk untranslated. No
  retail walk reads that bit, and Jedi Academy's single-player game uses it
  for its own breakable brushes.
- The walk kept about 6 KB of entity records in each recursive frame, so a
  chain about 170 portals deep would exhaust the 1 MiB main stack. Retail's
  frames are 0x30-0x40 bytes and reach the last portal entity.

The [JSON companion](2026-10-15-snapshot-portal-recursion.json) records each
binary, address, run and outcome. No compatibility gate closes.

Implementation is independently authored. The retail engines were read with
capstone and pefile (`.tmp/portal-recursion/walk.py`). id's GPL Quake III
1.32, RTCW, Enemy Territory and OpenJK sources were used only to name
routines and flags (reference observation; no text incorporated).

## Observed, retail engines

Each walk was found from its `FIXING ENT->S.NUMBER` string. In every one, a
portal entity that passes the visibility tests is added and then walked from
its `origin2`, with the walk's fourth argument set to 1, and nothing but the
already-added test stops a portal reached through another portal:

| Profile | Engine | Walk | Recursion | Reads its portal argument | Range test |
| --- | --- | --- | --- | --- | --- |
| `q3` | `quake3.exe` 1.32 | `0x43b6d0` | `0x43b972` | never | `generic1` |
| `ql` | `quakelive_steam.exe` | `0x4e5330` | `0x4e565d` | never | `generic1` |
| `rtcw-sp` | `WolfSP.exe` | `0x4390d0` | `0x439489` | camera test | none |
| `rtcw-mp` | `WolfMP.exe` | `0x43c390` | `0x43c749` | camera test | none |
| `wolfet` | `etded.exe`, `et.exe` 2.60b | `0x42f550`, `0x448ce0` | `0x42f8b5`, `0x449045` | has none | none |
| `stvef-mp` | `stvoyHM.exe` | `0x435ab0` | `0x435cc8` | never | none |
| `stvef-sp` | `stvoy.exe` | `0x4285a0` | `0x42876f` | never | none |
| `jka-mp` | `jamp.exe` 1.01 | `0x460e90` | `0x4613a7` | never | `generic1` |
| `jk2-mp` | `jk2mp.exe` 1.02a | `0x44fcf0` | `0x44ff65` | never | `generic1` |
| `jka-sp` | `jasp.exe` | `0x43c710` | `0x43c941` | Force Sight test | none |
| `jk2-sp` | `jk2sp.exe` | `0x43a170` | `0x43a33a` | never | none |

The range test skips a portal whose `generic1` is set and whose distance
from the pass's own viewpoint is greater (squared against squared,
`0x43b916..0x43b960` in `quake3.exe`). The IHV test (`0x41b1e2`) and all six
Q3Test releases (1.02 `0x41d6bf` to 1.08 `0x41f0d9`) recurse the same way
from walks of their own, which fnTech3 serves separately and which match.

Where the argument is read, it gates one rule:

- **RTCW's camera.** `WolfSP.exe` reads the viewer's game entity
  (`frame->ps.clientNum`) and, in the eye's pass only, tests its `eFlags`
  for `0x40000` (`0x4391de`, `0x4391e7`), the games' `EF_VIEWING_CAMERA`.
  With it set, each entity left after the client-flag and already-added
  tests (`0x4391d4`) is skipped unless it is a portal; a portal is added
  (`0x439203`) and walked with the argument 1 (`0x439215`), whether or not
  the eye sees it. Broadcast entities therefore arrive only through a
  camera's pass. `WolfMP.exe` does the same at `0x43c49e..0x43c4dd`. The
  games set the flag for a scripted camera (`startCamera`). Enemy
  Territory's engines have no such test.
- **Academy single player's Force Sight.** `jasp.exe` enables its Force
  Sight rule in the eye's pass only (`0x43c762`). fnTech3's shared walk has
  none of Raven's single-player rules (see below).

Enemy Territory's walks take no portal argument. They test the viewer's
`SVF_SELF_PORTAL` (`0x8000`) on every pass (`0x42f5af`, `0x448d3f`) and
recurse from its `origin2`, so a viewer carrying the bit would recurse
without end. ET's GPL game only ever clears it (inference for the retail
module).

Each engine's add marks the entity before testing the snapshot's entity cap
(`0x43b8f6` before `0x43b8fe`), so a portal past the cap still cannot be
walked twice: the depth is at most the number of portal entities.

## fnTech3 before this slice

`SV_AddEntitiesVisibleFromPoint` recursed with its own `portal` value, false
from the eye, and tested `!portal` before a portal and before Quake3e's
second view (`SVF_SELF_PORTAL2`), which recursed with true. So chains from
the eye already matched retail in every profile; the unit test below passes
its chain cases on the previous walk, and the live probe's "before" runtime
sends what retail sends. What did not match:

- RTCW's camera rule was absent: a camera's portal entity was walked only if
  the eye saw it, and the eye's own entities were sent as well.
- The second view was read for every profile that passes its flags through
  untranslated: `jka-mp`, `jk2-mp`, `jka-sp` and `jk2-sp` as well as `q3`
  and `ql`. Only Quake III and Quake Live modules can ask for it
  (`SV_GetValue`'s `SVF_SELF_PORTAL2_Q3E`). In Academy's single-player game
  `0x20000` is `SVF_BBRUSH`.
- The walk's frame was 5,948 bytes (MSVC `debugoptimized`, `S_FRAMEPROC`):
  three `sharedEntity_t` copies and, inlined, Enemy Territory's proxy
  target.

## Design

- **Chains** are unchanged: every pass from the eye or a portal walks the
  portals it adds, bounded by the already-added test.
  `SnapshotEntityNumbers::Add` already marked an entity before its cap test,
  as retail does. No depth guard is added: the bound is retail's.
- **RTCW's camera** (`rtcw-sp`, `rtcw-mp`): in the eye's pass, when the
  viewer's entity has `FNTECH3_RTCW_EF_VIEWING_CAMERA` (`0x40000`, now in
  `rtcw_game_abi.h`), the walk adds only portal entities, after the
  already-added test, each walked with `portal` true. Passes reached that
  way walk as any other, portals included.
- **The second view** is read for `q3` and `ql` only, and keeps Quake3e's
  rules there: it walks no portal of its own and runs at the end of each
  pass from the eye or a portal. The `!portal` test before a portal now
  applies to that pass alone.
- **The frame**: the walk reads records into two buffers held by the
  snapshot's numbers (the record being read and an ET proxy's target), and
  copies each viewpoint before recursing, since a nested pass reuses the
  buffer. The frame is now 60 bytes.
- **Kept with reasons**: ET's self portal is still expanded once per
  snapshot, as fnTech3 already did; retail would recurse without end on the
  same input. The range test is unchanged: Quake III, Quake Live and both
  Jedi multiplayer engines test `generic1`, and RTCW, ET and Elite Force
  records carry none, so their adapters leave it 0 and the test never fires
  there, as their engines have none. Each pass still rewrites the frame's
  area bits, as in retail (`quake3.exe` `0x43b70c`).

## Unit evidence

`tests/snapshot_portal_chain_tests.py` compiles the production walk with the
production Jedi broadcast rule over the shared fake game data, given three
rooms whose viewpoints see only their own cluster, in `q3`, `ql`, `rtcw-sp`,
`rtcw-mp`, `wolfet`, `stvef-mp`, `stvef-sp`, `jka-mp` and `jk2-mp`:

- A portal in the eye's room shows room 1, whose portal shows room 2: every
  profile sends all four entities and walks three viewpoints.
- A portal showing its own room, and two portals showing each other, end at
  the already-added test.
- With the camera flag on the viewer, RTCW sends both portals (one out of
  the eye's view), what their cameras see and a broadcast entity reached
  through them, but not an entity only the eye sees; every other profile
  ignores the flag.
- The second view adds its room in `q3` and `ql` and walks no portal from
  there; no other profile reads the bit.
- The `generic1` range is measured from the pass that sees the portal.
- A 1,023-deep chain past the entity cap ends with every viewpoint walked
  once (not run for the Jedi profiles, whose fixture has 16 records).

The test also checks that the walk keeps no record in its own frame. Run on
the previous walk, it passes every chain case, fails the camera case in both
RTCW profiles, sends the second view in seven profiles, and overflows the
stack on the deep chain. `wolfet_snapshot_tests.py`,
`jedi_snapshot_walk_tests.py`, `stvef_sp_snapshot_tests.py`,
`wolfet_viewpoint_tests.py` and `alice_snapshot_source_tests.py` pass.

## Live evidence, Jedi Academy

A synthetic game module (`.tmp/portal-recursion/probe/portal-probe.cpp`,
adapted from the broadcast slice's) allocates a bot on `mp/ffa1`, links test
entities around three spawn points none of which is in another's PVS (V, H
and K), and reads the bot's snapshot back through import 259, with
`fixedtime 75`. It ran in retail `jamp.exe` 1.01 and in two fnTech3
dedicated servers built from `b80e6f6e`, before and after this slice, each
with one identical diagnostic case serving import 259 (never committed):

| Stage | Retail `jamp.exe` | fnTech3 before | fnTech3 after |
| --- | --- | --- | --- |
| 0: portal at V shows H, portal at H shows K | 60, 61, 62, 63 | 60, 61, 62, 63 | 60, 61, 62, 63 |
| 1: bot entity has `0x20000`, `origin2` at K | 65 | **64**, 65 | 65 |
| 2: control, no portals | 60 | 60 | 60 |
| 3: control, only the first portal | 60, 61, 63 | 60, 61, 63 | 60, 61, 63 |

Entity 62 sits at K and is reached only through both portals. Every run
exited normally, left the installation unchanged, and pressed no key.

## Live evidence, RTCW camera, 2026-10-16

A second synthetic game module (`.tmp/rtcw-camera/probe/camera-probe.cpp`,
built once for each mode from one source) asks the same of RTCW's own
dedicated servers. It allocates a bot, takes three entity origins from the
map, none of them in another's PVS (V, H and K, on `mp_beach` for `rtcw-mp`
and `escape1` for `rtcw-sp`), links test entities there, and reads the bot's
snapshot back through RTCW's import 209 (`BOTLIB_GET_SNAPSHOT_ENTITY`) at its
120th and 130th game frames. The bot's entity carries `SVF_BOT`, so both
engines build its snapshot and keep it unsent, and the camera stages set
`0x40000` in that entity's `s.eFlags`. Entity 60 sits at H and, as a portal,
shows K; 61 sits at V, where only the eye sees it; 62 sits at K, where only
the camera sees it; 63 is a broadcast entity.

It ran in retail `WolfMP.exe` and `WolfSP.exe` dedicated servers and in two
fnTech3 dedicated servers built from `dfe1b9d4`: "after" is that tree, whose
walk is this slice's (neither file has changed since), and "before" takes
`sv_snapshot.cpp` and `rtcw_game_abi.h` from this slice's parent. fnTech3
refuses import 209, which no retail RTCW module calls, so both runtimes carry
one identical diagnostic case serving it (never committed). Both modes gave
the same sets in every engine:

| Stage | Retail `WolfMP.exe`, `WolfSP.exe` | fnTech3 before | fnTech3 after |
| --- | --- | --- | --- |
| 0: camera; portal 60 at H shows K | 60, 62, 63 | **61**, 63 | 60, 62, 63 |
| 1: control, camera off | 61, 63 | 61, 63 | 61, 63 |
| 2: camera; 60 not a portal | none | **61, 63** | none |
| 3: camera; stage 0 plus portal 64 at K showing V | 60, 61, 62, 63, 64 | **61, 63** | 60, 61, 62, 63, 64 |

With the camera flag, then, retail walks a portal the eye cannot see
(stage 0), sends neither what the eye sees nor a broadcast entity except
through a portal's pass (stages 0 and 2), and walks the portals a camera's
pass reaches as any other pass does, back to the eye's own room (stage 3).
fnTech3 after matches in every stage of both modes, in two runs each; before
ignored the flag. The frame-120 and frame-130 sets agree in every run, and
every run exited normally, left the installation unchanged and had no
client and no input. Retail `WolfSP.exe` first tries the installation's own
game library by its bare name, so it ran under cdb with a breakpoint that
fails that one attempt (`0x441cb4`, a register change; nothing on disk is
patched), as the [level clock record](2026-10-15-level-clock.md) did. Both
retail executables first exited at once with code 5, the SteamStub state,
until Steam was restarted.

The snapshot set is what the client receives, so this is the rule an RTCW
cutscene's camera relies on; a retail game's own camera scene, with a client
watching it, was not played.

## Remaining and unrelated findings

- RTCW's camera rule is Live against both retail servers through the
  synthetic module above (2026-10-16). A retail game's own camera scene,
  with a client watching it, was not played.
- Academy's single-player walk has rules fnTech3's shared walk does not:
  `jasp.exe` skips an entity whose `eFlags` has `0x80000` (`0x43c7a9`) and
  has the Force Sight rule above, and OpenJK's `code/server/sv_snapshot.cpp`
  also always sends entity 0 and sky-portal entities there. Outcast's
  single-player walk was read only for its portal recursion. Not examined
  further here.
