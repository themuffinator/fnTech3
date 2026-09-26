# Elite Force SP command ordinals

Elite Force SP now executes and acknowledges commands by their sequence
number, even when command timestamps repeat or move backwards. This fixes
the startup event and weapon-change timing difference left open by the
[player-snapshot correction](2026-09-21-stvef-player-snapshots.md).
The implementation is commit `8f9e284`.

## Native contract and correction

The supplied `stvoy.exe` 1.20 executes new command ordinals in `SV_UserMove`
at `0x424f64..0x425015`. Its last executed number is at client offset
`0x530`. It copies the command into `lastUsercmd` before calling the game
(`SV_ClientThink`, `0x424c60`), while passing the supplied command itself to
`ClientThink`. The game can modify that command. If a packet omits commands,
retail repeats the first supplied command at most five times, including
any changes the game makes to it between calls, then executes the new
supplied commands. A duplicate ordinal does not execute again.

`SV_ClientEnterWorld` resets the ordinal to zero (`0x424a4b`) and passes the
original entering command to `ClientBegin`. Every snapshot carries the last
executed ordinal after its delta byte (`0x428138`). Retail's snapshot flag
2 reports dropped commands and is cleared as the snapshot is serialized
(`0x428145..0x428161`). These observations are pinned to executable SHA-256
`8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74`.

fnTech3 had applied Q3's timestamp filter to SP, then guessed the last
executed command number from playerstate's command time. That loses
commands with repeated timestamps. It also changed the initial command time
to the previous acceptance threshold. The retail game clamps command time,
so its timestamp is not a reliable acknowledgement of a command ordinal.

The SP loopback command header now carries its newest ordinal and snapshots
carry the server's actual acknowledgement. SP command recovery follows the
observed bounded replay rule. The native game adapter passes a separate,
mutable command through the frozen SP record and copies its adjustments
back; `lastUsercmd` retains the pre-call copy. Counter comparisons use
unsigned distances, avoiding signed overflow and rejecting stale or
half-range-ambiguous ordinals. These are explicit hardening choices beyond the
positive-counter native fixtures.

The internal dropped-command flag uses bit 8, then becomes retail's bit 2
at the cgame boundary. This avoids colliding with the engine's existing
inactive-snapshot bit 2. Other profiles retain their header widths,
timestamp filters and one-argument `GAME_CLIENT_THINK` calls. SP's packet
body remains the independently defined internal transport described in the
[host record](2026-09-19-stvef-sp-host.md); this change does not claim native
SP network interoperability or expose expanded engine records to modules.

The [Elite-Reinforce source reference](https://github.com/kugelrund/Elite-Reinforce/tree/526e911c532817b3021aaa564b6b0cf087879827)
helped explain the game module's timestamp clamp and animation behavior.
Its STEF Game Source License is incompatible with incorporating its
implementation into this GPL engine. No source text or executable bytes
are incorporated. The engine changes and probes are independently written.

## Executable tests

An owned probe maps the supplied retail executable without starting the
game. It retains the full native `SV_UserMove` and `SV_ClientThink`, replaces
message decoders with controlled inputs and supplies an owned game callback.
Seventy-two cases cover command overlap, missing commands, the replay cap,
zero/increasing/decreasing timestamps, and a callback that modifies its
command. The tracked fixture contains the resulting scalar observations,
source identity and probe hash, without proprietary code.

`tests/stvef_sp_user_move_tests.py` compiles the actual command execution and
full packet decoder against those observations. It also checks partial
command records before any game callback, new ordinals after a time rewind,
invalid counts, null input, inactive clients, callback-triggered disconnects,
stale/ambiguous ordinals and full-width counter wrap. A second compiled
probe exercises the actual game adapter's mutable command conversion and
null-command refusal. A third executes 1,008 writer/parser header cases
across seven profiles and rejects each truncated acknowledgement boundary.
It also verifies the SP flag translation.

The shared decoder regression now covers all eight non-SP wire variants.
The RTCW bot/lifecycle harnesses supply the new profile query while retaining
their original behavior checks. The obsolete command-time inference test is
removed. Thirty focused tests pass on Windows x86 and Linux i686; the new
Linux probes use AddressSanitizer and UndefinedBehaviorSanitizer.

Windows x86 and Linux i686 client/dedicated builds complete. The full
Windows suite passes 120 targets with 2 skips. Its Python aggregate
runs 1,864 tests with 72 skips in 684.584 seconds.
The packaged-evidence link check also passes after adding this record.

## Runtime observations

Three independent diagnostic runs compare retail, fnTech3 before this
correction, and corrected fnTech3 in `_holodeck_camelot`. At the first active
snapshot, time 1,600, retail's player command time is 550; the previous engine
reports 1,550. The next native command still has a zero timestamp but a new
ordinal, so retail executes it while the previous engine rejects it.

| Field | Retail | Before correction | After correction |
| --- | ---: | ---: | ---: |
| Command time at 1,600 | 550 | 1,550 | 550 |
| Weapon timer at 1,600 | 199 | 200 | 199 |
| Event sequence at 1,650 | 14 | 13 | 14 |
| Playerstate weapon at 1,900 | 2 | 6 | 2 |
| Playerstate weapon at 2,150 | 6 | 6 | 6 |

All 69 sampled snapshots from times 1,600 through 5,000 now match retail in
the queried command acknowledgements, command times, movement flags, weapon
state/timer, leg/torso animation and timers, ground entity, weapon/events,
event parameters and external event fields. There are no differences in
these queried fields. Snapshot packet numbers remain offset by one; this
is distinct from the corrected command acknowledgement. This comparison
is not a claim that every playerstate byte or every game scene is identical.

The canonical production binary passes first-person Camelot and
third-person tutorial captures on GLx, Vulkan and RTX raster/SDR. It also
starts all 66 supplied SP/tour maps on GLx and writes three engine captures
per map. The previously restored player model and weapon choices remain
visible. These runs cover initial scenes, intro cards and cutscenes.

Save/reload probes advance tutorial to time 5,300, save it, let the world
advance, then load it and save again at 5,800. Transitioning to Borg2 starts
that level's own earlier clock; the hub restore resumes its saved clock.
Retail loads the fnTech3 save and writes another at 12,100; fnTech3 loads
that result and advances it to 12,450. All three sessions exit normally.

| Run group | Sessions | Engine captures |
| --- | ---: | ---: |
| Retail/before/after diagnostic sequence | 3 | 9 |
| Save, hub and cross-engine reloads | 3 | 5 |
| Canonical first/third-person renderer checks | 6 | 18 |
| Canonical GLx map inventory | 66 | 198 |
| Linux Holomatch bot/restart control | 1 | 1 |
| Refreshed manual-stage preflights | 2 | 2 |

The Linux Holomatch match retains Munro through `map_restart`. The staged
Windows Holomatch control also retains its bot; staged SP loads the existing
MDR scene save in third person. Each process completes normally.

Contact sheets review each map's final capture, and numerical comparisons
retain all three samples. Random actor choices and later combat effects
still differ. In one Camelot run a later explosion produces a large image
difference despite the matching earlier command trace. The diagnostic
native early image also contains logging text. These images do not define
a universal pixel-parity threshold.

All runtime homes are independent under `E:/Games/fnTech3-tests`. Windows
runs use private inactive desktops; Linux uses private Xvfb and software
Mesa. Every game runs windowed, with mouse/joystick input and sound disabled;
SP also disables feedback. Captures come from the engine's `screenshot`
command. No mouse or keyboard is controlled, and the real installation is
unchanged.

## Evidence and remaining gates

The [manifest](2026-09-21-stvef-command-ordinals.json) records source,
executable, module, pak, probe and fixture identities; commands, settings,
logs and captures; build/test outcomes; native and runtime comparisons; and
manual-stage provenance. Raw diagnostics and captures remain in the
worktree's `.tmp/stvef-gaps` and the designated test homes.

The manual installation at `E:/Games/fnTech3-tests/stvef-manual` now contains
the canonical engine from `8f9e284`. Its seven configuration/save/launcher
files retain their hashes. Only the README's engine-provenance paragraph
changes. The original staged homes and interactive launchers are not run;
fresh independent preflight homes exercise the staged binaries.

The observed startup command/event timing mismatch is resolved for the
sampled sequence. Broader campaign traversal, physical movement/look/fire,
menu rebinding/save/load, subjective listening, feedback hardware and
broader renderer/device parity remain open. The map inventory contains the
user's HD texture overlay. Both Elite Force profiles remain development-gated.

Unrelated existing issues remain: the installed `video/st_21.bik` has a bad
ZIP CRC; missing retail asset paths and excess `messhall` ambient subwaves
produce warnings; the Windows x86 driver lacks native RT support; and the
wider Linux suite has previously recorded failures outside this slice.
Linux rebuilds retain the existing reliable-command signedness warning
(now at `sv_snapshot.cpp:311`) and other pre-existing warnings. This change
does not repair those issues.
