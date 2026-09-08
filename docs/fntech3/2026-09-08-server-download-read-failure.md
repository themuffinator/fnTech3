# Stop incomplete server downloads and notify their clients

The server's download read loop treated a negative read as successful EOF and
could queue a zero-length block after a zero-byte read before the advertised
size was reached. It also requested a full block at the end of a transfer, so a
growing file could send bytes beyond its original declared size. The
[client size guard](2026-09-08-download-total-size.md) rejects those messages,
but the server must not manufacture successful completion either.

SV_WriteDownloadToClient now requests at most the remaining advertised bytes.
An incomplete read drops the affected client with an explicit server read error,
using normal cleanup to close the download handle and free buffered blocks.
Other clients and the server process remain on their normal paths. This is
shared Q3/QL logic with unchanged profile window and block-size contracts.

The first live truncation attempts exposed a second defect: downloads return to
CS_CONNECTED after receiving the gamestate, while SV_AddServerCommand previously
suppressed every command below CS_PRIMED. Thus the server dropped the client but
never transmitted its disconnect reason. The command gate now permits only a
`disconnect ` command for a downloading CS_CONNECTED client. Other pre-game
commands remain suppressed. No connection-state promotion is needed.

The executable regression compiles the production read loop and download cleanup
with controlled filesystem results. Negative, zero and short reads abort without
EOF or advancing the failed block, including failure after an earlier buffered
block. It checks cleanup, fixed-size reads when the backing data is longer, and
window-limited data/EOF scheduling. A production command-gate extraction checks
disconnect eligibility across free, zombie, connected, primed and active states.
The pre-change reader fails the new assertion; the final test passes on Windows
and WSL Ubuntu. The test models SV_DropClient by invoking actual download cleanup;
it does not exercise all game-module disconnect callbacks.

The final Windows client/dedicated build and incremental Linux i686 dedicated
build pass. The full Windows suite passes 110 checks with one skip and no
failures; its Python aggregate runs 1,285 tests with five skips.

Live Windows x86 dedicated-server probes use legitimate read-only assets,
isolated homes, input-disabled windowed reference clients on unswitched private
desktops, and generated fixture paks. Normal downloads to both reference clients
match the original 32,916-byte pak hash and reach active state after pure
admission. Truncation probes instead throttle a 262,292-byte generated pak and
truncate only that temporary server file after block zero is transmitted. The
server detects the subsequent short read and neither reference client installs
a final pak. In the corrected runs both client logs contain
`Server disconnected: Download read failed on server`; all processes exit
normally with no forced termination. The evidence retains the first attempts
where the reason was suppressed as well as the corrected runs.

Engine screenshots show the QL disconnect error. The isolated Q3 client instead
shows its stock CD-key prompt after disconnect; its log establishes receipt of
the read-error reason. No CD key or credentials were entered. Q3's reference is
the clean pinned FnQuake3 build; the existing FnQL reference's exact source
revision remains unverified. These tests do not establish independent retail
interop, concurrent unaffected-client behavior, arbitrary disk faults, HTTP
fallback or complete download compatibility.

The [evidence record](2026-09-08-server-download-read-failure.json) records builds,
source/artifact hashes, commands, configs, fixtures, log observations and test
results. The Linux i686 build is an incremental dedicated-server check in the
existing scratch checkout, not a new clean release build. No external
implementation was incorporated. QL reference-client temporary-hunk warnings
remain visible during error recovery/shutdown; this change does not explain or
repair them. The Linux build also retains the unrelated unused `burst` parameter
warning in SVC_BucketForAddress.
