# Q3 pinned-peer interoperability under UDP impairment

Protocol correction: the fnTech3 client / pinned FnQuake3 server selects 71;
the reverse direction selects 68. Earlier claims that both used 68 were
incorrect. See the [handshake audit](2026-09-08-second-fragment-recovery.md).

Both Q3-profile directions reached pure admission, CS_ACTIVE and ClientBegin
under controlled IPv4 loopback packet loss, delay and duplication. The peers
are the unchanged Windows x86 fnTech3 binaries and the clean Windows x64
FnQuake3 build described in the [pinned-peer checkpoint](2026-09-08-pinned-q3-interop.md).

The independently authored `tests/udp_impairment_proxy.py` forwards payloads
unchanged and preserves connectionless traffic. In each direction it drops
every 17th sequenced datagram, delays every 19th non-dropped datagram by 75 ms,
and duplicates every 23rd non-dropped datagram after 10 ms. Scheduling depends
on arrival order. It accepts one client and binds only IPv4 loopback sockets.

| Client / server | To server: drop / delay / duplicate / reordered forwards | To client: same counters | Probe duration |
| --- | --- | --- | ---: |
| fnTech3 / pinned FnQuake3 | 35 / 30 / 25 / 37 | 21 / 17 / 15 / 16 | 35.250 s |
| Pinned FnQuake3 / fnTech3 | 35 / 30 / 25 / 42 | 21 / 17 / 15 / 16 | 35.766 s |

Reordered forwards count decreases in the forwarded sequence/fragment-offset
pair, including delayed duplicates; the counter is not sequence-wrap aware.
The separate `overtakes` counter records sends while packets are pending and
must not be interpreted as proof of reordered delivery. Both relays stopped
without errors or pending packets. Three server-to-client fragment datagrams
were observed in each run, but targeted fragment loss/recovery is not proven.

Both clients rendered the world, weapon and HUD in inspected engine-generated
screenshotPNG images. All four processes exited normally with code zero.
Durations include startup and the scheduled server lifetime, not sustained
play. Stock pak0-pak8 assets, sv_pure=1, isolated writable homes, windowed GLx,
private unswitched desktops and disabled input/audio were used. Downloads and
master-server advertisements were disabled. No user input or OS capture was used.

The [evidence record](2026-09-08-q3-impaired-interop.json) contains exact binary,
source and fixture identities, command lines/configs, counters, decisive log
lines and log/image hashes. Proprietary inputs and image pixels remain outside
Git. The relay's real-socket regression passes on Windows and WSL Ubuntu.
The Windows Meson suite passes 110 tests with one OpenAL skip; its Python
aggregate runs 1,271 tests with five skips. Final formatting and the added
reordering assertion were followed by focused tests on both platforms.

This strengthens the Q3 reference gate only. Shared lineage, retail startup,
remote/WAN behavior, targeted fragment/reliable/download faults, broader protocol 71 coverage,
QL peer interoperability and long sessions remain open. Existing resource/cvar
warnings recur; the reference client also logged an IPv6 bind warning, while
the tested IPv4 connection succeeded. No new engine defect was observed.
