# Elite Force save round trip and Linux ray tracing

Two runtime limits in the earlier records now have additional evidence: retail
can re-save the current fnTech3 SP save after a longer startup wait, and the
Linux i686 client can render Holomatch through the ray-tracing pipeline on
the tested software Vulkan driver. These observations do not change either
profile's development gate.

## Retail SP re-save

The [clock record](2026-09-21-stvef-sp-clock.md) preserved retail's early
"while dead" refusal both after loading fnTech3's tutorial save and on a
fresh retail level. A new control attempts an early save after `wait 180`,
then waits another 420 command-buffer iterations. The first attempt is again
refused, but the later one succeeds at `TIME=8300`. The game is not patched
and no input is injected.

With `wait 600` after loading, the full round trip succeeds:

| Step | Engine | Saved level | `TIME` |
| --- | --- | --- | ---: |
| Original full save from the clock test | fnTech3 | `tutorial` | 5100 |
| Load that save and write a new one | Retail SP 1.20 | `tutorial` | 12350 |
| Load retail's new save and save again | fnTech3 | `tutorial` | 12700 |

All three new processes exit normally. The generated saves parse to their
last framed chunk; the final fnTech3 screenshot shows the loaded tutorial
view. This closes the earlier unattended re-save limitation for this fixture.
It is not a manual menu test or a campaign-wide save compatibility claim.

Native inspection also clarifies the guard. Retail's save command at
`0x426b3e` reads client zero's frame buffer: the counter at client offset
`0x50ac`, masked by 31, selects a 600-byte frame, with health at relative
offset `0x664`. The counter belongs to the netchannel at `0x5090`; its
transmit routine at `0x418370` uses and increments offset `0x1c` as the
outgoing sequence. The snapshot builder at `0x428330` selects the same ring.
The early refusal is consistent with the selected buffered health still
being empty or stale, rather than the current player's visible health.
That explanation is an inference from the code and the timing control; the
runtime probe did not instrument retail's frame-buffer contents.

fnTech3 currently checks live server-player health in
`SV_STVEFSPPlayerHealth`. It therefore does not reproduce this native
frame-buffer timing quirk. No health-gate behavior is changed by this
follow-up, and identical early-save refusal timing is not claimed.

## Linux ray-tracing pipeline

The rebuilt Linux i686 client plays the existing 107-frame recorded
Holomatch fixture on `hm_borg1` with `cl_renderer rtx`, `rtx_rt_mode 2`
and `rtx_rt_require 1`. The llvmpipe software Vulkan driver reports the
required acceleration-structure, deferred-host-operation, ray-query and
ray-tracing-pipeline capabilities. Startup reports requested and active
mode 2 with strict requirements enabled.

The runtime log then confirms `vkCmdTraceRaysKHR` output copied to scene
color with `shading=native_rt`; raster output provides depth fallback and
overlays. The engine writes a 320 by 240 screenshot, finishes all 107 frames
and exits normally. The image shows the Borg interior, view weapon and HUD.
The software run is functional evidence, not a hardware-performance result
or a retail-lighting parity claim.

This adds a successful Linux Holomatch RT subset to the
[Windows capability record](2026-09-21-stvef-runtime-coverage.md). It does
not remove the tested NVIDIA Windows x86 driver's missing extensions, prove
SP ray tracing, or cover every model, effect, shadow and reflection path.
The retail SP module remains a Windows x86 boundary.

## Reproduction and limits

The [manifest](2026-09-21-stvef-runtime-followup.json) records
source/binary identities, exact commands, saves, logs, captures and native
instruction addresses. Homes are independent copies under
`E:/Games/fnTech3-tests`; Windows uses a private desktop that is never
activated and Linux uses Xvfb. Both force windowed rendering and disable
mouse and joystick input. Screenshots come only from the engine command.
No proprietary files are committed or altered in the installation.

Physical input and listening results are still pending. Force-feedback had
no device backend in these runs; the later [feedback implementation](2026-09-21-stvef-force-feedback.md)
adds an opt-in installed-runtime bridge with physical acceptance still open.
Broader campaign, effect-image and platform acceptance remain open. No new unrelated issue was found; the
[earlier runtime record](2026-09-21-stvef-runtime-coverage.md) retains the
previously reported asset, Linux-suite and cleanup limitations.
