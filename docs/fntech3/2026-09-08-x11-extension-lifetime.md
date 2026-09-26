# Keep X11 extension callbacks alive through display shutdown

The native i686 Q3 shutdown fault is caused by premature unloading of the DGA
library. GDB observes `XESetCloseDisplay` registering callback `0xeb0423d0` from
`libXxf86dga.so.1`. The unavailable-extension branch then calls `DGA_Done`,
which unloads that library. At shutdown, `XCloseDisplay` calls the same stale
address. By then it belongs to a non-executable mapping of `libGLX_mesa.so.0`.
The earlier debugger attribution to that mapping did not establish a Mesa bug.

The independently authored fix retains extension libraries after unsuccessful
discovery, because even a failed extension query can register display callbacks.
The analogous RandR and VidMode failure paths receive the same correction.
OpenGL and Vulkan shutdown now release DGA alongside the other X11 extension
libraries after `XCloseDisplay`. A retained-display renderer restart keeps the
libraries available; unavailable extension capability flags remain false.

`tests/x11_extension_lifetime_tests.py` executes production DGA discovery and
OpenGL/Vulkan shutdown with controlled Xlib/library dependencies. The pre-fix
source fails when it unloads a library with a registered display callback.
The corrected source passes on Windows and Linux for unavailable extensions,
version-query failure, missing libraries/symbols, successful discovery,
retained-display restarts, partial initialization and repeated cleanup. The
21 existing Windows platform-safety tests also pass. The complete configured
Linux renderer suite passes 107 checks with zero failures and two unavailable
OpenAL/retail-factory fixture skips; its aggregate source suite passes 1,254
tests with 15 skips. These are lifecycle tests,
not a claim of Vulkan runtime or QL Linux client validation.

A windowed native i686 X11/GLx Q3 probe under private Xvfb and llvmpipe now
passes pure admission, receives snapshots, writes an engine screenshot, and
exits normally along with its dedicated server (both status zero, neither
forcibly terminated). The inspected screenshot has readable world and HUD
textures. This modified client is built from the earlier `b5776c9` checkout with
the Unix exit correction and this lifecycle correction; it is not a new clean
checkout build. The [evidence record](2026-09-08-x11-extension-lifetime.json)
pins source changes, artifacts, commands, installation identity and log hashes.

This closes the reproduced DGA stale-callback shutdown defect. SDL texture
corruption, full visual parity, native Vulkan runtime, sustained/restart runtime
coverage and all broader compatibility gates remain open. The legitimate local
QL installation lacks native Linux cgame/UI modules, so this client runtime
probe covers Q3 only. Existing image/font warnings and hosted CI's account
billing/spending blocker remain unresolved.
