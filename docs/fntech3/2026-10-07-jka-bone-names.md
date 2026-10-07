# Jedi Academy Ghoul2 bone names, 2026-10-07

Academy's server rejected clips and bends set with a different capitalization
of a bone's name, although its animation query already accepted them. This
slice measures the setters and AddBolt in retail `jamp.exe` 1.01 and enables
ASCII-insensitive name matching in Academy's server and client-game stores.
Outcast and stores that do not opt in retain exact matching. The
[JSON companion](2026-10-07-jka-bone-names.json) pins the executable and asset
identities, commands, observations and comparisons. No compatibility gate closes.

## Retail observations

The executable is the same retail 1.01 `jamp.exe` as the
[bone animation record](2026-10-07-jka-server-bone-anim.md), SHA-256
`e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b`.
Read-only disassembly with Capstone establishes these paths; no retail
implementation text is incorporated:

| Operation | Server import and path | Name comparison |
| --- | --- | --- |
| Play a clip | 596: `0x45cc63` → `0x4df310` → `0x4e35a0` → `0x4e32f0` | Bone find `0x4e2be0` / add `0x4e2c70` call `_stricmp`, `0x539336` |
| Bend a bone | 595: `0x45cc1b` → `0x4df460` → `0x4e3240` | The same find/add |
| Query animation | 597: `0x45cc9c` → `0x4df400` → `0x4e3750` | The same find/add, already served |
| Add a bolt | 593: `0x45cbe2` → `0x4df820` → `0x4e22d0` | Surface lookup `0x4ee5b0` calls `_stricmp` at `0x4ee5d2`; bone lookup calls it at `0x4e242d` |

AddBolt searches surfaces first, then bones. Once the name resolves, its
surface-index comparison at `0x4e2331` or bone-index comparison at `0x4e2470`
returns an existing bolt index. Changing capitalization therefore does not
allocate another bolt. Retail also increments its use count; this slice does
not add bolt removal or change the store's existing lifetime rules.

Academy's client-game imports 303, 302 and 317 reach those same wrappers,
respectively from `0x413afc`, `0x413a9c` and `0x413cf5`. That shared code is
source evidence for applying the same policy to the client game. The new live
measurement below is of the dedicated server, not a client rendering run.
Outcast's name matching has not been measured by this task.

## Synthetic server measurement

The independently authored
[fixture](../../tests/fixtures/jka_bone_names_probe.cpp) extends the previous
import-597 fixture. It loads two Kyle bodies and gives both the same clips
and bends, using canonical names on one and other-case names on the other.
It queries 597 and bolt matrices through 587 before and after replacing the
clips and bends, including transitions from previously running clips. It
also exercises a pistol's engine-provided default skeleton.

Both retail runs use `+set dedicated 1 +set fixedtime 75 +set sv_fps 20`,
`mp/ffa1`, an unswitched private desktop, `r_fullscreen 0`, disabled mouse and
joystick input, and isolated writable homes below `.tmp/jka-bone-names/`.
The executable runs from `.tmp/runtime/jka-retail/`; installed assets are
read through `fs_cdpath`. Neither input injection nor screen capture is used.
Each run quits itself, cleans its Ghoul2 handles, and leaves the installation
inventory unchanged. Writable homes are removed after logs are collected.

Observed in retail:

- `MODEL_ROOT` and `PeLvIs` clips succeed, as do `PELVIS` and `ThOrAcIc`
  bends. Querying any capitalization returns the same clip and flags.
- Replacing `MODEL_ROOT` with `Model_Root` finds the running clip for its
  transition. Replacing a `PELVIS` bend with `pElViS` changes that bone's
  angle mode; import 597 carries it in the same record's flags.
- The canonical and differently cased bodies give bit-identical answers
  in all 130 paired observations: 91 animation queries and 39 bolt matrices.
- `*L_HAND`, `*l_hand` and `*L_hAnD` return bolt 0; `PeLvIs`, `pelvis` and
  `PELVIS` return bolt 1; `MODEL_ROOT`, `model_root` and `Model_Root` return
  bolt 2. This covers first insertion with other-case names and repeated
  additions in other cases, for both surfaces and bones.
- `MODVIEW INTERNAL DEFAULT` and `modview internal default` reach the
  pistol's `ModView internal default` bone through setters and AddBolt.
- Empty, missing, prefix-only and suffixed names fail: setters return 0,
  AddBolt returns -1. They do not replace the existing clips or bends.
- The second retail run repeats every observation exactly.

The fixture's output is deduplicated in each run's `markers` list, as in the
earlier harness; counts of setter and AddBolt observations below are distinct
logged operations, not the total number of calls.

## Implementation and scope

The store's `NameMatch` policy is selected at construction. Its default is
exact comparison. Only the Academy server
and client-game stores select `NameMatch::AsciiFolded`. The comparison folds
ASCII A–Z and preserves every other byte, independent of the host locale.
The ASCII assets measured here establish no non-ASCII locale behavior.

Both setters and their `BoneAnimation` / `BoneAngles` accessors use the
store's policy. This keeps differently cased replacements on the same bone
index and lets blending find the prior clip. AddBolt uses the same policy
for surfaces, bones and existing bolt names, preserving surface precedence
and the existing bolt index. Import 597 keeps its already measured behavior.
Outcast's clip setter, its timing, and its default exact-name policy are
unchanged. Academy's UI and SP stores do not opt in through this change.

## Validation

| Observation | Before, matching retail | After, matching retail |
| --- | --- | --- |
| Animation answers (597) | 104 / 195 bit for bit | 195 / 195 bit for bit |
| Distinct clip setter results (596) | 12 / 17 | 17 / 17 |
| Distinct angle setter results (595) | 11 / 16 | 16 / 16 |
| Distinct AddBolt results (593) | 19 / 31 | 31 / 31 |
| Bolt matrices (587), tolerance 0.00015 per component | 44 / 91 | 91 / 91 |

The after matrices differ from retail by at most 0.000006261 in their axes
and 0.000010491 units in their origins. The second fnTech3 run repeats the
first bit for bit. Before, the maximum origin difference was 58.4141 units.
Both fnTech3 runs also leave the installation and tested binaries unchanged.

The baseline is the previous bone-animation slice's binary, SHA-256
`377a263e0426a24a7716f33a8058d5e32926621cec4bb423c4d96a87dd0b3369`.
The candidate client and dedicated server build from a clean export of
`59ca4826eaad7a49b760f917f2b27912c2c3c905` plus only this task's code and
tests, using the existing Windows x86 developer configuration: MSVC 19.44,
Meson `debugoptimized`, strict warnings, static CRT, all development gates
enabled. Both targets build without warnings. Runtime comparison reuses
`.tmp/runtime/jka-boneanim-after/fntech3.ded.exe`; its previous binary is
measured before rebuilding it in place. The JSON pins both executable hashes
and the six source/test files overlaid on that export.

`CheckAcademyBoneNames`, beside `CheckAcademyBoneAnim` in
`tests/jka_server_attachment_tests.py`, executes the production 595, 596,
597, 593 and 587 cases. It checks first insertion and replacement, retained
blend state, shared flags, equal poses, duplicate indices, surface precedence,
full-name matching, non-ASCII bytes, null/missing names, invalid indices,
foreign/stale handles, copies, cleanup, and Outcast/default-store isolation.
The client-game test executes its actual setter and AddBolt cases as well.
Validation passes: 6 focused Python tests with 4 subtests, 15 related
regression tests with 12 subtests (attachments, bolts, rendering, posing,
module refusals, UI and cleanup), and both native Ghoul2/pose test targets.
The full compatibility suite was not run; this is evidence for this slice.

The local reproduction helpers and raw results are retained under
`.tmp/jka-bone-names/`: `build-probe.ps1` compiles the checked-in fixture
with MSVC x86 (`/O2 /MT /W4 /WX /LD`) and exports `dllEntry` and `vmMain`;
`run-probe.py --engine retail --output .tmp/jka-bone-names/<fresh-run>`
runs it. For fnTech3, use `--engine fntech3 --executable
.tmp/runtime/jka-boneanim-after/fntech3.ded.exe` with a fresh output directory.
`compare.py` compares normalized observations with `retail-1`, requiring
exact scalar results and reporting each bolt matrix's maximum component
error. The fixture source and module hashes distinguish this experiment
from the earlier animation-query fixture.

## Presentation and remaining limits

This changes name resolution, not presentation. The dedicated server draws
nothing. The client game keeps the Fn Ghoul2 pose, skinning and attachment
paths, and its existing renderer, lighting, shadows and audio behavior. No
feature is disabled and no retail-exact presentation option is introduced.
The pose-clock and animation records' remaining limits, live interoperability,
demos, SP campaign and saves remain open.
