# The level clock across a map change, 2026-10-15

The [Jedi broadcast and level-time slice](2026-10-15-jedi-broadcast-level-time.md)
found that retail `jamp.exe` and `jk2mp.exe` never restart their level
clock at a map change, where fnTech3 restarted an empty server's at 0, and
gave those two profiles retail's rule. This slice reads and runs the other
Quake III-line multiplayer engines and settles each profile:

- `q3`, `rtcw-sp`, `rtcw-mp`, `wolfet` and `stvef-mp` keep their clock as
  the Jedi engines do. Each now has `FRAME_KEEP_LEVEL_TIME`.
- `ql` is the opposite. Retail Quake Live restarts its level clock at 0 at
  every map change, even with players connected. The inherited rule kept a
  connected player's time, so `ql` gets a rule of its own,
  `FRAME_RESET_LEVEL_TIME`.
- Every other profile keeps the inherited rule, for the reasons given
  [below](#profiles-left-on-the-inherited-rule).

The [JSON companion](2026-10-15-level-clock.json) records the binaries,
identities, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. The retail executables were read
with capstone and pefile. The id GPL sources pinned under `E:\_SOURCE\_CODE`
(Quake III Arena, RTCW SP and MP, Enemy Territory) and Quake3e were used
only to name routines and fields (reference observation; no text
incorporated).

## The retail engines

### Observed: one clock that only a shutdown clears

In each engine below the game runs on the server's own clock, `svs.time`.
`GAME_INIT` is handed that clock, and so are the spawn's settling frames,
`map_restart`'s frames, `SV_Frame`'s game frames and every snapshot. Each
spawn clears a per-level block that ends before the clock, and only
`SV_Shutdown`'s clear of the server's static block covers it.

| Use | `quake3.exe` 1.32 | `WolfSP.exe` (Wolf 1.32) | `WolfMP.exe` (Wolf 1.4-MP) | `etded.exe` 2.60b | `stvoyHM.exe` 1.20 |
| --- | --- | --- | --- | --- | --- |
| clock | `0x96d184` | `0xb99e64` | `0xae8fa4` | `0xc65f04` | `0x6b5044` |
| `GAME_INIT` | `0x4385dd` (spawn), `0x438514` | `0x435fd5` | `0x4389d7` | `0x42bcb8` | `0x433268`, `0x433345` |
| settling frames, +100 each | `0x438d92..0x438dc5`, then `0x438f02..0x438f31` | `0x436fa4..0x436fd7`, `0x437103..0x43712f` | `0x43945b..0x43948e`, `0x4395b1..0x4395e2` | six at `0x42d10c..0x42d142`, then `0x42d252..0x42d26a` | `0x433aa2..0x433ad5`, `0x433c32..0x433c60` |
| `map_restart`'s frames | `0x432ab6..0x432ad7`, `0x432b9e..0x432bbf` | `0x430998..0x4309b9`, `0x430a80..0x430aa1` | `0x4330b2..0x4330d4`, `0x433192..0x4331b9` | `0x4260b7..0x4260dc`, `0x4261d0..0x4261e5` | `0x42e79b..0x42e7bc`, `0x42e85f..0x42e880` |
| game frames, `+= frameMsec` | `0x43ac18..0x43ac23` | `0x438829..0x438834` | `0x43b838..0x43b843` | `0x42e867..0x42e877` | `0x4353a4..0x4353ac` |
| snapshot server time | `0x43b452`, `0x43bdf7` | `0x438c58` | `0x43bf10` | `0x4303e6` | `0x43583a`, `0x436134` |
| spawn's clear | `[0x9771e0, 0x9c3620)` at `0x438c9a` | `[0xba2ec0, 0xc042fc)` at `0x436c8c` | `[0xaf4000, 0xb554fc)` at `0x4391cc` | `[0xbfb7e0, 0xc65ee4)` at `0x42c3cc` | `[0x6be0a0, 0x7094dc)` at `0x433986` |
| shutdown's clear | `[0x96d180, 0x9771c8)` at `0x439577` | `[0xb99e60, 0xba2ea8)` at `0x437701` | `[0xae8fa0, 0xaf3fe8)` at `0x439de1` | `[0xc65f00, 0xc71558)` at `0x42cee6` | `[0x6b5040, 0x6be088)` at `0x434265` |

Every store into each clock was listed. Besides those in the table there are
only the save-time restores below. Every `memset` call and `rep stosd` with
a fixed base within 4 MiB of each clock was listed too, and only the
shutdown's clear covers one. No snapshot adds an offset for a client still
loading. The `et.exe` client engine has the same layout: clock
`0x11ef5c4`, `GAME_INIT` `0x445598`, six settling frames from `0x4469e1`,
spawn clear `[0x1184ea0, 0x11ef5a4)`, shutdown clear from `0x11ef5c0`.

`WolfSP.exe` sets the clock from a save's header when a save is loaded, in
`map` (`0x430de1`) and in `map_restart` (`0x430936`), and only when the
saved time is not negative. `etded.exe` keeps the same code from RTCW
(`0x4264c3`, `0x426057`).

### Observed: Quake Live restarts it

`quakelive_steam.exe` has two clocks. Its server clock, `svs.time`
(`0x13337a4`), is cleared only by its shutdown (`[0x13337a0, 0x13e17e0)`,
`0x4e3ffa`). The game runs on a clock inside the per-level block, `sv.time`
(`0x1333780`), its last field:

- `GAME_INIT` (export slot `+0xc`) is handed `sv.time` (`0x4e2b77`).
- The settling frames advance both clocks (`0x4e3770..0x4e37a1`), as do
  `map_restart`'s frames (`0x4de8c0..0x4de8dc`) and `SV_Frame`'s game frames
  (`0x4e4c70..0x4e4c83`).
- The spawn first gives every client in state 2 (connected) or above the old
  `sv.time` as a clock offset (`+0x25b58`, `0x4e35f0..0x4e3618`). It then
  clears the per-level block `[0x12e0340, 0x1333784)`, the clock included
  (`0x4e363a..0x4e3646`). It does this whoever is connected.
- A snapshot carries `sv.time` plus that offset while the offset is set
  (`0x4e5178..0x4e5192`). The offset is cleared when the client acknowledges
  the new gamestate (`0x4e06ab`).
- `map_restart` (`0x4de670`) returns while a restart is pending
  (`0x4de6bb`). Otherwise it gives every client in state 2 or above the
  pending restart time as its offset, which is then always 0
  (`0x4de869..0x4de89b`).

There is no `sv_levelTimeReset` in Quake Live. Its frame has an idle restart
at 24 hours and a stop past `0x70000000`, both on `svs.time`
(`0x4e4a7e`, `0x4e4ac2`). It has no 12-hour level restart.

No retail Quake Live server runs on this machine (the installation has
only the client), so Quake Live has static evidence only.

### Measured

A synthetic game module (`.tmp/level-clock/probe/level-clock-probe.c`, one
source built per engine with that engine's import numbers; not committed)
prints the time each stage's `GAME_INIT` is handed, the first five game
frames' times, frame 130's, and the last frame's time at `GAME_SHUTDOWN`.
At frame 130 it issues the next stage's command: `map` of the same map,
then `map_restart 0`, then `killserver` and `map`, then `quit`. Each frame
adds a fixed amount, so the numbers do not depend on the wall clock.

Each server ran headless on a private desktop with no input, an isolated
home, no master servers, `bot_enable 0` and `sv_pure 0`, and left its
installation unchanged:

- `q3dm1` on `quake3.exe`, the module in a scratch `fs_cdpath`.
- `escape1` on `WolfSP.exe`, under cdb. A breakpoint
  (`bp 0x441cb4 "r eax=0; gc"`) refuses the installation's own library, so
  the module loads from a scratch `fs_cdpath`.
- `mp_beach` on `WolfMP.exe` and `oasis` on `etded.exe`, each from a
  scratch `fs_basepath` with the installation as `fs_cdpath`. ET loads a game
  library only from its base path (`Sys_LoadDll`), and an idle `etded.exe`
  was sent a `getinfo` every 50 ms so that it ran its frames.
- `hm_borg1` on a byte-identical copy of `stvoyHM.exe` reading a copy of the
  four retail paks, as every Elite Force probe does, with `developer 0`.
  With `developer 1` the retail server overflowed its stack inside its
  console window's text append (`0x443a57`).

The fnTech3 runtimes were a dedicated server built from an export of
`b80e6f6e` ("before") and the same export with this slice's three engine
files ("after").

| Game | Stage | Retail | fnTech3 before | fnTech3 after |
| --- | --- | --- | --- | --- |
| `q3`, `rtcw-sp`, `rtcw-mp`, `stvef-mp` | first level | 0 | 0 | 0 |
| | after `map` | 6700 | **0** | 6700 |
| | after `map_restart 0` | 13400 | 6700 | 13400 |
| | after `killserver`, `map` | 0 | 0 | 0 |
| `wolfet` | first level | 0 | 0 | 0 |
| | after `map` | 6850 | **0** | 6850 |
| | after `map_restart 0` | 13700 | 6850 | 13700 |
| | after `killserver`, `map` | 0 | 0 | 0 |

Each `GAME_INIT` after the first was handed the time of the level's last
frame. The game frames ran 0, 100, 200 (settling), 300 (the frame after the
clients are sent their gamestate), then every 50 ms. ET settles in six
frames, so its frame 130 is at 6850. For all five games, all 33 markers of
the after run equal retail's, every settling frame and shutdown included.
Before differed at each map change.

RTCW MP's `map_restart` reads entity 1022's `worldflags` (`0x432f19`), so a
module that binds fewer entities faults inside retail `WolfMP.exe`. The
module binds 1,024.

## Implemented

- `FRAME_KEEP_LEVEL_TIME` (`code/qcommon/profile_frame_time.h`) now marks
  `q3`, `rtcw-sp`, `rtcw-mp`, `wolfet` and `stvef-mp` as well as `jka-mp` and
  `jk2-mp`. For them `SV_SpawnServer` skips the empty-server reset, and
  `sv_levelTimeReset` neither clears the clock nor gives connected clients an
  offset, as the Jedi slice set up.
- `FRAME_RESET_LEVEL_TIME` marks `ql`. `SV_LevelTimeResets()`
  (`code/server/sv_init.cpp`) is true for it whatever `sv_levelTimeReset`
  says. Each map change gives every connected client the old level time and
  clears the clock, as `quakelive_steam.exe` does. The inherited code
  already served the offset in snapshots and cleared it on acknowledgement.
- Quake Live's `map_restart` clears that offset for every client in the
  connected state or above, not only primed ones
  (`code/server/sv_ccmds.cpp`). Without it a client still loading the new map
  would carry the old level's time across the restart.
- Unchanged:
  - RTCW SP's save loading still sets the level time from the save after the
    clear (`SV_RTCWRestoreSaveTime`), as `WolfSP.exe` does before its spawn.
  - Enemy Territory still runs six settling frames.
  - Holomatch's server clock still advances only with ticks.
  - `SV_Frame`'s silent restart of an empty server after twelve hours of
    level time still applies to the profiles that keep their clock, whatever
    `sv_levelTimeReset` says. It is an inherited FnQL safeguard; retail stops
    at `0x70000000` (`quake3.exe` `0x43ab0f`).

## Profiles left on the inherited rule

- `q3-ihv` and `q3test` start every level at 1000 whatever the rule
  (`SV_SpawnServer`, after the IHV Test and every Q3Test release).
- `stvef-sp` starts every level at 1000, as `stvoy.exe` 1.20 does after
  clearing its clock in its spawn (`0x425a9b`, `0x425ac9`).
- `alice` and `fakk2` restart their level clock with every level, at 0 or at
  a save's time, through their own path (`Com_GameIsRitual()` in the same
  block; see the [Alice savegame record](2026-09-23-alice-savegames.md)).
- `jka-sp` and `jk2-sp`: `jasp.exe` and `jk2sp.exe` clear their clock in
  their spawn (`0x439d0d`, `0x437226`), where the inherited rule keeps a
  connected player's time. These two profiles were not examined here and
  stay open.
- `mohaa`: its SafeDisc engine cannot be read. Nothing establishes its
  clock.

## Tests

- `tests/jedi_level_time_tests.py` pins all seventeen profile ids. For each
  it checks that the other clock rules are unchanged, which profiles keep
  the clock (seven) and which reset it (`ql`). It compiles the production
  spawn block with `SV_ClearServer` and runs every profile with
  `sv_levelTimeReset` on and off, a client connected or not, and a restart
  pending or not. Quake Live clears the clock and gives a connected client
  the old time in every case.
- `tests/rtcw_match_restart_tests.py` runs the production `map_restart`
  loop: other profiles clear only a primed client's offset, Quake Live
  every connected client's.

Eight mutations, each applied to an export holding this slice and then
reverted, were run against those two files:

- Caught: the keep bit dropped for `q3`, for both RTCW modes, for `wolfet`
  or for `stvef-mp`; the reset bit dropped for `ql`; `SV_LevelTimeResets()`
  ignoring it; Quake Live's `map_restart` clearing only primed clients.
- Not caught: the empty-server test reading the cvar instead of
  `SV_LevelTimeResets()`. That mutant is equivalent: for Quake Live the
  clear zeroes the clock either way, after the offset has been taken from
  it.

## Validation

- An export of `f5381f0e` with this slice's files built all 1,545 targets
  under strict warnings.
- Its meson tests, without the Python suite: 273 passed, 2 environment
  skips, 1 timeout under load that then passed alone.
- Its Python suite: 2,774 passed and 132 skipped. Four failures were not
  this slice's:
  - two test files that do not collect at that commit itself (they import a
    helper only an uncommitted file defines);
  - a renderer fixture that does not compile at that commit;
  - a temporary-directory cleanup error that passed alone.
- On an export of the commit's parent with this slice's files, the
  dedicated server built and the level-time, restart, server-cvar,
  changelog, documentation, manual and packaging tests passed.

The [JSON companion](2026-10-15-level-clock.json) (`validation`) has the
detail. This slice's engine files are the same in `b80e6f6e` (the
runtimes), `f5381f0e` and the parent.

## Remaining and unrelated findings

- **Jedi single player.** `jka-sp` and `jk2-sp` keep the inherited rule,
  while their retail engines clear the clock per spawn (above). Open.
- **Quake Live's delayed `map_restart`.** Retail counts a delayed restart on
  `svs.time` (`0x4de71b..0x4de734`, fired at `0x4e4b01..0x4e4b13`) and
  writes configstring 5 as an info string with `time` and `g_gametype`
  (`0x4de727..0x4de7a8`). fnTech3 counts it on `sv.time` and writes a bare
  number. Not examined further.
- **Holomatch's server clock.** Retail's one clock takes the three settling
  frames of a spawn or a `map_restart`. fnTech3's tick-only `svs.time` takes
  only the frame after them (`code/server/sv_init.cpp`,
  `code/server/sv_ccmds.cpp`), so its snapshot deadlines and timeouts fall
  300 ms further behind the level clock at each level change. The level
  clock itself matches.
- **Enemy Territory saves.** `etded.exe` restores its clock from a save's
  header (above). fnTech3 serves save loading only for `rtcw-sp`.
- **Retail Holomatch console.** A `developer 1` dedicated `stvoyHM.exe`
  overflowed its stack in its console window on this machine (harness note).
