# FOV and HUD aspect restoration — 2026-09-17

This checkpoint restores the FnQuake3 HUD rule/dump implementation for Q3 and
both experimental RTCW profiles. The shared renderer policy now also corrects
RTCW's world FOV. Quake Live bypasses both transforms, rule loading, shader
tracking and dump collection even when old configs explicitly enable them.

## Implementation and provenance

The implementation in `code/client/cl_hud.cpp` is adapted from FnQuake3's
committed `7d8c9629cdc7979f09697266470359101ffd9eda` source. That file matches
the pinned baseline in the local reference; unrelated dirty reference changes
were not imported. Its GPL-2.0-or-later lineage is compatible with fnTech3.
Credits and notices distinguish this implementation import from independently
authored RTCW presets and reference-only observations.

The port restores uniform/stretch placement, horizontal/vertical anchors,
shader wildcards, text-like and region matching, 3D HUD viewports, `hud_reload`,
and grouped `cl_hudDump` output. Rule parsing validates the complete JSON with
bounded size, nesting and rule counts. Cgame restart clears rules and shader
handles. Small icon and number atlases retain the picture classification used
by Q3's shipped rules, independent of framebuffer size. RTCW gradients and MP
rotated pictures use corrected rectangles while
retaining the original color and UV contracts. Module records remain unchanged.

All three renderers share the FOV policy. `r_fovCorrection 1` preserves the
vertical framing of a 4:3-authored horizontal FOV; `cl_hudAspect 1` preserves
HUD proportions. Both default on for Q3/RTCW and are inert in QL. UI and 3D HUD
scenes retain their authored projection. The [aspect guide](../ASPECT_CORRECTION.md)
documents the controls and rule schema.

The root archive exposes only the two exact HUD rule filenames for Q3/RTCW,
using the active profile/mod namespace. Q3's legacy preset is restored to use.
New `fntech3-hud.json` files in the SP/MP `main` namespaces describe their
different stock layouts. The SP preset has 10 rules and MP has 19; the required
package inventory is 441 assets. The original retail `ui/hud.menu` and
`ui_mp/hud.menu`, cgame coordinate/shader contracts, and engine-produced draw
metadata supplied layout facts. No retail menu text or GPLv3 implementation
text was incorporated.

## Executable and runtime evidence

Windows x86 release client and GLx, Vulkan, and RTX raster renderers built
successfully with the existing `meson/build/win32-dev` preset and
`-Drtcw-development=true`. The worktree was dirty at base commit
`0b6af9ba7a17d73bf27c5c016c00772a1a93899a`; concurrent work means the individual
probe manifests' executable and renderer hashes identify the tested snapshots.
The [runtime record](2026-09-17-aspect-correction-runtime.json) retains exact
commands, binary/package/source hashes, installation fixture identities and
capture hashes for each selected probe.

The final icon-classification follow-up compiled successfully; relinking the
shared build output encountered `LNK1104` because that executable was running
outside this probe. Its process was left alone. The same production objects
linked successfully into `.tmp/aspect-restore/runtime-bin/fntech3.exe`, and
that uninstrumented binary completed the final Q3 Vulkan probe. The shared
output will need relinking once its current process exits.

All probes used isolated writable homes, `+set r_fullscreen 0`, disabled input
devices, and a private desktop that was never switched into view. Captures
come from the engine's `screenshotPNG` command. Installation inventories were
unchanged, and writable homes were removed after collecting evidence.

| Profile / scene | Renderer | Observation |
| --- | --- | --- |
| Q3 `q3dm1` | GLx, Vulkan, RTX raster | On/off/on captures; Q3's 13-rule preset loaded; proportional HUD and changed world framing. |
| QL `campgrounds ffa` | Vulkan | World/join-menu captures; no engine HUD dump despite enabled legacy cvars. Executable tests establish exact transform bypass. |
| RTCW SP `escape2` | Vulkan | On/off/on gameplay captures; 10-rule SP preset loaded; health/armor/stamina anchored left and equipped weapon/ammo anchored right. Inventory supplied through console commands. |
| RTCW MP `mp_beach` | Vulkan | Console commands join red and close limbo; on/off/on gameplay captures at 960×540 and 1280×540; 19-rule MP preset loaded; health/stamina left, ammo/charge right, round timer/objectives upper left, compass centered. |

RTCW frame scheduling and the standing-authorized SP briefing Ready click use
a disposable copy of `cl_main.cpp` linked into a separate probe executable.
The production source and object are not instrumented. MP uses module console
commands, with no simulated mouse or keyboard events. Initial SP probes that
blocked the command queue with long `wait` sequences produced black captures
even with correction off; frame scheduling resolved that harness artifact.
Those captures are not treated as engine rendering failures or positive evidence.

Local logs, manifests, engine images and HUD dumps are retained under
`.tmp/aspect-restore/`. Runtime observations establish these stock scenes only.
They do not establish every mod layout, optical overlay, scoreboard state,
resolution-change sequence, SP campaign/savegame, or live MP interoperability.
RTCW remains experimental and all broader compatibility gates remain open.

## Automated coverage

The HUD executable fixture exercises the real module with all four profiles,
4:3, widescreen, ultrawide and portrait geometry, fullscreen fades, mirrored
draws, shader/region/first-match rules, authored 3D viewport FOV, malformed and
oversized JSON, canonical/legacy fallback, restarts and dump reuse. It also
loads both shipped RTCW presets and checks stock status/ammo/compass/objective
coordinates, bar/frame agreement and centered message/crosshair behavior.

The FOV fixture executes all profiles and unknown-profile rejection in C11,
FBO and Vulkan configurations. RTCW render-import fixtures cover cgame/UI
isolation, transformed gradients and rotation, unchanged UVs/colors and
external records. Root-archive fixtures cover profile-specific HUD allowlists
and reject unrelated JSON paths. Build source lists include the restored HUD
module in Meson, Make and CMake's existing client discovery.

The complete Meson run finished with 111 passing targets, two skips, and one
failing aggregate target. That aggregate ran 1,514 Python tests with 62 skips;
all failures/errors came from release packaging reading the evidence link
while this document was still being written. After the document and runtime
record were complete, the entire 48-test packaging file passed (one skip).
The final HUD executable test also passed with all three shipped presets.
The earlier 600-second aggregate timeout was resolved by using Meson's
`--timeout-multiplier 2`; the complete aggregate took about 545 seconds.

An unrelated transient Vulkan bloom parity hash mismatch appeared during
concurrent renderer work. Its eight-test file passed on rerun and the later
Meson target passed. No unresolved unrelated source issue was found here.

Automatic approval review rejected recursive cleanup of the temporary probe
folders, returning only `blocked by policy`, including after their resolved
paths and absence of links had been checked. Disposable probe executables,
helpers and build files therefore remain with the useful evidence under
`.tmp/aspect-restore/`; successful probe homes were removed by the harness.
