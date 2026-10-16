# Raven's alphaFunc GE192, 2026-10-16

The [surface sprites slice](2026-10-15-jedi-surface-sprites.md) left one
word of the Jedi games' shaders unread: `alphaFunc GE192`. Fn's parser
knew only Quake III's `GT0`, `LT128` and `GE128`, so the word warned and
the stage drew with no alpha test at all. Most stages that use it are
grass, reeds and vines drawn as surface sprites, including the grass on
Jedi Academy's `mp/ctf3`. This slice measures what the four Jedi engines
do with the word, has `jka-sp`, `jka-mp`, `jk2-sp` and `jk2-mp` read it
so, and draws it through Fn's existing comparison-and-reference alpha test
in all three renderers. The
[JSON companion](2026-10-16-jedi-alphafunc-ge192.json) records the
binaries, identities, addresses, counts, commands and outcomes. No
compatibility gate closes.

Implementation is independently authored. The retail executables were read
with capstone and pefile; nothing else was consulted.

## What the retail engines do

### Observed

Each engine's `NameToAFunc` compares the word, without regard to case,
against four names and returns a state bit for each. Its `GL_State` masks
those four bits (`0xf0000000`) and sets the alpha test from them.

| Engine | `GE192` string | Returns `0x80000000` | `GL_State` mask test | `glAlphaFunc` for it |
| --- | --- | --- | --- | --- |
| `jamp.exe` 1.01 | `0x555eb8` (used at `0x4b24cb`) | `0x4b24da` | `0x48a426` | `0x48a4c2` |
| `jasp.exe` 1.01 | `0x50cbb0` (used at `0x4785df`) | `0x4785f0` | `0x454b56` | `0x454bf2` |
| `jk2mp.exe` 1.02a | `0x503124` (used at `0x48dc61`) | `0x48dc72` | `0x46d33c` | `0x46d3d8` |
| `jk2sp.exe` 1.02a | `0x4e4984` (used at `0x47da91`) | `0x47daa2` | `0x45c35c` | `0x45c3f8` |

- **The four names.** `GT0`, `LT128`, `GE128` and `GE192` return
  `0x10000000`, `0x20000000`, `0x40000000` and `0x80000000` in all four
  engines. Any other name warns and returns 0, so the stage has no test.
- **The test.** For `0x80000000`, `GL_State` enables `GL_ALPHA_TEST`
  (`0x0bc0`) and calls `glAlphaFunc( GL_GEQUAL, 0.75f )`: it pushes
  `0x3f400000` and `0x0206`. For comparison, `GE128` pushes `0x3f000000`
  (0.5) with the same function.
- **Where it is read.** `ParseStage`'s `alphaFunc` branch hands the next
  word on the line to `NameToAFunc` and keeps the result as the stage's
  alpha test bits (`jamp.exe` `0x4b38fd`-`0x4b3927`).

### Inferred

- A pixel draws when its alpha is at least 0.75, which in an 8-bit
  texture is an alpha of 192 or more, hence the name.

## The installed shaders

| Title | Stages | Definitions | Surface sprite stages | Other stages |
| --- | --- | --- | --- | --- |
| Jedi Academy | 44 | 32 | 38 | 6 |
| Jedi Outcast | 23 | 15 | 22 | 1 |

- **Counting.** Academy's scripts name `GE192` on 45 lines; one of them,
  in `shaders/imperial.shader`, is commented out.
- **The other stages.** Academy's two `textures/bounty` flags, its
  `textures/vjun/grate`, `grate1` and `textures/wedge/w_catwalk` cut-outs,
  and the antenna glow `models/map_objects/doom/antenna`, which Outcast
  has too.
- **`mp/ctf3`.** Its `textures/yavin/ground_grasssprite` grass stage is
  `alphaFunc GE192` with `blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA`
  and `depthWrite`. Of its texture's (`gfx/sprites/grass2.tga`, 512 by
  512) texels, 77.5% have alpha 0, 3.5% alpha 1 to 191 and 18.9% alpha 192
  or more.

## What fnTech3 does now

- **Parser.** Under `jka-sp`, `jka-mp`, `jk2-sp` and `jk2-mp`, each
  renderer's `NameToAFunc` (`code/renderer`, `renderervk` and
  `rendererrtx` `tr_shader.c`) returns `GLS_JEDI_ATEST_GE_C0` for `GE192`
  in any case (`R_JediAlphaFuncGE192`, `code/renderercommon/tr_jka_shader.h`).
  Every other profile still warns and draws the stage untested, as its own
  engine would: Elite Force and the id engines know only the three names.
- **The state bits.** `GLS_JEDI_ATEST_GE_C0` is the comparison-and-reference
  alpha test Fn already has for Alice's `alphatest`
  (`GLS_ALICE_ATEST_FUNC`/`REF` in each `tr_local.h`): comparison 5,
  greater or equal, and reference 192. Each back end applies the reference
  as 192 / 256, exactly 0.75: `GL_State` with `glAlphaFunc`, and the
  Vulkan and RTX pipelines as their fragment shaders' discard of the
  opposite comparison.
- **The GL renderer's CPU path.** The GL renderer's static world buffers
  and its GLX streamed draws run programs that know only Quake III's three
  tests. `FinishShader` therefore marks a Jedi stage carrying the bits as
  drawn on the CPU (`aliceDynamic`) and its shader as kept off the static
  buffers (`aliceNoStatic`), as `R_AliceFinishShaderStages` does for
  Alice's stages. A sprite stage's own shader is finished the same way. The
  Vulkan and RTX renderers need no such rule: their pipelines read the bits
  wherever the stage is drawn.

### FnQ3/FnQL features (invariant 11)

- **Used.** The stage is drawn by each renderer's own pipeline with Fn's
  alpha test, and with alpha to coverage where `r_ext_alpha_to_coverage`
  and multisampling are on, which Fn's Vulkan path keeps for a greater or
  equal test.
- **Left out, as for Alice's alpha tests.** Three Fn features that look
  for Quake III's three tests do not see this one: the point-light shadow
  pass (`tr_dlight_shadow_casters.h`) and `r_shadowCorrectness` treat such
  a surface as solid rather than leaving it out, and the RTX path tracer
  hands a shader with it to its raster pass instead of tracing it. Of the
  installed stages only Academy's three grates are opaque cut-outs that
  this can touch; they drew solid before this slice.

## Evidence

### Tests

`tests/jedi_alphafunc_ge192_tests.py` (2 tests, 3 renderer subtests):

- **The whole production parser** (`alice_shader_parse_tests`' host) for
  each renderer under all sixteen profiles. Quake III's three names keep
  their bits everywhere, and an unknown name still only warns. Under the
  four Jedi profiles `GE192` and `ge192` store comparison 5 and reference
  192 with no warning, and a surface sprite stage keeps them. Every other
  profile warns and stores no test. In the GL renderer, `FinishShader`
  marks a `GE192` stage and its shader for the CPU path, does the same for
  a sprite stage's own shader, and leaves a `GE128` stage on the static
  path.
- **The back ends.** Each reads the comparison and the reference from the
  same bits as reference / 256, and the GL static and GLX streamed paths
  defer a marked stage.
- **Mutations.** The same checks fail against the parser at `dfe1b9d4`
  (the warning, no bits) and against this slice's parser without the GL
  `FinishShader` rule (no CPU path).

### Every installed Jedi definition through the production parser

A census of every installed Academy and Outcast shader definition through
each renderer's production parser, keeping each definition's first warning
(`.tmp/jedi-alphafunc/census.py`), run on an export of `dfe1b9d4` and on
the same export with this slice's files alone:

| Profile | Definitions | Refused | First warnings | `GE192` warnings |
| --- | --- | --- | --- | --- |
| `jka-sp` | 3,391 | 40, unchanged | 190 to 159 | 31 to 0 |
| `jka-mp` | 3,391 | 40, unchanged | 190 to 161 | 29 to 0 |
| `jk2-sp`, `jk2-mp` | 1,187 | 25, unchanged | 30 to 15 | 15 to 0 |
| Quake III control (each script set) | | unchanged | unchanged | unchanged |

- **Alike in GLx, Vulkan and RTX.** The three renderers give the same
  counts and the same definitions.
- **Only these warnings.** Every first warning that goes is a `GE192`
  warning, no definition's warning changes, and none is added.
- **The rest.** A definition whose first warning is something else still
  warns for it: 32 Academy definitions use the word, and 31 or 29 had it
  as their first warning.

### Live

Before and after runtimes from one export of `dfe1b9d4`, differing only in
the three renderer DLLs (`.tmp/runtime/jedi-ge192-before` and `-after`;
`fntech3.exe` is the same file). Each ran `jka-mp` `mp/ctf3` windowed on a
private desktop with `net_enabled 0` and no input, joined as a spectator
and stood at `setviewpos -1088 -1150 150 90` over the grass field, then
took three engine `screenshotPNG` captures 60 frames apart.

- **The warning.** Every before run printed `invalid alphaFunc name
  'GE192'` for `textures/yavin/ground_grasssprite`; no after run did.
- **Before.** The blades' transparent parts still wrote depth, so nearer
  sprites hid the blades behind them, and the middle of the field drew as
  a murky band with soft fringes.
- **After.** The blades are cut cleanly at their edges, the field reads
  through to its far side, and the murky band is gone, in GLx, Vulkan and
  RTX.
- **Pixels.** The grass sways between captures, so per-pixel differences
  are as large within a run as between runs. The field band's mean
  brightness is steady within each run (at most 0.2 of 255 apart) and rises
  from 41.1 to 46.3 in GLx, 40.7 to 47.1 in Vulkan and 39.8 to 47.0 in RTX.
  GLx and Vulkan agree after the change: 1.7% of the band differs between
  them.
- **Clean runs.** The installation and binaries were unchanged and the
  runs ended cleanly.

Not checked live: both single-player profiles still stop at their module
boundary and the Outcast multiplayer client at client-game import 300, and
no multiplayer map uses the grates or flags, so they have only the unit
tests.

## Remaining and unrelated findings

- **Fn's other alpha-test checks.** The point-light shadow pass,
  `r_shadowCorrectness` and RTX's tracing decide whether a surface is
  masked from Quake III's three tests alone, for Alice's alpha tests and
  now `GE192` alike. Teaching them the comparison-and-reference bits would
  change Alice and F.A.K.K.2 too, so it is left to its own slice.
