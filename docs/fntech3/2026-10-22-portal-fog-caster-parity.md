# RTX portal fog and finite-face light culling, 2026-10-22

This follow-up resolves the two remaining findings from the
[RTX portal-sky shadow record](2026-10-22-rtx-portal-sky-shadows.md): a portal
sky consumed RTX's global-fog pass, and Vulkan/RTX selected more point-light
casters than GLx. Source and regression tests are in `d3547f6b`. The
[JSON companion](2026-10-22-portal-fog-caster-parity.json) records build
identities, commands, counters, capture hashes and validation limits.
No compatibility gate closes.

## Causes and changes

RTX's `vk_draw_global_fog` applies fog once per frame. RTCW draws its distant
sky as a separate full-screen scene before the world view. That scene passed
the compositor's checks and set `lastFogFrame`, leaving no fog for the world.
The compositor now rejects `skyboxPortalScene` before drawing or consuming
the frame's pass. This uses the existing profile-owned scene flag for RTCW,
Enemy Territory and Jedi Academy multiplayer. Main and post-bloom render
passes remain eligible; ordinary portal, stereo, partial and no-world views
remain excluded. A successful pass still restores depth layout and the
material pipeline's cached state.

GLx already caches each planar face's vertex bounds and checks those bounds
before testing a light's distance from its plane. Vulkan and RTX only tested
the infinite plane. A distant face in the same plane could therefore receive
the light and enter its shadow-caster list despite lying outside its radius.
Both renderers now cache the same bounds in `ParseFace` and use the existing
`R_LightCullBounds` before the plane test. This applies to receiver collection
and the shared world-caster collector. Touching bounds and linear lights
crossing them are retained conservatively. The record is renderer-internal;
no BSP format, module layout or protocol changes.

These changes preserve FnQ3/FnQL's per-pixel lights, dynamic-light shadows,
cascaded sun shadows and optional sidecar fog. No presentation feature is
removed and no retail-exact alternative is introduced. Global fog remains
off by default.

## Runtime evidence

Before uses local main `29641a09` (engine sources identical to the preceding
`6ca834d7` build). After uses that checkout with the exact source changes
subsequently committed as `d3547f6b`. The existing Windows x86
`meson/build/win32-dev` preset was rebuilt in place, MSVC `debugoptimized`,
all three renderers and client/server enabled, `strict-warnings=true`.
The JSON records both the base/patch identity and actual binary hashes;
the after captures were taken before the source commit was created.

Every launch was windowed 800x600 on an unswitched private desktop, with
input and network disabled, an isolated writable home and read-only retail
asset lookup. Captures are the engine's `screenshotPNG` output. All 20
launches exited normally, without reported engine errors or changes to
installations or tested binaries; 104 capture hashes were verified.

The before/after matrix covers GLx, Vulkan and RTX raster on RTCW SP `forest`
and RTCW MP `mp_village`, at identical recorded camera positions. Debug
logging starts after the comparison captures. These values held on all 130
debug frames per run:

| View | Renderer | Planned lights before → after | Rendered lights before → after | Caster surfaces before → after |
| --- | --- | --- | --- | --- |
| `forest` | GLx | 2/5 → 2/5 | 2 → 2 | 700 → 700 |
| `forest` | Vulkan | 2/5 → 2/5 | 2 → 2 | 763 → 700 |
| `forest` | RTX raster | 2/5 → 2/5 | 2 → 2 | 763 → 700 |
| `mp_village` | GLx | 0/16 → 0/16 | 0 → 0 | 0 → 0 |
| `mp_village` | Vulkan | 1/16 → 0/16 | 0 → 0 | 0 → 0 |
| `mp_village` | RTX raster | 1/16 → 0/16 | 0 → 0 | 0 → 0 |

Forest's sun-shadow counts remain 335 casters, 104 world receivers, 576
model receivers and 104 baked receivers in all three renderers. With fog
disabled, the before/after forest captures differ by over 12 luma on only
0.022% of the measured area for RTX and 0.058% for Vulkan; GLx is identical.
These separate runs include animation, so they are not bit-exact proofs.

The authored forest fog is `color 0.25 0.4 0.55`, `density 0.0015`,
`mode exp2`, `start 64`, `opacity 0.8`, `sky false`. The sequence toggles
strength 0 → 1 → 0. Measurements exclude the top notifications and bottom
HUD (image rows 80–535, 364,800 pixels). A changed pixel differs by more than
12 luma with fog and by at most 4 between the two fog-disabled frames;
luma is `0.299 R + 0.587 G + 0.114 B` on stored sRGB values. This reduces
motion contamination without identifying every fogged pixel.

| Forest | Fog-change area before → after | Mean luma without / with fog, after |
| --- | --- | --- |
| GLx | 53.73% → 53.75% | 16.63 / 41.12 |
| Vulkan | 17.08% → 17.16% | 16.80 / 21.12 |
| RTX raster | 0.36% → 54.58% | 16.79 / 41.27 |

Visual inspection confirms RTX now fogs the distant trees, terrain and
ground like GLx, while keeping the nearby player clear. Further RTX runs
show fog with depth fade disabled (the main-pass path), with `sky true`,
on Q3's non-portal `q3dm1`, on Jedi Academy's portal-sky `mp/duel9`, and in
forest's compute path tracer. The traced run logs a validated 800x600
primary dispatch; it is not hardware ray-tracing evidence.

Enemy Territory's `oasis` has a visible world/model smoke capture. Initial
ET and Jedi Academy probes retained their join menus and were repeated
with the modules' console commands to close them. The QL `campgrounds ca`
run retains its join menu and loads the packaged fog sidecar, which takes
priority over the test file: it establishes a map-load/shutdown smoke check,
not fog correctness. No fog conclusion is drawn from the tiny changes in
the ET or `mp_village` views. These limits are preserved in the JSON.

## Automated validation

- The strict Windows build passed without compiler warnings.
- The available Meson suite passed 275 entries, failed none and skipped
  one: OpenAL loopback is unavailable. The complete recursive Python suite
  was not rerun; the focused selection passed 428 tests and 64,418 subtests.
- `dlight_face_bounds_tests.py` compiles each renderer's actual face loader
  and point/linear-light culling. It checks bounds from just the selected
  vertices, valid index storage after the layout change, lights touching
  and just outside each bound, crossing segments and tilted planes.
- `rtx_global_fog_view_tests.py` compiles the actual compositor against a
  recording Vulkan host, for forward and reversed depth. It checks portal
  sky then main view in the same frame, repeated draws, disabled/invalid
  views and partial initialization, both eligible scene passes, depth
  transitions and restored material state.
- Existing malformed BSP, profile loader, shadow, fog and renderer tests
  pass, including the script-style shadow projection check. Final player
  manual, generated-document and changelog checks pass: 25 tests and 841
  subtests.

Logs, engine captures and reproducible launch/analysis scripts are retained
under `.tmp/portal-fog-casters/`. Disposable homes and compiler scratch are
removed after preserving launch configs and verifying capture hashes.

## Separate findings and limits

The standard Vulkan renderer still draws the authored forest fog differently
from GLx and the corrected RTX renderer, both before and after this patch.
Its depth-fade snapshot is reused across views, which is a possible cause
from source inspection; the cause is not established by these captures.
This patch does not change its fog compositor. The Quake Live smoke run also
logs three `Hunk_FreeTempMemory: not the final block` shutdown warnings;
it exits successfully, and those warnings were not investigated here.

Spot-light allocation, every map/profile, stereo/partial-view fog support
and hardware ray tracing are not established by this focused validation.
