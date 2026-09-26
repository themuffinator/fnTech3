# Stencil shadows darken by GL's 0.6 in the Vulkan and RTX renderers

## Scope

With `cg_shadows 2` a renderer draws each shadow volume into the stencil
buffer and then darkens every marked pixel in one full-screen pass,
`RB_ShadowFinish`, after the view's opaque surfaces. The GL renderer draws that
pass untextured, with fog switched off, in a colour of 0.6 multiplied into the
frame. The Vulkan and RTX renderers built the same 0.6 quad but pushed no
uniform of their own for it, so it drew with the uniform the view's last
stage had pushed. This record covers the change that gives the pass its own
uniform in every profile, and the evidence for it in RTCW multiplayer, Quake
III and Quake Live. The renderers' `r_shadows` variable is the `cg_shadows`
cvar, as in Quake III (`tr_init.c`).

## Observed

### The leak

Every generated Vulkan fragment shader reads two things from the uniform that
bear on the pass (source inspection): the RTCW fog block of
`code/renderercommon/tr_rtcw_fog_vk.h`, which RTCW's global fog and Alice's
far-plane fog fill for the stages they fog, and `texFactors[0]`, which a stage
sets to `r_intensity` for a mipmapped image. Each stage of
`RB_StageIteratorGeneric` pushes its own uniform, so nothing after the pass
inherits its state; only the pass itself read another draw's.

An earlier uncommitted revision pushed a clean uniform under the Alice profile
only, after Alice's far-plane fog tinted its shadows. The change here removes
that condition in `code/renderervk/tr_shadows.c` and
`code/rendererrtx/tr_shadows.c`: the pass pushes a zeroed uniform with
`texFactors[0]` 1 before binding `vk.shadow_finish_pipeline`, in every
profile. GL's pass is untouched.

### Method

`scripts/probe_shadow_finish.py` records one short demo on a listen server,
then replays it with `timedemo 1`, which advances one snapshot per rendered
frame, and captures with the engine's `screenshotPNG` at replay frames 40 and
80. Each renderer replays the demo twice, with `cg_shadows 2` and with
`cg_shadows 0`, both set on the command line so Vulkan builds its shadow
pipelines at start. Over the pixels the shadows changed and whose unshadowed
value is at least 48 in every channel, the shadowed capture divided by the
unshadowed one is the pass's multiplier. Replays set `cg_thirdPerson 1` (the
demos come from `devmap`, so cheats hold), `con_notifytime 0` and
`cg_lagometer 0`; the lagometer graphs wall-clock frame times, the only thing
that differed between otherwise identical Quake III replays.

Both runtimes came from one export of the shared working tree (HEAD
`2d35919` plus the uncommitted state of 2026-09-24 about 08:58 UTC+1), built
with MSVC x86 `debugoptimized`, strict warnings, `-Drenderers=glx,vk,rtx`,
`-Drtcw-development=true`, and staged at `.tmp/runtime/shadowfinish-before/`
and `.tmp/runtime/shadowfinish-after/`. They share `fntech3.exe` (SHA-256
`9939695b…`) and `fnql_glx_x86.dll` (`d8ef2136…`); "before" has
`tr_shadows.c` as at HEAD in both Vulkan-family renderers, "after" has the
change. Every run was windowed on a private desktop with no input, in an
isolated home, and every installation's file inventory was unchanged
afterwards. The full manifest is
[the evidence file](2026-09-24-stencil-shadow-finish.json).

### RTCW multiplayer, `mp_ice`

`mp_ice`'s sky shader `textures/skies/sky_ice` carries `fogvars ( .4 .4 .46 )
.0001`, so the map has global fog. The demo (`.dm_60`, SHA-256 `fb5b4693…`)
stands an Allied soldier in a hut, casting a shadow on the wall. Median
multiplier over about 1,170 measured pixels, frame 40 (frame 80 agrees to the
fourth decimal):

| Renderer | Before (R, G, B) | After (R, G, B) |
| --- | --- | --- |
| GL | 0.600, 0.600, 0.600 | 0.600, 0.600, 0.600 |
| Vulkan | 0.487, 0.487, 0.521 | 0.600, 0.600, 0.600 |
| RTX | 0.487, 0.487, 0.521 | 0.600, 0.600, 0.600 |

Inside GL's shadow mask the mean absolute difference from GL fell from 2.45
to 0.69 for Vulkan (RTX: 2.46 to 0.69), where the whole frame's difference,
shadows or not, is 0.66 to 0.73. GL's captures, and every unshadowed capture,
are pixel-identical between the two runtimes; only the shadowed Vulkan and RTX
captures changed. A second complete run reproduced all 24 captures
byte for byte.

### Quake III, `q3dm17`

Retail 1.32 installation, demo `.dm_68` SHA-256 `bfa87d5a…`. Every renderer
measured 0.600 before and after, and all 24 captures, shadowed and not, are
pixel-identical between the runtimes. By source inspection, the Quake III
profile fills no RTCW fog block, and at the default `r_intensity` of 1 a
stage's `texFactors[0]` is 1, so the old uniform already carried the values
the new one does.

### Quake Live

Quake Live's cgame limits `cg_shadows` to 1: each replay asked for 2 and its
log says `cvar 'cg_shadows' is out of range (max '1'), setting to '1'` before
the renderer starts. `RB_ShadowFinish` returns before the pass when
`r_shadows` is not 2, so the Quake Live profile never reaches the changed code.
Its `campgrounds` replays (demo `.dm_91` SHA-256 `3973b6f7…`) were
pixel-identical between the runtimes in all three renderers.

## Inferred

- The old Vulkan multiplier is GL's 0.6 blended 56.5% of the way toward the
  fog colour: `0.6 + 0.565 * (0.40 - 0.6) = 0.487` and
  `0.6 + 0.565 * (0.46 - 0.6) = 0.521`, which is why the blue channel darkened
  least.
- With `r_intensity` above 1, a leaked `texFactors[0]` would have brightened
  the pass in any profile. That was read from the code and not measured.

## Open

- Other profiles with stencil shadows and fog (RTCW single player, Enemy
  Territory, Elite Force, Alice) take the same path but were not measured
  here.
