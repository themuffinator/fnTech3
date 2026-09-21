# Elite Force texture-compression control and capture correction

Both Elite Force profiles now register the texture-compression control used
by their retail menus and configuration files: `r_ext_compress_textures`.
Previously the renderers registered only Q3's `r_ext_compressed_textures`,
so the retail setting could change without reaching the renderer. This also
explains most of the static corridor difference in the preceding
[startup/capture record](2026-09-21-stvef-sp-startup.md).

The [machine-readable record](evidence/2026-09-21-stvef-texture-compression.json)
contains commands, source and executable identities, effective settings,
image measurements and local evidence paths. No retail executable, module,
asset, disassembly or screenshot is incorporated in the repository.

## Native observations and implementation

The supplied Windows x86 SP and Holomatch 1.20 executables both register
`r_ext_compress_textures` with default `1` and flags `0x21` (archive and
latch). SP's registration starts at `0x4404c9`; Holomatch's at `0x453729`.
The SP UI reads this name at `0x40031518` and writes it through the numeric
cvar import at `0x40031f57`. All three supplied retail `vm/ui.qvm` revisions
also contain this name. Fresh runtime queries confirm the default and
changes latched until `vid_restart`.

A small shared registration helper selects that native contract for SP and
Holomatch in GLx, Vulkan and RTX. Other profiles retain their existing name,
default and developer restriction. Elite Force's control is archived and
does not require developer mode. Each profile has one authoritative setting;
a separately present cvar with the other spelling does not override it.
The implementation is independently authored from these observations.

The upload backends retain their existing capabilities. GLx can select DXT1
when the driver offers S3TC. Vulkan and RTX currently upload uncompressed
textures and expose `TC_NONE`; enabling the request does not create a
compression capability they lack. Their configured value is retained across
renderer restarts. This change does not add a compression encoder.

## Verification

The compiled regression uses each renderer's actual registration call and
the real engine cvar system. Sixty-three cases per platform cover all seven
profiles, defaults, preloaded zero/one settings, coexistence of both names,
developer restrictions, pending values, restart registration, numeric menu
writes and reset. It fails against the previous registration in all three
renderers, and passes on Windows x86 and Linux i686; Linux uses address and
undefined-behavior sanitizers. All three renderer modules rebuild on both
platforms. The full Windows suite passes 120 targets with two skips and no
failures, including 1,855 Python checks with 72 skips in 659.574 seconds.
The wider Linux suite is not rerun; its previously recorded failures remain.

Runtime probes cover SP and Holomatch default → off → on → off video
restarts. Windows GLx and Linux Mesa GLx switch their actual texture formats
with the setting. Vulkan and RTX retain uncompressed images. Native SP
switches compression on the same NVIDIA GPU as fnTech3. Native Holomatch
selects the Intel GPU in this test and reports no supported compression
method; its cvar/latch behavior is observed, but its pixels are not compared
against fnTech3's NVIDIA output. Every run checks the selected renderer and
uses images written by the engine's screenshot command.

Across the isolation, before/after, restart and staged-engine checks, all
27 runtime runs exit normally and produce 84 verified engine screenshots.
The manual test engine under `E:/Games/fnTech3-tests/stvef-manual` is refreshed
to source commit `c8dd7ca`. Its eight existing settings/save/launcher files
retain their hashes. Fresh isolated SP save-load and Holomatch bot-match
preflights pass with that staged engine; no user launcher is executed.

## Corrected SP image comparisons

The earlier SP capture scripts set `r_ext_compressed_textures 0`, which is
not the retail control. Native `imagelist` shows DXT1 where fnTech3 showed
RGB. That comparison had matching filters and displayed snapshot times,
but different texture compression. Its original measurements remain valid
for those settings; they do not isolate renderer error.

Lightmap-only captures reduce the static corridor's mean RGB difference
from 1.148274 to 0.018021 out of 255. Fullbright retains a 1.757552 difference,
pointing toward diffuse textures. Disabling the actual retail compression
control then produces the following full-frame results against native SP,
with the same asset set, fixed step, capture phase and nearest mip filter:

| Sample | GLx mean / maximum | Vulkan mean / maximum | RTX mean / maximum |
| --- | --- | --- | --- |
| 20 | 0.139117 / 27 | 0.145454 / 61 | 0.145454 / 61 |
| 80 | 0.075416 / 117 | 0.074269 / 117 | 0.074269 / 117 |
| 160 | 0.075416 / 117 | 0.074269 / 117 | 0.074269 / 117 |

With compression enabled in both native SP and the fixed GLx renderer,
mean differences are 0.608110 at sample 20 and 0.163173 at samples 80/160.
Both image lists show DXT1. The old GLx binary ignores the native enable
setting and remains uncompressed. Vulkan/RTX retain the earlier roughly
1.15 static difference when compared against compressed native output;
that comparison deliberately has different texture formats.

The small means are not pixel identity: localized maximum differences
remain, and only this controlled tutorial sequence is compared. The
reusable `tests/stvef_sp_capture.py` now also queries the effective native
compression setting so later comparisons can detect this mismatch.
Native SP also reports 146 DXT5 alpha images in the enabled capture, while
GLx reports 146 uncompressed RGBA images. Alpha compression is a separate
remaining format difference; this control-registration fix does not close it.

All writable homes are separate copies under `E:/Games/fnTech3-tests`.
Windows clients run windowed on inactive private desktops; Linux uses a
private Xvfb display. Mouse, joystick and SP feedback are disabled. No user
input is controlled and the original installation is unchanged. Useful
local scripts, logs and captures remain under the designated project/test
directories.

The native controls `r_ext_compress_lightmaps` and
`r_ext_preferred_tc_method` are also observed, with default zero and archive/
latch flags, but are not implemented by this slice. Broader texture-format
and alpha-compression parity, manual input/menu/listening tests, feedback hardware and
campaign acceptance remain open. Previously recorded asset defects, wider
Linux-suite failures and the Windows x86 RT extension limit remain open.
The Linux build also repeats existing signedness and unused-helper warnings.
Both Elite Force profiles remain development-gated.
