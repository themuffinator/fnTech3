# Stop downloads after a cURL multi-stack error

Both HTTP polling paths checked completion messages without first rejecting a
failed `curl_multi_perform` result. If no completion message was available, the
next frame could poll the failed stack again. The
[libcurl contract](https://curl.se/libcurl/c/curl_multi_perform.html) says transfer
state is uncertain after this error and cannot continue on the same handles.
This differs from an individual HTTP response error reported by a completion
message.

The connection download path now closes its temporary file, releases its cURL
handles and raises a transport error. The general download path reports the
multi error, cleans up and removes the temporary download, then returns an
inactive result. An automatic download at the connected state also raises a
connection error, matching the existing per-transfer failure policy. Existing
bounded handling of `CURLM_CALL_MULTI_PERFORM` and normal completion processing
remain in place. Both game profiles share these paths.

The executable regression compiles the actual polling prefixes through the
completion-message read boundary, with controlled cURL/storage/error stubs.
It reproduces the old behavior and verifies immediate errors, errors after
retry requests, automatic/manual and connected/disconnected general downloads,
normal pending transfers and both retry limits. It checks cleanup, temporary
removal and that no completion read occurs after a multi-stack error. It passes
on Windows and Linux. It does not inject a fault into a real libcurl runtime or
claim coverage of the remainder of the completion handlers.

Implementation and tests are independently authored. The linked API documentation
is behavioral evidence; no external implementation text is incorporated.

The [validation record](2026-09-08-curl-multi-errors.json) identifies the changed
source and rebuilt shared Windows x86 client. The full configured suite passes
110 checks with one skip and zero failures; the aggregate runs 1,289 tests with
five skips. Q3 and QL self-pairs both complete an HTTP-only generated add-on
transfer with matching archive hashes, reconnect, pass pure admission and reach
active state. Q3 renders world/weapon/HUD and QL renders its native join UI over
the world. Both client/server pairs exit normally without forced termination
in 35.359 and 35.875 seconds respectively. One too-soon reconnect rejection in
each session is followed by successful reconnection.

These probes use isolated homes, read-only legitimate installations, windowed
GLx, disabled input and unswitched private desktops. Screenshots come from the
engine render target and remain ignored with raw logs. Legacy script labels
mention reference peers, but these are explicitly merged-binary self-pairs.
Existing image/font diagnostics remain unresolved. No new unrelated defect was
established; physical storage faults, broader services and independent retail
interoperability remain open.
