# Enemy Territory map loads settle their game in six frames

## Scope

A map load starts a new game module. The engine runs a few frames to let the
level settle, reconnects the clients it carried over and begins its bots, then
runs one more frame. fnTech3's `SV_SpawnServer` did this the same way for
`wolfet` as for Quake III, as Quake3e does. Retail Enemy Territory 2.60b
differs in two ways, which this record checks against both retail executables:

1. It runs six settling frames, not three, and runs its bots with each. Each
   frame runs at the current time, and the time advances after it. Its last
   frame runs the same way.
2. It drops every bot when the game type is single player or co-op, as well
   as when the map command asks it to, and gives those bots no reason.

Both are now reproduced for the `wolfet` profile. This is the map-load
counterpart of the `SV_MapRestart_f` change in
`2026-09-24-wolfet-restart-settle.md`, which was uncommitted when this was
written. This change leaves every other profile's frames and bots as they
were. Their frame order is a separate change,
`2026-09-24-settle-frame-order.md`, also uncommitted when this was written.

## Observed

### et.exe and etded.exe

ET 2.60b `et.exe`, SHA-256
`4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e`, and
`etded.exe`, SHA-256
`7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf`, were read
with capstone. Their `SV_SpawnServer` runs from `0x4467f0` to `0x446c4f` in
`et.exe` and from `0x42cf20` to `0x42d37f` in `etded.exe`, with the same
instructions at their own addresses.

| `et.exe` | `etded.exe` | Behaviour |
| --- | --- | --- |
| `0x4469d7` | `0x42d107` | `SV_InitGameProgs`. Its `SV_InitGameVM` (`et.exe` `0x445540`, `etded.exe` `0x42bc60`) gives `GAME_INIT` `svs.time` as the level's start (`0x44558a`, `0x42bcaa`). |
| `0x4469e1` | `0x42d111` | `mov esi, 6`, the settling frame count |
| `0x4469e6..0x446a12` | `0x42d116..0x42d142` | Each pass calls `VM_Call( gvm, 8, svs.time )`, `GAME_RUN_FRAME`, then `SV_BotFrame( svs.time )` (`0x43efc0`, `0x425390`), and only then adds 100 to `svs.time`. |
| `0x446a14` | `0x42d144` | `SV_CreateBaseline` |
| `0x446a35` | `0x42d165` | A client below `CS_CONNECTED` (2) is skipped |
| `0x446a3f..0x446a7b` | `0x42d16f..0x42d1ab` | A client whose `remoteAddress.type` (+`0x546d8`) is `NA_BOT` (0) is dropped when the map command's `killBots` argument is set, or when `0x442cf0` or `0x442cd0` (`0x429440`, `0x429420`) returns non-zero. The call is `SV_DropClient( client, "" )`: the reason (`0x5586ec`, `0x4be549`) is an empty string. |
| `0x446a82..0x446aa7` | `0x42d1b2..0x42d1d7` | `GAME_CLIENT_CONNECT` (2) with `firstTime` 0 and the bot flag. A refusal drops the client with the game's reason. |
| `0x446aad..0x446ab2` | `0x42d1dd..0x42d1e2` | A person the game accepts goes back to `CS_CONNECTED`, to be sent the new gamestate |
| `0x446abb..0x446af7` | `0x42d1eb..0x42d227` | A bot the game accepts is begun: `CS_ACTIVE` (4), its entity and entity number, `deltaMessage` −1, a snapshot due at once, then `GAME_CLIENT_BEGIN` (3) |
| `0x446b14..0x446b3a` | `0x42d244..0x42d26a` | The last frame: `GAME_RUN_FRAME` and `SV_BotFrame` at `svs.time`, then adding 100 |

So a map load runs its game's frames, and its bots, at the level's start time
*T*, *T*+100, and so on up to *T*+500. It reconnects the clients and begins
its bots while the game's time is *T*+500, runs its last frame at *T*+600 and
leaves `svs.time` at *T*+700.

`SV_BotFrame` calls `BOTAI_START_FRAME` (11) when `bot_enable` is set and a
game is loaded, as fnTech3's does. `0x442cf0` and `0x442cd0` are the
single-player and co-op tests the restart record identified. Each shifts 1
left by `g_gametype->integer` and masks the result with the single-player or
co-op set of the engine's `gameinfo.dat` record (`0x121c824`, `0x121c82c`).
They run after the new game has registered its cvars. So they test the game
type the new level runs, including a value the console latched.

Retail begins a bot with inline code, not through `SV_ClientEnterWorld`, and
leaves its last command alone. fnTech3's `SV_ClientEnterWorld( client,
nullptr )` does the same for a bot. It makes the client active, points it at
its entity, forces a delta reset and a snapshot, keeps `lastUsercmd` and
calls `GAME_CLIENT_BEGIN`. For a person, fnTech3 also marks the gamestate
unsent and clears the entity pointer, as its connection handling requires.
Retail's `SV_InitGameVM` has already cleared every client's entity pointer
(`0x445560..0x44557f`).

`SV_Map_f` (`0x43fd90`) passes `killBots` for `devmap` and `spdevmap` only
(`0x440147..0x44017c`, call at `0x440194`).

ET's GPL source was read for its names and as a cross-check only. The
behaviour above is taken from the retail binaries, and no source text was
incorporated.

### fnTech3 before this change

Every map load added 100 to `sv.time` before each of three settling frames and
before its last frame, and ran the bots with each. The game ran frames at
*T*+100 to *T*+300 and *T*+400. It began the bots at *T*+300 and left the
level at *T*+400. The first frame never ran at the time `GAME_INIT` was
given. Bots were dropped only when the map command asked, with Quake3e's
reason, `was kicked`.

## Decisions

1. **Settling frames: a compatibility requirement.** `sv.time` is what
   fnTech3 gives the game, its bots and its snapshots, so it plays the part of
   ET's `svs.time`, as in the restart record. The game module sees a
   different sequence of frame times: a first frame with no elapsed time, and
   six frames before its clients come back, not three. Map scripts reach a
   different point by then. On goldrush a script that waits 500 ms from the
   level's start goes on in the load's last frame under retail, but after the
   load under the previous build (see [Evidence](#evidence)). `wolfet` now
   follows `et.exe`.
2. **Bots of a single-player or co-op game type: a compatibility
   requirement.** This is an engine rule of both retail executables, and it
   takes no change to how clients connect. `wolfet` now drops such bots. It
   uses the `gameinfo.dat` sets fnTech3 already reads, evaluated where retail
   evaluates them. Retail content does not reach it. `pak0.pk3`'s
   `gameinfo.dat` names 0 and 1 as its single-player and co-op game types,
   and the restart record found that the retail game module resets game
   types below 2 and has no `addbot` command.
3. **The reason: a compatibility requirement.** Retail gives every bot it
   drops here the same empty reason, including the bots the map command drops.
   `wolfet` now gives `""` for both. Where ET announces a drop, a dropped
   bot's notice then reads `cpm "<name>^7 \n"`, as retail's does. Every other
   profile keeps `was kicked`.
4. **Bot entry and reconnected people: no change.** Both already match retail,
   apart from fnTech3's own connection bookkeeping. With six frames the game
   begins its bots at *T*+500, as retail's does. ET's `ClientBegin` sends a
   team player to limbo only once the game's time is more than 600 ms past the
   level's start, as the restart record found, so neither order sent a bot
   there.

## Change

- `SV_SpawnServer` (`code/server/sv_init.cpp`), `wolfet` only:
  - It runs six settling frames, each with its bots at `sv.time` before the
    time advances, and its last frame the same way. A load therefore leaves
    the level 700 ms on.
  - It drops a bot when the new game's type is single player or co-op, as
    well as when the map command asks. A dropped bot gets an empty reason.
- Every other profile keeps its frames. It drops bots only when the map
  command asks, and still gives `was kicked`.
- Unchanged for every profile, `wolfet` included: one console `wait` counts
  down per frame, and fnTech3's own `svs.time` advances by 100 ms.

## Evidence

- `tests/wolfet_map_load_settle_tests.py` compiles the production block of
  `SV_SpawnServer`, from the settling frames to the last frame. It runs the
  block for all thirteen profiles:
  - Every game type from 0 to 5, with and without `killBots`.
  - Eight client slots: free, zombie, connected, loading, active, an active
    bot, a loading bot, and an active client the game refuses.

  It checks every call, the time each one sees and each drop's reason:
  - `wolfet` runs frames and bots at +0 to +500 and +600, begins its bots at
    +500 and ends at +700. It drops its bots with an empty reason when
    `killBots` is set or the game type is 0 or 1.
  - Every other profile runs +100 to +300 and +400, begins its bots at +300
    and ends at +400. It drops them only for `killBots`, with `was kicked`.
  - The IHV test's and Alice's own settle paths are checked where the tree
    has them.
  - For all of them, every reconnected person waits for the new gamestate,
    and the refused client is dropped.

  Fourteen mutations each fail it:
  - ET settling in three frames.
  - ET advancing the time before each settling frame.
  - ET running its bots before its game.
  - ET settling without its bots.
  - Every profile settling as ET does.
  - ET advancing the time before its last frame.
  - Every profile running its last frame as ET does.
  - ET keeping its single-player and co-op bots, its co-op bots alone, or its
    single-player bots alone.
  - Every profile dropping its single-player and co-op bots.
  - ET keeping its bots when the map command drops them.
  - ET dropping its bots with `was kicked`.
  - Every profile dropping its bots with no reason.
- The same checks fail on the tested commit's block at the `wolfet` profile,
  whose first frame came at +100, and pass there with `wolfet` skipped. They
  do the same on the shared working tree's block with this change reverted,
  which carried the uncommitted IHV and Alice paths. With this change, both
  pass.
- The other fixtures that slice or read `SV_SpawnServer` pass on the tested
  commit plus this change: `spawn_view_angle_source_tests.py`,
  `wolfet_map_load_tests.py`, `rtcw_server_policy_tests.py`,
  `rtcw_module_cvar_tests.py`, `ql_cvar_configstring_source_tests.py`,
  `workshop_source_tests.py` and `rtcw_match_restart_tests.py`. The
  uncommitted `alice_savegame_source_tests.py`'s settle assertions held on
  the shared working tree with this change.
- `sv_init.cpp` compiles with `win32-dev`'s strict-warning client and
  dedicated flags in the shared working tree. It also compiles as the tested
  commit plus this change.
- Runtime, Windows x86. Two dedicated servers were built from a clean
  `git archive` export of `2d35919`, debugoptimized with strict warnings and
  `-Dwolfet-development=true`:
  - Baseline, the export as it is:
    `579163bd27d258a8d418fd2fef445f0675e566491b364bfebeaef7d5d3575de2`.
  - Candidate, the export with this change:
    `120ca0e675cdc8648dcb2ed2f669795582ff3bf2ddd2f66b8fed1edb01c70aef`.

  `scripts/probe_wolfet_restart_settle.py` was uncommitted when this was
  written; the runs used SHA-256
  `8385b296bd78f9bd06bb1e10ef2bb5bf71a109b2cb5a8f7f24c337ba0bb3a9bf`. It runs
  retail `etded.exe` and then one fnTech3 server on a private desktop. Each
  server gets an isolated home under `.tmp/wolfet-map-load-settle/`, loopback
  only, no masters, no clients and no input. Each loads goldrush with `devmap`
  and `g_scriptDebug 1`, a cheat cvar, so the retail game module prints the
  level time of every map script event and command in milliseconds. The probe
  takes the load's lines from its `Game Initialization` banner, relative to
  the level start the spawn events print. It compares the first 2,500 ms with
  retail's. It does the same for two `map_restart 0`, which this change does
  not touch.

  | The load | Retail `etded.exe` | fnTech3 baseline | fnTech3 candidate |
  | --- | --- | --- | --- |
  | Times with script output, first 850 ms (ms after the level start) | 0, 100, 200, 300, 400, 500, 600, 750, 850 | 0, 100, 200, 300, 400, 450, 500, 550, 600 | 0, 100, 200, 300, 400, 500, 600, 750, 850 |
  | Script lines in the first 2,500 ms | 729 | 693 | 729 |
  | SHA-256 of those lines | `2780886f…81c4` | `bc661584…edae` | `2780886f…81c4` |
  | Same as retail | | no | yes, in both runs |

  Retail's load gave the same 729 lines in all three of its runs, the same
  lines its restarts gave. The candidate matched them line for line in both of
  its runs. The baseline first differs at line 326:
  - Retail and the candidate print goldrush's `truck_build` script's
    `wait 500` a second time at +0. That line comes from the first frame,
    which runs at the level's start.
  - The baseline's first frame came at +100.
  - That wait ends, and the script moves on, at +600 under retail and the
    candidate. That is the load's last frame. Under the baseline it ended at
    +550, in the third frame after the load.

  Both candidate runs' restarts gave the baseline's 693 lines (`bc661584…edae`),
  as they should: the tested commit does not carry the restart change.

  Every server logged no error, no lookup and no `variable change` reload.
  Each accepted every rcon command exactly once and ended with `gamestate` 0,
  and the installation's inventory was unchanged. Each fnTech3 server had the
  same hash before and after its runs. The module and `pak0.pk3`
  (`712966b20e06523fe81419516500e499c86b2b4fec823856ddbd333fcb3d26e5`) were
  retail's. The commands, configs, digests and per-step results are in the
  [evidence file](2026-09-24-wolfet-map-load-settle.json). The lines
  themselves quote the retail map script, so the evidence keeps only their
  digest.

## Not established

- **No client was connected.** A live client's view of the load, its
  gamestate and its first snapshots rest on the disassembly and the fixture.
- **The bot rules were not exercised live.** fnTech3 cannot spawn an ET bot
  yet (`2026-09-24-wolfet-bot-broadcasts.md`, uncommitted when this was
  written), and the retail game refuses the single-player and co-op game
  types.
- **`devmap` still keeps bots.** Retail `SV_Map_f` drops them for `devmap`
  and `spdevmap`. fnTech3's follows Quake3e for `wolfet`: only `spmap` and
  `spdevmap` drop them. That command is not changed here.
- **Console waits.** Retail counts a console `wait` down once per
  command-buffer pass, so a load uses none of it. fnTech3 counts one per
  frame, and now spends seven in an ET load, where it spent four.
- **fnTech3's own `svs.time`**, which neither the game nor clients see,
  advances 100 ms where retail's single clock advances 700.
- **A later load's level start.** Both servers started goldrush at level time
  0 here. fnTech3 also starts a later load onto an empty server at 0, as
  Quake3e does, where retail's clock runs on. That was not examined.
- **The two ET settle changes were not run together as one build.** This
  change and the restart change touch different functions, and the probe's
  load ends before its first restart.
