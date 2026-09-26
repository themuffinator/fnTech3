# Rectangular texture mipmaps found during Elite Force validation

Rectangular textures now retain their complete mip chain in Vulkan and RTX.
All three renderers also reduce the final one-dimensional levels correctly
with the weighted filter and preserve a minimum one-texel extent when fitting
an image to the device limit. These are shared renderer corrections affecting
every profile, independently found during Elite Force image investigation.
They do not explain the retracted corridor difference in the
[SP cadence record](2026-09-21-stvef-sp-cadence.md).

## Correction and executable coverage

The Vulkan/RTX upload loop formerly stopped when either axis reached one
texel. A `1 x 16` image therefore had only its base level, and a `4 x 32`
image stopped at `1 x 8`. It now continues until both axes reach one.
The existing upload allocation accommodates the complete chain, including
its longest one-dimensional case.

The weighted `R_MipMap2` filter previously computed a zero output extent on
the short axis. Keeping that extent at one allows the existing wrapped
kernel to reduce the remaining axis, with its original normalization.
The device-size loop likewise keeps each axis at least one while reducing
an over-limit image. Square-image filtering and the simple filter are
unchanged. No external implementation text was incorporated.

`tests/image_mipmap_tests.py` compiles the production resampler, both mip
filters and each backend's upload preparation. It checks exact RGBA bytes,
dimensions, level counts and total byte counts against an independently
written scalar filter. Final OpenGL upload calls are recorded; the Vulkan
upload buffer is inspected directly. Guard regions and allocation order
checks cover the temporary buffers.

Each renderer runs 1,121 cases: all seven profile IDs, twelve image shapes
from `1 x 1` through `64 x 128`, mipmapping enabled and disabled, simple and
weighted filtering, picmip levels zero through two, and thin images larger
than a simulated device limit. The latter include zero-initialized and
nonzero solid input. There are 3,363 cases per platform. The old code fails
in all three renderers; the correction passes on Windows x86 and Linux
i686, with AddressSanitizer and UndefinedBehaviorSanitizer on Linux.
This test covers valid upload dimensions; it does not replace asset-loader
malformed-input checks or test gamma/color-shift callbacks.

Windows x86 and Linux i686 client builds complete with all three renderer
modules. The full Windows suite passes 120 targets with two skips and no
failures. Its Python aggregate runs 1,853 tests with 72 skips in 836.390
seconds. Individual results and the precise executable, module, source
and log hashes are in the
[evidence manifest](2026-09-21-stvef-image-mips.json).
The wider Linux suite was not rerun; its previously recorded failures remain
open.

## GPU observations

Six production sessions compare the previous and corrected GLx, Vulkan and
RTX modules. Each isolated SP `tutorial` home supplies an independently
created `1 x 256` RGBA texture with alternating four-row red/blue stripes
as a loose `textures/cargo/crate1.tga` override. The map geometry identifies
that material on the walls visible from the scripted corridor viewpoint;
the engine image list confirms the loaded name and dimensions. Installed
retail files are not edited.

All sessions use 50 ms fixed steps, picmip zero, the simple mip filter,
trilinear filtering, SDR output, and the same three screenshot points.
At corridor sample 80, full-frame RGB comparisons give:

| Comparison | Mean absolute channel difference, out of 255 | Largest channel difference |
| --- | ---: | ---: |
| Old Vulkan or RTX against GLx | 11.824090 | 124 |
| Corrected Vulkan or RTX against GLx | 0.040293 | 3 |
| Old against corrected GLx | 0 | 0 |

This establishes a visible improvement for the controlled rectangular
texture. It is not a retail-image parity claim. All captures, including
the earlier animated sample and the later corridor sample, remain in the
manifest. Earlier exploratory overrides loaded successfully but were not
on the visible walls; they are retained as inconclusive controls, not GPU
proof of the correction.

Six further Windows sessions run the unmodified supplied assets on
`tutorial` and `voy1`, once per backend. A Linux i686 GLx session starts a
local Holomatch server on `hm_borg1` and writes an engine screenshot using
private Mesa/Xvfb. These runs complete normally. They establish bounded
production startup/render coverage, not a campaign playthrough or exact
dynamic-frame parity.

The stock Windows rerun initially reused six earlier cadence probe homes.
Its new evidence was copied to separate mip-test folders; the manifest
preserves the command lines actually executed. The previous cadence hashes
remain historical observations: six logs and eight of eighteen captures
changed, while ten captures still match their recorded hashes. This does not affect the cadence
fixture, the saved numerical comparisons, native controls or the new
before/after mip fixture, whose homes are separate.

The manual test engine under `E:/Games/fnTech3-tests/stvef-manual` now uses
source commit `b1e7b94` and the tested modules. Its settings, saves and
launchers are preserved, with eight retained files checked by hash. Fresh
input-disabled preflight homes exercise SP loading the known tutorial save
and Holomatch starting a local match with Munro. Both complete and write
an engine screenshot; the user's manual launchers are not run.

All probes are windowed with mouse, joystick and SP feedback disabled.
Windows uses an inactive private desktop, Linux a private virtual display,
and all images come from the engine's registered `screenshot` command.
No proprietary inputs, executable bytes or game screenshots are committed.

The shared mip defect is broader than Elite Force. Existing missing/corrupt
asset diagnostics, wider Linux-suite failures and the Windows x86 RT
extension limit remain separately recorded. Physical input, menu save/load,
listening, feedback hardware and campaign-wide acceptance remain open;
neither Elite Force profile is promoted out of its development gate.
