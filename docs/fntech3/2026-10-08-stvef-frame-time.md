# Elite Force frame time under timescale and after a level loads, 2026-10-08

Holomatch bots on fnTech3's server fragged about four times as often in
capture the flag as on retail's, at the `timescale 3` the mode probes play at.
fnTech3's server had kept Quake3e's rule there and lengthened every game tick
by the timescale, where `stvoyHM.exe` keeps its 50 ms tick and runs more of
them. Reading how both retail engines turn a frame's wall time into game time
turned up four more differences, in both modes: a zero timescale, the floor of
1 ms, single player's frame cap while a cutscene is skipped, and the first
frame after a level loads. fnTech3 now does each as `stvoy.exe` 1.20 and
`stvoyHM.exe` 1.20 do. The [JSON companion](2026-10-08-stvef-frame-time.json)
holds identities, addresses and every run.

## Retail

Single player, `stvoy.exe`
(`8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74`):

| What | Retail | Address |
| --- | --- | --- |
| Frame clock | `Com_Frame` waits for the frame, runs the command buffer, then stores the frame's start as the next frame's base. Nothing else writes that base, so a level loaded by a command counts into the next frame | wait 0x41304b..0x41306d, `Cbuf_Execute` 0x41306f, base 0x66601c written only at 0x413061 and 0x41307b |
| Game time of a frame | `fixedtime` if set; else, unless `timescale` is 0, the x87 product of the frame's milliseconds and `timescale`, truncated (`_ftol`); a zero timescale leaves it as it is | `Com_ModifyMsec` 0x412f70; 0x412f7c..0x412f99 |
| Floor and cap | at least 1 ms whatever the timescale; at most 500 ms while `skippingCinematic` is set, else 200, with or without a game running | 0x412fa4, 0x412fae..0x412fcb |
| Tick | fixed `1000 / sv_fps` | `SV_Frame` 0x4265c0 ([SP cadence record](2026-09-21-stvef-sp-cadence.md)) |

Holomatch, `stvoyHM.exe`
(`5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff`):

| What | Retail | Address |
| --- | --- | --- |
| Frame clock | as single player's; the base is written only in `Com_Frame` | 0x41c6c4..0x41c6eb; base 0x5916b0 written at 0x41c6da and 0x41c6fb |
| Game time of a frame | as single player's | 0x41c706..0x41c72b |
| Floor and cap | at least 1 ms; a dedicated server warns of a hitch above 500 ms and caps at 5,000; otherwise 200 while it hosts a game, 5,000 when it does not | 0x41c732..0x41c77e |
| Tick | `1000 / sv_fps`, with no timescale in it; `sv_fps` below 1 is set to 10 | `SV_Frame` 0x4351a0; 0x435252..0x435265, 0x43523b |
| Bots | a dedicated server runs the bot frame once, before the ticks a frame owes | 0x43538e, ticks 0x435396..0x4353c8 |

The single-player game skips a scripted scene when the use-item button (bit
4) is newly pressed in it: it sets `skippingCinematic 1` and `timescale 100`,
and clears both when the scene ends or the button is pressed again
(`efgamex86.dll` `c159cba6…` `ClientThink_real` 0x20056250, button test
0x20056ab0). The engine's 500 ms cap sets how fast that runs.

## Change

- `Com_ModifyMsec` (`code/qcommon/common.c`): under both Elite Force profiles
  a zero timescale leaves the frame unscaled, the product is truncated from a
  double, which holds the x87 product exactly, and no frame falls below 1 ms
  whatever the timescale. Single player caps a frame at 500 ms while
  `skippingCinematic` is set and at 200 otherwise, with or without a game;
  Holomatch keeps the shared caps.
- `SV_Frame` (`code/server/sv_main.cpp`): Holomatch keeps the fixed
  `1000 / sv_fps` tick that single player already had. Its snapshots keep the
  shared schedule.
- `SV_SpawnServer` (`code/server/sv_init.cpp`): neither Elite Force profile
  restarts the frame clock when a level has spawned, so the load counts into
  the next frame up to its cap, as it does in both retail engines. Single
  player therefore begins every level with a full 200 ms frame, four ticks; a
  dedicated Holomatch server catches up on the load in its first frame.

The other profiles keep Quake3e's rules, which the controls in the tests
below pin. No retail text or code was incorporated.

## Evidence

- `tests/stvef_frame_time_tests.py` compiles the production `Com_ModifyMsec`
  for both modes and five control profiles with stubbed cvars: unscaled zero
  timescale, 10 ms at `timescale 0.7` giving 6 (the exact product
  6.99999988 truncated; a float multiply rounds it to 7 first), the 1 ms
  floor for tiny and negative timescales and `fixedtime`, single player's
  200/500 caps with and without a game, and Holomatch's shared caps and hitch
  warning. It also checks Holomatch's fixed tick and the spawn's guarded
  clock restart in the production text. HEAD's function, a cap without
  `skippingCinematic` and a float multiply each fail it.
- `tests/stvef_sp_cadence_tests.py`, which compiles the production `SV_Frame`,
  now expects Holomatch's fixed tick at timescale 0.5, 2 and 3, while the
  five other profiles keep their scaled ticks; with Holomatch's tick scaled
  again it fails.
- Every edited file builds with strict warnings in `git archive` exports of
  `37b87dce` with this change (`fntech3.exe` `3f1cf262…`, `fntech3.ded.exe`
  `b05b3b50…`; the fixed-tick rows below come from an earlier build without
  the spawn rule, `2a45f501…` and `ccc8691a…`) and of `8da1234c` with it and
  the next change (`fd4f979d…`, `663f4162…`). The baseline servers are
  `088a4f40…` (`45444bd3`) and `c1720b14…`, the baseline client `83a7723e…`.
- A one-minute duel at `timescale 0` (`scripts/probe_stvef_modes.py --cases
  tournament --timescale 0 --timelimit 1`): retail's server reached its time
  limit after 55.8 s and this change's after 60.8 s; the baseline's game time
  never moved, and the probe stopped it after 100 s.
- Capture the flag with six bots and team games with four, at `timescale 3`
  with three-minute limits, bots added by the server's startup config:

  | Server | CTF kills | Mean | Bots left spectating |
  | --- | --- | --- | --- |
  | retail | 0, 2, 7, 3, 2 | 2.8 | in 4 of 5 games |
  | baseline | 18, 14, 10, 5, 15 | 12.4 | in 2 of 5 |
  | fixed tick | 7, 1, 12, 4 | 6.0 | in 2 of 4 |
  | this change | 7, 12, 12, 6 | 9.2 | in 2 of 4 |

  Retail's bots join their teams after a hitch of 1.4 to 3.5 s of game time
  (`Hitch warning: 1374` to `3465 msec`): its first frame after the load
  carries the map's and the bots' load, which took it 0.46 to 1.15 s of real
  time. The baseline's bots joined at 0:00. With this change they join at
  0:01, after the shorter load fnTech3 needs, and bots stay spectators as
  retail's do. Which bots stay out is the game module's team rule; it varies
  from game to game on both servers.
- The same capture the flag with the bots added over rcon three seconds into
  a running level, so neither server's load is in play: retail's games had
  15, 13, 9 and 3 kills (mean 10.0) and every bot joined; the fixed-tick
  build's 8, 12, 3 and 9 (mean 8.0), one bot spectating in one game.
- Skipping `voy1`'s opening scene with the use-item button at five frames a
  second (`scripts/probe_stvef_input.py --skip`, synthetic input on a private
  desktop as the user approved for these probes): `skippingCinematic` stayed
  set for 25.3 to 26.4 s on the baseline client and 8.8 to 9.9 s with this
  change, the ratio of the 200 and 500 ms caps.
- Every run left the retail installation unchanged.

## Not changed

- At a timescale other than 1, `stvoyHM.exe` sends each remote client its
  snapshots on game time, so a client then gets one per tick; fnTech3 keeps
  its real-time schedule and rate limits. Timescale needs cheats, and the
  game sees no difference.
- `sv_fps` stays within 10 to 125, where retail takes any value from 1.
- The hitch warning still needs a map to be running, where retail's prints on
  an idle dedicated server too.
- Both retail engines list infostring keys newest first; fnTech3 keeps its
  sorted order. Readers look keys up by name.
