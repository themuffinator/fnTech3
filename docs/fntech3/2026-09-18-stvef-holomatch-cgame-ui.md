# Elite Force Holomatch cgame, UI and effect rendering

## Scope

This slice runs the retail Holomatch `cgame.qvm` and `ui.qvm` in the
`stvef-mp` client. It covers their import tables and records, the UI's
server lists and key events, and the renderer contracts Holomatch's effects
depend on: Raven's effect entity types, the lighting flags, two shader
keywords, the overbright default, the console font and the silent default
sound, and entity alpha forcing. With it, the retail demos play in the
developer client. A live session with a server and the single-player profile
are later slices. The profile stays behind `-Dstvef-development=true`.

## Evidence and provenance

Retail facts come from `stvoyHM.exe` 1.20 (identified in the
[foundation record](2026-09-18-stvef-contracts.md)), read through a local
decompilation and checked against its instructions. [Lilium Voyager](https://github.com/zturtleman/lilium-voyager/tree/0f7dcd89d142c8b635ce2756a80cb62411731848)
was used as a cross-check and as the reference for rendered frames; where it
differs from retail, retail is followed.

| Contract | Retail source | fnTech3 |
| --- | --- | --- |
| cgame imports | Dispatcher `0x402ed0` takes 0-20, 22-60 and 100-110, numbered as Quake III 1.32 up to 58 and for the math imports; 59 registers a 3D shader, 60 sets a cvar quietly, 109 and 110 print | Same set. The shared imports keep their Quake III numbers; 21 (`CM_LOADMODEL`) and every unknown number stop with `Bad cgame system trap`, as retail does |
| cgame records | Snapshot, command, gamestate (1,024 configstrings in 16,000 bytes) and glconfig (1.17 strings, a 2 KB extension list and an anisotropic filtering flag) | Converted at the boundary. A gamestate whose offsets leave its own text is refused rather than published |
| File reads | `FS_Read` `0x41f072` returns before it looks at the length when the handle is 0; the cgame reads -1 bytes from a file that failed to open | Same for the cgame and the UI |
| Sounds | `S_RegisterSound` takes one argument and `S_ClearLoopingSounds` none; an empty intro stops the music; handle 0 is `sound/null.wav` (`0x42a296`) | Same, on both audio backends |
| 3D shaders | Import 59 is `R_FindShader( name, LIGHTMAP_NONE, qtrue )` | `RegisterShader3D` in all three renderers, a renderer API change to version 26 |
| Quiet cvar set | `Cvar_SetNoModify` `0x41cf50`: the cgame sets `model` without announcing a userinfo change | An existing cvar ends up flagged userinfo and unmodified; a pending userinfo update stays pending |
| Test prints | 109 and 110 print `%s%i` and `%s%f` with a newline | Same |
| Server disconnect | `CL_GetServerCommand` `0x402b80` shows the command's own text after the word | Same |
| UI version | Only API 4 is accepted (`0x4132bc`) | Same; every other version is refused, and a UI that fails to load names the Holomatch paks |
| UI imports | Dispatcher `0x4125a0` takes 0-29, 31-58 and 100-108. 46-49 are the local and global server counts and addresses, 50-53 the ping queue, 54-58 cvar registration and update, memory, the CD key and model bounds | Same mapping; 30 and unknown numbers stop with `Bad UI system trap` |
| UI keys | The UI is sent key presses only (`0x40bee6`) | No release events reach Holomatch's UI, from a key or from the menu toggle |
| CD key | The UI asks for a key until `ui_cdkeychecked` is set; retail sets it after its own check | No key is accepted, stored or sent: the key import returns false, and the client sets `ui_cdkeychecked` and `ui_cdkeychecked2` after UI startup so the UI does not ask. No authorize request is made |
| Entity record | 160 bytes; one 28-byte union after `shaderTime`; `RE_AddRefEntityToScene` `0x46ba70` refuses types 15 and above | The adapter maps Raven's type ordinals and unpacks each union member into engine-owned fields |
| Effect surfaces | `RB_SurfaceEntity` `0x476c30`: oriented sprite `0x476fd0`, corner-colored sprite `0x477150`, view-facing line `0x4743b0`/`0x4744a0`, oriented line (inline), tapered line `0x474800`/`0x474900`, bezier `0x474d70`/`0x474ea0`, cylinder `0x475360`, electricity `0x475d90`/`0x475910` | One shared implementation in all three renderers, with retail's vertex order, texture coordinates and colors |
| Lighting flags | `R_SetupEntityLighting` `0x479f3c`: 0x10 adds 100 x identityLight and 0x1 subtracts it, in place of the minimum add of 32 | Same. A darkened ambient stops at zero where retail lets it go negative |
| Entity alpha | Render flag 0x800: when the surface list flushes a batch (`RB_RenderDrawSurfList` `0x463831`), the batch's entity supplies its alpha if that is from 1 to 254; every single-texture stage then takes it at each vertex and draws with source-alpha blending only, so with no depth write or alpha test (`RB_StageIteratorGeneric` `0x46c5e2`). Multitexture stages keep their state, and the list's final batch is flushed without it | Same in all three renderers. The Vulkan renderers draw a forced stage with a per-vertex-color pipeline of the stage's own kind. Shadow-map passes, which retail does not have, never take it |
| Shader keywords | Wave function `random` (`EvalWaveForm` `0x47b5f8`, noise `0x46bf20`) and `depthFunc disable` (`0x466267`) | Both, for the Elite Force profiles only |
| Map overbright | `r_mapOverBrightBits` defaults to 1 | 1 for Elite Force; other profiles keep 2 |
| Console font | `gfx/2d/charsgrid_med`, each glyph in the left half of its cell | Same |

Two retail details differ from Lilium. Retail maps an oriented line's texture
from 0 to the record's `stscale`; Lilium always uses 1. The portable shield
passes 1, so both agree there. Retail gives the first corner of a line
quarter color, which Lilium does not.

## Runtime evidence

A Windows x86 developer build ran against the retail installation, windowed,
with an isolated home directory, mouse input off and volume zero; the
installation was left unchanged. Frames were checked with the engine's
`screenshotJPEG` command only.

- The retail UI draws Holomatch's splash, its first-run configuration screen
  and, with `ui_initialsetup 1`, the main menu with its 3D weapon model.
- All three retail demos play to the end in `timedemo` on the OpenGL
  renderer: `demo0000` 11,803 frames, `demo0001` 6,248 and `extreme` 2,268,
  with no module error. The remaining warnings name 121 sounds the cgame asks
  for that no retail pak contains, and report 56 invalid collision bevels on
  one map.
- Seven frames of `demo0000` were captured from fnTech3 and from Lilium
  Voyager at matched `timedemo` frames. Lilium runs its command buffer twice
  per frame, so its waits were doubled. The world, HUD, weapons and effects
  agree. At frame 280 Lilium drew the portable shield's damaged look; fnTech3
  draws the same look at frames 276 to 278, so the two engines start their
  demos a few frames apart. The gap grows later in the demo, so frames were
  not matched by number past frame 460. Which engine matches retail's timing
  was not tested.
- The Vulkan and RTX renderers draw the same demo frames, with the effect
  entities and HUD.
- Entities with forced alpha are in 8,433 of `demo0000`'s 11,803 frames:
  players pulsing between alpha 50 and 150 and eliminated players fading
  out. With the flag applied, a pulsing player at fnTech3's frame 10 matches
  Lilium's frame 12 (the two-frame start offset): the scene shows through it
  the same way. All three renderers draw it translucent.

## Tests

- `fntech3_stvef_render_records` checks the entity and view records: sizes,
  type ordinals, flags, every union member and the glconfig copy.
- `stvef_client_module_dispatch_tests` compiles the shipping cgame and UI
  dispatchers against recording stubs. It checks retail's accepted and
  refused import numbers, the shared numbering, reads from handle 0, the
  sound imports, 3D shaders, both glconfig sources, the quiet cvar set, the
  test prints, both server lists, the CD key and the gamestate transfer.
- `rtcw_light_tests` checks the brightening and darkening flags in all three
  renderers.
- `stvef_forced_alpha_tests` compiles the shared alpha rules (which alpha is
  forced, which flushes and stages take it, the 0x65 blend state) and checks
  where each renderer's surface list and stage loop apply them.
- The renderer API version, default sound, UI version and key-release checks
  in `rtx_renderer_contract_source_tests`, `ql_font_source_tests`,
  `underwater_view_source_tests`, `rtcw_speech_tests`, `ui_module_abi_tests`
  and `webui_wiring_tests` follow these changes, as does the draw loop probe in
  `rtx_raster_execution_tests`.

## Remaining

- Retail shader parser differences not yet compared: surface parameters such
  as `shotclip`, and the environment texture generator.
- A session between this client and a server: fnTech3's own, retail or
  Lilium.
- Language selection for text and voice (`s_language`).
