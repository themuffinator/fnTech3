# QL retail demo interoperability

On 2026-09-08, short protocol-91 recordings passed cross-engine playback in
both directions at normal speed and in timedemo mode. All four playback runs
reached an exact completion-marker line, wrote engine screenshots, and exited
0 normally without truncation warnings or ERROR diagnostics. Inspection shows
the first-person warmup world, weapon, HUD and demo controls.

The [machine-readable evidence](2026-09-08-ql-retail-demo-interop.json) pins
recording provenance, binary identities, commands, configs, envelope counts,
file hashes and process results. Source baseline is `7ff4950`; no engine code
changed for this checkpoint.

## Fixtures and observations

| Recorder and live peer | Demo bytes/messages | Playback engine | Normal/timedemo process duration |
| --- | --- | --- | --- |
| Installed retail Windows QL client, fnTech3 x86 dedicated server | 8,846 / 183 | fnTech3 Windows x86 client | 7.734 s / 3.734 s |
| fnTech3 Windows x86 client, unmodified Linux x86 retail dedicated server under WSL | 11,797 / 275 | Installed retail Windows QL client | 14.485 s / 6.641 s |

The retail-written fixture SHA-256 is
`4a051b4148207bc6fb16febc0af42daec47c3cfdd937bf8dc2e7242fd5a4e9af`.
The fnTech3-written fixture SHA-256 is
`1e10076e6a69c69a81388f56d8a6ebef0790bea42d1be90243221152ca84d1bc`.
Both have bounded complete message envelopes and an exact (-1, -1) terminator.
The recorders used `cmd team f`, a settling wait, `record`, a further wait,
an engine screenshot, `stoprecord`, and normal quit. Both recording pairs
also exited normally. The server distributions and authentication context are
those established by the [retail admission](2026-09-08-ql-retail-auth-interop.md)
and [dedicated-server](2026-09-08-ql-dedicated-retail-interop.md) checkpoints.

These are independently identified recording engines, not two copies of the
same demo relabeled as different formats. However, each live pair contains
fnTech3: the retail-written file carries snapshots supplied by fnTech3's server,
and the fnTech3-written file carries snapshots supplied by the retail server.
There is no claim of a wholly retail client/server recording in this checkpoint.
Durations include startup and shutdown and are not performance comparisons.
The scenes remain warmup with one player, not scored matches or combat stress.

## Validation and boundaries

Playback used fresh writable homes and user-supplied legitimate retail assets.
Retail executables and runtimes were loaded from existing installations. All
clients ran windowed with input and audio disabled on private, unswitched Windows
desktops. Commands came from startup configs; no keyboard/mouse control, RCON
or OS capture was used. The retail engine's TGA output was converted to PNG
only for inspection. Raw demos, images, logs, account identities, tickets and
proprietary binaries/assets remain untracked.

Retail shutdown can replace qconsole.log, so the probe preserves the longest
live log and checks the actual standalone completion-marker line, rather than
accepting a matching cvar declaration. All four logs have that line. Normal
playback and timedemo both terminate through their completion paths. This does
not test seeking, pause/rate controls, long recordings, voice, config changes,
map transitions, every legacy format, or malformed external recordings.

No engine source changed, so the full suite was not repeated. The preceding
[build/suite checkpoint](2026-09-08-glibc-baseline-build.md) still identifies
source validation; this update passed release-document staging and link checks.
Q3 behavior and its evidence remain unchanged.

Unrelated diagnostics persist: the retail client's QUAKE LIVE website request
fails with error -2 despite successful direct play/playback; muted-audio and
optional asset diagnostics do not establish audio or website compatibility.
The provider version-label discrepancy remains recorded in the admission report.
