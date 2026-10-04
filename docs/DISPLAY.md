# Display Guide

fnTech3's renderers draw every game with a client: Quake III Arena, Quake
Live, Return to Castle Wolfenstein, Enemy Territory, Elite Force, Alice and
F.A.K.K.2. The same three renderers, display modes and post-processing serve
all of them, fed with each game's own maps, models, shaders, lights and fog.
Where a game behaves differently -- a default, a format it alone uses, or a
feature it cannot use -- this guide says so.

Display settings fall into three layers: how the window or fullscreen mode is
created, how the scene is rendered internally, and which post-processing is
applied to the final image. This guide covers renderer choice, video modes,
the framebuffer path, anti-aliasing, lighting and shadows, fog, liquids, bloom
and the other scene controls.

For FOV, HUD, menu and cinematic layout on widescreen displays, see the
[Aspect Handling Guide](ASPECT_CORRECTION.md). For screenshot output and
capture options, see the [Screenshot Guide](SCREENSHOTS.md). Which game runs
at all is described in the [Games Guide](GAMES.md).


Jedi Academy's developer MP adapter feeds ordinary-sky views, map distance
data, HUD/menu transforms and single-polygon submissions into these renderers.
Owned single-model geometry and forward-animation transitions use Fn CPU
poses, weighted geometry, runtime MDR, materials, entity lighting, fog and
shadows. The ordinary sky hint uses Fn sky/cloud drawing. An isolated OpenGL
probe now captures the player-configuration menu over the world; complete
gameplay, complete all-backend cgame drawing and active RTX tracing remain unverified.
Owned remote bone/bolt queries retain measured frame clocks and no-rotation
selector behavior. Initial and active forward-animation blends are served;
angle blends, attachments, reverse clips, unfrozen completion and local-server
clock sharing remain open. All 41 installed engine-default GLMs pass geometry
checks across their 65 LODs. Missing ambient sets return without audio; present
sets still refuse active playback. The limited effect lifetimes stage Fn
sprite/polygon packets; light/sound primitives, motion and active curve
controls remain unserved. The maintained forward probe now completes empty
ROFF/weather cleanup and disconnects cleanly. Ordinary/additive module scene
lights now use Fn's light, color and shadow controls; Academy's half-cell atlas
supplies readable Fn diagnostic/console text. A bounded demo control reaches
EOF with spectator-world captures in candidate GLx, Vulkan and RTX raster.
This slice's production relink/runtime checks are pending after a concurrent
renderer sweep blocks the shared client linker. The longer native-server stream
passes the light import, then refuses Raven's saber-glow entity (type 5). Complete gameplay,
all maps/materials/effects and the newer shared RTX implementation remain
unverified. See the
[Academy gate matrix](fntech3/COMPATIBILITY.md#jedi-academy-gate-matrix).

## Renderer Choice

`cl_renderer` selects the rendering backend and requires `vid_restart`. All
three renderers are available in every game with a client.

- `cl_renderer glx`: Canonical OpenGL-lineage renderer and client default, with compatibility tiers, streaming, static-world, material, postprocess, output, and profiling paths.
- `cl_renderer vk`: Vulkan raster renderer with FBO rendering, HDR, multisampling, supersampling, render scaling, shader-based SDR gamma/overbright, greyscale, and bloom.
- `cl_renderer rtx`: Vulkan ray-tracing renderer. It path traces the main view by default (`rtx_rt_mode 2`) and draws a complete raster frame with `rtx_rt_mode 0` or where the GPU can trace neither way; see the [RTX Renderer Guide](RTX.md).

Only `glx`, `vk`, and `rtx` are valid renderer selectors. See [GLX.md](GLX.md) for GLx diagnostics and troubleshooting.

## Display Modes And Window Behavior

These settings control the actual game window or fullscreen mode. Treat them as `vid_restart` settings.

- `r_mode`: Main video mode selector.
  - `-2`: Use the current desktop resolution.
  - `-1`: Use `r_customWidth` and `r_customHeight`.
  - `0..N`: Use a predefined mode from `\modelist`.
- `r_modeFullscreen`: Optional dedicated fullscreen override. Leave it empty to reuse `r_mode`, or set it to one of the same values you would normally use for `r_mode`.
- `r_fullscreen`: `1` for fullscreen, `0` for windowed mode.
- `r_customWidth` and `r_customHeight`: Custom resolution used by `r_mode -1`.
- `r_customPixelAspect`: Advanced custom pixel aspect value for `r_mode -1` or `r_mode -2`. Leave this at `1` unless you intentionally need non-square pixel behavior.
- `r_displayRefresh`: Fullscreen refresh-rate override. `0` uses the current monitor refresh rate.
- `r_noborder`: Removes the title bar and borders in windowed mode.
- `vid_xpos` and `vid_ypos`: Saved window position in windowed mode.
- `r_swapInterval`: V-Sync control.
  - `0`: Do not wait for v-blank.
  - `1`: Sync swaps to the monitor refresh rate.

Windowed placement is checked against the display's usable work area using the
complete native frame, including resize borders and the title bar. This keeps
the chrome reachable after a DPI change, dock/taskbar change, monitor removal,
or renderer restart. On Wayland, the compositor owns top-level placement and
server-side decorations by protocol, so fnTech3 leaves that policy to it
instead of issuing unsupported position requests.

Fullscreen picks one of two forms automatically. A request that matches the current desktop resolution with `r_displayRefresh 0` uses borderless desktop fullscreen: no display mode change, so alt-tab, other monitors, and the desktop layout are left alone. Any other resolution, or an explicit `r_displayRefresh`, switches the display to the closest matching mode at or above the requested size. The console line `...fullscreen <width>x<height>@<rate>Hz (exclusive|desktop)` reports what was actually applied; if no mode is large enough for the request, the engine says so and falls back to desktop fullscreen.

Practical setups:

- Exclusive fullscreen at desktop resolution:

```cfg
seta r_fullscreen "1"
seta r_mode "-2"
vid_restart
```

- Borderless desktop-sized window:

```cfg
seta r_fullscreen "0"
seta r_mode "-2"
seta r_noborder "1"
vid_restart
```

- Custom fixed window size:

```cfg
seta r_fullscreen "0"
seta r_mode "-1"
seta r_customWidth "1600"
seta r_customHeight "900"
vid_restart
```

### Window Title And Icon

The window and console titles name the running game, for example
`fnTech3 0.1.0 - Return to Castle Wolfenstein (Multiplayer)`; `+set cl_title`
on the command line replaces the title. The window, taskbar and Dock icon is
the running game's own application icon, remade at high resolution:

- `cl_gameIcon 1`: The built-in remake of the active game's icon. This is the default.
- `cl_gameIcon 2`: The installation's own icon where the platform can read one, otherwise the remake.
- `cl_gameIcon 0`: fnTech3's own icon.

`cl_gameIcon` takes effect at the next `vid_restart`.

### Fast Windowed/Fullscreen Toggle

`vid_restart fast` (alias: `vid_restart keep_window`) restarts the renderer while keeping the game window alive. When only the windowed/fullscreen state, borders, size, or position changed, the existing window is morphed in place instead of being destroyed and re-created, which avoids the desktop flicker and focus churn of a full restart:

```cfg
seta r_fullscreen "0"
vid_restart fast
```

Alt+Enter toggles `r_fullscreen` and issues `vid_restart fast` automatically.

The fast path is safe to use unconditionally: if the window cannot be reused — for example after changing `r_colorbits`, `r_stencilbits`, `r_depthbits`, `r_stereoEnabled`, HDR output requests, or switching between OpenGL and Vulkan renderers — the engine automatically falls back to a full window re-creation. The console prints `...reusing existing window` when the in-place toggle was taken.

## Framebuffer Path, Internal Resolution, And Anti-Aliasing

These settings control the render path behind the display output.

- `r_fbo`: Enables framebuffer-object rendering. This is the foundation for the modern display path and is required for bloom, motion blur, enhanced liquid refraction, optional global fog, HDR, multisample anti-aliasing, supersampling, greyscale, and arbitrary internal render resolutions.
- `r_hdr`: Selects the HDR-capable FBO render pipeline.
  - `0`: Display-referred SDR compatibility path.
  - `1`: High-precision FBO path. With the default legacy tone mapper this preserves each game's display-referred lighting; non-legacy tone mapping, color grading, and explicit HDR output use scene-linear color.
  - `-1`: Legacy debug alias for `r_hdrPrecision -1` without enabling scene-linear HDR.
- `r_hdrPrecision`: Controls the internal FBO color storage used by the display pipeline.
  - `0`: Automatic. SDR uses 8-bit normalized storage; `r_hdr 1` uses `RGBA16F` floating-point scene storage.
  - `-1`: Debug 4-bit storage for deliberate banding tests.
  - `8`: Force 8-bit storage.
  - `16`: Force 16-bit normalized storage for SDR/debug testing; `r_hdr 1` still uses `RGBA16F`.
- `r_hdrBloomFormat`: Controls scene-linear HDR bloom/extract intermediate storage without changing the main scene target.
  - `0`: Automatic. RGB bloom tries `R11G11B10F` and falls back to `RGBA16F` if the driver rejects the FBO.
  - `1`: Force `RGBA16F` bloom intermediates for conservative parity.
  - `2`: Prefer `R11G11B10F` for positive RGB bloom intermediates.
  - `3`: Prefer `RG16F` for positive two-channel intermediates; current RGB bloom safely falls back.
- `r_srgbTextures`: Allows authored color textures to use hardware sRGB decode when the `r_hdr 1` path is actually running scene-linear color. Lightmaps, fog, dynamic-light masks, and data textures remain linear/data.
- `r_framebufferSRGB`: Allows `GL_FRAMEBUFFER_SRGB` only when the draw target itself is sRGB-encoded. The current OpenGL/GLx SDR final shader keeps it disabled because the shader already writes SDR sRGB output.
- `r_outputBackend`: Selects the final hardware output backend.
  - `0`: Automatic. Conservative by default; Vulkan keeps using `r_hdrDisplay` to request HDR10, while GLx selects a non-SDR transform only when `r_hdr 1` and platform HDR/EDR state are visible.
  - `1`: Force SDR sRGB.
  - `2`: Request Windows scRGB output on an HDR/Advanced Color display path.
  - `3`: Request HDR10/PQ output. Vulkan maps this to an HDR10 swapchain when the surface exposes `VK_COLOR_SPACE_HDR10_ST2084_EXT`.
  - `4`: Request macOS extended-linear-sRGB/EDR output when SDL reports EDR headroom.
  - `5`: Request Linux experimental HDR telemetry/prototype output, gated by `r_outputAllowExperimentalLinuxHDR` plus explicit SDL compositor/protocol HDR checks.
- `r_outputAllowExperimentalLinuxHDR`: Allows the Linux experimental HDR telemetry/prototype backend only when the platform reports HDR headroom and an explicit compositor/protocol path. Leave this disabled unless you are validating a known HDR-capable Wayland path.
- On SDR output, GLx treats paper white as the max output reference for the final transform. HDR-only max-luminance headroom is used only when a hardware HDR/EDR backend is actually active.
- Output primaries are explicit in the GLx final transform. SDR sRGB and Windows scRGB use sRGB/BT.709 primaries, HDR10/PQ uses BT.2020, and macOS EDR uses Display P3. The Linux experimental HDR path is the only native-primaries state: it performs no primaries matrix and expects the compositor/protocol path to own native colorimetry. Unknown primaries are not selectable output states.
- `r_ext_multisample`: Geometry-edge multisample anti-aliasing. GLx and Vulkan share this cvar. Common values are `0`, `2`, `4`, `8`, and `16`; the renderer resolves unsupported requests to the best supported sample count not above the request.
- `r_ext_alpha_to_coverage`: Optional multisample smoothing for alpha-tested texture edges such as grates, foliage-style cutouts, and masked decals. Requires `r_ext_multisample`. Default is `0` for legacy alpha-test parity.
- `r_ext_supersample`: Enables supersample anti-aliasing.
- `r_renderWidth` and `r_renderHeight`: Internal render resolution when `r_renderScale > 0`.
- `r_renderScale`: Controls how the internal render image is scaled to the actual window or fullscreen size.
  - `0`: Disabled.
  - `1`: Nearest-neighbor stretch.
  - `2`: Nearest-neighbor with preserved aspect ratio.
  - `3`: Linear stretch.
  - `4`: Linear with preserved aspect ratio.

Guidance:

- Use `r_fbo 1` if you want any modern post-processing or internal render scaling.
- Use `r_ext_multisample` for cleaner geometry edges.
- Use `r_ext_alpha_to_coverage 1` with MSAA if alpha-tested texture edges need extra smoothing.
- Use `r_ext_supersample 1` when you want higher image quality and can afford the extra GPU cost.
- Use `r_renderWidth`, `r_renderHeight`, and `r_renderScale` when you want a lower or higher internal render resolution than the actual window size.

Example: render internally at `1280x720` and scale to your current display with linear filtering and preserved aspect ratio:

```cfg
seta r_fbo "1"
seta r_renderWidth "1280"
seta r_renderHeight "720"
seta r_renderScale "4"
vid_restart
```

## Scene Presentation Controls

These settings affect the rendered scene itself rather than the window mode.

- `r_gamma`: Gamma correction factor. This is one of the first settings to check if the whole frame looks too dark or too washed out.
- `r_ignorehwgamma`: Force the software/shader gamma path and never write the display's hardware ramp. Latched; requires `vid_restart`. Any nonzero value, including the `-1` Enemy Territory's own configs use, disables the hardware ramp.
- `r_mapOverBrightBits`: Overbright bits baked into the map's lighting. The default is each game's own: `1` for Elite Force, whose maps are lit for one bit, `0` for Alice, whose engine draws its maps' light as the map holds it, and `2` elsewhere. Latched.
- `r_tonemap`: Final-pass tone scale for `r_hdr 1`.
  - `0`: Legacy gamma/overbright behavior.
  - `1`: Simple Reinhard, the per-channel `x / (1 + x)` curve. Existing configs that referred to this as `Reinhard` keep the same behavior for this release cycle.
  - `2`: ACES-fitted filmic curve. This is the common fitted approximation, not the official ACES output transform; existing configs that referred to it as `ACES` keep the same behavior for this release cycle.
- `r_tonemapExposure`: Exposure multiplier for scene-linear tone mapping and bloom extraction.
- `r_autoExposure`: Automatic exposure for the shared OpenGL postprocess tone-map path.
  - `0`: Disabled.
  - `1`: Time-constant adaptation. This is the default; it updates from elapsed render time, clamps exposure inputs, and uses the deterministic luminance reduction path.
  - `2`: Legacy parity mode using the old frame-cadenced adaptation and GL_LINEAR reduction behavior.
  - `3`: Modern-tier histogram percentile reduction. This is opt-in; it carries low/median/high percentile luminance through the reduction chain on floating-point targets and falls back to mode `1` on older fixed-point tiers.
- `r_glxAutoExposure`: GLx scene-linear auto-exposure reduction.
  - `0`: Manual exposure from `r_tonemapExposure`. This is the default.
  - `1`: Tiered automatic mode: histogram percentile on modern post/output tiers, simple-average fallback on older tiers. The selected algorithm is reported by `glxpostprocess` and compact GLx diagnostics.
  - `2`: Force the simple-average fallback.
  - `3`: Force histogram percentile on modern tiers, with conservative fallback when the tier cannot support it.
- `r_glxAutoExposurePercentile`: Percentile used by the GLx histogram path. Default `80`.
- `r_glxAutoExposureTargetLuma`: Scene-linear luminance target for GLx auto exposure. Default `0.18`.
- `r_glxAutoExposureMin` / `r_glxAutoExposureMax`: Clamp the resolved GLx exposure. Defaults `0.125` and `8.0`.
- `r_glxAutoExposureAdapt`: Per-frame blend factor for GLx exposure changes. Default `0.15`; `1` applies the current reduction immediately.
- `r_colorGrade`: Scene-linear grading stage for `r_hdr 1`.
  - `0`: Disabled. This is the conservative default and preserves the compatibility image.
  - `1`: Lift/gamma/gain plus white-point adaptation.
  - `2`: 3D LUT atlas.
  - `3`: Lift/gamma/gain, white-point adaptation, then the 3D LUT atlas.
- `r_colorGradeLift`: Per-channel lift as `"r g b"` before tone mapping. Default is `"0 0 0"`.
- `r_colorGradeGamma`: Per-channel grading gamma as `"r g b"`. Default is `"1 1 1"`.
- `r_colorGradeGain`: Per-channel scene-linear gain as `"r g b"`. Default is `"1 1 1"`.
- `r_colorGradeWhitePoint`: Source scene white point in Kelvin for Bradford adaptation. Default is `6504`.
- `r_colorGradeAdaptWhitePoint`: Target white point in Kelvin. Default is `6504`.
- `r_colorGradeLUT`: Optional 3D LUT atlas image for `r_colorGrade 2` or `3`. The atlas layout is width `N*N`, height `N`, with blue slices arranged horizontally.
- `r_colorGradeLUTScale`: Scene-linear RGB range represented by the LUT. Default `4.0` maps `0..4` into the LUT domain.
- `r_greyscale`: Full-frame desaturation. Requires `r_fbo 1`.

`r_fovCorrection 1` fits the world FOV to the screen's shape in all renderers, keeping its vertical framing. It applies to every game written for a 4:3 screen. In Quake Live it completes the game's own Hor+, which fits only 4:3, 5:4, 16:10 and 16:9 screens. Set it to `0` for mods that already correct FOV, or for the game's FOV exactly as retail drew it. See [ASPECT_CORRECTION.md](ASPECT_CORRECTION.md) for HUD rules and presentation controls.

### Hardware Gamma Ramps

A display's gamma ramp is owned by the desktop, not by the game: it is global to
the monitor and survives the process. fnTech3 therefore takes it over only when
it has something to add that the renderer cannot do itself.

- With `r_fbo 1` (the default for every renderer) gamma is applied by the output
  shader, so the ramp is left alone and the monitor keeps its ICC calibration.
  Nothing changes on the desktop when you alt-tab.
- The ramp is written only in fullscreen, only when `r_fbo` is off and `r_gamma`
  or `r_overBrightBits` actually asks for a correction. It is handed back on
  focus loss, monitor changes, `vid_restart`, quit, and crash.
- Windows drops ramps while a mode set or ICC reload is in flight even after
  reporting success, so each write is confirmed by reading the ramp back, and
  the ramp is re-asserted after display changes and on returning to the game.
- `r_ignorehwgamma 1` disables the hardware path outright. Remote Desktop
  sessions and displays that report broken ramps are detected and skipped.

If the game or the desktop looks too dark after alt-tabbing, check `r_gamma`
first and confirm `r_fbo 1`; a monitor left on a stale ramp can also be reset by
reapplying its color profile from Windows display settings.

## Soft Particles

Soft particles use the scene depth buffer to fade supported translucent effects as they intersect world geometry. This softens the hard clipping that can otherwise appear where smoke, explosions, blood trails, and similar sprites meet floors, walls, or other map surfaces. The effect is visual-only: it does not change demos, protocol behavior, assets, or game logic.

- `r_depthFade`: Enables soft-particle depth fade for supported translucent shaders. Default is `1`. This is latched, so use `vid_restart` after changing it.

Supported shaders include the built-in Quake III explosion, smoke, plasma, bullet, rail, BFG, and blood effect shaders that fnTech3 marks for depth fade automatically, plus content shaders in any game that declare `q3map_depthFade <distance> <bias>`. Opaque shaders, depth-writing shaders, and blend modes that cannot be faded safely keep their normal behavior. While RTCW's or Enemy Territory's own fog is drawing, depth fade falls back to the plain path.

Renderer notes:

- OpenGL-lineage renderers use the depth-texture path for this effect.
- Vulkan uses the same kind of depth snapshot. If MSAA is requested, depth fade takes precedence and the main scene uses the single-sample depth-copy path.

Typical setup:

```cfg
seta r_depthFade "1"
vid_restart
```

## Dynamic Lighting And Shadowing

fnTech3 keeps each game's classic dynamic lights available, then adds optional higher-quality per-pixel dynamic lights and shadow maps on the GLx/OpenGL-lineage and Vulkan paths. These effects are visual-only renderer features: they do not change demos, protocol behavior, map data, or game logic. The defaults are the same in every game.

Dynamic lighting controls:

- `r_dynamiclight`: Enables dynamic lights such as weapon flashes and projectile lights. Default is `1`.
- `r_dlightMode`: Selects the dynamic-light renderer.
  - `0`: The game's classic dynamic lights: Quake III's projected lights, or RTCW's own.
  - `1`: High-quality per-pixel dynamic lights on world surfaces.
  - `2`: Same as `1`, and also applies dynamic lights to entity models.
  - The desktop default is `2`; constrained ARM builds retain the compatibility default `0`.
- `r_dlightScale`: Scales dynamic-light radius. Default is `0.5`, or `1.0` in RTCW single player, which also lifts the usual `0.1`--`1` range.
- `r_dlightIntensity`: Scales dynamic-light intensity without changing radius. Default is `1.0`.
- `r_dlightFalloff`: Blends the high-quality dynamic-light attenuation toward a smoother edge falloff. Default is `1`.
- `r_dlightSaturation`: Adjusts dynamic-light color saturation in linear light. Default is `0.8`.

Game notes:

- **RTCW and Enemy Territory:** the lights their modules place -- muzzle flashes, flames, explosions and forced lights -- also reach the per-pixel path, up to the modules' 32 lights per frame. RTCW sends them there at `r_dlightMode 1` or `2`, except at `r_dynamiclight 2` and through the snooper scope; ET does whenever `r_dynamiclight` is on. Lights the game forces still draw at `r_dynamiclight 0`. RTCW's darkening and flame lights never cast shadows.
- **Quake III Arena:** a map with the IHV Test's fog falls back to `r_dlightMode 0` in every renderer.

Classic model shadow controls:

- `cg_shadows`: Selects the classic player/model shadow mode in games whose client module offers it.
  - `0`: Disabled.
  - `1`: Classic blob/blur-style model shadows.
  - `2`: Stencil shadow volumes. This needs an available stencil buffer; `cl_stencilbits 8` is the usual compatible setting.
  - `3`: Black planar projection shadows.

Dynamic-light shadow-map controls:

- `r_dlightShadows`: Enables dynamic-light shadow-map planning, atlas rendering, and filtered sampling. Default is `1`. This is latched, so use `vid_restart` after changing it.
- `r_dlightShadowFilter`: Selects shadow filtering. Default is `2`.
  - `0`: Hard shadows.
  - `1`: 2x2 PCF.
  - `2`: Four-tap poisson PCF.
- `r_dlightShadowResolution`: Requested per-face shadow-map resolution. Valid range is `64..1024`, default is `1024`. The renderer rounds this down to a power of two and may reduce it to fit the atlas.
- `r_dlightShadowMaxLights`: Maximum dynamic lights allowed to cast shadows in one view. Default is `4`. Lower values give each light more atlas space; higher values favor coverage over sharpness.
- `r_dlightShadowStrength`: Controls how strongly shadow-map occlusion dims the dynamic light. Default is `0.95`.
- `r_dlightShadowBias`: Receiver bias in world units. Default is `4`.
- `r_dlightShadowCasterDepthBias`: Constant depth bias while rendering shadow casters. Default is `1`.
- `r_dlightShadowCasterSlopeBias`: Slope-scaled caster depth bias. Default is `1`.
- `r_dlightShadowCasterNormalBias`: Light-aware caster normal offset in world units. Default is `0.25`.
- `r_dlightShadowDebug`: Prints dynamic-light shadow planning and atlas counters. Use this or `r_speeds 4` when you want to confirm the effective atlas size and per-face resolution.
- `r_muzzleFlashDlightOffset`: Moves recognized weapon muzzle-flash lights along the submitted flash model's forward axis, in world units. Default is `8`; use `0` for the retail origin.
- `r_muzzleFlashDlightShadows`: Allows recognized weapon muzzle-flash lights to cast shadows. Default is `1`; set it to `0` to keep the light while disabling only its shadows.
- `r_spotShadows`: Enables the spotlight shadow atlas used by spot lights from static light files and surface-light proxies. Default is `0`; latched.

Shadow casters are opaque world surfaces and curves, and MD3, MDR, IQM, and RTCW's MDC and MDS models; alpha-tested and deformed shaders do not cast. Enemy Territory's skeletal models and the TIKI models of Alice and F.A.K.K.2 do not cast shadow-map shadows yet.

Quality-first dynamic-light shadow-map setup:

```cfg
seta r_dynamiclight "1"
seta r_dlightMode "2"
seta r_dlightShadows "1"
seta r_dlightShadowFilter "2"
seta r_dlightShadowResolution "1024"
seta r_dlightShadowMaxLights "2"
vid_restart
```

If the debug output reports a smaller face size than requested, reduce `r_dlightShadowMaxLights` before increasing any bias values. Bias tuning is mainly for fixing acne, shimmering, or detached-looking shadows; the defaults are the best first pass for normal play.

Directional cascaded shadow maps are available with `r_csmShadows 1` on GLx and Vulkan in any game whose map has a sky sun. They use parsed `q3map_sun`/`q3map_sunExt`/`q3map_sunExt2` sky shader parameters for the sun direction, color, and strength, and shadow opaque BSP world geometry, entity models, and brush models.

## Static Map Lights

Maps are lit by baked lightmaps, which a moving model cannot cast a shadow into. Static map lights add a map's own light sources back as per-pixel lights, so models are lit by them and, within the shadow budget, cast shadows from them. They come from `maps/<map>.lights.json` sidecar files, which only the renderer reads: they change no map data, collision, demos, or game logic.

fnTech3's engine package ships a light file, generated from the light sources compiled into the map itself, for every stock map of:

- Return to Castle Wolfenstein: all 32 single-player and 15 multiplayer maps, each fitted for its own mode;
- American McGee's Alice: all 36 maps;
- Heavy Metal: F.A.K.K.2: all 30 maps;
- Medal of Honor: Allied Assault: all 54 maps, each file beside its map in the folder its pak keeps it in.

A file in the engine package takes precedence over one in the game or a mod, and a mod can supply files for maps without one. No light files ship for Quake III Arena, Quake Live, Enemy Territory, or Elite Force; Enemy Territory's stock maps have no light entities to convert.

- `r_staticLights`: Enables static map lights. Default is `1`.
- `r_staticLightMaxLights`: Maximum static lights promoted into one scene. Default is `8`.
- `r_staticLightShadows`: Lets eligible static point lights enter the dynamic-light shadow planner. Default is `1`. Shadows also need `r_dlightShadows 1` and `r_dlightMode 1` or `2`.
- `r_staticLightShadowMaxLights`: Maximum static lights allowed to cast shadows in one scene. Default is `2`.
- `r_staticLightDebug`: Prints loading and promotion counters. Cheat-protected.
- `r_staticLightReload`: Reloads the current map's light file.

A light file is JSON: `{"version": 1, "lights": [ ... ]}`, where each light has a `type` of `point` or `spot`, an `origin`, a `color`, an `intensity` and a `radius`, plus a `direction` and cone angles for spots, and optional `castsShadows`, `priority` and `style` fields. Unknown keys are ignored, and a map holds at most 128 lights. Spot lights cast shadows only with `r_spotShadows 1`.

Surface-light proxies are a separate, optional source: `r_surfaceLightProxies 1` promotes the map's non-sky `q3map_surfaceLight` surfaces into proxy lights, with `r_surfaceLightProxyMaxLights` (default `4`), `r_surfaceLightProxyShadows` (default `0`) and `r_surfaceLightProxyShadowMaxLights` (default `1`).

## Texture Picmip

Picmip lowers texture upload resolution to trade sharpness for memory use and speed. It is a latched texture-loading setting, so changes require `vid_restart` or a fresh renderer start before already-loaded images are rebuilt.

Controls:

- `r_picmip`: Texture reduction amount. `0` keeps full upload resolution. Higher values progressively shift eligible images down by powers of two.
- `r_picmip2`: RTCW single player only: the separate character-texture reduction its original engine had. `0` keeps full resolution.
- `r_picmipFilter`: Filters which shader paths are allowed to use `r_picmip`. Default is `1`.
  - `0`: Legacy behavior. Any picmip-capable shader or raw image can be reduced unless it was explicitly marked `nopicmip`.
  - `1`: Allow `textures/*`. This is the default and keeps normal world-material textures under picmip while treating most UI, model, effect, and utility shaders like `nopicmip`.
  - `2`: Allow `models/*`.
  - `4`: Allow `sprites/*`.
  - `8`: Allow 2D/UI-style paths: `gfx/*`, `icons/*`, `menu/*`, `ui/*`, and `fonts/*`.
  - Add values to combine groups. For example, `3` allows `textures/*` and `models/*`; `15` allows every built-in group.
- `r_nomip`: Applies picmip only while map/world images are loading. This is stricter than `r_picmipFilter` because it limits when picmip is applied, not only which shader names are eligible.
- `r_neatsky`: Disables mipmapping and picmip for skybox images.

Notes:

- `r_picmipFilter` is intentionally shader-name based. It classifies the shader being registered, not every individual image referenced by a shader stage. A shader named `textures/foo/bar` can still picmip its eligible stages, even if one stage loads an image outside `textures/`.
- Authored shader keywords still win. `nopicmip` and `nomipmaps` keep their historical meaning.
- Use `r_picmipFilter 0` for old configs or visual comparisons that expect every picmip-capable image to be reduced.
- Use `r_picmipFilter 1` when you want world textures to scale down without blurring HUD, menu, font, and most model/effect assets.
- Use `r_picmipFilter 3` when you also want player, weapon, and item model skins to follow `r_picmip`.

Example quality/performance setup:

```cfg
seta r_picmip "1"
seta r_picmipFilter "1"
vid_restart
```

Example aggressive legacy-style setup:

```cfg
seta r_picmip "2"
seta r_picmipFilter "0"
vid_restart
```

## Cel Shading

Cel shading is split between model presentation and BSP world edge detection.

Model cel shading applies to model entities, including player models, weapons, and inline brush models. It can quantize model lighting into hard bands and draw a stencil-style silhouette shell around eligible models. World cel shading is separate: it draws screen-space outlines from the world depth buffer so BSP geometry gets dark edge accents without changing lightmaps, vertex data, demos, protocol behavior, or assets.

The GLx/OpenGL-lineage and Vulkan renderers expose the same user-facing controls.

### Controls

- `r_celShading`: Model cel-shading master toggle. Default is `0`.
  - `0`: Disabled.
  - `1`: Enable cel shading on model entities, including inline brush models.
- `r_celShadingModelShadows`: Quantizes model lighting into cel shadow bands when `r_celShading` is enabled. Default is `1`.
- `r_celViewWeapon`: Lets the first-person weapon participate in model cel shading and model cel outlines. Default is `1`.
- `r_celShadingSteps`: Number of diffuse lighting bands for model cel shading. Valid range is `2..8`, default is `4`. Lower values push a harsher hard-step look. Higher values keep more intermediate tone.
- `r_celOutline`: Enables the silhouette shell around cel-shaded model entities. Default is `1`.
- `r_celOutlineScale`: Model outline shell expansion amount. Valid range is `1.0..1.25`, default is `1.03`. Values just above `1.0` keep the outline tight. Larger values make it thicker.
- `r_celOutlineAlpha`: Model outline opacity multiplier. Valid range is `0.0..1.0`, default is `1.0`.
- `r_celViewWeaponOutlineScale`: First-person weapon outline shell expansion amount. Valid range is `1.0..1.10`, default is `1.006`. This is separate because viewmodels sit much closer to the camera than other models.
- `r_celViewWeaponOutlineAlpha`: First-person weapon outline opacity multiplier. Valid range is `0.0..1.0`, default is `1.0`.
- `r_celOutlineColor`: Model outline color as `"r g b a"`. Default is `"0 0 0 255"`. The alpha channel is also multiplied by `r_celOutlineAlpha` or `r_celViewWeaponOutlineAlpha`.
- `r_celShadingWorld`: Enables BSP world cel outlines. Default is `0`.
  - `0`: Disabled.
  - `1`: Draw screen-space depth-edge outlines over opaque BSP world geometry.
- `r_celShadingWorldWidth`: World outline radius in pixels. Valid range is `1.0..8.0`, default is `2.0`. Increase this for thicker world edges.
- `r_celShadingWorldAlpha`: World outline opacity. Valid range is `0.0..1.0`, default is `1.0`.
- `r_celShadingWorldDepthThreshold`: Depth discontinuity threshold for world edge detection. Valid range is `0.0001..0.02`, default is `0.0015`. Lower values catch subtler depth edges; higher values restrict the effect to stronger geometry breaks.

Notes:

- `r_celOutline` depends on a stencil buffer being available.
- `r_celShadingWorld` uses depth edges only. It does not quantize or cel-shade BSP lightmaps.
- `r_celShadingWorld` affects the main BSP world only. Inline brush models remain under `r_celShading`.
- `r_celShadingWorld` needs the renderer depth-texture path. If that path is unavailable, the cvar is harmless and the world outline pass is skipped.
- Model cel outlines, world cel outlines, and player highlight passes are drawn late enough to remain visible over bloom.
- The model outline color is shared across eligible model entities.

Recommended starting point:

```cfg
seta r_celShading "1"
seta r_celShadingModelShadows "1"
seta r_celViewWeapon "1"
seta r_celShadingSteps "4"
seta r_celOutline "1"
seta r_celOutlineScale "1.03"
seta r_celOutlineAlpha "1.0"
seta r_celViewWeaponOutlineScale "1.006"
seta r_celViewWeaponOutlineAlpha "1.0"
seta r_celOutlineColor "0 0 0 255"
seta r_celShadingWorld "1"
seta r_celShadingWorldWidth "2.0"
seta r_celShadingWorldAlpha "1.0"
seta r_celShadingWorldDepthThreshold "0.0015"
```

For a harsher stylized model-lighting look, reduce `r_celShadingSteps` to `2` or `3`. For a softer result, keep the outline enabled but raise `r_celShadingSteps` to `5` or `6`. For stronger world edges, raise `r_celShadingWorldWidth`; for fewer world edges, raise `r_celShadingWorldDepthThreshold`.

## Map Flares, Coronas And Lens Effects

Maps can author flare surfaces as part of the BSP, as many Quake III and Quake Live maps do. fnTech3 keeps the original visibility, fog, distance falloff, and fade behavior, while offering an enhanced optical presentation on GLx/OpenGL and Vulkan.

- `r_flares 0`: Disable map flares. This is the default outside RTCW.
- `r_flares 1`: Draw the classic Quake III corona only. Existing configurations retain their original presentation.
- `r_flares 2`: Draw the classic corona and supplement it with a broad source halo, a restrained horizontal lens streak, and chromatic aperture ghosts along the light-to-screen-centre axis.

The enhanced effect reuses the retail flare image through a dedicated additive surface, so black texels stay neutral and no replacement assets or map-format change are required. It also reuses the classic flare's occlusion and fade result; hidden lights cannot leave detached lens ghosts behind. Bloom is optional, but enabling it can make the brightest corona layers spread more naturally.

Game notes:

- **RTCW** draws its own coronas instead of map flare surfaces, as its original engine did, and `r_flares` takes that engine's meanings: `1` (the default) draws the coronas the game places, `2` draws a corona on every dynamic light, and `3` draws both. `r_flareFade` defaults to `5` there, and `10` elsewhere.
- **Enemy Territory** uses the same corona path, but its `r_flares` default is `0`, so its coronas are off until you enable them.
- **Alice and F.A.K.K.2** draw their maps' sun flares and light coronas: `r_sunflare`, `r_place_sunflare` and `r_sunflare_inportalsky` are the original engine's sun-flare controls, and `r_lightcoronasize` (default `0.1`) scales the coronas.

## Enhanced Liquids

The OpenGL-lineage renderers, including GLx, and Vulkan can add warped same-frame scene-color refraction, a Fresnel-weighted screen-space reflection, and visual interaction ripples to existing liquid surfaces. Warped scene color is drawn behind each qualifying transparent water face before the map's authored stages; those stages still provide their original tint, scroll, animation, blend, and deformation. After the authored stages, a grazing-angle reflection pass mirrors the same captured pre-transparency snapshot back onto the surface — it never resamples the live color buffer, so the duplicated and smeared camera-image feedback produced by the earlier experimental path cannot occur. Enhancements and ripples both default to off, so existing maps, configurations, and demos keep the classic presentation unless you opt in.

- `r_liquid` selects qualifying liquids from their shader contents. The default `0` disables the enhancement, `1` enhances shaders marked as water, and `2` also enhances shaders marked as slime or lava with more subdued material-specific strengths. This setting is latched.
- `r_liquidResolution` sets the snapshot resolution from `0.25` to `1.0`; the default is `1.0`, which samples the scene at full resolution and keeps warped edges crisp. Lower values reduce bandwidth but soften the refraction and can make displaced high-contrast edges crawl. This setting is latched.
- `r_liquidRefraction` controls the opacity of warped scene color behind the authored transparent stages, from `0.0` to `1.0`. The default is `0.65`; lower values retain more of the original destination.
- `r_liquidWarpScale` multiplies the ambient wave distortion, from `0.0` to `2.0` with a default of `1.0` (about `12` pixels at 1080 lines). The distortion is scaled to the view height so it keeps the same angular size at every resolution, and it fades with eye distance and grazing angle so distant or near-horizon liquid does not shimmer.
- `r_liquidReflection` controls the strength of the grazing-angle reflection pass, from `0.0` to `1.0`. The default is `0.65`; `0` completely skips the pass. Where the mirrored sample would land off screen or behind the camera, the pass falls back to a flat material-coloured sheen.
- `r_liquidRipples` sets the amplitude of visual disturbances from players and missiles entering, leaving, or moving through liquid, from `0.0` to `2.0`. The default is `1.0`; `0` disables the impulse feed entirely. A nonzero `r_liquid` mode is required.

When scene depth is available (it is by default; see `r_depthFade`), the refraction rejects warped samples that land on foreground geometry, which keeps the waterline crisp instead of smearing rims and ledges into the water. The reflection is a bounded single-tap screen-space approximation of the captured view, not a second mirrored world render.

These names replace the older `r_liquidReflections`, `r_liquidReflectionScale`, `r_liquidWarp`, `r_liquidFresnel`, and `r_liquidRippleStrength` cvars, which are no longer read.

The effect requires `r_fbo 1`. Changing `r_liquid` or `r_liquidResolution` reallocates the snapshot resource, so issue `vid_restart` after changing either one. Refraction, warp, reflection, and ripples can be tuned live. A balanced starting point is:

```cfg
seta r_fbo "1"
seta r_liquid "1"
vid_restart
```

This is an intentionally inexpensive screen-space effect, not a second mirrored world render, a depth-ray-marched SSR, or a fluid simulation. Both the refraction and the reflection can only reuse the same-frame color already captured from the current view: objects outside the screen, behind the camera, occluded, or drawn later in the transparent sort cannot appear in either, and the reflection projects each mirrored ray to a single proxy distance rather than searching the depth buffer for a true hit. Warp and reflection fade near screen edges, and the reflection cleanly falls back to a material sheen where its sample is invalid. Unusually early custom sorts, alpha-tested liquid stages, depth-fade or line-mode stages, and nonstandard depth-test modes retain their fully authored appearance because a geometry-only underlay cannot reproduce their coverage safely. Ripples are bounded visual responses to player and missile motion with amplitude independent of ambient warp; they distort the sampled image but do not deform the liquid mesh or change collision, buoyancy, player movement, projectile paths, networking, game logic, or demo state. If the framebuffer path or private liquid snapshot is unavailable, the original authored liquid remains visible.

Game notes: the ripple impulses come from the Quake III-style client-game path, which Quake III Arena, Quake Live and Elite Force use; RTCW, Enemy Territory, Alice and F.A.K.K.2 do not feed it, so their liquids get refraction and reflection without interaction ripples. RTCW's and ET's own water ripples are drawn separately, as their original engines drew them. While RTCW's or ET's fog is drawing, enhanced liquids fall back to the plain path.

For the pass ordering, backend tiers, dedicated snapshot, and compatibility invariants, see [Liquid Rendering](fnql/LIQUID_RENDERING.md).

## Underwater View

While the camera is under water, slime, or lava, the OpenGL-lineage renderers, including GLx, and the Vulkan renderers can composite a submerged view over the finished scene: an animated screen warp with a slight colour separation, a darkened periphery, and a medium tint that deepens with distance. It is the view-side counterpart to Enhanced Liquids above — that setting draws water seen from outside, this one draws the world seen from inside it — and the two are independent. The layer defaults to off, so existing configurations and demos keep the classic presentation unless you opt in.

- `r_underwater` enables the layer for all three liquid types. The default is `0`. This setting is latched.
- `r_underwaterWarp` multiplies the wave distortion, from `0.0` to `2.0` with a default of `1.0` (about `11` pixels at 1080 lines before the liquid's own multiplier). The distortion is scaled to the view height so it keeps the same angular size at every resolution. `0` flattens the warp.
- `r_underwaterDispersion` sets how far the red and blue channels separate across the warp, from `0.0` to `1.0` with a default of `0.35`. `0` keeps all three channels aligned.
- `r_underwaterFog` scales the distance tint, from `0.0` to `1.0` with a default of `1.0`. The colour and how quickly it closes in follow the liquid: water stays readable at short range, slime loses the scene within a room, and lava is nearly opaque. `0` disables the tint.
- `r_underwaterVignette` sets the edge darkening, from `0.0` to `1.0` with a default of `0.35`. `0` leaves the periphery at full brightness.

Entering and leaving a liquid ramps the whole layer over about a fifth of a second, so a surface crossing does not pop. The HUD and console are drawn after the layer and stay sharp. The distance tint needs a usable depth texture; without one the warp and edge darkening still apply.

The layer learns that the view is submerged from the Quake III-style client-game path, which Quake III Arena, Quake Live and Elite Force use. RTCW, Enemy Territory, Alice and F.A.K.K.2 do not send that signal, so the layer stays off in those games.

The effect requires `r_fbo 1`. Changing `r_underwater` allocates renderer resources, so issue `vid_restart` after changing it. The other four settings can be tuned live. A balanced starting point is:

```cfg
seta r_fbo "1"
seta r_underwater "1"
vid_restart
```

This is a post-process on the frame that has already been rendered, not a refracted re-render of the world. It bends and tints what was visible rather than revealing anything new, the screen border is stretched rather than filled, and there are no caustics, light shafts, or bubbles. Crossing the waterline switches the medium at the eye instead of splitting the view into above-surface and below-surface halves. Nothing about the layer touches collision, buoyancy, movement, projectile paths, networking, game logic, or demo state.

For the submersion feed, pass ordering, backend tiers, and compatibility invariants, see [Underwater View](fnql/UNDERWATER_VIEW.md).

## Fog

### RTCW And Enemy Territory Fog

Return to Castle Wolfenstein draws the distance fog its maps and game scripts set, including sky fog and fog the game changes during play. Enemy Territory's whole-map fog, which its game module can retune, goes through the same path; its on-screen result has not yet been confirmed.

- `r_wolffog`: Draws that fog. Default is `1`; `0` turns RTCW's and ET's scene and sky fog off.

### Optional Global Fog

Map authors and mods can add a visual-only, depth-aware atmospheric layer with a `maps/<map>.fog` sidecar. fnTech3's engine package ships conservative fog presets for Quake Live's stock maps and for Quake III Arena and Team Arena maps, and a mod can supply one for any game. The feature does not replace authored BSP fog or affect visibility, collision, game state, networking, demos, or module ABIs.

- `r_globalFog`: Enables sidecar loading and composition on the OpenGL-lineage and Vulkan renderers, and on RTX with `r_fbo 1` and no multisampling. The default is `0`; changing it requires `vid_restart`.
- `r_globalFogStrength`: Live opacity multiplier from `0.0` to `1.0`, with a default of `1.0`.
- `r_fbo 1` and a usable depth texture are required. Missing or invalid sidecars and optional-shader failures disable only the fog layer; they never prevent `glx`, `vk`, or `rtx` from starting.

See [Global Fog Sidecars](fnql/GLOBAL_FOG.md) for the bounded file format, search precedence, curve definitions, and fallback contract.

### Alice And F.A.K.K.2 Far Plane

Ritual's engine gives each level a far plane: past a set distance the view fades into a fog colour and stops drawing. fnTech3 draws it as the level sets it, and keeps the original engine's overrides:

- `r_farplane`: Overrides the level's far-plane distance when nonzero; `r_farplane_color` sets the fog colour and `r_farplane_nocull 1` keeps drawing beyond it.
- `r_farplane_nofog`: Turns the far plane's distance fog off.
- `r_farplane_pulse` and `r_farplane_pulseFrequency`: Pull the far plane in and out along a sine wave.
- `r_skyportal` and `r_skyportal_origin`: Draw the level's sky portal from another origin.

## Bloom

Bloom extracts bright areas from the rendered frame, blurs them through a downsampled chain, and blends the result back over the original image. It is a post-processing effect, so it depends on the framebuffer path.

### Requirements

- `r_fbo 1` is required.
- OpenGL and Vulkan both support bloom.
- OpenGL exposes the largest bloom control set. GLx preserves that same control surface as the canonical OpenGL-lineage renderer.
- Vulkan currently exposes the shared extraction and intensity controls, but not the OpenGL-only shape controls.

### Shared Bloom Controls

These settings are available in both renderers.

- `r_bloom`: Master bloom toggle.
  - On OpenGL, use `0`, `1`, or `2`.
    - `0`: Disabled.
    - `1`: Bloom the rendered 3D scene.
    - `2`: Also let bloom affect 2D and HUD elements drawn before the final post-process pass.
  - On Vulkan, treat `r_bloom` as `0` or `1`.
- `r_bloom_threshold`: Brightness cutoff for extraction. Higher values restrict bloom to stronger highlights. Lower values let more of the frame glow.
- `r_bloom_threshold_mode`: How brightness is measured.
  - `0`: Trigger when any color channel reaches the threshold.
  - `1`: Trigger when the average of `r`, `g`, and `b` reaches the threshold.
  - `2`: Trigger using luma weighting. This is usually the cleanest and most predictable mode.
- `r_bloom_modulate`: How strongly extracted color is biased toward already-bright areas.
  - `0`: Leave extracted color unchanged.
  - `1`: Multiply the color by itself for a more aggressive highlight push.
  - `2`: Multiply the color by its luma for a cleaner brightness-weighted result.
- `r_bloom_intensity`: Final bloom blend strength.
- `r_flareSceneLinear`: In the `r_hdr 1` scene-linear path, flare vertex colors are decoded to linear RGB before bloom extraction. Set this to `0` only when comparing against the legacy display-referred flare behavior.

Recommended tuning order:

1. Enable `r_bloom 1`.
2. Set `r_bloom_threshold_mode 2`.
3. Adjust `r_bloom_threshold` until only the highlights you want are being extracted.
4. Adjust `r_bloom_modulate` if you want a tighter or more contrast-driven response.
5. Adjust `r_bloom_intensity` last.

### GLx Bloom Controls

These settings are specific to the canonical OpenGL-lineage renderer, `glx`.

- `r_bloom_passes`: Number of downsampled bloom levels used in the effect. More passes generally create a wider haze and cost more GPU time. The engine may clamp the effective chain length based on hardware limits or very small internal render sizes.
- `r_bloom_blend_base`: Which downsampled level to start blending from. Higher values skip the tighter levels and bias the result toward a broader, softer haze.
- `r_bloom_filter_size`: Blur filter size per bloom level. Higher values widen the blur and cost more.
- `r_bloom_reflection`: Lens reflection effect intensity.
  - Positive values add the reflection on top of the main bloom.
  - Negative values keep only the reflection path and skip the main bloom texture.

Practical interpretation:

- Increase `r_bloom_passes` if you want bloom to spread farther from bright sources.
- Increase `r_bloom_blend_base` if you want less tight glow and more broad atmosphere.
- Increase `r_bloom_filter_size` if the bloom still feels too sharp.
- Use `r_bloom_reflection` carefully. It is a stylized effect and becomes obvious quickly.

### Bloom Tuning Recipes

Subtle highlight bloom:

```cfg
seta r_fbo "1"
seta r_bloom "1"
seta r_bloom_threshold "0.75"
seta r_bloom_threshold_mode "2"
seta r_bloom_modulate "2"
seta r_bloom_intensity "0.25"
```

Balanced bloom for bright maps and effects:

```cfg
seta r_fbo "1"
seta r_bloom "1"
seta r_bloom_threshold "0.60"
seta r_bloom_threshold_mode "2"
seta r_bloom_modulate "2"
seta r_bloom_intensity "0.45"
```

GLx haze-heavy bloom:

```cfg
seta cl_renderer "glx"
seta r_fbo "1"
seta r_bloom "1"
seta r_bloom_threshold "0.55"
seta r_bloom_threshold_mode "2"
seta r_bloom_modulate "2"
seta r_bloom_intensity "0.45"
seta r_bloom_passes "6"
seta r_bloom_blend_base "2"
seta r_bloom_filter_size "8"
vid_restart
```

GLx HUD-inclusive bloom:

```cfg
seta cl_renderer "glx"
seta r_fbo "1"
seta r_bloom "2"
seta r_bloom_threshold "0.65"
seta r_bloom_threshold_mode "2"
seta r_bloom_intensity "0.35"
```

GLx lens reflection add-on:

```cfg
seta cl_renderer "glx"
seta r_fbo "1"
seta r_bloom "1"
seta r_bloom_threshold "0.60"
seta r_bloom_threshold_mode "2"
seta r_bloom_intensity "0.40"
seta r_bloom_reflection "0.25"
```

### Bloom Troubleshooting

If bloom seems too weak:

- Lower `r_bloom_threshold`.
- Use `r_bloom_threshold_mode 0` or `2`.
- Raise `r_bloom_intensity`.
- On OpenGL or GLx, increase `r_bloom_passes` or `r_bloom_filter_size`.

If bloom is making the whole frame look milky:

- Raise `r_bloom_threshold`.
- Lower `r_bloom_intensity`.
- Use `r_bloom_threshold_mode 2`.
- On OpenGL or GLx, lower `r_bloom_passes`, lower `r_bloom_filter_size`, or reduce `r_bloom_blend_base`.

If HUD elements are glowing and you do not want that:

- On OpenGL or GLx, use `r_bloom 1` instead of `r_bloom 2`.

If nothing happens at all:

- Confirm `r_fbo 1`.
- Confirm bloom is actually enabled with `r_bloom`.
- Lower `r_bloom_threshold` to test.
- If you just changed renderer, display mode, or other latched video settings, use `vid_restart`.

## Motion Blur

The OpenGL-lineage renderers, including GLx, and Vulkan provide optional camera-driven screen motion blur. After the completed world view, the renderer measures camera rotation and lateral movement and applies a bounded seven-tap directional kernel to the current scene before bloom, tone mapping, and output encoding. Sampling is clamped to the 3D view rectangle, including when `cg_viewsize` is reduced. It does not blend previous-frame pixels, so a stationary view is unchanged and moving entities cannot leave persistent trails.

- `r_motionBlur`: Master toggle. The default is `0` (disabled), and `r_fbo 1` is required.
- `r_motionBlurStrength`: Camera-motion shutter scale from `0.0` to `1.0`. The default `0.25` is deliberately subtle; higher values increase the directional blur radius.
- HUD, console, and other 2D drawing are always added after motion blur and remain sharp. `r_hudExcludePostProcess` continues to control the separate bloom/3D-HUD behavior.

Recommended subtle setup:

```cfg
seta r_fbo "1"
seta r_motionBlur "1"
seta r_motionBlurStrength "0.25"
seta r_hudExcludePostProcess "1"
```

The blur is skipped below a sub-pixel motion threshold and capped at a 32-pixel radius, preventing needless work during stationary play and pathological smearing during fast turns. Long frame stalls, minimize/restore, stereo rendering, large camera cuts, and teleports reset the camera sample. GLX allocates its scratch buffer lazily when motion first needs it and can toggle the effect live. Vulkan allocates its scratch image at renderer initialization, so changing `r_motionBlur` there requires `vid_restart`.

## Game-Specific Renderer Settings

A few renderer settings exist only in the game whose engine had them, with that engine's defaults:

| Game | Setting | Default | Effect |
| --- | --- | --- | --- |
| RTCW (both modes) | `r_flares` | `1` | Coronas; see [Map Flares, Coronas And Lens Effects](#map-flares-coronas-and-lens-effects). |
| RTCW (both modes) | `r_flareFade` | `5` | Corona fade distance; `10` in other games. Cheat-protected. |
| RTCW single player | `r_picmip2` | `0` | Character-texture picmip. |
| RTCW single player | `r_dlightScale` | `1.0` | Dynamic-light radius scale, `0.5` in other games. |
| RTCW and Enemy Territory | `r_wolffog` | `1` | The games' own fog. |
| Enemy Territory | `r_drawfoliage` | `1` | Draws foliage models. Cheat-protected. |
| Elite Force | `r_mapOverBrightBits` | `1` | Map overbright bits, `2` in other games. |
| Alice | `r_mapOverBrightBits` | `0` | Map overbright bits; Alice's engine draws each map's light unshifted. |
| Alice and F.A.K.K.2 | `r_overBrightBits` | `0` | Overbright bits, `1` in other games. Latched. |
| Alice and F.A.K.K.2 | `r_shaderlod` | `0.5` | Keeps a shader's `if shaderlod` blocks above this level; applies when shaders load. |
| Alice and F.A.K.K.2 | `r_detailtextures` | `0` | Detail shader stages; `1`, and latched, in other games. |
| Alice and F.A.K.K.2 | `r_lodscale` | `5` | Model level-of-detail scale, adjustable without cheats as in the original engine. |
| Alice and F.A.K.K.2 | `r_farplane`, `r_skyportal` | `0` | See [Alice And F.A.K.K.2 Far Plane](#alice-and-fakk2-far-plane). |
| Alice and F.A.K.K.2 | `r_sunflare`, `r_lightcoronasize` | `0`, `0.1` | Sun flares and light coronas. |
| Alice and F.A.K.K.2 | `r_ritualModelLights` | `1` | Lights the characters and weapons the game flags from the map's compiled lights, as the original engines do; `0` lights every model from the light grid. `r_ritualModelLightsDebug` (cheat-protected) prints what each one takes. |
| Alice and F.A.K.K.2 | `r_ambientScale` | `0.5` | Scales the light grid's ambient on the models it lights, as the original engines do; `0.6` in Quake III and the other games. Cheat-protected. |
| Quake Live | `r_ambientScale` | `10` | The same scale, a player option from `1` to `100` as in the original: the Video page's Ambient Light Scale. Kept in `repconfig.cfg`. |
| Quake III IHV Test | `r_lerpmodels` | `1` | Model frame interpolation. |

## When To Use `vid_restart`

Use `vid_restart` after changes to:

- `cl_renderer`
- `r_mode`, `r_modeFullscreen`, `r_fullscreen`
- `r_customWidth`, `r_customHeight`, `r_customPixelAspect`
- `r_displayRefresh`
- `r_noborder`
- `r_fbo`
- `r_hdr`
- `r_hdrPrecision`
- `r_ext_multisample`
- `r_ext_alpha_to_coverage`
- `r_depthFade`
- `r_renderWidth`, `r_renderHeight`, `r_renderScale`
- `r_ext_supersample`
- `r_dlightShadows`, `r_spotShadows`, `r_csmShadows`
- `r_mapOverBrightBits`, `r_picmip`, `r_picmip2`
- `cl_gameIcon`
- OpenGL or GLx `r_bloom_passes`
- OpenGL or GLx `r_hdrBloomFormat`
- Vulkan `r_bloom`
- Vulkan `r_motionBlur`
- `r_liquid` or `r_liquidResolution`
- `r_underwater`
- `r_globalFog`

For pure window-state changes (`r_fullscreen`, `r_mode`, `r_noborder`, window size or position), prefer `vid_restart fast`: it applies the change on the existing window when possible and falls back to a full restart automatically. See [Fast Windowed/Fullscreen Toggle](#fast-windowedfullscreen-toggle).

Settings that are usually safe to tune live:

- `r_swapInterval`
- `r_gamma`
- `r_greyscale`
- `r_bloom_threshold`
- `r_bloom_threshold_mode`
- `r_bloom_soft_knee`
- `r_bloom_modulate`
- `r_bloom_intensity`
- `r_motionBlurStrength`
- `r_tonemap`
- `r_tonemapExposure`
- `r_colorGrade`
- `r_colorGradeLift`
- `r_colorGradeGamma`
- `r_colorGradeGain`
- `r_colorGradeWhitePoint`
- `r_colorGradeAdaptWhitePoint`
- `r_colorGradeLUT`
- `r_colorGradeLUTScale`
- `r_celShading`
- `r_celShadingModelShadows`
- `r_celViewWeapon`
- `r_celShadingSteps`
- `r_celOutline`
- `r_celOutlineScale`
- `r_celOutlineAlpha`
- `r_celViewWeaponOutlineScale`
- `r_celViewWeaponOutlineAlpha`
- `r_celOutlineColor`
- `r_celShadingWorld`
- `r_celShadingWorldWidth`
- `r_celShadingWorldAlpha`
- `r_celShadingWorldDepthThreshold`
- `r_staticLights` (use `r_staticLightReload` after editing a light file)
- `r_wolffog`, `r_farplane` and its companions
- OpenGL or GLx `r_bloom_blend_base`
- OpenGL or GLx `r_bloom_filter_size`
- OpenGL or GLx `r_bloom_reflection`

If a live change does not seem to take effect immediately, `vid_restart` is the safe fallback.

## Related Guides

- [ASPECT_CORRECTION.md](ASPECT_CORRECTION.md) for FOV, HUD, menu, and cinematic presentation.
- [VISUALS.md](VISUALS.md) for player highlighting and other visual presentation controls.
- [SCREENSHOTS.md](SCREENSHOTS.md) for screenshot and capture output options.
- [RTX.md](RTX.md) for the path tracer, which lights each map from the same light entities as the static map lights.
