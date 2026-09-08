# Refresh the clean Linux package after WAV and Ogg corrections

A fresh export of `e02a4b3` builds in the existing isolated Ubuntu 20.04 userspace
with GCC 9.4, Python 3.11.9 and Meson 1.9.1. This includes the startup overflow,
WAV short-read, Ogg bounds/callback and Vorbis speaker-order corrections since
the [previous Linux refresh](2026-09-08-linux-audio-build-refresh.md).
No source changes were needed. All 1,523 exported files remain identical to the
commit after validation.

The complete configured suite passes 108 targets, zero failures and one skip.
Its Python target runs 1,299 tests with 21 skips. These are the configured Linux
results, not a relabeling of Windows test counts. All six ELF32 i386 binaries
require at most GLIBC_2.29 and pass the GLIBC_2.31 release ceiling check. The
300-file package produces identical tar.gz archives after staged timestamps
are changed; this establishes archive determinism, not independent compiler
reproducibility.

The extracted package runs inside the same older userspace. Q3 joins its pure
server and reaches active play; an inspected engine `screenshotPNG` output shows
the q3dm6 world, weapon and HUD. Both client and server exit zero normally. QL
loads the legitimate native qagame API 10 module and runs Campgrounds; its
dedicated process also exits zero. All 300 extracted package files remain
unchanged after testing.

Runtime uses a private Xvfb display with software GL, windowed input-disabled
clients, isolated writable homes and read-only retail mounts. No OS screenshot,
input control or installation modification occurs. The Q3 runtime disables
audio, so this checkpoint does not establish Linux audio output. Linux QL client
modules, hardware Vulkan/RTX, remote peers, independent compiler reproducibility
and hosted CI remain outside this evidence. Existing compiler warnings and QL
`g_ammoPack` initial-value warnings remain; no new unrelated blocker was found.

The [structured record](2026-09-08-linux-media-build-refresh.json) pins the full
source commit, userspace/tool versions, build configuration, binaries, archive,
runtime commands, outcomes and log hashes. Retail data, raw runtime logs and
engine screenshots remain ignored. No external implementation was added.
