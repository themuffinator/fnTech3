# Enemy Territory map_restart runs while a delayed restart is pending

## Scope

`map_restart <seconds>` arms a delayed restart: the server stores the time it
is due and publishes it in `CS_WARMUP`. fnTech3's `SV_MapRestart_f` then
refused every further `map_restart` until that restart fired, for every
profile, as Quake III's GPL source and retail `WolfMP.exe` do. Retail Enemy
Territory never refuses on that account. This record covers the
`wolfet` profile only. Every other profile keeps the refusal.

## Observed

### et.exe and etded.exe

ET 2.60b `et.exe`, SHA-256
`4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e`, read with
capstone. Its `SV_MapRestart_f` runs from `0x43fa20` to `0x43fd83`:

| Address | Behaviour |
| --- | --- |
| `0x43fa23..0x43fa31` | Return when `com_frameTime` (`0x121c7b8`) equals `sv.serverId` (`0x1184ea8`): a second restart in the same frame |
| `0x43fa37..0x43fa52` | Return on a stopped server, `com_sv_running->integer` at +`0x20`, printing `Server is not running.` |
| `0x43fa53..0x43fa6f` | Straight to `Cmd_Argc` and `atoi( Cmd_Argv( 1 ) )`. Nothing between the running test and here reads `sv.restartTime`. No argument, or 0, goes to the immediate restart at `0x43fa9c` |
| `0x43fa71..0x43fa83` | `sv.restartTime` (`0x11e72dc`) `= svs.time` (`0x11ef5c4`) `+ delay * 1000`, a plain store over whatever was pending |
| `0x43fa88..0x43fa9b` | `SV_SetConfigstring( 5, va( "%i", sv.restartTime ) )`, index 5 being `CS_WARMUP`, then return |
| `0x43fa9c..0x43fd83` | The immediate restart. It never writes `sv.restartTime` |

`SV_Frame` (`0x447da0`) loads `svs.time` into `ecx` at `0x447e81`, and at
`0x447f16..0x447f3f` does `if ( sv.restartTime && svs.time >= sv.restartTime )
{ sv.restartTime = 0; Cbuf_AddText( "map_restart 0\n" ); return; }`, a
signed comparison. A scan of the code for the absolute address finds it only
at `0x43fa83`, `0x447f16` and `0x447f28`.

So, while a delayed restart is pending:

- `map_restart N`, for any non-zero `N`, replaces the pending time with
  `svs.time + N * 1000`, earlier or later than the old one, and rewrites
  `CS_WARMUP` with the new time.
- `map_restart` or `map_restart 0` restarts the level at once and leaves the
  pending time, and `CS_WARMUP`, alone. The pending restart still fires when
  it falls due, restarting the level a second time.

The dedicated `etded.exe`, SHA-256
`7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf`, has the
same command at `0x425e70`: the same-frame test at `0x425e85`, the running
test at `0x425e96`, then `Cmd_Argc` at `0x425eb7`, the store to
`sv.restartTime` (`0xc5dc1c`) from `svs.time` (`0xc65f04`) at `0x425ee7`, and
`CS_WARMUP` at `0x425ef4`. Its `SV_Frame` fires the restart at `0x42e748`.
Both executables carry the version string `ET 2.60b`.

ET's GPL `sv_ccmds.c` has the test commented out, with the note "allow
multiple delayed server restarts [atvi bug 3813]".

### WolfMP.exe keeps the refusal

RTCW MP `WolfMP.exe`, SHA-256
`7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5`, returns at
`0x432eea` when the dword at `0xb5543c` is non-zero. Its delay path stores
`svs.time + delay * 1000` to that address at `0x432fb4` and publishes it in
`CS_WARMUP` at `0x432fc1`, so this is its pending-restart refusal. The
branch goes to the function's epilogue at `0x4331c8`. The RTCW MP game-type
record, `2026-09-23-rtcw-mp-restart-game-type.md` (uncommitted when this was
written), places it among retail's first refusals.

### fnTech3 before this change

`SV_MapRestart_f` returned whenever `sv.restartTime` was non-zero, whatever the
profile. On an ET server with a delayed restart pending, `map_restart` did
nothing, neither restarting at once nor replacing the delay.

Further down, the immediate restart gives each `CS_PRIMED` client
`oldServerTime = sv.restartTime`, as Quake III does. Snapshots send such a
client `sv.time + oldServerTime` until it enters the world. The refusal kept
`sv.restartTime` zero there, so every profile stored zero. `et.exe`'s restart
has no such step, and ET's GPL server has no `oldServerTime` at all.

## Change

- `SV_MapRestart_f` (`sv_ccmds.cpp`) keeps the pending-restart refusal for
  every profile except `wolfet`. ET's command goes on to the delay it already
  parsed: a non-zero delay takes the existing path, which stores the new time
  and publishes it in `CS_WARMUP`; no delay restarts the level and leaves the
  pending time to fire.
- The primed-client step now stores zero outright. That is what every profile
  stored before. It keeps an ET restart that runs with a delay still pending
  from sending a loading client a time near twice the level time.
- The same-frame and stopped-server refusals, the match-state transitions and
  every other profile's `map_restart` are unchanged.

## Evidence

- `tests/rtcw_match_restart_tests.py` compiles the production
  `SV_MapRestart_f` up to its level restart, as before. For Quake III, Quake
  Live, RTCW SP, RTCW MP and ET, with a restart pending at 20000, `map_restart`
  restarts only ET, which moves from countdown to playing and keeps the
  pending time, while the others do nothing. `map_restart 3`, at level time
  10000 with a restart pending at 12000 or 20000, re-arms only ET, to 13000,
  and only ET writes `CS_WARMUP` `"13000"`. The fixture also compiles the
  primed-client loop sliced from the command. With a restart pending, a primed
  client's offset becomes zero and the other client states are untouched.
  Nine mutations of the command each fail an assertion: no ET exemption, no
  refusal for any profile, RTCW MP exempted instead of ET, ET clearing the
  pending restart, a re-arm keeping the pending time, a re-arm keeping the
  earlier time, a re-arm leaving `CS_WARMUP`, primed clients taking the pending
  time, and primed clients skipped while a restart is pending.
- The fixture passes on the tested commit plus this change and on the shared
  tree. `factory_runtime_source_tests.py`, `spawn_view_angle_source_tests.py`
  and `rtcw_savegame_tests.py`, which also read or stub the command, pass.
  `sv_ccmds.cpp` compiles with `win32-dev`'s strict-warning client and
  dedicated flags.
- Runtime, Windows x86. Two dedicated servers were built from a clean
  `git archive` export of `1074fe3`, one as it is (baseline) and one with this
  change (candidate), debugoptimized with strict warnings and
  `-Dwolfet-development=true`. `scripts/probe_wolfet_pending_restart.py` runs
  one server at a time on a private desktop: an isolated home under
  `.tmp/et-pending-restart/`, loopback only, no masters, no clients, no input,
  `g_gametype 2` on `goldrush`. A delayed restart appends its
  `map_restart 0` to the end of the command buffer, so one exec'd script would
  hold it back until the script ended. The probe therefore runs each step,
  and a marker after it, as its own config through a loopback `rcon exec`,
  paced in real time. It counts the retail game module's `Game
  Initialization` banners between markers. `etded.exe` returns from its rcon
  handler, before logging or replying, when `Com_Milliseconds()` is within
  500 ms of the last command it accepted (`0x42dc5a..0x42dc73`). A first
  retail run lost its one-hour delay that way: the command before it was
  handled late, after the map load. The probe now spaces commands a second
  apart, resends until the reply shows the config ran, and requires each
  server's log to have accepted every command exactly once.

  | Step | Retail `etded.exe` | fnTech3 baseline | fnTech3 candidate |
  | --- | --- | --- | --- |
  | `goldrush` loaded | 1 | 1 | 1 |
  | `map_restart 3600`, wait 1.5 s | 0 | 0 | 0 |
  | `map_restart 0` while it is pending, wait 1.5 s | 1 | 0 | 1 |
  | `map_restart 2` while it is pending, wait 4.5 s | 1 | 0 | 1 |
  | `map_restart 2`, `map_restart 0` five frames later, wait 4.5 s | 2 | 0 | 2 |
  | `gamestate` after each step | 1, 1, 0, 0, 0 | 1, 1, 1, 1, 1 | 1, 1, 0, 0, 0 |

  The baseline refused every restart once the hour's delay was pending. The
  candidate made the same restarts as retail and left the same match state.
  No server logged a lookup, an error or a `variable change` reload, and the
  installation's inventory was unchanged after every run. The servers were
  retail `etded.exe` as above, baseline
  `3765ce1e2b870a5a0edae6c98291020aaa318a206f3dea158eb1d85c3e1fe5b1` and
  candidate `4e2dcb736eeb545070b2092eb7ac8d3eb917675635e9f08a8117323e04754fcb`,
  all with the retail module, `qagame_mp_x86.dll`
  `160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b`, and
  `pak0.pk3`
  `712966b20e06523fe81419516500e499c86b2b4fec823856ddbd333fcb3d26e5`. The
  commands, configs and per-step results are in the
  [evidence file](2026-09-24-wolfet-pending-map-restart.json).

## Not established

- No client was connected. The `CS_WARMUP` value after a re-arm, and the
  countdown ET's cgame draws from it, rest on the disassembly and the fixture,
  not on a client's view.
- fnTech3's delay path turns a restart time of exactly zero into 1, for every
  profile; Quake3e does the same. None of `et.exe`, `etded.exe` and
  `WolfMP.exe` does. Reaching zero takes a negative delay, such as
  `map_restart -10` at level time 10000. Retail ET then stores zero, which
  cancels any pending restart and publishes `CS_WARMUP` `"0"`, where fnTech3
  restarts on the next frame. This is unchanged.
- `et.exe`'s immediate restart differs from fnTech3's in ways this change
  leaves alone. It runs six settling frames (`0x43fc3b`), each at the current
  time before advancing it, where fnTech3 runs three and advances first.
  It makes every connected client active and enters it into the world with
  its last command (`0x43fd1f..0x43fd2d`), where fnTech3 enters only clients
  already active. In single-player and co-op game types it skips bots
  (`0x43fca1..0x43fcbe`).
- The RTCW MP refusal was confirmed at `0x432eea` only. The other profiles'
  retail refusals were not reread for this change.
