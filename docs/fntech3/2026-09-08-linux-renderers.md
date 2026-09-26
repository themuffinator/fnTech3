# Clean Linux i686 client and renderer build

The clean detached `b5776c9` checkout used for the
[dedicated-server checkpoint](2026-09-08-clean-linux.md) was configured in a
separate build directory for the client and all three renderer modules.
GCC 13.3.0 and Meson 1.10.2 used `misc/meson/linux-x86.ini`, debugoptimized
output, renderer DLL loading and GLx as the default. Audio and GLx tests were
enabled; server, SDL, curl and Ogg Vorbis were disabled. This tests the
non-SDL X11 client build, not the separate bundled-SDL build path.

All 670 configured build steps completed. The client and GLx, Vulkan and RTX
modules are ELF 32-bit Intel 80386 artifacts. Client loader dependencies
resolve to the installed i386 libraries. Artifact hashes and exact commands
are in the [evidence record](2026-09-08-linux-renderers.json).

Missing system dependencies used the repository's existing wrap fallbacks:
FreeType 2.14.3, libjpeg-turbo 3.1.4.1, OpenAL Soft 1.25.2 headers and
FontStash. Meson downloaded the pinned archives and verified their declared
hashes. No version, checksum, dependency source or tracked engine source was
changed to obtain this build. The checkout remained clean.

The complete configured suite passed 107 tests with zero failures and two
skips: unavailable OpenAL loopback and the retail-factory probe (both exit 77).
The aggregate Python suite completed in 242.28 seconds. Its standalone
compiler probes select their own targets and are not all claimed as i686
binaries. These results supersede neither the separate server configuration
nor the Windows x86 runtime evidence.

This is build and configured-test evidence. No Linux client was launched and
no native renderer scene, input behavior, ray-tracing capability or WebUI
lifecycle was observed here. The shared Windows x86 runtime gates and Linux
client runtime gates remain separate. Hosted CI remains blocked by the
recorded account billing/spending issue. Existing compiler and image/font
warnings remain unresolved.
