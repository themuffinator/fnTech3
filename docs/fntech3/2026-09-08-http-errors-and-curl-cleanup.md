# HTTP error handling and cURL ownership at disconnect

The shared client chooses UDP if HTTP redirection is unavailable, disabled or
cannot initialize. Once a cURL transfer begins, an HTTP/transfer error raises
ERR_DROP; it does not retry that transfer over UDP. This checkpoint verifies
that existing policy rather than adding an automatic fallback.

Inspection found a separate ownership defect: CL_Disconnect cleared `clc`,
including its cURL easy/multi pointers, before the later CL_ShutdownAll call
could free those handles. The disconnect path now calls CL_cURL_Cleanup before
clearing connection state. The existing early return that preserves a connection
during a game switch remains in place. The cURL library itself still follows its
existing shutdown lifecycle.

An executable regression extracts the actual disconnect/reset sequence and
CL_cURL_Cleanup implementation. It tracks live easy/multi handles and rejects
resetting connection state while either remains live. It exercises both handles,
easy-only, multi-only and neither, plus repeated cleanup without double release.
The pre-change sequence fails; the fixed sequence passes on Windows and WSL
Ubuntu. This is controlled ownership evidence, not a process-wide leak measurement
or every libcurl callback/reentrancy path.

The Windows x86 client build passes. The full Windows suite passes 110 checks
with one skip and no failures; its Python aggregate runs 1,286 tests with five
skips. Linux validation in this slice is the focused executable regression, not
a new full Linux client build.

Loopback HTTP probes use a server-only generated 32,916-byte pak with no retail
payload. Both profiles use the same merged Windows x86 binaries, isolated homes,
windowed clients with input disabled and unswitched private desktops. The HTTP
server either returns 404, or declares the full content length and closes after
2,048 bytes. Each condition was exercised before and after the cleanup fix.
Every error run records exactly one HTTP request, the corresponding client error,
no UDP file transfer, and no final installed pak. The 404 leaves no partial file;
the truncated response leaves a 2,048-byte temporary file.

These error runs are **not normal-exit passes**. The client remained open after
error recovery and was terminated by the owning probe at its 50-second deadline;
the dedicated server exited normally. A follow-up harness wrote a shutdown script
to the isolated base-game profile config when HTTP was requested, but that script
also did not execute. The earlier explanation that base-game config reload alone
accounted for the missing scripted quit was an inference, not established by
those attempts. Both sets of results are retained. No fatal-error or UI-dismissal
success is claimed, and the cleanup fix does not claim to repair that automation
limitation. No mouse/keyboard input or OS screenshots were used.

Successful HTTP controls with the rebuilt binaries pass for both profiles:
the generated pak matches its server hash, automatic reconnect reaches active
state after pure admission, and both client and server exit zero without forced
termination. Engine screenshots show Q3 world/HUD and QL native join UI over the
world. Their total probe lifetimes are 35.359 and 35.343 seconds respectively,
not transfer durations. HTTP errors still deliberately do not fall back to UDP.

The [evidence record](2026-09-08-http-errors-and-curl-cleanup.json) includes source,
artifact and fixture hashes, commands/configs, exact HTTP outcomes, before/after
regression and process outcomes. No external implementation was incorporated.
Error-path automated shutdown, retained partial-file disposal, arbitrary storage
faults and independent retail interoperability remain open. Existing QL hunk
warnings and missing image/font diagnostics remain unresolved; no unrelated
engine correction is claimed.
