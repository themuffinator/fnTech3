# Clean Linux build and package after the filesystem corrections

A fresh export of d5c7fbd completes all 928 Linux i686 build steps and the full
configured suite: 109 targets pass, none fail and two skip. The Python aggregate
runs 1,288 tests with 20 platform/fixture skips. Verification compares all 1,439
exported regular files without differences. Wrapped dependency archives are
reused, but source/object/build directories are new. The configuration uses the
checked-in x86 cross file, non-SDL X11 release mode, runtime-loaded cURL,
Ogg/Vorbis, GLx/Vulkan/RTX modules and audio/GLx tests.

Meson installs the built project with --no-rebuild and --skip-subprojects into an
isolated destination. The production release staging/archive helpers produce a
226-file Linux tar.gz containing the engine, three renderer modules, audio-zone
tool, root/web archives, licenses and evidence docs. Layout/internal-link checks
pass. Repeating archive creation after timestamp changes produces identical bytes.
A fresh extraction into the native Linux filesystem preserves executable modes
on all six ELF32 i386 files. This proves archive repeatability for fixed inputs,
not reproducible compiler output or a full public release pipeline.

The extracted client/dedicated binaries pass Q3 pure admission, q3dm6 world/HUD
rendering in an engine screenshot and normal exit on both processes. The same
dedicated executable loads retail QL qagame, runs campgrounds/ffa and exits zero.
All six synthetic invalid-pak checks also pass: both profiles reject incorrect
CRC, INT_MAX and UINT32_MAX payload declarations while executing a following
valid config and exiting normally. After runtime all 226 extracted files remain
unchanged, without extra files. The [evidence record](2026-09-08-linux-filesystem-package.json)
pins commands, source, binary and archive hashes, modes, dependencies and outcomes.

## Release baseline failure

The explicit release glibc validator exits 1. Each of the six ELF files requires
GLIBC_2.38, exceeding .github/workflows/release.yml's configured GLIBC_2.31 maximum.
The earlier stage/archive-layout passes do not supersede this failure. These
artifacts run on the current WSL host but are not eligible evidence for the
configured Linux release baseline. A build against the supported older userspace
and its runtime validation remain required; the baseline has not been relaxed.

The Q3 client runs windowed with input disabled on a private Xvfb display using
software OpenGL. Writable homes are isolated and installations read-only. No
keyboard/mouse control or OS screenshot is used; proprietary inputs/screenshots
remain ignored. QL Linux client playback is not claimed because the installed
archive lacks Linux cgame/UI modules. Vulkan/RTX compilation is not current
hardware-runtime evidence. Existing compiler and image/font diagnostics remain
outside this checkpoint. No engine source or external implementation was added.

The [Ubuntu 20.04 follow-up](2026-09-08-glibc-baseline-build.md) builds new binaries
that pass the configured baseline and run in the older userspace. It does not
change the GLIBC_2.38 requirement of the artifacts recorded above.
