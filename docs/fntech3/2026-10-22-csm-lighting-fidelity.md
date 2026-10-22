# Cascaded sun shadows: lighting fidelity and forest comparison

This follows the [shadow format integration](2026-10-22-fnq-shadow-formats.md)
and [portal-fog/caster-parity correction](2026-10-22-portal-fog-caster-parity.md).
It changes the shared CSM policy in GLx, Vulkan and RTX raster
(`rtx_rt_mode 0`). The path tracer is a separate rendering path. No game
module ABI, map format, gameplay contract or compatibility gate changes.
The [JSON companion](2026-10-22-csm-lighting-fidelity.json) records commands,
binary identities, capture hashes, counters and validation results.

## What the forest comparison established

At the fixed RTCW `forest` starting view, enabling CSM previously darkened
fixed-brightness tree materials and nearly removed the player's entire
lighting. Alpha-tested tree cards cast solid silhouettes. This was a
receiver/material error, not evidence that the original level intended
those materials to become black.

The installed BSP worldspawn supplies `ambient 25` and `_color 1 1 1`.
Its sun is already interpreted in Wolfenstein's compiler units. The earlier
record's observation that the 149 runtime tree entities have no baked ground
shadows remains valid; its suggestion that almost all light could therefore
be removed did not account for the authored ambient or cutout coverage.

The corrected view retains the tree artwork and the player's ambient light.
Tree shadows on the ground and player contain gaps matching the leaves.
Ground beneath dense foliage remains darker than the CSM-disabled scene;
this follows the level's ambient/sun ratio, without a special forest override.

## Shared implementation

- A material receives sun shadows according to its lighting stage, rather
  than merely because it belongs to a model. Diffuse-lit models receive
  both world and entity shadows. Fixed-brightness, constant-colour and
  entity-colour artwork does not acquire an invented sunlight contribution.
- The model light calculation snapshots the estimated sun contribution
  before adding dynamic lights. A receiver removes only that contribution,
  bounded by its total diffuse light and ambient floor, including byte
  saturation. The existing vertex colour stream carries this fraction.
- Baked receivers retain the two atlas rows: they receive entity shadows
  only where the world's row leaves them in sunlight. The removable fraction
  is `sun / max(baked, ambient + sun, 1/255)`, per channel, with sun facing
  applied first. This preserves authored ambient when overbright conversion
  saturates the baked sample. Ritual's compiled world models keep their
  baked vertex-lighting policy.
- Cutouts retain their material's alpha test, texture coordinates and
  animation. Deformed materials retain their geometry calculation in both
  casting and receiving passes. Non-depth-writing detail/glow stages cannot
  fill the holes. Opaque materials keep the shared depth-only batching path.
- Animated coverage, deforms and mutable skeletal poses are not reused as
  static cache entries. This includes the runtime MDR used for Ghoul2 poses.
  Material remaps participate in cache identity and receiver classification.
- Conservative camera-depth bounds reject receivers outside a cascade;
  rotated/scaled and boundary-crossing bounds remain eligible. Unknown model
  bounds and shader-deformed geometry remain conservative. Existing PVS,
  light-space culling and atlas caching remain in use.
- CSM allocation and planning use the same GPU attachment limit. The old
  2048-pixel limit belongs to the CPU image resampler and stays in place for
  image uploads. It no longer silently reduces four requested 1024-pixel
  cascades to 512. The default four-cascade, two-row depth atlas grows from
  8 to 32 MiB at four bytes per texel; users can select 512 to retain the
  smaller allocation. Device limits still reduce unsupported sizes.
- A successful sun, point-light or spotlight atlas publication sets
  `cg_shadows` to `0` to avoid overlapping game-owned shadows. Requests,
  failed allocation, absent sun and no-world views do not trigger this.
  Cached atlases do, and re-enabling legacy shadows while maps are active
  resets the value again. No previous setting is restored automatically.

The light grid contains aggregate lighting and one dominant direction, not
an independent sun channel. The model sun split is therefore a bounded
estimate using its alignment with the map's sun, not an exact recovery of
the compiler's light list. It does not claim physically based relighting.

## Validation

<!-- CSM_VALIDATION -->
Windows x86 `win32-dev`, MSVC debugoptimized, NVIDIA GeForce RTX 4060 Laptop
GPU (OpenGL driver 591.74): all three renderer modules built successfully.
The final available Meson selection passed **275 tests**, with one OpenAL
loopback test skipped because `ALC_SOFT_loopback` was unavailable. The focused
final lighting/profile run passed **39 tests and 65 subtests**; the IHV and
Elite Force lighting follow-up passed **7 tests and 12 subtests**. The CSM
cache source contract also passed. Embedded shader checks compile and compare
the GLSL and SPIR-V. The full recursive Python compatibility suite was not run.
Player-manual, generated-document and changelog checks passed 25 tests and
841 subtests. Disposable homes and scratch were removed after preserving
engine captures, logs and launch scripts.

**64 isolated runtime probes and 162 engine captures** passed the harness's
exit, error, binary-stability and installation-stability checks. Of these,
43 form the comparison matrix, 15 verify the legacy-shadow policy, and six
repeat `forest` and the indoor `shield` view after final material review.
Process success alone is not a visual-quality claim; the scene limits below
apply to the observations.

| Profile | Scene | Observation / limit |
| --- | --- | --- |
| RTCW SP | `forest` | Same view in all three renderers; ambient and fixed-brightness foliage retained, cutout ground/player shadows. |
| ET | `oasis` | Sun shadows on the lit player/world; skeletal casters refresh. |
| F.A.K.K.2 | `fakkhouse`, `shield` | Outdoor entity shadows; indoor lamp-lit view remains substantially unchanged. |
| Alice | `grounds1` | Entity-sun path casts and receives; moving actors limit pixel comparisons. |
| MOHAA | `m5l1a` | Soldier casts a sun shadow while retaining lighting; actor motion limits pixel comparisons. |
| Q3 | `q3dm6` | Fixed indoor player view preserves lighting with CSM enabled. |
| QL | `hiddenfortress` | Gameplay smoke and captures; random spawn prevents renderer-count comparison. |
| JKA MP | `mp/ffa2` | Fixed view, runtime Ghoul2 pose invalidates the static cache. |
| Elite Force MP | `ctf_and1` | CSM atlas and receivers active; spawn fade makes brightness metrics unsuitable. |
| RTCW MP | `mp_tram` | No-sun fallback; limbo menu obscures the main view. |
| JK2 MP | `duel_jedi` | Atlas/receiver smoke; player-configuration menu obscures most of the scene. |
| Q3Test 1.08 | `q3test2` | Gameplay smoke; final HUD view replaces the per-frame CSM diagnostics. |

JKA SP, JK2 SP, Elite Force SP and Q3 IHV were not runtime-tested in this
matrix. IHV's separate model-lighting contract has no independent sun budget;
its numerical tests confirm that a reused entity cannot inherit one.

At the forest view, all three renderers report **335 caster submissions**,
**57 world receivers and 84 entity receivers**, versus the prior RTX raster
view's 104 world and 576 entity receivers. Total receiver submissions fall
from **680 to 141 (79.3%)**. Counts include cascade submissions, not unique
objects. This is no claim of a measured frame-rate increase. Default cascades
are now actually 1024 pixels; the explicit 512-pixel control also passed.

True `r_csmShadows 0` controls agree with the strength-zero static scene to
within two 8-bit channel values in GLx and one in Vulkan/RTX; the comparison
excludes the animated player and HUD. The prior implementation's whole-model
darkening and solid leaf cards are visible in the retained baseline captures.

The 15 policy probes begin with `cg_shadows 3`. In each renderer, CSM and
point-light maps change it to `0`, and enabling `3` again during active CSM
resets it again. All maps disabled, zero-strength CSM, and a no-sun map with
local shadow maps disabled retain `3`. Unit fixtures also cover spot maps,
unpublished/failed atlases, no-world views and absent cvar state.

An earlier 432-test focused run and the first Meson run encountered one RTX
contract failure while another chat temporarily restored its baseline shader
for measurement. Its restored implementation passed the 32-test contract
rerun and the clean 275-test Meson selection above. Intermediate fixture
failures were corrected and rechecked; they are not reported as final passes.
<!-- /CSM_VALIDATION -->

Probes use the fixed `meson/build/win32-dev/fntech3.exe` path, isolated homes
under `.tmp/csm-design/`, windowed 800x600 clients on an unswitched private
desktop, and the engine's `screenshotPNG` command. They inject no mouse or
keyboard input. Installed game files are read-only; the harness checks their
file sizes and modification times and checks that binaries do not change
during a run. The companion additionally identifies the map inputs by hash. Captures,
logs, exact launch scripts and analysis are retained in `.tmp/csm-design/`.

## Scope and limitations

This uses fnTech3's shadow atlas, light grid, per-pixel dynamic lighting and
material system across game profiles. It keeps each game's supplied sun,
ambient and shader semantics. `r_csmShadows 0` remains the documented option
for the level's original baked presentation; the default is unchanged.

Transparent effects, sprites, decals and view weapons retain the existing
caster exclusions. The comparison does not establish every map, material
combination, profile's gameplay, animation or interoperability gate. The
default atlas's sharper resolution is a quality improvement with a memory
and fill-rate cost; reduced receiver submissions are not an FPS benchmark.

The standard Vulkan global-fog compositor difference and Quake Live's
`Hunk_FreeTempMemory` shutdown warnings remain separate known issues. The
concurrent RTX quality/FPS work owns the path-tracing changes in the shared
checkout; its modifications are not part of this CSM implementation.

## Source observations

Ambient semantics were observed in the local Quake III compiler's
`q3map/light.c`, `LightWorld`, and NetRadiant's
`tools/quake3/q3map2/light.c`, the `_ambient`/`ambient` and `_color` handling.
Ritual and MOHAA use the profile-owned ambient readers already documented in
this repository. These are format observations; no external implementation
text was incorporated. Local reference paths and hashes are in the companion.
