# Linux baseline refresh after the audio corrections

A fresh export of commit `2c33a2bae2da91f7826599e2a21aed5239bf1c69`
compiles the recent streaming resampler and PCM8 WAV corrections in the existing
isolated Ubuntu 20.04 userspace. All 460 build steps succeed with GCC 9.4,
Python 3.11.9 and Meson 1.9.1. The complete configured suite reports 108 passes,
zero failures and one retail-factory probe skip; its Python regression target
runs 1,293 tests with 21 skips. No source changes were needed.

The six ELF32 i386 binaries require at most GLIBC_2.29 and pass the configured
GLIBC_2.31 release check. A 268-file package produces identical tar.gz archives
after changing staged timestamps. The extracted package then runs inside that
same older userspace: Q3 joins its pure server and an inspected engine screenshot
shows the q3dm6 world, weapon and HUD; QL loads the legitimate retail native
qagame API 10 module and runs Campgrounds. Both Q3 processes and the QL dedicated
server exit zero normally. Every exported source file and all 268 extracted
package files remain unchanged after testing.

The [structured evidence](2026-09-08-linux-audio-build-refresh.json) pins the
source, tool/package versions, command lines, configuration, binary identities,
archive inventory, test/log hashes and runtime outcomes. The isolated userspace
and its provenance are documented in the
[earlier baseline build](2026-09-08-glibc-baseline-build.md).
A CRLF blank line in the disposable shell wrapper caused a command-not-found
exit after the successful suite. Its line endings were corrected; the completed
build and tests were not unnecessarily repeated. Subsequent installation,
packaging and runtime commands completed successfully.

Runtime uses private Xvfb/software GL, a windowed input-disabled client, isolated
writable homes and read-only retail mounts. No OS screenshot or input control
occurs. Proprietary inputs, engine screenshots and raw logs remain ignored.
The Q3 runtime disables audio: this proves current Linux build/test compatibility
and bounded packaged runtime, not Linux audio output. Linux QL client modules,
hardware Vulkan/RTX, independent retail peers, hosted CI and reproducible compiler
outputs remain outside this checkpoint. Existing compiler warnings and image
mixed-flags diagnostics remain unresolved. No external implementation was added.
