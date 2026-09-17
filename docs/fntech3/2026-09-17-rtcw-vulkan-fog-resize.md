# RTCW Vulkan/RTX fog and menu resize

The `escape1` startup error came from an explicit guard: the RTCW controller
already supplied scene fog, but only GLx applied it to GPU draws. Vulkan and
RTX now consume that same immutable scene snapshot. This implements the
missing paths; it does not select GLx or silently disable fog.

## Implementation

- Both Vulkan raster backends upload per-pass fog color, equation parameters
  and local-to-eye rows. Generated vertex/fragment shaders apply linear or
  exponential RGB fog before the authored blend, preserving alpha/discard.
  Sky, `nofog`/stage overrides, SP snooper, clear color and fog far clipping
  follow the existing shared RTCW/GLx contract. UI draws clear stale fog.
- Mixed stage fog overrides cannot collapse into a single material pass.
  Plain projected lights remain unfogged; authored light materials retain
  their stage policy. Optional enhanced liquids/depth fade use the authored
  generic path while scene fog is active, as in GLx.
- RTX ray generation receives the view fog, material no-fog flag and primary
  distance. It applies fog after lighting without storing fog in temporal
  history, accounts for the already-fogged raster reference, retains authored
  raster sky, and clips primary rays at the eye-space far plane. This path
  has shader-build evidence, **not native ray-tracing runtime evidence**.
- Video restart already rebuilt UI/cgame with the new `glconfig`, but lost
  the open menu. It now restores RTCW's exported menu ID; Q3/QL restore the
  appropriate main/in-game root because their public UI API cannot expose
  an arbitrary submenu stack. Paused in-game menus reopen only after the
  first active snapshot, preventing a `CA_PRIMED` local-server stall.
  Disconnect/error recovery resets resize guards and pending restoration.

This reuses project-owned fog contracts; no external implementation text was
incorporated. Reference behavior and earlier attribution remain documented in
the [fog controller](2026-09-15-rtcw-fog-controller.md) and
[GL fog/startup checkpoint](2026-09-16-rtcw-coronas.md).

## Evidence and limits

Windows x86 development build, working tree based on
`0b6af9ba7a17d73bf27c5c016c00772a1a93899a`, NVIDIA RTX 4060 Laptop GPU,
driver 591.74. Renderer SPIR-V was regenerated through the existing generator
(84 Vulkan variants and 89 RTX variants). All three renderer/client targets
build. Compiled fixtures exercise shared equations, actual GLSL visibility,
UBO layout/transfer, sky/material/SP/MP/UI choices, clean Q3/QL snapshots,
restart ordering, paused activation, and error recovery.

Validation results:

- Development `RelWithDebInfo` and ordinary `Release` Windows x86 client builds
  pass for all three renderer modules using `.vscode/build-release.ps1` and
  the existing `meson/build/win32-dev` / `meson/build/win32` outputs.
- The 57-entry native Meson selection has 56 passes, one audio-loopback skip,
  zero failures (`native-tests.log`, `native-test-detail.log`).
- Full Python discovery runs 1,488 tests in 480.059 seconds, with 11 skips and
  one failure (`full-python-final.log`). That failure loaded the old Windows
  icon dependency assertion before a concurrent task changed Meson to
  `assets/icons/fntech3.ico`; the current assertion was already updated by
  that task. No icon/build-system edits belong to this fog fix.
- The final 79-test focused rerun includes that current resource assertion,
  fog scene/GPU/light fixtures, real shared GLSL math, menu resize and the
  updated input assertions: all pass (`final-focused.log`). The complete
  discovery run was not repeated after the concurrent icon edit.

Runtime evidence is retained under `.tmp/rtcw-fog-resize/`. Each probe's
`manifest.json` records executable/renderer SHA-256, exact launch command,
states, exit status and installation-integrity comparison. `commands.cfg`
records scheduled engine commands; `console.log` and PNGs retain observations.
The writable test homes were removed. Retail inputs were not staged or changed.

| Probe | Observation |
| --- | --- |
| `vk-boot/` | Uninstrumented Vulkan client starts installed SP and MP maps and captures native briefing/limbo UI. |
| `rtcw-sp-vk-map-verified/` | `escape1` briefing survives 640x480 to 960x540 restart, then reaches the opening 3D scene; controlled fog and fog-disabled captures differ as expected, without the old error. |
| `rtcw-sp-rtx-map-verified/` | Same successful scene/menu/fog check with RTX explicitly in raster mode 0. |
| `rtcw-sp-vk-map-os-resize/` | `SDL_SetWindowSize` generates real platform resize events; scheduler reloads at 960x540 and restores paused briefing, then Ready reaches active gameplay. |
| `q3-vk-ui-verified/` | Main-menu state persists and inspected engine capture shows the correctly resized menu. |
| `ql-vk-ui-verified/` | Native UI reloads/restores main-menu state and dimensions; capture shows the retail splash with WebUI disabled. This does not establish usable browser/main-menu interaction. |
| `rtcw-sp-rtx-ui-native-capability/` | Explicit required ray mode rejects missing `VK_KHR_acceleration_structure`, `VK_KHR_deferred_host_operations` and `VK_KHR_ray_tracing_pipeline` capabilities. Fatal dialog stayed on the private desktop until the 60-second probe timeout terminated the process; no capture or runtime RT pass. |

The map/resize probes use an independently linked diagnostic client with
temporary console commands, recorded in `build_probe.py` and
`probe-instrumentation*.json`. Production source contains no probe commands.
They run windowed on an unswitched private desktop, mouse/joystick disabled,
isolated writable home, sound and online services disabled. Each SP process
delivers only the standing-authorized briefing Ready click to the native UI.
No operating-system input or screen capture is used; all PNGs come from
`screenshotPNG` on the engine render target. These short opening-scene tests
do not establish campaign, combat, audio or visual parity across every map.

The x86 driver's advertised extensions omit the required native ray-tracing
extensions. Shader compilation and RTX raster success must not be described
as a native RT pass. Outdoor/underwater/sky transitions, original-engine
image comparisons, sustained play, HDR and broader native RT validation remain
open. Q3/QL arbitrary submenu preservation and WebUI interaction are outside
the demonstrated menu evidence.

Unrelated findings: five source assertions still expected the previous menu
pointer policy; their expectations were updated to the existing profile-aware
input implementation. The concurrent icon assertion is recorded above.
Retail QL logs also retain unavailable optional service
messages with services disabled. RTCW logs retain existing asset fallback and
map warnings; this change does not claim those compatibility gates closed.
