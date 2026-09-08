# Terminal gamestate-fragment recovery

Four reference-peer configurations recover after loss of the terminal server
gamestate fragment. Earlier full fragments are forwarded unchanged. Every server
reports gamestate resend, acknowledgement and CS_ACTIVE; every client reports
pure admission. All eight client/server processes exit normally with status zero.

| Client / server | Protocol | Terminal offset / payload bytes | Duration |
| --- | --- | --- | --- |
| fnTech3 / pinned FnQuake3 | 71 | 2600 / 365 | 35.766 s |
| Pinned FnQuake3 / fnTech3 | 68 | 2600 / 173 | 35.859 s |
| fnTech3 / existing FnQL | 91 | 3900 / 537 | 36.438 s |
| Existing FnQL / fnTech3 | 91 | 3900 / 538 | 35.781 s |

All dropped packets belong to sequence 1. The relay identifies a terminal
fragment by the profile-appropriate header followed by a declared payload
shorter than the 1300-byte full-fragment size, with an exact datagram-length
match. It drops only the first matching terminal fragment per direction.
Periodic faults are disabled. No client-to-server fragment traffic occurred;
that live direction remains unverified.

The relay's five real-socket tests pass on Windows and WSL Ubuntu. They cover
nonempty and zero-length tails, full-fragment preservation, malformed declared
lengths and short headers, checksum framing, independent directions, subsequent
identical retransmission, and previous first/second/periodic fault selectors.
Zero-length terminators are covered by the relay regression only; the actual
game sessions above produced nonempty tails. These probes demonstrate recovery
by gamestate resend, not a netchannel guarantee of per-fragment retransmission.

Inspected engine screenshotPNG outputs show Q3 world/weapon/HUD and QL join
UI/world. No QL team entry or long gameplay duration is claimed. Runs use
private unswitched desktops, windowed GLx, disabled input/audio/services/downloads,
empty master endpoints and isolated writable homes. No user input control or OS
capture occurred. Proprietary inputs and screenshot pixels remain outside Git.
All relays stop with no errors or pending packets.

The [evidence record](2026-09-08-terminal-fragment-recovery.json) preserves peer,
engine, asset, probe, relay and log hashes, command/config details, negotiated
protocol lines, packet counters and image hashes. Engine source/binaries are
unchanged. The Q3 reference is pinned at 7d8c9629; the FnQL reference source
revision remains unknown. Shared lineage limits peer independence.

Together with the [second-fragment checkpoint](2026-09-08-second-fragment-recovery.md)
and first-fragment probes, this adds bounded position-specific gamestate recovery
evidence. It does not close client fragmentation, downloads, repeated/multiple
fragment loss, broader reliable traffic, WAN stress or independent retail/service
interoperability. Existing optional-resource/reference default warnings recur;
no new unrelated engine defect was observed.
