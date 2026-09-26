# GLx Renderer Deep Research and Definitive Replacement Plan

## Bottom line

Task E status: the active GLx policy tier is now the required five-value ladder (`GL12`, `GL2X`, `GL3X`, `GL41`, `GL46`), with feature flags and an internal capability hint kept underneath it.

The current GLx work in urlthemuffinator/FnQLhttps://github.com/themuffinator/FnQL is real, substantial, and trending in the right direction, but it is **not yet** the definitive replacement for the legacy OpenGL renderers. The strongest parts already in place are the modular bridge surface, capability probing, debug/profiling hooks, dynamic stream strategy ladder, static-world experimentation, a material-key system, and verification tooling. The biggest structural problem is that GLx is still explicitly documented and built as an **experimental** renderer that reuses the compatibility OpenGL renderer as its rendering baseline while GLx-owned paths are brought up behind it. That directly violates your “no fallbacks to legacy OpenGL” end state. fileciteturn16file0 fileciteturn14file0 fileciteturn9file0 fileciteturn9file1 fileciteturn9file2

The second major problem is that the current tier model does **not** match your required platform ladder. The checked-in GLx capability logic rejects anything below OpenGL 2.1 and only classifies `Compat`, `Core`, and `Advanced`. Your requested support ladder starts at **GL 1.2**, then requires meaningful GL 2.x and GL 3.x middle tiers, plus a deliberate macOS ceiling at **GL 4.1** and a high-end Windows/Linux ceiling at **GL 4.6**. As shipped today, GLx does not satisfy that requirement. fileciteturn10file0 citeturn3search0turn4search6

The third major problem is image quality and output modernity. In the current docs, `r_hdr` is described as **framebuffer precision selection** with 4-bit, 8-bit, and 16-bit modes. That is useful internal precision, but it is **not** a true display-HDR/color-managed pipeline with calibrated output transforms, HDR metadata paths, or color grading. The current GLx documentation also positions bloom parity as a preserved legacy surface rather than a full modern scene-linear grading/output pipeline. fileciteturn14file0

My recommendation is decisive: **do not treat the current GLx as a release-ready replacement**. Treat it as an unusually strong transitional substrate, then drive it through one final architectural cutover: make GLx own all rendering paths end-to-end, introduce a real five-tier execution model, finish the feature-closure matrix against legacy GL and FnQL, and build a proper scene-linear SDR/HDR color pipeline with platform-specific output backends. fileciteturn16file0 fileciteturn17file0 fileciteturn18file0

## What is already good in the current implementation

The codebase already has the right seam lines for a modern replacement renderer. There is a dedicated public GLx API surface, a renderer-common bridge, and a module object that owns capability detection, debug state, material state, postprocess state, profiling, static-world state, and dynamic stream state. That is the right architecture for a clean renderer module with a stable C ABI and modern C++ internals. fileciteturn9file0 fileciteturn9file1 fileciteturn9file2

The repo also already contains meaningful GPU-oriented design elements. The capability logic recognizes map-buffer-range style streaming, UBO-style structure, sync objects, indirect draw, multi-draw indirect, direct state access, debug output, and timer queries. The stream logic already implements the correct basic fallback ladder: persistent-mapped when buffer storage and sync are available, then map-range, then orphan/subdata. That is exactly the correct direction for reducing CPU overhead and avoiding pipeline stalls. fileciteturn10file0 fileciteturn11file0 citeturn1search0turn10search3turn2search3turn10search2turn1search1turn0search1

The material system is also a serious start, not a stub. The checked-in material key logic already models single-texture, several multitexture combine modes, fog, texmods, environment mapping, depth-fragment behavior, wave functions, fog-adjust behavior, and detailed per-stage language keys. The GLx logic test harness explicitly says it covers prepared id Tech 3 stage-language dimensions including `rgbGen`, `alphaGen`, waveforms, `tcGen`, ordered `tcMod` chains, detail stages, fog adjustment, blend/depth/alpha-test flags, dynamic-light/screen-map/video-map gating, stream strategy choice, and capability parsing. That is a valuable foundation for full feature closure. fileciteturn12file0 fileciteturn8file2

Verification is better than usual for an in-flight renderer. The repository already has deterministic GLx logic tests, header-boundary tests to prevent contamination from legacy renderer internals, and a GitHub workflow that builds the logic tests, generates RC gate plans, and supports optional self-hosted runtime sweeps with screenshot and performance baselines. That means the project is already thinking in terms of proof, not vibes. fileciteturn8file1 fileciteturn8file2

## Where the current implementation misses your target bar

Task E status: the public `Compat/Core/Advanced` classifier has been removed from the active policy surface. The old broad buckets now survive only as internal capability hints for accelerator choices.

The single largest miss is **legacy dependency**. The repo documentation says GLx “currently reuses the compatibility OpenGL renderer as its rendering baseline,” and the display guide says GLx preserves the existing OpenGL display/bloom surface while GLx-owned capability, streaming, static-world, material, and profiling paths are brought up behind compatibility fallbacks. That is a valid migration tactic, but it is not acceptable as the final state you requested. fileciteturn16file0 fileciteturn14file0

The current tier model is also too shallow and starts too high. The implemented logic treats anything below 2.1 as below floor, then chooses only `Compat`, `Core`, or `Advanced`. By contrast, the feature milestones in OpenGL itself clearly break at several points: framebuffer objects are core in 3.0, uniform buffers in 3.1, sync objects in 3.2, timer queries and instanced arrays in 3.3, multi-draw indirect and debug output in 4.3, buffer storage in 4.4, and direct state access in 4.5. Meanwhile, Apple’s profile surface tops out at 4.1 core. A real production design therefore needs the tier ladder you requested, not the repo’s current three-bucket classifier. fileciteturn10file0 citeturn2search1turn1search4turn2search3turn0search1turn11search1turn10search2turn2search0turn10search3turn1search1turn3search0

The high-detail-map story is promising but still too experimental. In the current GLx profile table, the conservative RC profile leaves several static-world acceleration paths off, while the `stress` profile turns on things like static-world indirect buffering and multi-draw-indirect variants. That means the renderer has **some** of the right big-map machinery, but not yet as part of the stable default path. If the goal is “basic maps to extremely high-detailed modern maps,” those paths must move from experimental stress mode into a measured shipped pipeline. fileciteturn9file0

Feature closure is not finished. The same profile table shows that dynamic-light, screen-map, and video-map stream draws are still disabled in the shipped profiles, and the material system, while strong, is still a curated allowlist rather than a proven universal replacement for everything the legacy renderer and all FnQL rendering features can do. The repo’s own GLx plan and review documents point in exactly this direction: strong progress, but not yet enough to retire `opengl` or `opengl2` safely. fileciteturn9file0 fileciteturn18file0 fileciteturn18file1

HDR and color management are the other major miss. The current docs describe `r_hdr` in terms of framebuffer precision and discuss bloom, gamma, greyscale, and render scaling. They do **not** describe a scene-linear exposure pipeline with output transforms for SDR, scRGB, HDR10 PQ, or macOS EDR, and they do not document color grading support. On the platform side, the modern path is available: Windows Advanced Color uses high-bit-depth composition and preserves extended color through an HDR-aware desktop pipeline; SDL 3 exposes monitor ICC profiles and window HDR state/headroom; Apple documents extended linear sRGB/EDR-oriented color spaces; Wayland has a staging color-management protocol that explicitly includes SDR/HDR colorimetry and HDR metadata. None of that is currently expressed as a cohesive GLx output system. fileciteturn14file0 citeturn7search8turn5search0turn5search2turn8search6turn9search1

## The target architecture this renderer should become

The right end state is **one renderer**, not a family of ad hoc OpenGL lineages. Internally, it should be a modern C++ renderer with a stable C ABI boundary, exactly as the bridge/API shape already suggests. Architecturally, it should split into a renderer-independent front end and a set of **GLx-owned execution backends**. The front end should compile the Quake/FnQL rendering intent into a deterministic intermediate representation: passes, materials, buffer uploads, world packets, dynamic draws, postprocess nodes, screenshot/export jobs. The back ends should then execute that IR according to the active tier. That is how you satisfy “no fallback to legacy OpenGL” while still supporting multiple generations of hardware. fileciteturn9file0 fileciteturn9file1 fileciteturn9file2 fileciteturn17file0

The tier model should be explicit and first-class:

**GL 1.2 tier**  
This must be a **GLx-owned fixed-function compatibility executor**, not a handoff to the old renderer. It should guarantee correctness, gameplay safety, basic maps, lightmaps, multitexture/lightmap composition, fog, sprites, beams, dynamic lights, stencil shadows if available, screenshots, UI, and demos. It should be SDR-only, no material compiler, no modern HDR/post chain requirement, and no promise of heavy-map acceleration. The key is that it stays inside the GLx module. The current repo does not have this tier at all. fileciteturn10file0

**GL 2.x tier**  
This should be the first programmable tier, built around GLSL-era execution, VBOs when present, and shader-based material execution for the most common stage shapes. It should own the baseline programmable path and eliminate old assembly-era assumptions. This is where the material IR starts to matter. The repo is partially here already, but still carries compatibility dependence. fileciteturn12file0 citeturn4search8turn4search0

**GL 3.x tier**  
This should be the real minimum “modern pipeline” tier: FBOs, structured postprocess, UBO-backed frame/object state, sync-aware uploads, timer queries, instancing where it actually helps, and a robust static/dynamic buffer model. The OpenGL feature timeline strongly supports this as the first truly comfortable performance tier. citeturn2search1turn1search4turn2search3turn0search1turn11search1

**GL 4.1 tier**  
This is the **macOS ceiling tier** and should be treated as a named product target, not a degraded afterthought. It can support a strong modern renderer, but not the whole 4.3–4.6 convenience stack. That means no architectural dependence on multi-draw indirect, debug output as a requirement, DSA as a requirement, or buffer storage as a requirement. If the renderer is designed correctly, 4.1 should still look modern and perform well. citeturn3search0

**GL 4.6 tier**  
This is the full-fat Windows/Linux path: persistent mapped buffers through buffer storage, multi-draw indirect, DSA, KHR_debug, refined staging/allocation models, and the highest-end static-world submission path. This is where you turn the renderer into a monster on heavy maps. citeturn10search3turn10search2turn1search1turn2search0turn4search6

This architecture also needs a **real color pipeline**. The correct design is: scene rendering in linear light, linear/sRGB-correct texture handling, exposure and bloom in scene-linear space, color grading after exposure in linear or log space, then output transforms for SDR sRGB, Windows scRGB/HDR10, macOS extended-linear-sRGB/EDR, and Linux HDR where the compositor stack actually supports it. The current docs already acknowledge framebuffer precision, sRGB conversion behavior, and bloom/post infrastructure; the missing step is to turn that into a fully color-managed output system instead of just “better FBO precision.” fileciteturn14file0 citeturn12search0turn7search8turn5search2turn8search6turn9search1

## Task-based implementation plan for GPT-5.5 agents

The tasks below are written as production work items, not brainstorming notes. The rule for all of them is simple: **GLx owns the path, the old renderer does not**. The repo already has the right bridge points and the right proof culture; these tasks are about finishing the cutover and raising the bar. fileciteturn9file2 fileciteturn8file1 fileciteturn8file2

### Architectural cutover tasks

All architectural cutover tasks are now implemented. The remaining tasks below this section build out tier-specific execution, color management, and promotion proof on top of the completed cutover surface.

**Task A — Freeze the final GLx contract**  
Define a renderer ADR that makes the following non-negotiable: stable C ABI, modern C++ internals, no runtime delegation to legacy OpenGL draw ownership, five GL tiers, deterministic pass order, and a scene-linear color pipeline.  
**Done when:** the design doc exists, the tier/feature matrix is explicit, and every later task references it.  
**Implemented by:** [GLX_FINAL_CONTRACT.md](../GLX_FINAL_CONTRACT.md).

**Task B — Replace transitional ownership with GLx ownership**  
Use the current bridge/API seam as the cut line. Move all render-time decision ownership into GLx. Shared types and compatibility helpers may remain in common code, but `code/renderer` must stop being the place where final draw behavior lives for the GLx renderer.  
**Contract:** [Ownership Contract](../GLX_FINAL_CONTRACT.md#ownership-contract) and [ABI Contract](../GLX_FINAL_CONTRACT.md#abi-contract).  
**Done when:** there is no GLx rendering path whose success depends on “fall back to legacy renderer” semantics. fileciteturn9file1 fileciteturn9file2 fileciteturn16file0

**Implemented by:** GLx-owned draw submission in `glx_draw.*`, the `GLX_Renderer_DrawElements` / `GLX_Renderer_DrawArrays` bridge cutover, `r_glxRequireOwnership`, ownership diagnostics, and runtime sweep enforcement for the `glx-ownership` profile.

**Task C — Introduce render IR and executor interfaces**  
Create explicit IR types for `FramePass`, `WorldPacket`, `DynamicDraw`, `MaterialIR`, `UploadPlan`, `PostNode`, and `OutputTransform`. Create per-tier executors that consume the same IR.  
**Contract:** [Render Products](../GLX_FINAL_CONTRACT.md#render-products) and [Product Tier Matrix](../GLX_FINAL_CONTRACT.md#product-tier-matrix).  
**Done when:** all GL tiers render from the same front-end products and only the executor changes.

**Implemented by:** `glx_render_ir.h`, `glx_executor.*`, and the GLx module bridge hooks described in [GLX_RENDERER.md](../GLX_RENDERER.md#implementation-map).

**Task D — Lock the pass schedule**  
Do not build a giant general-purpose frame graph. Keep the current id Tech 3 / FnQL-compatible deterministic schedule: world opaques, entities, transparent layers, weapon, 2D/HUD, postprocess, screenshot/export. Postprocess internals may be graph-like, but frame order must stay deterministic.  
**Contract:** [Pass Order](../GLX_FINAL_CONTRACT.md#pass-order).  
**Done when:** pass order is emitted once from the front end and validated in tests and capture logs. fileciteturn18file0 fileciteturn17file0

**Implemented by:** the GLx front-end pass schedule emission in `glx_module.cpp`, render-IR schedule validation/hash helpers, executor schedule consumption, and runtime sweep validation of the `glx: pass schedule ...` capture line.

### Tiered execution tasks

All tiered execution tasks are now implemented. The remaining tasks below this section build material compilation, map-scale feature closure, color management, and promotion proof on top of the completed five-tier executor ladder.

**Task E — Replace the current 2.1 floor with the required five-tier ladder**  
Rewrite capability probing so the active product tier is one of: `GL12`, `GL2X`, `GL3X`, `GL41`, `GL46`. Preserve feature flags underneath that, but make the shipped policy tier visible and testable.  
**Contract:** [Product Tier Matrix](../GLX_FINAL_CONTRACT.md#product-tier-matrix) and [Tier Feature Matrix](../GLX_FINAL_CONTRACT.md#tier-feature-matrix).  
**Done when:** startup logs, tests, and diagnostics all report the five-tier model, and the old `Compat/Core/Advanced` abstraction is removed or demoted to an internal hint layer. fileciteturn10file0

**Implemented by:** `Capabilities::tier` now uses `RenderProductTier` (`GL12`, `GL2X`, `GL3X`, `GL41`, `GL46`), the former broad capability buckets are internal `CapabilityHint` values, startup/`glxinfo`/`r_speeds 7` diagnostics report the product tier, and logic/runtime-sweep tests reject old active-tier names.

**Task F — Build the GL 1.2 executor**  
Implement a first-class fixed-function compatibility executor inside GLx. Support lightmaps, multitexture composition, fog, 2D, beams, sprites, screenshots, and demo-safe rendering behavior. Explicitly document which advanced FnQL effects are unavailable on this tier.  
**Contract:** [`GL12` tier](../GLX_FINAL_CONTRACT.md#product-tier-matrix) and [Tier Feature Matrix](../GLX_FINAL_CONTRACT.md#tier-feature-matrix).  
**Done when:** GL 1.2 can launch, render stock gameplay correctly, and never calls into the legacy OpenGL renderer.

**Implemented by:** `GL12` now has a named fixed-function executor policy in `glx_render_ir.h` and `glx_executor.*`, submits client-memory indexed/array draws through GLx-owned `glx_draw.*`, disables stream VBO initialization on GL12, treats GLSL material compilation, modern post/HDR nodes, and transient stream uploads as unavailable on that tier, reports the GL12 fixed-function contract in diagnostics, and has logic/runtime-sweep tests for the supported and unavailable GL12 surfaces.

**Task G — Build the GL 2.x programmable executor**  
Implement GLSL-era execution for common materials, dynamic entities, and postprocess-lite behavior. Do not require later conveniences. This is the baseline programmable path.  
**Contract:** [`GL2X` tier](../GLX_FINAL_CONTRACT.md#product-tier-matrix) and [Tier Feature Matrix](../GLX_FINAL_CONTRACT.md#tier-feature-matrix).  
**Done when:** the majority of stock maps and core FnQL visual features run through the material compiler on GL 2.x-class systems.

**Implemented by:** `GL2X` now has a named programmable executor policy in `glx_render_ir.h` and `glx_executor.*`; it accepts transient stream uploads, GLSL material products, common prepared material shapes, dynamic draws, and postprocess-lite nodes, while rejecting persistent/sync-required upload products, scene-linear output, tone mapping, color grading, and other modern post requirements. Diagnostics and runtime-sweep tests now validate the `GL2X programmable executor` contract.

**Task H — Build the GL 3.x performance executor**  
Promote FBO-backed postprocess, UBO-backed frame/object constants, timer queries, sync-aware uploads, and stronger static/dynamic buffer ownership into the default GL 3.x path.  
**Contract:** [`GL3X` tier](../GLX_FINAL_CONTRACT.md#product-tier-matrix) and [Tier Feature Matrix](../GLX_FINAL_CONTRACT.md#tier-feature-matrix).  
**Done when:** GL 3.x becomes the first fully modern-feeling shipped tier. citeturn2search1turn1search4turn2search3turn0search1

**Implemented by:** `GL3X` now has a named performance executor policy in `glx_render_ir.h` and `glx_executor.*`; it advertises required FBO-backed postprocess, UBO-style frame/object constants, timer-query support, sync-aware uploads, static buffer ownership, dynamic buffer ownership, modern post-chain, scene-linear output, screenshots, and demos, while rejecting GL4-only persistent mapped uploads, indirect submission, and direct-state-access requirements. Diagnostics and runtime-sweep tests now validate the `GL3X performance executor` contract.

**Task I — Build the GL 4.1 macOS executor**  
Design this tier around the actual Apple ceiling. Support the modern renderer without depending on 4.3+ or 4.5+ features.  
**Contract:** [`GL41` tier](../GLX_FINAL_CONTRACT.md#product-tier-matrix) and [Tier Feature Matrix](../GLX_FINAL_CONTRACT.md#tier-feature-matrix).  
**Done when:** macOS has a named “fully supported modern tier” with no accidental dependence on unavailable 4.3/4.4/4.5 features. citeturn3search0

**Implemented by:** `GL41` now has a named `mac-modern` executor policy in `glx_render_ir.h` and `glx_executor.*`; it supports the full modern GLx render-product surface used by the shipped macOS ceiling tier, including FBO postprocess, UBO-style frame/object constants, timer queries, sync-aware uploads, static/dynamic buffer ownership, modern post-chain, scene-linear output, high-quality SDR, optional hardware HDR output, screenshots, and demos. The policy and diagnostics explicitly reject required GL4.3 debug output, GL4.4 buffer storage/persistent mapped uploads, GL4.5 direct-state access, and required multi-draw-indirect submission, with logic/runtime-sweep tests guarding the `GL41 mac-modern executor` contract.

**Task J — Build the GL 4.6 high-end executor**  
Turn on persistent mapped uploads, DSA, multi-draw-indirect, and the most aggressive static-world submission/batching on the newest drivers.  
**Contract:** [`GL46` tier](../GLX_FINAL_CONTRACT.md#product-tier-matrix) and [Tier Feature Matrix](../GLX_FINAL_CONTRACT.md#tier-feature-matrix).  
**Done when:** Windows/Linux high-end hardware gets the best path available, with explicit counters proving the gain. citeturn10search3turn10search2turn1search1turn2search0

**Implemented by:** `GL46` now has a named high-end executor policy in `glx_render_ir.h` and `glx_executor.*`; it requires persistent mapped uploads, buffer-storage uploads, sync-heavy streaming, direct state access, multi-draw-indirect submission, aggressive static-world submission, detailed GPU counters, modern post-chain, scene-linear output, hardware HDR output, screenshots, and demos. `glxinfo` and `r_speeds 7` report high-end counters for persistent uploads, DSA products, MDI products, aggressive static products, backend GPU query counters, and static-world MDI calls/indexes, with logic/runtime-sweep tests guarding the `GL46 high-end executor` contract.

### Material, map-scale, and feature-closure tasks

All material, map-scale, and feature-closure tasks are now implemented. The remaining tasks below this section build HDR/color management, output backends, promotion proof, and release policy on top of the completed material compiler, map-scale acceleration, dynamic streaming, feature matrix, and stress-corpus surface.

**Task K — Compile id Tech 3 / FnQL shader intent into MaterialIR**  
Replace hand-grown special cases with a compiler from stage language to `MaterialIR`, then from `MaterialIR` to tier-specific program/state plans. Preserve sort order and state semantics exactly.  
**Contract:** [Render Products](../GLX_FINAL_CONTRACT.md#render-products), [Pass Order](../GLX_FINAL_CONTRACT.md#pass-order), and [Tier Feature Matrix](../GLX_FINAL_CONTRACT.md#tier-feature-matrix).  
**Done when:** material binding no longer depends on ad hoc allowlists for the common path, and unsupported combinations are explicit, logged, and test-covered. fileciteturn12file0 fileciteturn8file2

**Implemented by:** GLx now compiles prepared id Tech 3 / FnQL shader-stage language into `MaterialIR`, then validates that IR into a `MaterialStatePlan` carrying the preserved sort value plus the full GLSL program/state key. Material binding, stream material eligibility, and material telemetry consume that compiler path instead of rebuilding compact ad hoc keys at the bind site. Unsupported stage-language combinations now produce named reason bits, are surfaced through `glxmaterial`/`glxinfo` diagnostics, and are covered by pure GLx logic tests for invalid combine modes, invalid ordered texmod sequences, and invalid waveform usage.

**Task L — Finish the feature-closure matrix**  
Enumerate every feature the legacy GL renderer covers and every FnQL render feature that must survive: bloom including `r_bloom 2`, gamma, greyscale, render scaling, multisample/supersample display behavior, cel shading, outline behavior, dynamic lights, screen maps, video maps, depth-fragment paths, beams, shadows, screenshots/cubemaps, HUD and cinematic correctness.  
**Contract:** [Tier Feature Matrix](../GLX_FINAL_CONTRACT.md#tier-feature-matrix) and [Promotion Rules](../GLX_FINAL_CONTRACT.md).  
**Done when:** there is a checked-in feature matrix with `covered / partially covered / missing` status and zero ambiguous rows. fileciteturn14file0 fileciteturn8file2

**Implemented by:** [GLX_FEATURE_MATRIX.md](../GLX_FEATURE_MATRIX.md) now enumerates the GLx replacement surface with stable row IDs, exact `covered`, `partially covered`, or `missing` statuses, current evidence, and closure gates. Runtime-sweep unit tests parse the checked-in matrix, require all Task L feature families, reject duplicate IDs, and fail on ambiguous status/evidence/closure rows.

**Task M — Promote static-world acceleration from stress-only to shipped**  
The current profile table shows that several large-map acceleration paths still live mainly in `stress`. Move the stable pieces into the shipped path: packetized static-world arenas, run coalescing, per-tier multi-draw, and high-end indirect submission.  
**Contract:** [Product Tier Matrix](../GLX_FINAL_CONTRACT.md#product-tier-matrix) and [Tier Feature Matrix](../GLX_FINAL_CONTRACT.md#tier-feature-matrix).  
**Done when:** the conservative shipped profile has a real large-map advantage, not just the stress profile. fileciteturn9file0

**Implemented by:** The `rc`/`glx-parity` profile now explicitly ships GLx static-world arenas, arena binding, device and soft static draw dispatch, packet-batch spans, same-state `glMultiDrawElements`, indirect command-buffer upload, single `glDrawElementsIndirect`, and ordered MDI span submission where the selected tier exposes the required capability. The static-world indirect paths now check the frozen capability table before attempting high-end draws, so lower tiers keep the shipped profile without repeated unsupported MDI attempts. `stress` is narrowed to the remaining compact visible-command MDI upload path, and runtime-sweep tests freeze that profile split.

**Task N — Unify dynamic scene streaming**  
Make all dynamic scene geometry use one GLx-owned transient upload system with tier-appropriate allocators. Separate static mesh storage from per-frame mutable data.  
**Contract:** [Render Products](../GLX_FINAL_CONTRACT.md#render-products) and [Tier Feature Matrix](../GLX_FINAL_CONTRACT.md#tier-feature-matrix).  
**Done when:** entities, particles, polys, marks, weapon, UI quads, beams, and special passes all pass through the same reservation/upload/commit model, with per-category metrics. fileciteturn11file0 fileciteturn9file0

**Implemented by:** GLx dynamic streaming now carries an explicit dynamic-scene category mask through the renderer-common bridge, GLx module ABI, and transient stream state. Generic streamed shader batches infer entity, weapon, UI, particle, poly, mark, or special category from the active tessellation, entity, shader sort, and pass flags; immediate beams, stencil-shadow quads, and postprocess draw-array batches pass explicit category masks into the same stream helpers. All category-aware paths still reserve, upload, and commit through the single `glx_stream.*` ring selected by the tier-appropriate stream strategy, while static-world storage remains in the static arena/packet path. `glxinfo` and `r_speeds 7` now report per-category draw/attempt/fallback metrics, and parser plus logic tests keep the category vocabulary and diagnostics locked.

**Task O — Add heavy-map and modern-map stress content gates**  
Create an official corpus of stock maps, high-geometry maps, shader-heavy maps, fog-heavy maps, particle-heavy demos, and UI/HUD-sensitive scenes.  
**Contract:** [Promotion Rules](../GLX_FINAL_CONTRACT.md).  
**Done when:** every release and CI artifact references the same corpus for screenshots and performance comparisons.

**Implemented by:** The runtime sweep now owns a versioned GLx proof corpus with stable scene IDs for retail stock maps, high-geometry probes, shader-heavy probes, fog-sensitive probes, staged modern-map stress scenes, a particle-heavy stress timedemo slot, and UI/HUD-sensitive screenshots. Named RC gates derive their default maps and demos from that corpus, include selected scene IDs and tags in manifests, attach corpus scene metadata to screenshots, embed the corpus object into approved performance-baseline JSON, and fail non-dry-run gates if required corpus tags drift. CI dry-run gate artifacts print and upload the corpus reference document, and release packaging includes the same document plus `glx_proof_corpus` metadata in the release manifest.

### HDR, color grading, and output tasks

**Task P — Redefine HDR from “precision mode” to “scene-linear pipeline”**  
Keep the internal precision control, but redesign `r_hdr` semantics around a real scene-linear renderer: exposure, bloom thresholding, grading, and output transforms.  
**Contract:** [Color Pipeline Contract](../GLX_FINAL_CONTRACT.md#color-pipeline-contract).  
**Done when:** renderer docs no longer describe HDR merely as 4-bit/8-bit/16-bit framebuffer precision. fileciteturn14file0

**Implemented by:** `r_hdr` now represents scene-linear HDR pipeline intent while `r_hdrPrecision` owns automatic/debug/8-bit/16-bit internal storage selection across OpenGL/GLx and Vulkan. GLx carries the color pipeline through `OutputTransform` with scene color space, output transfer, exposure, tone-map operator, grading mode, bloom threshold/knee, paper-white, and max-output fields; `glxpostprocess`, `r_speeds 7`, and the runtime sweep parser expose that state. The OpenGL/GLx FBO path now chooses storage through `r_hdrPrecision`, applies scene-linear exposure/tone-scale state in the ARB final pass, and records bloom thresholding as scene-linear pipeline data.

**Task Q — Make color handling physically sane**  
Adopt correct sRGB decode/encode rules, ensure `GL_FRAMEBUFFER_SRGB` behavior is correct where applicable, audit all texture formats, and keep blending in linear space where the destination encoding requires it.  
**Contract:** [Color Pipeline Contract](../GLX_FINAL_CONTRACT.md#color-pipeline-contract) and [Tier Feature Matrix](../GLX_FINAL_CONTRACT.md#tier-feature-matrix).  
**Done when:** SDR output is color-correct, screenshot baselines stop drifting due to gamma mistakes, and the renderer has a color-space audit document. citeturn12search0turn7search9

**Implemented by:** GLx/OpenGL and Vulkan images now carry explicit color-space metadata, authored color textures use sRGB sampled formats in the scene-linear `r_hdr 1` path through `r_srgbTextures`, lightmaps/fog/dynamic-light/data textures stay linear or data, the OpenGL final pass shader applies SDR sRGB output encoding for scene-linear output, the current shader-encoded SDR path keeps `GL_FRAMEBUFFER_SRGB` disabled to avoid double encoding, `glxpostprocess`/`r_speeds 7` expose color-audit diagnostics, screenshots/video are documented as SDR sRGB captures, and [GLX_COLORSPACE_AUDIT.md](../GLX_COLORSPACE_AUDIT.md) records the texture, framebuffer, blending, and capture audit.

**Task R — Add color grading and tone mapping**  
Implement at minimum: exposure control, filmic tone map, lift/gamma/gain controls, white-point adaptation, and 3D LUT color grading. Keep defaults conservative and demo-safe.  
**Contract:** [Color Pipeline Contract](../GLX_FINAL_CONTRACT.md#color-pipeline-contract) and [Pass Order](../GLX_FINAL_CONTRACT.md#pass-order).  
**Done when:** there is a dedicated postprocess grading stage with test scenes and user-facing controls.

**Implemented by:** OpenGL/GLx and Vulkan now expose a conservative scene-linear grading surface with `r_colorGrade`, lift/gamma/gain vectors, Bradford white-point adaptation, `r_colorGradeLUT` 3D atlas grading, and `r_colorGradeLUTScale`; defaults are disabled or identity so retail demos and SDR output remain stable. The final postprocess order is exposure, optional grading, tone mapping, and output encoding, with GLx `OutputTransform` and post nodes recording grade and tone-map stages separately. Runtime diagnostics, `glxpostprocess`, `r_speeds 7`, the runtime sweep parser, the `glx-color` proof profile, and the proof corpus now expose color-grade state and retail tone/grading proof scenes.

**Task S — Add platform output backends for true HDR hardware support**  
Implement an output abstraction with at least these targets: SDR sRGB; Windows scRGB and HDR10-capable output; macOS extended-linear-sRGB/EDR output; Linux HDR behind explicit compositor/protocol checks. Use SDL monitor ICC and HDR state/headroom where available.  
**Contract:** [Color Pipeline Contract](../GLX_FINAL_CONTRACT.md#color-pipeline-contract) and [Render Products](../GLX_FINAL_CONTRACT.md#render-products).  
**Done when:** the renderer can query display state, select an output transform, and prove correct behavior on at least one HDR-capable Windows system and one Apple EDR-capable system; Linux remains experimental until compositor support is validated. citeturn7search8turn5search0turn5search2turn6search5turn8search6turn9search1

**Implemented by:** The platform/renderer ABI now has `rendererDisplayOutput_t` plus `GLimp_QueryDisplayOutput`, with SDL3 querying display HDR state, window HDR headroom, SDR-white hints, monitor ICC profile bytes, Windows monitor handles, Wayland output handles, and platform-specific native backend capability. GLx records `r_outputBackend` and `r_outputAllowExperimentalLinuxHDR`, selects SDR sRGB, Windows scRGB, HDR10/PQ, macOS EDR, or Linux experimental HDR transforms only when `r_hdr 1` and platform state allow it, and exposes request/selected/native backend state in `OutputTransform`, `glxpostprocess`, and compact `r_speeds 7` output. Vulkan now maps explicit HDR10 output requests and legacy `r_hdrDisplay 1` to HDR10 swapchain negotiation, clamps HDR metadata/tone scale against queried display headroom when available, and reports the same output-backend state in `vkinfo`. Linux HDR remains opt-in behind SDL compositor/protocol checks and `r_outputAllowExperimentalLinuxHDR`; the hardware proof slots are represented in the runtime parsers and docs so Windows HDR and Apple EDR artifacts can be archived as release evidence.

### Performance, testing, and release tasks

**Task T — Turn runtime proof from optional to mandatory**  
The current workflow has strong logic tests and optional runtime sweeps. Make GPU-backed runtime parity runs mandatory for release promotion and at least periodic for mainline development.  
**Contract:** [Promotion Rules](../GLX_FINAL_CONTRACT.md).  
**Done when:** screenshot baselines, performance baselines, and proof artifacts are required, not optional side runs. fileciteturn8file1 fileciteturn8file2

**Implemented by:** `glx_runtime_sweep.py` now writes stable `proofPlatform` metadata, exposes a release proof-root validator, and re-evaluates non-dry-run `rc-smoke`, `rc-parity`, and `rc-proof` manifests for the blocking `windows-x64` and `linux-x86_64` matrix. `release.py --channel release` requires `--glx-proof-root` and refuses tagged packaging unless that validator passes, while `glx-verification.yml` runs a scheduled self-hosted `rc-parity` sweep for mainline and preserves manual proof-gate dispatch for reviewed baselines. Docs and tests now describe and enforce the mandatory proof policy.

**Task U — Expand the current counters into hard budgets**  
Use the existing profiler surface to define per-tier budgets for draw calls, upload volume, fallback counts, shader binds, static packet misses, same-frame stream wrap rejects, and GPU frame time.  
**Contract:** [Product Tier Matrix](../GLX_FINAL_CONTRACT.md#product-tier-matrix), [Tier Feature Matrix](../GLX_FINAL_CONTRACT.md#tier-feature-matrix), and [Promotion Rules](../GLX_FINAL_CONTRACT.md).  
**Done when:** CI and RC proof jobs fail when the renderer regresses outside approved thresholds. fileciteturn9file0 fileciteturn8file1

**Implemented by:** `glx_runtime_sweep.py` now parses numeric GPU frame time, shader/material bind pressure, stream upload volume, same-frame wrap rejects, static draw packet misses, static queue packet misses, and static packet lookup misses from `r_speeds 7` GLx captures. The built-in performance budget now has global zero-tolerance fallback/error limits plus per-tier hard budgets for `GL12`, `GL2X`, `GL3X`, `GL41`, and `GL46`; modern tiers also require a numeric GPU frame-time sample. Budget overrides merge by tier, runtime gate manifests record the active budget, and `evaluate_gate` fails RC proof jobs when aggregate samples exceed those thresholds.

**Task V — Add deterministic image and demo parity gates**  
Create screenshot, demo playback, HUD, shadow, bloom, and cel-shading parity suites. Keep absolute scene lists versioned in the repo.  
**Contract:** [Pass Order](../GLX_FINAL_CONTRACT.md#pass-order) and [Promotion Rules](../GLX_FINAL_CONTRACT.md).  
**Done when:** “fully compatible” is backed by artifact evidence, not manual confidence.

**Implemented by:** The GLx proof corpus now carries versioned screenshot, demo-playback, HUD, shadow, bloom, and cel-shading parity suites with fixed scene IDs and suite-specific cvar overrides for stencil-shadow and cel/outline captures. `rc-proof` and `rc-stress` require those suite records in their manifests, the sweep validates suite versions/tags/scene membership during gate evaluation and release proof-root checks, screenshot entries identify their parity suites and suite cvars, and docs/tests freeze the suite contract so artifact evidence cannot silently drift from the promotion rules.

**Task W — Promote GLx only after passing the full matrix**  
Do not alias `opengl` to `glx` until the feature matrix is green, the five tiers are real, runtime proof is mandatory, and the legacy dependency is gone.  
**Contract:** [Promotion Rules](../GLX_FINAL_CONTRACT.md).  
**Done when:** `opengl` can safely become a migration alias and `opengl2` can move behind a legacy flag with a rollback package. fileciteturn18file0 fileciteturn18file1

**Implemented by:** GLx promotion is now a separate machine-readable gate in `scripts/glx_promotion.py`. It requires an all-covered feature matrix, the five product tiers, passing release proof for the blocking runtime matrix, passing `glx-ownership` proof with zero legacy delegation, and the migration/rollback contract in `GLX_PROMOTION.md`. Release packaging records the promotion report and refuses a source tree that has already promoted renderer defaults before the gate reports ready, while CI publishes the current blocked-but-policy-compliant promotion report beside the GLx gate plans.

**Task X — Finish the productization work**  
Update build defaults, help text, docs, migration notes, and troubleshooting so GLx is described as the canonical OpenGL-lineage renderer, not an experiment.  
**Contract:** [Promotion Rules](../GLX_FINAL_CONTRACT.md) and [Consequences](../GLX_FINAL_CONTRACT.md#consequences).  
**Done when:** the docs and build system stop calling GLx experimental. fileciteturn16file0 fileciteturn14file0

**Implemented by:** GLx is now productized as the canonical OpenGL-lineage renderer while remaining promotion-gated for default selection. Make and CMake include the GLx modular renderer by default without changing `RENDERER_DEFAULT=opengl`, current user docs link a GLx renderer guide with migration and troubleshooting notes, build/help text no longer labels GLx experimental, release packages include the GLx guide and promotion policy, and tests freeze the split between “built and documented as canonical” and “not aliased/default until promotion proof is ready.”

## Release gates and open questions

The release gate should be strict. GLx is ready to replace the legacy renderers only when all of the following are true: it no longer depends on legacy OpenGL draw ownership; it fully covers the legacy GL and FnQL feature matrix; it ships all five execution tiers you requested, including a real internal GL 1.2 tier; it has a true scene-linear SDR/HDR pipeline with color grading and at least Windows/macOS hardware-output proof; and its screenshot/demo/performance proof is automated and mandatory. Until then, it is still a migration candidate, not the final renderer. fileciteturn16file0 fileciteturn14file0 fileciteturn8file1

The only meaningful open questions are policy questions, not renderer-shape questions. The project still needs to decide whether it is willing to pay the engineering cost of a **real internal GL 1.2 executor**, whether it will raise the internal GLx toolchain to a stronger modern C++ baseline, how aggressive Linux HDR support should be given compositor/protocol maturity, and whether macOS HDR output is a first-wave deliverable or a second-wave one. Those are important, but none of them change the central conclusion: the path to a definitive GLx replacement is clear, and the next step is an ownership cutover plus full proof, not another round of partial experiments. fileciteturn10file0 citeturn3search0turn9search1
