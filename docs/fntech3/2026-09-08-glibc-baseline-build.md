# Build and runtime within the Linux glibc release baseline

The final six ELF32 i386 binaries built in isolated Ubuntu 20.04 userspace pass
the configured GLIBC_2.31 release check. Their highest required symbol version is
GLIBC_2.29. A repeatable 228-file tar.gz is extracted and run within that same
older userspace, so the evidence includes actual older-library runtime as well
as symbol inspection. This resolves the local artifact baseline failure recorded
in the [preceding Linux package checkpoint](2026-09-08-linux-filesystem-package.md)
for these newly identified binaries, without relaxing the baseline.

Docker's daemon was unavailable. The build instead uses a fresh chroot under the
native Linux build workspace from Canonical's
[Ubuntu Base 20.04.5 image](https://cdimage.ubuntu.com/ubuntu-base/releases/20.04/release/),
verified against its published SHA256SUMS. Package installations and an isolated
[Python 3.11.9 source build](https://www.python.org/ftp/python/3.11.9/)
remain inside that userspace. Private mount namespaces contain temporary proc/dev
and read-only retail-asset mounts. The host's libraries and installations remain
unchanged. The initial clean source export is 6da5108; comparison against that
archive records exactly the five intentional source/test changes below.

The first engine build completes 460 steps with GCC 9.4 and glibc 2.31. More system
dependencies are available here than in the previous build, so step counts are
not equivalent across environments. It uses the same i686 Meson cross file,
non-SDL X11 release configuration, cURL, Ogg/Vorbis, three renderer modules and
audio/GLx tests. The first suite reports 103 passes, five failures and one skip:
Python 3.8 lacks APIs used by the complete regression suite, and GCC 9 does not
accept the test-only C++20 flag. Python 3.8 can build this engine but is not a
verified interpreter for the full test suite. Use Python 3.11 for the complete
regression run in this documented environment.

The cgame-extension and server-command harnesses now compile as C++17, matching
the engine. The latter's test-only ranges stub uses a vector/iota equivalent.
The download-write harness names the temporary aggregate types explicitly for
GCC 9. These changes retain their existing behavioral assertions.

The next run reveals an engine-side numeric issue: the statistics JSON parser
can perturb an exactly representable value such as -12.5 through repeated
multiplication by binary 0.1 on the older x86 build. It now divides by powers of
ten and explicitly rounds the final result to double before validating finite,
nonzero output. Existing negative/exponent/signed-zero/range tests and additional
representations of exact 12.5 pass. This is not exhaustive correctly-rounded
conversion or live optional-service evidence. The intermediate 106-pass,
two-failure, one-skip result is retained in the record.

After corrections the complete Ubuntu 20.04 suite passes 108 targets with no
failures and one skip, including 1,288 Python tests with 21 skips. Windows x86
also builds and passes all 110 configured targets with no failures and one skip;
its nested Python result is recorded. No assertion or failing category was
removed. The [structured evidence](2026-09-08-glibc-baseline-build.json) pins
source changes, tools/packages, downloads, binary modes/dependencies, test and
build logs, commands, package inventory and runtime outcomes.

From the final archive extraction, Q3 passes pure admission and renders its
world, weapon and HUD in an engine screenshot before both processes exit zero
without forced termination. The same dedicated executable loads retail QL
qagame, runs campgrounds/ffa and exits zero. All 228 runtime files remain unchanged.
Clients are windowed, input-disabled and confined to private Xvfb/software GL;
writable homes are isolated and retail mounts read-only. No keyboard/mouse control
or OS capture occurs. Proprietary inputs and screenshots remain ignored.

The preceding Windows/Ubuntu artifacts are not retroactively relabelled by this
pass. Full public-release orchestration, compiler reproducibility, hardware
Vulkan/RTX runtime, QL Linux client modules, optional services and independent
retail interoperability remain open. Existing compiler/image/font diagnostics
remain outside this checkpoint. No external implementation was incorporated;
Ubuntu/Python downloads are isolated build tools, not shipped engine content.
