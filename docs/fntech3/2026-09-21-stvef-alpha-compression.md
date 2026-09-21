# Elite Force alpha-texture compression

GLx now selects DXT5 for Elite Force alpha textures when S3TC compression
is enabled and the image permits compression. The preceding
[control-registration correction](2026-09-21-stvef-texture-compression.md)
made the menu setting effective but still left those images uncompressed.
The retained OpenGL paths in Vulkan/RTX receive the same format correction;
their shipping Vulkan upload paths remain uncompressed.

The [evidence record](evidence/2026-09-21-stvef-alpha-compression.json)
pins native observations, sources, binaries, test results, runtime commands,
image measurements and retained local files. Implementation is independent;
only numeric observations and owned regression code are committed.

## Native contract and correction

An owned Windows x86 observer maps each supplied retail 1.20 executable
without starting it. It executes only the texture-format selection block,
using owned cvar storage and a return before any upload, resampling or
graphics call. SP's block is `0x44d7c2`–`0x44d8ad`; Holomatch's is
`0x460ba7`–`0x460c92`. The observer provides either three or four components,
with the lightmap argument off; it does not execute native image loading or
alpha scanning.

Both engines return identical results for 36 cases: three compression
capabilities, six texture-bit settings (`-1`, `0`, `8`, `16`, `24`, `32`)
and opaque/alpha input. They choose DXT5 for alpha images with the ARB/S3TC
capability, regardless of texture-bit setting. The older S3 extension alone
retains uncompressed alpha. Native unsized requests use component counts
three/four; comparisons normalize them to fnTech3's RGB/RGBA symbols.

The renderer correction follows that profile rule after alpha scanning.
Other profiles, compression opt-outs, lightmaps and GLx's explicit sRGB
decode path retain their previous choices. `imagelist` now labels DXT5 and
estimates its base-level storage in 16-byte blocks, including partial edge
blocks. It retains the existing approximate mip-overhead calculation.

The regression compiles actual alpha scanning and format-selection functions
from all three renderer sources. It checks 16,128 selections per platform,
covering all seven profiles, the native vectors, zero/254 alpha at different
pixel positions, null allocation data, compression opt-outs, lightmaps and
sRGB controls. All three old implementations fail on the first applicable
Elite Force alpha case. Windows x86 and Linux i686 pass; Linux uses address
and undefined-behavior sanitizers. All three renderer modules rebuild on both
platforms. The full Windows retry passes 120 targets with two skips and no
failures, including 1,856 Python checks with 72 skips in 703.592 seconds.
An initial run caught a link to this evidence JSON before it had been
written; the completed record passes all 49 packaging checks (one platform
skip) and the full retry. The initial failure remains in the evidence ledger.
The wider Linux suite is not rerun; its existing failures remain open.

## Runtime evidence and limits

Windowed SP tutorial captures show 145 DXT5 images in GLx, with the dynamic
`*fontstash` atlas remaining RGBA through its compression opt-out. The native
capture has 146 DXT5 images, but image inventories differ between engines;
this is not a claim of identical image allocation. SP and Holomatch default
→ off → on → off video-restart cycles switch alpha formats reliably. Windows
Holomatch and Linux Mesa each report 179 DXT5 images when enabled and none
when disabled, with their font atlases also remaining uncompressed.

The SP comparison retains the previous capture phase, filter, asset set and
compression setting. Against the same native screenshots:

| Sample | Previous GLx mean / maximum | DXT5 GLx mean / maximum |
| --- | --- | --- |
| 20 | 0.608110 / 97 | 0.304863 / 63 |
| 80 | 0.163173 / 114 | 0.163132 / 125 |
| 160 | 0.163173 / 114 | 0.163132 / 125 |

The first sample improves, while the static corridor changes little and its
localized maximum increases. These measurements do not establish pixel
identity or broader renderer parity. Three compression-disabled GLx images
and all six Vulkan/RTX control images are pixel-identical before and after
this correction. Vulkan/RTX keep their existing uncompressed capability.

All nine new runtime sessions exit normally and produce 26 verified engine
screenshots. The staged engine under `E:/Games/fnTech3-tests/stvef-manual`
now uses source commit `be1e595`; eight existing settings/save/launcher
files retain their hashes. Fresh input-disabled SP save-load and Holomatch
bot-match preflights pass with that staged engine. User launchers are not run.

Runtime homes are independent under `E:/Games/fnTech3-tests`. Clients are
windowed, with mouse/joystick/feedback disabled, on inactive private Windows
desktops or private Linux Xvfb. Only engine screenshot commands produce the
images. No user input, original installation or proprietary content is
modified or committed. Useful local logs, probes and captures are retained.

Native lightmap compression and compression-method selection still need
their own implementation/validation. Localized image differences, broader
campaign behavior, physical input, menu use, listening and feedback hardware
remain open. Existing asset defects, wider Linux-suite failures and the
Windows x86 ray-tracing limitation are unchanged. The Linux build repeats
existing signedness and unused-helper warnings.
