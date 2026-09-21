# Elite Force texture-compression method selection

Both Elite Force profiles now honor `r_ext_preferred_tc_method` and the
floating-point enable value of `r_ext_compress_textures`. Previously fnTech3
always chose ARB compression when available and treated `0.5` as disabled.
The default remains ARB S3TC. Preference `1` selects the legacy S3 method
when available; other values prefer ARB, and an unavailable preferred method
falls back to the supported one. With compression disabled or neither method
available, the reported capability remains none.

The [evidence record](evidence/2026-09-21-stvef-compression-method.json)
pins native observations, sources, binaries, checks, runtime commands and
retained local files. Production commit `891d694` follows the earlier
[control registration](2026-09-21-stvef-texture-compression.md) and
[alpha-format correction](2026-09-21-stvef-alpha-compression.md).

## Native contract and implementation

An owned Windows x86 observer maps each supplied retail 1.20 executable
without starting the game or graphics. It calls the complete native selector
at SP `0x456cc0` or Holomatch `0x46a140`, using owned cvar storage, extension
strings and a print sink. Native string searching remains unchanged.

The two engines agree on all 224 cases each: eight combinations of the three
extension tokens, seven preferences (`-7`, `-1`, `0`, `1`, `2`, `3`,
`2147483647`) and four enable values (`0`, `1`, `-1`, `0.5`). ARB compression
requires both `GL_ARB_texture_compression` and
`GL_EXT_texture_compression_s3tc`; the legacy method requires `GL_S3_s3tc`.
Both engines register the preference with default `0` and archive/latch flags
`0x21`, at SP `0x4404f7` and Holomatch `0x453757`.

The shared OpenGL initialization helper implements the independently observed
EF policy. Its other-profile branch preserves the previous integer enable,
ARB-first selection and diagnostic text. Only the EF profiles register and
adopt the preference; an unrelated profile's config value does not affect
its renderer. The early `r_allowExtensions` gate remains in place. The helper
also serves the retained OpenGL paths in the Vulkan/RTX modules, while their
shipping Vulkan upload paths continue to report uncompressed capability.

The compiled regression executes the selection block used by each renderer
against the native fixture and all seven profiles: 4,704 choices per platform.
The real cvar-system regression covers another 147 registration, config,
latch, reset and profile-isolation cases per platform. The previous code
fails in all three renderer implementations on the fractional enable case.
Windows x86 and Linux i686 pass; Linux uses address and undefined-behavior
sanitizers. All three modules build on both platforms.

The focused Windows run passes 49 Python checks. The full Windows suite
passes 120 targets, with two skips and no failures, including 1,857 Python
checks with 72 skips in 633.452 seconds. The wider Linux suite is not
rerun; its previously recorded unrelated failures remain open. The Linux
build repeats existing signedness and unused-helper warnings.

## Runtime evidence and limits

Six Windows sessions and one Linux session each exercise seven phases:
default, legacy preference, ARB preference, invalid preference, fractional
enable, disabled and restored. Each session performs six video restarts,
queries both controls, records image formats and writes seven engine
screenshots. All exit normally.

| Runtime | Legacy preference | Fractional enable | Disabled |
| --- | --- | --- | --- |
| Retail SP, NVIDIA | 324 S3TC, alpha uncompressed | 324 DXT1 / 146 DXT5 | RGB/RGBA |
| fnTech3 SP GLx, NVIDIA | 301 S3TC, alpha uncompressed | 301 DXT1 / 145 DXT5 | RGB/RGBA |
| fnTech3 Holomatch GLx, NVIDIA | 279 S3TC, alpha uncompressed | 279 DXT1 / 179 DXT5 | RGB/RGBA |
| fnTech3 Holomatch GLx, Mesa | 279 S3TC, alpha uncompressed | 279 DXT1 / 179 DXT5 | RGB/RGBA |
| Retail Holomatch, Intel | No supported compression method | No supported compression method | RGB/RGBA |
| fnTech3 SP Vulkan / RTX raster | RGBA | RGBA | RGBA |

The default, explicit ARB, invalid-preference, fractional-enable and restored
phases have equal format counts within each session. Native SP and fnTech3
have different image inventories; the counts do not imply identical loading.
Native Holomatch selects Intel Iris Xe on this machine and advertises no
supported compression method. Its result is a capability limitation, not a
same-device comparison with the NVIDIA clients. These captures validate
restart recovery and method selection; they are not synchronized pixel tests.

Two more preflights use the refreshed engine staged at
`E:/Games/fnTech3-tests/stvef-manual`: SP loads the known save, and Holomatch
loads a bot match. The six staged engine artifacts match the tested build;
eight existing settings/save/launcher files retain their hashes. No user
launcher is executed. All nine sessions exit normally and produce 51 engine
screenshots, whose hashes are verified in the evidence record.

All clients run windowed with mouse, joystick and force feedback disabled,
on inactive private Windows desktops or private Linux Xvfb. Writable homes
are independent under `E:/Games/fnTech3-tests`. Only engine screenshot commands
produce the captures. The original installation is unchanged, no user input
is controlled, and no proprietary content or executable bytes are committed.
Useful logs, helper sources, observations and captures remain in the designated
project and test directories.

Native lightmap compression and the `r_texturebitslm` precision control still
need implementation and runtime evidence. Localized image differences,
campaign play, physical input, menu save/load/rebinding, listening and feedback
hardware remain open. Existing asset defects, wider Linux-suite failures and
the Windows x86 ray-tracing limitation are unchanged. Both EF profiles remain
development-gated; this correction does not establish full compatibility.
