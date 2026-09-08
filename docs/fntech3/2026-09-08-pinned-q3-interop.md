# Bidirectional Q3 interoperability with a revision-pinned peer

Protocol correction: the fnTech3 client / pinned FnQuake3 server selects 71;
the reverse direction selects 68. Earlier claims that both used 68 were
incorrect. See the [handshake audit](2026-09-08-second-fragment-recovery.md).

Both Q3-profile loopback directions pass between the current shared Windows
x86 fnTech3 binaries and a freshly built Windows x64 FnQuake3 reference at
`7d8c9629cdc7979f09697266470359101ffd9eda`, the merger's pinned Q3 baseline.
This removes the peer-revision uncertainty from the earlier reference check.

The reference was built in a new detached worktree and build directory using
MSVC 19.44.35222, Meson debugoptimized, GLx, disabled SDL/cURL/Ogg, and disabled
optional audio/GLx test groups. Meson-owned OpenAL Soft 1.25.2 headers and
libjpeg-turbo 3.1.4.1 fallbacks supplied dependencies. All 441 build steps and
14 configured reference tests pass. No reference source was changed; both the
detached checkout and the user's original FnQuake3 checkout remain clean.
fnTech3 itself was not rebuilt or changed for this evidence-only checkpoint.

| Direction | Observed result | Total probe duration |
| --- | --- | ---: |
| fnTech3 x86 client to pinned FnQuake3 x64 dedicated | Pure admission, CS_ACTIVE, ClientBegin, world/HUD, normal exits | 35.593 s |
| Pinned FnQuake3 x64 client to fnTech3 x86 dedicated | Pure admission, CS_ACTIVE, ClientBegin, world/HUD, normal exits | 35.406 s |

The durations include startup and the scheduled server lifetime; they are not
sustained-play measurements. Both screenshots came from registered engine
screenshotPNG commands and were inspected. The scenes/settings differ, so they
establish rendering reached gameplay rather than visual parity.

All nine stock pak0-pak8 file hashes were rechecked against the recorded fixture.
Each process used a distinct temporary writable home and read-only stock assets,
sv_pure=1, private unswitched desktop, windowed GLx, disabled input/audio, and
loopback addresses. Master-server addresses were empty. Readiness polling stopped
before connection attempts. No user input was injected or captured and no OS
screenshot was used. Retail inputs and screenshot pixels remain outside Git.

The [evidence record](2026-09-08-pinned-q3-interop.json) includes exact peer/source
identity, fresh-build settings, binary and pak hashes, commands/configs, decisive
log lines, and image/log hashes. No external implementation was incorporated.
Shared lineage still limits peer independence. This is reference compatibility,
not retail Steam interoperability closure: the retail startup blocker, protocol
71 coverage, downloads, remote fragmentation/loss/reliability, long sessions, and QL peer
gates remain open. Existing optional resource and reference cvar warnings remain;
no new interop defect was observed.
