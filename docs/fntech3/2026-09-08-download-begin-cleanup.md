# Clean up rejected download initialization

The general downloader creates its cURL easy handle and URL before stripping
the archive extension from the requested name. A name consisting only of
`.pk3` or `.pak` leaves an empty basename. That rejection returned without
cleanup, so the URL and easy handle still made `Com_DL_InProgress` report busy
despite there being no multi handle or valid transfer.

A pre-correction Q3 runtime probe issues `download .pk3` followed immediately by
a valid download command. The log records the empty-name rejection, rejects the
valid command as already downloading, then reports an invalid multi handle on
the next frame. The existing multi-error handler eventually cleans up, but the
valid command has already been lost; no archive is installed. Both processes
quit normally, so this is not a permanent-hang claim.

The empty-basename branch now calls `Com_DL_Cleanup` before returning. The
executable regression compiles the actual rejection guard, in-progress predicate,
cleanup and library-release functions. With controlled partial initialization,
it reproduces the retained handle/busy state before the fix. Afterward it checks
easy-handle release, optional loaded-library release, cleared URL/extension,
inactive state and idempotent repeated cleanup. It passes on Windows and Linux.
The fixture controls cURL/platform operations; it does not simulate every
allocation failure in the complete initialization function.

The correction and test are independently authored. No external implementation
text was incorporated.

The [validation record](2026-09-08-download-begin-cleanup.json) retains the failed
baseline, source/artifact identities and corrected Q3/QL commands. Both corrected
profiles reject the extension-only name, immediately request the valid archive
and install a copy matching the generated fixture. Each sends exactly one HTTP
request; neither logs the old busy or invalid-multi-handle error. All clients
and companion servers exit normally. The valid response retains the one-byte,
half-second-delay split-header behavior from the previous checkpoint.

These are download-only sessions, not pure-admission or interoperability tests.
They use legitimate read-only installations, isolated writable homes, windowed
GLx, disabled input and unswitched private desktops. Raw logs and engine
screenshots remain ignored; no OS capture or user input control was used.
Existing Q3 isolated-home CD-key UI, QL startup/UI limitations and image/font
diagnostics remain. No new unrelated defect was established. Other partial
initialization and physical allocation-failure cases require separate evidence.

The rebuilt shared Windows x86 client passes the full configured suite: 110
checks pass, zero fail and one skips. The aggregate runs 1,292 tests with five
skips. Packaged evidence file links also validate.
