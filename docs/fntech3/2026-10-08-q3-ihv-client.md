# The IHV Test's developer client plays its demo, 2026-10-08

Until now the February 1999 IHV Test's client side was gated completely. Its
client game, `cgamex86.dll`, had been run only in isolated hosts with
stand-in services, and no fnTech3 client could load it. A Windows x86 developer
client (`-Dq3-ihv-development=true`) now runs the original client game inside
the engine, with the engine's own collision, renderers, sound and 2D drawing.
It plays the installation's `sli.dm3` to its end on the GLx, Vulkan and RTX
renderers. Live play is not done: there is no protocol-33 connection yet, in
either direction. The [JSON companion](2026-10-08-q3-ihv-client.json) holds
identities, commands and results.

## Identities

| File | SHA-256 |
| --- | --- |
| `quake3.exe` (725,042 bytes) | `e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37` |
| `cgamex86.dll` (192,570 bytes) | `175f94756f994b06235690fdd2d9fe798503f7a797f2e070277a672847593614` |
| `baseq3/demos/sli.dm3` (8,628 bytes) | `fbdc209d1c7fc1075976c9120c79d5e579ed36f4e38062b01248d6b2d807c57b` |
| `fntech3.exe`, `win32-dev` at the change | `06af347a0cc05972b27a65928eff5804a6cb625e45d007a992be860e70c04fa2` |

The installation at `E:\Games\Quake 3 IHV` was only read; a hash of its whole
tree was unchanged after every run. The original engine ran from the writable
copy under `.tmp/q3-ihv/original-server`. Addresses below are virtual
addresses (`quake3.exe` at `0x400000`, `cgamex86.dll` at `0x10000000`), read
with pefile and Capstone. No original code is incorporated.

## Observed in the original

**The client game's table.** `CL_InitCGame` (`0x405e30`) zeroes 62 words on
its stack, fills them and passes the table to the module's Init. Words 0 and
57 stay null. Words 37 and 38 are the video width and height themselves,
copied from the renderer's export record. Word 61 is the address of the
byte the engine copies into each command's weapon field (`0x402e91`); the
module writes its weapon there. Every other word is a function. The services
named by what each target does: print, error, milliseconds, `Cvar_Get` and a
forced `Cvar_Set`, the three argument calls, whole-file read, free and write,
command text, the debug graph, a client command (`0x404aa0`, a reliable
string), then the collision calls (15-26), the sound calls (27-36), the
world, model, skin and shader registrations, the scene, colour, stretched
picture, begin and end frame (39-47), the screen update and 2D text and
pictures (48-55), the gamestate copy (56), and the entity, command-number and
command getters (58-60). `tests/q3_ihv_client_tests.py` pins all 62.

**Its exports.** `GetCGameAPI` (`0x10009590`) returns eight functions:
Init, the snapshot push, the configstring push, the server command (already
tokenized), the console command, the frame, the draw and the loading screen.
There is no shutdown. The engine pushes the frame record each time a packet
carrying one is parsed (`0x406c2f`), after making the connection active.
Server commands come through `CL_ParseServerCommand` (`0x406f60`): the
engine itself takes `disconnect` and `clientLevelShot`, and passes the rest.

**The frame.** While active, `CL_CgameFrame` (`0x4059b0`) calls the frame
export with the real time and a flag that is 1 while a demo plays. The
export answers whether it has something to draw. While a demo plays, the engine
reads another record each time it answers no (`0x405a40`). The module answers
no without a snapshot, while the next demo frame is older than its time, or
when the map it names is not the one loaded (import 19, `0x10010daa`). Run
under cdb, the original read 18 records over 77 frame calls, 16 of which
answered no, and rendered a 640x480 scene after each of the other 61.

**Collision.** The calls are Quake II's. An inline model is a pointer to a
112-byte record, and a trace or contents call takes the headnode that
record holds. The record is mins, maxs, then the headnode at byte 24: the
module passes the word there to the contents call (`0x1000e73f`,
`0x1000e9e8`), and the engine's own brush walk starts from the world's
(`0x40aafd`). fnTech3's record type had put the headnode first; it now
follows the original, with the name, MD3 flag and pointers where they were.
Import 16 is not a leaf query but the brush query the module clips marks
against: pointers to 40-byte brushes whose side count is at `0x1c` and sides
at `0x20` (`0x1000b2c6`), Q3Test 1.05's shape exactly.

**The original's own captures.** Its `screenshot` reads the current buffer
the moment the console command runs (`0x429660`), after the frame's swap. On
this machine its captures held only the module's scoreboard over black,
however far the demo had played, so they are not pixel references, and no
pixel comparison with the original is claimed.

## The fnTech3 client

`code/client/cl_q3ihv_cgame.cpp` hosts the module through `VM_CreateTable`,
with the 62 words filled from engine services:

- Cvars reach the module as its 32-byte records, kept in step before every
  call into it, as the server's game mirrors are.
- The inline-model records are built when the module loads the map, each
  holding the engine's clip handle as its headnode. Traces and contents run
  through the engine's collision. MD3 records come from the existing
  metadata reader. The brush query is the 40-byte mirror Q3Test 1.05 uses.
- Registrations come back as tokens carrying their kind. A scene's model,
  skin and shader fields resolve only to their own kind, and an unknown
  token refuses the scene.
- A scene is read where the module left it, each array checked against the
  memory it names. It goes to the renderer through its IHV entity and
  particle paths, with lights, polygons, the no-world flag and IHV's inverted
  area bits.
- Sounds go to the engine's audio pipeline, positioned as the module's own
  position callback reports. Text and pictures go through the engine's
  640x480 screen.

`code/client/cl_q3ihv.cpp` holds the session. Messages are decoded by the
existing protocol-33 `ClientSession`, and its events are pushed as the
original pushes them. The engine's own gamestate mirrors the configstrings for
its map name and console. Demo records are length-prefixed, and the frame
driver reads them until the module can draw. A session is rebuilt for every
connection, since an error raised in a module callback leaves by `longjmp`.
In place of the build's built-in menus, which are not reproduced, a title
screen is drawn on the installation's own console background. Recording an
IHV demo is refused by name for now.

## Runs

Each run used a private desktop, a window, no input and an isolated home with
an empty `q3config.cfg` (the installation's own is read otherwise). Each ran
`demo sli` with `cl_quitOnDemoCompleted 1` and took engine screenshots
120 frames apart:

| Renderer | Exit | Seconds | Notes |
| --- | --- | --- | --- |
| GLx | 0 | 6.1 | `km_portal`, the portal's own view, players, weapon, HUD, obituaries |
| Vulkan | 0 | 4.9 | the same scene |
| RTX (compute path tracing) | 0 | 8.1 | the same scene, lit by traced light only |
| GLx, sound on (OpenAL) | 0 | 6.0 | sounds load from the installation; the one it lacks, `sound/weapons/chainsaw.wav`, is reported missing |

In every run the demo's players named in its userinfo as `./sarge/default`
ask for `models/players/./lower.md3` and its two siblings, which do not exist;
the original reports the same three missing models.

In timedemo mode the module showed the same scoreboard at the same frames in
both engines, and stayed there in both, so its timedemo stepping is the
module's own. Earlier runs of this work found two faults, both fixed before
these. A demo started with only its gamestate read stalled at the snapshot
wait, and the brush query, served as a leaf query, faulted inside the module
(`cgamex86.dll+0xb2c6`).

## Presentation

Following invariant 11, scenes are drawn by fnTech3's renderers through the
IHV paths those already have. Models are posed by the native MD3 animation
and lit by the native model lighting, and the build's fog and sky are drawn
as fnTech3's renderers draw them. RTX traces the scene. Sounds use the OpenAL
pipeline with its distance model, occlusion and environments. Text and HUD
use the engine's screen. Left out: the textured overlay quad the original
draws after a scene (`0x442700`, unused by `sli.dm3`), and the original's
menus. No retail-exact option is added.

## Open

Live protocol-33 sessions in both directions, demo recording, the overlay
quad, the menus, input, a listening test and a frame-by-frame comparison with
the original remain open. The client stays a developer, Windows x86 path, and
release builds still refuse the profile.
