# RTX liquid interfaces — 11 October 2026

Evidence: **Source, Unit, Live subset**. Profile observed: `q3`, `q3dm12`.
No compatibility gate is closed. The adjacent JSON pins the builds, commands,
installation inputs and retained engine captures.

## Behaviour and ownership

Native RT owns supported contents-marked water, slime and lava by default
(`rtx_rt_liquids 1`). It replaces both authored screen-space/environment
presentation and FnQL's screen-copy liquid passes at those pixels. Raster
mode, failed RT, excluded views and unsupported materials keep the raster
path. `rtx_rt_liquids 0` plus `vid_restart` is the explicit comparison option.
No profile, module, BSP, collision, network or demo contract changes.

Clear water with a blended first stage transmits; an opaque first stage
stays opaque, including q3dm12's slime. Cutouts, screen-reading stages,
polygon offsets, unsupported deforms and wave displacement above four map
units remain raster. This is a contents/stage rule, not a texture-name list.

The shared hardware/compute path uses:

- Exact dielectric Fresnel and Snell refraction, including total internal
  reflection. Both primary lobes are traced; later visible interfaces sample
  their Fresnel probabilities, with a bounded interface count independent
  of diffuse depth. A zero diffuse-bounce setting still traces water.
- Beer attenuation along transmitting segments, with an animated authored
  tint. Water IOR 1.333, slime 1.38, lava 1.45 and absorption coefficients are
  engine presentation choices, not measured retail constants.
- The existing full-world geometry and supported dynamic models, including
  off-screen geometry. Authored opaque liquid stages retain colour and glow.
- FnQL's three-octave wave field, tangent frame and eight bounded interaction
  rings. `r_liquidWarpScale` and `r_liquidRipples` retain their shared controls.
  The client supplies interactions to RT even when screen-space `r_liquid` is
  zero. Games without that interaction feed retain ambient waves only.
- Texture differentials through visible interfaces, bounded temporal history
  for liquid pixels and limited wide specular filtering. Transmitted receiver
  colour is separated from its lighting before denoising, preserving the
  pool's texture instead of smoothing it with the light. The screen-copy
  resolution, reflection-strength and refraction-strength settings are not
  optical parameters and remain confined to the raster effect.

## Observations

The requested camera was `setviewpos 17 -2206 -157 52`, with mouse and
joystick disabled on an unswitched private desktop. Every launch forced
windowed mode. All images came from `screenshotPNG`; no OS capture or input
injection was used. Writable homes, the locally recorded camera demo and
logs are under `.tmp/rtx-liquids/`. The installed game was read only.

`verified-hardware` and `compare-fnql` replay the same locally recorded camera demo,
advance ten frames and freeze playback. The latter explicitly starts with
`rtx_rt_liquids 0` and `r_liquid 2`, so it actually enables the latched FnQL
effect. Both use the same native room lighting, then the same baked-light
comparison. The RT view bends the pool geometry and shows the reflected lamp.
Source inspection establishes full-world reflection rays independent of
screen coordinates; this fixed-camera comparison does not test a moving
source leaving the screen. Disabling specular
removes the reflected lamp while preserving transmission. Calming the waves
straightens the optical distortion. The source rule and runtime material
pick identify `clear_calm1` as a traced transmitting interface.

`optical-controls` leaves `r_liquid 2` enabled while RT owns the water: the
screen-copy effect does not overlay it. Its native-lighting comparison turns
specular off, then restores specular with zero diffuse bounces; the reflected
lamp disappears in the former and remains in the latter.

Win32 compute and the Q3-only x64 hardware diagnostic both execute the same
liquid shaders. The hardware run enables Vulkan validation and reports no
validation messages. Win32 reports that its requested validation layer is
unavailable, so its successful rendering is not validation-layer evidence.
The final compute probe includes native, baked and unfiltered
captures. Earlier live captures also view the underside from
`setviewpos 192 -2100 -265 90`; its grazing surface reflects the underwater
room. No equivalent claim is made for lava, another game's liquids or every
possible water volume. q3dm12 slime ownership is covered by its stage rules;
the fog-obscured side view is not proof of its reflection appearance.

The installation includes its existing `zz-q3netpack.pk3`; the fixture hashes
record it. This is a comparison of fnTech3 presentations with user-supplied
assets, not a claim of untouched retail-image parity.

## Checks and limits

The actual shader Fresnel/attenuation functions execute in a native fixture:
normal incidence, reciprocal boundaries, grazing angles, critical angle,
total internal reflection, equal media, finite bounded energy, zero/negative
distance, channel ordering and exponential composition. The material fixture
executes the real classifier with water/slime/lava, disabled tracing,
cutouts, oversized/NaN waves and unsupported stages. RTX, liquid regression,
generated-document and player-manual checks are recorded in the JSON.

Small authored displacement is replaced by optical normals on the BSP plane;
the liquid silhouette is not a dynamically rebuilt displaced mesh. Nested
different media have no volume stack. Diffuse rays cross clear water with
absorption but without specular bending: caustics are not traced. Shadow rays
retain the map compiler's filtering rules. Raster-only effects and models do
not appear in traced reflections; the existing fog and optional submerged-view
compositor remain separate. General native-RT grain remains visible in the
room in both sides of the comparison, outside the liquid work.

Q3/QL shared source and fallback tests do not establish RTCW, ET, Raven,
Ritual, Elite Force or MOHAA runtime liquid compatibility. These paths keep
fnTech3's lighting, audio, input and diagnostics; no retail engine is copied
or enabled in their place. This implementation is independently authored
within fnTech3 and reuses its inherited FnQL wave/interaction helpers; no
external implementation text was incorporated.
