# General HTTP downloader URL joining and runtime

The general downloader appended a slash whenever the character at
`URL[strlen(URL)]` was not `/`. That character is the terminator, so a configured
base ending in a slash always acquired a second separator. The executable URL
regression reproduces this error. A pre-correction manual Q3 download also
records the doubled path; its permissive local HTTP server accepted it, so that
session is not reported as a failed download.

The join now checks the last character, with an explicit empty-string guard.
It preserves an existing trailing slash, adds one when absent and retains the
existing `%1` template behavior. The controlled executable test compiles the
production URL-joining block and covers bases with/without a trailing slash,
template substitution, an escaped filename and the empty-string boundary.
Escaping and replacement are stubs in this test; it does not claim to test
libcurl's escaping implementation. The test passes on Windows and Linux.

This path is selected through the registered `download` command, which calls
`Com_DL_Begin` and `Com_DL_Perform` with automatic-download behavior disabled.
It is distinct from the server-negotiated cURL transfer path. The runtime probes
use a generated PK3 and an HTTP server that accepts only the exact expected
path. Successful responses send one byte, wait half a second, then send the
remainder, exercising the general downloader's accumulated-header validation.

The change and regression are independently authored and incorporate no
external implementation text.

The [record](2026-09-08-manual-http-downloads.json) identifies the source change,
artifacts, exact commands/configurations, installation and generated fixture.
Both profiles request the exact path, install an archive matching the generated
fixture and exit normally. Separate sessions first receive an empty HTTP 200,
report an incomplete-header/write error, then issue another manual download in
the same process. At the second request, neither a final archive nor a matching
temporary file exists. Both retries complete the split response, install the
matching archive and exit normally. All companion dedicated processes also quit
normally. The clients never connect to them: this is manual-download evidence,
not client/server interoperability or pure admission.

These probes use isolated writable homes, read-only legitimate installations,
windowed GLx, disabled input and unswitched private desktops. Their engine
screenshots remain ignored with raw logs. Q3 shows its isolated-home CD-key
prompt and QL its startup screen; no credentials are entered and no gameplay or
menu-completeness claim is made. Existing image/font diagnostics and broader
retail service/UI gates remain unresolved. No new unrelated defect was established.

The rebuilt shared Windows x86 client passes the full configured suite: 110
checks pass, zero fail and one skips. The aggregate runs 1,291 tests with five
skips. Packaged evidence file links also validate.
