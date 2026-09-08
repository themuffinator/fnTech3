# Retail QL authenticated admission

On 2026-09-08, the installed retail Quake Live client joined the Windows x86
fnTech3 dedicated server at `1c1eeac` with the local Steam provider enabled.
The server observed platform authentication validation, pure admission and
`CS_ACTIVE`. An engine screenshot shows the Campgrounds world and native
Join Match panel. Both processes exited 0 normally in a 26.188-second pair.
This is a short authenticated retail-client session, not bidirectional closure
or evidence of playing a match beyond the join panel.

The [machine-readable evidence](2026-09-08-ql-retail-auth-interop.json) pins
binaries, asset hashes, commands, configs, process outcomes and log hashes.
Retail executable SHA-256 is
`c926fe9f6c851e00b3b9332e88903ad01f28fdd60454873891c0158f5ded1299`.
The provider file matches the repository's 0.3.1 manifest hash; its runtime
version string reports 0.3.0. That discrepancy is recorded without changing
or redistributing the binary.

## Probe progression

A short dedicated startup probe exited normally but retained only sparse
shutdown logging. Longer retail-server probes established Campgrounds/AAS
initialization and a Steam-server connection in live logs. The retail server
bound the configured loopback UDP endpoint but did not answer the harness's
`getinfo` query during either readiness attempt. Therefore those attempts did
not launch a client and are not network-admission failures.

The next probes used observed AAS initialization as the connection trigger
and preserved live logs before retail shutdown replaced qconsole.log. The
fnTech3 client first ran with Steam integration disabled and repeatedly
reported its no-ticket fallback. With the pinned local provider and legitimate
retail Steam API enabled, provider initialization succeeded and the fallback
message disappeared, but no challenge response or active session was observed.
Both forward connection probes required harness termination of the remaining
client at the 100-second deadline; the retail servers exited normally.
The cause of the missing response remains unresolved. Provider startup alone
does not prove the client sent a valid ticket or the retail server accepted it.

The reverse probe enabled the same provider on fnTech3's dedicated server and
launched the unmodified installed retail client. The server's
`Platform authentication validated client 0.` diagnostic comes from processing
the provider authentication result, followed by CS_ACTIVE. Both client and
server completed normally, and the engine TGA was inspected after conversion
to PNG. The screenshot shows the native join interface over the world, not
active first-person combat.

## Safety, validation and remaining work

All probes used isolated writable homes and stock pak hardlinks. Retail
installations remained unchanged. Clients were windowed on private, unswitched
Windows desktops with input and audio disabled. Startup configs supplied
commands; no keyboard/mouse control, OS capture or RCON was used. Raw logs,
account identities, tickets, screenshots, retail binaries and assets remain
untracked. The provider and Steam API were loaded from existing local files.

No engine code changed. The preceding
[build and suite checkpoint](2026-09-08-glibc-baseline-build.md) remains the
source validation record; documentation links and release-document staging
were checked for this update. Q3's
[retail bidirectional evidence](2026-09-08-q3-retail-interop.md) is unchanged.
QL still needs the opposite direction, remote and long-session coverage,
authentication failure/lifecycle checks and broader gameplay evidence.

Unrelated diagnostics: the retail client reported a QUAKE LIVE website load
failure (error -2), but the direct connection and native join UI still worked.
The provider's manifest/runtime version mismatch remains unresolved. No online
website, audio or general optional-service success is claimed.
