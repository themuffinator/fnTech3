# Elite Force single-player cgame

## Scope

This record covers running the cgame half of the retail `efgamex86.dll` in
fnTech3's client: how the client reaches it through the game library the
server loads, its exports and 71 imports, its records, the order in which it
starts and stops with the game, the command number it predicts from, and the
viewpoint the server builds its snapshots from. Ambient sound sets,
savegames and their screenshots, level transitions, Bink movies and
lip-sync belong to later slices. The profile stays behind
`-Dstvef-development=true`.

The host is `code/client/client_stvef_sp_cgame.hpp`; the game host it
attaches to is described in the [game host record](2026-09-19-stvef-sp-host.md).

## The boundary

| Contract | Retail `stvoy.exe` 1.20 | fnTech3 |
| --- | --- | --- |
| Where the cgame lives | In the game library. The server loads it for the game, then calls its `dllEntry` with the client's import handler and keeps its `vmMain` (`0x42e670`); the client calls that `vmMain` directly (`0x42ccf0`) | The server attaches the cgame half as before. The client lends the server its import handler and a shutdown hook, and runs the cgame through a VM that owns no library and calls the `vmMain` the server keeps |
| Exports | `CG_INIT` with one argument, the command sequence the gamestate carried (`CL_InitCGame` `0x4032cf`); `CG_SHUTDOWN`; `CG_CONSOLE_COMMAND` (`0x403320`); `CG_DRAW_ACTIVE_FRAME` with the server time, the stereo frame and 0 (`0x403358`); `CG_CROSSHAIR_PLAYER`. `CG_CAMERA_POS` (5) is the server's, below | The engine's cgame calls become these `vmMain` calls. Its other cgame calls have no single-player export: they answer nothing, and `CG_LAST_ATTACKER` −1, as `vmMain` answers an unknown export |
| Imports | 71, numbered 0 to 70 as in Raven's SP SDK, in one dispatcher; any other number is a dropped "Bad cgame system trap" (`0x402b20`) | Once the cgame runs, the shared cgame dispatcher handles the calls whose meaning and records are the engine's; single player's records and its own calls are decoded in the host. The same error for any other number |
| Before the cgame starts | The game half calls cgame code while a level spawns: precaching its characters registers models, skins, shaders and sounds, and movers ask for their sounds. The renderer is shut down then (`CL_ShutdownAll` `0x4073f0` runs before the game's `Init`) and refuses models ("called before ready"); the cgame registers every character's media again as it starts. Sound starts are dropped until the cgame has started (`0x402d5a`) | The same: renderer registrations answer 0 until the renderer runs, sound registration works, sound starts, the scene and snapshots wait for the cgame |
| Shutdown | The game's `Shutdown`, then any cinematic stopped, then the client's cgame shut down, then the library freed (`SV_ShutdownGameProgs` `0x425450`) | The server calls the client's hook between the game's `Shutdown` and freeing the library, so the cgame never runs from a freed library |
| Snapshot | 60,004 bytes. Retail writes the flags, ping, server time, area mask, command number, player state, at most 256 entities and the command sequence, and leaves the rest of the cgame's record alone (`CL_GetSnapshot` `0x4026a0`) | The same fields through the state adapter; a snapshot the record cannot express publishes nothing |
| Gamestate, command, glconfig | 0x4e84, 28 and 0x144c bytes (`0x4025d0`, `0x402610`, `0x4025f0`) | The same records |
| Command number | The server writes the number of the last command it ran into every snapshot (`SV_WriteSnapshotToClient` `0x428138`) and the client keeps it (`CL_ParseSnapshot` `0x409559`); the cgame replays the commands after it | Loopback snapshots carry that command's time instead, so the client takes the newest command it sent at or before that time |
| First snapshot | Single player's snapshot flags are rate-delayed and dropped commands; the client goes active with its first snapshot (`CL_FirstSnapshot` `0x403460`) | This engine marks snapshots from before the player enters the world as inactive and waits for one that is not. The cgame is not shown the inactive ones: the retail cgame stops with "cg.snap->serverTime > cg.time" if its first snapshot is newer than the client's clock, where Quake III 1.32 clamps |
| Viewpoint | The server asks the cgame for a running camera scene's position (`vmMain(5)`) and otherwise uses the eye, moved by the lean: the roll plus half the lean, then the lean along that right vector (`SV_BuildClientSnapshot` `0x4283a3`) | The same, for the snapshots of the SP profile |
| Lighting | The light grid at a point; zeros where a map has none (`R_GetLighting` `0x45bf10`). The cgame uses it for the player's light level, which characters can see by | The renderer's `LightForPoint`, with zeros where it has no grid |
| Rotated pictures | Turned by an angle in degrees about the picture's top-right corner, drawing the shader's first image (`RB_RotatePic` `0x450fc0`) | A textured quad with the same corners, drawn with the whole shader |
| Kept screenshot | The loading screen draws the 256×256 screenshot a savegame keeps where a level has no levelshot (`0x403006`) | The shared single-player picture of the screen, which the savegames keep and fill ([record](2026-09-19-stvef-sp-savegames.md)) |
| Sample length | Length in milliseconds, 0 for an unknown sound (`0x423520`) | The same |
| Sound channels | Raven's SP numbering: an attenuated voice (4) after the voice, item 5, body 6, ambient 7, local 8, announcer 9 and two menu channels. Retail attenuates by channel: a voice stays at full volume to 1,024 units, an attenuated voice falls silent by about 290, others stay full to 256 and then fall off at Quake III's rate (`S_SpatializeOrigin` `0x420510`) | Translated to the engine's channels for the cgame's and the menus' sounds, so body and ambient sounds are no longer taken for the engine's local channels; single player's own channels take distinct numbers above the engine's. The per-channel falloff belongs with the ambient sound slice |
| Force feedback (34–37) | Force-feedback joysticks | None |
| Scissor (55) | A 2D clip rectangle (backend command 3) | Ignored: neither the SDK's cgame nor the menus set one |
| Ambient sets (65–69) | Sets read from `sound/sound.txt` | Not played yet. Unknown sets are answered as retail answers them: a local set the client's time (`0x41ddc2`), a mover's sound −1 (`0x41de1f`), so movers are silent |
| Update screen (16) | Pumps the event loop, then redraws | Redraws only, as Quake III 1.32 does, because a server restart inside that loop breaks the load |
| Noise waves | Both executables take `noise` as a wave anywhere a wave is read: the base plus the amplitude times the noise at (time + phase) × frequency (`EvalWaveForm`, `stvoy.exe` `0x4642ec`, `stvoyHM.exe` `0x47b5cc`); Quake III takes it only in vertex deforms. The retail `_brig` map's panels stretch their texture with it | The same in all three renderers, for both modes; without it the renderer stopped the level |
| Held frames | The render flag 0x400 caps both frames at a model's last before any wrap (`R_AddMD3Surfaces` `0x461f8b`); the cgame sets it on props that animate once and on explosion models | The engine's `RF_CAP_FRAMES`, with the same number, in all three renderers' MD3 path |

## Results

A developer client with the `stvef-sp` profile, 1,024 by 768 in a window,
runs the retail cgame on the retail maps, captured with the engine's
`screenshot` command. `borg1` starts the cgame in about 2.6 seconds, goes
active on its first snapshot and plays the game's opening text crawl;
`voy1` plays its bridge scene with the subtitles its script sends. The
tutorial, the holodeck firing range, `borg2`, `dn1`, `scav1`, `forge3`,
`stasis1`, `tour/deck01` and `voy2` draw the world, characters, the view
weapon and the retail HUD in play, and `_brig` shows its mission analysis.
The sessions end cleanly through the cgame's shutdown before the library
unloads. The installation was left unchanged.

Two of those maps first failed. `stasis1`'s gamestate did not fit single
player's 16 KB message while every changed word of a baseline took 33 bits;
the loopback bodies now send words in shorter lossless forms
([game host record](2026-09-19-stvef-sp-host.md)). `_brig` stopped the
renderer at its panels' noise wave, now drawn as retail draws it.

## Tests

- `stvef_sp_cgame_tests` checks that all 71 imports are answered, pins the
  VM that owns no library, the export calls and their arguments, the
  client's side lent to the server, the shutdown order, the calls answered
  before the cgame starts, the snapshot record and its inactive-snapshot
  rule and the viewpoint; its probe checks the command number against sent
  commands' times.
- `fntech3_stvef_render_records` turns a picture about its top-right corner
  and refuses non-finite input, and passes the held-frames flag through.
- The same test file pins held frames and noise waves in all three renderers.

## Remaining

Ambient sound sets, which also give movers their sounds; Bink movies;
lip-sync; and a comparison with the retail renderer of the lighting values
and of the characters, whose bodies are Raven's RDM5 skeletal models.
Savegames, their pictures and the levels a player moves between are in the
[savegame record](2026-09-19-stvef-sp-savegames.md).
