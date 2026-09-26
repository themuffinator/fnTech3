# Elite Force lightmap compression and precision controls

Both Elite Force profiles now honor `r_ext_compress_lightmaps` and
`r_texturebitslm`. GLx requests the native compressed or RGB lightmap format;
Vulkan/RTX honor precision through their existing uncompressed upload path.
Previously all lightmaps were forced to the renderer's ordinary uncompressed
format, and neither retail control was registered.

Production commit `8435103` follows the
[compression-method correction](2026-09-21-stvef-compression-method.md).
The [evidence record](2026-09-21-stvef-lightmap-controls.json)
pins native observations, exact sources and binaries, regression results,
runtime commands and retained files. Implementation is independent; no
proprietary executable bytes, modules or assets are committed.

## Native contract and correction

An owned observer executes only the RGB lightmap format-selection block in
the supplied retail 1.20 executables, with owned cvars and a return before
upload. It does not start the game, driver or compression encoder. SP and
Holomatch agree on all 324 cases each: three compression capabilities, six
texture-bit values, six lightmap-bit values (`-1`, `0`, `8`, `16`, `24`, `32`)
and three lightmap-compression values (`-1`, `0`, `1`).

Both lightmap controls register with default `0` and archive/latch flags
`0x21`. The precision control is named `r_texturebitslm`; a preliminary string
search for `r_lightmapbits` found no native registration. Pointer references
identify the correct SP registration at `0x440644` and HM at `0x45386e`.
Compression registers at SP `0x4404e0` and HM `0x453740`.

When lightmap compression is requested and the selected method supports it,
the native engines request DXT1 or legacy RGB4 S3TC before considering
precision. Otherwise a positive `r_texturebitslm` overrides `r_texturebits`;
zero or negative values inherit it. Effective precision 16 requests RGB5,
32 requests RGB8, and other values request unsized RGB. A positive unsupported
value such as 8 does not fall back to `r_texturebits`.

The GL paths implement this rule while retaining explicit image compression
opt-outs. Only the two EF BSP lightmap loaders remove their default opt-out
when compression is requested. Other profiles and ET's external lightmap
loader retain their previous flags and format rules.

Merged atlases keep the existing 128-pixel lightmaps plus two duplicated edge
texels on each side. Thus each tile and upload step is 132 pixels, with
four-aligned offsets and power-of-two atlas dimensions. These satisfy the
[ARB/S3TC subimage alignment contract](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt).
The duplicated edges are ordinary image texels; the OpenGL API border remains
zero. The [legacy S3 registry entry](https://registry.khronos.org/OpenGL/extensions/S3/S3_s3tc.txt)
lacks a detailed subimage contract, so its atlas behavior is established only
for the NVIDIA and Mesa implementations exercised here.

Vulkan/RTX keep their uncompressed capability. EF lightmaps with effective
precision 16 use the existing opaque A1R5G5B5 upload; other precision values
use RGBA8. Allocation-only atlas uploads remain opaque without treating their
zero-filled alpha as transparency. Other profiles, ordinary textures and explicit
sRGB uploads retain their previous decisions.

## Validation

The compiled tests exercise actual renderer functions and the flags
expressions used by both BSP allocation paths across all seven profiles:
54,432 GL format/flag selections and 6,048 actual Vulkan upload selections
per platform. Cases include compression opt-outs, null allocation data,
alpha-bearing input, invalid precision values and explicit sRGB controls.
Another 357 real cvar-system cases cover registration, config values,
latching, reset and isolation from other profiles. The old code fails in all
three GL implementations and both Vulkan upload implementations.

Windows x86 and Linux i686 pass the six focused regressions, including the
prior alpha-format, compression-method and mip-upload checks. Linux uses
address and undefined-behavior sanitizers. All three modules build on both
platforms. The full Windows suite passes 120 targets, with two skips and no
failures, including 1,859 Python checks with 72 skips in 668.119 seconds.
The wider Linux suite is not rerun; its previously recorded failures remain
open.

Six Windows and three Linux sessions complete the restart cycles. The
fnTech3 sessions each cover ten phases: defaults, explicit 16/32-bit
lightmaps, inherited 16-bit precision, unsupported positive precision,
compressed ARB and legacy S3 atlases, separate compressed lightmaps,
separate 16-bit lightmaps and restored defaults. Retail runs cover the eight
applicable phases because they do not use fnTech3's merged atlas setting.

The SP tutorial has 11 lightmaps; Holomatch's `hm_borg1` has seven. GLx
switches its merged texture between RGB, DXT1 and S3TC, and between one atlas
and the original-sized 128-by-128 textures. Windows and Linux Vulkan/RTX
switch precision while staying uncompressed. GL error checking is enabled
with `r_ignoreGLErrors 0`; all sessions exit normally without GL or Vulkan
error reports. Restored lightmap dimensions, names and format reports match
the initial phase.

Native Holomatch selects Intel Iris Xe and reports no supported compression
method on this host. Its compression-requested cases therefore retain the
requested RGB5 precision. Native SP and fnTech3 Windows GLx use NVIDIA.
These differences are recorded as device capabilities. Camera and animation
phases also differ between captures; this is not synchronized image-parity
evidence.

Three initial Linux trials started and exited normally but an incorrectly
assembled helper omitted the phase loop. Its capture assertion failed. Those
startup-only observations are retained separately and excluded from the nine
completed cycles; the corrected probes use fresh writable homes.

The refreshed engine under `E:/Games/fnTech3-tests/stvef-manual` uses source
`8435103`. All six staged artifacts match the tested build, and eight existing
settings/save/launcher files retain their hashes. Two fresh, input-disabled
preflights load the known SP save and a Holomatch bot match successfully.
Together the eleven completed validation sessions produce 88 engine
screenshots; their hashes are verified in the record. User launchers are not
executed.

Clients run windowed with mouse, joystick and force feedback disabled on
inactive private Windows desktops or private Linux Xvfb. Only engine
screenshot commands create the images. Writable homes are independent under
`E:/Games/fnTech3-tests`; the original installation is unchanged and no user
input is controlled. Useful logs, helpers and captures remain in the
designated project and test directories.

## Remaining limits and unrelated findings

GLx's existing `imagelist` groups RGB5/RGB8/unsized RGB under the same label
and estimates three bytes per texel. The compiled regression proves the
exact requested format; that existing diagnostic is not a physical GPU
storage measurement. No Vulkan S3TC encoder is added.

Broader renderer parity, campaign play, physical input, menu save/load and
rebinding, listening and feedback hardware remain open. Both EF profiles
remain development-gated. Existing asset defects, wider Linux-suite failures
and the Windows x86 ray-tracing limitation are unchanged. The wider rebuild
also repeats existing RTCW/ET signedness, qualifier and unused-helper
warnings, plus an ET torso-pose possible-uninitialized warning; none is
introduced by these lightmap changes.
