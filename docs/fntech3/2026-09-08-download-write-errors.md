# Propagate download storage failures

The UDP download handler and both HTTP write callbacks ignored `FileWrite`'s
return value. `FS_Write` can return zero after failing to write, including
after a partially successful underlying write. UDP would acknowledge and
advance the block; HTTP would report the entire buffer consumed. A later
checksum failure does not make those acknowledgements correct.

UDP now drops on a write result different from the chunk size before advancing
its counters or acknowledging receipt. The HTTP callbacks return the actual
positive write count, or zero for a failed/nonpositive result, allowing
libcurl to reject an incomplete write. The existing disconnect/HTTP cleanup
paths own closing handles. This does not promise durable storage after a
successful buffered write or repair separate close/rename failures.

## Verification

The extended `download_chunk_tests.py` and new `download_write_tests.py`
reproduced the ignored-write failures before the correction. Both pass on
Windows and Linux afterward. The former runs the production UDP handler under
both selected profiles with zero/partial writes; it checks that no block/count
advance, acknowledgement, archive rename or next-download action occurs. The
latter runs both production HTTP callbacks with negative, zero, partial and
complete results for newly opened and existing handles. Storage operations
are controlled stubs: these tests do not fill a physical disk or inject an
operating-system device failure.

The change is independently authored. The [evidence record](2026-09-08-download-write-errors.json)
contains build/suite results and bounded runtime non-regression probes. Retail
map data used in temporary loopback download fixtures remains local and is
deleted with the isolated homes; it is not committed or published. Runtime
probes force windowed GLx and disable input capture on an unswitched private
desktop. Visual evidence uses engine screenshots.

The shared Windows x86 client rebuild and complete Meson suite pass: 110
passes, zero failures and one unavailable OpenAL-loopback skip. The aggregate
Python suite runs 1,256 tests with seven skips. Q3 completes separate UDP and
HTTP-only transfers of the temporary add-on, with each downloaded SHA-256
matching the server fixture, followed by pure admission and world/HUD
rendering. The passing HTTP probe sets `cl_allowDownload 5` to prevent UDP
fallback and uses an isolated local HTTP server. QL's pure non-regression session also
renders its native join UI/world. All clients and servers exit normally.
These runtime sessions verify successful transfers; storage failures are
established by the controlled executable tests above.

The first HTTP probe completed its transfer but the screenshot remained at
"Awaiting gamestate" after the server-observed disconnect; it is not counted
as a gameplay pass. The follow-up uses `sv_allowDownload 9`, the supported
HTTP no-disconnect flag, to separate transfer behavior from reconnect timing.
The subsequent automatic-reconnect probe below resolves this observation.

## Automatic HTTP reconnect follow-up

At `3aeba05`, the same shared x86 binaries passed Q3's normal HTTP
disconnect/reconnect path with `sv_allowDownload 1` and `cl_allowDownload 5`
(HTTP enabled, UDP fallback disabled). The server observed two connections,
with the second reaching active state after pure admission. The downloaded
archive hash matched the temporary server fixture, snapshots continued, the
engine screenshot showed world/HUD gameplay and both processes exited normally.
Exact configurations, hashes and excerpts are in the
[reconnect record](2026-09-08-http-reconnect.json).

The earlier script had queued a long wait, screenshot and quit immediately
after connecting. `CL_Reconnect_f` appends its connect command to that same
buffer, behind those pending actions. The corrected probe leaves the buffer
free after connecting and uses the existing `activeAction` cvar to schedule
the screenshot and quit after the first gameplay frame. This is a probe
correction, not an engine behavior change. It validates local Q3 automatic
HTTP reconnect; QL HTTP reconnect and remote network-fault cases remain open.
No new unrelated engine issue was found.

Physical storage faults, buffered flush failures, interrupted-transfer
recovery and remote retail-peer coverage remain open. Existing unrelated
image/font warnings remain unresolved.
