# Client fragmentation and reliable-command recovery

All four reference-peer configurations deliver a bounded batch of six reliable
client commands after loss of the first client fragment. Each complete command
matches its generated configuration, in order and exactly once in the server's
reliable-command dispatch log. Observed reliable sequence numbers are 3 through
8 in every run. The client remains connected and renders after the batch.

| Client / server | Protocol | Client fragment packets received | Dropped packet bytes | Duration |
| --- | --- | ---: | ---: | ---: |
| fnTech3 / pinned FnQuake3 | 71 | 15 | 1314 | 35.266 s |
| Pinned FnQuake3 / fnTech3 | 68 | 10 | 1310 | 35.281 s |
| fnTech3 / existing FnQL | 91 | 15 | 1310 | 35.422 s |
| Existing FnQL / fnTech3 | 91 | 10 | 1310 | 35.437 s |

The relay drops client sequence 180, fragment offset 0, payload length 1300
in all four cases. It preserves server fragments and connectionless traffic;
periodic impairment is disabled. The received-fragment totals include subsequent
traffic and are not claimed as counts of per-fragment retransmissions. Recovery
is demonstrated by intact reliable command delivery after loss.

The client configuration waits 180 frames after initiating connection, then
queues six harmless unknown commands named fntech3_fragment_probe_0 through _5.
Each has an 880-character deterministic alphanumeric argument. The shared-frame
batch makes the engine fragment its outgoing reliable-command message. The
private server disables command flood protection for this bounded batch. No
keyboard/mouse injection is used. Unknown game commands are intentional; the
test checks transport and reliable dispatch, not gameplay acceptance of them.

All connections first pass pure admission and reach CS_ACTIVE. Subsequent engine
screenshotPNG images show Q3 world/weapon/HUD or QL join UI/world. No QL team entry
is claimed. All eight processes exit normally with status zero. Relays stop
without errors or pending packets. Durations include startup and scheduled
shutdown, not sustained gameplay.

The relay now supports a direction constraint for fragment faults. Its six
real-socket tests pass on Windows and WSL Ubuntu, including the new check that a
client-only fault preserves server fragments, plus all previous loss and framing
checks. Engine code/binaries are unchanged in this checkpoint.

The [evidence record](2026-09-08-client-fragment-recovery.json) contains exact
command/configuration and peer identities, relay/probe hashes, packet counters,
command sequence/length/hash records, decisive logs and image hashes. Q3 uses the
clean reference pinned at 7d8c9629; the existing FnQL reference source revision
is unknown. Shared lineage still limits independence. Runs use read-only legitimate
assets, isolated writable homes, private unswitched desktops, windowed GLx,
disabled input/audio/Steam/Workshop/WebUI/downloads and empty master endpoints.
No OS capture or proprietary redistribution occurred.

This adds live client-fragment evidence to the prior server-gamestate probes.
Client middle/terminal/multiple-fragment loss, reliable-ring saturation,
downloads, reconnect/long-session/WAN stress and independent retail/service
interoperability remain open. Existing optional-resource/default warnings and
the expected unknown-command responses occur; no new unrelated defect was found.
