# Q3 initial gamestate fragment-loss recovery

Protocol correction: the fnTech3 client / pinned FnQuake3 server selects 71;
the reverse direction selects 68. Earlier claims that both used 68 were
incorrect. See the [handshake audit](2026-09-08-second-fragment-recovery.md).

Both Q3-profile peer configurations recover when the first server-to-client
gamestate fragment is deliberately dropped. The unchanged shared Windows x86
fnTech3 binaries were tested against the clean Windows x64 FnQuake3 build at
`7d8c9629cdc7979f09697266470359101ffd9eda`; see the
[reference build checkpoint](2026-09-08-pinned-q3-interop.md).

The loopback relay now accepts `drop_first_fragment=True`, independently per
direction. These runs set `drop_every`, `delay_every` and `duplicate_every` to
zero. Connectionless traffic and surviving payloads remain unchanged. The
relay records the dropped packet's sequence, fragment start and datagram size,
without saving payload bytes.

| Client / server | Dropped sequence / fragment start / bytes | Server fragment packets received | Probe duration |
| --- | --- | ---: | ---: |
| fnTech3 / pinned FnQuake3 | 1 / 0 / 1312 | 6 | 35.313 s |
| Pinned FnQuake3 / fnTech3 | 1 / 0 / 1308 | 6 | 35.766 s |

Each server logged `dropped gamestate, resending`, then acknowledged gamestate,
CS_ACTIVE and ClientBegin. Each client logged pure-server admission. Inspected
engine screenshotPNG outputs show a rendered world, weapon and HUD. All four
processes exited normally with code zero; relays stopped without errors or
pending packets. This establishes recovery through gamestate resend, not an
assertion that the netchannel retransmits individual fragments.

No client-to-server fragment packets occurred, so that live direction is still
unverified. A real-socket regression on Windows and WSL Ubuntu verifies the
relay drops only the first fragment independently in both directions, preserves
connectionless traffic and forwards an identical subsequent fragment unchanged.
The existing loss/delay/duplication regression also passes. The full Windows
Meson suite passes 110 tests with one OpenAL skip; its Python aggregate runs
1,272 tests with five skips, including the opt-in Q3 and QL native probes.

Both runs used IPv4 loopback, stock pak0-pak8 (hashes reverified), sv_pure=1,
isolated temporary writable homes, disabled input/audio, private unswitched
desktops and windowed GLx. Downloads and master-server advertisements were
disabled. No user input control or OS screenshot was used. Durations include
startup and the scheduled server lifetime, not sustained play.

The [evidence record](2026-09-08-q3-fragment-recovery.json) includes exact binary,
source and fixture identities, commands/configs, packet counters, decisive log
lines and log/image hashes. Proprietary inputs and screenshot pixels stay out
of Git. Engine source and binaries were unchanged for this checkpoint.

This adds a targeted fault case to the broader
[impairment checkpoint](2026-09-08-q3-impaired-interop.md). Middle/terminal-fragment
loss, sustained reliable traffic, downloads, broader protocol 71/91 coverage, remote/WAN behavior
and retail peer interoperability remain open. Shared reference lineage remains
a limitation. Existing optional-resource and reference cvar warnings recur;
no new engine defect was observed.
