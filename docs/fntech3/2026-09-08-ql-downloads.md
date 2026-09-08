# QL add-on downloads and automatic HTTP reconnect

At `411d1a7`, the shared Windows MSVC x86 client and dedicated binaries passed
separate QL UDP and HTTP-only add-on download sessions. The temporary server
archive `baseq3/zz-fntech3-download-probe.pk3` contains `maps/campgrounds.bsp`
from the legitimate local retail QL `pak00.pk3`. It is an additional archive,
not a replacement for the protected retail pak. Neither the source map nor
the temporary archive is committed or published; temporary homes and copies
are removed after each probe.

Both downloaded archives matched their server fixture SHA-256. UDP completed
through the engine's download protocol. HTTP used a loopback HTTP server with
`cl_allowDownload 5` to disable UDP fallback and `sv_allowDownload 1` to allow
the normal disconnect/reconnect path. The latter produced two server-observed
connections, with the second reaching active state after pure admission. UDP
produced one connection and also reached active state.

Both sessions continued receiving snapshots, rendered the native QL join UI
over the world, and exited normally on client and server. Exact command lines,
startup scripts, binary hashes, archive hashes and excerpts are in the
[evidence manifest](evidence/2026-09-08-ql-downloads.json). The scripts use
`activeAction` to schedule engine screenshots and quit after play starts,
leaving the automatic reconnect command free to execute. Windowed GLx,
isolated writable homes, disabled input capture and an unswitched private
desktop preserve the established runtime restrictions.

This extends the [Q3 result](2026-09-08-download-write-errors.md) to QL using
the same binary set. It is local engine-to-engine evidence, not retail-peer
or remote-network closure. HTTP errors, interrupted transfers, storage faults
and broader download/profile-isolation cases remain open. No engine source
change was needed. Existing unrelated image/font warnings remain unresolved.

## Hosted CI availability

The hosted [release run at this commit](https://github.com/themuffinator/fnTech3/actions/runs/34171987676)
and [Linux verification at 3aeba05](https://github.com/themuffinator/fnTech3/actions/runs/34171857523)
failed before their jobs started. GitHub check annotations identify failed
account payments or a spending limit as the reason. The inspected annotations
are check IDs `101893912929` (release prepare) and `101893589345` (Linux Meson).
They do not report a compilation or test failure. Current commits therefore
lack completed hosted CI evidence; earlier green runs and local verification
must not be presented as current hosted results. Account billing/spending
settings require the account owner's attention before these CI gates can run.
