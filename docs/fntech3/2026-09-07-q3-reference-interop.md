# Q3 reference interoperability — 2026-09-07

Both directions passed a bounded protocol-68 loopback session: the Windows
x86 fnTech3 client joined an existing FnQuake3 x64 dedicated server, and the
existing FnQuake3 x64 client joined the same fnTech3 x86 dedicated binary used
for the Q3/QL discovery checkpoint. Each client reported pure admission, each
server recorded `CS_ACTIVE` and `ClientBegin: 0`, and each engine screenshot
showed the q3dm6 world, weapon and HUD. All four processes exited normally.

This is reference-peer evidence, not retail interoperability closure. The
FnQuake3 binaries identify themselves as `FnQ3 0.1.0 win_msvc-x86_64-debug
Aug 28 2026`; their source revision has not been established. They must not be
represented as builds of the merger's pinned FnQuake3 source baseline. Shared
lineage also limits the independence of this peer check.

## Identity and reproduction record

The [machine-readable record](2026-09-07-q3-reference.json) contains
the exact executable/renderer/package hashes, command argument arrays, authored
configs, pak hashes, UTC times, log hashes, decisive log lines and screenshot
hashes for both directions. fnTech3 was checked out clean at
`0f3c5ba6f784222f3e0f50517068b4b1f4486552`. The reused binaries are identified by
their hashes; this was not a fresh clean-build claim.

The supplied Steam Q3 installation contributed only pak0 through pak8 to an
isolated asset tree outside the repository. Each process received a separate
temporary writable home. Neither the installation nor the reference checkout
was changed. The command arrays refer to these ephemeral paths; reproduction
requires substituting equivalent asset/home roots and available loopback ports.

The local probe created a private Windows desktop without switching to it,
launched clients windowed with `r_fullscreen=0`, disabled mouse/joystick capture,
and supplied console configs at startup. No user input was controlled or
injected. Screenshots came from `screenshotPNG interop-proof` in each engine,
and the resulting images were inspected. No OS screen capture was used.
Audio was disabled. Server configs used `sv_pure=1`, q3dm6, no master addresses,
and a bounded `wait 700; quit`; clients connected, waited, captured and quit.

## Results and limitations

| Direction | Elapsed probe time | Result |
| --- | --- | --- |
| fnTech3 client → FnQuake3 dedicated | 35.797 seconds | Pure admission, active client, rendered world/HUD, clean exits |
| FnQuake3 client → fnTech3 dedicated | 35.359 seconds | Pure admission, active client, rendered world/HUD, clean exits |

These durations include startup and the server's remaining scheduled lifetime;
they are not measurements of sustained connected play. Full logs and engine
PNGs remain local under `.tmp/q3-reference-{direction}-quiet/`. Retail assets
and screenshot pixels are not committed. No external implementation was copied.

The initial probe queried server status approximately every 0.2 seconds,
triggering connectionless rate limits and delaying connection attempts. Its
reverse screenshot showed an awaiting-connection screen despite exit code zero.
That run is not counted as a pass. Removing status polling after the initial
readiness response produced the two results above, without rate-limit drops.
This was a harness correction, with no engine change or relaxation of limits.

Protocol 71, downloads, non-loopback loss/fragmentation, adversarial clients,
long sessions, audio and retail Steam interoperability remain open. QL was not
exercised by this probe. The previous retail Q3 `Failed to find Steam` blocker
is unchanged. Missing shader/model resource warnings and reference cvar-flag
warnings were observed and remain outside this interop result.

Separately, Linux CI at this commit passed, as did the Windows MSVC release
lane. Vulkan verification's RTX lane failed after FreeType download timeouts
and HTTP 502 responses; the later missing Meson build-directory error was
secondary. This does not establish an RTX compile regression. The complete release validation workflow subsequently passed, including
Windows MSYS2 and MSVC x86, Ubuntu x86, Linux i686 regressions and source
validation ([run 34162306724](https://github.com/themuffinator/fnTech3/actions/runs/34162306724)).
Publishing was skipped for this ordinary branch push; provider redistribution
policy remains unresolved even though the inherited packaging checks pass.
