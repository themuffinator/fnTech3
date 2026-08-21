# Underwater View

## Scope

FnQL's underwater view is an opt-in, visual-only post-process layer applied to the finished 3D scene while the camera is submerged in a liquid. It resamples the frame through an animated screen-space wave field with a small per-channel dispersion, darkens the periphery, and absorbs the scene toward a medium colour with eye distance. With `r_underwater 0` the renderers behave exactly as before.

The layer changes no BSP data, contents flags, collision, visibility, prediction, snapshots, protocol data, demo data, or VM/native-module interfaces. It is the view-side counterpart to the enhanced liquid *surfaces* documented in [`LIQUID_RENDERING.md`](./LIQUID_RENDERING.md): that feature draws water as seen from outside, this one draws the world as seen from inside water. The two are independent and can be enabled separately.

Retail Quake Live has no equivalent engine-side layer, and none of the cvars below exist in retail, so enabling the feature cannot change how a retail server evaluates a client. It is default-off for the same reason every other optional FnQL visual layer is: a competitive player should get the classic presentation until they ask for something else.

## Submersion Feed

The renderer has no collision model, so it cannot ask what the camera is inside. The engine client samples it instead:

1. At the `CG_R_RENDERSCENE` trap, immediately before forwarding the scene, `CL_UpdateUnderwaterView` samples `CM_PointContents` at `refdef.vieworg` and masks the result with `MASK_WATER`.
2. The sample crosses the renderer export boundary as an `underwaterView_t` record holding only the masked contents and the scene time. It does not enter `entityState_t`, `playerState_t`, the cgame VM trap ABI, or network snapshots.
3. The renderer stores the most recent record and ages it in `R_CopyUnderwaterViewToRefdef`, which writes a resolved medium and ramp strength into `trRefdef_t` for the backend.

`SetUnderwaterView` is part of the engine-to-renderer module ABI. Adding it required a coordinated `REF_API_VERSION` bump to `14` with matching OpenGL-lineage, Vulkan, and RTX modules; it must not be exposed by repurposing a cgame VM trap or a network field.

The split of responsibility is deliberate. The engine owns the collision query because only it has the collision model. The renderer owns the entry and exit ramp because only it knows which submitted views are real world views: cgame routinely submits `RDF_NOWORLDMODEL` scenes for 3D HUD models, and those carry an unrelated camera that must not disturb the medium. Scenes flagged `RDF_HYPERSPACE` are skipped rather than reported dry, so the teleport flash does not briefly cancel the layer.

Because the ramp is driven by scene time alone, a stereo pair, a repeated submission at the same time, and a demo seek cannot advance it twice. A sample older than `UNDERWATER_TRANSITION_RESET_MSEC` is ignored and the view reads as dry, so a record left over from a previous map or a disconnected session cannot resurrect a medium. A backwards or oversized time step snaps to the target instead of playing a ramp from an unrelated moment.

A demo reproduces the same medium because it replays the same camera path through the same map, but the sample itself is never recorded in the demo and cannot influence playback, prediction, movement, weapon traces, damage, or authoritative state. Mods require no new asset or game-code support.

## Effect Model

Every constant lives in `renderercommon/tr_underwater.h`, and `tests/underwater_view_source_tests.py` enforces that each backend's copy keeps them identical. The layer has four parts, composited in this order:

1. **Wave warp.** Three sine octaves are summed into a displacement field evaluated at the fragment's normalized viewport coordinate, with the x coefficients pre-scaled by the viewport aspect so the field is isotropic on any aspect. Each octave direction is unit length and the three amplitudes sum to exactly one, which bounds the field to the unit disc and therefore bounds the displacement to the authored pixel count. Amplitude is authored in pixels at a 1080-line reference view and scaled by the real viewport height, so the warp keeps the same angular size at every resolution. Octave speeds are all multiples of `0.05` rad/s, so scene time is wrapped to their common period before upload and trigonometric arguments stay small on low-precision fragment hardware without a visible phase jump.

2. **Chromatic dispersion.** Water refracts short wavelengths more than long ones. The red and blue channels sample at a fraction of the shared displacement either side of green, bounded by `UNDERWATER_DISPERSION_MAX`. The bound matters: past a few percent of the warp the split stops reading as dispersion and starts reading as a broken colour channel.

3. **Edge falloff.** Light reaching the eye drops toward the edge of the field of view. The radius is measured in unscaled normalized device coordinates, so the darkening follows the screen ellipse rather than a circle inscribed in it — a circular falloff on a wide viewport visibly darkens the sides only.

4. **Distance absorption.** Beer-Lambert absorption against the opaque scene depth, sampled at the same refracted coordinate so the medium follows the warped view rather than sitting behind it. This is the single largest contributor to a convincing submerged view: a flat colour wash reads as a coloured filter over a dry scene, while distance-weighted absorption puts the geometry inside a medium. The result is bounded by `UNDERWATER_FOG_MAX_OPACITY` below full opacity, because a saturated screen would hide gameplay-relevant contrast.

The medium colour, density, and warp multiplier come from the eye's contents. Water is a clear blue-green that stays readable at short range; slime is murkier and loses the scene within a room; lava is effectively opaque and its warp is the strongest of the three because it stands in for heat shimmer rather than refraction. When a point carries more than one liquid bit the densest medium wins, so a water/lava overlap cannot look like clear water.

The wave field is anchored to the screen rather than to the world. This layer models light bending as it passes through the water immediately in front of the eye, so the pattern belongs to the viewer, not to the scene; that is also how Quake-lineage engines and their contemporaries have always presented it. An angle-anchored field would slide the crests across the view whenever the camera turned, which reads as a smeared overlay instead of a wet lens.

Like every other authored colour that replaces scene content, the medium colour is converted out of the display-referred domain before upload: it is divided by the overbright factor the output transform will re-apply, and linearized as well in scene-linear mode. Without that step an authored mid-tone reaches the display at twice its intended brightness and the medium reads as a uniform wash.

## Pass Placement And Backends

The layer composites after the world, deferred lighting, the world outline, native BSP fog, and the optional global-fog sidecar, and before motion blur, bloom, gamma, and any later HUD or console scene. The medium is a property of the eye, so it belongs to the scene the eye is looking at and ahead of anything that models the camera or the display.

It composites once per frame, over the primary view only. Recursive portal and mirror views are submitted before the primary view and carry their own camera, so they are skipped. Reduced or sub-rect 3D viewports are skipped because the projective coordinates cover the whole target and would otherwise warp and darken screen area the view does not own. Stereo pairs, anaglyph frames, minimized windows, and cubemap screenshot capture are skipped rather than half-applied.

**OpenGL-lineage, including the GLx GL2+ tier.** `FBO_DrawUnderwater` runs a single ARB assembly fragment program through the ordinary ping-pong pair, reading the resolved scene colour from one buffer and writing the composite to the other, then blitting the result back into the primary scene buffer so bloom and motion blur still consume it. It never samples the attachment it is writing. A multisample colour target is resolved first. The program compiles through `ARB_CompileProgramInternal` with failure treated as non-fatal: on a device that cannot fit it, the renderer prints a warning and the layer is disabled for that session while everything else starts normally. The FBO requirement already guarantees ARB program support, so this is the normal legacy tier rather than an upgrade.

**Vulkan and RTX.** A dedicated fragment shader samples the scene colour on descriptor set 0 and the scene depth on set 1 — the existing post-process pipeline layout already declares two sampler sets, so the whole composite is one draw. It renders into a private single-sample scratch attachment while the main colour image is unbound and therefore readable, then a second draw copies the scratch back into the resumed composition pass. The copy reuses the same shader with a zeroed warp, dispersion split, edge falloff, and absorption, which resolves to an exact passthrough rather than needing a second program. The sample bounds widen to the fragment's own coordinate inside the border margin specifically so that zero displacement is preserved exactly and the copy does not duplicate the outermost texels.

RTX samples the scene depth attachment directly rather than through a private copy, so it moves the depth image out of its attachment layout for the duration of the composite and back before the pass resumes, and it resumes the post-bloom composition pass when that is the active one so a successful ray-traced frame is never suppressed.

The two Vulkan backends declare a 128-byte fragment push-constant range for the post-process layout, up from 48. That is the Vulkan guaranteed minimum, so every compliant device accepts it, and the smaller post-process passes ignore the unused tail.

Depth is optional. When the opaque depth copy is unavailable the medium density is uploaded as zero, which cleanly removes the absorption term while leaving the warp and the edge falloff intact, instead of dropping the whole layer. On the OpenGL-lineage path `FBO_DrawUnderwater` requests the copy itself, and `r_underwater` also keeps the framebuffer's depth attachment alive under multisampling. On Vulkan the backend requests `vk_copy_depth_fade` for the submerged view as well as for the global-fog sidecar, so the absorption does not silently disappear whenever a map happens to have no fog sidecar. Enabling the copy for this layer cannot enable soft particles, because `r_depthFade 0` already forces every material's `dfType` to `DFT_NONE`.

Parity across backends is visual rather than pixel-for-pixel. The OpenGL and Vulkan quad conventions place texture-coordinate zero at opposite ends of the vertical axis, so the wave field is vertically mirrored between them. The field is a sum of sines with no up or down semantics and the edge falloff is symmetric, so the difference is not observable in the result. The ARB program also pre-computes `zNear * zFar` on the host and re-derives `zFar` with one instruction, where the GLSL copy computes what it needs inline; the uploaded values and the evaluated curve are the same.

## Controls

- `r_underwater` (`0`, latched): enables the layer for water, slime, and lava. Requires `r_fbo 1` and `vid_restart`, because the ARB program, the Vulkan shader module, and the scratch attachment are established when renderer resources are created.
- `r_underwaterWarp` (`1.0`, range `0.0..2.0`): wave displacement multiplier. `1.0` is about `11` pixels at 1080 lines before the medium's own multiplier, scaled to the view height. `0` flattens the warp; the three colour taps then coincide.
- `r_underwaterDispersion` (`0.35`, range `0.0..1.0`): chromatic dispersion as a fraction of the warp displacement. `0` keeps all three channels aligned.
- `r_underwaterFog` (`1.0`, range `0.0..1.0`): distance-absorption strength. The colour and density follow the liquid type; `0` disables the tint.
- `r_underwaterVignette` (`0.35`, range `0.0..1.0`): edge darkening. `0` leaves the periphery at full brightness.

Only `r_underwater` is latched. The other four are live tuning values, and all four are scaled by the entry ramp, so setting all of them to zero leaves the layer inert without a restart.

Entry and exit ramp over `UNDERWATER_TRANSITION_MSEC` is deliberately not a cvar. An instant full-strength warp reads as a rendering glitch rather than as a surface crossing, and the window is short enough that no reasonable value would be worth exposing. Swapping media while already submerged adopts the new colour and density immediately: both endpoints are liquids, so there is nothing to ramp between, and holding the previous medium would tint the wrong colour for as long as the ramp lasted.

## Cost And Limits

The layer costs one full-screen pass containing four texture samples — three colour taps for the dispersion split and one depth tap — plus, on the Vulkan backends, one passthrough copy of the same resolution. It runs only while the camera is actually submerged and the ramp is above zero, so a map with no reachable liquid pays nothing beyond the resource allocation at renderer init: one scratch colour attachment at viewport size on the Vulkan backends, and the shared depth copy the global-fog and liquid layers already use.

The warp is a screen-space resample of the frame that has already been rendered. It cannot recover content the frame does not contain, so the border is stretched rather than filled, and it bends the image without changing what was visible. It is not a refracted re-render of the world, and there is no caustic projection, no light shafts, no bubble particles, and no distortion of the HUD or console, which are drawn after the layer and stay sharp.

Absorption is a per-fragment function of the opaque depth at one refracted coordinate. It does not integrate along the view ray, so it cannot model a partially submerged column of water, a surface seen from below at a grazing angle, or the boundary itself: crossing the waterline switches medium at the eye rather than splitting the view. Transparent surfaces sorted after the depth copy are absent from the depth used for the tint, so the medium is weighted by the opaque geometry behind them.

These are the expected constraints of a bounded post-process, and they are preferable to reinterpreting authored shader stages or adding a second world traversal. The authored scene remains the base appearance and the layer either augments it or cleanly drops out.

## Future Directions

In ascending cost and fidelity: a waterline split that renders the above-surface and below-surface halves of a partially submerged view separately; a ray-marched absorption integral against the depth buffer so a water column between the eye and a surface is weighted correctly; projected caustics from a dominant liquid plane; and renderer-local bubble or particulate drift fed from the same visual-only impulse records the liquid surfaces already use.

Any higher tier should stay default-off until OpenGL-lineage, Vulkan, and RTX visual parity, failure fallback, retail map coverage, demo seeking, and performance budgets have been validated, and none of it may feed collision or game logic.
