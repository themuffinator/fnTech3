# Jedi Outcast's client game: services and effects (2026-10-16)

Jedi Outcast multiplayer (`jk2-mp`) ran its menus but its client game stopped
at client-game import 300, before any map loaded. This slice serves the
client game's collision, sound, registration, scene, 2D, input-value, parser,
cinematic and ROFF imports and its effects system, each from its handler in
retail `jk2mp.exe`. A local `ffa_bespin` start now registers the client
game's shared buffer, loads the collision map and the world, starts the
effects system and registers its effect files, and stops at client-game
import 285, the Ghoul2 model initializer. No map has been drawn yet, so
Outcast's in-game HUD is still not placed, and no gate closes.

Evidence levels follow the [ledger](COMPATIBILITY.md): Source (retail
disassembly read here), Unit (tests), Live (engine runs).

## Imports served (Source)

Read from `jk2mp.exe` 1.02a (SHA256 `2f08dcf9...a1577a`), dispatcher
`0x4017a0` (index `0x4031a0`, jump table `0x402f14`). The retail client game
is `vm/cgame.qvm` in `assets0.pk3`, so every address it passes is an offset
into its own data; each record is bounds-checked whole and copied
(`code/client/client_jk2_cgame_services.hpp`).

| Imports | Retail handler | What it is | How fnTech3 serves it |
| --- | --- | --- | --- |
| 22, 23 | `0x401a40`, `0x401a55` | temporary box / capsule model | `CM_TempBoxModel` |
| 24, 25 | `0x401a7f`, `0x401a9e` | point contents, transformed | collision model |
| 26-29 | `0x401ad7`..`0x401b7a` | box/capsule traces, transformed | collision model, 1080-byte record (below) |
| 30 | `0x401bef` | the renderer's mark fragments | bounded to 64 points, as the renderer's arrays are |
| 31 | `0x401c3e` -> `0x45d200` | mute an entity's channel | nothing below channel 1, as retail |
| 32-39 | `0x401c56`..`0x401d41` | sounds, loops, listener | Fn's spatial audio, Quake III's channels |
| 40 | `0x401d73` | register a sound | the name only; no Quake III compression word |
| 41 | `0x401d8b` -> `0x45e2c0` | background music | with its third word set retail only keeps the names (for its `snd_restart`, `0x40b05e`), so nothing starts |
| 42-46 | `0x401dc0`..`0x401e26` | world, model, skin, shaders | the shared registrations, names bounded |
| 52 | `0x401eeb` -> `0x473e50` | read one character | see below |
| 200-211 | `0x401f04`..`0x4020b8` | scene, entities, polygons, lights, 2D, bounds, tags | Outcast's own records (`jk2_render_abi.h`) into Fn's scene; 2D from Raven's virtual 640x480 screen |
| 214 | `0x4025cc` | remap a shader | shared |
| 226 | `0x402215` -> `0x4011d0` | weapon, sensitivity, force and inventory selections | `CL_FinishMove` `0x4076c0` sends their low bytes, as Academy's does |
| 229 | `0x40226f` | open a UI menu | `VM_Call( uivm, 7, menu )`, the number in the UI's own terms |
| 234, 235 | `0x40fb50`, `0x40fb60` | key catcher | shared |
| 238-241 | `0x40243a`..`0x402499` | definition parser | botlib's load, free, read and locate slots (`+0x200`..`+0x20c`), the UI's |
| 246-251 | `0x402527`..`0x4025a8` | snap vector, cinematics | shared |
| 273, 274 | `0x402a88`, `0x402a9a` | ROFF clean and update, client lane | an empty lane: clean returns 1 (`0x4357c0`), update does no work |
| 300 | `0x402ee1` | the shared buffer | the module address is kept, as retail keeps it at `0x9d9e24` |

Records measured for these imports:

- **Trace.** `CM_BoxTrace` `0x426a60` clears its work area (`0x42a5f0`,
  `0x51c` bytes) and copies `0x10e` words to the caller
  (`0x426ff8..0x426ffd`): 1080 bytes, Quake III's 56-byte record then a
  1024-byte Ghoul2 collision map that a world trace leaves zero. fnTech3 now
  writes all 1080 (`fntech3_jk2_mp_trace_t`, `jk2_client_abi.h`).
- **Read a character.** Retail's renderer reads the module's string pointer
  as a host pointer (`0x473e56`); from the QVM it is an offset, so fnTech3
  reads it within the module's data. Outside Asian languages retail returns
  the byte and steps one (`0x473eac..0x473eb2`); the Asian languages stay
  refused, as Outcast's fonts do. The client game reaches this import when it
  wraps text too wide for its line.
- **Selections.** The force and inventory selections go out in the usercmd
  record's bytes 21 and 22, the shape Academy's has.

## Effects (Source, Unit, Live)

Imports 254, 256, 259, 260 and 262-265 reach Outcast's effects engine, the
design Academy's grew from: a scheduler (`0x9e4710`) and helper (`0x9eb7b8`)
over a pool of 1800 primitives, the same thirteen primitive kinds, and the
same registration rules (`0x415f70`: a lower-cased cache key, `.efx` by
default, `effects/` unless the name starts with it, files under 65535 bytes).
Fn plays Outcast's effect files through the engine it built for Academy
(`client_jka_cgame_effects.hpp`), which draws them as Fn sprites, polygons and
dynamic lights. An adapter (`client_jk2_cgame_effects.hpp`) supplies what
reaches Outcast's own module and view:

- **Start and clock.** `FX_Init` `0x41d910` frees everything `FX_Free`
  `0x41d8d0` does, registered effects included
  (`CFxScheduler::Clean( true, 0 )`), then starts a fresh clock and returns 1.
  Import 265 (`0x41ac60`) hands over the time with the view's origin and
  axes; it has no pause rule. Every primitive adder returns before creating
  anything unless the clock moved forward at least a millisecond
  (`0x41dc76`, `0x41e0c6` and the rest).
- **Traces** go to client-game export 16 through the shared buffer
  (`0x4126a0`): start, bounds, end, skip and mask after the 1080-byte result
  (`0x438`..`0x46c`), skip -1 and mask 1 at both call sites (`0x4124a3`,
  `0x418075`). Fn traces a point, as it does for Academy; retail passes a
  particle's own bounds when the effect asks for them.
- **Decals** go to client-game export 23, the client game's own impact marks:
  a 52-byte record of shader, origin, direction, rotation, colour, alpha and
  size (`0x418989..0x418a0d`), where Academy's go to a renderer pool.
- **Camera shakes do nothing**: retail's helper (`0x41ace0`) measures the
  distance from the view and discards it.
- **Sounds** start at the spawn origin on entity 1023's automatic channel
  (`0x418b26`), as Academy's do.

Retail's parser prints the keys it does not know (`0x4f33b0`..`0x4f3504`),
as Academy's does, so the engine's "as retail does" messages hold for both.
Outcast's effect files have not been compared against `jk2mp.exe` primitive by
primitive; they are read with Academy's measured rules. Imports 266-270, the
client game's direct polygons, Bezier curves, primitives, sprites and
electricity, are refused by name until reached.

Under the feature-set invariant (AGENTS.md, invariant 11), Outcast's effects use Fn's
sprite, polygon and dynamic-light paths and Fn's spatial audio (distance,
occlusion, EFX and HRTF through `S_StartSound`); its decals are the client
game's own marks, drawn through Fn's polygon path. No retail-exact
presentation option is kept: the retail data and module calls are honoured,
not the retail drawing.

## Live runs

Each run: a `jk2-mp` client windowed 1280x720 on a private desktop,
`net_enabled 0`, no sound, no input, isolated home, `+devmap ffa_bespin`
(`.tmp/jka-hud/probe_hud.py`). The first two were built from HEAD
`6442621b` with the change as it then stood, the last from `d17f578a` with
the change as committed. The installation was unchanged after each.

| Run | `fntech3.exe` | Result |
| --- | --- | --- |
| `jk2slice-1`, services only | `4125b683...` | the world loads (3290 faces); stops at import 263, effects |
| `jk2fx-1`, services and effects | `e1f24dba...` | the effects system starts and 22 effect files register with no unknown keys; stops at import 285, Ghoul2 |
| `jk2final-1`, as committed | `5d76ca5d...` | the same: 22 effect files, no unknown keys, stops at import 285 |

The first file registered is `effects/turret/shot.efx` (2 primitives), the
last before the stop `effects/force/lightning.efx`.

## Open

- **Ghoul2** (imports 278-299): models, bolts, animation and their drawing.
  The client game initializes its first Ghoul2 model during start-up, so no
  frame is drawn until this is served. The server already serves Outcast's
  Ghoul2 game imports through the same retail cores (`0x4a6e00`..`0x4a7dc0`).
- Rotated pictures (212, 213), brush-model vertices (217), the effects line
  (218), the forced view angle and turn extent (227, 228), client ROFF
  playback (276) and the direct effect primitives (266-270) are refused by
  name.
- Render entities other than plain models, Raven render flags outside the
  model set, and view flags other than "no world" and "hyperspace" are
  refused by the record adapter; none has been reached live yet.
- Outcast's HUD placement and widescreen rules wait on a drawn frame.

## Found on the way

- The server's Outcast trace import (game import 24) writes Quake III's 56
  bytes, while retail `SV_Trace` `0x450b00` copies the same 1080 bytes
  (`0x450c90..0x450c95`). The 1024-byte tail is left as the game module had
  it. Not changed here.

## Tests

`tests/jk2_cgame_services_tests.py` checks the import map against the
handlers above, the named refusals, the hook order, the selection publish,
the effects adapter's routes and Academy's unchanged defaults, and runs the
trace record, character reader and name readers against a QVM data segment.
`tests/jka_cgame_lifecycle_tests.py` and
`tests/jka_cgame_command_values_tests.py` now run Outcast's reset and its
selections. The Academy effects tests run unchanged.
