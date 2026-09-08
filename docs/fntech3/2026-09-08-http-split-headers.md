# Validate HTTP archive headers across callbacks

The HTTP write callbacks previously applied `CL_ValidPakSignature` to the first
callback alone. That validator requires 22 bytes for ZIP or 12 for PAK, while
[libcurl permits a one-byte body callback](https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html).
A controlled HTTP response sending one byte, waiting half a second, then sending
the rest reproduced rejection of a valid generated PK3 in the Q3 client at
c3f73df. The client reported an invalid signature and required the probe's
bounded cleanup; it did not install the archive.

Both HTTP paths now share a bounded header accumulator. It holds at most 22
bytes, applies the unchanged signature rules as data arrives, then opens the
temporary file and writes the buffered prefix once followed by the remaining
body. These fields are engine-internal transfer state, outside module and wire
contracts, and are reset during transfer cleanup. UDP validation is unchanged.
Invalid headers and failed opens/writes return failure to libcurl instead of
raising a connection error from inside the write callback. Completion rejects
an empty or insufficient header before any archive rename, including when an
older temporary file already exists.

The executable tests compile the production helper, both callbacks and the
existing signature validator. They exercise every chunk size from one byte to
the complete fixture for normal ZIP, empty-ZIP signatures and a minimal PAK
header; they compare exact accumulated output. Controlled opening and first,
second and later write failures must abort, and invalid/zero-length callbacks
must not open a file. Additional tests compile the completion guards for every
prefix length through 22 bytes, requiring invalid prefixes to stay outside the
rename branch and leaving libcurl's original completion message unchanged.
These tests validate signature handling, not the complete archive parser or
physical storage durability.

The implementation is independently authored. The API documentation is used as
behavioral evidence; no external implementation text was incorporated.

The [validation record](2026-09-08-http-split-headers.json) retains the failed
baseline and corrected artifact identities. The focused tests pass on Windows
and Linux. The shared Windows x86 client rebuild passes, followed by the full
suite: 110 passing checks, zero failures and one skip; the aggregate runs 1,290
tests with five skips.

With the same delayed one-byte response and UDP fallback disabled, Q3 and QL
self-pairs now install matching generated PK3s, reconnect, pass pure admission
and reach active state. Engine screenshots show Q3 world/weapon/HUD and QL's
native join UI over the world. All four processes exit normally, with probe
lifetimes of 35.328 and 35.625 seconds. These are merged-binary self-pairs even
though inherited local script labels mention reference peers.

Separate empty-body probes in both profiles leave a pre-existing 20-byte
placeholder in its temporary location and install no final archive. They report
an incomplete header. Their clients require forced cleanup at the 50-second
bound; the servers quit normally. As established by the earlier command-buffer
investigation, ERR_DROP clears the pending test-script commands, so these probes
establish rejection and non-promotion, not normal error-path shutdown or a hung
client loop. The general downloader's completion path is covered by the controlled
executable guard tests, not a separate general-downloader runtime session.

Runtime probes use isolated writable homes, read-only legitimate installations,
windowed GLx, disabled input and unswitched private desktops. Raw logs and engine
screenshots remain ignored. No OS capture or keyboard/mouse control was used.
Existing image/font diagnostics remain; no new unrelated defect was established.
Broader HTTP faults, full archive parsing, storage durability, services and
independent retail interoperability still require their own evidence.
