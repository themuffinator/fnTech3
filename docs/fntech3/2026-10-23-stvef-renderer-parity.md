# Elite Force views on Vulkan and RTX beside GLx and retail, 2026-10-23

The [October 9 retail views](2026-10-09-stvef-sp-retail-views.md) left one
measured gap in Elite Force's renderer gate. In eight synchronized
single-player scenes, fnTech3's GLx came within 0.03 to 1.4 of retail
`stvoy.exe` 1.20 (mean absolute RGB difference out of 255). Vulkan and RTX
raster stayed at 0.5 to 2.5, with "bright strip differences" that neither HDR
nor the offscreen buffer explained. This record finds the four causes. Two
were defects that the Vulkan and RTX renderers share with every profile, and
both are fixed: anisotropic filtering under a bilinear texture mode, and the
depth margin of the flare test. The other two are fnTech3 presentation
choices, and they stay. Like for like, Vulkan and RTX now come within about
0.25 of GLx on every scene. The [JSON companion](2026-10-23-stvef-renderer-parity.json)
holds commands, runtimes, hashes and every measurement. This is Unit and Live
subset evidence; no compatibility gate closes.

## Method

Every capture is the engine's own `screenshot`, from
`scripts/probe_stvef_sp_views.py`. Each process runs windowed at 640 by 480
on a private desktop, with no input and no sound. Retail runs twice and
fnTech3 once per renderer. The runs use `fixedtime 50`, frozen NPCs and
effects, and the eight scenes and views of the October 9 record, whose
camera positions every run reproduced. The probe gained `--retail-all-set`,
which sets a cvar on both retail runs, so compression could be switched off
in retail as well. A small harness (`.tmp/stvef-round-1023/toggles.py`) ran
fnTech3 alone, one setting at a time, and compared Vulkan's and RTX's images
with GLx's. A ray cast through each map's BSP, from the recorded view,
named the surface and shader behind each pixel that differed.

## Causes

**Bloom.** Vulkan and RTX register `r_bloom 1`, and GLx `0`
(`code/renderervk/tr_init.c`, `code/rendererrtx/tr_init.c`,
`code/renderer/tr_init.c`). The earlier comparisons left it on, and its glow
is the "bright strip": light fixtures, Borg panels and the walls beside
them. Switching it off, or switching off the offscreen buffer bloom needs,
removed most of the gap: `stasis1` fell from 0.54/1.06/1.07 to 0.11/0.52/0.55
against GLx, and `voy14` from 1.10/1.09/1.01 to 0.34/0.33/0.28. Bloom is an
fnTech3 feature, so it stays on by default (invariant 11); a like-for-like
comparison turns it off.

**Texture compression.** Elite Force's menus and configuration default
`r_ext_compress_textures` to 1. GLx then uploads DXT1 and DXT5, as retail
does on the same NVIDIA GPU. Vulkan and RTX have no compression encoder and
upload uncompressed ([compression record](2026-09-21-stvef-texture-compression.md)).
With compression off in both engines, `voy2`'s Vulkan and RTX difference
from retail fell from 0.50 to 0.015, the same as GLx's. Uncompressed textures
are the better image, and they stay.

**Anisotropy under a bilinear texture mode, now fixed.** The GL renderers
give every mipmapped image the `r_ext_max_anisotropy` setting, whatever its
texture mode (`code/renderer/tr_image.c`). Vulkan and RTX gave it only to
the two modes that blend between mip levels (`vk_find_sampler`). Both retail
executables default to `GL_LINEAR_MIPMAP_NEAREST`, and under it Vulkan and
RTX filtered with no anisotropy at all. Floors seen at a grazing angle, and the `detail` stages on
top of them, drew blurrier than in GLx and retail: `borg2`'s `floor5b`,
`scav2`'s ceilings. Both renderers now apply it to every mipmapped image, as
the GL renderers do. With bloom, compression and flares off, Vulkan and RTX
against GLx:

| Scene | Vulkan before | Vulkan after | RTX before | RTX after |
| --- | --- | --- | --- | --- |
| stasis1 | 0.105/0.519/0.549 | 0.032/0.075/0.077 | 0.083/0.497/0.527 | 0.009/0.053/0.055 |
| scav2 | 0.637/0.659/0.650 | 0.307/0.297/0.288 | 0.549/0.570/0.561 | 0.149/0.140/0.134 |
| borg2 | 1.357/1.271/0.740 | 0.467/0.454/0.301 | 1.298/1.214/0.693 | 0.326/0.316/0.176 |
| scav3 | 0.307/0.305/0.265 | 0.235/0.236/0.205 | 0.206/0.196/0.138 | 0.081/0.073/0.051 |
| forge2 | 0.320/0.113/0.121 | 0.026/0.021/0.021 | 0.319/0.111/0.119 | 0.024/0.019/0.019 |
| voy14 | 0.341/0.332/0.281 | 0.072/0.075/0.076 | 0.316/0.307/0.255 | 0.044/0.047/0.048 |

**The flare test's depth margin, now fixed.** The GL renderers treat a
flare as visible while its depth lies within 0.25 / w of the depth in front
of it, or 0.75 / w in a portal view (`code/renderer/tr_flares.c`). Their
depth maps distance as the Vulkan renderers' does. Vulkan and RTX used
0.2 / w everywhere, so a flare set just behind its own surface, such as the
glow at the foot of `forge2`'s red pillar, vanished there and drew in GLx
and retail. Both now use GL's margins. On `forge2` with flares on, bloom and
compression off, Vulkan's difference from GLx fell from 0.751/0.603/0.610 to
0.036/0.031/0.031, and RTX's to 0.024/0.019/0.019.

Two smaller differences remain. Vulkan, though not RTX, differs from GLx by
one level in places, a mean of up to about 0.24 on `dn1` with no pixel more
than four levels apart; it was not traced further. And flares themselves
differ from retail's in every fnTech3 renderer. Giving fnTech3 retail's
flare size of 30 moved its images further from retail's, not closer, so
the difference lies in how a flare is drawn; retail's renderer is inferred
to keep an older formula than the Quake III 1.32 one fnTech3 uses **[I]**.
Flares are off by default in fnTech3, and their drawing is presentation; it
is left as it is.

## Beside retail, like for like

The final sweep runs fnTech3 with the settings retail's first start leaves
in force (`--matched`: flares, projection shadows, picmip 1, bilinear
filtering, anisotropy 16), bloom off, and compression off in both engines,
on binaries built from a `git archive` of `f4d07ef0` with this round's
files alone. Retail's two runs agree to within 0.009.

| Scene | GLx | Vulkan | RTX raster SDR |
| --- | --- | --- | --- |
| voy2 | 0.015/0.014/0.015 | 0.015/0.015/0.015 | 0.015/0.014/0.014 |
| stasis1 | 0.148/0.474/0.476 | 0.155/0.519/0.523 | 0.156/0.519/0.523 |
| scav2 | 0.163/0.188/0.260 | 0.391/0.403/0.432 | 0.276/0.292/0.354 |
| dn1 | 0.035/0.036/0.072 | 0.237/0.239/0.251 | 0.037/0.040/0.078 |
| borg2 | 1.458/1.548/0.737 | 1.694/1.783/0.898 | 1.625/1.714/0.832 |
| scav3 | 0.577/1.294/1.595 | 0.695/1.415/1.724 | 0.597/1.312/1.614 |
| forge2 | 0.697/0.307/0.279 | 0.703/0.316/0.288 | 0.703/0.316/0.289 |
| voy14 | 0.213/0.187/0.166 | 0.265/0.253/0.232 | 0.200/0.189/0.172 |

The larger numbers on `borg2`, `scav3` and `forge2` are flares, which
differ from retail in all three renderers alike. With flares off as well,
GLx came within 0.13 of retail on `scav3` and 0.95/1.17/0.52 on `borg2` in
the same session. All 40 runs exited normally at matching view positions,
and the installation was unchanged.

## Other profiles

Both fixes are in code every profile shares. Under the default
`GL_LINEAR_MIPMAP_LINEAR` the sampler is created as before: the rule is
unchanged for the modes that blend between mip levels.
`tests/vulkan_gl_filter_flare_parity_tests.py` asks each renderer's
`vk_find_sampler` for all 96 combinations of texture mode, magnification,
fixed level of detail, opt-out and setting. Each comes out as the GL rule
gives it, and HEAD's code fails the same test in both renderers. Flares are
off by default in every profile; with them on, each profile's Vulkan and RTX
flares now follow its GL flares. A Quake III check on `q3dm1` (shared
client, `fixedtime 16`, flares on, bloom off, anisotropy 16) ran all three
renderers under both texture modes, exited normally and captured. Vulkan's
difference from GLx was 1.32/1.04/0.82 under trilinear and 1.36/1.08/0.87
under bilinear, where the bilinear sampler had had no anisotropy at all.
That remaining Quake III difference predates this change.

## Features

Bloom, uncompressed textures, per-pixel and static lighting, and the
renderers' own fog and shadows stay as fnTech3's defaults; RTX's ray-traced
mode, still unavailable on this machine's x86 driver, is not covered. No
retail-exact option is added: none of these choices hides a contract, and
each comparison names the settings it turns off.

## Validation

The renderer changes were built and tested with the rest of the round, as
the commit's tree alone; the [registrations record](2026-10-23-stvef-retail-registrations.md#validation)
gives the build and suite results.

## Limits

These are eight static scenes, without animated NPCs or effects, and
synchronized pixel identity is not claimed. Flares still differ from retail,
Vulkan keeps its one-level rounding differences, and hardware ray tracing,
the physical display and other GPUs are not covered.
