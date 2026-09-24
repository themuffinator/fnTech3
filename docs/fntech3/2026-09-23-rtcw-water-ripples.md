# RTCW water ripples and Quake Live's host fonts

## Scope

A developer build running `rtcw-mp` on `mp_beach` ended its session with

```text
ERROR: R_AddEntitySurfaces: Bad reType
----- Server Shutdown (Server crashed: R_AddEntitySurfaces: Bad reType) -----
```

and the same console showed Quake Live's font lane running under RTCW:

```text
QL fonts: unable to read fonts/handelgothic.ttf
QL fonts: unable to read fonts/notosans-regular.ttf
QL fonts: unable to read fonts/droidsansmono.ttf
QL fonts: unable to read fonts/droidsansfallbackfull.ttf
QL fonts: using Windows fallback C:\WINDOWS\Fonts\segoeui.ttf
```

This record covers both. The first is two reference entity types that RTCW
and Enemy Territory declare and their retail renderers accept, but that none
of fnTech3's three renderers accepted. The second is a Quake Live contract
that every profile inherited.

## Reference entity types

### Observed

The client adapters in
[`render_record_adapter.hpp`](../../code/compat/render_record_adapter.hpp)
already translate RTCW's and ET's type ordinals to the engine's own enum. For
both titles, module type 3 becomes `RT_SPLASH` and type 6
`RT_RAIL_CORE_TAPER`. No renderer handled either one.
`R_AddEntitySurfaces` in `renderer`, `renderervk` and `rendererrtx` fell to its
`default` case and raised `ERR_DROP`.

RTCW's cgame makes a `RT_SPLASH` entity in `CG_WaterRipple`, a local entity
spawned when a bullet, grenade or explosion strikes water. The GPL cgame
sources never make a type 6 entity. ET's cgame makes the same ripple in its own
`CG_WaterRipple`. That the observed error was a ripple is an inference, not an
observation. Type 3 is the only type the RTCW MP cgame sources send that these
renderers rejected and retail accepts.

Both retail RTCW executables and retail Enemy Territory 2.60b were read with
capstone. Addresses are virtual addresses in each image.

| | `WolfMP.exe` | `WolfSP.exe` | `et.exe` 2.60b |
| --- | --- | --- | --- |
| SHA-256 | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` | `07d206f61da5c80562ff60d68c645bca349b31adb5d7e0e7f186d61ad46683c7` | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` |
| `R_AddEntitySurfaces` | `0x467af0` | `0x44a5f0` | `0x4af770` |
| Type bound | `cmp ecx, 9; ja` to the `Bad reType` error | same | `cmp eax, 9; ja`, same error |
| Per-type byte table (0 model, 1 generated, 2 skip, 3 error) | `0x467d48`: `0 3 1 1 1 1 1 1 1 2` | `0x44a844`: same values | `0x4af994`: same values |
| `RB_SurfaceEntity` (`rb_surfaceTable[SF_ENTITY]`) | `0x4851c0` (table `0x4fc734`) | `0x4721c0` (table `0x4f5b04`) | `0x4d4e30` (table `0x5967e8`, entry 12) |
| Its switch | `reType - 2`, `cmp eax, 6; ja` default | same | same, default `0x4d571f` |
| Ripple | inline at `0x4851de` | inline at `0x4721de` | inline at `0x4d4e56`, with SSE |

Decoded, all three executables do the same thing:

- **`R_AddEntitySurfaces`.** Type 0 is a model. Types 2 through 8 (sprite,
  ripple, beam, rail core, tapered rail core, rail rings and lightning) share
  one generated-surface branch. That branch skips third-person entities in
  the primary view, takes the custom shader and computes the sprite fog number.
  Type 9 (portal) draws nothing. Type 1 (polygon) and anything above 9 raise
  `Bad reType`.
- **`RB_SurfaceEntity`.** Type 3 reads the radius and builds `left = (-r, 0,
  0)` and `up = (0, r, 0)`. It negates `left` when
  `backEnd.viewParms.isMirror` is set, then calls `RB_AddQuadStampExt(origin,
  left, up, shaderRGBA, 0, 0, 1, 1)`. The result is a quad lying flat in the
  world's x/y plane, not facing the viewer. Type 6 goes to the switch's
  default, which binds the white image and draws red, green and blue lines
  16 units long from the origin: `RB_SurfaceAxis`.

The RTCW SP, RTCW MP and ET GPL sources say the same (`tr_main.c` and
`tr_surface.c`). An earlier revision of this record said no retail ET
executable was available; the Steam installation's `et.exe` was there all
along and is now read above.

### Change

All three renderers now accept `RT_SPLASH` and `RT_RAIL_CORE_TAPER` in the
generated-surface branch of `R_AddEntitySurfaces`. Their `RB_SurfaceEntity`
draws the ripple through a new `RB_SurfaceSplash` that matches retail's.
The tapered rail core stays on the existing default, the debug axis, as in
retail. `RT_POLY` still raises `Bad reType`, as it does in every retail engine.
Q3, QL and Elite Force never produce either type, so their drawing is
unchanged.

Ray tracing's effect geometry in `rendererrtx/vk.c` still takes only sprites
and beams. As with Elite Force's primitives, a ripple is drawn by the raster
path alone.

## Quake Live's host fonts

### Observed

`R_InitFreeType` calls `R_InitHostFonts` in every renderer and every profile.
That function read Quake Live's four faces by name. On Windows it then always
added a system face (`ARIALUNI.TTF`, `segoeui.ttf` or `l_10646.ttf`). The lane
was marked ready if any face loaded. `QL_ResolveHostFaceId` hands back any
loaded face when the requested one is missing.

The console asks for face 2 (`droidsansmono.ttf`). On Windows, every profile
except Quake Live therefore drew its console, and the client's large text,
in the proportional `segoeui.ttf`. That was the only face loaded. On Linux
there is no system fallback, so the same profiles drew their own bitmap
charset. The behavior differed by platform and did not match any original
engine.

### Change

`R_InitHostFonts` now returns before reading anything unless the profile is
`ql`. The face slots are reset to invalid first, so shutdown and the metrics
query both see an empty lane. Every other profile uses the charset the client
registers for it: `gfx/2d/bigchars`, ET's `gfx/2d/consolechars`, or Elite
Force's `gfx/2d/charsgrid_med`. This matches the Linux behavior and the
original engines. Quake Live's lane is unchanged.

## Evidence

- `tests/rtcw_splash_surface_tests.py` reads every adapter's type table.
  It checks that each engine type an adapter can produce, apart from
  `RT_POLY`, reaches the generated-surface branch in all three renderers. It
  also checks that `RB_SurfaceEntity` routes the ripple to `RB_SurfaceSplash`
  and leaves the tapered core on the axis. Finally it compiles each renderer's
  own `RB_SurfaceSplash` and checks the retail vectors in primary, portal and
  mirror views at four radii. Against the previous sources it fails in all
  three renderers.
- `tests/host_font_profile_tests.py` compiles the renderer's own
  `R_InitHostFonts`, `RE_GetScaledFontMetrics` and `R_DoneHostFonts` with
  FontStash and the file system stubbed. It runs them for every profile id.
  Only `ql` reads its four faces and the Windows fallback, and only `ql`
  answers the console's metrics query. Every other profile reads nothing, has
  no FontStash context and frees nothing at shutdown. Against the previous
  source it fails.
- `tests/rtx_scene_linear_gate_tests.py` compiles RTX's own
  `vk_scene_linear_enabled` beside GLx's own `FBO_HdrSceneLinearMode` and
  `FBO_HdrSceneLinearColorMode`, and checks that they agree on all 72 input
  combinations where both apply. It also checks that the shipped defaults are
  display-referred and that each opt-in selects scene-linear colour, and that
  the fog and underwater compositors ask the same gate. Against the previous
  source it fails at the first mismatching combination.
- Runtime, Windows x86 `meson/build/win32-dev` (`-Drtcw-development=true`).
  The tree was at `9e1841d` with this change and other uncommitted work.
  `fntech3.exe` had SHA-256
  `99324378df9e4379e6e3c24dd54f27a191260e88156973af4dd24a0a8007805f`.
  Each run used a private desktop, a windowed 1280x720 window, mouse input
  off, sound off and an isolated home directory, with the legitimate Steam
  RTCW installation read-only. It started with `+devmap mp_beach` and an active
  action of `wait 90; toggleconsole; wait 90; screenshotPNG; quit`.
  GLx (`e0d3a482…`), Vulkan (`886a691d…`) and RTX raster (`d6f608ea…`)
  all exited 0 with no error lines and no `QL fonts` lines. Each engine
  screenshot shows `mp_beach`'s limbo panel under a console drawn in RTCW's
  bitmap charset. The installation inventory was unchanged. The same
  probe on `q3dm17` with the Quake III installation behaved the same way,
  with Quake III's `bigchars`. Under `ql`, at the main menu of the Steam
  installation, the lane was unchanged. All four faces loaded with no read
  warning, the log reported `QL fonts: retail FontStash/STB host text
  enabled`, and the console was drawn in `droidsansmono.ttf`.

No weapon was fired in these runs; the next section fires one.

## Live ripple probe

[`scripts/probe_rtcw_ripple.py`](../../scripts/probe_rtcw_ripple.py) fires into
`mp_beach`'s surf. Every action is a console command from a config written
into the isolated home and run as `activeAction` after `+devmap mp_beach`: join
the Allies as a soldier with a Thompson (`team b 0 14 0 17 0`), close the limbo
menu, `setviewpos -1700 2600 100 180`, `+lookdown` for seven frames, then
`+attack`, with three engine screenshots. No keyboard or mouse input is
injected; the window is on a private desktop, windowed, with sound off. The
config travels in a file because retail splits its command line at every
`+`, even inside quotes.

Placement mattered. `mp_beach` has one water brush,
`textures/liquids/water_beach_nerve`, with its surface at z 64 and the Allied
spawns inside it, but the Allies spawn aboard landing craft and the craft's
decks take the shots. The chosen spot is sand at the waterline between the two
groups of craft, where rounds aimed about 16 degrees down strike the seabed
some 280 units out. A first attempt placed the soldier in the open surf, where
`TeleportPlayer`'s 400 units a second along the new yaw carried the soldier
into deeper water with the eye only 16 units above the surface. The spray then
filled the view.

| Build | Renderer | Result |
| --- | --- | --- |
| Before, `meson/build/win32-gaps` (2026-09-18, `fntech3.exe` `4d04deff…`, `fnql_glx_x86.dll` `86e92564…`) | GLx | `ERROR: R_AddEntitySurfaces: Bad reType`, then `Server crashed: R_AddEntitySurfaces: Bad reType`, the session dropped and the probe had to be stopped: the reported fault, reproduced |
| After, `meson/build/win32-dev` (`fntech3.exe` `805e1ac1…`) | GLx `83528ae7…`, Vulkan `109b8622…`, RTX raster `5f7bfc5e…` | Exit 0, no error line, three screenshots each |
| Retail `WolfMP.exe` `7cf99a93…`, same script | its own renderer | Exit 0, no error line, three screenshots |

The installation inventory was unchanged by every run. In the after captures
each round throws a water spout. Where the view is steep enough, the ripple
shows as concentric rings lying flat on the water at the spout's base: an
ellipse, as a horizontal quad seen at an angle should be. An earlier run aimed
about 30 degrees down showed them plainly under GLx. The wake shader
(`wakeAnim`: `sprites/splashalpha.tga`, alpha-blended, vertex colour,
`tcmod stretch sin 0 1 0 .25` keyed to the entity's shader time) grows each ring
from a point over its one-second life.

Retail ran the same script to the same place (`viewpos` `(-1700 2600 141) :
-180`) and its rounds throw the same water spout. The two do not share a frame:
retail's warmup clock read two seconds earlier at each capture, it pitched
down less for the same seven frames of `+lookdown`, and by each capture it
had fired one, two and three rounds where fnTech3 had fired three, five and
six. So retail runs these frame
counts faster than 60 a second here, even with `com_maxfps 60` on its command
line. (Set from a config, retail's `com_maxfps` is latched and waits for a
restart.) Its spout is therefore smaller and nearer the horizon, and its ring is
too thin to compare at that distance. `fixedtime` would give both engines the
same 16 ms frames, but retail restarts its game soon after `fixedtime` is set
and then stalls, so the probe does not use it. The retail geometry rests on the
disassembly above.

Retail first could not be launched at all. Its SteamStub wrapper (`.bind`
section) exited at once with code 5, or 4 with `SteamAppId` set, before
writing a log. A full restart of Steam cleared it. The stale
`HKCU\Software\Valve\Steam\ActiveProcess\pid` that suggested a cause still
named a dead process after the restart, when launches worked, so it was not
the cause.

Retail also names a new player `WolfPlayer`, where fnTech3's RTCW MP profile
used Quake III's `UnnamedPlayer`. The profile catalog now gives RTCW MP
`WolfPlayer`, RTCW SP `Player` and ET `ETPlayer`, each its engine's registered
default: observed live for `WolfMP.exe`, and from the GPL sources and the
executables' strings for the other two. `tests/profile_player_name_tests.py`
compiles the catalog and checks every one. Retail draws Quake III-style notify
lines at the top of the screen too, which fnTech3 draws in no profile; that is
listed under Open.

## RTX translucent blending

### Observed

Among the after captures, RTX drew the water spray visibly brighter and harder
edged than GLx and Vulkan. To compare the same frame, the probe's firing was
recorded once as a demo (`ripple.dm_60`, SHA-256 `f15e4a05…`) and replayed with
`timedemo 1`. That advances one snapshot per rendered frame, and RTCW MP's
cgame never seeds `rand()`, so frame N of a replay holds the same cgame state
in every run. Two GLx replays were pixel-identical. The mean absolute
difference per pixel from GLx at seven frames spanning the spray was:

| Renderer | Before the spray (frame 100) | During it (frames 104-124) |
| --- | --- | --- |
| Vulkan | 0.77 | 0.77-0.88 |
| RTX, before the change below | 3.54 | 3.59-4.78 |
| RTX, after it | 0.76 | 0.76-0.88 |

The difference image outlined each spray quad and the translucent edges of the
HUD glyphs, while opaque surfaces matched. That is the signature of blending in
another colour space. RTX's `vk_scene_linear_enabled()` returned true whenever
`r_hdr` was 1 with a floating-point scene target. That decoded every authored
texture from sRGB and made every blend happen in linear light. `r_hdr` has
defaulted to 1 in every renderer since `026ff2a` (for bloom parity with
FnQuake3). `docs/DISPLAY.md` defines `r_hdr 1` with the legacy tone mapper
as display-referred, and GLx implements that in `FBO_HdrSceneLinearColorMode`,
which also needs `r_srgbTextures` and a non-legacy tone mapper, colour grading
or HDR output. The RTX guide still listed `r_hdr` as defaulting to 0 and called
scene-linear HDR an opt-in.

### Change

RTX's `vk_scene_linear_enabled()` now asks what GLx asks: `r_fbo`, `r_hdr 1`,
`r_srgbTextures` and a float scene target, plus a non-legacy tone mapper,
colour grading or an active HDR presentation. Its global-fog and underwater
compositors, which must match the output transform, ask the same function
instead of reading `r_hdr` alone. The RTX guide's `r_hdr` row now states the
real default and contract. This affects every profile on RTX, not only RTCW:
default translucent effects and 2D edges now blend as they do in GLx and
Vulkan, and an explicit tone mapper, grade or HDR display still selects
scene-linear colour.

## Console cell

RTCW MP's console charset, background and logo layer, which retail
`WolfMP.exe` takes from `gfx/2d/hudchars`, `console-16bit` and
`console2-16bit`, are the subject of the
[RTCW MP console record](2026-09-23-rtcw-mp-console.md). This section covers
the cell they are drawn in.

With the console drawn in each profile's own charset, its cell size mattered.
Every profile used Quake Live's: retail QL's 12x24 cell at half size, scaled
from a 768-pixel reference height, which is 6x11 at 1280x720. Quake III, RTCW
SP and MP, ET and Elite Force all define an 8x16 `SMALLCHAR`. FnQuake3, the
Quake III modernization reference, scales that cell with the 640x480 virtual
screen (`cls.scale`) under `con_scaleUniform 1`. That gives 8x16 at 640x480,
12x24 at 1280x720 and 18x36 at 1920x1080.

The cell now belongs to the profile. `Con_ProfileCell` gives Quake Live its
retail cell and 768-row reference, and every other profile the 8x16 small
character on FnQuake3's virtual-screen scale. `con_scale 1` is each profile's
own cell, and `con_scaleUniform 0` keeps native pixels. The Quake Live chat
overlay's own cell is unchanged. `tests/console_profile_cell_tests.py` compiles
the console's cell helper and `Con_CheckResize`'s sizing lines, with the small
character read from `q_shared.h` and the client's own canvas fit, and checks
both cells at 640x480, 1280x720, 1366x768, 1920x1080 and a portrait
1024x1280, with and without uniform scaling and a capture factor. RTCW MP and
Quake III consoles probed at 1280x720 now draw 12x24 cells, and Quake Live's
console is unchanged.

## Open

- A same-frame comparison with retail `WolfMP.exe`. Both engines run the probe
  to the same place and throw the same spouts, but retail paces its frames
  faster and stalls under `fixedtime`, so they never share a frame (see the
  live probe section).
- The chat line and notify lines outside Quake Live. Every profile runs Quake
  Live's chat overlay (a strip at virtual y 413 on QL's 12x24 cell) and, as
  retail Quake Live does, draws no recent console lines at the top of the
  screen. Quake III, RTCW, ET and Elite Force draw notify lines for
  `con_notifytime` seconds and their own `say:` prompt with large characters.
  FnQuake3 does the same.
