# Protocol 91 joining under controlled UDP impairment

The unchanged fnTech3 Windows x86 binaries at `a61b8bd` pass both client/server
directions against the existing FnQL Win32 reference artifacts under periodic
packet loss, delay and duplication, and separately under initial gamestate
fragment loss. Each of the four connections reaches CS_ACTIVE after pure
admission. All eight client/server processes exit normally with status zero.

| Fault case | Client / server | Dropped to server / client | Duration |
| --- | --- | --- | --- |
| Periodic impairment | fnTech3 / FnQL | 35 / 40 | 36.328 s |
| Periodic impairment | FnQL / fnTech3 | 35 / 41 | 35.781 s |
| First gamestate fragment | fnTech3 / FnQL | 0 / 1 | 35.313 s |
| First gamestate fragment | FnQL / fnTech3 | 0 / 1 | 35.406 s |

The loopback relay preserves connectionless messages and surviving payloads.
Periodic settings drop every 17th sequenced datagram, delay every 19th by 75 ms
and duplicate every 23rd after 10 ms. Both runs record delayed, duplicated and
reordered forwards in both directions. Their four received server fragments
per run do not alone establish targeted fragment recovery.

The separate fragment cases disable periodic faults and drop the first fragment
independently per direction. Both runs drop server sequence 1, offset 0, size
1,308 bytes. Each server reports `dropped gamestate, resending`, then gamestate
acknowledgement and CS_ACTIVE. Each relay receives eight server fragments and
zero client fragments. This is recovery by gamestate resend, not a claim of
per-fragment retransmission or client-to-server fragmented-message coverage.

Inspected engine screenshotPNG outputs show the native QL join UI over the
campgrounds world in all four cases. They establish joined-client snapshot/render
delivery, not team entry or active gameplay. Durations include startup and the
scheduled server lifetime; they are not sustained gameplay durations. The
startup wait queue is acceptable for this fixed-map admission probe, but cannot
establish game-requested restart or map-rotation behavior.

Reference binary hashes match the [previous QL checkpoint](2026-09-07-ql-reference-interop.md).
Their precise source revision remains unknown, and shared lineage limits peer
independence. The reference checkout was not changed. This is not interoperability
with the retail executable or authenticated Steam-ticket admission.

The [evidence record](2026-09-08-ql-impaired-interop.json) contains binary/package,
relay/probe and asset hashes, commands/configs, packet counters, decisive log
lines and screenshot/log hashes. Legitimate asset hashes were revalidated. Runs
used isolated homes, private unswitched desktops, windowed GLx, input/audio off,
Steam/Workshop/WebUI off, downloads off and empty master endpoints. No user input
control, OS capture or proprietary redistribution occurred. Relays stopped with
no errors or queued packets. The existing real-socket relay tests pass on Windows
and WSL Ubuntu (two each). Engine source/binaries are unchanged, so no rebuild or
full-suite rerun was needed for this evidence-only checkpoint.

Middle/terminal fragment loss, client fragmentation, reliable/download stress,
WAN behavior, active gameplay and bidirectional retail/service interoperability
remain open. Existing optional-resource and reference default-registration
warnings recur; no new engine defect was observed.
