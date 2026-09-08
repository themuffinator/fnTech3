# fnTech3 client to retail QL dedicated server

On 2026-09-08, the same Windows x86 fnTech3 client used for the preceding
[retail-client checkpoint](2026-09-08-ql-retail-auth-interop.md) connected to
Valve's unmodified Linux x86 Quake Live dedicated-server distribution under
WSL. The server accepted BeginAuthSession with result 0, admitted the pure
client to CS_ACTIVE, and ended its authentication session on disconnect.
Two runs captured engine screenshots: first the native Join Match panel over
Campgrounds, then first-person warmup with a weapon and HUD after `cmd team f`.
All four processes exited 0 normally, without harness termination.

Together with the preceding Windows retail-client to fnTech3-server result,
this supplies short authenticated retail interoperability in both directions.
It does not close the complete network, gameplay, service or platform gates.
The Windows retail client executable's dedicated-mode probe remains unresolved;
this result uses the separately distributed dedicated server, not that mode.

## Identity and method

The [machine-readable record](2026-09-08-ql-dedicated-retail-interop.json) records
commands, configs, executable identities, asset hashes, logs and screenshots.
Source baseline is `8aa16ed`; no engine source changed in this checkpoint.
SteamCMD anonymously installed and validated app 349090, build 1168250,
with a Linux platform override into a separate external fixture. The bundled
server_readme identifies this app and its x86/x64 launchers; its hash is pinned
in the record. [Valve's SteamCMD documentation](https://developer.valvesoftware.com/wiki/SteamCMD)
describes the installation tool. No server code, assets or proprietary runtime
was incorporated into fnTech3.

The server is an ELF32 i386 executable, build identity
`03cd28999a91c109175a159309f78bb0659e52d9`. It ran as uid/gid 65534 with a
private HOME, writable config path, and process-local library path to the
shipped Steam runtime. Windows used the previously pinned provider and legitimate
retail Steam API. Communication crossed the private Windows/WSL network; this
is not an Internet or multi-machine test. The server bound only its WSL address.

| Run | Client duration | Server duration | Screenshot observation |
| --- | --- | --- | --- |
| Admission | 9.313 s | 62.892 s | Native Join Match panel over Campgrounds |
| Team join | 8.968 s | 63.360 s | First-person weapon and HUD; waiting for more players |

The second run sends `cmd team f` through activeAction after connection and
then waits before taking the engine screenshot. This is a startup command,
not keyboard/mouse injection. It proves entry into warmup, not a scored match
or interactive combat. Clients ran windowed, input-disabled and audio-disabled
on private unswitched Windows desktops. No OS screenshot, RCON or input-control
action was used. Account identities, tickets and all raw proprietary inputs
remain untracked.

## Intermediate failures and corrected probes

The first dedicated launch from WSL's mounted Windows drive found no pk3 files
and exited 1 with `Couldn't load default.cfg`. Copying the distribution to WSL's
native filesystem resolved discovery; the exact filesystem cause is not isolated.
An initial native copy under /tmp was absent on a later invocation, so the final
fixture uses a persistent build directory. These were probe setup failures.

The dedicated server received `getinfo` requests but did not produce the expected
readiness reply. A subsequent AAS-log trigger was also unsuitable for its actual
log output. Those probes exited normally without launching a client. Final
runs used the observed publication of sv_paks after map initialization. These
readiness changes do not establish support or non-support for every query format.
The actual connection supplies the protocol, pure and authentication evidence.

After both successful sessions, all 27 files in the native server fixture
matched the downloaded distribution byte-for-byte, with no added files. Writable
runtime state stayed in isolated homes. The full suite was not repeated for a
documentation-only checkpoint; prior build/suite evidence remains in the
[baseline report](2026-09-08-glibc-baseline-build.md). Evidence links and release
document staging were validated.

## Remaining gates and unrelated diagnostics

Long sessions, packet impairment against retail peers, authentication rejection
and callback/shutdown faults, broader maps/mods and full match play remain open.
The previous Windows retail-executable server-mode silence is not diagnosed by
this successful dedicated-server run. The earlier provider version-label mismatch
and retail website failure remain recorded separately.

The dedicated log reports SteamAPI_Init client-side failure without a running
Linux Steam client, then successfully initializes Steam Gameserver, connects to
Steam servers and handles authentication. This distinction is preserved; no
logged-in Linux client service success is claimed. Mounted-drive pak discovery
also remains an unrelated retail-runtime environment issue.
