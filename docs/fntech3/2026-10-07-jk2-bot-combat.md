# Jedi Outcast: bot enemy selection, collision and saber deaths

Date: 2026-10-07. Profile: `jk2-mp`, retail 1.02a. Evidence: retail binary
observation, Source, Unit and a Windows x86 dedicated-server Live subset.
Machine-readable companion: [2026-10-07-jk2-bot-combat.json](2026-10-07-jk2-bot-combat.json).

## Finding

The bots rejected every other player because fnTech3's entity linker wrote
`s.solid = 0`. Outcast's player BODY bit is `0x100`, while that linker tested
Quake III's `0x02000000`. The retail QVM checks `s.solid` before its PVS and
line-of-sight tests. Correcting the link mask alone makes both bots acquire
enemies and issue attack actions. Snapshot import 209 and AAS are not that
search's input.

Two further collision contracts prevented attacks from doing damage: the
temporary body hull used the same wrong BODY bit, and the entity adapter
read `ownerNum` at byte 392 instead of retail's byte 396. A zero in the
misidentified owner word makes the shared trace skip players as owned by
client 0, or as sharing that owner. After correcting the hull and owner
offset, the original game module produces saber damage and kills.

Imports 407 and 408 were also no-ops. They do not cause the enemy-selection
failure, but they suppress alternate attack and force power after selection.
They now set the flags measured in this title's retail engine, with the
previous-jump marker translated when returning input to its module.

This supersedes the inferred owner placement in the
[original contracts](2026-09-22-jk2-contracts.md) and the limited bot outcome in
[records](2026-09-22-jk2-records.md) and
[server clips](2026-10-07-jk2-server-clips.md). Spawning, moving and collecting
items in those earlier runs did not establish combat.

## Retail QVM: how an enemy is found

Instruction numbers below are decoded QVM instruction indices, not file
offsets. `vm/jk2mpgame.qvm` was read directly from the legitimate installation's
`base/assets0.pk3`, using `.tmp/jk2-server-bolts/qvm.py`, `qvm_sites.py` and
`qvm_graph.py`, plus a local expression-stack listing. No module code was
incorporated or redistributed.

| Site | Observed operation |
| --- | --- |
| Procedure 14027 | Walks the module's own entity array, base 159636, stride 828. The candidate's client pointer is at entity +408. |
| 14200, procedure 12610 | Calls the ordinary enemy predicate. Instructions 12746â€“12750 reject a zero state word at entity +248, the measured `s.solid`. Other checks reject invalid/inactive clients, self and unsuitable relationships. |
| 14223 | Calls engine import 26, InPVS, with the bot/candidate positions. |
| Procedure 9585, import 24 at 9609 | Clear-sight test: point trace with zero bounds, pass entity -1, mask 1; accepts fraction 1. Player BODY is not part of this world-visibility mask. |
| 14554; 32707 in procedure 29782 | Returns the selected candidate; the main AI stores it at bot-state +1804. The scan also applies distance, field-of-view/hearing and relationship checks. |
| Procedure 6819, call 6828 | Wrapper for snapshot import 209. No code CONST reference or aligned data function-pointer reference to this wrapper was found. |
| Procedure 6868, call 6876 | Wrapper for AAS import 303, likewise with no caller/reference found. Neither wrapper lies on the decoded enemy-search path. |

In the controlled baseline, both bot states retained a zero enemy pointer,
both entities had raw contents `0x100` but `s.solid = 0`, and neither bot
called primary attack. With only the link mask changed, `s.solid` became
nonzero, both enemy pointers named the opposing entity and primary-attack
calls appeared. This isolates the selection failure from the later collision
and elementary-action corrections. It does not promise that every mod using
209 or AAS is supported: those imports remain refused.

## What retail jk2mp.exe does

The legitimate 1.02a executable was read with Capstone. Its dedicated server
requires the disc; it was not run or bypassed. These are observations of its
instructions, not live retail-server comparisons.

| Retail address | Contract |
| --- | --- |
| `0x4502d0`, test at `0x450315` | Entity linking tests contents at +`0x154` (340) against `0x101` before packing state solid at +`0xf8` (248). This is SOLID 1 plus BODY `0x100`. |
| `0x420e92` in `0x420e40` | Initializes the temporary box brush's contents to `0x100`. |
| `0x425fb3`, `0x42633d` | Sphere/cylinder hit writers report contents `0x100`. |
| `0x45093d`, `0x4509cc` in sweep `0x4508f0` | Read the pass entity's and candidate's owners at entity +`0x18c` (396). The previously inferred word at 392 is not the owner. |
| `0x450962`, `0x4509d6` | Read/test svFlags at entity +`0x130` (304), also independently fixing that member's identity. |
| `0x44b817`, `0x44b82f` | Imports 407/408 call the bot export slots +`0x98`/+`0x9c`. Binder stores at `0x4b1194`/`0x4b119b` resolve them to `0x4b1570`/`0x4b1590`. |
| `0x4b1570`, `0x4b1590` | OR alternate-attack `0x200000` and force-power `0x100000` into input +32, using 40-byte client inputs. Primary attack at `0x4b1550` ORs 1. |
| `0x4b1820` | Writes think time and copies the 40-byte input record. |
| `0x4b1850`, store at `0x4b1880` | Resets input, retaining a previous jump as `0x800000`. Shared Q3 botlib uses `0x10000000` internally. |

The QVM's input conversion, procedure 7449, converts alternate-attack and
force-power flags into command buttons 128 and 512 at 7540 and 7637. These
are title-specific bits carried through the shared elementary-action library;
other profiles' input flags are unchanged.

The shared-entity declaration now describes a **minimum accessed prefix** of
400 bytes (296 state plus 104 shared), not an assertion that private game data
begins at 400. The unnamed word at 392 is preserved. The retail QVM's actual
client pointer remains at 408 and its whole entity stride remains 828.

`CM_BodyContents()` selects `0x100` only for JK2 MP, preserving every other
profile's established BODY behavior. The linker, initial temporary box,
capsule hit contents, map-contents seed and restoration of temporarily changed
box contents use that selection. Raw map/module contents are not translated
to Q3 numbers.

The new elementary-action boundary validates allocated client input state
and output bounds. Its QVM output check covers the complete `dataMask + 1`
arena: this module places the input output on its program stack, beyond
`exactDataLength`. An initial development attempt using the smaller limit
refused that legitimate output; it was repaired before the final runs and
is included among the unsuccessful experiments in the companion.

## The requested model-0 bone query

The call at 129659 in procedure 129321 is part of saber **death/dismemberment**,
not each ordinary saber hit. The death path (procedure 126435, call 128089)
enters procedure 130381 for damage mode 3, logged live as `MOD_SABER`.
That procedure checks `g_dismember`, a random percentage test, damage of at
least 20, and a usable hit location before calling 129321 at 130505.
The cvar table record at data 1948 names the handle at 143044 `g_dismember`,
default `0`; the procedure reads its integer at 143056. The diagnostic run
explicitly set `g_dismember=100` to reach this path.

Client model setup adds `*r_hand` and `*l_hand` as bolts 0/1, then `cranium`
(2), `thoracic` (3), `lradius` (4), `rradius` (5), `ltibia` (6), `rtibia` (7)
and `Motion` (8), at the measured setup calls 110861 through 111005 for
the bones. Procedure 129321 selects one of bolts 2â€“7 from the entity's
stored handles, passes model 0 to 586, and reads the returned matrix's
translation at float indices 3, 7 and 11; it discards the return status.

In the 120-second instrumented `duel_pit` run (game type 0), three saber
kills reached this call. All three queries returned 1, with no refusal:

| Time (ms) | Bolt | Bone | Returned translation minus supplied position (x, y, z) |
| --- | --- | --- | --- |
| 36200 | 7 | `rtibia` | `(3.939636, 4.539062, -6.325467)` |
| 78600 | 4 | `lradius` | `(-10.249207, -2.951172, 18.914917)` |
| 92650 | 2 | `cranium` | `(-2.387695, -9.504028, 26.430767)` |

The complete returned 3Ã—3 rows are in the JSON companion. These are fnTech3
answers to the original module, using its existing Ghoul2 server store and
the supplied skeleton. They do not establish equality with retail poses.
This run did not query the other three location bones. No renderer, spatial
audio, lighting, input or presentation path was changed or disabled; this
is dedicated-server simulation, so those FnQ3/FnQL features do not apply to
the probe. No retail-exact presentation option was added.

## Runs and validation

| Build variant | Map / game type | Duration | Outcome |
| --- | --- | --- | --- |
| Baseline with logging | `duel_pit` / 3 | 35 s | Zero enemies, attacks and kills; both players' solid word is zero. |
| Link mask only | `duel_pit` / 3 | 45 s | Both bots select an enemy and issue primary attack; no damage kills. |
| Link mask and hull contents | `duel_pit` / 3 | 90 s | Enemy selection and attacks continue; zero kills. |
| Link, hull and action fixes; old owner offset | `duel_pit` / 3 | 120 s | Both bots attack; zero kills. |
| All fixes, with logging and `g_dismember=100` | `duel_pit` / 0 | 120 s | Three saber kills, three successful model-0 location queries, no errors. |
| All fixes, production build and default `g_dismember=0` | `duel_pit` / 3 | 120 s | Three saber kills in both directions, no errors. |

The companion also records a 90-second game-type-0 `ffa_bespin` run before
the owner/action corrections (zero kills), the unsuccessful initial output
bounds check, and a repeated link-only run whose binary hash exposed a
stale build. Neither stale nor failed build is counted as final evidence.
The dismemberment probe used game type 0; the final production probe
explicitly used the reported game type 3.

All runs use Reborn and Kyle at skill 3, added by an exec'd cfg, an isolated
home, `logfile 2`, `net_enabled 0`, sound and Steam integration off, and
`r_fullscreen 0`. They run on a private desktop, with no injected or captured
input and no screen capture. The stable executable path is
`.tmp/runtime/candidate/fntech3.ded.exe`, reused between runs. The runner is
`.tmp/jk2-g2-anims/run-bots.py`; command arrays, identities and observations
are preserved in the JSON companion. Each timed run is deliberately stopped
at its deadline; unchanged-installation and unchanged-binary checks pass.

The strict MSVC 2022 Windows x86 dedicated build uses an isolated export of
`59ca4826eaad7a49b760f917f2b27912c2c3c905` plus this task's source changes,
with `jk2-development=true`, `jka-development=true`, client off and
`strict-warnings=true`. Other sessions' concurrent changes are not part of
these runtime binaries. Diagnostic builds only add counters/read-only QVM
state logging around the game import dispatcher. The final run uses the
production dispatcher with that instrumentation removed.
The companion distinguishes compiled-source hashes from worktree hashes:
only two lines of indentation in `cm_trace.c` differ, with identical code.

Focused checks pass: 18 Python tests covering real collision/linking,
JK2/JKA/RTCW actions, body-mask isolation, world lifecycle, owner traces and
map contents; one IHV native-layout test skips on the 64-bit Python host.
The updated JK2 action test also passes after its final fixture refinement.
Three MSVC x86 tests pass for JK2 ABI layouts, game ABI adapters and the
checked server game-data view. The collision regression runs the production
linker and real hull/trace code against independent fixed-offset records,
including a zero at 392 and `ENTITYNUM_NONE` at 396, raw tail guards, sight
versus shot masks, point contents, capsule contents and unlinking.
All four generated-documentation tests pass as well.

Small logs, decoder/diagnostic helpers and per-run results are retained in
`.tmp/jk2-bot-combat/`; disposable source/build copies are removed after
validation when the host permits cleanup. This task's host blocked their
deletion, so `src/`, `build/` and `os-temp/` remain there. No retail binary,
QVM or asset is committed.

## Remaining and unrelated findings

This establishes bot enemy selection and a narrow saber-combat subset, not
full server gameplay or bidirectional interoperability. Live retail-server
comparison, network clients, broad weapon/force/map coverage and equality
of all Ghoul2 poses remain open.

The same retail sweep has additional special cases at `0x450970` through
`0x450a0b` (svFlags `0x1000` ownership rules), and `0x450a22` through
`0x450a3a` (positive contents including `0x200000`, with masks `0x40301` or
`0x301`). They are not reproduced by the shared sweep and remain a separate
collision-compatibility gap; this correction does not claim those cases.

Separately, Academy's existing `SV_JKABotInputDestination` also bounds a QVM
output by `exactDataLength`. The Outcast runtime failure demonstrates why
that pattern cannot accept stack outputs. Academy's QVM case needs its own
validation; no Academy failure is claimed from these runs, and that adapter
was left for its separate workstream.
