# Holomatch's server clock, snapshot schedule and dedicated frames, 2026-10-09

The [frame-time record](2026-10-08-stvef-frame-time.md) left one difference
in how a Holomatch server keeps time: at a timescale other than 1,
`stvoyHM.exe` sends each client its snapshots on game time and fnTech3 sent
them on the wall clock. Reading the rest of the retail server's timing turned
up how it is done: its server clock is its game clock; each client's next
snapshot waits for a deadline its last message set, from the client's rate
and snapshot interval as retail reads them; and a dedicated server sleeps
5 ms between frames and does nothing in a frame that owes no tick. fnTech3's
dedicated server had instead woken for each tick on the wall clock, so at
`timescale 3` a frame ran three ticks or more and sent one snapshot, and at
a slow timescale its game ran at the timescale exactly, where retail's runs
faster. fnTech3 now keeps Holomatch's time as `stvoyHM.exe` 1.20 does. The
[JSON companion](2026-10-09-stvef-holomatch-server-clock.json) holds
identities, addresses and every run.

## Retail

`stvoyHM.exe` 1.20
(`5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff`):

| What | Retail | Address |
| --- | --- | --- |
| Dedicated frames | `WinMain` calls `Sleep( 5 )` before every `Com_Frame` of a dedicated (or minimized) process, whose timer period `timeBeginPeriod( 1 )` sets; `Com_Frame` then waits for at least 1 ms | 0x440a1a, 0x4404f3; 0x41c6bf |
| Frames that owe no tick | `SV_Frame` on a dedicated server returns when the frame leaves less than a tick, after `NET_Sleep`, which is an empty function on Windows | 0x43529c..0x4352b4; 0x440190 |
| Server clock | `svs.time` advances only with each tick, by the tick, and is the time the game frame gets | 0x4353a4, 0x4353b9 |
| Bot frames | a dedicated server runs one before the ticks a frame owes, at the last tick's time; a listen server runs one every frame, at the time it has reached | 0x43537a..0x435393; 0x43527e..0x435289 |
| A client's next snapshot | after each message: a local client's deadline is `svs.time - 1`, so it gets one every frame; anyone else's is `svs.time` plus the larger of its snapshot interval and `(size + 48) * 1000 / rate`, the size held to 1,500 bytes and the rate held under `sv_maxRate` (raised to 1,000 when set lower); `rateDelayed` is set when the rate's wait is the larger | `SV_SendMessageToClient` 0x435478..0x43551d |
| Sending | a client is skipped while `svs.time` is short of its deadline; a bot's deadline is never set, so its snapshot is built in every frame that reaches the pass | `SV_SendClientMessages` 0x435d25..0x435d31 |
| Rate and snapshots | a LAN client's rate is 99999; anyone else's is its own, held to 1,000..90,000, or 3,000 when it names none; `snaps` is held to 1..30 a second, or 20 when absent; neither change moves the deadline already set | `SV_UserinfoChanged` 0x4306ca..0x4307bb |
| `sv_fps` | any value from 1; below 1 it is set to 10 | 0x43523b; `stvoy.exe` 1.20 (`8f7ccadb…`) 0x426633 |

## Change

- `Com_Frame` (`code/qcommon/common.c`): a dedicated Holomatch server starts
  each frame on the first millisecond after five whole ones have passed
  since its last frame ended, where it had waited for its next tick, as
  every other profile's server still does. `Sleep( 5 )` on retail's 1 ms
  timer behaves the same way.
- `SV_Frame` (`code/server/sv_main.cpp`): Holomatch's `svs.time` advances
  with each tick instead of with the wall clock, and a dedicated Holomatch
  server returns at once from a frame that owes no tick, before its bot
  frame, timeouts, snapshots and heartbeat.
- `SV_SendMessageToClient` and `SV_SendClientMessages`
  (`code/server/sv_snapshot.cpp`): each Holomatch message sets the client's
  next deadline as retail's does (`SV_HolomatchSnapshotDelay`), and the
  snapshot pass waits for it in place of the shared interval and rate checks.
- `SV_UserinfoChanged` (`code/server/sv_client.cpp`): Holomatch reads `rate`
  and `snaps` with retail's limits (`SV_HolomatchRateAndSnaps`); the shared
  rule moves unchanged into `SV_ClientRateAndSnaps` for every other profile.
- `SV_Init` (`code/server/sv_init.cpp`): both Elite Force profiles take
  `sv_fps` from 1 to 1,000. Past 1,000 the tick would be 0 ms, on which
  retail's server never leaves its frame.

Every other profile keeps its scheduler, clock and rules; the tests below pin
them. Because the server clock is the game clock, Holomatch's challenge
pings, timeouts, reconnect limit, download resends and master heartbeat
count game time too, as retail's do, and so does fnTech3's own limit of four
seconds for a new client's first packet. No retail text or code was
incorporated.

## Evidence

- `tests/stvef_sp_cadence_tests.py`, which compiles the production
  `SV_Frame`, `SV_SendMessageToClient` and `SV_SendClientMessages`, now
  checks Holomatch's clock standing still between ticks and while paused,
  its deadlines from the rate (1,400 bytes at 3,000 a second hold the next
  snapshot 482 ms and flag it), the 1,500-byte cap, `sv_maxRate`, one
  snapshot per tick at timescales 0.5 to 3, local and bot clients every
  frame, the queue guard, and a dedicated server that runs nothing until a
  tick is owed: at timescale 3 and 15 ms frames only the fourth and seventh
  frames tick, run the bot frame and send; at timescale 20 a 5 ms frame runs
  two ticks, one bot frame and one snapshot. Listen servers and the five
  other profiles' dedicated servers still run every frame through. Each of
  eight mutations fails it.
- `tests/stvef_client_rate_tests.py` (new) compiles the production
  `SV_HolomatchRateAndSnaps` and `SV_ClientRateAndSnaps`: the 3,000 default,
  the 1,000 and 90,000 limits, malformed and negative rates, 99999 on the LAN
  and loopback, `snaps` from 0 to 40 and absent, with or without a LAN
  address, and the shared rule's LAN choke, 10,000 default, `sv_maxRate`,
  `sv_minRate` and immediate deadline for every other profile. It also
  checks that only Holomatch dispatches to the retail rule, and the
  `sv_fps` ranges. Each of eight mutations fails it.
- `tests/stvef_frame_time_tests.py` checks the dedicated Holomatch frame
  wait in `Com_Frame`.
- Game speed (`scripts/probe_stvef_game_speed.py`, new): on `hm_borg1` at
  each timescale, three bots added over rcon a minute of wall time apart,
  and the game times of their `ClientConnect` lines read from the game log,
  to the second. Game seconds per wall second over the two minutes:

  | Server | `timescale 0.1` | `0.5` | `3` |
  | --- | --- | --- | --- |
  | retail `stvoyHM.exe` | 0.175 | 0.475 | 3.000 |
  | HEAD (`bada7c65…`) | 0.092 | 0.492 | 2.999 |
  | first build, frames on the fifth millisecond (`be04fb9a…`) | 0.200 | 0.400 | 3.000 |
  | this change (`234c3af5…`) | 0.167 | 0.500 | 3.000 |

  At a whole timescale a frame's game time is exact, and every server runs
  at the timescale. At a fraction it is truncated to whole milliseconds, at
  least one, so the speed follows how long a frame lasts: retail's 0.175 at
  `timescale 0.1` (the 1 ms floor in every frame) and 0.475 at 0.5 both mean
  frames of 5.71 ms on average on this machine, 71% of them 6 ms, as
  `Sleep( 5 )` on a 1 ms timer gives. HEAD's frames lasted until the next
  tick, so its game ran at the timescale itself. A first build of this
  change started each frame on the fifth millisecond after the last ended
  and ran slower than retail at 0.5 and faster at 0.1; starting it on the
  sixth runs within 5% of retail at both. A rerun of the committed probe on
  retail and the final build gave the same figures at 0.1 and 0.5.
- Snapshot spacing: `scripts/probe_stvef_snapshot_cadence.py` (new) joins the
  fnTech3 client to each server with `snaps 20`, records a demo for 20 s of
  wall time and reads every snapshot's server time with
  `fntech3_stvef_demo_probe --times` (new option of
  `tests/fixtures/stvef_demo_probe.cpp`):

  | Server | `timescale` | Snapshots | Server-time gaps (ms: count) |
  | --- | --- | --- | --- |
  | retail | 1 | 398 | 50: 397 |
  | retail | 3 | 1,195 | 50: 1,194 |
  | retail | 0.5 | 182 | 50: 181 |
  | HEAD | 1 | 387 | 50: 375, 100: 11 |
  | HEAD | 3 | 310 | 150: 75, 200: 208, 250: 26 |
  | HEAD | 0.5 | 390 | 0: 194, 50: 195 |
  | this change | 1 | 403 | 50: 402 |
  | this change | 3 | 1,195 | 50: 1,194 |
  | this change | 0.5 | 200 | 50: 199 |

  Retail and this change send a snapshot after every 50 ms tick at every
  timescale. HEAD's wall-clock schedule sent one every three to five ticks
  at `timescale 3`, every snapshot twice with the same server time at 0.5,
  and skipped a tick now and then at 1. No snapshot was flagged as delayed:
  127.0.0.1 is a LAN address, so its rate is 99999. The fnTech3 client
  records only from a server that sets `g_synchronousClients 1`, as retail
  Holomatch's does, so the probe sets it on every server; the schedule does
  not read it.

  The decoder had stopped with a stack overflow on the first demo it read,
  the `win32-dev` build of October 7 too: its entity baselines and frames
  had outgrown Windows' default 1 MB stack. It now keeps them on the heap,
  and decodes all 32,675 records of the three retail demos again, as in the
  [Holomatch record](2026-09-18-stvef-holomatch.md).
- Sessions (`scripts/probe_legacy_sessions.py`, the final build): the retail
  `stvoyHM.exe` client on fnTech3's dedicated server and on an fnTech3 client
  hosting the game, and the fnTech3 client on the retail server, pass every
  step: join, chat both ways, a map change, `map_restart`, reconnect, the
  server quitting under the client, a kick and a pak download inside a mod,
  and, on both dedicated servers, a retail and an fnTech3 client sharing it.
- Every edited file builds with strict warnings in both executables, in
  `git archive` exports of `465b5eb1` with this change (`fntech3.exe`
  `c6650f52…`, `fntech3.ded.exe` `234c3af5…`, `fntech3_stvef_demo_probe.exe`
  `7bc783d8…`). A build of the same code with one comment placed
  differently (`5e333d8f…`) ran at the same three speeds.
- Every run left the retail installation unchanged.

## Not changed

- Retail sends a fragment of a large message from its snapshot pass, one per
  frame that reaches it and no sooner than its rate allows on the game clock.
  fnTech3's netchan queue sends fragments on the wall clock at the client's
  rate, and Holomatch holds the next snapshot until they are all sent. A
  client reassembles the message either way.
- Retail's `Sleep( 5 )` lasts as long as its host's timer makes it; on this
  machine its frames averaged 5.71 ms and fnTech3's 6. A slow timescale
  therefore runs within a few per cent of retail's speed, not at it exactly.
- Pings stay measured on the wall clock to the millisecond; retail's are
  counted in whole ticks.
