# Full map changes with retail peers

On 2026-09-08, one full map change passed in each retail interoperability
direction for both profiles using the Windows x86 fnTech3 binaries. Q3 changed
q3dm6 to q3dm7; QL changed Campgrounds to Overkill. All eight processes in the
four final pairs exited 0 normally. Destination-map engine screenshots show the
world, first-person weapon and HUD.

The [machine-readable record](2026-09-08-retail-map-changes.json) pins source
baseline `15b832b`, exact binaries, assets, commands, configs, logs, images and
outcomes. No engine source changed. The retail executable/provider provenance
is established in the [Q3](2026-09-08-q3-retail-interop.md),
[QL admission](2026-09-08-ql-retail-auth-interop.md) and
[QL dedicated-server](2026-09-08-ql-dedicated-retail-interop.md) checkpoints.

| Direction | Transition | Process duration |
| --- | --- | --- |
| fnTech3 client to retail Q3 server | q3dm6 to q3dm7 | 13.719 s pair |
| Retail Q3 client to fnTech3 server | q3dm6 to q3dm7 | 12.859 s pair |
| fnTech3 client to Linux x86 retail QL dedicated server under WSL | Campgrounds to Overkill | 27.016 s client / 52.454 s server |
| Retail QL client to fnTech3 server | Campgrounds to Overkill | 35.891 s pair |

Each final server log contains two game initializations and two transitions to
CS_ACTIVE. Q3 logs contain two pure commands per pair; the retail QL server logs
two, and fnTech3's QL server logs four including repeated submissions. These
counts are evidence of the observed exchanges, not a claim that every submission
represents a distinct negotiation. Both maps admit the client and produce
rendered destination scenes. The QL retail dedicated server records one accepted
BeginAuthSession and one EndAuthSession; fnTech3's QL server records one platform
authentication validation across its transition.

## Probe synchronization and incomplete attempts

The QL map changes reuse the previous synchronized restart harness: the server
waits for the accepted team-join command before scheduling the map command. Full
map changes replace gamestate and load new assets/modules rather than only
performing the fast restart previously tested. The server's append-only Linux
stdout is used for complete transition counts because its qconsole log can reset
when maps load.

The first Q3 forward attempt reached the second map but the retail server quit
before the delayed second screenshot, leaving a client requiring harness
termination. A subsequent attempt captured both images and let the client exit
normally, but did not complete the server shutdown handshake before its deadline;
that server was harness-terminated. Neither intermediate pair is counted as a
fully passing run.

The final Q3 harness waits for the actual first screenshot file before scheduling
the map change and for the second file before requesting server quit through its
local polling config. Config updates use a single atomic file replacement,
avoiding repeated truncation while the engine may read them. Both final Q3 pairs
then complete normally. These are probe corrections, not engine modifications.

## Safety, validation and remaining limits

All clients were windowed and input/audio-disabled on private unswitched Windows
desktops. Commands came from startup and isolated polling configs; no keyboard
or mouse injection, RCON or OS screenshots were used. The Linux server ran
unprivileged and bound only its private WSL address. Legitimate retail inputs
were kept external, with isolated writable homes. Images came from the engine
render target; retail TGA conversion was only for inspection. Raw images, logs,
account identities, tickets, binaries and assets remain untracked.

The [latest code-change validation](2026-09-08-paused-command-history.md) remains
the source test record; the complete suite was not repeated for documentation
only. Evidence links and release-document staging passed for this update.
Repeated map cycles, multi-player matches, Internet/packet-impairment cases,
module unload stress and broader asset/renderer formats remain open. The QL
scenes remain warmup; no scored-match transition is claimed.

Unrelated diagnostics persist: retail Q3 muted-audio messages, QL website-loading
and optional statistics failures, and existing optional asset warnings. These
observations do not establish success for the affected services or audio.
