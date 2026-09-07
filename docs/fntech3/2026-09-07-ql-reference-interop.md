# QL reference interoperability — 2026-09-07

At clean checkout `90a8cb2`, the unified Windows x86 client joined an existing
FnQL Win32 dedicated server, and the FnQL Win32 client joined the unified
dedicated server. Both connections negotiated protocol 91, passed pure
admission and reached `CS_ACTIVE`. Each engine screenshot showed the native
QL join UI over the campgrounds world. All four processes exited normally.

The [evidence record](evidence/2026-09-07-ql-reference-interop.json) contains
exact executable/renderer/package hashes, fixture hashes, startup commands,
generated configs and decisive log lines. The reference artifacts came from
the existing `FnQL/meson/build-win32` directory. Their exact source revision is
unknown; the reference checkout's uncommitted work was not changed or claimed
as part of a pinned binary build. Shared lineage limits the independence of
these checks.

Runs lasted 18.391 and 17.890 seconds respectively, including startup and
scheduled server shutdown; these are not sustained gameplay durations. The
same fnTech3 x86 binaries served both roles without profile-specific rebuilding.
Homes were isolated and writable; legitimate installations remained read-only.
Clients used an unswitched private desktop, windowed GLx and disabled input
capture/audio. Images came from engine `screenshotPNG`, with no OS capture or
user input injection. No proprietary inputs were committed.

Steam integration, Workshop and WebUI were disabled. These probes establish
bounded protocol/pure/native-module reference interoperability, not Steam
ticket admission, service success or interoperability with the retail QL
executable. The screenshot stops at the join UI; no mouse or keyboard action
was used to enter a team. Remote loss/fragmentation/download behavior, long
sessions, authentication, WebUI and broader gameplay remain open.

Local logs and screenshots remain in `.tmp/ql-reference-{direction}-initial/`.
Existing resource warnings are unchanged. Separately, complete release CI at
`49fc484` passed ([run 34164825038](https://github.com/themuffinator/fnTech3/actions/runs/34164825038)),
covering snapshot/entity-history changes through that commit; later demo
isolation still needs its own clean CI evidence.
