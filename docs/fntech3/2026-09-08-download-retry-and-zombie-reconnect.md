# Interrupted download retry and zombie-slot reuse

The interrupted-download probe exposed a QL reconnect failure: a fresh client
process reused the previous UDP endpoint with a new qport, while the server kept
the old zombie slot and allocated another slot. The zombie's disconnect traffic
then reached the replacement connection, which reported an invalid reliable
acknowledgement. The failure reproduced before and after fragment-queue cleanup
alone; reusing the old zombie slot resolves the observed case.

SVC_DirectConnect now permits reuse of a zombie at the exact network address and
port even when qport changes. Live slots still require the matching qport, and
existing challenge, admission and reconnect-rate checks run first. This preserves
separate live sessions while retiring an obsolete connection at the reused
endpoint. It does not waive authentication or other profile checks.

Inspection also found that SV_FreeClient freed queued netchannel messages but
left an actively fragmented message eligible for the zombie's send pump. Cleanup
now cancels its unsent-fragment flag and offsets as well. An executable regression
fails before this change and passes afterward, verifying queued-buffer release,
active-fragment cancellation, repeated cleanup and retained sequence identity.
It passes on Windows and WSL Ubuntu. This cleanup correction alone did not fix
the QL retry; the evidence keeps those observations separate.

Both profiles were tested with shared fnTech3 client/dedicated binaries and
legitimate modules. The server initially throttles the independently authored
[fixture pak](2026-09-08-udp-pak-download.md) to 1 KB/s. The first client exits
normally after a short wait. At that point its final pak is absent and its
temporary file contains 2,048 bytes. A second client process reuses the same
writable home and UDP port, reconnects and downloads again after the server
raises its throttle. Both final 32,916-byte files match the fixture SHA-256
`9cc5e33aac0b21cb32c5e618fa137ad0b448acd507120558ebc56310197b3161`;
the `.tmp` file is absent after success. Both retries reach CS_ACTIVE and all
final-run processes exit normally. A further QL run repeats the successful
partial-file/retry/reconnect outcome with normal exits.

This is restart-from-zero recovery, not byte-range resume. Interrupted temporary
files remain on disk until a later attempt truncates/replaces them; cleanup on
abandonment is not claimed. The first Q3 investigation also exceeded its runner's
server-lifetime deadline despite successful client recovery, so it is preserved
as a forced-stop investigation rather than normal-shutdown evidence. Final
runner lifetimes were corrected. The QL failure attempts required cleanup of the
private client process after its recursive-error shutdown did not exit normally.

The [evidence record](2026-09-08-download-retry-and-zombie-reconnect.json) preserves
the stages, partial/final file identities, source/binary hashes, configs and
decisive logs. These are self-pair tests; inherited runner mode labels and unused
reference artifact metadata are not treated as reference-peer evidence. Engine
screenshots show Q3 world/weapon/HUD and QL join UI/world after successful retry.
No QL team entry is claimed. Runs use windowed GLx, private unswitched desktops,
disabled input/audio/services, empty master endpoints, read-only installations
and isolated homes. No input-device control or OS capture occurred.

Windows x86 and the incremental Linux i686 dedicated build pass. The full
Windows suite passes 110 tests with one OpenAL skip; its Python aggregate runs
1,283 tests with five skips, including enabled native runtime probes. Linux uses the
existing dirty scratch checkout, not a clean release build. Broader reconnect
timing, packet-in-flight races, NAT changes, interrupted HTTP transfers and
independent retail/service interoperability remain open. Existing optional
resource/default/service warnings remain. Recursive-error shutdown in the failed
QL attempts remains a separate diagnostic/lifecycle issue; no general fix for
that path is claimed here.
