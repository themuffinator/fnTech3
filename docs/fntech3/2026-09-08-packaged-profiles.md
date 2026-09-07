# Reproducible archive and packaged profiles — 2026-09-08

At `5122b34`, a local Windows x86 stage was assembled from the existing
engine-only install layout, replacing the client, dedicated server, three
renderers and root/WebUI packages with current build artifacts and copying
current release documentation. The inherited tool executable is identified
separately in the file manifest; this was not an independent clean rebuild.
No artifact metadata from the old install stage was reused.

The production stage validator passed. The production ZIP writer then wrote
two archives of the same payload, with every staged file's timestamp changed
between writes. Both archive validators passed and the ZIPs were byte-identical:
7,708,168 bytes, SHA-256
`cd31d0bdc96a3e1ad4af0ab3660057373cff0fba0b73d9dab2c8d4ecec9574c8`.
This verifies deterministic archive metadata for an identical payload, not
reproducibility of separately compiled binaries or across compression versions.

One archive was extracted and used unchanged for sequential Q3 and QL local
pure sessions. Both profiles passed admission, reached active state, received
snapshots, rendered their world/HUD or native join UI and exited normally.
Engine PNGs were visually inspected. Client, dedicated, renderer and package
hashes were checked directly against the ZIP entries. The
[evidence bundle](evidence/2026-09-08-packaged-profiles.json) records all stage
files, exact runtime commands/configs and log/image hashes.

Runs used legitimate installations, isolated writable homes, windowed GLx and
unswitched private desktops with input capture, audio, WebUI and Steam
disabled. No proprietary input, native fixture module, credential or screenshot
was added to the archive or repository. The archive remains a local probe,
not a published release. Broader packaged runtime, platform and service gates
remain open. Existing resource/font warnings remain unresolved.

Separately, [clean release validation at `b60580f`](https://github.com/themuffinator/fnTech3/actions/runs/34168479388)
now passes Windows MSVC/MSYS2 x86, Ubuntu x86, Linux i686 regressions and
source validation. Publication was skipped. That run covers the native pure
selection and shader RGB fixes, but precedes the path-conversion and filesystem
recovery changes represented in this local package. No new unrelated issue
was found.
