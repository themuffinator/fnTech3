# Linux i686 Q3 HTTP runtime

The client and dedicated artifacts from the [current Linux i686 build](2026-09-08-current-linux-i686.md)
complete a loopback HTTP add-on download, reconnect, pure admission and Q3 map
rendering. The engine source is b6b98a6; the later source-archive test correction
does not change these binaries. The [record](2026-09-08-linux-http-runtime.json)
contains executable hashes, commands, configuration, fixture identity and outcomes.

The client logs loading `libcurl.so.4`, resolving its transfer symbols and
entering `CL_cURL_BeginDownload`. With `cl_allowDownload 5`, UDP fallback is
disabled. The HTTP server records exactly one request for the generated
32,916-byte PK3. The installed client copy matches the server SHA-256. The
server records no UDP download, an initial connection, one too-soon reconnect
rejection, then a successful reconnect followed by the pure checksum command
and `CS_ACTIVE`. Both processes exit with code zero without forced termination;
the complete probe lasts 51.871 seconds.

The engine screenshot shows the q3dm6 world, weapon and readable HUD. This uses
legitimate read-only Q3 assets, isolated writable homes, a private Xvfb display,
software OpenGL, windowed mode and disabled input. No OS screenshot or user input
control was used. Raw logs and the screenshot remain in ignored local output.
Only the generated payload is transferred.

The record separately identifies the available system i386 cURL library. Its
resolved path and hash are installation observations; the runtime log establishes
the loaded SONAME, without tracing the resolved file path. The existing full
Linux suite result remains 109 passing checks and two skips; no production code
changed and the suite was not repeated for this evidence-only checkpoint.

This is one Linux Q3 HTTP success path, not HTTPS, HTTP failure/fallback coverage,
independent retail-peer interoperability or a Linux QL client result. The retail
QL installation lacks Linux cgame/UI modules. Existing image/font diagnostics
remain, and broader packaging, renderer and service gates stay open. No new
unrelated defect was established by this probe.
