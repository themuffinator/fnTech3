# Enemy Territory map_restart settles its game in six frames

## Scope

An immediate `map_restart` restarts the game module in place. The engine runs
a few frames to let the level settle, reconnects the clients and begins those
it can, then runs one more frame. fnTech3's `SV_MapRestart_f` did this the same
way for every profile, as Quake3e does. Retail Enemy Territory
2.60b differs in three ways, which this record checks against both retail
executables:

1. It runs six settling frames, not three. Each frame runs at the current time,
   and the time advances after it.
2. It makes every client it reconnects active and enters it into the world,
   not only the clients that were already active.
3. In a single-player or co-op game type it leaves bots out of the restart.

The first and third are now reproduced for the `wolfet` profile. The second is
recorded but not reproduced, for the reasons under
[Decisions](#decisions). Every other profile is unchanged.

## Observed

### et.exe and etded.exe

ET 2.60b `et.exe`, SHA-256
`4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e`, and
`etded.exe`, SHA-256
`7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf`, were read
with capstone. Their `SV_MapRestart_f` runs from `0x43fa20` to `0x43fd83` in
`et.exe` and from `0x425e70` to `0x42620d` in `etded.exe`. Both have the same
instructions, apart from `etded.exe`'s stack cookie.

| `et.exe` | `etded.exe` | Behaviour |
| --- | --- | --- |
| `0x43fc30` | `0x4260b2` | `SV_RestartGameProgs`. Its `SV_InitGameVM` (`et.exe` `0x445540`) calls `GAME_INIT` with `svs.time` (`0x11ef5c4`) as the level time (`0x44558a`). |
| `0x43fc3b` | `0x4260bd` | `mov esi, 6`, the settling frame count |
| `0x43fc40..0x43fc63` | `0x4260c2..0x4260e5` | Each pass calls `VM_Call( gvm, 8, svs.time )`, `GAME_RUN_FRAME`, and only then adds 100 to `svs.time`. The first frame runs at the time `GAME_INIT` was given, the sixth 500 ms after it. |
| `0x43fc98` | `0x426118` | A client below `CS_CONNECTED` (2) is skipped |
| `0x43fca1..0x43fcbe` | `0x426121..0x42613e` | A client whose `remoteAddress.type` (+`0x546d8`) is `NA_BOT` (0) is skipped outright when `0x442cf0` or `0x442cd0` (`etded.exe` `0x429440`, `0x429420`) returns non-zero: no `map_restart` command, no `GAME_CLIENT_CONNECT`, no entry |
| `0x43fcc7..0x43fcea` | `0x426147..0x42616a` | The `map_restart` server command, then `GAME_CLIENT_CONNECT` (2) with `firstTime` 0 and the bot flag |
| `0x43fd1f..0x43fd2d` | `0x42619f..0x4261ad` | For every client the game accepts: `state = CS_ACTIVE` (4), then `SV_ClientEnterWorld( client, &client->lastUsercmd )` (+`0x48424`) |
| `0x43fd57..0x43fd65` | `0x4261d7..0x4261e5` | The last frame, again at `svs.time` before adding 100 |

So a restart runs frames at the level's start time *T*, *T*+100, and so on up
to *T*+500. It begins the clients while the game's time is *T*+500, runs its
last frame at *T*+600 and leaves `svs.time` at *T*+700.

`0x442cf0` and `0x442cd0` each shift 1 left by `g_gametype->integer` and mask
the result with a dword of the engine's `gameinfo.dat` record: `0x121c824` and
`0x121c82c`. The same function reads `0x121c838` as `usesProfiles` for its
savegame path. That places the two masks as the single-player and co-op game
types, as in ET's GPL `gameInfo_t`. The cvar pointer at `0x1184de8` is filled
from `Cvar_Get( "g_gametype", ... )` at `0x4463ca`. The two tests run inside
the client loop, after the game has restarted. So the game type they test is
the one the restarted game registered, including a value the console had
latched.

`SV_ClientEnterWorld` (`et.exe` `0x440e60`) sets the state to `CS_ACTIVE`, points
the client at its entity, resets the delta and snapshot times and copies
`*cmd` into `lastUsercmd` (`0x440ec3..0x440edd`). It then calls
`GAME_CLIENT_BEGIN` (3). Given `&client->lastUsercmd`, the copy leaves the
command as it was. That is what fnTech3's `SV_ClientEnterWorld( client, nullptr )`
does for a restarted active client.

Retail can enter a client that is still connecting or loading because its
`SV_ExecuteClientMessage` (`0x441b8b..0x441c09`) sends a gamestate to any
client, whatever its state, whose packet names a `serverId` from before the
current level and acknowledges a message past the last gamestate it was sent.
Each restart gives `sv.serverId` a new value (`0x43fbe3..0x43fc05`). A packet
naming one of the level's earlier values is ignored until the client catches
up (`0x441bc0..0x441be4`). Pure checksums are no obstacle. `SV_UserMove`
(`0x442202..0x442217`) drops a client without valid ones on every move packet
of a pure server, and fnTech3's `SV_UserMove` does the same.

### The retail game module

`etmain/qagame_mp_x86.dll`, SHA-256
`160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b`. Its
`G_InitGame` (`0x20051990`, which prints `------- Game Initialization -------`)
stores the level time it is given at both `0x20916920` and `0x2091692c`
(`0x20051aae..0x20051ab3`). Those are `level.time` and `level.startTime`.
`ClientBegin`, which logs `ClientBegin: %i` at `0x20040189`, subtracts the
second from the first at `0x2004010e..0x20040120` and compares the result with
`0x258`, 600. When the result is greater it sends a player on a team to limbo.
ET's GPL game source has this test as
`level.time - level.startTime > FRAMETIME * GAME_INIT_FRAMES`, with 100 and 6,
and its `q_shared.h` names the engine's settling frames `GAME_INIT_FRAMES` too.

The same module has no `addbot` command, and it resets game types below 2
(`g_gametype %i is out of range, defaulting to GT_WOLF(%i)`). Retail content
therefore never reaches the bot skip. A modification that defines
single-player or co-op game types and adds bots does.

ET's GPL source was read for its names and as a cross-check only. The
behaviour above is taken from the retail binaries, and no source text was
incorporated.

### fnTech3 before this change

Every in-place restart, whatever the profile, added 100 to `sv.time` before
each of three settling frames and before its last frame. The game ran frames at
*T*+100 to *T*+300 and *T*+400, began clients at *T*+300 and left the level at
*T*+400. The first frame never ran at the time `GAME_INIT` was given. The
restart reconnected every client, bots in any game type included, and entered
only the clients that were already active.

## Decisions

1. **Settling frames: a compatibility requirement.** `sv.time` is what fnTech3
   gives the game, its snapshots and its delayed restarts, so it plays the
   part of ET's `svs.time`. The game module sees a different sequence of frame
   times: a first frame with no elapsed time, and six frames before its
   clients begin, not three. Map scripts reach a different point by then. On
   goldrush a script that waits 500 ms from the level's start goes on in the
   restart's last frame under retail, but after the restart under the previous
   build (see [Evidence](#evidence)). The level also runs 300 ms further per
   restart. `wolfet` now follows `et.exe`.
2. **Entering connecting and loading clients: a real difference, not
   reproduced.** For a client that was already active, retail and fnTech3
   already agree, as described above. For the others, retail's forced entry
   relies on its `serverId` check to deliver their gamestate. fnTech3's
   Quake3e-based connection handling has nothing like it for an active client:
   - It sends a gamestate only to a `CS_CONNECTED` client, and resends it only
     to a `CS_PRIMED` client that has not acknowledged it
     (`SV_ExecuteClientMessage`).
   - `SV_DoneDownload_f` sends nothing to an active client.
   - `SV_ClientEnterWorld` marks the gamestate acknowledged.

   Forcing entry here would therefore leave a connecting or downloading client
   without its gamestate, and a loading client whose gamestate was lost would
   never get it again. The spawn would also anchor the player's view to a
   command from before the gamestate. The spawn-view-angle work removed that
   (`tests/spawn_view_angle_source_tests.py`). Such clients still enter on
   their first command, as before. The difference this leaves is described
   under [Not established](#not-established).
3. **Bots in single-player and co-op game types: a compatibility requirement.**
   This is an engine rule of both retail executables, and it takes no client
   state change. `wolfet` now skips such bots before the `map_restart` command.
   It uses the `gameinfo.dat` single-player and co-op sets fnTech3 already
   reads, evaluated where retail evaluates them.

## Change

- `SV_MapRestart_f` (`code/server/sv_ccmds.cpp`), `wolfet` only:
  - It runs six settling frames. Each frame runs at `sv.time` before the time
    advances, and the last frame is run the same way, so a restart leaves the
    level 700 ms on.
  - A bot is skipped before its `map_restart` command when the game type,
    read after the game restarted, is single player or co-op.
- A comment at the client entry records why retail's entry of connecting and
  loading clients is not reproduced.
- Every other profile keeps three frames and its last frame, each after
  advancing the time, and still carries its bots.
- Unchanged for every profile, `wolfet` included: one console `wait` counts
  down per frame, fnTech3's own `svs.time` advances by 100 ms, and the command
  gate still moves after the restart.

## Evidence

- `tests/wolfet_restart_settle_tests.py` compiles the production block of
  `SV_MapRestart_f`, from `sv_serverRestarting 1` through the last frame, and
  runs it for all thirteen profiles:
  - Every pair of game types before and after the game's restart, 0 to 5.
  - Eight client slots: free, zombie, connected, loading, active, an active
    bot, a loading bot, and an active client the game refuses.

  It checks every call and the time each one sees. `wolfet` runs frames at
  +0 to +500 and +600, begins the active client and bot at +500 and ends at
  +700. It leaves the bots out when the restarted game's type is 0 or 1. Every
  other profile runs +100 to +300 and +400, begins at +300, ends at +400 and
  keeps its bots. For all of them, connecting and loading clients reconnect
  but do not enter, and the refused client is dropped.

  Ten mutations each fail it: three frames for ET, six for every profile, ET
  advancing before a settling frame, ET advancing before its last frame, no
  bot skip, a bot skip for every profile, a skip of people as well, co-op bots
  kept, single-player bots kept, and every reconnected client entered as in
  `et.exe`. The same fixture fails on the tested commit's block at the
  `wolfet` profile, whose first frame came at +100. It passes there for every
  profile before it.
- `rtcw_match_restart_tests.py`, `spawn_view_angle_source_tests.py`,
  `factory_runtime_source_tests.py`, `restart_variable_change_tests.py` and
  `rtcw_savegame_tests.py`, which slice, read or stub the command, pass.
  `sv_ccmds.cpp` compiles with `win32-dev`'s strict-warning client and
  dedicated flags, both in the shared tree and as the tested commit plus this
  change.
- Runtime, Windows x86. Two dedicated servers were built from a clean
  `git archive` export of `2d35919`, debugoptimized with strict warnings and
  `-Dwolfet-development=true`:
  - Baseline, the export as it is:
    `86dfb8677d398905e792c3e9ba4237ce806a4bbfd4bdf90b1ab35bdf013c05df`.
  - Candidate, the export with this change:
    `dd4a1efd38b74acb4cc45e59364f40f41af2873d7f0ef69d4656d61685dc218b`.

  `scripts/probe_wolfet_restart_settle.py` runs retail `etded.exe` and then one
  fnTech3 server on a private desktop. Each server gets an isolated home under
  `.tmp/et-settle/`, loopback only, no masters, no clients and no input. Each
  loads goldrush with `devmap` and `g_scriptDebug 1`, a cheat cvar, so the
  retail game module prints the level time of every map script event and
  command in milliseconds. Two `map_restart 0` follow, paced by loopback rcon
  as in the pending-restart probe (`2026-09-24-wolfet-pending-map-restart.md`,
  uncommitted when this was written). The probe takes each restart's lines
  from its `Game Initialization` banner, relative to the level start the spawn
  events print. It compares the first 2,500 ms with retail's.

  Retail `etded.exe` with no clients blocks for seconds between frames and
  runs the missed ones only after the next packet. A first run therefore cut
  retail's timeline short at its marker. The probe now sends an info query
  every 50 ms and waits until each level has run past the compared span.

  | Per restart | Retail `etded.exe` | fnTech3 baseline | fnTech3 candidate |
  | --- | --- | --- | --- |
  | Times with script output, first 850 ms (ms after the level start) | 0, 100, 200, 300, 400, 500, 600, 750, 850 | 0, 100, 200, 300, 400, 450, 500, 550, 600 | 0, 100, 200, 300, 400, 500, 600, 750, 850 |
  | Script lines in the first 2,500 ms | 729 | 693 | 729 |
  | SHA-256 of those lines | `2780886f…81c4` | `bc661584…edae` | `2780886f…81c4` |
  | Same as retail, first and second restart | | no, no | yes, yes |

  Retail gave the same 729 lines for all four of its restarts, over two runs.
  The candidate matched them line for line. The baseline first differs at
  line 326:
  - Retail and the candidate print goldrush's `truck_build` script's
    `wait 500` a second time at +0. That line comes from the first frame,
    which runs at the level's start.
  - The baseline's first frame came at +100.
  - That wait ends, and the script moves on, at +600 under retail and the
    candidate. That is the restart's last frame. Under the baseline it ended at
    +550, in the third frame after the restart. A script `wait 500` ends in
    the first frame more than 500 ms after it began.

  Every server logged no error, no lookup and no `variable change` reload.
  Each accepted every rcon command exactly once and ended with `gamestate` 0,
  and the installation's inventory was unchanged. The module and `pak0.pk3`
  (`712966b20e06523fe81419516500e499c86b2b4fec823856ddbd333fcb3d26e5`) were
  retail's. The commands, configs, digests and per-step results are in the
  [evidence file](evidence/2026-09-24-wolfet-restart-settle.json). The lines
  themselves quote the retail map script, so the evidence keeps only their
  digest.

## Not established

- **No client was connected.** A live client's view of the 700 ms restart,
  its snapshots and its commands across it rest on the disassembly and the
  fixture.
- **The bot skip was not exercised live.** Retail content cannot reach it.
- **Connecting and loading clients still differ, per Decision 2.** Retail
  begins every connected and loading client inside the restart, 500 ms after
  the level's start, so a loading team player spawns with the others. fnTech3
  begins such a client on its first command, after the game's 600 ms, and the
  retail game then sends it to limbo until the next reinforcement. Retail also
  begins a client that had no gamestate yet, and begins it again once its
  gamestate arrives. A retail-exact treatment needs a connection model that
  can begin a client in the game while its gamestate is still to be sent or
  acknowledged.
- **Other restart differences remain.**
  - Retail gives each restart a new `sv.serverId` and `sv_serverid` and drops
    commands from before it by that. fnTech3 keeps the `serverId` and moves a
    command-time gate. It also clears each loading or active client's last
    command after the restart, where retail keeps it.
  - Retail counts a console `wait` down once per command-buffer pass, so a
    restart uses none of it. fnTech3 counts one per game frame and now spends
    seven in an ET restart, where it spent four.
  - fnTech3's own `svs.time`, which neither the game nor clients see,
    advances 100 ms where retail's single clock advances 700.
- **The other profiles keep Quake3e's order.** Their retail engines also run
  each restart frame at the current time before advancing it, with three
  frames. Quake III 1.32 `quake3.exe`, SHA-256
  `80b0add0e014b863cf47cab6c997e0a54939236219a56910b7a3eb77dc2d44d4`, does so
  at `0x432ab1..0x432add`, with its last frame at `0x432b9e..0x432bbf`.
  `WolfMP.exe`, SHA-256
  `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5`, does so
  at `0x4330ad..0x4330d3` and `0x433192..0x4331b1`.
  `WolfSP.exe`, SHA-256
  `07d206f61da5c80562ff60d68c645bca349b31adb5d7e0e7f186d61ad46683c7`, does so
  at `0x430993..0x4309b8` and `0x430a80..0x430a9c`. Quake Live's executable was
  not read. fnTech3 still advances the time first for all of them, as this
  change was confined to `wolfet`. A later change moved every profile to the
  retail order, having read each retail engine, Quake Live's included
  (`2026-09-24-settle-frame-order.md`).
- **A map load still settles in three frames.** Retail `SV_SpawnServer` runs
  six settling frames too, each with a bot frame and each at the current
  time before advancing it: `et.exe` `0x4469e1`, final frame
  `0x446b14..0x446b3a`, and `etded.exe` `0x42d111`. fnTech3's map load still
  runs three for `wolfet`, each after advancing the time. In the probe,
  retail's goldrush load gave the same 729 lines as its restarts, and both
  fnTech3 builds gave the baseline restart's 693. This change leaves the
  load alone.
