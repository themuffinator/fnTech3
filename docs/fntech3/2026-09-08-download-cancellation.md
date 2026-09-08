# Cancel an active manual HTTP download and retry

Both Q3 and QL pass a bounded cancellation/retry probe using the shared Windows
x86 client from the [initialization cleanup checkpoint](2026-09-08-download-begin-cleanup.md).
The [record](2026-09-08-download-cancellation.json) verifies that the client and
dedicated hashes match those already tested artifacts. No production source or
binary changes are made for this checkpoint, and the full suite is not repeated.
Its preceding result remains 110 passing checks, one skip and zero failures,
with 1,292 aggregate tests and five skips.

The first HTTP response declares the complete generated PK3 size but sends only
16,384 bytes and keeps the response open. The probe observes a 16,384-byte
temporary file before the queued `download -` command cancels the transfer.
It then issues a second download in the same process. At that second HTTP
request, neither a final archive nor a matching temporary file exists. The
first response is still held open, distinguishing cancellation from natural
server completion. The second response sends one byte, pauses half a second,
then sends the remainder; the installed archive matches the server fixture.

Each profile makes exactly two HTTP requests and reports one successful
download, without an HTTP transport error or stale busy state. All clients and
companion dedicated processes exit normally; probe lifetimes are 35.360 seconds
for Q3 and 35.891 seconds for QL. The companion server prepares the generated
fixture but receives no client connection. This establishes manual cancellation,
temporary-file removal and same-process retry, not automatic-download Escape
handling, pure admission or independent retail interoperability.

The probes use read-only legitimate installations, isolated writable homes,
windowed GLx, disabled input and unswitched private desktops. Cancellation is a
queued engine command; no keyboard/mouse control is used. Existing engine
screenshot output and raw logs remain ignored and are not used to claim gameplay
or menu completeness. No OS capture is used. Existing UI/image/font limitations
remain; no new unrelated defect was established. Physical storage failures and
other cancellation timings still require separate evidence.
