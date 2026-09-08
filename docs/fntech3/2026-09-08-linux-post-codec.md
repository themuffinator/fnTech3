# Linux i686 validation after the QL codec corrections

A fresh source export of commit 218ab89 builds the Linux i686 client, dedicated
server and GLx/Vulkan/RTX modules in all 928 steps. The source verification
compares 1,423 exported regular files with no differences. Dependency archives
are reused, but compilation uses a new source and build directory. The checked-in
cross file selects the non-SDL X11 release configuration, runtime-loaded cURL,
Ogg/Vorbis and enabled audio/GLx tests. The [evidence record](2026-09-08-linux-post-codec.json)
contains build commands, options, executable identities, hashes and probe commands.

The complete configured suite passes 109 targets with no failures and two skips.
The Python aggregate runs 1,285 tests with 20 platform/fixture skips. These counts
are nested, not additive, and Python-spawned compiler probes are not all asserted
to be i686. No exported source correction or test retry was needed. This refreshes
the earlier [Linux checkpoint](2026-09-08-current-linux-i686.md) after the downloader
and retail QL movement/key-mask corrections.

Using the newly built binaries and legitimate read-only Q3 assets, the isolated
loopback client/server pair passes pure admission and reaches CS_ACTIVE on q3dm6.
The engine screenshot shows the world, first-person weapon and readable HUD.
Both processes exit zero without forced termination; the probe lasts 51.014 seconds.
The client uses a private Xvfb display, windowed mode, disabled input and software
OpenGL. No keyboard/mouse control or operating-system screenshot is used.

The identical dedicated executable loads the retail QL game module, runs
campgrounds/ffa, prints status and exits zero. This is bounded dedicated startup
and map evidence, not a Linux QL client observation. The supplied retail archive
lacks Linux cgame/UI modules. Writable homes are isolated; proprietary inputs,
raw runtime outputs, binaries and the screenshot remain ignored.

All compatibility gates remain partial. This does not establish the packaged
build matrix, independent retail-engine interoperability, current hardware
Vulkan/RTX behavior or full Windows shared-client compatibility. Existing mixed
image-flag/font diagnostics and compiler warnings remain outside this slice.
No external implementation or engine change was incorporated.
