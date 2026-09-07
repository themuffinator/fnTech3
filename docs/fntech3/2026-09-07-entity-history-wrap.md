# Entity-history counter wrap — 2026-09-07

The producer side of the snapshot history audit now uses the existing defined
32-bit counter increment and distance helpers. Packet-entity ring reads, two
cgame helpers and demo recording use unsigned addition before their ring mask
or modulo. Previously the parser could overflow a signed counter, and demo
indexing could produce a negative index after wrap. History capacities,
protocols, delta codecs and module layouts are unchanged.

An executable regression extracts the production `CL_DeltaEntity` and
`CL_ParsePacketEntities` routines, with a stub wire decoder, and exercises
counter wrap, removal without advancing history, unchanged entities, delta
updates and final old-entity copying across the signed boundary. It also
executes the actual demo-recording index expressions at that boundary. The
test passes on Windows and native Linux. Combined snapshot/native bridge
checks passed (33 tests); Windows MSVC x86 client compilation also passed.

At `dd3e47e` plus this patch, the same rebuilt client recorded and replayed
short Q3 `.dm_68` and QL `.dm_91` demos using legitimate local installations.
Both recordings started/stopped normally; both files contained the terminal
trailer; playback reached the configured completion marker. All four runs
exited normally without truncation warnings or reported demo-message errors.
The [runtime manifest](evidence/2026-09-07-entity-history-wrap.json) records
binary/base-pak/demo hashes, commands, configs and outcomes. Logs and generated
demos remain local in `.tmp/demo-{profile}-entity-wrap/`.

Runs used separate temporary writable homes, windowed GLx, disabled mouse and
joystick capture, disabled audio/Steam/WebUI and startup console configs. No
input injection or screen capture was used. No retail modules/assets were
added to the repository. Runtime demos establish ordinary record/playback
non-regression; the injected executable tests establish arithmetic behavior
at wrap. Neither proves sustained live-network wrap or every claimed demo
format. Other asset warnings and full interoperability gates remain open.
