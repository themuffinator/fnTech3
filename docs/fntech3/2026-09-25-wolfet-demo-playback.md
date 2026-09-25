# Enemy Territory demo playback

## Scope

The `wolfet` profile admitted no demo protocol, so `demo` refused every Enemy
Territory demo, and the RTX renderer could not be checked against a replayed
ET match. This slice admits ET's own demos and checks them against retail
`et.exe` in both directions. It covers demo playback only: recording already
existed (`docs/fntech3/2026-09-23-wolfet-demo-recording-cvars.md`).

## Observed

- The Enemy Territory GPL source (`src/client/cl_main.c`) names a recording
  `demos/<name>.dm_<PROTOCOL_VERSION>`, 84 for 2.60b, as retail `et.exe` did
  here (`retail_goldrush2.dm_84` below), and its `CL_PlayDemo_f` opens
  `.dm_83` and `.dm_84` names alike, both read with its one parser. The
  release binary's playback path was not disassembled for this slice. `CL_ReadDemoMessage` is Quake III's: a message sequence, a
  length (-1 ends the demo), the message bytes, parsed by
  `CL_ParseServerMessage`. The cgame is told it is replaying at `CG_INIT`
  (`clc.demoplaying`) and at every `CG_DRAW_ACTIVE_FRAME`; timedemo steps 50
  ms a frame, as Quake III's does.
- Retail `et.exe`'s `Com_Frame` runs `Cbuf_Execute` twice a client frame (after
  each event loop), and each call counts a `wait` down by one, so a retail
  client's `wait N` lasts N / 2 frames. fnTech3 counts a wait down once a
  frame (`Cbuf_Wait`), as Quake3e does. The comparisons below give retail
  twice the waits.

## Change

- `demo_protocols` (`code/qcommon/common.c`) lists
  `NETCHAN_WOLFET_PROTOCOL_VERSION`; `CL_DemoProtocolSupported` admits it
  under `wolfet` alone, and no other profile admits 84.
- `CL_DemoWireProfileForProtocol` reads a protocol 84 demo with
  `NETCHAN_WIRE_WOLFET`, the codec live ET play already uses.
- `.dm_83` names are not admitted: there is no 2.55 or 2.56 demo to check
  them with.

## Evidence

Identities: `et.exe` ET 2.60b SHA-256
`4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e`;
`meson/build/win32-dev/fntech3.exe` SHA-256
`e655c33b5176180f12d4ada705a488d917cf0507fdf0be9f0f0d41590779affb` (working
tree over commit `b68782bfe41eeff02097ec2b2eaeed3eb0a1b4db`); installation
`E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory`, read only.
Every run was windowed, on a private desktop, in an isolated home under
`.tmp/wolfet-demos/`, driven by console commands alone. The scratch scripts
that ran them are not committed.

Two demos, each a spectator on `goldrush` placed at three spawn and
intermission points with `setviewpos` (a cheat the local `devmap` allows):

| Demo | Recorded by | Bytes | SHA-256 |
| --- | --- | --- | --- |
| `retail_goldrush2.dm_84` | `et.exe +devmap goldrush`, `record` | 20,988 | `4dc510c2e874c81d286f362d578820e74b065ff99a4ba931b2692dd867a6db64` |
| `fnt_goldrush2.dm_84` | `fntech3.exe` (`wolfet`), `devmap goldrush`, `record` | 15,421 | `cc5424996a1d72120d90ec0a4bbbb7e9aa0346fceee817b5facd9d5b1d55af7e` |

Results:

- **Parse.** With `cl_shownet 3`, both engines print the same command
  sequence for each demo -- every message's `svc_gamestate`,
  `svc_snapshot`, `playerstate` and `packet entities` at the same byte
  offsets, and every `END OF MESSAGE` at the same read count: 1,205 lines and
  300 messages for the retail demo, 305 lines and 76 messages for fnTech3's.
  (Retail also prints each baseline field at this level; fnTech3's ET codec
  prints none, and those lines are left out of the comparison.)
- **Timing.** With `timedemo 1`, each engine replays the retail demo in 196
  frames and fnTech3's in 94, and prints the cgame's `viewpos` every 10
  frames (every 20 retail waits): the same position and yaw at every sample,
  changing between the same two samples for each move.
- **Frames.** fnTech3 replays the retail demo in the GL, Vulkan and RTX
  renderers at 800 x 600 with the HUD, and retail replays it: at timedemo
  frames 20, 70 and 170 all four show the same view and the retail client
  game's same text -- `(WARMUP) Match begins in: 54`, `51` and `46`,
  `SPECTATOR`, and its demo controls panel, which it draws only when told it
  is replaying. Retail replaying fnTech3's demo shows the same view as
  fnTech3 at frames 20 and 70; its countdown reads `58` at frame 20 in both
  and `56` against fnTech3's `55` at frame 70, where retail's half-frame
  waits place its capture a timedemo frame (50 ms) away across a second's
  boundary.
- Tests: `tests/demo_profile_selection_tests.py` and
  `tests/demo_missing_name_tests.py` compile the client's own demo lookup:
  `wolfet` searches `.dm_84` alone, opens one, and names it when it is
  missing, and no other profile admits 84.

## Open

- One map, spectators only: no player, bot or gameplay event crossed a demo,
  and no retail ET demo from a match was replayed.
- `.dm_83` names, which retail also opens.
- ET's demo gate stays partial in the
  [compatibility ledger](COMPATIBILITY.md).
