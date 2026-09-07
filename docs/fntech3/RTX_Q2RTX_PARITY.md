# RTX renderer: Q2RTX comparison and remaining work

Updated 2026-09-06. **fnTech3 has not reached Q2RTX renderer parity.** This is
a feature and implementation inventory for Q3/QL, not a full-compatibility or
visual-quality sign-off. The [compatibility ledger](COMPATIBILITY.md) owns
runtime gate status. A successful RTX raster test does not exercise ray tracing.

## Reference and scope

The comparison uses NVIDIA's official Q2RTX tree at
[`f2526e9a165949f66e91e82f0d63aa7bb2567b4d`](https://github.com/NVIDIA/Q2RTX/tree/f2526e9a165949f66e91e82f0d63aa7bb2567b4d),
resolved from `refs/heads/master` on 2026-09-06. Its
[README](https://github.com/NVIDIA/Q2RTX/blob/f2526e9a165949f66e91e82f0d63aa7bb2567b4d/readme.md)
describes the renderer feature set and marks the project discontinued. Its
[changelog through 1.8.1](https://github.com/NVIDIA/Q2RTX/blob/f2526e9a165949f66e91e82f0d63aa7bb2567b4d/changelog.md)
provides additional implementation history. These are reference observations;
this work does not incorporate Q2RTX implementation text or distribute its media.

The relevant target is lighting, materials, geometry, image reconstruction, and
reliable operation for Q3 assets. Q2-specific weapons, monsters, mission packs,
and gameplay are not features for this engine-only distribution. Legitimate
Q3/QL modules and assets remain user supplied. QL's x86 client-module boundary
also remains a separate requirement from Q2RTX's client architecture.

A separate Windows x86_64 diagnostic configuration, `-Dq3-only=true`, builds
`fntech3-q3.exe` with `fnql_rtx_x86_64.dll` and rejects QL selection before
filesystem startup. It permits Q3 RT investigation on drivers whose required
features differ between Win32 and x86_64. It is not the shared Q3/QL release
contract, and Q3-only evidence must retain that executable distinction.

In the matrix, **implemented** means a production path exists for the stated
scope, **partial** means important restrictions remain, and **open** means the
comparable capability is absent. None of those labels alone establishes Live
evidence or equivalent performance and image quality.

## Feature matrix

| Capability | fnTech3 status | Production scope and remaining work |
| --- | --- | --- |
| Hardware scene tracing | Implemented, bounded | Vulkan KHR ray-tracing pipeline, scene BLAS/TLAS, material and texture descriptors, explicit capability/failure handling. Mode 2 is the native path; the mode-1 label does not provide a second working ray-query renderer. See [Vulkan backend](../../code/rendererrtx/vk.c). |
| Q3 base texture and baked-light materials | Implemented, bounded | Native world admission supports one static opaque base texture, optionally multiplied by one collapsed lightmap bundle or baked vertex lighting. Vertex lighting includes both the renderer's marked collapse and implicit `LIGHTMAP_BY_VERTEX` shaders created by `R_CreateDefaultShading`. Lightmap coordinates, image identity and intensity survive extraction; baked vertex RGB remains separate from base albedo. Arbitrary authored vertex-color stages, animation, deformation, unusual stage combinations, remaps and fogged surfaces retain raster ownership. See [shader admission](../../code/rendererrtx/tr_shader.c), [extraction](../../code/rendererrtx/vk.c), and [closest-hit shading](../../code/rendererrtx/shaders/rt_main.rchit). |
| Secondary diffuse transport | Partial | Raygen iterates up to four secondary scene-radiance bounces. One sampled diffuse/specular/transmission lobe continues each path. Baked lightmaps remain an additional lighting contribution; overlap with direct and indirect light prevents an unbiased physical-lighting claim. No emitter importance sampling is present. See [raygen](../../code/rendererrtx/shaders/rt_main.rgen). |
| Scene reflections | Partial | Native shading now traces scene radiance using a sampled GGX reflection lobe; the analytic environment supplies escaped rays. Eligible static world materials and optional simple posed model skins can be reflected. Raster-only material stages and effects are absent from that reflected scene. |
| Refraction and caustics | Partial / open | Continuation code handles Fresnel, Snell refraction and total internal reflection for admitted transmissive materials. Primary native world admission still excludes blended liquids/glass. Existing raster liquid effects remain authoritative. Complete Q3 water/glass transport, thickness, absorption, colored shadow transmission and caustics are open. Q2RTX's broader reference scope is documented in its [feature list](https://github.com/NVIDIA/Q2RTX/blob/f2526e9a165949f66e91e82f0d63aa7bb2567b4d/readme.md#features). |
| Point, segment and directional lights | Implemented, bounded | Current scene lights, authored sun and available light sidecars are uploaded with finite/range validation and a bounded light budget. Shadow rays offer hard/soft modes. BSP compile-time entity reconstruction remains approximate and disabled by default; it cannot recover arbitrary baked lighting. |
| Emissive geometry and area-light sampling | Partial / open | Translated material emission contributes when a path hits it. Scalar emissive guesses and surface-light proxies are not polygonal emitter extraction or multiple-importance sampling. Animated/model emitter extraction, area-weighted sampling and light-visibility distributions remain open; Q2RTX records these capabilities in its [changelog](https://github.com/NVIDIA/Q2RTX/blob/f2526e9a165949f66e91e82f0d63aa7bb2567b4d/changelog.md). |
| PBR material authoring | Partial | Scalar roughness, metallic, emissive parameters and diagnostic overrides exist. Default name/flag heuristics remain off. There is no equivalent authored normal/roughness/metallic/emissive texture-map pipeline or editable replacement-material library. The [Q2RTX material system](https://github.com/NVIDIA/Q2RTX/blob/f2526e9a165949f66e91e82f0d63aa7bb2567b4d/readme.md#material-system) is a reference for that missing workflow. |
| Posed models and player avatar | Partial, experimental | MD3 uses explicit LOD0 and frame interpolation; MDR now contributes weighted posed triangles instead of an enclosing box; IQM retains CPU pose/skinning support. Optional dynamic BLAS can expose supported opaque/masked skins to shadow and secondary rays. All primary model color remains raster. Hidden third-person bodies can cast; depth-hacked/first-person weapons and no-shadow models are excluded. Dynamic BLAS is off by default. |
| Particles, beams, decals and animated shaders | Partial | Raster overlay preserves authored primary appearance and draw order. Optional dynamic effects use limited proxy geometry; they do not provide complete radiance, volumetric or reflected-effect parity. |
| Environment and sun | Partial | Authored directional sun and an analytic environment are present. Full sky-texture lighting, importance sampling, procedural atmosphere/clouds and time-of-day authoring remain open. Q2RTX's procedural environment and sunlight features are listed in its [README](https://github.com/NVIDIA/Q2RTX/blob/f2526e9a165949f66e91e82f0d63aa7bb2567b4d/readme.md#features). |
| Participating media | Partial / open | Raster fog and liquid effects remain available; particle-volume shading is an approximation. Scene-ray integration of volumetric light scattering and shadowed fog is open. |
| Temporal image reconstruction | Partial | Camera reprojection, history clamping, hit/miss rejection, previous-camera depth comparison and camera-cut resets exist. Dynamic hits and reflections involving them reject history until previous object/triangle motion is available. Stable moving-object and specular reprojection remain open. |
| Denoising | Partial | Temporal blending, firefly suppression and an optional four-neighbour bilateral filter exist; the spatial filter launches additional radiance paths. Separate diffuse/specular signals, adaptive variance/gradient filtering and robust reflected/refracted motion are missing. Q2RTX documents separate HF/LF/specular channels and specialized filters in [A-SVGF notes](https://github.com/NVIDIA/Q2RTX/blob/f2526e9a165949f66e91e82f0d63aa7bb2567b4d/src/refresh/vkpt/shader/asvgf.glsl). |
| Exposure, tone mapping and HDR | Partial | Shared final-pass composition and RT-local SDR compatibility controls exist. They must respect the active scene-linear target. HDR display/capture parity, exposure stability across combat and a calibrated reference workflow remain open. |
| Resolution scaling and performance | Partial | Timing, adaptive budgets and experimental dynamic resolution exist. Equivalent temporal upscaling/FSR reconstruction and a demonstrated frame-time target remain open. Q2RTX's upscaling changes are recorded in its [changelog](https://github.com/NVIDIA/Q2RTX/blob/f2526e9a165949f66e91e82f0d63aa7bb2567b4d/changelog.md). |
| Photo mode and alternative projections | Open for RT | Engine screenshots are available, but high-sample reference accumulation, physical depth of field, and a validated family of alternate RT projections are absent. Q2RTX documents its [photo mode](https://github.com/NVIDIA/Q2RTX/blob/f2526e9a165949f66e91e82f0d63aa7bb2567b4d/readme.md#photo-mode) and later projection additions in its changelog. |
| Multi-GPU rendering | Open | No comparable distributed path-tracing implementation. This is a lower-priority scalability feature after single-device correctness and quality. |
| Portal, stereo, subview and HUD integration | Partial | Native dispatch is limited to the primary full-window, non-portal, centered-stereo world view. Unsupported views and authored overlays retain raster behavior. Fully traced portal/mirror/stereo views and their temporal histories remain open. |

## Operator controls

Current registrations and ranges live in
[tr_init.c](../../code/rendererrtx/tr_init.c). These controls are renderer
options, not game-profile ABI switches. Profile selection is immutable and
must be supplied separately at process startup.

| Control | Default | Use and limits |
| --- | --- | --- |
| `cl_renderer` | Project default | Set `rtx` to load this renderer. |
| `rtx_rt_mode` | `0` | `0` exercises RTX raster; `2` requests the native pipeline. Latched; use startup settings or `vid_restart`. |
| `rtx_rt_require` | `0` | Set `1` in native validation to reject capability/primary dispatch failures instead of silently using raster. Latched. |
| `rtx_rt_raster_reference` | `0` | `0` uses native shading for admitted surfaces; `1` uses authored raster color with bounded RT cues. The latter does not exercise the same transport path. |
| `r_vertexLight` | `0` | Map-ingestion policy, latched. Set `0` before startup for texture-lightmap comparisons. Native shading also supports renderer-collapsed vertex lighting and implicit `LIGHTMAP_BY_VERTEX` baked colors used by inherited settings such as `2`; record the effective value because the baked lighting source changes. Authored vertex-color stages remain constrained by native admission. |
| `rtx_rt_indirect_bounce` | `0` | Secondary-bounce limit `0..4`. Zero disables diffuse continuation; enabled reflection/refraction can still request one continuation. |
| `rtx_rt_reflection_strength` | `1.0` | Controls the native reflection contribution. Turn off alongside refraction for a direct-only diagnostic. |
| `rtx_rt_refractive_mode` / `rtx_rt_refractive_ior` | `1` / `1.33` | Experimental transmission policy and IOR; material-admission restrictions still apply. |
| `rtx_rt_dynamic_blas` | `0` | Opt into posed model/effect visibility. Keep separate evidence for enabled and disabled modes. |
| `rtx_rt_iqm_skinning` | `1` | `0` bind pose; `1` interpolated CPU skinning; `2` current skinned frame. |
| `rtx_rt_material_heuristics` | `0` | Experimental legacy-name/flag guesses. They are not authored PBR metadata. |
| `rtx_rt_world_light_scale` | `0` | Opt into approximate compile-time BSP light reconstruction; check for duplicate baked lighting. |
| `rtx_rt_temporal_enable` / `rtx_rt_spatial_denoise` | `1` / `0` | Temporal accumulation and the costly experimental neighbour filter. Test camera movement, disocclusion and animated content separately. |
| `rtx_rt_quality_preset` / `rtx_rt_adaptive_budget` | `3` / `1` | Presets and adaptation can change effective light/quality budgets. Use custom preset `0` and disable adaptation for controlled comparisons. |
| `rtx_rt_dynamic_resolution` / `rtx_rt_async_overlap` | `0` / `0` | Experimental; enable only in a separate performance/validation run. |
| `rtx_rt_debug_visualizer` | `0` | `1..8` inspect geometry/materials/lights; `9` motion; `10` history blend. These are diagnostics, not shaded-output evidence. |
| `rtx_rt_perf_timing` / `rtx_rt_debug_as_stats` | `0` / `0` | Record timing and acceleration-structure diagnostics with exact binaries, driver and settings. |

The [runtime smoke script](../../scripts/rtx_runtime_smoke.py) accepts
`--game-profile q3|ql` (default `ql`) independently of
`--profile raster|native|all`. It requires an explicit installed map name,
verifies Q3's base pak or QL's retail markers, creates isolated writable homes,
forces windowed mode, and disables mouse capture, joystick, Steam integration,
Workshop and WebUI. It sets `r_vertexLight 0` before startup to keep the
lightmap comparison explicit. `--plan` emits a reviewable command/manifest without starting
the engine. Example plan:

```text
python scripts/rtx_runtime_smoke.py --exe <fntech3.exe> --retail-root <legitimate-Q3-root> --game-profile q3 --map <installed-map> --profile all --plan
```

Q3 uses `devmap <map>`; QL retains `devmap <map> <gametype>`. Captures must use
the engine's `screenshotPNG` command. No operating-system capture or injected
user input belongs in renderer validation. A structurally valid, non-flat PNG
and an active-RT log are necessary smoke evidence, not visual-parity proof.
With `rtx_rt_debug_as_stats 1`, the world-extraction log reports the native
surface, triangle and material counts. Level `2` additionally records up to 128
distinct shader/fog combinations and their stage/bundle state. A trace/copy log
with no native world coverage can still produce an entirely raster-owned view.

## Evidence and next gates

The [2026-09-06 RTX runtime report](2026-09-06-rtx-round.md) records the
shared Win32 raster checks and separately identified Q3-only x86_64 native
probes, including driver capabilities, native world coverage and remaining
limits. The [diagnostic build instructions](../../BUILD.md#windows-q3-only-rtx-diagnostics-x86_64)
describe the explicit `-Dq3-only=true` target; its evidence does not establish
the shared Q3/QL release contract.

Executable regressions cover
[draw-list visibility, uniform allocation and logical Vulkan buffer bounds](../../tests/rtx_raster_execution_tests.py),
[lightmap admission](../../tests/rtx_lightmap_execution_tests.py),
[MDR geometry](../../tests/rtx_mdr_geometry_tests.c), and
[transport](../../tests/rtx_transport_tests.py). The
[smoke-tool tests](../../tests/rtx_runtime_smoke_tests.py) exercise both profile
plans without launching a game. These establish Unit/Source evidence only.

The earlier frozen Q3 RTX raster black-polygon failure was traced to personal
shadow-caster-only surfaces entering ordinary color batches. The draw-list fix
rejects these surfaces before the equal-sort fast path while preserving the
list for shadow rendering. A new matched-frame runtime comparison is recorded
separately in the [compatibility ledger](COMPATIBILITY.md); it does not close
the native transport, animation, material or performance gates above.

Next useful gates are matched Q3/QL demos with strict native RT, controlled
direct/reflection/indirect settings, animated posed models and masked skins,
multistage/fog/portal fallback scenes, restart/resize/failure stress, Vulkan
validation, and measured image-quality/frame-time budgets. Emitter sampling,
authored PBR texture inputs and motion-aware denoising are the largest missing
pieces for a convincing Q2RTX-class result after those correctness gates.
