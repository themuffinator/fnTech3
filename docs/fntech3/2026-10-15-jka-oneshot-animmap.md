# Jedi Academy one-shot animations: `oneShotAnimMap` and `clampAnimMap`, 2026-10-15

The [tails-and-physics slice](2026-10-15-jka-tails-and-physics.md) found
explosions drawing as a grey square. Academy's explosion shaders animate with
the `oneShotAnimMap` stage keyword, which no renderer read: ParseStage
rejected it ("unknown parameter 'oneshotanimmap'") and the whole shader
defaulted. A `jka-mp` `shaderlist` named `gfx/exp/explosion1`, `explosion3`,
`slower_rocket_explosion`, `rocket_explosion`, `wookie` and `demp2`
DEFAULTED. All three renderers now read the keyword, and its sibling
`clampAnimMap`, for the two Academy profiles. The explosions load and play
their frames once in GLx, Vulkan and RTX. The
[JSON companion](2026-10-15-jka-oneshot-animmap.json) records the binaries,
retail identities, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 and
`jasp.exe` were read with capstone. Pinned OpenJK was used only to locate
the routines (reference observation; no text incorporated).

## The shaders

`assets1.pk3` `shaders/explosions.shader` gives each `gfx/exp` explosion two
stages. Each stage plays seven or eight frames with `oneShotAnimMap` at 3 to
17 frames a second. The last frame is `gfx/colors/black.tga`, which adds
nothing under the stages' `blendFunc GL_ONE GL_ONE`. The two stages
crossfade with sawtooth `rgbGen` waves. The installed scripts also use
`oneShotAnimMap` for the `*_onoff` door and switch textures, and
`clampAnimMap` for two textures. `effects/chunks/sparkexplode.efx` plays
`gfx/exp/explosion1` on a `setShaderTime` particle that lives 1.2 seconds.

## Observed, retail

**ParseStage.** `jamp.exe` compares the token with `animMap` (`0x4b3851`),
`clampanimMap` (`0x4b386a`) and `oneshotanimMap` (`0x4b3883`), and all three
go to one branch (`0x4b426c`; `jasp.exe` `0x47a42e`). The comparisons ignore
case. In that branch:

- `clampanimMap` loads every frame with wrap mode `0x2900` (`GL_CLAMP`);
  the other two use `0x2901` (`GL_REPEAT`) (`0x4b42f3..0x4b42fb`).
- The branch writes the stage's one-shot byte (stage `+0x1d`, the first
  bundle's `+0x19`) for every animMap form: set for `oneshotanimMap`, clear
  otherwise (`0x4b42c4`; `jasp.exe` `0x47a47d`).
- The speed goes to bundle `+0x14` (`0x4b42b6`).
- Up to 32 frames are loaded: the count (a word at bundle `+0x12`) is
  compared with `0x20` (`0x4b42e4`; `jasp.exe` `0x47a497`). Names beyond it
  are read and skipped without a warning.
- A missing speed prints the missing-parameter warning with the two names
  swapped (`0x4b44ed`) and fails the stage.

**R_BindAnimatedImage** (`jamp.exe` `0x4ac3d0`, `jasp.exe` `0x472fd0`; the
bundle arrives in `esi`):

1. A video map runs and uploads its cinematic. A lightmap bundle under
   `r_fullbright` binds white. A bundle of one frame binds it.
2. If the current entity's renderfx has `0x80000` (`jasp.exe`: `0x20000`;
   OpenJK's `RF_SETANIMINDEX`), the index is the entity's `skinNum`
   (refEntity `+0x84`; `jasp.exe` `+0x68`).
3. Otherwise the index is `fistp(shaderTime * speed * 1024.0) >> 10`
   (`0x4ac440..0x4ac462`; `jasp.exe` calls its `Q_ftol` at `0x477a00`), and
   a negative index becomes 0. `shaderTime` is the stage's clock: the
   entity's, for a particle whose effect sets `setShaderTime` at creation
   (served since the tails slice).
4. A one-shot bundle whose index has reached the count binds its last frame
   (`0x4ac46b..0x4ac483`; `jasp.exe` `0x473058`). Any other bundle binds
   frame `index % count` (`0x4ac48b`).

## Change

`code/renderercommon/tr_jka_shader.h` gains the measured 32-frame bound and
two keyword checks that answer only for `jka-sp` and `jka-mp`. In all three
renderers:

- `tr_shader.c`: the `animMap` branch also takes `clampAnimMap` and
  `oneShotAnimMap` for Academy. It writes the bundle's new
  `jkaOneShotAnimMap` bit every time, loads `clampAnimMap`'s frames clamped
  to their edges, and keeps up to 32 frames for Academy (other profiles keep
  their own bounds).
- `tr_local.h`: the texture bundle gains `jkaOneShotAnimMap`.
- `tr_shade.c`: frame selection holds a one-shot bundle on its last frame,
  as Alice's `animMapOnce` already does. In Vulkan this is
  `R_AnimatedImageDescriptor`. When Vulkan or RTX collapse two stages into
  one, the bit travels with its bundle.

No other profile reads either keyword, and no other profile's frame bound
changes.

**Fn presentation (invariant 11).** The keyword decides which image a stage
binds. Fn's shaders, fog, sorting, bloom and post-processing draw the
explosion as they draw any stage. Particles stay Fn sprites with the
entity's float shader clock. Nothing is left out, and there is no
retail-exact option, because there is no presentation to choose between.

**Engine choices that differ from retail:**

- Fn truncates `shaderTime * speed`. Retail rounds `shaderTime * speed *
  1024` and shifts by ten. Fn's frame changes can be up to 1/1024 of a frame
  earlier.
- Fn warns when an animMap lists more frames than its bound; retail skips
  them silently.
- Fn's missing-parameter warning names `animMap`; retail swaps the names.

**Not served:** `0x80000` (`RF_SETANIMINDEX`). The cgame sets it, with
`skinNum` as the frame, on `EF_SHADER_ANIM` movers. Those are mostly the
`*_onoff` doors and switches that use `oneShotAnimMap`. The render adapter
still refuses that renderfx bit, so such an entity is refused rather than
drawn with a time-driven frame.

## Unit (Source)

`tests/jka_oneshot_animmap_tests.py` compiles each renderer's own parser
branch and frame selection, sliced from `tr_shader.c` and `tr_shade.c`, with
the real tokenizer. For every profile:

- `oneShotAnimMap`, `oneshotanimmap` and `ONESHOTANIMMAP` parse only for
  `jka-sp` and `jka-mp`, set the bit and load repeat-addressed frames.
- `clampAnimMap` loads clamped frames and leaves the bit clear.
- A plain `animMap` clears a set bit and keeps the profile's bound: 32 for
  Academy, as before for the rest.
- A missing speed fails.

Frame selection, speed 7 over eight frames: before one second the looped
and one-shot bundles agree (0, 3 at 0.5 s, 7 at 1.0 s). After that a
one-shot bundle stays on 7, while a looped one wraps (0 at 1.15 s, 6 at
10 s). Alice's `animMapOnce` still holds. A one-frame bundle binds its only
frame.

`tests/shader_animation_tests.py` now finds the widened branch and checks
the Academy profiles' 32-frame bound next to the other profiles' bounds.
The other tests that slice ParseStage or the frame selection
(`alice_shader_parse_tests.py`, `model_shader_tests.py`,
`rtcw_shader_material_tests.py`, `rtcw_snooper_view_tests.py`, the IHV,
Q3Test and Elite Force shader tests and
`jka_particle_dynamics_tests.py`) still pass.

## Live (Live subset)

There are two runtimes from a `git archive` of `fbde92d4`, built with
`win32-dev`'s options and strict warnings:

- `.tmp/runtime/jka-oneshot-before`, HEAD;
- `.tmp/runtime/jka-oneshot`, the same export with this slice. Only the
  renderer DLLs differ; `fntech3.exe` is byte-identical.

The harness is `.tmp/jka-oneshot/listen.py`, the tails slice's listen
harness. Each run is windowed, loopback only, with no input, on a private
desktop, with an isolated home. An entity override
(`.tmp/jka-oneshot/ffa2-oneshot.ent`) adds one `fx_runner` playing
`chunks/sparkexplode` every 2.5 seconds, 150 units ahead of a spectator at
`setviewpos -2014 1536 90 180`. The run sets `cg_draw2D 0` and
`timescale 0.5`, and captures every 12 frames, about 100 ms of game time.

- **`shaderlist`.** Before, every renderer prints the oneshotanimmap warning
  seven times and lists all six `gfx/exp` shaders DEFAULTED. After, there
  are no warnings. GLx lists the six with two stages; Vulkan and RTX list
  each as one collapsed multitexture stage.
- **Captures.** Before, each play draws the engine's default square. After,
  it draws the explosion (`runs/renderers-before-after.png`).
- **Playing once.** The particle lives 1.2 s, and the stages reach their
  black last frame after 1.0 s. Before, the square stays lit until the
  particle dies, for 12 captures (GLx and Vulkan 09-20, RTX 14-19). After,
  each explosion goes dark two or three captures earlier, while the particle
  is still alive (GLx 09-17, Vulkan 09-18, RTX 13-17), and stays dark until
  the next play. With looping frames, the last 0.05 s would show frame 0
  again under a near-full inverse-sawtooth stage.
- Every run reaches its scripted end with no error, and the binaries'
  hashes do not change.

An earlier GLx run with the tails slice's five-runner override
(`glx-after-1`) also lists the six shaders with two stages each and no
oneshotanimmap warning. Retail installation `E:\Games\Jedi Academy` was read
only; its inventory is unchanged after every run.

## Open

- `RF_SETANIMINDEX` movers (the `*_onoff` doors and switches) are refused by
  the render adapter.
- No capture compares an explosion with retail's.
- `jka-sp` shares the parser and frame selection but was not probed live.
- Other shader warnings seen in the same logs remain: the `random` wave
  function (`gfx/misc/red_dmgshield`, `powerups/rezout`), `depthFunc
  disable` (`gfx/effects/whiteFlash`) and the `sun` keyword
  (`textures/mp/s_kor_sky`).
