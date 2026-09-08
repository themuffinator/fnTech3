# UDP fixture-pak downloads and pure admission

Both profiles download an engine-authored fixture pak and complete pure admission
in both reference-peer directions, with baseline transport and with periodic
loss, delay and duplication. Every successful client file matches the server's
32,916-byte ZIP exactly: SHA-256
`9cc5e33aac0b21cb32c5e618fa137ad0b448acd507120558ebc56310197b3161`.

| Client / server | Protocol | Baseline duration | Successful impaired duration |
| --- | --- | ---: | ---: |
| fnTech3 / pinned FnQuake3 | 71 | 35.375 s | 35.297 s |
| Pinned FnQuake3 / fnTech3 | 68 | 35.390 s | 35.344 s |
| fnTech3 / existing FnQL | 91 | 35.516 s | 80.515 s |
| Existing FnQL / fnTech3 | 91 | 35.391 s | 80.484 s |

These are whole-probe lifetimes, not transfer times. The original short QL
impaired probes quit while downloading: logs show advancing acknowledgements
through block 7/8 and loading UI screenshots. Their final files were absent.
They are retained as incomplete results, not successful transfers or proof of
a permanent stall. Repeated runs with a 3,600-frame client wait and longer server
lifetime complete, produce identical final files and reach CS_ACTIVE. This
documents the observation-window limitation without changing engine behavior.

The fixture is created only in the server's isolated home under
`fntech3_download_probe/fntech3-fixture.pk3`. Both peers select that fs_game.
The engine includes non-base-game paks in its referenced-pak list, causing the
client with an initially empty mod directory to request the file. No retail
file is transferred. The client uses its normal temporary-write/finalization and
filesystem restart paths before loading modules and entering the pure session.

The ZIP contains one independently generated 32,768-byte random payload,
`fntech3_probe_payload.dat`, using Python Random seed 20260908, ZIP_STORED and
timestamp 2026-01-01 00:00:00. The evidence records its generation recipe and
payload hash; neither proprietary content nor executable code is in the fixture.
The generated archive and all retail inputs stay outside Git.

Client cl_allowDownload and server sv_allowDownload are 1; sv_dlURL is empty.
CL_BeginDownload, svc_download packets, server completion, final-file hashes and
subsequent CS_ACTIVE establish UDP transfer, not HTTP. The reference server's
empty-redirection-URL warning is expected in this setup. Impaired runs use the
existing relay's drop-every-17, delay-every-19 by 75 ms and duplicate-every-23
policy. Faults occur throughout the connection; no claim is made that a particular
download block was selected by those ordinal settings.

All processes exit normally. All relays stop without errors or pending packets.
Inspected successful impaired screenshots show Q3 world/weapon/HUD or QL join
UI/world; QL team entry remains unproven. Probes use private unswitched desktops,
windowed GLx, disabled input/audio/Steam/Workshop/WebUI, empty master endpoints,
read-only legitimate assets and isolated writable homes. No input control or OS
capture occurred. Durations do not establish sustained gameplay.

The [evidence record](2026-09-08-udp-pak-download.json) preserves eight successful
and two incomplete outcomes, file/engine/reference identities, configurations,
protocol negotiation, packet counters and decisive download logs. Existing
download chunk/truncation, short-write and profile-pak executable regressions
also pass (three tests). Engine source/binaries are unchanged. The Q3 reference
is pinned at 7d8c9629; the existing FnQL source revision is unknown. Shared lineage
limits independence.

HTTP/redirects, interrupted-transfer cleanup/resume, disk exhaustion, oversized
and malicious archives, sustained download throttling, WAN behavior and
independent retail/service interoperability remain open. Optional-resource and
default-registration warnings recur; no new unrelated engine defect was found.
