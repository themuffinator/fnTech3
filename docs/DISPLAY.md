# Graphics Guide

This guide covers how fnTech3 draws your game: the window or fullscreen mode,
sharper edges, brightness, lighting and shadows, water, fog, glow and other
effects. The same three renderers and the same settings work in every game,
using each game's own maps, models, lights and fog. Where a game behaves
differently, this guide says so.

Jedi Academy and Jedi Outcast still have incomplete graphics support; their
supported menu models and effects use these same renderers and settings
([current limits](GAMES.md#star-wars-jedi-knight-jedi-academy-jka-sp-jka-mp)).

New to typing settings? [Getting Started](GETTING_STARTED.md#the-console-and-settings)
shows how to open the console, change a setting and keep it. Widescreen
field of view and HUD are in the [Widescreen guide](ASPECT_CORRECTION.md), and
saving pictures in the [Screenshots guide](SCREENSHOTS.md).

## Quick recipes

Paste a recipe into the console a line at a time, or into your
`autoexec.cfg`. Settings marked "restart" need `vid_restart` afterwards; the
recipes already end with it.

**Fullscreen at your desktop's resolution:**

```cfg
seta r_fullscreen "1"
seta r_mode "-2"
vid_restart
```

**A borderless window that fills the screen:**

```cfg
seta r_fullscreen "0"
seta r_mode "-2"
seta r_noborder "1"
vid_restart
```

**A window of a size you choose:**

```cfg
seta r_fullscreen "0"
seta r_mode "-1"
seta r_customWidth "1600"
seta r_customHeight "900"
vid_restart
```

**Smoother edges:** `seta r_ext_multisample "4"` then `vid_restart`. See
[Sharper edges and render resolution](#sharper-edges-and-render-resolution).

**Too dark or too bright:** change `r_gamma`, for example `r_gamma 1.2` to
brighten. No restart is needed.

**More frames on an older PC:** turn the extras down and render at a lower
resolution, scaled up to fill the screen:

```cfg
seta r_ext_multisample "0"
seta r_dlightShadows "0"
seta r_renderWidth "1280"
seta r_renderHeight "720"
seta r_renderScale "4"
vid_restart
```

**Turn on the eye candy:** glow, water reflections, the underwater view and,
in GLx and Vulkan, shadows from the sun:

```cfg
seta r_bloom "1"
seta r_bloom_threshold_mode "2"
seta r_liquid "1"
seta r_underwater "1"
seta r_csmShadows "1"
vid_restart
```

## Renderer Choice

A renderer is the part of fnTech3 that draws the picture. There are three,
and every game can use any of them. Choose one with `cl_renderer`, then
`vid_restart`:

- `cl_renderer glx` -- **GLx**, the default: fnTech3's main OpenGL renderer,
  and the one most features are made for first. See the
  [GLx guide](GLX.md).
- `cl_renderer vk` -- **Vulkan**: a Vulkan renderer with the same core
  features -- HDR, anti-aliasing, render scaling, brightness and glow. Try it
  if GLx has a problem on your graphics card.
- `cl_renderer rtx` -- **RTX**: Vulkan with path-traced lighting, which lights
  each map from its own lamps, sky and sun. It falls back to normal drawing
  where your graphics card cannot trace. See the [RTX guide](RTX.md).

Only `glx`, `vk` and `rtx` are valid names.

## Display Modes And Window Behavior

These settings control the window itself. All of them but V-Sync
(`r_swapInterval`) need `vid_restart`; `vid_restart fast` usually applies them
without closing the window (see
[Fast switching](#fast-switching-between-window-and-fullscreen)).

- `r_fullscreen`: `1` for fullscreen, `0` for a window.
- `r_mode`: the resolution.
  - `-2`: your desktop's resolution.
  - `-1`: the size in `r_customWidth` and `r_customHeight`.
  - `0` and up: one of the sizes listed by the `modelist` command.
- `r_modeFullscreen`: a different resolution just for fullscreen. Leave it
  empty to use `r_mode` for both; it takes the same values.
- `r_customWidth` and `r_customHeight`: the size `r_mode -1` uses.
- `r_customPixelAspect`: the shape of one pixel for `r_mode -1` or `-2`.
  Leave it at `1` unless you know your display needs otherwise.
- `r_displayRefresh`: the refresh rate in fullscreen. `0` keeps your
  monitor's current rate.
- `r_noborder`: `1` removes the window's title bar and borders.
- `vid_xpos` and `vid_ypos`: where the window sits; they are saved for you.
- `r_swapInterval`: V-Sync. `1` waits for your monitor, which stops tearing;
  `0` does not wait, for the most frames per second.

**How fullscreen works.** When you ask for fullscreen at your desktop's
resolution with `r_displayRefresh 0`, fnTech3 uses *borderless fullscreen*:
the display mode does not change, so Alt+Tab is instant and your other
monitors are left alone. Any other resolution, or a set refresh rate, switches
the display to the closest mode at least as large as the one you asked for.
The console reports what it chose, for example
`...fullscreen 2560x1440@144Hz (exclusive)`; if no mode is large enough, it
says so and uses borderless fullscreen instead.

The window is always kept where you can reach its title bar, even after you
change your display scaling or taskbar, unplug a monitor or restart the
renderer. On Linux desktops that use Wayland, the desktop places windows
itself.

### Window Title And Icon

The window's title names the game, for example
`fnTech3 0.1.0 - Return to Castle Wolfenstein (Multiplayer)`; `+set cl_title`
when you start replaces it. The icon in the window and the taskbar is the
game's own, remade in high resolution:

- `cl_gameIcon 1`: the remade icon of the game you are playing. This is the
  default.
- `cl_gameIcon 2`: the icon from your installation where it can be read,
  otherwise the remake.
- `cl_gameIcon 0`: fnTech3's own flame icon.

`cl_gameIcon` takes effect at the next `vid_restart`.

### Fast switching between window and fullscreen

**Alt+Enter** switches between a window and fullscreen without closing the
game window. `vid_restart fast` does the same for any window change -- size,
position, borders or fullscreen -- and avoids the flicker of a full restart:

```cfg
seta r_fullscreen "0"
vid_restart fast
```

It is always safe to use: when a change needs a new window -- switching
between GLx and Vulkan or RTX, a different colour or depth setting, or an HDR
request --
fnTech3 makes a new one by itself. The console prints
`...reusing existing window` when it kept the old one.

## Sharper Edges And Render Resolution

fnTech3 draws each frame into an image of its own before showing it, which
makes most of the effects in this guide possible. That is `r_fbo`, which is on
by default; leave it on. With `r_fbo 0`, HDR, anti-aliasing, render scaling,
glow, greyscale, the water effects, the underwater view, motion blur and the
global fog all stop working.

- `r_ext_multisample`: **anti-aliasing** that smooths the jagged edges of
  walls and models. Try `2`, `4` or `8` (up to `16`); `0` turns it off. If
  your graphics card cannot do the number you ask for, the next lower one is
  used.
- `r_ext_alpha_to_coverage`: `1` also smooths the edges of see-through
  patterns such as grates, fences and leaves. It needs `r_ext_multisample`
  and is off by default, as in the original games.
- `r_ext_supersample`: `1` draws every frame at a higher resolution and
  shrinks it, for the cleanest picture at a large cost in speed.
- `r_ext_max_anisotropy`: how sharp textures stay on floors and walls seen
  at an angle. The default is `8`; `16` is sharper still.
  `r_ext_texture_filter_anisotropic 0` turns it off. Restart.
- `r_renderScale`, `r_renderWidth` and `r_renderHeight`: draw the game at
  one resolution and show it at another -- lower for speed, higher for
  quality. `r_renderWidth` and `r_renderHeight` set the size to draw at, and
  `r_renderScale` how it is stretched to the screen:
  - `0`: off; the game draws at the window's size.
  - `1`: blocky (nearest-pixel) stretch.
  - `2`: blocky, keeping the shape and leaving bars at the sides.
  - `3`: smooth stretch.
  - `4`: smooth, keeping the shape.

> [!NOTE]
> In Vulkan, anti-aliasing switches itself off while
> [soft particles](#soft-particles) are on, which they are by default. Set
> `r_depthFade 0` to use anti-aliasing in Vulkan.

For example, to draw at 1280x720 and scale smoothly to your screen:

```cfg
seta r_renderWidth "1280"
seta r_renderHeight "720"
seta r_renderScale "4"
vid_restart
```

## Brightness And Colour

- `r_gamma`: the overall brightness. Raise it if the game is too dark, lower
  it if it looks washed out; `1` is the default. No restart is needed.
- `r_mapOverBrightBits`: how bright each map's baked lighting is drawn. Each
  game has its own default: `1` for Elite Force Holomatch, whose maps are
  made for it, `0` for Elite Force single player and Alice, which show their
  maps' light exactly as stored, and `2` elsewhere. Restart.
- `r_greyscale`: turns the colour down, to black and white at `1`.
- `r_ignorehwgamma`: `1` never changes your monitor's own brightness settings
  (see below). Any other non-zero value, such as the `-1` some Enemy
  Territory configs use, does the same. Restart.

The widescreen field of view setting, `r_fovCorrection`, is described in the
[Widescreen guide](ASPECT_CORRECTION.md).

### Brightness after Alt+Tab

Older games changed your monitor's brightness settings directly, which could
leave the desktop too dark or too bright afterwards. fnTech3 avoids that:

- Normally fnTech3 applies brightness itself, inside its own picture, and
  never touches the monitor's settings or its colour calibration. Nothing
  changes on the desktop when you Alt+Tab.
- Only in fullscreen with `r_fbo 0`, when `r_gamma` or the game's
  brightness asks for it, does fnTech3 change the monitor's settings. It
  gives them back whenever you leave the game, change monitors, restart the
  renderer, quit, or the game crashes.
- `r_ignorehwgamma 1` stops it ever doing so. Remote Desktop sessions and
  monitors that report broken settings are skipped automatically.

If the game or the desktop looks too dark after Alt+Tab, check `r_gamma` and
make sure `r_fbo` is `1`. A monitor left too dark can be fixed by
reapplying its colour profile in Windows' display settings.

## HDR, Tone Mapping And Colour Grading

These are advanced settings. The defaults keep each game looking as its
designers lit it, so you only need this section if you want HDR on an HDR
monitor, a filmic look, or colour grading.

**HDR rendering** draws the scene with a much wider range of brightness than
a normal picture can hold, then fits it to your screen. That gives smoother
gradients and lets very bright highlights glow. `r_hdr` is off by default in
GLx and on in Vulkan and RTX.

- `r_hdr`: Selects the HDR render pipeline.
  - `0`: the normal picture, as the games always drew it.
  - `1`: the high-precision HDR pipeline. With the default tone mapping it
    still keeps each game's original lighting; the filmic tone mapping,
    colour grading and HDR output below use its full brightness range.
  - `-1`: a debugging mode, the same as `r_hdrPrecision -1` without HDR.
- `r_hdrPrecision`: how precisely colours are stored while drawing.
  - `0`: automatic: normal precision without HDR, floating-point with
    `r_hdr 1`. This is the default.
  - `-1`: deliberately coarse, to show colour banding when testing.
  - `8` or `16`: force 8-bit or 16-bit storage when testing.
- `r_hdrBloomFormat`: how glow is stored with `r_hdr 1`. `0`, the default,
  picks a compact format and falls back to a larger one if your driver
  refuses it; `1`, `2` and `3` force a particular format for comparisons.
- `r_srgbTextures` and `r_framebufferSRGB`: technical colour-space switches
  for the HDR pipeline. Leave them at their defaults.

**HDR output** sends a real HDR signal to an HDR monitor. It needs `r_hdr 1`
and HDR turned on in your operating system.

- `r_outputBackend`: what kind of signal to send.
  - `0`: automatic, and careful: Vulkan sends HDR when `r_hdrDisplay 1` asks
    for it, and GLx only when `r_hdr 1` is set and the system reports HDR.
  - `1`: always a normal (SDR) picture.
  - `2`: Windows HDR ("scRGB").
  - `3`: HDR10. Vulkan uses it when the display supports it.
  - `4`: macOS extended brightness (EDR), when the system reports headroom.
  - `5`: Linux HDR, experimental; it also needs
    `r_outputAllowExperimentalLinuxHDR 1`.
- `r_outputAllowExperimentalLinuxHDR`: allows the experimental Linux HDR
  output, only when the desktop reports that HDR works. Leave it off unless
  you are testing a Wayland desktop known to support HDR.

- `r_hdrDisplay`: `1` asks Vulkan to send HDR10 to your monitor. Restart.

**Tone mapping** decides how the HDR range is squeezed onto your screen:

- `r_tonemap`: `0` keeps the games' original look (the default), `1` a
  gentle curve (Reinhard), `2` a filmic curve (ACES).
- `r_tonemapExposure`: brightens or darkens the HDR image before it is
  fitted; `1.0` is the default.
- `r_glxAutoExposure`: in GLx, lets the picture adjust to dark and bright
  places, as your eyes do. `0`, the default, uses `r_tonemapExposure`; `1`
  adapts automatically with the best method your card supports; `2` and `3`
  force a particular method. It is
  tuned with `r_glxAutoExposurePercentile` (default `80`),
  `r_glxAutoExposureTargetLuma` (how bright to aim for, default `0.18`),
  `r_glxAutoExposureMin` and `r_glxAutoExposureMax` (the limits, `0.125` and
  `8.0`) and `r_glxAutoExposureAdapt` (how quickly it changes, `0.15`; `1` is
  instant). The `glxpostprocess` command shows which method is running.

**Colour grading** tints and balances the HDR picture, like the colour tools
in photo software. It needs `r_hdr 1` and is off by default.

- `r_colorGrade`: `0` off; `1` lift, gamma and gain with white balance; `2`
  a colour lookup table (LUT) image; `3` both.
- `r_colorGradeLift`, `r_colorGradeGamma` and `r_colorGradeGain`: adjust the
  shadows, the mid-tones and the highlights, each as red, green and blue
  values, such as `"0 0 0"` for lift and `"1 1 1"` for gamma and gain (the
  defaults).
- `r_colorGradeWhitePoint` and `r_colorGradeAdaptWhitePoint`: white balance,
  as colour temperatures in Kelvin. The picture is shifted from the first to
  the second; both default to `6504`, which changes nothing.
- `r_colorGradeLUT`: a lookup table image for `r_colorGrade 2` or `3`,
  laid out as `N*N` pixels wide and `N` high, with the blue slices side by
  side.
- `r_colorGradeLUTScale`: the brightness range the table covers. The default
  `4.0` covers `0` to `4`.

## Dynamic Lighting And Shadowing

*Dynamic lights* are the lights that move or flash -- rockets, muzzle flashes,
explosions and glowing power-ups. fnTech3 draws them as smooth, per-pixel
light that can also cast shadows. These are drawing effects only: they never
change gameplay, demos or online play, and the defaults are the same in every
game.

- `r_dynamiclight`: `1` (the default) shows dynamic lights; `0` hides them.
- `r_dlightMode`: how they are drawn.
  - `0`: the game's original style.
  - `1`: smooth per-pixel light on walls and floors.
  - `2`: the same, and on players and models too. This is the default on PCs.
- `r_dlightScale`: how far the lights reach. The default is `0.5`, or `1.0`
  in Return to Castle Wolfenstein's single player, which also allows a wider
  range than the usual `0.1` to `1`.
- `r_dlightIntensity`: how bright they are, without changing their reach.
  Default `1.0`.
- `r_dlightFalloff`: `1` (the default) fades a light's edge more softly.
- `r_dlightSaturation`: how colourful the lights are. Default `0.8`.

**Shadows from dynamic lights** are on by default:

- `r_dlightShadows`: `1` lets dynamic lights cast shadows. Restart.
- `r_dlightShadowFilter`: how soft the shadows' edges are: `0` hard, `1`
  softened, `2` softest (the default).
- `r_dlightShadowResolution`: how detailed each shadow is, from `64` to
  `1024` (the default). fnTech3 may lower it to fit several shadows at once.
- `r_dlightShadowMaxLights`: how many lights may cast shadows at once.
  Default `4`; fewer gives each shadow more detail.
- `r_dlightShadowStrength`: how dark the shadows are. Default `0.95`.
- `r_muzzleFlashDlightShadows`: `0` keeps the light of a gun's flash but
  without its shadow. On by default.
- `r_muzzleFlashDlightOffset`: moves a muzzle flash's light forward along
  the gun, in game units. Default `8`; `0` puts it where the original did.
- `r_spotShadows`: `1` lets the spotlights of [Static Map Lights](#static-map-lights)
  cast shadows too. Off by default. Restart.
- `r_csmShadows`: `1` adds shadows from the sun in maps that have one, cast
  by the world, models and doors, and falling on models as well as walls.
  Alice, F.A.K.K.2 and Medal of Honor give their sun in the level itself
  rather than its sky, and fnTech3 takes it from there. Off by default.
  GLx and Vulkan, and RTX when it draws normally (`rtx_rt_mode 0`).
  Restart.

Fine-tuning, for odd-looking shadows -- speckles, flicker, or shadows that
float off their objects: `r_dlightShadowBias` (default `4`),
`r_dlightShadowCasterDepthBias` (`1`), `r_dlightShadowCasterSlopeBias` (`1`)
and `r_dlightShadowCasterNormalBias` (`0.25`). The defaults suit normal play;
if a shadow lacks detail, lower `r_dlightShadowMaxLights` before changing
these. `r_dlightShadowDebug 1`, or `r_speeds 4`, prints how much detail each
shadow really gets.

Walls, curved surfaces and every game's models cast shadows, from dynamic
lights and from the sun: Enemy Territory's soldiers and the characters of
Alice, F.A.K.K.2 and Medal of Honor included. See-through patterns and
moving surfaces such as waving flags cast none from dynamic lights.

**Original model shadows.** `cg_shadows` is the game's own shadow under
players, in games that have it: `0` off, `1` a soft blob, `2` sharp shadow
shapes (these need a stencil buffer, `r_stencilbits 8`, the default), `3` flat black shadows.

**Game notes:**

- **Return to Castle Wolfenstein and Enemy Territory:** the game's own
  lights -- muzzle flashes, flames and explosions -- use the smooth
  per-pixel lights too, up to 32 at once. Return to Castle Wolfenstein uses
  them at `r_dlightMode 1` or `2`, except at `r_dynamiclight 2` and in the
  Snooper rifle's heat view; Enemy Territory whenever `r_dynamiclight` is on.
  Lights
  the game insists on still show at `r_dynamiclight 0`. Return to Castle
  Wolfenstein's darkening and flame lights never cast shadows.
- **Quake III Arena:** a map made with the IHV Test's fog uses the original
  style, `r_dlightMode 0`, in every renderer.

A balanced high-quality setup:

```cfg
seta r_dynamiclight "1"
seta r_dlightMode "2"
seta r_dlightShadows "1"
seta r_dlightShadowFilter "2"
seta r_dlightShadowResolution "1024"
seta r_dlightShadowMaxLights "2"
vid_restart
```

## Static Map Lights

A map's ordinary lighting is painted onto its walls in advance, so a player
walking past a lamp casts no shadow from it. *Static map lights* add each
map's own lamps back as real lights, so players and models are lit by them
and, within the shadow limits above, cast shadows from them. They change
nothing but the picture.

fnTech3 brings a light file, made from the lights stored in each map, for
every map of:

- Return to Castle Wolfenstein: all 32 single-player and 15 multiplayer maps;
- American McGee's Alice: all 36 maps;
- Heavy Metal: F.A.K.K.2: all 30 maps;
- Medal of Honor: Allied Assault: all 54 maps.

Quake III Arena, Quake Live, Enemy Territory and Elite Force have none yet;
Enemy Territory's maps keep no lights to convert.

- `r_staticLights`: `1` (the default) uses them.
- `r_staticLightMaxLights`: how many may light one scene. Default `8`.
- `r_staticLightShadows`: `1` (the default) lets them cast shadows, which
  also needs `r_dlightShadows 1` and `r_dlightMode 1` or `2`.
- `r_staticLightShadowMaxLights`: how many may cast shadows at once.
  Default `2`.
- `r_staticLightReload`: reloads the current map's light file after you
  edit it.

### For map and mod makers

Light files are `maps/<map>.lights.json`. One in fnTech3's own package wins
over one in the game or a mod, and a mod can add files for maps without one.
The file is JSON, `{"version": 1, "lights": [ ... ]}`, where each light has a
`type` of `point` or `spot`, an `origin`, a `color`, an `intensity` and a
`radius`, plus a `direction` and cone angles for spots, and may have
`castsShadows`, `priority` and `style`. Unknown keys are ignored, and a map
holds at most 128 lights. Spotlights cast shadows only with `r_spotShadows 1`.
`r_staticLightDebug` (a cheat) prints what was loaded and used.

Glowing surfaces can also become lights: `r_surfaceLightProxies 1` turns a
map's glowing (`q3map_surfaceLight`) surfaces, other than the sky, into
lights, with `r_surfaceLightProxyMaxLights` (default `4`),
`r_surfaceLightProxyShadows` (default `0`) and
`r_surfaceLightProxyShadowMaxLights` (default `1`).

## Soft Particles

Smoke, explosions, blood and similar effects used to show a hard line where
they cut into a floor or wall. *Soft particles* fade them out where they meet
the world, so they blend in. It only changes the picture.

- `r_depthFade`: `1` (the default) turns it on. Restart.

It works on the Quake III Arena effects fnTech3 knows -- explosions, smoke,
plasma, bullets, the railgun, the BFG and blood -- and on any game's effects
whose shader asks for it with `q3map_depthFade <distance> <bias>`. While
Return to Castle Wolfenstein's or Enemy Territory's own fog is drawing, it is
skipped. In Vulkan, soft particles switch anti-aliasing off; set
`r_depthFade 0` there if you prefer anti-aliasing.

## Texture Detail

`r_picmip` lowers the sharpness of textures to save graphics memory and gain
speed, as in the original games. Restart after changing it.

- `r_picmip`: `0` keeps full detail; each step up halves it.
- `r_picmipFilter`: which textures `r_picmip` may blur. Add the numbers
  together to combine them:
  - `1`: walls and floors (`textures/`). This is the default, so the HUD,
    menus, fonts and most models stay sharp.
  - `2`: models -- players, weapons and items (`models/`).
  - `4`: sprites (`sprites/`).
  - `8`: menus, icons and fonts.
  - `0`: everything, as the original games did.

  For example, `3` blurs walls, floors and models, and `15` blurs everything
  in the groups above.
- `r_picmip2`: Return to Castle Wolfenstein single player's separate setting
  for character textures, as in its original. `0` keeps full detail.
- `r_nomip`: applies `r_picmip` only while a map's own textures load.
- `r_neatsky`: keeps the sky sharp.

A texture marked to stay sharp (`nopicmip`) always does.

## Cel Shading

Cel shading gives a cartoon look. It has two parts: flat bands of light with
outlines on players, weapons and other models, and dark outlines along the
edges of the world itself. Both are off by default and only change the
picture. GLx and Vulkan both have them.

**Models:**

- `r_celShading`: `1` turns on cel shading for models.
- `r_celShadingModelShadows`: `1` (the default) draws their lighting in flat
  bands.
- `r_celShadingSteps`: how many bands, from `2` to `8`. Default `4`; fewer
  is bolder.
- `r_celViewWeapon`: `1` (the default) includes your own weapon.
- `r_celOutline`: `1` (the default) draws an outline round each model. It
  needs a stencil buffer, which is on by default (`r_stencilbits 8`).
- `r_celOutlineScale`: the outline's thickness, from `1.0` to `1.25`.
  Default `1.03`.
- `r_celOutlineAlpha`: the outline's strength, from `0` to `1`. Default `1`.
- `r_celOutlineColor`: the outline's colour as `"r g b a"`. Default
  `"0 0 0 255"`, black.
- `r_celViewWeaponOutlineScale` and `r_celViewWeaponOutlineAlpha`: the same
  for your own weapon, which is much closer to you. Defaults `1.006` and
  `1.0`.

**The world:**

- `r_celShadingWorld`: `1` draws outlines along the world's edges.
- `r_celShadingWorldWidth`: their width in pixels, from `1` to `8`.
  Default `2`.
- `r_celShadingWorldAlpha`: their strength, from `0` to `1`. Default `1`.
- `r_celShadingWorldDepthThreshold`: how sharp an edge must be to get an
  outline, from `0.0001` to `0.02`. Default `0.0015`; lower outlines more
  edges.

The outlines draw over glow, so they stay visible. A good starting point:

```cfg
seta r_celShading "1"
seta r_celShadingSteps "4"
seta r_celOutline "1"
seta r_celShadingWorld "1"
```

For a bolder look, set `r_celShadingSteps` to `2` or `3`; for a softer one,
`5` or `6`. For thicker world outlines raise `r_celShadingWorldWidth`, and
for fewer of them raise `r_celShadingWorldDepthThreshold`.

## Map Flares, Coronas And Lens Effects

Many Quake III Arena and Quake Live maps place *flares*: glows round lights
that fade as you move away or behind something. fnTech3 keeps how they
appear and hide, and can add a camera-lens look.

- `r_flares 0`: no flares. This is the default outside Return to Castle
  Wolfenstein.
- `r_flares 1`: the original Quake III glow.
- `r_flares 2`: the original glow plus a soft halo, a subtle horizontal lens
  streak and faint lens reflections across the screen.

The lens effects reuse the game's own flare image and hide with the flare, so
a light out of sight leaves nothing behind. Glow (`r_bloom`) is not needed,
but makes the brightest flares spread more naturally.

**Game notes:**

- **Return to Castle Wolfenstein** draws its own coronas round lights, as the
  original did, and `r_flares` means what it meant there: `1` (the default)
  the coronas the game places, `2` a corona on every dynamic light, `3` both.
  `r_flareFade` is `5` there and `10` elsewhere.
- **Enemy Territory** draws coronas the same way, but `r_flares` is `0` by
  default, so they are off until you turn them on.
- **Alice and F.A.K.K.2** draw their sun flares and light coronas with their
  original settings: `r_sunflare`, `r_place_sunflare` and
  `r_sunflare_inportalsky` for the sun, and `r_lightcoronasize` (default
  `0.1`) for the size of the coronas.

## Enhanced Liquids

With [RTX ray tracing](RTX.md#what-you-will-see), liquids use traced
reflections and refraction automatically. The screen-space effect described
below applies to normal drawing. RTX shares its wave and ripple controls.

fnTech3 can make water look more alive: the world behind it shimmers through
it, the surface reflects the room at low angles, and players and rockets send
ripples across it. Everything the map's designer gave the water -- its colour,
movement and waves -- stays. It only changes the picture, and is off by
default.

- `r_liquid`: which liquids to enhance. `0` none (the default), `1` water,
  `2` water, slime and lava, the last two more subtly. Restart.
- `r_liquidResolution`: how detailed the shimmer is, from `0.25` to `1.0`
  (the default). Lower is faster but softer. Restart.
- `r_liquidRefraction`: how strongly the world shows through, from `0` to
  `1`. Default `0.65`.
- `r_liquidWarpScale`: how much the water's waves bend what is behind it,
  from `0` to `2`. Default `1`. It looks the same at every resolution, and
  calms down with distance so far-off water does not sparkle.
- `r_liquidReflection`: how strongly the surface reflects at low angles,
  from `0` (off) to `1`. Default `0.65`.
- `r_liquidRipples`: how big the ripples from players and rockets are, from
  `0` (off) to `2`. Default `1`.

A good starting point is just:

```cfg
seta r_liquid "1"
vid_restart
```

The reflections are a fast trick that reuses what is already on screen, so
they cannot show things off the edge of the screen or behind you; near the
edges they fade to a plain sheen. The ripples only bend the picture -- they
never change swimming, physics or online play. A few unusual liquids that the
trick cannot draw safely keep their normal look. The older names
`r_liquidReflections`, `r_liquidReflectionScale`, `r_liquidWarp`,
`r_liquidFresnel` and `r_liquidRippleStrength` no longer do anything.

**Game notes:** ripples come from Quake III Arena, Quake Live and Elite Force;
Return to Castle Wolfenstein, Enemy Territory, Alice and F.A.K.K.2 get the
shimmer and reflections without them, and the other games have not been
checked. Return
to Castle Wolfenstein and Enemy Territory still draw their own water ripples,
as they always did, and enhanced liquids pause while their fog is drawing.
How it works is described in [Liquid Rendering](fnql/LIQUID_RENDERING.md).

## Underwater View

When you dive into water, slime or lava, fnTech3 can make the world look as if
you are inside it: the picture wobbles, with slight colour fringes, the edges
darken, and the distance fades into the liquid's colour -- quickly in slime,
and almost at once in lava. The HUD and console stay sharp. It is the inside
view of [Enhanced Liquids](#enhanced-liquids), and the two can be used
separately. It only changes the picture, and is off by default.

- `r_underwater`: `1` turns it on. Restart.
- `r_underwaterWarp`: how much the picture wobbles, from `0` (none) to `2`.
  Default `1`.
- `r_underwaterDispersion`: the colour fringes, from `0` (none) to `1`.
  Default `0.35`.
- `r_underwaterFog`: how quickly the distance fades into the liquid's colour,
  from `0` (not at all) to `1` (the default).
- `r_underwaterVignette`: how much the edges darken, from `0` (not at all) to
  `1`. Default `0.35`.

Going in and out fades the effect over a fifth of a second. It works in Quake
III Arena, Quake Live and Elite Force, whose game code tells fnTech3 when you
are under the surface; Return to Castle Wolfenstein, Enemy Territory, Alice
and F.A.K.K.2 do not, so it stays off there, and the other games have not been
checked. It
bends what is already on screen rather than drawing a new view, so it adds no
light shafts or bubbles. How it works is described in
[Underwater View](fnql/UNDERWATER_VIEW.md).

```cfg
seta r_underwater "1"
vid_restart
```

## Fog

### RTCW And Enemy Territory Fog

Return to Castle Wolfenstein draws the distance fog its maps and levels set,
including fog the game changes as you play. Enemy Territory's whole-map fog
is drawn the same way, though its look has not yet been compared with the
original.

- `r_wolffog`: `1` (the default) draws it; `0` turns that fog off.

### Optional Global Fog

fnTech3 can add a light, atmospheric haze to a map, which deepens with
distance. Its own package brings gentle fog for the maps of Quake Live, Quake
III Arena and Team Arena, and a mod can add it for any game. It only changes
the picture -- never the map's own fog, what the game can see, or gameplay --
though thick fog can make distant players harder to spot. It is off by
default.

- `r_globalFog`: `1` turns it on. It works in GLx and Vulkan, and in RTX with
  anti-aliasing off. Restart.
- `r_globalFogStrength`: how thick it is, from `0` to `1` (the default).

A broken or missing fog file only turns the haze off; it never stops the game
starting. The file format, for map makers, is in
[Global Fog Sidecars](fnql/GLOBAL_FOG.md).

### Alice And F.A.K.K.2 Far Plane

Ritual's games fade each level into a fog colour at a set distance and draw
nothing beyond it. fnTech3 draws it as the level sets it, and keeps the
original settings for changing it:

- `r_farplane`: a distance of your own, when not `0`; `r_farplane_color` sets
  the fog colour, and `r_farplane_nocull 1` keeps drawing beyond it.
- `r_farplane_nofog`: turns the fading off.
- `r_farplane_pulse` and `r_farplane_pulseFrequency`: make the distance
  breathe in and out.
- `r_skyportal` and `r_skyportal_origin`: draw the level's distant sky view
  from another place.

## Bloom

Bloom makes bright things glow: lights, explosions and shiny surfaces spill a
soft haze around themselves. It is off by default in GLx, and on by default in
Vulkan and RTX.

- `r_bloom`: `1` turns it on. In GLx, `2` also lets the HUD glow. In Vulkan
  and RTX, use `0` or `1`, followed by `vid_restart`.
- `r_hudExcludePostProcess`: `1` (the default) keeps 3D models drawn in the
  HUD, such as a player's head, from glowing.
- `r_bloom_threshold`: how bright something must be to glow. Default `0.75`;
  lower makes more of the picture glow.
- `r_bloom_threshold_mode`: how brightness is judged. `0` (the default) by
  the brightest colour, `1` by the average, `2` by how bright it looks to the
  eye -- usually the cleanest.
- `r_bloom_soft_knee`: softens the line between glowing and not, from `0`
  (the default, a hard line) to `1`.
- `r_bloom_modulate`: makes the glow favour the brightest spots. `0` (the
  default) leaves it, `1` and `2` strengthen it.
- `r_bloom_intensity`: how strong the glow is. Default `0.5`.
- `r_flareSceneLinear`: `0` returns flares to their older look in the HDR
  pipeline, for comparisons. Leave it at `1`.

GLx has extra controls for the glow's shape:

- `r_bloom_passes`: how far the glow spreads. Default `5`; more spreads
  further and costs more. Restart.
- `r_bloom_blend_base`: start the glow wider and softer. Default `1`.
- `r_bloom_filter_size`: how blurry the glow is. Default `6`.
- `r_bloom_reflection`: an extra lens-reflection effect. Try small values
  such as `0.25`; a negative value shows only the reflection. Default `0`.

Set it up in this order: turn on `r_bloom 1`, choose
`r_bloom_threshold_mode 2`, lower `r_bloom_threshold` until the right things
glow, then adjust `r_bloom_intensity`.

A subtle glow:

```cfg
seta r_bloom "1"
seta r_bloom_threshold "0.75"
seta r_bloom_threshold_mode "2"
seta r_bloom_modulate "2"
seta r_bloom_intensity "0.25"
```

A stronger glow for bright maps:

```cfg
seta r_bloom "1"
seta r_bloom_threshold "0.60"
seta r_bloom_threshold_mode "2"
seta r_bloom_modulate "2"
seta r_bloom_intensity "0.45"
```

A hazy, dreamy glow (GLx):

```cfg
seta cl_renderer "glx"
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

**If the glow is too weak,** lower `r_bloom_threshold` or raise
`r_bloom_intensity`; in GLx, raise `r_bloom_passes` or
`r_bloom_filter_size`. **If the whole picture looks milky,** raise
`r_bloom_threshold`, lower `r_bloom_intensity`, and use
`r_bloom_threshold_mode 2`. **If the HUD glows,** use `r_bloom 1` rather than
`2`. **If nothing happens,** check `r_bloom` is `1` and `r_fbo` is `1`, and
try a low `r_bloom_threshold`.

## Motion Blur

Motion blur smears the picture slightly when you turn or move quickly, as a
camera does. It follows your view only, never leaves trails behind moving
players, and the HUD and console always stay sharp. It is off by default.

- `r_motionBlur`: `1` turns it on. It works in GLx and Vulkan; in Vulkan,
  `vid_restart` after changing it.
- `r_motionBlurStrength`: how strong it is, from `0` to `1`. The default
  `0.25` is deliberately subtle.

```cfg
seta r_motionBlur "1"
seta r_motionBlurStrength "0.25"
```

Standing still gives a perfectly sharp picture, very fast turns are capped so
they never smear badly, and teleporting or a jump in the action resets it.

## View Shake

Some games shake your view for a moment when a gun fires or something
explodes near you. If that shaking bothers you, turn it down or off.

- `cl_viewShake`: how strong the shake is, from `0` (no shake at all) to
  `1` (the game's own shake, the default). `0.5` halves it. A change takes
  effect with the next shake; no restart is needed.

```cfg
seta cl_viewShake "0"
```

For now this works in Jedi Academy multiplayer, where muzzle flashes and
explosions shake your view. The other games work out their shakes entirely
inside their own game code, where the engine cannot reach them, so this
setting leaves them as they are. The
[view shake details](fntech3/2026-10-15-view-shake.md) list every game and
why.

## Game-Specific Renderer Settings

Medal of Honor's barrel explosions and other hidden emitters now draw
their particles and debris in all three renderers, with no extra setting
needed ([effects details](fntech3/2026-10-11-mohaa-effects.md)).

A few settings exist only in the game whose original engine had them, with
that engine's defaults:

| Game | Setting | Default | What it does |
| --- | --- | --- | --- |
| Return to Castle Wolfenstein (both modes) | `r_flares` | `1` | Coronas; see [Map Flares, Coronas And Lens Effects](#map-flares-coronas-and-lens-effects). |
| Return to Castle Wolfenstein (both modes) | `r_flareFade` | `5` | How quickly coronas fade; `10` in other games. A cheat. |
| Return to Castle Wolfenstein single player | `r_picmip2` | `0` | Character texture detail. |
| Return to Castle Wolfenstein single player | `r_dlightScale` | `1.0` | How far dynamic lights reach; `0.5` in other games. |
| Return to Castle Wolfenstein and Enemy Territory | `r_wolffog` | `1` | The games' own fog. |
| Enemy Territory | `r_drawfoliage` | `1` | Draws grass and plants. A cheat. |
| Elite Force Holomatch | `r_mapOverBrightBits` | `1` | Map brightness; `2` in most other games. |
| Elite Force single player and Alice | `r_mapOverBrightBits` | `0` | Map brightness; these games show their maps' light as stored. |
| Elite Force single player, Alice and F.A.K.K.2 | `r_overBrightBits` | `0` | Overall brightness; `1` in other games. Restart. |
| Alice and F.A.K.K.2 | `r_shaderlod` | `0.5` | Which detail level of a surface's effects to keep; applies as they load. |
| Alice and F.A.K.K.2 | `r_detailtextures` | `0` | Extra fine detail on surfaces; `1` in other games, where changing it needs `vid_restart`. |
| Alice and F.A.K.K.2 | `r_lodscale` | `5` | How quickly models lose detail with distance; no cheat needed, as in the original. |
| Alice and F.A.K.K.2 | `r_farplane`, `r_skyportal` | `0` | See [Alice And F.A.K.K.2 Far Plane](#alice-and-fakk2-far-plane). |
| Alice and F.A.K.K.2 | `r_sunflare`, `r_lightcoronasize` | `0`, `0.1` | Sun flares and light coronas. |
| Alice and F.A.K.K.2 | `r_ritualModelLights` | `1` | Lights characters and weapons from the map's lamps, as the originals did; `0` lights every model from the map's general lighting instead. `r_ritualModelLightsDebug` (a cheat) shows what each one uses. |
| Alice and F.A.K.K.2 | `r_ambientScale` | `0.5` | How much background light models get, as in the originals; `0.6` in other games. A cheat. |
| Quake Live | `r_ambientScale` | `10` | The same, as a player option from `1` to `100`: the Video menu's Ambient Light Scale. Saved in `repconfig.cfg`. |
| Quake III IHV Test | `r_lerpmodels` | `1` | Smooth model animation. |
| Jedi Academy multiplayer | `r_entityRefraction` | `1` | How strongly the see-through shimmer round a newly spawned player bends what is behind it, from `0` (not drawn) to `4`. Turning it on from `0` needs `vid_restart`. The game's own `cg_renderToTextureFX 0` also leaves the shimmer out. |

## When To Use `vid_restart`

Type `vid_restart` after changing any of these:

- `cl_renderer`
- `r_mode`, `r_modeFullscreen`, `r_fullscreen`
- `r_customWidth`, `r_customHeight`, `r_customPixelAspect`
- `r_displayRefresh`
- `r_noborder`
- `r_fbo`
- `r_hdr`
- `r_hdrPrecision`
- `r_outputBackend`, `r_hdrDisplay`
- `r_ext_multisample`
- `r_ext_alpha_to_coverage`
- `r_ext_max_anisotropy`, `r_ext_texture_filter_anisotropic`
- `r_depthFade`
- `r_renderWidth`, `r_renderHeight`, `r_renderScale`
- `r_ext_supersample`
- `r_dlightShadows`, `r_spotShadows`, `r_csmShadows`
- `r_mapOverBrightBits`, `r_picmip`, `r_picmip2`, `r_picmipFilter`
- `cl_gameIcon`
- `r_bloom_passes` and `r_hdrBloomFormat` in GLx
- `r_bloom` and `r_motionBlur` in Vulkan and RTX
- `r_liquid` or `r_liquidResolution`
- `r_underwater`
- `r_globalFog`

For window changes -- `r_fullscreen`, `r_mode`, `r_noborder`, size or
position -- use `vid_restart fast`, which keeps the window when it can.

These work straight away, without a restart: `r_swapInterval`, `r_gamma`,
`r_greyscale`, the `r_bloom_threshold`, `r_bloom_threshold_mode`,
`r_bloom_soft_knee`, `r_bloom_modulate` and `r_bloom_intensity` glow
settings, `r_motionBlurStrength`, `r_tonemap`, `r_tonemapExposure`, every
`r_colorGrade` setting, every `r_cel` setting, `r_staticLights` (use
`r_staticLightReload` after editing a light file), `r_wolffog`, `r_farplane`
and its companions, and GLx's `r_bloom_blend_base`, `r_bloom_filter_size` and
`r_bloom_reflection`.

If a change does not seem to work, `vid_restart` is always safe to try.

## Related Guides

- [Widescreen guide](ASPECT_CORRECTION.md) for field of view, the HUD, menus
  and movies.
- [Player highlights](VISUALS.md) for outlining other players.
- [Screenshots guide](SCREENSHOTS.md) for saving pictures.
- [RTX guide](RTX.md) for the path tracer, which lights each map from the same
  lamps as the static map lights.
- [GLx guide](GLX.md) for problems with the default renderer.
