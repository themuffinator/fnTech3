# Elite Force renderer selection audit and verified reruns

The September 20 live probes used `r_renderer`. fnTech3 selects its renderer
with `cl_renderer`, so the runs labelled Vulkan and RTX actually loaded GLx.
This invalidates their claimed live coverage of those backends and their
cross-backend image comparisons. The original command lines, binary identities
and measurements are retained with explicit corrections in each record.
Compiled fixtures and native mathematical comparisons are unaffected.

The corrected probes use `+set cl_renderer glx|vk|rtx`. Acceptance requires
backend evidence in the engine log: `GL_RENDERER` for GLx, `VK_RENDERER` for
Vulkan, and the RTX capability report for RTX. A completed process or a
requested cvar alone is insufficient because a failed renderer load can fall
back to the default. Every run below has its actual backend verified; no
renderer-load fallback is accepted. RTX runs use its raster path, with ray
tracing disabled.

## Windows x86

Twenty-one windowed runs use the same production client and renderer DLLs,
except for a private scissor probe linked with one instrumented screen object.
The production source and executable are unchanged by that probe. The
[manifest](evidence/2026-09-21-stvef-renderer-verification.json) records their
SHA-256 identities, commands, backend log proof and 107 engine captures.

| Probe | Verified backends | Observation |
| --- | --- | --- |
| Scissor clipping and reset | GLx, Vulkan, RTX | Identical 640 by 480 pixels: 282,800 blue, 24,000 green and 400 red; the clipped yellow draw contributes none |
| Saved tutorial scene and shadows | Vulkan, RTX | Blob, stencil and projection modes render and return normally; comparison with retail and the previous verified GLx image is recorded below |
| Holomatch effects | Vulkan, RTX | The retail demo completes and three opening frames are captured in each backend |
| Movie fades and crop | Vulkan, RTX | 32 transition captures and three quality captures per backend; separate held-frame checks avoid relying on an assumed frame count |
| Held movie frames | Vulkan, RTX | Each quality mode repeats the same final image; modes 0 and 1 are identical within each backend |
| Movie lifecycle | Vulkan, RTX | Standalone playback invokes its continuation; in-game playback reports `cl_paused 1`, then `cl_paused 0` and returns to the connected level |
| Movie audio, English and German | Vulkan, RTX | Both selected tracks complete 300 video frames and reach Bink's sound output |
| Sound-buffer failure | Vulkan, RTX | Audio is reported unavailable while all 75 video frames still complete |

The first quality captures waited 360 engine frames. Their differing modes
0 and 1 did not establish a held final image. The repeated checks instead wait
1,200 frames, then capture again 60 frames later. Every repeated image is
identical. Vulkan's quality-0 image differs from the prior GLx image at only
85 pixels, by at most one channel value; its quality-2 image is identical.
RTX's quality-0 and quality-2 mean absolute channel differences from GLx are
0.308617 and 0.725246 on the 8-bit scale, with a maximum of 71. These are
observations, not an accepted image-parity tolerance or a diagnosed cause.

The wider shadow scene also exposes differences hidden by the old backend
selection mistake. Isolating each shadow against that backend's shadows-off
image gives these mean absolute channel differences from retail:

| Backend | Blob | Stencil | Projection |
| --- | ---: | ---: | ---: |
| GLx, previous correctly identified run | 0.006172 | 0.009352 | 0.018451 |
| Vulkan | 0.010299 | 0.011904 | 0.018519 |
| RTX raster | 0.013738 | 0.016866 | 0.180566 |

Full images are not identical across backends. In particular RTX's unshadowed
scene differs from retail by a mean of 29.770335 channel values. The manifest
also preserves mask disagreements, maximum differences and full-image
comparisons. Complete image parity remains open.

For movie audio, each RTX capture is byte-identical to its corresponding
3,528,000-byte native-file PCM reference. Vulkan's English capture contains
252 additional zero bytes (1.429 ms); German contains 1,788 (10.136 ms).
Removing only those observed silent insertions makes all other bytes equal.
The observer records only the test movie's own output, not other applications.
This establishes track/content delivery, not exact real-time pacing or
subjective listening parity. The authentic installed Bink DLL is restored
and its identity verified after the observer probes.

## SDR control

Two subsequent RTX runs isolate its colour pipeline. RTX defaults to `r_hdr 1`,
which uses linear-light texture filtering and lighting before final sRGB output;
the earlier GLx/Vulkan comparisons used their legacy colour path. Selecting
RTX's existing `r_hdr 0` setting gives `display-referred-sdr` in both new logs.
No production code or defaults change.

With only that setting changed, all three held-movie quality modes match the
verified Vulkan images byte-for-byte, including the repeated final frames.
The complete shadows-off, blob, stencil and projection images differ from
Vulkan by at most one channel value. Their mean differences range from
0.001212 to 0.008261. The unshadowed scene's mean difference from retail falls
from 29.770335 to 0.606185, comparable to GLx and Vulkan. Ten further engine
captures and the unchanged executable identities are recorded under
`sdrControls` in the manifest.

These controls attribute the large mismatch in these scenes to comparing the
enhanced linear-light pipeline with legacy display-referred rendering. They
do not establish general image parity. Future retail comparisons must record
the active colour pipeline as well as the loaded renderer.

## Linux i686

The Linux i686 client and all three renderer modules build with GCC 13.3.
Correctly selected GLx, Vulkan and RTX each play the Linux-hosted protocol-24
recording to completion: 107 frames in each run, with engine screenshots.
These runs use Mesa llvmpipe on a private Xvfb display. They establish software
raster rendering on Linux, not hardware ray tracing or Linux audio support.
The separate [Linux server record](2026-09-21-stvef-linux-server.md) covers
live server interoperability. These client runs add demo and renderer evidence;
Linux client networking and audio remain untested in this record.

All probes disable mouse/joystick input and use the registered engine
`screenshot` command. Windows test homes and independent asset copies remain
under `E:\Games\fnTech3-tests`; Linux uses a private virtual display. Logs,
scripts and comparisons remain under `.tmp/stvef-gaps/renderer-*` and
`.tmp/stvef-gaps/linux-client-*`. No proprietary media is committed. Broader
scenes, devices, listening coverage and hardware ray tracing remain open, and
the Elite Force development gate remains enabled.
