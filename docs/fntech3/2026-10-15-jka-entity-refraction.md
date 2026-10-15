# Jedi Academy entity refraction, 2026-10-15

With `cg_renderToTextureFX` at its retail default of 1, Jedi Academy's
multiplayer client game draws a second, half-size copy of the spawn shield
with renderfx `0x2000` (distortion). The [play-blockers
slice](2026-10-15-jka-play-blockers.md) validated that copy and left it out,
because fnTech3 had no entity refraction pass. This slice gives it one in
GLx, Vulkan and RTX, built on fnTech3's liquid refraction rather than on the
retail technique. The [JSON companion](2026-10-15-jka-entity-refraction.json)
records the binaries, commands, measurements and outcomes. No compatibility
gate closes.

Retail `jamp.exe` 1.01 and the 1.01 `cgamex86.dll` were read with Capstone,
with OpenJK's renderer as a map to the code. The retail binaries outrank it.
Implementation is independently authored.

## Retail (Observed)

**The surface list** holds back an entity whose renderfx has any of
`0x202400` (`0x48aba6`) until the rest of the view is drawn. That covers
forced alpha (`0x400`), distortion (`0x2000`) and forced post-render.

**The capture.** Before a posted distortion entity's first surface
(`0x48af26`), the backend binds the screen image (`0xfe3254`). It takes
the entity's radius, truncated to an integer (`0x48af4e`), and projects
the entity's origin to the screen (`0x48a930`). It then copies a square
of radius × radius pixels with `glCopyTexImage2D(GL_TEXTURE_2D, 0,
GL_RGBA16, x, y, r, r, 0)` (`0x48afe8`). The square's corner is
`vidWidth - x - r/2`, `vidHeight - y - r/2`: it is centred on the origin's
mirror image across the screen's vertical axis, then clamped inside the
screen. Each entity is copied once (`[ebp+8]` holds the last one).

**The draw.** For a distortion entity, the stage iterator binds that copy in
place of each stage's image and draws two-sided (`0x4af083`, `0x4af0cf`).
Each stage keeps its own texture coordinates and colours. With forced alpha
(`0x400`), a single-texture stage takes the entity's alpha at every vertex
and is drawn with source-alpha blending (`0x4af172`, state `0x65`).

**The client game's copy** (`cgamex86.dll` `0x3004d950`, flags written at
`0x3004db31`). For the invulnerability shell, it sets colour
`(0, 255, 0, 100)` and replaces the shader with `effects/refract_2`
(`0x3004dc4a`, string `0x300908ac`). It also sets radius 256 (`0x3004dc13`),
half the shell's scale (`0x3004db26`), and moves the origin 40 units toward
the camera (`0x3004dc04`..`0x3004dc46`). Ysalamiri's shell is yellow at
alpha 100. Two other shells are drawn at alpha 20. The `assets1.pk3` shader
is:

```
effects/refract_2
{
	qer_editorimage	effects/refraction
	surfaceparm	nonsolid
	surfaceparm	nonopaque
	surfaceparm	trans
	cull	twosided
    {
        map $whiteimage
        blendFunc GL_SRC_ALPHA GL_SRC_COLOR
        rgbGen entity
        alphaGen entity
        tcMod stretch triangle 2 1 0 0.4
    }
}
```

**Inference.** Retail's copy shows the background through the shell's
texture coordinates. `tcMod stretch` magnifies it one to three times on a
2.5-second cycle. The copy is multiplied by the entity colour and blended
at 100/255, so the shield filters what is behind it green. It does not
only bend it, as the earlier record said. A sample moves by (1 − 1/s) of
its distance from the square's centre. That is up to 85 pixels at the
edge, whatever the resolution, and about 44 pixels on average over the
cycle (1 − ln 3 / 2 = 0.45 of the square's mean 98-pixel distance from its
centre). At the shield's opacity, that is about 17 pixels. This average is
an estimate: the shell's texture coordinates were not measured.

**Correction.** The earlier record said retail builds `internal_distortion`
with an ARB vertex program at `0x4b6e63`. That site only names a marker
shader with no stages (`0x554410`, sort 14.0, kept at `0xfe3274`), which the
stage iterator tests for a stencil effect (`0x4af108`). The client game's
copy never uses it. The only ARB programs in `jamp.exe` are the glow blur
pair (`0x547530`, `0x547948`).

## Change

**Adapter.** `code/compat/jka_render_adapter.hpp` now accepts `0x2000` as
the engine-owned `refEntity_t.jka.distortion`. The copy also takes
`RF_NOSHADOW`, since retail's copy draws only the screen behind it. Every
other unserved flag, type or invalid field still refuses by name. The
client game's entity import (`client_jka_cgame_host.hpp`) no longer consumes
the copy undrawn. A Ghoul2 copy goes through the owned Ghoul2 queue like any
other Ghoul2 entity.

**Shared rules** (`code/renderercommon/tr_entity_refraction.h`):

- **Allocation.** Only `jka-mp`, with `r_entityRefraction` above 0,
  allocates the scene snapshot that the pass samples. Where `r_liquid` is
  0, other profiles allocate nothing new.
- **Presence.** This is the entity's forced alpha (100/255 for the shield),
  or full without one.
- **Tint.** When the first stage takes the entity colour (`rgbGen entity`,
  or the `0x800` tint), the bent scene is multiplied by
  `1 + (colour − 1) × presence`. For the shield that is `(0.61, 1, 0.61)`.
  Otherwise the tint is white.
- **Bend.** The scene moves by `r_entityRefraction` × 0.20 × view height ×
  presence, before the wave field's own amplitude and fades.
  `r_entityRefraction` runs from 0 to 4, default 1, and is archived.
- **Order.** `tr_jka_entity.h`'s surface deferral now holds back distortion
  copies as well as forced-alpha entities, so the copy refracts a finished
  scene, as retail's posted list does.

**Renderers.** The backend draw loop of each renderer captures its liquid
scene snapshot, and the scene depth, before each distortion copy's first
batch. It recaptures for every such entity, as retail copies once per
entity. GLx uses `FBO_CopyLiquidScreen` and `FBO_CopyLiquidDepth`. Vulkan
and RTX use `vk_capture_refraction_scene`, which leaves a liquid's own
capture state as it was. The stage iterator then draws the copy's batch
with the liquid refraction underlay alone, in place of its stages. The
underlay is opaque, two-sided, without ripples, and uses the bend and tint
above. Light and shadow passes skip the copy. So does any view without a
snapshot: a portal or mirror view, a view without the world, a stereo view,
`r_entityRefraction 0`, or RTX's base list before the trace. RTX's path
tracer leaves the copy to raster, which draws it in the overlay list after
the trace.

**Programs.** Every tier of the liquid refraction now multiplies the bent
scene by a tint, and liquids pass white: the GLx GLSL program (`u_Reflect.rgb`
in the refraction branch), the ARB program (`program.local[8]`), the
fixed-function combiner (`GL_MODULATE` with the primary colour) and both
Vulkan shaders (`liquid_reflect.rgb`). The SPIR-V arrays in both
`shader_data.c` files were regenerated from the changed `liquid.frag` with
the same glslang 1.4.313. That glslang reproduced the previous arrays byte
for byte before the edit, and `liquid.vert`'s array is unchanged.

**Default bend.** With a first default of 0.06, the wave field moved the
scene behind the shield by a mean of 4.4 pixels (GLx) and 5.9 pixels
(Vulkan) at 768 lines. That is far below retail's estimated 17. With 0.20,
it measured 19 to 27 pixels across the three renderers (below). That
matches retail's estimate within the estimate's uncertainty, and it scales
with the view height.

## Fn presentation (invariant 11)

The pass adapts the module's data into fnTech3's liquid refraction. It
does not reproduce retail's mirrored square and texture-coordinate zoom.

- **Used:** the liquid scene snapshot, its scaled resolution
  (`r_liquidResolution`), the depth-based foreground rejection, the
  wave-field displacement in the entity's model space with its distance,
  grazing and screen-edge fades, and every program tier, from the GLx GLSL
  path to the fixed-function fallback.
- **Left out:** ripples (they belong to liquid surfaces), the Fresnel sheen
  and reflection (a shield copy is not a reflecting surface), and the
  partial blend of a bent copy over the unbent scene. Like the liquid
  underlay, the pass replaces the background and folds the copy's presence
  into how far it bends.
- **Not reproduced:** retail's horizontal mirroring of the copied square.
  It reads as an artefact of `0x48af26`, and the module's data do not ask
  for it.
- **Retail-exact option:** none. Retail's player option
  `cg_renderToTextureFX 0` still removes the copy, and so does
  `r_entityRefraction 0`.

## Unit (Source)

- `tests/jka_entity_refraction_tests.py` (new) compiles the adapter. It
  checks the shield's `0x2400`, `0x2000`, `0x2800` and `0x2c00`, the
  shadowless copy, refusals beside them, and the rules header: allocation
  per profile and cvar, presence, tint for `rgbGen entity`, identity, const
  and the `0x800` tint, and bend scaling and clamping. Source checks pin
  the capture before each copy's first batch in every renderer, the pass
  replacing the copy's stages, every program tier's tint, the allocation
  gates, the cvar's registration, and RTX's ownership. A further check
  recompiles both `liquid.frag` files and compares them with the
  checked-in SPIR-V, after `liquid.vert` shows that the local glslang
  matches the checked-in toolchain.
- These tests now expect the new behaviour: `tests/jka_render_tests.cpp`,
  `tests/jka_entity_override_tests.py` (adapter, the deferral of a
  distortion-only entity, the path tracer leaving it to raster) and
  `tests/jka_render_host_tests.py` (the copy reaches the canonical
  submission with its field set). The liquid fixtures now pass `NULL` as
  the entity: `tests/liquid_rendering_source_tests.py` and
  `tests/rtx_raster_execution_tests.py`.

## Live (subset)

Runtime `.tmp/runtime/jka-refraction` was built from the shared working
tree on top of HEAD `652efbf0`, which also held other sessions' uncommitted
work. The harness is `.tmp/jka-refraction/live/listen.py`, adapted from the
spawn-effect slice's. A windowed `jka-mp` client on a private desktop, using
loopback only and no input, hosted `mp/ffa1` with `cg_renderToTextureFX` at
its retail default. It joined with `cmd team free`, closed the menu and
slowed to `timescale 0.02`. It then took engine `screenshotPNG` captures in
pairs, 12 frames apart: `r_entityRefraction 0` (the copy left out), then 1.

`.tmp/jka-refraction/live/pairdiff.py` compares each pair.
`.tmp/jka-refraction/live/shift.py` matches 32-pixel tiles of the green
channel, which the shield's tint leaves almost untouched, inside the
copy's footprint. The table gives, per run, the range over its pairs of
the mean change of red, green and blue in the changed pixels, and the bend
in pixels at 768 lines (tiles searched to ±40 pixels).

| Run | Pairs | Changed pixels | Red | Green | Blue | Mean bend | Largest bend |
| --- | --- | --- | --- | --- | --- | --- | --- |
| GLx, camera in front | 3 | 82,205 to 107,825 | −13 to −20 | +6 to +11 | −15 to −17 | 21 to 27 | 53 |
| Vulkan, camera in front | 3 | 84,915 to 106,446 | −29 to −42 | −3 to +3 | −29 to −39 | 19 to 23 | 38 |
| RTX, camera in front | 3 | 102,375 to 105,326 | −16 | −4 | −17 | 18 to 20 | 46 |
| GLx, camera behind | 3 | 293,541 to 294,492 | −44 | −4 to −5 | −48 | 28 to 30 | 38 |

All 4 runs exited normally with no refusal and no error. The installation
and the runtime binaries were unchanged. Each run's diff falls in the copy's
footprint and shows its filter: red and blue drop while green holds. Every
renderer bends the scene behind it. In RTX the trace's noise also differs
across the frame, at small amplitude.

During the work, a red diagnostic tint (removed) confirmed that the GLx pass
draws in the copy's footprint. The first pairs, 4 frames apart, caught both
shots after the switch had taken effect. `screenshotPNG` lands a few frames
after it is issued, so the harness waits 12 frames between switch and shot.

## Validation

Two exports held HEAD plus this slice's files alone. The first sat on
`652efbf0`. All 1,541 Windows x86 targets built with strict warnings. Its
Meson suite, without the pytest wrapper, passed 265 tests with 2 skipped
and none failed. The whole pytest suite passed 2,726 tests with 131
skipped and none failed.

The second sat on `fbde92d4`, after two other slices landed in files this
one does not touch. All 1,541 targets built again. The Meson suite passed
265 with 2 skipped and none failed. One UI render test,
`fntech3_jka_ui_render`, timed out at 32.9 s under load; alone in the same
build it passed in 3 s. A focused pytest run passed 317 tests with 1
skipped and none failed: every Academy test plus the liquid, RTX raster,
override, fog, underwater, documentation and packaging tests. The JSON
companion lists both runs.

## Remaining

- Retail's mirrored square and texture-coordinate zoom are not reproduced
  (invariant 11), so a side-by-side picture against retail would not
  match. No retail capture was taken.
- Only the invulnerability shell's copy was captured. The item-respawn trail
  (`0x3004a98a`, white at alpha 100) and the other shells' copies at alpha
  20 have not been seen live.
- Single player is not served: its client game stops on its first frame,
  its distortion flag is its own (`0x400000` in OpenJK's single-player
  headers, not measured in retail), and `R_EntityRefractionProfile` names
  only `jka-mp`.
