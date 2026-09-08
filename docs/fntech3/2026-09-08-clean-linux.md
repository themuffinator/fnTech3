# Clean local Linux i686 validation

Hosted CI is currently unable to start new jobs because of the recorded
account billing/spending blocker. A clean detached checkout of `b5776c9`
was therefore configured and built locally under Ubuntu/WSL2 using GCC
13.3.0, Meson 1.10.2 and `misc/meson/linux-x86.ini`.

The checkout had no tracked changes. Its Git pointer was made relative so
Windows and WSL could both resolve its metadata. Source and build output were
isolated from the existing Windows and Linux build trees. Configuration used
`--buildtype=debugoptimized --wrap-mode=nofallback`, server enabled, and client,
SDL, curl, Ogg Vorbis, audio tests and GLx tests disabled. All 190 configured
build steps completed, including the dedicated server and available test
binaries. No bundled dependency fallback was downloaded or introduced.

`fntech3.ded` is ELF 32-bit LSB PIE for Intel 80386, using `/lib/ld-linux.so.2`.
Its SHA-256 is
`06ce3692030c9412f36e68e2dd62fdda9a0c5fb08b908ac6c0b5c403297cc82c`.
This is a native Linux dedicated configuration, not a Linux client/renderer
build or a replacement for the full hosted release matrix. Standalone Python
compiler probes use their own compiler selection; they are not all claimed
to be i686 executables.

All 84 runnable tests in this configuration passed, with zero failures and
one unavailable retail-factory probe skipped (exit 77). The aggregate Python
suite passed in 259.60 seconds. This is the complete configured server suite;
disabled audio/client/GLx targets are not counted as tested.

## Native profile probes

The clean checkout's `scripts/probe_game_profiles.py --steam-discovery` ran
both profiles with `STEAM_DIR=/mnt/c/Program Files (x86)/Steam`, without an
explicit `fs_basepath`. Q3 loaded its retail QVM/map and QL its native Linux
qagame/map. Both returned the expected loopback protocol/map info and exited
normally. Each used a fresh writable Linux home, pure mode enabled, Steam
integration and Workshop disabled, and automatic recording disabled. No
client, input control or screen capture was used; installations were read-only.

The [evidence record](evidence/2026-09-08-clean-linux.json) contains exact
configuration/build/test commands, test results, log hashes, executable
identity and profile manifests. These probes establish dedicated discovery
and startup, not live client pure admission or retail interoperability.

The build still emits existing unused-parameter warnings in inherited botlib
code. Hosted CI's billing/spending blocker, existing image/font warnings and
the broader compatibility gates remain open.
