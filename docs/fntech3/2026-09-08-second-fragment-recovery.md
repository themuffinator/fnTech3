# Second-fragment recovery and protocol evidence correction

All four reference-peer configurations recover when the second server gamestate
fragment is dropped. The relay records sequence 1, offset 1300 in every case;
the first fragment is forwarded. Servers report gamestate resend, then
acknowledgement and CS_ACTIVE; clients report pure admission. All processes
exit normally and relays stop without errors or pending packets.

| Client / server | Negotiated protocol | Dropped datagram bytes | Duration |
| --- | --- | --- | --- |
| fnTech3 / pinned FnQuake3 | 71 | 1312 | 35.641 s |
| Pinned FnQuake3 / fnTech3 | 68 | 1308 | 35.891 s |
| fnTech3 / existing FnQL | 91 | 1308 | 35.641 s |
| Existing FnQL / fnTech3 | 91 | 1308 | 35.844 s |

The extra four bytes in the first direction exposed a recorder and documentation
error: pinned FnQuake3 advertises 68 in its info response but selects 71 during
challenge/connect. Earlier pinned-peer, periodic-impairment and first-fragment
checkpoint prose incorrectly described both Q3 directions as protocol 68.
Their original client logs confirm 71 in this direction and 68 in the reverse.
The evidence record now includes those retrospective handshake lines and hashes.
Source inspection confirms the selected 71 netchannel includes a sequence
checksum before fragment metadata; an info response alone cannot prove selection.

The relay formerly interpreted checksum bytes as the fragment offset. Its
drop-first-fragment policy and payload forwarding did not depend on that offset,
so the recorded recovery outcomes remain valid; checksummed fragment-offset and
within-sequence ordering metadata from those older runs must not be relied on.
The relay now accepts explicit sequence-checksum framing and a positive fragment
ordinal. Four real-socket regressions pass on Windows and WSL Ubuntu, verifying
first/second-fragment selection, checksummed offsets, unchanged surviving bytes,
independent directions and the existing periodic impairment policy. The corrected
Q3 checksummed run was repeated successfully. Other new runs used unchanged
non-checksummed framing. Protocol 71 is therefore observed here, but complete
protocol-71 demo, download and compatibility claims remain unproven.

Inspected engine screenshots show Q3 world/weapon/HUD and QL join UI/world.
No QL team entry is claimed. Private unswitched desktops, windowed GLx, disabled
input/audio/services/downloads/master publication and isolated writable homes
were used. No user input control or OS capture occurred. Proprietary inputs and
raw screenshots remain outside Git. These are bounded IPv4-loopback admission
probes, not long sessions or WAN/gameplay stress.

The [evidence record](2026-09-08-second-fragment-recovery.json) retains executable,
fixture, relay and log identities, commands/configs, negotiated-protocol lines,
packet counters and image hashes. Engine source/binaries are unchanged. The Q3
reference is pinned at 7d8c9629; the existing FnQL reference source revision is
unknown. Shared lineage still limits peer independence. Terminal-fragment loss,
client fragmentation, downloads and independent retail/service interoperability
remain open. Existing resource/default warnings recur; no new engine defect was
observed. The unrelated earlier protocol/recorder inaccuracies are corrected here.
