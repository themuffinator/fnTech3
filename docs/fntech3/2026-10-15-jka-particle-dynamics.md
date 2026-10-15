# Jedi Academy particle dynamics: shader lists, motion, curves and delays, 2026-10-15

A local `mp/ffa2` join stopped on the map's torches:
`effects/env/small_fire.efx` lists several shaders for one particle, and the
effects adapter took exactly one
([spawn-effect slice](2026-10-15-jka-spawn-effect.md),
[play-blockers slice](2026-10-15-jka-play-blockers.md)). The torch effect
needs much more than a shader list, so this slice serves what its particles
use and names what stays refused. The `mp/ffa2` join now plays, with the
torches burning, in GLx, Vulkan and RTX. The
[JSON companion](2026-10-15-jka-particle-dynamics.json) records the
binaries, retail identities, commands and outcomes. No compatibility gate
closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 (and, for
one keyword, `jasp.exe`) was read with capstone. Pinned OpenJK was used only
to locate routines and name fields (reference observation; no text
incorporated). Retail behaviour wins where they differ, as noted below.

## The effect

`mp/ffa2` has 28 `fx_runner` entities, all playing `env/small_fire`. Its four
Particles (a base flame, smoke, sparks and an extra flame) use, between them:

- shader lists of one to three shaders;
- velocity, acceleration and gravity;
- rotation with a decaying rotation step (`rotationDelta`);
- `rgb`, `alpha` and `size` curves with `end`, `parm` and the curve words
  `linear`, `nonlinear`, `wave` and `clamp`;
- the `useAlpha` flag;
- the spawn flags `evenDistribution` and `rgbComponentInterpolation`;
- `delay` ranges, and `cullrange`.

Its flame shaders also carry Academy's `glow` stage keyword, which the
renderers rejected, so the flames drew as the engine's default image.

## Observed, retail `jamp.exe` 1.01

- **Media pick** (`0x405d50`). Each primitive keeps its shader list at
  template `+0x48` (impact, death, emit and play lists follow at `+0x58`,
  `+0x68`, `+0x78`, `+0x88`). Each spawn picks one handle,
  `index = ((seed >> 17) * count) >> 15`, from the effects generator
  (`0x56e760`, `seed * 0x343fd + 0x269ec3`); an empty list gives 0.
- **Key table** (`0x40e928..0x40edc0`): `delay` at `+0x2c`, `rotation`
  `+0x104`, `rotationDelta` `+0x10c`, acceleration (`0x40d810`) `+0x15c`,
  `gravity` `+0x174`. `cullrange` is compared and then skipped: its branch
  goes straight to the key loop's end (`0x40eac2` to `0x40ee17`), so nothing
  is stored and PlayEffect's range test (`0x40721b`, a field only the
  constructor sets) never culls.
- **Curve words** (`0x40cb10`): up to four words are read (`%s %s %s %s`)
  and ORed: linear 1, random 2, nonlinear 4, wave 8, clamp 12. So
  `nonlinear wave` is clamp, and a fifth word is never read. The alpha
  curve's nibble is bits 0..3 of the flags, rgb 4..7, size 8..11.
- **Scheduler** (`0x4073c6..0x40755d`). With `evenDistribution`
  (spawn flag `0x2000`) the delay of spawn `t` is
  `_ftol(t * |_ftol(max - min)| / count)`; otherwise each spawn draws its
  delay and truncates it. A delay below one creates at once, late by the
  negated delay; any other is allocated (`new`, no bound) as a scheduled
  record keeping template, due time, origin and axis.
  **AddScheduledEffects** (`0x407810`) creates each due record late by
  `now - due`, then runs the primitive pass (`0x40f320`).
- **CreateEffect** (`0x407c40`). Velocity and acceleration ranges are each
  drawn once per component (`0x408739..0x408d73`); gravity is added to world
  Z (`0x408e30`). A late moving primitive (Particle, OrientedParticle, Tail,
  Emitter) takes `v += t * a` and `org += (t + t*t/2) * v`, with
  `t = late * 0.001` (`0x408e84..0x408f29`). **GetRGB** (`0x406de0`): with
  `rgbComponentInterpolation` (spawn flag `0x4000`) one fraction serves all
  six colour ranges; without it each is drawn on its own.
- **FX_AddParticle** (`0x40f480`). A wave parm is multiplied by
  `0.0031415901` (`0x5664d8`); any other curve parm becomes the absolute
  time `life * parm * 0.01 + now`.
- **CParticle::Update** (`0x401730`). A particle created before this frame
  moves (`0x401a50`: velocity takes `realTime * acceleration` first, then
  the origin moves by `realTime * velocity`). If it is not culled, it updates
  size (`0x401e50`), RGB (`0x402040`), alpha (`0x402390`) and rotation
  (`0x402640`), then draws.
- **Curves.** All three use one law in binary32:
  - linear: `p = 1 - age / life`;
  - nonlinear: `q = 1 - (now - parm) / (end - parm)` once `now > parm`;
  - clamp: `q = (parm - now) / (parm - start)` before `parm`, then 0;
  - linear with nonlinear or clamp: `p = (q + p) / 2`; either alone: `p = q`;
  - wave: `p *= cos(age * parm)`;
  - random: `p` scaled by a fresh draw.

  The value is `start * p + end * (1 - p)`. RGB is clamped to [0, 1], times
  255, and rounded by `fistp`, and the alpha byte is set to 255. Alpha is
  clamped, times 255, clamped and truncated (`_ftol`). With `useAlpha`
  (`0x8000000`) it is written as the alpha byte; otherwise RGB is scaled by
  `alpha >> 8`.
- **Rotation** (`0x402640`): `rotation += frameTime * step * 0.01`, then
  `step *= 1 - frameTime * 0.0007`.
- **`glow`.** Both executables' ParseStage read it as a stage keyword:
  `jamp.exe` `0x4b3fd4` and `jasp.exe` `0x47a188` set the stage's byte at
  `+0x88` and read on.

## Change

`code/compat/jka_effect_play.hpp`:

- compiles every visual primitive's shader list and picks one per spawn;
- compiles a Particle's motion, rotation step, curves (`end`, `parm` and the
  curve words), `useAlpha`, delays, and the two spawn flags;
- accepts `cullrange` on any primitive, inert as in retail;
- adds the curve law, a particle's frame look, its rotation and motion,
  the late-spawn advance and the delay rule.

`code/client/client_jka_cgame_effects.hpp`:

- keeps an owned schedule of delayed spawns, created at the start of each
  normal pass, late by their lag;
- moves particles in the primitive pass;
- draws each particle with this frame's size, colour, alpha and rotation;
- reports waiting spawns in `fx_debug`'s retail "Scheduled FX" count.

All three renderers' shader parsers now accept `glow` for the two Academy
profiles.

Still refused by name:

- the `random` curve word;
- particle physics (`usePhysics` and the other particle flags);
- curve words on every family but Particle;
- the families and fields the sweep below lists.

**Fn presentation (invariant 11):**

- Particles stay Fn sprites (`RT_SPRITE`). Fn's shaders, fog, sorting and
  post-processing apply.
- A `glow` stage draws as any other stage. Fn's bloom, where enabled, stands
  in for retail's glow buffer.

Fn adds no retail glow pass and no retail-exact option.

**Engine choices that differ from retail:**

- Fn draws from its own generator, not retail's, so the picks are equally
  likely but not retail's sequence.
- A paused frame keeps waiting spawns until the clock runs again. Retail's
  pass would consume them and create nothing.
- The schedule is bounded at 8,192 waiting spawns. Beyond that a new
  delayed spawn is dropped and counted, and `fx_debug` prints the count;
  retail allocates without a bound.
- Rotation advances each frame a particle is drawn, after Fn's near cull.
  Retail's cull also skips particles behind the viewer.

## Unit (Source)

`tests/jka_particle_dynamics_tests.py`, plain and with `-ffast-math`, covers:

- an independently authored torch-shaped declaration and its compiled data;
- the curve words, and twelve named refusals;
- shader picks over 3,000 spawns;
- component interpolation, with and without the flag;
- the curve law: linear, nonlinear, clamp, wave, and linear with each;
- a frame's bytes with and without `useAlpha`;
- rotation decay, motion, the late advance, and both delay rules.

Through the actual imports, a played torch:

- spawns one particle and schedules three at 50, 100 and 150 ms;
- draws with the measured bytes and rotation, and moves under gravity;
- creates its waiting spawns on time, and a late one advanced by its lag;
- keeps them over a paused frame;
- runs its curves to the measured bytes at 300 ms;
- draws all three shaders over repeated plays.

`Free` drops the schedule, and the bound drops and counts. A source check
holds the `glow` branch in all three renderers.

The existing effect tests were updated where the subset grew:

- delays are served, so the refusal cases now use one beyond the Fn bound
  (`tests/jka_effect_play_tests.py`, `tests/jka_line_effect_tests.py`,
  `tests/jka_spawn_effect_tests.py`);
- particle velocity is served, so that refusal case is now `usePhysics`.

The four shader-parse fixtures that slice ParseStage still pass.

## Sweep (Source)

Every primitive of the 376 installed effect files was compiled through the
adapter at HEAD and with this change, using placeholder media handles. This
is a per-file check: it does not follow an effect's children.

- 15 files compiled completely before; 68 do now.
- No file that compiled before refuses now.

The first refusals left are, by count:

- Tail family (30);
- Line curve words (28);
- Particle `bounce` (25);
- Line flags (24);
- the `forcefeedback` group (21);
- Emitter models (18);
- Sound family (17);
- Particle flags (14);
- Particle `radius` (14);
- Electricity `bounce` (12).

## Live (Live subset)

Runtime `.tmp/runtime/jka-spawn-effect`, rebuilt in place. Its binaries come
from a `git archive` of HEAD `599b09af`, built with `win32-dev`'s options
(strict warnings). The export carries this slice's hunks and, as a labelled
overlay, the "Compatibility gap resolution" session's uncommitted
client-game import 51 (local ambient sets). Without that overlay, HEAD stops
at import 51 before any torch plays (run `glx-ffa2-1`). The overlay is five
files whose hashes are in the companion.

The harness is `.tmp/jka-multishader/listen.py`: `.tmp/jka-emitter/live`'s,
with the map from `JKA_MAP`. Each run is windowed, loopback only, with no
input, on a private desktop, `cg_renderToTextureFX 0`, `cl_menuBlur 0` and an
isolated home.

- **Before `glow`:**
  - `glx-ffa2-2`: a joined player plays 517 torch effects with up to 13
    spawns waiting, and runs to its scripted end.
  - `glx-ffa2-views`, `glx-ffa2-shaders`: a spectator view of two torches
    shows default-image squares; `shaderlist` names `gfx/effects/fire2..4`
    DEFAULTED.
- **With `glow`:**
  - `glx-ffa2-glow`: those shaders load and no `glow` warning remains.
  - `glx|vk|rtx-ffa2-final`: each renderer's close capture
    (`torch-1.png`) shows the flame over the sconce, and the
    two-torch view (`pair.png`) shows both burning. In the RTX capture the
    room around the fire is much darker; not investigated.
  - `glx|vk|rtx-ffa2-play`: a joined player runs each renderer's scripted
    session to its end, playing 222, 222 and 359 torch effects, with no
    error.

Retail installation `E:\Games\Jedi Academy` was read only; its inventory is
unchanged after every run.

## Open

- No capture compares a torch with retail's.
- Fighting, other maps' effects, and the families and fields above remain
  unchecked or refused.
- OrientedParticle, Line, Cylinder and Electricity curves beyond the
  existing subset still refuse.
- Fn has no counterpart of retail's dynamic glow beyond its bloom.
