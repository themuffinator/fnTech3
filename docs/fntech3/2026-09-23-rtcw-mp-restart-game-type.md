# RTCW MP map_restart keeps a game type the map allows

## Scope

A Return to Castle Wolfenstein multiplayer map can forbid some of the game's
own game types. Retail `WolfMP.exe` reads that from the world's entity record
each time `map_restart` gets past its first refusals, and replaces a forbidden
`g_gametype` before it restarts anything. fnTech3's `map_restart` had no such
check, so it restarted the level in the forbidden type and left the game
module to notice and restart it again. This record covers the `rtcw-mp`
profile only. Enemy Territory's `map_restart` has no such check, and RTCW
single player's world record has no such member.

## Observed

### The map's flags

The GPL game source declares the worldspawn spawn flags `NO_GT_WOLF`,
`NO_STOPWATCH` and `NO_CHECKPOINT` as bits 1, 2 and 4 (`g_local.h`, and the
`QUAKED worldspawn` comment in `g_spawn.c`). `SP_worldspawn` reads the map's
`spawnflags` key and copies it into the world entity's `spawnflags` and its
engine-visible `r.worldflags`. The game types are numbered 5 (objective),
6 (stopwatch), 7 (checkpoint) and 8 (capture and hold; `bg_public.h`).

Retail `qagame_mp_x86.dll`, SHA-256
`d8975b5c4e14488283c5eca7d549005304b04f1690705c13c174a171cc24c387`, read with
capstone:

| What | Where |
| --- | --- |
| World record | entity 1022 of the arena at `0x21ed7900`, stride `0x548`: `0x22028e70` |
| `SP_worldspawn` stores the map's flags | `0x2005a4e4` spawn flags (+`0x2b8`), `0x2005a4e9` `r.worldflags` (+`0x2a8`, `0x22029118`), `0x2005a4ee` number `0x3fe` |
| `G_RunFrame`'s own check | `0x20043757..0x200437a6`, described below |

Offset 680 is `worldflags` in fnTech3's frozen `fntech3_rtcw_mp_sharedEntity_t`:
the four bytes just past the 680 that RTCW single player's record shares with it
([contracts record](2026-09-14-rtcw-contracts.md)).

Nine of the fifteen stock MP maps set these flags. They were read from each
map's first entity, in the legitimate Steam installation's paks, read-only:

| Worldspawn `spawnflags` | Maps |
| --- | --- |
| 4 (no checkpoint) | `mp_beach`, `mp_castle`, `mp_chateau`, `mp_keep`, `mp_rocket`, `mp_tram` |
| 2 (no stopwatch) | `mp_depot` |
| 3 (no objective or stopwatch) | `mp_destruction`, `mp_trenchtoast` |
| 0 or absent | `mp_assault`, `mp_base`, `mp_dam`, `mp_ice`, `mp_sub`, `mp_village` |

No stock map forbids both objective and checkpoint.

### The engine's check

Retail `WolfMP.exe`, SHA-256
`7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5`. Its
`SV_MapRestart_f` begins at `0x432eb0`:

| Address | Behaviour |
| --- | --- |
| `0x432eb0..0x432ef0` | Return on a second restart in the same frame, on a stopped server (printing so) and on a pending delayed restart |
| `0x432ef6..0x432f0c` | `sv_gametype = Cvar_Get( "g_gametype", "5", CVAR_SERVERINFO \| CVAR_LATCH )`, then read its integer. `Cvar_Get` (`0x421ba1..0x421bb3`) applies a latched value by a forced `Cvar_Set2`, so a type typed at the console since the last restart is the one checked |
| `0x432f0f..0x432f19` | `SV_GentityNum( 0x3fe )->r.worldflags`, at +`0x2a8`. `SV_GentityNum` (`0x436530`) has no range test |
| `0x432f22..0x432f4b` | Forbidden when the type is 5 with bit 1, 6 with bit 2, or 7 or 8 with bit 4. The replacement is `"7"` when bit 1 is set, otherwise `"5"` |
| `0x432f5b..0x432f7c` | `Cvar_Set( "g_gametype", ... )`, a forced set (`0x422010` passes 1 to `Cvar_Set2`), then `Cvar_Get` again into `sv_gametype` |
| `0x432f81` | Only then parse the delay. A delayed restart has already replaced the type |
| `0x432fd0..0x433010` | The match-state transition, which may still refuse the restart |
| `0x433016..0x43301f` | Only `sv_maxclients->modified` forces a full level spawn |

A map that forbids both objective and checkpoint therefore restarts in
checkpoint anyway. The engine sets it even when it is the type already in
force.

### What the game module does without it

Retail `G_RunFrame` (`0x20043757..0x200437a6`) applies the same rule from the
world's own spawn flags, but behind two more tests, which the GPL source names
`GS_PLAYING` and `level.latchGametype`: only while the match is playing, and
only once per level. It broadcasts `Invalid gametype was specified,
Restarting` and queues `wait 2 ; g_gametype 5|7 ; map_restart 10 0`. A
server that restarts into a forbidden type therefore starts playing in it,
and the game restarts the level again ten seconds later.

### Enemy Territory

ET 2.60b `et.exe`, SHA-256
`4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e`, begins
`SV_MapRestart_f` at `0x43fa20`. It returns on a same-frame restart and on a
stopped server, then goes straight to the delay (`0x43fa53`): no world read
and no pending-restart test. ET's GPL `sv_ccmds.c` agrees. ET's record carries
a `worldflags` member too, but its `map_restart` never reads it.

### fnTech3 before this change

`SV_MapRestart_f` had no check. The world record lies past the entity count the
module publishes. It became reachable only when `SV_GameEntityArena` started
binding RTCW MP's whole 1024-record arena (`2026-09-23-world-owned-explosives.md`,
uncommitted when this was written), and `SV_ReadGameEntity` does not copy
`worldflags` into the engine's entity.

## Change

- `rtcw_game_abi.h` names RTCW MP's game types 5 to 8 and the three world flags.
- `fntech3::rtcw::MapForbidsGameType` (`rtcw_match_state.hpp`) states the rule
  above: whether a type is forbidden, and its replacement.
- `ServerGameDataView::GetRTCWMPWorldflags` reads the four bytes at 680 of an
  RTCW MP record and refuses every other profile, including records long
  enough to reach that offset. `SV_RTCWMPWorldflags` (`sv_game.cpp`) reads the
  world record's flags through it. If the record cannot be read, it drops the
  server with an error naming the entity, rather than skip a check that retail
  always makes.
- `SV_MapRestart_f`, for `rtcw-mp` only, gets `g_gametype` as retail does, and
  forces the replacement with `Cvar_SetIntegerValue`, then gets the cvar again.
  This happens after the same-frame, stopped-server and pending-restart
  refusals, and before the delay and the match-state transition.
- Every other profile's `map_restart` is unchanged. The `map` command still
  loads a map in any type, as retail's does. The game module's own correction
  still runs.

## Evidence

- `tests/rtcw_match_restart_tests.py` compiles the production
  `SV_MapRestart_f` up to its level restart, with stubs that count world
  reads, cvar gets and forced sets. Types -1 to 9 run against flags 0 to 9,
  12, 15, -8 and -1, each with the type set or latched, for an immediate and a
  three-second restart. Types 5 to 8 must end as a literal table transcribed
  from the retail branches, with exactly one world read, one or two gets, and a
  forced set exactly when the type is forbidden. Other types are never
  replaced. A replacement stands when the match state then refuses the
  restart. The three earlier refusals read nothing, and Quake III, Quake Live,
  RTCW SP and ET never read the world or touch `g_gametype`. Seven mutations of
  the command each fail: no check, the check after the delay, two flag bits, no
  forced set, no second get, both RTCW modes, and no latched value.
- `tests/server_game_data_tests.cpp` writes flags 3 at +`0x2a8` of record 1022
  in a retail-stride RTCW MP arena and reads 3 back without touching module
  memory. It refuses an expired reference, and every other profile with a
  plain binding: six at the time of writing, nine in a tree with the Jedi
  Academy, Jedi Outcast and Alice profiles. Without the profile test, the view
  read those three longer records; at 680 bytes, RTCW SP's record ends before
  the member. Reading the neighbouring `r.eventTime` instead fails too.
- Runtime, Windows x86 `meson/build/win32-dev` (`-Drtcw-development=true`),
  tree at `f65471e` plus this change and other sessions' uncommitted work,
  including the arena binding. `scripts/probe_rtcw_restart_game_type.py` runs
  one dedicated server on a private desktop: isolated home under
  `.tmp/rtcw-mp-restart-gametype/`, loopback only, no masters, no clients, no
  input. It execs this sequence from a config file and prints `g_gametype` and
  `gamestate` after each step:
  `g_gametype 5`, `map mp_destruction`; `map_restart`; `g_gametype 6`,
  `map_restart`; `map mp_beach`; `map_restart`; `g_gametype 6`, `map_restart`.
  Both maps come from `mp_pak0.pk3`, SHA-256
  `c0e090e7b4245313bea64b5fb20b24d69786e593716cfe6bf85ee9b4f3c794b3`. The
  installation's inventory was unchanged after every run.

  | Step | Retail `WolfMP.exe` | fnTech3, previous build | fnTech3, this change |
  | --- | --- | --- | --- |
  | `mp_destruction` loaded in objective | 5 | 5 | 5 |
  | `map_restart` | 7 | 5 | 7 |
  | `g_gametype 6`, `map_restart` | 7 | 6 | 7 |
  | `mp_beach` loaded | 7 | 6 | 7 |
  | `map_restart` | 5 | 6 | 5 |
  | `g_gametype 6`, `map_restart` | 6 | 6 | 6 |
  | `Invalid gametype` broadcasts | 0 | 2 | 0 |
  | `variable change -- restarting` | 0 | 1 | 4 |

  The servers were retail `WolfMP.exe` as above, the previous
  `fntech3.ded.exe`
  `651d9595eb8ab970f4c32555ef714da58608cd179eb7a913f313c2867c81e9b4`, and this
  change's `12bf0310465e1b98c70e1ecb4d975ab6ce52be9082c515dd2c32a316972a10dc`,
  all with the retail module above. `gamestate` read 1 after each load and 0
  after each restart in all three. The fixed build and retail also broadcast
  the same time-limit changes, 10 on `mp_destruction` and 8 on `mp_beach`.

## Not established

- **How the level restarts is not this change's.** Retail restarts in place:
  only `sv_maxclients` forces a spawn (`0x433016`), and its log shows a game
  initialization but no server initialization. When these runs were made,
  fnTech3's shared check also spawned the level after a `sv_gametype` or
  `sv_pure` change, for every profile but Quake Live, so each replacement
  above reloaded the map (the `variable change -- restarting` row), and
  connected clients would have received a new gamestate instead of a
  `map_restart` command. `2026-09-24-wolfenstein-restart-variable-change.md`
  changes that check for RTCW MP and ET; this change leaves it alone.
- No client was connected in any run, so what players see across a replaced
  restart is not compared with retail.
- A map that forbids both objective and checkpoint is inferred to restart in
  checkpoint, which the game then restarts again every ten seconds of play.
  No stock map does this, and it was not run.
- iortcw's `map_restart` was not compared.
