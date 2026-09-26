# Elite Force SP player snapshots and map startup coverage

Elite Force SP now includes the visible local player in its packet-entity
list. This restores the third-person player model and the shared animation
updates on which the retail game module's weapon-change behavior depends.
For example, `_holodeck_camelot` now selects the grenade launcher, as retail
does, instead of the stasis weapon. The correction is commit `36058b3`.

## The missing player

The shared Q3 snapshot builder marked the client's own entity as already
sent before walking visible entities. That is appropriate for the other
profiles, whose clients regenerate their player entity from playerstate.
EF SP also constructs that entity from playerstate, but visits it through
the packet-entity list. Visiting it updates animation frames in the state
shared by the game and cgame halves of `efgamex86.dll`.

The supplied `stvoy.exe` 1.20's snapshot builder at `0x428330` has no such
self-suppression. Its visibility walk at `0x4285a0` includes entity zero
when linked, client-visible and visible from the current viewpoint. An owned
logging probe in an independent retail executable copy confirms that a
Camelot snapshot at time 1,650 contains 90 entities, beginning with entity
zero. The original installation remains unchanged.

`SV_BuildClientSnapshot` now omits the premark only for `stvef-sp`. It does
not force the entity into every snapshot: linked state, `SVF_NOCLIENT`,
area/PVS visibility, portals, deduplication and existing capacity checks
still apply. The other six profiles retain their previous suppression.

The [Elite-Reinforce source reference](https://github.com/kugelrund/Elite-Reinforce/tree/526e911c532817b3021aaa564b6b0cf087879827)
helped trace packet-entity visitation, shared animation updates and scripted
weapon grants. Its STEF Game Source License is incompatible with incorporating
its implementation into this GPL engine. No implementation text or native
executable bytes are incorporated. The small change to fnTech3's existing
snapshot filter is independently implemented and checked against retail.

## Regression and runtime checks

`tests/stvef_sp_snapshot_tests.py` compiles the actual complete client snapshot
builder with its common snapshot and visibility helpers. It exercises 168
cases across all seven profiles: two player slots, repeat snapshots, portal
overlap, a disjoint camera PVS, broadcast, unlinked/NOCLIENT entities,
disconnected areas, missing/zombie clients, invalid player numbers and failed
player reads. It checks membership and unchanged playerstate bytes. The
previous code fails on SP's expected player membership.

The new test and the two existing ET snapshot regressions pass on Windows
and Linux i686. The new Linux probe uses AddressSanitizer and
UndefinedBehaviorSanitizer. Windows x86 and Linux i686 client/dedicated
builds complete. The full Windows suite passes 120 targets with two skips;
its Python aggregate runs 1,862 tests with 72 skips in 707.834 seconds.
This suite follows the source correction; the canonical client is relinked
after the concurrently running baseline map sweep finishes.
The packaged-evidence link check also passes after the ledger and record
are added.

The supplied five-pak inventory contains 66 SP/tour maps with player starts.
The inventory checks BSP identity and bounded entity lumps with the same pak
precedence as the test asset copy. Each session starts a map, captures three
engine screenshots and quits normally:

| Run group | Sessions | Captures | Scope |
| --- | ---: | ---: | --- |
| Retail and baseline GLx/Vulkan/RTX | 264 | 792 | All 66 maps, before correction |
| Corrected private GLx binary | 66 | 198 | All 66 maps, after correction |
| Targeted before/after comparison | 8 | 24 | Camelot first person; tutorial third person |
| Canonical production binary | 6 | 18 | Both targeted scenes on all three renderers |
| Linux Holomatch control | 1 | 1 | Local match with a Munro bot |
| Refreshed manual-stage preflight | 2 | 2 | SP save-load in third person and Holomatch bot match |

The private corrected binary links the changed snapshot object into the
existing build without runtime logging. Its exact source and executable
hashes are recorded separately from the baseline and canonical binary.
Renderers are checked from their actual initialization messages. All map
sessions verify queried fixed time, window mode and texture/lightmap
settings; the comparison uses uncompressed textures, 50 ms fixed steps and
RTX raster/SDR output. The installed x86 GPU driver lacks native RT support.

Retail and all corrected renderers show the tutorial player in third person;
the baseline shows an empty corridor. The corrected GLx map sweep also shows
the retail weapon choices in `borg3`, `_holodeck_temple`, `scav2` and `voy8`.
At Camelot's middle capture, full-frame RGB mean absolute error drops from
about 4.043 to 0.172 on each renderer. This is a scene observation, not a
general parity threshold. Early weapon effects and later combat differ;
all samples, including larger late differences, remain in the record.

## Remaining startup differences

The later [command-ordinal correction](2026-09-21-stvef-command-ordinals.md)
resolves the event and weapon-change timing mismatch below in 69 sampled
startup snapshots. The observations here describe this earlier revision.

Instrumented runs distinguish weapon selection from the weapon currently in
playerstate. At snapshot time 1,650, both corrected fnTech3 and retail select
weapon 6, but their event sequences still differ:

| Observation at 1,650 | Retail | Previous fnTech3 | Corrected fnTech3 |
| --- | --- | --- | --- |
| Selected weapon | 6 | 5 | 6 |
| Playerstate weapon | 2 | 2 | 2 |
| Event sequence | 14 | 8 | 13 |
| Event slots | 18, 35 | 18, 18 | 35, 18 |
| Event parameters | 7, 10 | 12, 7 | 10, 7 |

At time 1,900, corrected playerstate has weapon 6 while retail still has
weapon 2; by 2,150 both have weapon 6. Startup event order and weapon-change
timing therefore remain open. Snapshot numbers also differ by one despite
equal snapshot times and command numbers in these samples.

The first native logging attempt read an argument after the retail function
had advanced it to the end of the entity array. Its zero-valued snapshot
fields are invalid and discarded. The second probe logs immediately after
the playerstate copy at `0x40274a`; the third additionally reads entity count
and the first entity from the native source ring. These corrected probes,
their local logs and separate binary identities are retained. None of the
diagnostic logging is in production code.

## Evidence scope and open gates

The [manifest](2026-09-21-stvef-player-snapshots.json) records commits,
source/executable/module/pak hashes, commands and configuration templates,
per-session homes, logs and screenshot identities, comparisons, native trace
samples, build/test logs and staged artifacts. The collector rechecks 1,406
runtime log/capture files. Contact sheets review each map's final capture;
numerical comparisons retain all three samples. Raw logs, diagnostic helpers
and images stay in the worktree's `.tmp/stvef-gaps` and the designated
`E:/Games/fnTech3-tests` homes, without committing proprietary inputs.

The manual installation at `E:/Games/fnTech3-tests/stvef-manual` now contains
the canonical engine from `36058b3`. Its seven settings/save/launcher files
retain their hashes. Only the README's current-engine provenance paragraph
changes, with before/after hashes recorded. Fresh isolated preflights use
the staged executable; the user's interactive launchers are not run.

These are initial scene checks. Some maps show intro cards or scripted
cutscenes, and the asset set includes the user's `zSTVEFtex.pk3` HD overlay.
They do not establish campaign traversal, stock-only coverage or broader
installation compatibility. All probes are windowed, use private inactive
Windows desktops or private Linux Xvfb, disable mouse/joystick input, sound
and force feedback, and capture through the engine's `screenshot` command.

Campaign play, physical movement/look/fire, menu rebinding/save-load,
listening, feedback hardware and broader renderer/device parity remain open.
Random actor/combat choices and camera timing also affect image comparisons.
Both Elite Force profiles remain development-gated.

Unrelated existing issues remain: the installed `video/st_21.bik` has a bad
ZIP CRC, some retail asset paths produce warnings, and the wider Linux suite
has previously recorded failures outside this slice. The Linux rebuilds also
retain an existing signedness warning in the reliable-command count check at
`sv_snapshot.cpp:304`. Those issues are not repaired by this correction.
