# Enemy Territory spawned movement and map rotation

## Scope and authority

This work follows `d563ff7`, with a clean worktree at the start. It adds two
bounded inbound interoperability checks to the existing ET retail-client probe:
a spawned movement run and a server map rotation, both under pure admission.
No engine source changed in this slice. The ET client and default-release gates
are unchanged; ET remains experimental and dedicated-only behind
`-Dwolfet-development=true`.

The view-position readout comes from the retail ET game module's own `viewpos`
console command, which prints the rendered view origin and yaw. No reference
source is incorporated.

## Method

Both runs use the existing safety envelope: Windows only, an unswitched private
desktop, a windowed client with `in_mouse 0` and `in_joystick 0`, isolated
writable homes for both processes, and a loopback-bound server. Every action is
a console command executed by the engine's own command buffer. No mouse or
keyboard input is injected at any point, and the retail installation is verified
byte-for-byte unchanged afterwards.

The movement run writes a short config into the client's isolated home and
executes it after the client has joined Axis: `viewpos`, hold forward, release,
`viewpos`, hold left, release, `viewpos`, quit. The rotation run changes the
server's map from `goldrush` to `battery` while a client is active and requires
that client to re-enter the world on the new map.

## Observations

Spawned movement, `sv_pure 1`, `goldrush`:

| Fact | Value |
| --- | --- |
| Spawn | `ClientBegin: 0` on the server |
| View positions printed | 3 |
| Distinct view origins | 3 |
| First origin | `(4 3628 432)` |
| Last origin | `(215 3764 432)` |
| Client / server exit | 0 / 0 |
| Installation unchanged | yes |
| Elapsed | 55.4 s |

The origin moves about 250 units horizontally at a constant height, which is
what a grounded forward-then-left move produces. The view origin only changes if
the server accepted the user commands and both sides simulated the same motion.

Map rotation, `sv_pure 1`, `goldrush` then `battery`:

| Fact | Value |
| --- | --- |
| Server loaded the second map | yes |
| Client re-entered the world | yes (two `CS_PRIMED` to `CS_ACTIVE` transitions) |
| Client loaded the second map | yes |
| Client / server exit | 0 / 0 |
| Installation unchanged | yes |
| Elapsed | 80.6 s |

| Artifact | SHA-256 |
| --- | --- |
| `fntech3.ded.exe` | `d8cdbc4dff0a8e821c409a977bf05d7346665ec1bf9057452f452754a498de93` |
| retail `ET.exe` | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` |
| retail `qagame_mp_x86.dll` | `160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b` |

## Remaining gates

- One client, two maps, one short move. This is not movement parity: no
  trajectory, timing or prediction comparison against a retail server was made,
  and nothing here validates jumping, prone, leaning, vehicles or water.
- Combat, objectives, scoring, longer rotations and dense multi-client
  exchanges remain open, as do landmine, proxy, brush and lean visibility and
  rendered cameras.
- Reverse interoperability with fnTech3 as the ET client is still gated.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Unrelated observations

None new in this slice. The retail Gold Rush script warnings recorded in the
initial ET record are unchanged.
