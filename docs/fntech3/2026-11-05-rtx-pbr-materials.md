# RTX physically based materials, 2026-11-05

RTX's path tracer already took a texture pack's normal, specular-gloss and
`_rmo`/`_orm` maps. This round completes its material model. It adds rend2's
material words in shader scripts, all six of rend2's packings with their
specular channel and scales, occlusion, emission maps that also light the
scene, and parallax from height maps. It is built on
`affb351168f1bf51d016f23af473216b71818cad`. The
[JSON companion](2026-11-05-rtx-pbr-materials.json) holds the identities,
commands and figures.

Nothing here touches a module ABI, wire, demo, save or pak contract, and no
compatibility gate closes. Retail data carries no material maps, so retail
frames are unchanged (measured below). The path tracer keeps fnTech3's own
traced light, denoiser and camera (invariant 11). The maps are a texture
pack's data, adapted into that pipeline.

## What a texture pack can give a material

A lit material (one with a lightmap, vertex light or `lightingDiffuse`
stage) takes:

- **Material words** in one stage of its shader script, as OpenJK's rend2
  reads them (reference observation of `codemp/rd-rend2/tr_shader.cpp`
  `ParseStage` at OpenJK `1a6a643`): `normalMap`, `normalHeightMap`,
  `specMap`/`specularMap`, `rmoMap`, `rmosMap`, `moxrMap`, `mosrMap`,
  `ormMap`, `ormsMap`, and the scalars `specularReflectance`,
  `specularExponent`, `gloss`, `roughness`, `parallaxDepth`, `parallaxBias`,
  `normalScale` and `specularScale`. One parser,
  `code/renderercommon/tr_material_words.h`, serves all three renderers. The
  GL and Vulkan renderers read past the words and draw the stage as the rest
  of it says. Before this round, any of these words failed the whole shader in
  every renderer, so a rend2-authored pack drew its walls as the default
  image. Only the path tracer gives the words meaning.
- **Companion images** where the words name none, beside the texture of the
  stage that read words, else of the material's first stage that is not a
  lightmap or environment map: `_nh` (height in alpha), else `_n`; then
  `_specGloss`, `_rmo`, `_orm` (rend2's names, in its order), then `_rmos`,
  `_orms`, `_moxr` and `_mosr` (rend2's packings under its naming pattern);
  and `_emissive`, else `_luma` (DarkPlaces's glow name) for emission.
- **Scalars alone.** A stage that gives a gloss, roughness, reflectance or
  scale with no map is a material of those constants: a white specular map
  scaled as rend2 scales one.

### Decoding

- Packings follow rend2's swizzles into occlusion, roughness, metalness and
  specular (`tr_image.cpp` `R_LoadPackedMaterialImage`). A packing without a
  specular channel takes half its range. A dielectric's normal-incidence
  reflectance is 0.08 times the specular channel, so 0.04 by default, as in
  rend2's `lightall.glsl`. A metal reflects in its texture's colour and
  loses its diffuse light.
- Scales apply as rend2 applies them: `ORMS *= specularScale.zwxy` for a
  map of constants (`$whiteimage`), and colour and gloss scales for a
  specular map. rend2's loader resets the scales of a real packed image, so
  that image's channels are taken as they are.
- A normal map's x and y are multiplied by `normalScale`. A negative y reads
  a map whose green points the other way.
- **Occlusion** is the texture's own relief, finer than the geometry the
  rays meet. It scales the light the rays bring back: the diffuse bounce at
  the primary hit and at every bounce, and the compilers' ambient. The
  glossy reflection takes Lagarde and de Rousiers' specular occlusion
  (SIGGRAPH 2014). The light samples are left alone: they find the
  geometry's shadows with their own rays.
- **Emission maps** add their texels to what the stages give off. The
  map's average makes the material a light of the light sampler, as a glow
  stage does (`RT_ShaderEmission`), so a glowing pack lights the room. A
  `q3map_surfacelight` shader keeps its authored light.
- **Parallax.** Where the normal map holds height, the primary view's ray
  continues under the surface. It meets the height field `parallaxDepth`
  texture units deep (0.05 by default) by rend2's search: sixteen steps,
  eight halvings, then a linear fit (`lightall.glsl`
  `RayIntersectDisplaceMap`), with rend2's non-square scale. The point's
  texture and lightmap coordinates move there. Light and bounce rays leave
  from the flat surface, and reflections and bounces skip the search.
  The relief fades out between 63 and 81 degrees from the normal (see
  "q3dm1 converted" below).
  `rtx_rt_parallax 0` turns it off without a restart.

### Retail-safe names

`.tmp/rtx-pbr/suffix_census.py` counted every image of every
installed game whose name plus a suffix is another image. It covered the
paks and loose files of Steam Quake III, Quake Live, RTCW, Enemy Territory,
Elite Force, both Jedi games, Alice, the laid-down F.A.K.K.2, Medal of Honor,
Q3Test and the IHV test.

- **Observed.** `_glow`, `_light` and `_e` pair with retail glow and trim
  textures: 14 `_glow` pairs in Quake III, 199 in Jedi Academy and 21 in
  Alice. ioquake3 rend2's `_s` pairs with retail textures in RTCW (2),
  Enemy Territory (2), Jedi Academy (10), Jedi Outcast (13) and Medal of
  Honor (6). `_spec`, `_h`, `_g`, `_l`, `_metal` and `_rough` also collide.
- **Observed.** `_n`, `_nh`, `_specGloss`, `_rmo`, `_orm`, `_rmos`, `_orms`,
  `_moxr`, `_mosr` and `_luma` pair with no image in any retail pak. The
  only `_emissive` pair, `rq3dm5_dyson/bouncepad_conc1`, is in the Steam
  install's community netpack, not a retail pak. A companion that the shader
  already draws as a stage is never taken.

So the finder takes only the names that collide nowhere in retail data.

## Fitting the 32-bit driver's compiler

On Windows x86 the NVIDIA driver offers no ray-tracing pipeline, so RTX
traces with its compute shader. The first build of this round failed to
create that pipeline (`VkResult -13`), and the frame fell back to raster. The
code already warns that the 32-bit driver's compiler runs out of address
space when the trace grows. Every sampled map had inlined the stage's
coordinate and modifier code up to three times.

The fix gives each sampler one coordinate call site: `rt_image_texel` loops
over the point and its two footprint corners, `rt_material_frame` over the
point and two edges, and `rt_shading` samples its three maps in one loop.
After that the compute pipeline builds and traces. The tests pin the single
call sites (`tests/rtx_pbr_material_tests.py`).

## Evidence

Runtime probes used `scripts/rtx_probe.py` on Quake III 1.32 demo `four`
(q3dm6), frozen after 400 frames, 960x540, windowed on a private desktop with
an isolated home. The GPU was an RTX 4060 Laptop with driver 591.74. The
installation was a retail-only tree: the Steam install's `pak0`-`pak8`
copied alone under `.tmp/rtx-pbr/q3retail`. The Steam install itself also
loads a third-party `q3r-maps.pk3` whose q3dm6 replaces retail's, so the
first runs, made on it, are not counted here.

The packs are synthetic (`.tmp/rtx-pbr/make_pack.py`): only the map's
texture names are read from the installation, and every image is
generated. They hold a tiled bevel `_nh` with height, an `_orms` (occlusion
in the grooves, roughness 0.2 or 0.6, metal on a checker, specular 0.5) for
all 79 images q3dm6 draws, and an `_emissive` (cyan grooves) for one
texture, or for all of them in the glow-all variant. A words pack gives
`metalbase03a_blocks18c2`, which the map draws without a script,
`normalHeightMap`, `ormsMap $whiteimage`, `specularScale 1 1 1 0.9` and
`parallaxDepth 0.1`.

- **Unit (observed).** The new tests run each renderer's production
  `ParseShader`/`ParseStage` under all sixteen profiles. The stage reads on
  past the words, a missing value warns, a misspelt word still fails, and the
  path tracer's parser stores the words' values. The record, the decoding,
  the single call sites and the finder's names are checked against their
  sources. The 79 RTX, shader-parser, asset and documentation test files
  pass, after the source pins this round moved were updated.
- **Live, compute backend (x86, observed).** No validation messages in any
  run. 56 materials took maps, and the words stage took `_bump.tga` with
  `*white`. `blocks11b` was built as a light (`glows 0.000 0.211 0.234`, its
  emissive map's mean): 3 glowing shaders over 753 triangles, against 2 over
  50 without the pack. With every texture glowing, 46 shaders over 22,565
  triangles became lights, and their cyan light shows on the player model
  and floor. Parallax changes the frame by 13.5/255 on average (34% of
  pixels by more than 8 levels); the floor's tiles stand in relief. With
  `rtx_rt_material_maps 0`, no material took a map, and the frame matches the
  no-pack frame within 0.06/255.
- **Live, ray-tracing pipeline (x64 `q3-only`, observed).** With
  `rtx_rt_require 1` the backend was `ray_tracing_pipeline`. Its emission
  view is the compute one's to 0.001/255, and its final glow-all frame comes
  within 2.5/255 on average (separate launches and builds). Parallax changes
  it by 31.8/255.
- **No retail change (observed).** HEAD's RTX renderer and this round's ran
  beside one frozen client, with no pack. Their raw one-sample traces
  (`rtx_rt_debug_visualizer 4`) are byte-identical. The denoised frames
  differ by 0.42/255 on average, and frames accumulated over 240 more differ
  by 0.10, which is launch-to-launch accumulation timing.
- **Merged with the animated-surfaces work (observed).** That session's
  uncommitted RTX changes, merged three ways with this round's, built all
  three renderers under `/W4 /WX`, regenerated SPIR-V, and passed both
  rounds' tests.
- **Not measured.** Other profiles' maps with a pack (the finder and
  decoding are profile-independent, and the parser runs under every
  profile), frame time with maps, and a rend2 pack made by someone else.

## q3dm1 converted from its own textures

To check the material model on real content rather than synthetic patterns,
every image q3dm1's lit materials draw was given derived material maps:
72 images, from the user's own retail `pak0`-`pak8`. The maps are derived
from retail art, so they and the pack stay under `.tmp/rtx-pbr-q3dm1` and are
never committed (invariant 8). The converter there, `pbr_convert.py`, reads
each image as RTX's finder picks it (`q3_materials.py`) and writes `_nh` or
`_n` and `_orms`. It follows the single-image material estimation of
CrazyBump and Materialize:

- **Classes by eye.** Each of the 72 images was classified from contact
  sheets into one of 13 classes: masonry, slate, cobble, stone with metal
  inlay, sculpted metal, plate, rust, grating, wood, mud, flesh, cloth and
  paint. Each class sets a relief depth, band weights, roughness range and
  metal rule.
- **Height.** The luminance less its large-scale albedo is split into
  wrap-around band-pass layers. Coarse bands make the structure (shaped
  into plateaus for stone and mud), fine bands the grain. Masonry is built
  from its mortar instead: painted lines found by Hessian line strength at
  two scales, of whichever polarity forms the larger connected network
  (both where they are equal, as in `blocks17`'s pale pointing), with
  isolated specks dropped. Grooves get bevelled shoulders and the block
  faces stay nearly flat.
- **Normals** come from that height at the tracer's own parallax depth, so
  bumps and parallax agree. **Occlusion** is horizon-based on the same
  relief. **Roughness and metalness** take class priors, modulated by
  rust (warm, saturated, noisy), grime and painted highlights.

What the probes taught, each a change made and re-checked:

- **Painted metal must stay partly diffuse.** Quake III paints its bronze
  and iron with the shading baked in, in dark colours. Fully metallic, a
  surface keeps no diffuse light and mirrors only in that dark colour, so
  arches, columns and statues went nearly black. Metalness is capped at
  0.5-0.6 for painted metal, which keeps the painted look with a metal's
  sheen.
- **Parallax suits broad relief only.** Blocks, cobbles, slate, mud and
  flesh take height (`_nh`). Rivets, grating, carving and grain smeared
  into spikes under the search (a riveted 64x256 plate stretched up a
  pillar), so those classes give normals alone (`_n`).
- **Grazing views smear any height field.** The tracer now fades parallax
  out between 63 and 81 degrees from the normal (`rt_path.glsl`
  `rt_parallax`).
- **Low roughness over noisy normals glitters.** The pitted
  `metalbridge06` floor sparkled under torches until made rough and mostly
  non-metallic.

**Live (observed).** `scripts/rtx_probe.py` ran on retail q3dm1 (`pak0`-`pak8`
alone) with `win32-dev` at `b27921b1` plus the fade, compute backend. It
used five fixed views (four spawns and the intermission camera), each traced
without and then with maps after a `vid_restart` and accumulated frozen.
There were no validation messages, and all 72 materials took their maps on
every maps-on load and none on the plain ones. Frame means, plain against
PBR: 39.4/38.3, 43.8/36.5, 37.7/34.6, 40.3/36.0 and 65.8/60.1. Occlusion,
and metal that keeps less diffuse light, darken the PBR frames by 3-17%.
Parallax changes v1 and v3 by 4.5 and 3.8/255. Separate runs differ in how
far a frozen view has accumulated: the noise of the same view varied by up
to 2.5x between runs, so single runs are compared only for gross faults.
`blocks18b`'s pointing is faint and broken in the source art, so its
mortar network stays partial.

## Limits

- Height maps shift only the primary view. Shadow, bounce and reflection
  rays see the flat surface, so there is no parallax self-shadowing.
- Maps belong to one stage of a lit material. A fullbright shader takes
  none, as in rend2.
- There is no override file in the Q2RTX manner. Quake III's shader scripts
  are the material system: a pack overrides a shader to set its maps and
  constants.
