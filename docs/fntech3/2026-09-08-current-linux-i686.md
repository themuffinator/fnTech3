# Current Linux i686 build and source-archive test portability

A fresh export of commit b6b98a6 builds the Linux i686 client, dedicated server
and GLx/Vulkan/RTX modules in 928 steps. It uses the checked-in i686 cross file,
GCC 13.3.0, release optimization, cURL enabled with runtime loading, Ogg/Vorbis
enabled, non-SDL X11 and enabled audio/GLx tests. Wrapped dependency archives are
reused, but no source tree, object file or binary is inherited from older builds.
The resulting executables/modules identify as ELF32 Intel 80386 and their linked
loader dependencies resolve locally. This is not the separate bundled-SDL build.

The original build in /tmp completed and passed bounded runtime probes, but that
source directory was later absent for an unestablished reason. Preliminary
suite launches also encountered shell working-directory and line-ending errors;
none is reported as an executed suite. A second clean export under the persistent
Linux home build directory completed the same build, with compilation and tests
chained in one process session. Its binaries were also preserved in ignored
workspace output. The [evidence record](2026-09-08-current-linux-i686.json)
retains the preliminary outcomes and identifies the final artifacts.

The first actual full suite found one portability defect in a test: the curated
release-notes case depended on the working checkout's Git HEAD/history, although
it only checks note precedence and formatting. An exported source tree has no
.git directory. The test now uses deterministic commit fixtures and mocked
history while retaining its production render_release_notes call and assertions.
It also rejects an unexpected call to the Git helper. The focused 15-test module
passes on Windows and in the Linux export. Only this test file is changed from
the source export; engine and other exported files remain unchanged. The initial
full-suite failure is preserved, not rewritten as a pass. The corrected full
suite passes 109 configured checks with two skips and no failures. Its aggregate
runs 1,281 tests with 20 platform/fixture skips. Standalone compiler tests choose
their own target; not every Python-spawned probe is asserted to be i686.

The final Q3 runtime uses the new client and dedicated server, legitimate Q3
assets, isolated writable homes, windowed/input-disabled X11 and a private Xvfb
display with software OpenGL. It reaches CS_ACTIVE after pure admission,
continues receiving snapshots, renders a readable world/HUD in an engine
screenshot, and exits normally on both processes. No fatal signal or recursive
shutdown appears. This is a bounded GLx software-rendering observation, not
native Vulkan/RTX or hardware parity evidence.

The same dedicated binary loads legitimate retail QL API-10 qagame, runs
campgrounds/ffa, executes status and exits normally. No QL Linux client is
claimed: this installation supplies Linux qagame but lacks Linux cgame/UI.
Both installations remain read-only. Proprietary inputs and engine screenshots
remain untracked, and no keyboard/mouse control or OS capture was used.

The source tar was checked against all 1,384 exported regular files before the
test correction with no differences. Follow-up verification records only the
intentional test change. A subsequent [Linux HTTP probe](2026-09-08-linux-http-runtime.md)
verifies the Q3 runtime-loaded cURL success path with these artifacts. Full
packaging, the separate SDL path, broader HTTP coverage, all-renderer/driver
coverage, Windows shared-client sign-off and
independent retail interoperability remain open. Existing compiler signedness/
unused-function warnings and image/font diagnostics are not fixed here. The
cause of the first temporary source's disappearance remains unestablished.

No external implementation was incorporated by the test correction.
