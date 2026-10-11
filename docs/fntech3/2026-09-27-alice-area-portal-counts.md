# A close past zero in Alice's area portals

## Scope

Under the `alice` profile, American McGee's Alice's retail map `wforest`
stopped as its level spawned: the engine dropped it with
`CM_AdjustAreaPortalState: negative reference count`. It was the only one of
the thirty-six maps to stop that way ([BSP index record](2026-09-27-bsp-index-checks.md)).
This record finds the entity and the call behind it, reads what retail
`alice.exe` and F.A.K.K.2 1.02's `fakk2.exe` do at the same point, and moves
the two Ritual profiles to it, and Medal of Honor on OpenMoHAA's reading.
Quake III, Quake Live, both RTCW modes, Enemy Territory, both Elite Force
modes, the four Jedi profiles, the IHV test and Q3Test keep the drop. The
evidence JSON is [2026-09-27-alice-area-portal-counts.json](2026-09-27-alice-area-portal-counts.json).

Area portals work alike in all these engines. Each pair of areas has a
reference count. An entity that stands in two areas, such as a door, raises
the count of that pair when it opens and lowers it when it closes. The areas
are then flooded again, and two areas join only over a count above zero. A
map loads with every count at zero, so every portal starts shut.

## Observed: the close

The game module traces its own events (`g_showevents 1`). On a developer
client (`scripts/probe_ritual_client.py`, `devmap wforest`), the last lines
before the drop were:

```
CThread #3:'Setup_LastPass' : Code :execute
...
ScriptSlave (*61) 'chesswall' : maps/wforest.scr(134) :closeportal
ERROR: CM_AdjustAreaPortalState: negative reference count
```

`wforest`'s entity string holds two `info_player_start`s. The first,
`wforest_start2` (entity 80 of 735), runs the thread `Setup_LastPass`; the
second, `wforest_start1` (entity 303), runs `Setup_FirstPass`. A load that
names no start began at `wforest_start2`, as the sweep's plain `map wforest`
did. `$chesswall` names four `script_object`s, the brush models `*16`, `*18`,
`*19` and `*20`. `Setup_FirstPass` opens their portal and hides them
(`maps/wforest.scr` line 40); `Setup_LastPass` closes it and shows them
(line 134). The map has just loaded, with every count at zero, and nothing
on the last pass opens the portal first, so the close takes its count below
zero.

The campaign enters `wforest` both ways. `centipede2` leaves for
`wforest$wforest_start1`, both through its `trigger_changelevel`
(`c2_changelevel`) and at the end of its cinematic
(`maps/cinematics/centipede2_cinematics.scr` line 168). `jlair2`'s
`pickup_eye()`, run when Alice takes the Eye Staff, leaves for
`wforest$wforest_start2` (`maps/cinematics/jlair2_cinematics.scr` and
`jlair2_cine_pete.scr`, line 46). Under the drop, the campaign could not
return from the Jabberwock's lair.

The call is the retail game module's (`base/fgamex86.dll`, 2,084,930 bytes,
SHA-256 `ae5a5727...1aea`). Its `closeportal` event (the event object at
`0x1042b0d8`, built at `0x1012e7a0`) is answered by `0x10135a00` in
`ScriptSlave`'s response table (`0x101d2540`, registered at `0x1012ecf0` for
the class ID `script_object`). That handler passes the entity and 0 to import
slot 39, `AdjustAreaPortalState` (`call [0x101e8dac]`, in the import table
at `0x101e8d10`, whose slot 0 is `Printf`), and tests no count of its own.
`openportal` (`0x101359e0`) passes 1. The engine's slot 39 (`0x004536d0`)
reads the two areas the entity was linked into (`+0x250` and `+0x254` of
its side record), returns when the second is -1, and otherwise calls
`CM_AdjustAreaPortalState` (`0x004361a0`). fnTech3's
`SV_AliceAdjustAreaPortalState` does the same.

## Observed: the retail engines

`alice.exe` (2,209,835 bytes, SHA-256 `8aa22917...fc18`) is SafeDisc-wrapped
and cannot run here, but its code section reads. The F.A.K.K.2 disc's
`fakk2.exe` is encrypted; the official 1.02 patch's (1,318,912 bytes,
SHA-256 `681bc51c...5785`) is plain. Both were read with capstone; addresses
are virtual addresses in each image.

| What | `alice.exe` | `fakk2.exe` 1.02 | Reading |
| --- | --- | --- | --- |
| `CM_AdjustAreaPortalState` | `0x004361a0` | `0x0042abb0` | an area below zero returns at once; an area at or past the map's count is `Com_Error( 1, "CM_ChangeAreaPortalState: bad area number" )` (`0x004361c7`, `0x0042abd7`); an open adds one to the pair's count both ways and a close takes one off both ways |
| Below zero | `0x00436254..0x00436264` | `0x0042ac64..0x0042ac74` | when a close leaves `[area2 * n + area1]` below zero, the engine calls `0x00438a40` (`0x0042d2a0`) with `"CM_AdjustAreaPortalState: negative reference count\n"`; nothing clamps the count |
| The print | `0x00438a40` | `0x0042d2a0` | `Com_DPrintf`: it formats and prints through `Com_Printf` only while the cvar at `0x00bd1c58` (`0x00ad6558`) is set; that is `developer`, registered `"0"` (`0x004398c6..0x004398d7`, `0x0042e11c..0x0042e137`) |
| Then | `0x00436267` | `0x0042ac77` | `CM_FloodAreaConnections` (`0x00436150`, `0x0042ab60`), after an open, a close or a close past zero alike |
| `CM_FloodArea_r` | `0x004360d0` | `0x0042aae0` | follows a pair only when its count is above zero (`cmp [edi], 0; jle`, `0x00436126`, `0x0042ab36`) |
| `CM_ReadPortalState` | `0x004362e0` | `0x0042acf0` | a different area count is `Com_Error( 1, "numareaportals differs from save game" )` (`0x00436302`, `0x0042ad12`); otherwise the saved block is read over the counts whole, with no test, and the areas are flooded |
| `CM_WritePortalState` | `0x004362a0` | `0x0042acb0` | the area count and the counts as they stand |

So in both engines a close past zero does not stop the level. The count
stays below zero and the portal stays shut. A later open brings the count
back to zero, still shut, and the portal opens only when the opens outnumber
the closes. With `developer` at its default of 0, the player sees nothing.
A save made meanwhile holds the count below zero, and a load puts it back.
F.A.K.K.2 needs no inference from Alice here: its own 1.02 engine reads the
same, instruction for instruction. That leaves 1.00, which the disc encrypts,
to be taken as 1.02 (inferred).

## Observed: every other engine drops

Every other retail engine on this machine was read the same way. Each passes
its own `"CM_AdjustAreaPortalState: negative reference count"` to the
function its `"CM_ChangeAreaPortalState: bad area number"` goes to, with the
level that drops: 1 in each, 2 in Enemy Territory, whose error levels put
`ERR_VID_FATAL` before `ERR_DROP`. The JSON gives each executable's hash.

| Profile | Executable | Site | `Com_Error` |
| --- | --- | --- | --- |
| `q3` | `quake3.exe` 1.32 | `0x00419475` | `0x0041cd90` |
| `ql` | `quakelive_steam.exe` | `0x004c4f2f` | `0x004c9b60` |
| `rtcw-sp` | `WolfSP.exe` | `0x004184d9` | `0x0041b850` |
| `rtcw-mp` | `WolfMP.exe` | `0x0041aff9` | `0x0041e420` |
| `wolfet` | `et.exe` 2.60b | `0x00426c20` | `0x00429cf0` |
| `stvef-sp` | `stvoy.exe` | `0x0040f2f4` | `0x00411170` |
| `stvef-mp` | `stvoyHM.exe` | `0x00417da5` | `0x0041a0a0` |
| `q3-ihv` | the IHV test's `quake3.exe` | `0x0040aeba` | `0x0040cca0` |
| `jka-sp` | `jasp.exe` | `0x00417921` | `0x0041a440` |
| `jka-mp` | `jamp.exe` | `0x004332d7` | `0x00437290` |
| `jk2-sp` | `jk2sp.exe` | `0x00412249` | `0x004142b0` |
| `jk2-mp` | `jk2mp.exe` | `0x00424bc9` | `0x00427ed0` |
| `q3test` | 1.02, 1.03, 1.05, 1.06, 1.07, 1.08 `quake3.exe` | `0x0040cdaa`, `0x0040d09a`, `0x0040d57a`, `0x0040ddfa`, `0x0040de3a`, `0x0040e7aa` | `0x0040eac0`, `0x0040ee00`, `0x0040f2e0`, `0x0040fbc0`, `0x0040fc00`, `0x00410580` |

So the drop those profiles keep is their own retail engines' rule, and the
Ritual rule is Ritual's alone among the engines that can be read.

## Medal of Honor

`MOHAA.exe` is SafeDisc-encrypted and cannot be read. OpenMoHAA (pinned at
`ab43a0de`) reconstructs 2015's engine with the Ritual rule
**[O]**: `code/qcommon/cm_test.c` prints the negative count through
`Com_DPrintf`, and its `CM_ReadPortalState` reads the saved block back whole.
fnTech3's Medal of Honor adapters reach the same function: import 68,
`AdjustAreaPortalState`, calls `CM_AdjustAreaPortalState` for an entity
that stands in two areas (`code/server/server_mohaa_game.hpp`). No retail
Medal of Honor data was found to close a portal from zero. No script in the
six paks names `openportal` or `closeportal`. By OpenMoHAA's reading of the
game module **[O]**, a door's master opens its portal as it starts to open
from shut and closes it as it finishes shutting, so a door balances its own
count. The rule follows OpenMoHAA **[O]**, as the invalid-bevel report
already does in `code/qcommon/cm_patch.c`.

## Observed: the shipped scripts

Every `openportal` and `closeportal` in the retail scripts was listed from
the installations' paks. Alice has 29 in 10 scripts, F.A.K.K.2 37 in 14 (its
1.02 patch's `pak2.pk3` adds none), and Medal of Honor none. Besides
`wforest`, two Alice levels can close a portal from zero in normal play:

- `wchess1`'s intro (`cinema_wchess1_intro_thread`) starts as the player
  spawns. It opens `$quad_gate_top`'s portal some nine seconds in (line 369)
  and closes it at its end (395). Its skip thread, `Skipthread_cwit`, runs
  `cwit_End`, which closes the portal whether or not the intro opened it
  (line 314). A skip in the intro's first nine seconds therefore closes a
  portal nothing opened.
- `funhouse`'s Tweedle scene (`Funhouse_Tweedle_Cinema1`) sets its skip
  thread and opens `$end_door1`'s portal in the same frame (lines 479-485).
  It closes the portal about two and a half seconds later (506) and then
  plays its dialogue. Its skip thread, `Skipthread_F2`, ends the scene with
  `F2_End`, which closes the portal again (451). A skip during the dialogue
  therefore takes the count below zero.

`wchess1` opens its gate's portal twice in a row when the bell is rung, both
in `cinema_ring_bell_thread` (lines 1073-1074) and in its skip thread
`crbt_End` (1009-1010). That brings the count above zero from either -1 or
0, whichever an early or a late skip of the intro left, where one open would
have served a count that cannot fall below zero. The level appears to have
been written against retail's rule (inferred). Some of F.A.K.K.2's scripts
guard their calls with level variables (`creeperpens.scr` and
`training.scr`: `level.pensdoor1_portal ifequal 0 $pensdoor1 openportal`);
the rest were not traced.

## Changed

- `CM_AdjustAreaPortalState` (`code/qcommon/cm_test.c`): under `alice`,
  `fakk2` and `mohaa`, a close that takes a count below zero is reported
  through `Com_DPrintf` in retail's words, the count is kept, and the areas
  are flooded. Every other profile keeps the `ERR_DROP`.
- Alice's and F.A.K.K.2's save restore (`SV_AliceRestorePortalState`,
  `code/server/server_alice_save.hpp`) had refused a save holding a negative
  count, a check retail does not make. It now puts such a count back as
  retail's `CM_ReadPortalState` does. A block for another map, or one cut
  short, is still refused.
- Medal of Honor's save restore (`RestorePortals`,
  `code/server/server_mohaa_save.hpp`) had raised a negative count to zero.
  It now puts the count back as saved, as OpenMoHAA's reader does
  **[O]**. Such a count would otherwise come back as a portal that one open
  unlocks, where the running level needed two.

## Measured

Two builds, frozen in fixed folders so that no rebuild could replace them
mid-run: before, `.tmp/runtime/alice-portal-before`, a `git archive` of
`2d999a9` configured like `win32-dev` (strict warnings, all ten development
options); after, `.tmp/runtime/alice-portal-after`, the same build directory
with this change. The JSON gives each binary's hash, unchanged from the first
run to the last. Every run was a Windows x86 client, windowed on a private
desktop with an isolated home, with no input sent. The Alice installation and
the laid-down F.A.K.K.2 game were left unchanged: no file in either was
modified during the runs.

**Map loads.** `scripts/probe_retail_map_loads.py` (GLx, `developer 0`) over
the 36 Alice and 30 F.A.K.K.2 maps, three at a time, once per build. Before,
65 loaded and `alice:wforest` stopped with the drop, as first reported;
after, all 66 loaded. `--compare` finds one change in 66: `wforest`, now
loaded with 7,490 faces and 46 meshes. Every other map's outcome and
surface counts are the same in both builds.

**Developer clients.** `scripts/probe_ritual_client.py --profile alice`,
`developer 1`, one run a build each:

| Run | Before | After |
| --- | --- | --- |
| `devmap wforest$wforest_start2`, 10 s | dropped while spawning: `CM_AdjustAreaPortalState: negative reference count` | one developer line, `CM_AdjustAreaPortalState: negative reference count`, then play; the capture shows Alice in play at the second start, with her HUD |
| `devmap wforest$wforest_start1`, 10 s | play, no report | play, no report |
| `devmap wchess1`, `skipcinematic` at 3 s, 8 s more | dropped with the same error once the skip thread closed the gate's portal | one developer line, then play on the board |

The two runs that dropped sat at the console until their timeout, and the
private-process helper then raised `PermissionError` while stopping them;
their console logs are complete.

**A save on the last pass.** After, `devmap wforest$wforest_start2`, then
`savegame portals` 10 s in, `loadgame portals` 3 s later, and a capture 10 s
after that. `save/config_portals.ssv` is 460 bytes: the 200-byte header,
the area count 8, and 64 counts, of which only areas 0 and 4 differ from
zero, at -1 both ways. That is the chess wall's close, the one the
developer line reported. The load read the level back (the game module
reports `ActiveList has 6 elems, SleepList has 47 elems` on writing and on
reading) and play resumed where it was saved. The reader before this change
refuses that block with `The save game holds a negative area portal count`
(read from the code; the unit test below runs it).

**Tests.** `tests/area_portal_count_tests.py` compiles the engine's own
area-portal code from `code/qcommon/cm_test.c` with the real profile catalog
and profile tests from `code/qcommon/game_profile.cpp`, and runs every
catalog profile. Under `alice`, `fakk2` and `mohaa` a close from zero is
reported in retail's words, the count stays at -1 and the areas are
flooded; one open then leaves the portal shut, and a second opens it. The
other thirteen profiles drop with no report and no flood. For all sixteen,
an area past the map drops and a negative area is ignored. A second fixture
compiles the Alice and Medal of Honor save restores: counts of -2 come back
as saved, and another map's block or one cut short is still refused or left
as spawned. Run against a tree of `2d999a9`'s code, the profile fixture
fails for `alice`, `fakk2` and `mohaa` alone, and the restore fixture fails
for both restores. `tests/entity_box_contents_tests.py` links the whole of
`cm_test.c`, so its prelude gains the two profile tests the file now calls.
On an export of `2d999a9` with this change, before the documents were
written, the Meson suite passed (175 passed, 2 skipped, none failed). The
Python suite passed 2,321 tests with 128 skipped. Its one failure was that
link, run before the prelude was fixed; the file then passed alone. On the
commit's own content, documents included, the Python suite passed 2,322
tests with 128 skipped and none failed.

## Not established

- No retail-written save of `wforest`'s last pass, or of any Alice level,
  has been read, and no fnTech3 save has been loaded by retail.
- No frame has been compared with retail's. After the last pass, retail's
  counts keep the chess wall's portal shut, and so do fnTech3's; the effect
  on what a client is sent was not measured.
- `funhouse`'s Tweedle scene was not played; its close is read from the
  script. The other closes in Alice's scripts (`pandemonium`'s minecart,
  `hatter1`'s `NORETURN`, `skool1`'s lift) and F.A.K.K.2's unguarded ones
  were not traced. (*2026-10-08:* the Tweedle scene, `hatter1`'s clock room
  and `skool1`'s lift were played and the rest of Alice's traced;
  [round record](2026-10-08-alice-compatibility-round.md#area-portals). F.A.K.K.2's remain.)
- F.A.K.K.2 1.00's engine is taken to be 1.02's. Medal of Honor's rule is
  OpenMoHAA's **[O]**. No Medal of Honor content has been found that reaches
  it.
