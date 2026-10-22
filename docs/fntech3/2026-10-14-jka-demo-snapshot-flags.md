# Jedi Academy server demo snapshot flags, 2026-10-14

The [October 14 round](2026-10-14-compatibility-round.md) reported that
fnTech3's own server-side demo of a `jka-mp` session drew no 3D view once
its opening player-configuration menu closed. It also reported that a
debugger read found the client's snapshot flagged `SNAPFLAG_NOT_ACTIVE`.
Neither points to an engine fault. The flag is the one retail `jamp.exe`
1.01 writes for a client that is still loading. fnTech3's playback skips
those snapshots exactly as retail's does. The "no 3D view" readings were
taken after the demo had already ended. Within the demo's short active span
the world is drawn in every capture, with the menu over it. No engine code
changed. This slice adds a demo snapshot-header reader to the evidence
tooling and regression tests that pin the retail-matched behaviour.

This is Source (retail disassembly), Unit and Live subset evidence. The
[JSON companion](2026-10-14-jka-demo-snapshot-flags.json) identifies the
demos, binaries, retail executable, commands and local outputs. No gate
closes.

## What the demo holds (observed)

The demo is `.tmp/jka-compat/maintained-demo-01/capture/demos/server.dm_26`
(440 records, SHA-256 `6ca7fbd0...`). fnTech3's dedicated server recorded it
on 2026-10-02 while a maintained OpenJK client joined `mp/ffa1`, entered and
disconnected. Reading every record's snapshot header gives:

| Records | Server time (ms) | Delta | `snapFlags` | Meaning |
| --- | --- | --- | --- | --- |
| 0 | -- | -- | -- | gamestate |
| 1-406 | 5,350-26,150 | none | 6 | `SNAPFLAG_NOT_ACTIVE` with `SNAPFLAG_SERVERCOUNT`: client still loading |
| 407-439 | 26,200-27,800 | mostly delta | 4 | `SNAPFLAG_SERVERCOUNT` only: client in the game |

- The server's console shows why. The recorder opens when the client is sent
  its gamestate (`CS_PRIMED`). The OpenJK client then spent about 21 seconds
  of server time loading before `ClientBegin`. Every snapshot the server sent
  meanwhile is flagged not active and has no delta. Recording ends at the
  client's `disconnect`, 1.6 seconds after it entered.
- Record 407 is the first active snapshot. Its message also carries the
  `spc` reliable command, on which the client game opens the
  player-configuration menu. The recorded client never closed that menu
  before disconnecting.
- The client-recorded demo of the same session (`client.dm_26`, 28
  snapshots) holds no loading snapshots. Its gamestate is at sequence 408,
  after `spc` had already been delivered, and its first snapshot is the next
  non-delta one (server time 26,300). Its playback therefore opens no menu
  and shows the spectator view.

## Retail comparison (observed in `jamp.exe` 1.01)

`jamp.exe` (SHA-256 `e8062d39...`) was disassembled with capstone. Nothing
was run.

- **Server.** `SV_WriteSnapshotToClient` (`0x00460bd0`) starts the flags from
  `svs.snapFlagServerBit` (`[0x00c317c8]`) and adds 1 for a rate-delayed
  client. At `0x00460c93` it adds `SNAPFLAG_NOT_ACTIVE` (2) whenever
  `client->state != CS_ACTIVE` (4). At `0x00460bfd` it encodes a non-active
  client's snapshot without a delta base. fnTech3's writer
  (`code/server/sv_snapshot.cpp`) does both the same way. The loading
  snapshots in the demo are therefore what a retail server sends a loading
  client.
- **Client playback.** `CL_SetCGameTime` (`0x00414a90`) reads one demo
  message per frame (`0x0041cf00`) while the client is `CA_PRIMED` (7).
  `CL_FirstSnapshot` (`0x00414a20`) returns without entering the game while
  `cl.snap.snapFlags` (`[0x009770d8]`) has bit 2 set. fnTech3's
  `CL_FirstSnapshot` and `CL_SetCGameTime` do the same. A retail client
  therefore skips the 406 loading snapshots one per frame, as fnTech3 does,
  and starts play at record 407.
- **Retail has no server-side recorder.** Its client recorder refuses to
  start before `CA_ACTIVE` (8), at `0x0041c9d9`: "You must be in a level to
  record". A retail demo therefore never begins with loading snapshots.
  Keeping them is a property of fnTech3's server recorder, not a divergence
  in what the snapshots contain.
- **Menu.** Retail's client-game import 238 (`CG_OPENUIMENU`, case at
  `0x00413032`) passes the menu to the UI's set-active-menu export (7) with
  no demo-playback check, as fnTech3 does. The player-configuration menu
  therefore opens during retail playback too.

## Why the earlier readings showed no world (observed)

The round's probe chained `wait` commands from `activeAction`. Of 29
capture requests, only the first two ran before the demo ended. The rest,
and every `r_speeds` reading in the menu diagnostic, ran after
`CL_NextDemo`. By then the client had disconnected, so 0 leafs and a black
frame were expected. The debugger read seven seconds after launch fell
inside the loading stretch (server time 19,350 is record 272). Playback
reaches the active span about 6.5 seconds after the cgame loads, at one
record per frame.

A new run captures inside the active span. `activeAction` sets
`timescale 0.1`, then takes 12 engine screenshots 60 frames apart, each
after two frames of `r_speeds` (`.tmp/round-2026-10-14/jka-demo/probe_active_span.py`,
GLx, windowed, private desktop, no input, network off). All 12 shots land
before the end-of-demo marker. Every reading counts the world: 1,129-1,157
shaders, 5,786-5,810 surfaces and 63-66 leafs. Every capture shows the
`mp/ffa1` view behind the player-configuration menu. The replay exits 0
after all 439 snapshots, with the demo, installation and binaries
unchanged.

## Changes

- `scripts/jka_demo_evidence.py` gains `snapshot_flags()` and
  `snapshot_summary()`. They decode each record's reliable commands and
  snapshot header with the engine's own static Huffman decoder table, read
  from `code/qcommon/huffman_static.c`. They report the loading and active
  snapshot counts, the first active record and the active span. Container
  bounds are validated first. A truncated field, an overrun symbol or an
  unterminated command raises `ValueError`. Run it as
  `python scripts/jka_demo_evidence.py DEMO.dm_26`. For the server demo it
  reports 406 leading not-active snapshots, 33 active ones and a
  1,600-millisecond active span.
- `tests/jka_demo_snapshot_flags_tests.py` encodes synthetic demos with the
  engine's encoder table and checks the following:
  - every byte symbol round-trips;
  - loading snapshots before and after reliable commands;
  - a demo that never goes active;
  - truncation and unterminated commands.

  It also pins the two retail-matched rules in source: the writer flags any
  non-`CS_ACTIVE` client, and `CL_FirstSnapshot` ignores flagged snapshots.
  When the local capture is present, it checks the 406/33 split and the
  client demo's all-active start.

No other profile's demo writing or playback changed.

## Remaining

- No capture shows a saber blade. This demo's only active span is 1.6
  seconds with the menu open over a spectator's view. A blade capture needs
  a demo recorded from a client that joins and draws a saber.
- Whether fnTech3's server recorder should drop loading snapshots, or start
  at the client's first active snapshot, is a product choice, not a
  compatibility fix. Retail offers no server recorder to compare against.
  Playback of either form matches retail.
- The world behind the menu appears soft in the captures. That was not
  examined here.
- Retail playback of this demo was not run. The comparison above is from
  disassembly.
