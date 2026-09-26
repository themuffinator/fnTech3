# Every retail engine settles a level at the time its game began

## Scope

A map load and an immediate `map_restart` start the game module, run a few
frames to let the level settle, bring the clients back, and run one more frame.
fnTech3 inherited Quake3e's order for those frames: add 100 ms to the level
time, then run the frame. The Enemy Territory profile had already moved to
retail ET's order (`2026-09-24-wolfet-restart-settle.md`,
`2026-09-24-wolfet-map-load-settle.md`, both uncommitted when this was
written). Every other retail engine read here uses that order as well: run the
frame at the current time, and only then add 100. So a game's first frame runs
at the time `GAME_INIT` was given, and a three-frame settle runs at *T*, *T*+100
and *T*+200, begins the clients at *T*+200 and runs the last frame at *T*+300.
Quake3e ran them at *T*+100 to *T*+300, began the clients at *T*+300 and ran the
last frame at *T*+400. Both leave the level at *T*+400.

This record checks the order in each profile's retail engine, decides it per
profile, and moves every profile that runs these frames to the retail order.
The frame count, ET's six, and every other part of both commands are unchanged.

## Observed

### The retail engines

Each executable was read with capstone. The settling loop runs from the loop
counter's `mov esi, 3` to its `jne`, and the last frame from its read of the
clock to the store after it adds 100. Every loop hands the game the clock,
calls `GAME_RUN_FRAME` (8) or the export table's `RunFrame`, and adds 100 to the
same clock afterwards. Where the engine has bots, the bot frame follows the
game frame at the same time.

| Executable | SHA-256 | `map_restart` frames, last frame | Map load frames, last frame | `GAME_INIT` given |
| --- | --- | --- | --- | --- |
| Quake III 1.32 `quake3.exe` | `80b0add0…dc2d44d4` | `0x432ab1..0x432add`, `0x432b9e..0x432bbf` | `0x438d8d..0x438dc5`, `0x438f02..0x438f31` | `svs.time` (`0x96d184`), `0x438514` |
| Quake Live `quakelive_steam.exe` | `c926fe9f…5ded1299` | `0x4de8ae..0x4de8e7`, `0x4de9f2..0x4dea0e` | `0x4e3766..0x4e37a1`, `0x4e388b..0x4e38b4` | `sv.time` (`0x1333780`), `0x4e2b82` |
| RTCW SP 1.32 `WolfSP.exe` | `07d206f6…d46683c7` | `0x430993..0x4309bf`, `0x430a80..0x430aa1` | `0x436f98..0x436fd7`, `0x437103..0x43712f` | `svs.time` (`0xb99e64`), `0x435fd5` |
| RTCW MP 1.4 `WolfMP.exe` | `7cf99a93…91e321f5` | `0x4330ad..0x4330da`, `0x433192..0x4331b9` | `0x439453..0x43948e`, `0x4395b1..0x4395e2` | `svs.time` (`0xae8fa4`), `0x4389d7` |
| Holomatch 1.20 `stvoyHM.exe` | `5b5b6e82…86487dff` | `0x42e796..0x42e7c2`, `0x42e85f..0x42e880` | `0x433a9d..0x433ad5`, `0x433c32..0x433c60` | `svs.time` (`0x6b5044`), `0x433268` |
| Elite Force SP 1.20 `stvoy.exe` | `8f7ccadb…da0dff74` | none | `0x425b43..0x425b6e`, `0x425be2..0x425bfb` | `sv.time` (`0xa41c3c`), `0x4256d3` |
| Jedi Academy MP 1.01 `jamp.exe` | `e8062d39…ddfee28b` | `0x456396..0x4563c6`, `0x4564ae..0x4564cf` | `0x45eaee..0x45eb62`, `0x45eca7..0x45eceb` | `0xc317c4`, `0x45dd5f` |
| Jedi Academy SP 1.01 `jasp.exe` | `9351a8de…2566d6ce` | none | `0x439dbd..0x439de6`, `0x439e69..0x439e83` | `sv.time` (`0xc2a19c`), `0x4398e4` |
| Jedi Outcast MP 1.02a `jk2mp.exe` | `2f08dcf9…b7a1577a` | `0x447c03..0x447c2f`, `0x447cf2..0x447d14` | `0x44da5c..0x44da94`, `0x44db99..0x44dbc8` | `0xb0d674`, `0x44cf65` |
| Jedi Outcast SP 1.02a `jk2sp.exe` | `0d3c635e…cc0d0e0c` | none | `0x4372d8..0x437308`, `0x43736c..0x437386` | `sv.time` (`0x126bb24`), `0x436da2` |

The full digests are in the
[evidence file](2026-09-24-settle-frame-order.json). "None" means the
executable holds no `map_restart` string at all. Enemy Territory 2.60b's
`et.exe` and `etded.exe` run six frames the same way, as the ET records show.
The IHV test's frames already ran at 1000, 1100 and 1200 (`Q3_IHV.md`), and
Alice's own settle runs at 0, 100 and 200.

Quake Live adds 100 to both of its clocks after each frame: `sv.time`, which
the game sees, and `svs.time` (`0x13337a4`), from which a delayed restart takes
its time (`0x4de71b..0x4de734`).

### What the order changes

The level ends 400 ms on either way, so the difference is the time of each
frame the game runs:

- The game's first frame runs at its own start time, with no time elapsed.
- A player the restart brings back is begun 200 ms after the level's start,
  not 300. In Quake III's GPL game source, for one, `ClientSpawn` dates the
  player's command time and respawn time from that frame.
- The game runs a frame at the level's start and none 400 ms after it. The
  first ordinary frame follows at the level's start plus 400 plus one frame, in
  both orders.

RTCW's map scripts show it. Both RTCW games register `g_scriptDebug` without
the cheat flag (GPL `g_script.c`), and the retail modules then print the level
time of each script command they reach, with no cheats enabled, as the runs
below show. A spawn event's script starts while `GAME_INIT` spawns the level,
so its first command prints at the level's start. A frame prints a script's
current command again when that command began at the frame's own time. Under
retail, the first frame runs at the level's start, so every spawn script
stopped at a `wait` prints its current command a second time at +0. Under the
previous build the first frame came at +100, and those lines never printed.

### The RTCW SP save and bot-frame interactions

- Retail restores a save's time before `GAME_INIT` on both of its paths:
  `SV_MapRestart_f` sets `svs.time` from `save/current.svg` when
  `savegame_loading` is set (`0x4308d3..0x430941`, the store at `0x430936`), and
  `SV_Map_f` does so before it loads the map (`0x430de1`). fnTech3's
  `SV_RTCWRestoreSaveTime` sets `sv.time` from the save's header at the same
  point of both paths. So under retail the first settling frame runs at the
  saved level time exactly, and now it does under fnTech3 too.
- Retail's `map_restart` runs no bot frame at all (`0x430993..0x430aa1`).
  fnTech3's runs one for RTCW SP straight after the game restarts, at the time
  `GAME_INIT` was given, before any settling frame. That fixed a native
  divide-by-zero on repeated `escape2` save loads
  (`2026-09-17-rtcw-campaign-savegames.md`). The change leaves it where it
  was: it still runs before every settling frame, at the same time, which is
  now also the time of the first frame.
- A map load runs a bot frame after each game frame at the same time, in
  retail (`0x4301c0`) and in fnTech3 alike. Only the time the pair runs at
  moves.

## Decisions

Each profile's own retail engine is the authority for it. Where the engine
runs these frames, their times are part of what the game module sees, and the
live RTCW runs show that a game's own behaviour depends on them.

| Profile | Map load | `map_restart` | Evidence |
| --- | --- | --- | --- |
| `q3` | Retail order | Retail order | `quake3.exe` 1.32. Q3's game prints the level time only for `g_debugDamage`, so no live witness; disassembly and fixtures. |
| `ql` | Retail order | Retail order | `quakelive_steam.exe`, read for this change. No live witness; disassembly and fixtures. |
| `rtcw-sp` | Retail order | Retail order | `WolfSP.exe`, with the save interactions above; live on `rocket`. |
| `rtcw-mp` | Retail order | Retail order | `WolfMP.exe`; live on `mp_rocket`. |
| `wolfet` | Retail order, six frames (its own change) | Retail order, six frames (its own change) | `et.exe`, `etded.exe`; behaviour unchanged here. |
| `stvef-sp` | Retail order | No retail command; follows its load | `stvoy.exe`. fnTech3 registers a `map_restart` that retail lacks (see below). |
| `stvef-mp` | Retail order | Retail order | `stvoyHM.exe`. |
| `q3-ihv` | Already retail order | Goes through the map load | IHV record. |
| `jka-mp`, `jk2-mp` | Retail order | Retail order | `jamp.exe`, `jk2mp.exe`. Their developer servers run the retail modules through the frame loop. |
| `jka-sp`, `jk2-sp` | Retail order | No retail command | `jasp.exe`, `jk2sp.exe`. No module of theirs runs yet. |
| `alice` | Its own settle, unchanged | None (removed) | Alice records. |

No retail engine read runs these frames the way Quake3e does, so the
previous order was no profile's contract. The three single-player engines
without a `map_restart` still settle their map loads at the current time, and
the command fnTech3 gives them now does the same.

## Change

- `SV_MapRestart_f` (`code/server/sv_ccmds.cpp`) runs each settling frame and
  its last frame at `sv.time` and advances the time after it, in every
  profile. ET keeps its six frames through the existing `wolfETFrames` flag.
- `SV_SpawnServer` (`code/server/sv_init.cpp`) does the same in the branch
  every profile but ET and Alice takes, bot frame included. The IHV test's
  frames are unchanged, and it still runs no last frame. ET's branch and
  Alice's settle are untouched.
- Unchanged for every profile: the frame counts, `sv_serverRestarting`, the
  RTCW SP bot frame, one console `wait` per frame, fnTech3's own `svs.time`
  advancing 100 ms, the clients each command brings back and how, and the
  command gate after a restart. The level still ends at *T*+400, or *T*+700
  for ET.

## Evidence

- `tests/wolfet_restart_settle_tests.py` compiles `SV_MapRestart_f`'s block
  from the game restart to its last frame and runs it for all thirteen
  profiles. Every profile runs its frames at the current time: ET at +0 to
  +500 with its last at +600, the rest at +0 to +200 with the last at +300.
  Active clients are begun at game time +500 in ET and +200 elsewhere, and the
  RTCW SP bot frame comes first, at +0. Eleven mutations each fail it,
  including the previous ET-only order and Quake3e's order for the frames and
  for the last frame.
- `tests/wolfet_map_load_settle_tests.py` does the same for `SV_SpawnServer`'s
  settling frames through its last frame. Every profile but the IHV test and
  Alice runs its frames and bot frames at the current time, as ET does. Its
  ET mutations now anchor on ET-only text, since the other branch reads the
  same. Three more fail it: Quake3e's order for the frames, for the last
  frame, and the previous IHV-only order.
- `tests/alice_savegame_source_tests.py` checks the three frames and the last
  frame by their new text. `rtcw_match_restart_tests.py`,
  `spawn_view_angle_source_tests.py`, `factory_runtime_source_tests.py`,
  `restart_variable_change_tests.py`, `rtcw_savegame_tests.py`,
  `rtcw_server_policy_tests.py`, `wolfet_map_load_tests.py`,
  `ql_cvar_configstring_source_tests.py`, `workshop_source_tests.py`,
  `stvef_sp_savegame_tests.py` and `q3_ihv_game_lifecycle_tests.py` pass.
  `sv_ccmds.cpp` and `sv_init.cpp` compile with `win32-dev`'s strict-warning
  client and dedicated flags.
- Runtime, Windows x86. Two dedicated servers were built from one snapshot of
  the shared working tree, debugoptimized with strict warnings and
  `-Drtcw-development=true`: the baseline without this change and the
  candidate with it. Their two changed files matched the working tree's byte
  for byte. `scripts/probe_settle_frame_order.py` runs the retail server and
  then each fnTech3 server on a private desktop, with an isolated home under
  `.tmp/settle-frame-order/`, loopback only, no masters, no clients and no
  input. Each loads the map with `g_scriptDebug 1` and restarts it twice with
  `map_restart 0`. The probe compares the script lines of each level's first
  2.5 s, relative to its start. It paces the fnTech3 servers and retail
  `WolfMP.exe` by loopback rcon, as the ET probe does. Retail `WolfSP.exe`'s
  dedicated server opens no network socket, so its steps run from its startup
  config, each followed by 300 console waits.

  | Per level, first 2.5 s | Retail | fnTech3 baseline | fnTech3 candidate |
  | --- | --- | --- | --- |
  | RTCW MP `mp_rocket`: lines, at +0 | 88, 68 | 74, 54 | 88, 68 |
  | RTCW MP: SHA-256 of the lines | `e9dcd6b9…d63d8f3e` | `eb1cd6ec…258cd67d` | `e9dcd6b9…d63d8f3e` |
  | RTCW SP `rocket`: lines, at +0 | 167, 126 | 137, 96 | 167, 126 |
  | RTCW SP: SHA-256 of the lines | `b396397b…cc81f6fd` | `4ad28428…1e4537cc` | `b396397b…cc81f6fd` |
  | Same as retail: load, first and second restart | | no, no, no | yes, yes, yes |

  Retail gave the same lines for its load and both restarts in each mode, and
  so did each fnTech3 server. The baseline's lines are retail's without the
  ones retail's first frame prints at +0: 14 a level on `mp_rocket` and 30 on
  `rocket`. Each repeats a script's current command, printed again because
  it began at that frame's own time. Every other line matches, in order, so
  the baseline first differs at line 54 on `mp_rocket` and line 96 on
  `rocket`. Apart from that first frame, the two orders differ only in the
  frame at +400 that retail does not run, and no script line fell there.

  Every run exited cleanly, logged no error, lookup or `variable change`
  reload, and left the installation unchanged. Each rcon-paced run accepted
  every command once. Retail's MP levels ran 6,800 and 6,100 ms before the
  next restart, and its SP levels 7,950 and 2,550 ms. Earlier runs of both
  modes gave the same digests. The commands, configs, digests and per-level
  results are in the [evidence file](2026-09-24-settle-frame-order.json);
  the lines quote the retail map scripts, so it keeps only their digests.

## Not established

- **No live witness for Quake III, Quake Live, Elite Force or the Jedi
  titles.** Their games print no millisecond level time the probe could read
  without a client. They rest on the disassembly and the fixtures.
- **No client was connected.** A live client's view of the settled level,
  its snapshots and the spawn times of the players a restart begins rest on
  the disassembly and the fixtures.
- **No RTCW SP save was loaded live.** Loading one needs a connected player.
  The save paths rest on the disassembly and `SV_RTCWRestoreSaveTime`.
- **Every retail restart enters every client it reconnects.** Q3
  (`0x432b6f`, `0x432b75`), QL (`0x4de9b6`, `0x4de9bc`), `WolfSP.exe`
  (`0x430a4f`, `0x430a57`), `WolfMP.exe` (`0x43316c`, `0x433174`) and
  `stvoyHM.exe` (`0x42e83c`, `0x42e842`) set each accepted client active and
  call `SV_ClientEnterWorld` with its last command, as `et.exe` does. fnTech3
  still enters only clients that were already active, for the reasons the ET
  restart record gives.
- **Quake Live's `svs.time` advances 400 ms over a restart.** fnTech3's own
  `svs.time` still advances 100 ms, and fnTech3 dates a delayed restart from
  `sv.time`, where retail QL dates it from `svs.time`. Neither was examined
  here.
- **Other differences found while reading these engines** are outside this
  change:
  - `WolfMP.exe`'s map load sets `sv_serverRestarting` to 1 before it starts
    its game (`0x43942c..0x439440`) and back to 0 after its last frame
    (`0x439707..0x439711`). fnTech3 does so at a map load for ET alone. The
    RTCW MP game reads it to keep a player's `enterTime` and to hold back its
    `g_noTeamSwitching` restart.
  - `WolfSP.exe`'s `map_restart` with no argument waits five seconds when the
    game type is not single player and `g_doWarmup` is 0
    (`0x4308a8..0x4308b4`, `0x430859..0x430871`). fnTech3 restarts at once in
    every RTCW mode.
  - `stvoy.exe`, `jasp.exe` and `jk2sp.exe` have no `map_restart`, but fnTech3
    registers one for `stvef-sp`, `jka-sp` and `jk2-sp`.

## Later work

The Elite Force branch merged on 2026-09-26 registers no `map_restart` for
`stvef-sp`, as `stvoy.exe` registers none: single player's `nextmap` reloads
the level in full with `map <name>`, as retail's does
(`2026-09-21-stvef-map-reloads.md`). The frame order above is unchanged; the
`stvef-sp` rows of the tables describe the build this record measured.
