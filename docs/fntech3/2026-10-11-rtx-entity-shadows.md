# RTX entity shadows, frame cost and input pacing — 2026-10-11

The reported problem was entity shadows, especially the first-person player's
BODY shadow, moving in steps, together with low frame rate and delayed input.
The shared renderer contained several mechanisms consistent with those symptoms:
direct shadows inherited long illumination history on static receivers, old dark
history capped newly revealed light, dynamic geometry was repeatedly rebuilt,
and GPU waits occurred after input sampling. The changes below address those
mechanisms without reducing body-pose or direct-shadow update frequency.

Evidence is **Source, Unit, and a bounded Q3 Live subset**. No profile gate closes.
The synthetic BODY probe exercises first-person shadow visibility and current
geometry, but does not establish every game's player-animation behaviour.
The adjacent [machine-readable record](2026-10-11-rtx-entity-shadows.json) identifies
source snapshots, builds, commands, fixtures and retained runtime evidence.

## Audit dispositions

| Finding | Implemented response and evidence |
| --- | --- |
| 1. A static receiver keeps a moving entity's shadow in long history. | Direct, indirect and specular histories are separate. Dynamic shadow blockers, including BODY, mark direct illumination reactive for 150 ms; reactive direct response is 35 ms, static direct 320 ms, specular 60 ms and indirect 160 ms. Response uses elapsed time. Executed shader math covers 20, 60, 120 and 240 FPS; consecutive engine captures move the shadow on every frame. |
| 2. History-derived firefly clipping suppresses light when an entity uncovers it. | Compatible current-neighbourhood bounds clip old history instead of capping new light to old darkness. Frozen accumulation remains a running average; indirect variance includes sample residual and effective sample count. Pulse, dark-to-light, outlier and frozen-mean tests execute the actual helper math. |
| 3. An RTX-branded GPU does not establish the active tracing backend. | Startup/capability and timing reports name compute or hardware. Compute defaults to indirect divisor 2, hardware to 1. An optional measured GPU budget adjusts indirect density with hysteresis. The local Win32 driver exposes no required KHR ray-tracing capability; the Q3-only x64 probe runs the hardware path. See the architecture finding below. |
| 4. Input/view work precedes GPU waits. | Optional internal renderer API 35 frame events move fence waits and presentation acquisition before OS input polling. Default `rtx_rt_low_latency 1` limits GPU work to one frame in flight; 2 allows two, 0 restores late waits. Monotonic input/simulation/submit/present events and rolling CPU/GPU p50/p95/p99 distinguish frame cost from wait placement. No queue-idle workaround or game-clock change is introduced. |
| 5. Full raster shading is discarded under the traced image. | Trace-owned base surfaces write depth only. Opaque surfaces use position-only depth; cutouts retain authored alpha coverage and EQUAL-depth dependencies. Raster shadow maps are scheduled from their remaining consumers, retaining all required casters. A permissive trace failure clears the partial base and redraws complete raster colour, lighting, shadows and fog. Executed coverage tests and compute/hardware failure/recovery captures pass. |
| 6. Dynamic geometry and acceleration structures are rebuilt needlessly. | A bounded, exact-pose model-space cache handles MD3/MDR/IQM/brush models; current transforms, lighting and motion are applied each frame. IQM scratch is reused. Compute BVHs refit exact connectivity with cost/age rebuild thresholds; hardware BLASes UPDATE stable topology with periodic rebuilding. Cache tests include tiny interpolation changes, transforms, material changes, eviction, invalidation and allocation failure. No animation quantization or caster throttling is used. |
| 7. Nearest matching can swap histories between identical models. | Internal entity IDs where supplied by RTCW/ET, model/part topology, material generation and one-to-one matching replace proximity-only reuse. Ambiguous identity-free duplicates, changed topology/materials, absent matches and teleports reject history. External module ABIs remain unchanged. A reused ID with no supplied spawn generation cannot universally distinguish a respawn. |
| 8. Frequently read uploads use host memory without a VRAM preference. | Material, stage, light and dynamic uploads prefer coherent host-visible device-local memory, retaining portable allocation fallback. Heap/type/flags are logged. The latched diagnostic `rtx_rt_device_local` permits A/B. The probe confirms different selected heaps; varying GPU contention prevents an isolated speedup claim. |
| 9. Missing indirect samples and reused light choices need explicit treatment. | Indirect validity and history are independent of direct light. Missing samples are not black observations. Weighted proposals improve light selection. Optional common-random-number replay shares four rotating candidate streams across compatible primary receivers; current PDFs, selection and visibility are always evaluated. Energy and current-visibility tests pass. Runtime noise did not improve, so `rtx_rt_light_reuse` remains **0 by default**. This is not ReSTIR reservoir reuse. |
| 10. Full-resolution tracing and large intermediates dominate cost. | Independent indirect divisors 1–4, rotating coverage phases, depth/normal-guided reconstruction and an optional GPU budget reduce bounce work while preserving full-rate primary/direct work. Depth/normal and motion use RG32UI; moments store mean/stddev in RGBA16F. Shared variance work is computed once. The 25 intermediate images total 192 bytes/pixel, 180 MiB at 720p. Their storage remains full resolution. |
| 11. A cell's 64-light cap removes other emitters from sampling. | Overflow cells, omitted entries and importance are counted. The global proposal has power weighting with a 10% uniform floor and participates in the exact local/global mixture PDF, including MIS replacement. Every light retains support. Sorted retained IDs make membership a binary search. Executed Monte Carlo tests cross disjoint cell lists and omitted lights, with and without reuse and changing visibility. |
| 12. `RF_NOSHADOW` is not a separate ray visibility policy. | Internal vertex metadata excludes flagged entities only from shadow visibility; primary/reflected geometry remains. Hardware uses a shadow-specific any-hit SBT group. BODY is excluded from primary rays but remains a shadow caster unless flagged. Software/hardware tests and captures show the shadow disappearing with the flag. |

The implementation is shared across profiles. It retains fnTech3's per-pixel
lighting, traced shadows, surface emitters, bounced light, reflections, material
maps, fog and raster overlays. It introduces no retail-exact presentation default.
MDS/ET procedural poses and IHV camera-dependent interpolation bypass the pose
cache because their complete keys are not represented; they retain current-pose
extraction. Unsupported model formats retain their existing raster path.

## Runtime observations

All captures use the engine's `screenshotPNG` command through
`scripts/rtx_probe.py`: a windowed, unswitched private desktop, disabled input,
an isolated writable home and a read-only legitimate Q3 installation. No input
was injected and no operating-system screenshot was used. The installation has
additional paks; these are engine-path observations, not untouched-retail image
parity. Runs retain their actual binary hashes and commands below
`.tmp/rtx-implementation-2026-10-11/`.

The machine is Windows with an NVIDIA GeForce RTX 4060 Laptop GPU and driver
591.74. The shared Win32 client uses compute. The separate Q3-only x64 client
uses the ray-tracing pipeline. Hardware runs explicitly enabled the Khronos
validation layer and logged no validation errors/VUIDs.

### BODY shadow response

`body-floor-response-720` loads `q3dm1`, positions the camera at
`212 2360 65`, yaw 315, and places a BODY box at `600 2000 0`, half-width 24.
Direct-light diagnostic 11 isolates its floor shadow. Eight consecutive captures
move it four world units each frame to x=632. `--capture-gap 1` and capture
steps `@0` keep each changed pose on its own rendered frame.

Within image rectangle `[500,415,720,475]`, the largest connected component
below mean RGB 70 moves from centroid `(612.33,431.64)` to `(583.15,427.64)`.
Every step moves approximately 2–5 pixels; none repeats the previous shadow.
After 60 settling frames the centroid is `(582.85,427.40)`, 0.38 pixels away.
The same region contains zero qualifying shadow pixels before the caster and
after `RF_NOSHADOW`. This threshold measure checks motion and settling, not
subpixel penumbra accuracy or a perceptual response constant.

The final light-reuse comparison repeats this on both backends with reuse off
and on, and with frozen accumulation disabled. All four sequences move at
every step; the final moving frame is 0.39–1.57 pixels from its settled centre,
and every no-shadow capture clears the component. The hardware run completes
with validation enabled. A stationary 16-frame image region has higher temporal
variation with proposal replay in both runs (compute mean channel stddev
3.22→5.45; hardware 4.58→5.06 on encoded 0–255 pixels). Animation and variable
frame timing limit that comparison; it establishes no quality win. Replay is
therefore an opt-in experiment, while the correctness and shadow fixes are on.

### Frame cost and input age

`latency-policy-720` compares policies 0, 1 and 2 in one process. Representative
steady intervals are:

| Policy | Input sample to submit | Fence wait before / after input | CPU frame median |
| --- | --- | --- | --- |
| 0, late waits | 26.7–29.4 ms | approximately 0 / 25.5–28 ms | approximately 28–29 ms |
| 1, one GPU frame | 1.26–2.65 ms | approximately 26.7 / 0.01 ms | approximately 28–29 ms |
| 2, two GPU frames | 2.1–2.33 ms | approximately 24–25 / 0.01 ms | approximately 28–29 ms |

These measure CPU/API timing, **not input-to-photon latency**. They support
moving waits before input; they do not establish reduced display queueing or
changed simulation responsiveness. No mouse/keyboard automation was used.

`indirect-budget-720` measured approximately 40/30/23 ms of total RT work at
divisors 1/2/4 in one run. A 20 ms target reached divisor 4 but could not meet
the target: primary/direct work sets a floor. The GPU was also used by other
running game sessions during parts of this investigation, so these are bounded
diagnostics, not isolated benchmark or general FPS claims.

Runtime counters confirm reuse of work: one compute run records 7,265 exact-pose
cache hits for 346 extractions; the hardware latency run records 57 BLAS builds
and 1,743 updates. The final compute run records 14 dynamic builds and 1,186
refits with current-frame vertices. These are work counts, not speedup ratios.
The grid probe reports 438 overflow cells and 7,842 omitted local entries
(0.91% local importance), with all 252 static lights still in the global proposal.

`memory-ab-720` selects host type 2/heap 1/flags 0x6 versus device-local host-visible
type 4/heap 0/flags 0x7. Trace and denoise time changed together during the run;
no isolated GPU-memory performance improvement is claimed.

### Architecture and GPU skinning decisions

The local x86 driver lacks `VK_KHR_acceleration_structure`, ray query and the
ray-tracing pipeline; enabling flags cannot create those features. The Q3-only
x64 build proves the hardware route on this driver, but replacing the shared
client with it would break the required x86 retail-module boundary. An out-of-process
x64 renderer would need a new scene/resource IPC contract, synchronization,
resource ownership and latency/interop validation. That is not an in-process
switch and is not represented as an implemented universal hardware solution.
The finding is specific to the probed driver, not a claim about every vendor's
32-bit implementation. The [Vulkan ray-tracing guide](https://docs.vulkan.org/guide/latest/extensions/ray_tracing.html)
and [acceleration-structure contract](https://docs.vulkan.org/spec/latest/chapters/accelstructures.html)
are API references only; no external implementation was incorporated.

GPU skinning was evaluated after pose-cache/refit work. The compute backend
still needs CPU vertices for its BVH, so GPU-only poses would add readback or
duplicate pose work. In the bounded cached compute run, total RT CPU preparation
is about 0.6–1.3 ms against tens of milliseconds of GPU tracing. Current shared
skin evaluation is retained. A hardware-only GPU-skinning path remains a separate
optimization requiring complete per-format pose fixtures; it is not necessary
to keep BODY shadows current, and no update throttling was substituted.

## Validation and remaining limits

- 134 RTX tests execute temporal math, indirect coverage/budget selection,
  packing precision, light PDFs/energy, BVH refits, exact-pose cache handling,
  motion identity, frame pacing, alpha-aware depth, fallback and visibility.
- 90 affected tests cover command waits, QL fonts, MDR poses, Elite Force raw
  images, underwater ownership and malformed asset bounds; 98 input-related
  tests and the frame-clock checks also passed.
- All 97 shader variants compile and generated payloads are refreshed.
- The final shared Win32 build and targeted Q3-only x64 client/RTX builds pass.
  Twenty documentation tests and player-guide link validation pass. The companion
  record identifies the final builds separately from earlier runtime binaries.
- Compute and hardware captures cover BODY movement, no-shadow visibility,
  permissive depth-base failure/recovery and clean shutdown. Hardware validation
  was enabled; compute captures alone do not claim that layer was enabled.

No Linux runtime, broad driver matrix, retail first-person animation corpus or
end-to-end latency measurement is supplied by this slice. Q3 evidence does not
establish QL, either RTCW mode, ET or another profile's full renderer gate.
Independent indirect density reduces ray cost, not the full-resolution image
allocation; very large scenes can still exceed 32-bit address-space limits.

Unrelated issue observed: the default `all` target in the Q3-only x64 build also
tries to build x86-only Jedi Academy ABI tests and fails their pointer/layout
assertions. The targeted Q3 client and RTX renderer build successfully; this
task does not change those unrelated ABI tests. Unrelated and concurrent work
in the shared working tree is preserved.
