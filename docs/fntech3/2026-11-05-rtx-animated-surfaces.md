# RTX path tracer: animated shader surfaces and their light, 2026-11-05

The question was whether ray tracing follows animated shader surfaces such as
flames, tested on Quake III's q3dm1 by the large health, beside the big
flame. It did only in part:

- A flame is drawn by raster over the traced picture and animated as raster
  animates it, but the light it cast was fixed at its shader's average.
- Traced surfaces evaluated `animMap`, `tcMod` and waves at every hit, but
  kept only the first eight frames of a stage.
- They also ignored the frame rules of Medal of Honor, Alice and the Jedi
  games.

Now the light of every animated emitter follows its stages from frame to
frame. Traced surfaces keep all their frames and raster's frame rules. The
[JSON companion](2026-11-05-rtx-animated-surfaces.json) records the
binaries, installations, commands and measurements. No wire, demo, save,
asset or pak contract is touched, and no compatibility gate closes.

## Before

Observed in the source and on q3dm1 (retail paks):

- **Flames are raster's.** `textures/sfx/flame1_hell`, the 192 x 208 wall
  of fire 150 units south of the `item_health_large` at (672 1288 16), and
  the braziers' `flame2` and `flame1side` blend additively. The trace leaves
  them to the raster overlay, which animates them as the game draws them.
  They are not in the acceleration structures, so a reflection ray passes
  through them.
- **Their light stood still.** `flame1_hell` is a `q3map_surfacelight 600`
  area light. Its shader cross-fades eight frames ten times a second (two
  `animMap`s a frame apart under inverse and plain sawtooth waves) beneath a
  `flameball` glow on `rgbGen wave sin .6 .2 0 .6`. Its fold, sampled every
  0.16 s from pak0's images, varies with a relative deviation of 10%. The
  light builder took the shader's average once, so the light delivered to
  the floor was constant.
- **Eight frames.** `rt_stage_source.h` kept eight `animMap` frames per
  stage. A stage of 9 to 64 frames (RTCW allows 16, the Jedi games 32,
  extended shaders 64) cycled through the first eight in the trace, on the
  wrong period, and its emission averaged those eight alone.
- **No frame rules.** The trace ignored all of these:
  - Medal of Honor's `animMapPhase` head start.
  - Alice's `animMapOnce` and the Jedi games' `oneShotAnimMap`, which hold
    the last frame. The trace looped them instead.
  - Alice's `frameFromEntity`, and a Jedi Academy `RF_SETANIMINDEX` entity's
    `skinNum`, which choose the frame.

  Each of these was served by raster (`tr_shade.c` `RB_BindAnimatedImage`).
- **An additive emitter without a surface light** (`flame1side`) took the
  rule for a traced shader drawn fullbright: the bright texels of its first
  frame, at half intensity, plus its later stages. It now gives off all it
  adds.
- **A colour wave's mean** left out `tr.identityLight`, which raster and the
  GPU both apply (`RB_CalcWaveColor`, `rt_stage_generator`).

## Change

**Animated emitters** (`rt_lights.c`, `rt_backend.c` `RT_UploadLights`):

- Each world light records the shader whose surface made it. A shader whose
  stages may change with time (an `animMap` that cycles, a colour or alpha
  wave) becomes an animation. Its mean is the fold of its stages sampled 480
  times, 0.0731 s apart (35 s). A shader that varies by less than 0.5% of
  that mean stays a steady light. So does one whose frames' averages are not
  kept, since its files would otherwise be read every frame.
- A `q3map_surfacelight` shader's fold takes the lighting term white,
  because the compiler's light stands for all its surface shows. A glowing
  shader's fold takes it black: its glow.
  - On q3dm1, `computer_blocks17` (a lightmapped panel under fading letters)
    varies 0.6% lit, against 121% for its letters alone.
  - `pentagram_light1_1K` varies 3.7% lit.
  - `flame1_hell`, `flame2` and `flame1side` vary 8.8%.
- Every frame each animation's scale is its fold at the frame's shader time
  (the view's clock less the shader's `timeOffset`, held at its `clampTime`)
  over its mean, between 0 and 8. That scale multiplies the colour of each of
  its lights in the frame's light buffer.
- The grid and the sampler's proposals keep the mean's power. Light
  sampling, the multiple-importance weights and a reflection's view of a
  light all read the scaled colour, so they stay consistent.
- A still view is averaged only while its clock stands still
  (`s_rt.accumulateTime`), when the folds do too, so the still average is
  unchanged.
- `rtx_rt_animated_lights 0` keeps every light at its mean, as the map
  compilers baked it. A change to that setting starts the denoiser's history
  over.

**Frames** (`rt_stage_source.h`, `tr_shader.c`, `rt_material.c`,
`rt_material.glsl`):

- The stage snapshot keeps every frame (`MAX_IMAGE_ANIMATIONS`, 64) and the
  bundle's phase, hold and frame-from-entity bits.
- A GPU stage still holds eight frames. `state.z` bits 22..27 carry the
  frame count, and bits 20 and 21 the hold and frame-from-entity rules.
- `RT_AppendFrameRecords` writes the frames past eight, and a non-zero
  phase, into records after the material's stages. The stage's `tc1.w` names
  the first record, whose `animTc0.x` is the phase. The trace's stage loop
  stops before them.
- `rt_stage_frame` applies raster's order: the timed frame
  `(time + phase) * speed`, or the entity's frame; never below the first;
  held on the last; then modulo the count.
- A model or brush model vertex's normal lane `w` now carries its entity's
  frame beside its no-shadow sign (`RT_EntityFrameLane`, decoded in
  `rt_geometry.glsl`). The world's frame is 0.
- The light builder's `RT_StageFrameAt` uses the same rule in double
  precision.

**Emission.** A shader whose first stage blends over what is behind it is no
longer "fullbright lit". A colour wave's value and mean take
`tr.identityLight` (`RT_ColorWave`). Elite Force's random wave is evaluated
on the CPU from the view's clock.

## Evidence

### q3dm1's flame (Live)

The Steam Quake III install now also carries `q3r-maps.pk3`, a third-party
remaster pack added on 2026-10-31. It sorts after the retail paks and
replaces `maps/q3dm1.bsp` and most retail maps, so these probes load the
nine retail paks alone, copied unchanged into `.tmp/q3-retail/baseq3`
(`pak0.pk3` SHA-256 `7ce8b391...`, as installed).

The probe ran `devmap q3dm1` (`setviewpos 672 1500 24 270`, looking south at
the flame wall over the health) at 960 x 540 with `fixedtime 16`, so every
frame advances the game by 16 ms. It took 24 captures 160 ms apart in the
final picture and in the light-alone view (`rtx_rt_debug_visualizer 10`).
Means are of luminance over fixed regions of the light-alone view.

| Run | Floor mean | Floor deviation over time | Correlation with the flame's brightness (lag 0 / best) |
| --- | ---: | ---: | ---: |
| Before | 0.325 | 0.56% | -0.03 / 0.11 |
| After | 0.324 | 2.02% | 0.56 / 0.75 (one capture, 160 ms) |
| After, `rtx_rt_animated_lights 0` | 0.326 | 0.54% | 0.00 / 0.18 |
| After, merged with the PBR commit | 0.308 | 2.70% | 0.76 / 0.76 |

The flame region itself, drawn by raster, varies 1.6% to 1.9% in this view
in every run. The pillar faces, which look away from the flame, stay at the
noise level (0.5% to 0.6%). The floor's average is unchanged; a single
3.8-second series covers about two cycles of the 0.6 Hz glow, so its mean
moves a few percent with the phase it starts at. The lag is the
denoiser's response to quiet light (a time constant of 0.32 s), which also
softens the frame-to-frame changes at 10 Hz.

The light build on q3dm1 took 7.1 ms before and 10.0 ms after, for the
480-sample means of 5 shaders.

### Jedi Academy mp/duel4's consoles (Live)

`textures/doomgiver/comp_panel_g` plays eight frames once at one frame a
second (`oneShotAnimMap 1`) and then holds the last, under a blinking cursor.
The probe was a `jka-mp` listen server, as a spectator at `setviewpos -2384
2300 140 90`, with 8 traced and then 8 raster captures 30 frames apart, from
15 s in:

| Run | Traced states of the screen | Raster states |
| --- | ---: | ---: |
| Before | 4 (its frames cycling) | 1 |
| After | 1 | 1 |

The traced screen now shows raster's last frame. The traced console is
brighter than raster's, because it is lit by the traced lights in place of
its lightmap.

### Cost

On q3dm1's view, in two pairs of runs in opposite orders, the median trace
was 14.63 and 14.01 ms before and 14.31 and 14.76 ms after, which is within
run-to-run noise. The CPU's "rest" stayed at 0.03 to 0.07 ms. One earlier
after run read 35 ms, and its repeats did not; it was a transient **[I]**.

### Unit and build

`tests/rtx_animated_surface_tests.py` has seven tests:

- **Frames, end to end.** The production `RT_TranslateStage` and
  `RT_AppendFrameRecords` pack stages of 1 to 64 frames. Each has three
  speeds, three phases, and the hold and frame-from-entity rules. The GLSL
  `rt_stage_frame` and `rt_stage_image`, compiled as C++, then read them back.
  The test checks 3,425,760 frame choices for the world, an Alice entity and
  a Jedi Academy `RF_SETANIMINDEX` entity against raster's rule. A time
  within single precision of a frame's start may land on either side; 72
  did, and the test holds that allowance under one in a thousand.
  `RT_StageFrameAt` matches raster exactly.
- **Frame lanes.** The entity frame lane carries frames, the no-shadow sign
  and the setAnimIndex scope through `RT_EntityFrameLane` and the GLSL
  decode.
- **Animated emitters.** With raster's wave tables, a flame shaped like
  q3dm1's gives each frame's scale as raster's stages show it over the mean,
  averaging 1 within 1% over 4,000 frames, with no file read per frame.
  Identical cross-faded frames stay steady. A lit panel follows itself lit
  (0.7 to 1.4) while its letters alone swing from below 0.05 to over 2. A
  still shader, and one whose averages are not kept, stay steady.
- **Source checks.** The remaining tests check that the C and GLSL frame
  bits agree and that the upload writes the scaled colours.

The 59 RTX and related test files pass, after one assertion followed the new
no-shadow lane. All 97 shader variants compile. `spirv-val` passes the 13
embedded path-tracer modules. The Windows x86 RTX renderer builds with the
shared build's strict warnings.

Runtimes: `.tmp/runtime/rtx-anim-before` (`affb3511`) and `rtx-anim-after`
(`affb3511` plus this change). During the task another session landed
`689ef9e6`, physically based materials for texture packs, which touches the
same renderer files. After the merge, the RTX renderer builds with the
strict warnings, the 60 RTX and related test files pass (with
`rtx_pbr_material_tests.py`), `spirv-val` passes the 13 modules, and
q3dm1's flame series repeats on the merged build (`rtx-anim-merged`, table
above). The compute backend traced every probe, with ray tracing required,
windowed on a private desktop, with no input, isolated homes and read-only
installations.

## Correction to the November 5 round

The [November 5 round](2026-11-05-rtx-round.md) took its Quake III views
through the Steam install. That install's `q3r-maps.pk3` replaces both
`q3dm7`, on which `four` plays, and `q3dm1`. So those timings and the still
view's darkening were measured on that pack's versions of the maps, not on
the retail ones. Both sides of each comparison loaded the same files, and
the darkening was a property of the history store, not of the map. The
relative results stand, but they are not retail-map evidence. The round's
record now says so.

## FnQ features

The retail engines baked a flame's light into lightmaps as a constant. Here
the path tracer, the FnQ feature, takes the light from the shader's own
animation data, adapting the profile's shaders rather than reproducing that
presentation. `rtx_rt_animated_lights 0` is the explicit option that keeps
the baked-style steady light.

## Not done

- A reflection ray does not see a raster-drawn flame. Only the light
  sample's highlight of its polygon reaches a glossy surface.
- The light follows the fold's brightness, not its colour. Under the linear
  camera the ratio is taken in display units.
- Glowing movers and models still light nothing: lights are built from the
  world alone.
- On the GPU a traced surface's Elite Force random wave is still evaluated
  as a sine (`RT_WaveFunc`). Only the lights follow it.
- Entity-chosen frames on traced models (Alice's `frameFromEntity`, a Jedi
  Academy `RF_SETANIMINDEX` mover) have unit evidence only.
- The hardware ray-tracing backend has not run any of this.
