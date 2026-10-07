# Jedi Outcast server clips and client setup, 2026-10-07

The [server attachments record](2026-10-06-jedi-server-attachments.md) gave
Jedi Outcast's dedicated server a store that keeps one build per time and
served its bolt queries, but the retail game module never reached them:
fnTech3 read 584's instance word as a module address, so 584 answered "no
models" for the precached Kyle and the module's client setup stopped there.
Read correctly, it leads on to 592 (clips), 593 (the skeleton's name), 596
(duplicate an instance) and 599 (clean one), which were refused by number.
This slice reads those four cores, Outcast's own clip timing and the module
fields its instance calls write, serves them, and corrects 584. The retail
QVM's client setup now completes for two bots on fnTech3's developer
dedicated server. Outcast's own dedicated server needs the game disc, so no
retail answer is observed. The [JSON companion](2026-10-07-jk2-server-clips.json)
holds the addresses, runs and tests. No gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jk2mp.exe` 1.02a | `2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a` |
| `base/assets0.pk3:vm/jk2mpgame.qvm` (Outcast 1.02a game) | `4810670f4f6ec3890d510e294dfd9fd4fb7f692b8d415b496aa132a0a79dc105` |
| `base/assets0.pk3` | `e8e466f219bb2faed536021bb0d10aa6b7f5cd687302aa43080da2debdae307c` |
| `fntech3.ded.exe`, HEAD `6c5c7ed9` plus this slice | `e8e0dffd6f12d6f42bd359b78a49aacf1560b92c315491094b11beae2c456057` |
| `fntech3.ded.exe`, the same plus a trace wrapper (diagnostic, not committed) | `51a1d0516e41fd5cd64d12edf6278377d3927ee7369e5ed8b7ba3d4283d3184e` |
| `fntech3.ded.exe`, HEAD `280adf57` plus this slice (the commit's tree) | `ee45fdb15b2f89c9b2164362400091c71d007318991be0360815e76d50764d22` |

The first two servers were built from one clean export of HEAD `6c5c7ed9`
with this slice laid over it, the third from a clean export of `280adf57`,
which two Jedi Academy slices reached meanwhile, with the same slice (MSVC
2022 x86, Meson `debugoptimized`, strict warnings, every development gate on;
all 1,499 targets, no warnings). They ran from
`.tmp/runtime/jk2-server-clips/committed`, `diagnostic` and
`committed-280adf57`. The diagnostic
build wraps `SV_JK2GameCall` to log imports 584 to 599 and nothing else
(`.tmp/jk2-g2-anims/trace_patch.py`, not committed).

## Observed in retail: the handlers and cores

Read from `jk2mp.exe` with Capstone through the game dispatcher's byte table
(0x44cc50) and jump table (0x44c8d8); no retail code is incorporated.

| Import | Handler | Arguments converted | Core | Returns |
| --: | --- | --- | --- | --- |
| 584 | 0x44c64e | none | 0x4a7890: 1 at the first model whose +0x30 is not -1 | the core's |
| 590 | 0x44c74a | none | 0x4a75f0 | 0 (0x44c75f) |
| 592 | 0x44c7b5 | the third, the bone's name, of ten | 0x4a7440 | the core's: 0 or 1 |
| 593 | 0x44c7f1 | the third, a buffer | 0x4a7d80, then `strcpy` into the buffer | 0 |
| 595 | 0x44c83a | none | 0x4a7bf0 | 0 (0x44c853) |
| 596 | 0x44c857 | the second, a field's address | 0x4a7ce0 | 0 |
| 599 | 0x44c8aa | the first, a field's address | 0x4a6e00 | 0 |

- **584** asks whether any model is still there; its handler passes the word
  unconverted, as 586 to 595 do. fnTech3 had converted it.
- **592**, `ghoul2, modelIndex, boneName, startFrame, endFrame, flags,
  animSpeed, currentTime, setFrame, blendTime`, speed and set frame floats.
  The core holds an end frame outside 1..99999 at 1 and a start frame outside
  0..99999 at 0, keeps a set frame that is -1, within 0..100000 or not a
  number and makes any other 0 (0x4a7462..0x4a74a8), checks the instance and
  the model index (unsigned), sets the model's build time (+0x98) to 0
  (0x4a74f6) before the bone is looked up, and reaches the bone's record
  through 0x4ab390, which finds or adds it by name (0x4aa8d0, 0x4aa940) and
  calls the setter 0x4ab070.
- **593** answers, for an instance and a model index below its count, the
  skeleton name in that model's mesh header (`mdxm` +0x48, through the model's
  file name, 0x485220 and 0x484bf0). The handler copies it unbounded.
- **596**, `ghoul2From, &ghoul2To`: when the field already holds anything the
  core does nothing at all (0x4a7cfd). Otherwise it makes an empty instance,
  stores it in the field, lists the field's address in the server's or the
  client's list (by 0x5084a0, which 588's handler sets), and runs the instance
  copy 0x4a79e0 with -1, which copies every model of the source, removed ones
  included, into the new instance with the record assignment 0x4a7b10.
- **599**, `&ghoul2`: a field holding nothing is left alone (0x4a6e06).
  Otherwise the core looks the field's *address* up in the server's list
  (0x1ae664c) and then the client's (0x1ae6638), erases it from the first that
  has it and deletes the instance the field holds, and empties the field
  either way (0x4a6e80): a field that was never listed is emptied and nothing
  freed.
- **588** makes and lists the instance before it loads the model
  (0x4a6ef9..0x4a6f51), so a failed load leaves an empty instance in the field.
- A new bone record (0x4aaa0c) is given only its bone number and flags 0; the
  rest is whatever the stack held. The angle setter (0x4aaf85) replaces only
  the low three bits of the shared record's flags.

## Observed in retail: Outcast's clip timing

**The setter 0x4ab070.** With the blend bit (0x80) given and the record
running a clip, it samples that clip with the getter 0x4ab420: when the
record's transition started at this very time it keeps its sample and takes
the new duration (0x4ab0ee); otherwise, for a forward clip, the transition's
frame is the frame reached and its other frame the whole frame after it
(truncated), each moved inside the old clip (to its start when looping, else
one frame inside its end, 0x4ab18f..0x4ab23f); for a reverse clip both are
the floor of the frame reached (0x4ab108..0x4ab157); and it starts at this
time. With no clip running the bit is dropped and the sample and duration
zeroed (0x4ab262); without the bit, the start too (0x4ab277). Then the new
range, speed and a start time: the time given, or for a set frame the time
less `(setFrame - startFrame) * 50 / speed`, truncated (0x4ab2fb..0x4ab32e).
The record's animation bits 0xd8 are replaced by the flags given, less 0x80
where dropped; any other bit given is added (0x4ab34a..0x4ab377).

**The getter 0x4ab420.** The age is `(now - startTime) * 0.02f` (0x4eaa44),
stored as a float and never below 0; the frame `startFrame + age * speed`,
stored as a float. Past the end (beyond `end - 1` forwards, short of `end + 1`
backwards) a loop keeps it inside the last frame or wraps it with `fmod`, and
any other clip stands at `end - 1` (`end + 1` backwards), frozen or not.

**The bone transform 0x476380.** Each bone starts from its parent's frames,
fraction and transition, the root from the model's default frame (+0x94, 0
from the record's constructor and 588, 0x4a85a9 and 0x4a709a), and a bone's
own record then sets them (0x4763d3..0x476952):

- *The transition.* With 0x80, inside `0 <= age < blendTime` it poses the
  sample, its whole frame weighted by its fraction and the other frame by the
  rest (0x476959..0x4769d3, the same inversion Academy's has), and blends to
  the clip by `age / blendTime`; outside, none. With a clip and no 0x80, none.
  With neither, the parent's.
- *Forwards*, `frame = startFrame + age * speed`: the whole frame, the next
  and the fraction; where the next reaches the end it becomes the start for a
  loop and the end itself otherwise (0x4767d0..0x47682b). Past the end, a loop poses its last frame
  towards its start and then wraps with `fmod` (0x4766a7..0x47675d); `0x48`
  (freeze) holds `end - 1` (0x476762..0x4767a1); anything else sets nothing,
  so the bone keeps its parent's frames.
- *Backwards and still*, speed 0 or less: the ceiling, the frame below it as
  the next and their difference as the fraction; where the next falls below
  `end + 1` it becomes the start for a loop and `end + 1` otherwise
  (0x476830..0x4768d1). Past the end a loop wraps the other way and a freeze
  holds `end + 1`.
- *A clip of no length* holds `end - 1` (`end + 1` backwards), not below 0.
- A negative frame abandons the bone and every bone below it, which keep
  their last matrices (0x476912..0x476951), and a zero fraction reads the
  current frame alone (0x4769f7).

**Precision.** The engine's startup sets the x87 to 53-bit precision
(0x4d4b51 calls `_controlfp(_PC_53, _MCW_PC)` at 0x4d7cc5), so its arithmetic
rounds as IEEE doubles do; it stores the age, frame, fractions and transition
as floats. The helpers are `ceil` 0x4d4c5f (control word 0x1b3f), `floor`
0x4d4d2e (0x173f), `_ftol` 0x4d4f00 (truncation to 64 bits, low word kept)
and `fmod` 0x4d75fa.

**Against Academy's measured timing** (`PlayheadAt`, `ForwardPlayhead` in
`code/ghoul2/ghoul2_server.hpp`, from `jamp.exe` 1.01): Academy divides the
age by fifty, so 5 ms is 0.1 frames there and 0.099999994 here; it wraps a
loop by subtracting whole spans, not by `fmod`; it refuses a reverse clip and
one that ended without loop or freeze, which Outcast poses as above.

## The retail module's calls

From `vm/jk2mpgame.qvm` (decoded by `.tmp/jk2-server-bolts/qvm.py`, not
committed):

- The client setup (procedure 108882): if the client's field holds an
  instance, 584 on it and 599 on the field (+1716 of the entity); 584 on the
  precached Kyle (data 25396); 596 of the Kyle into the field; 593 into a
  frame buffer unless its animation table is loaded (data 16308), to find the
  `animation.cfg`; 589 `*r_hand`, `*l_hand`; 592 `model_root` frames 0..12,
  flags 0x10, speed 1, set frame -1, blend time -1; 591 `upper_lumbar` and
  `cranium`; 588 the saber, 590 linking it to the hand, 589 `*flash` and 595
  into the client's second place. Its caller (110404) then adds the seven
  bone bolts.
- Every frame (procedure 235883): 592 on `model_root`, `upper_lumbar` (set
  frame its start) and `Motion`, flags 0x90 (loop) or 0xc8 (freeze) with a
  150 ms transition, speed `50 / frameLerp` times a multiplier, or one frame
  frozen; and 586 on the saber's `*flash` (model 1, bolt 0).
- The module's animation parser keeps the sign of a negative fps
  (QVM 83669..83719: `floor(1000 / fps)`, `ceil` below 0), and 129 of the
  989 clips in `models/players/_humanoid/animation.cfg` have one, so the
  server is given reverse clips.
- Only 584's result is read; 590, 593, 595, 596 and 599's are discarded.

## What this slice serves

- **584** reads the word as retail does (`SV_JK2Ghoul2Handle`).
- **592** reaches `Store::SetBoneAnim`, which in a store keeping builds is
  Outcast's setter (`Store::SetOutcastBoneAnim`); `DeformationPalette` poses
  each record through `outcast::Control`, the transform above, and the pose
  kernel gains a control that keeps its parent's frames
  (`pose::Animation::inheritFrame`). Academy's stores are unchanged.
- **593** copies `Model::animName`, the mesh header's name, bounded by the
  module's memory; nothing for no answer.
- **596** is `Store::DuplicateInto`, **599** `Store::CleanField`, with
  retail's field listing: 588 and 596 list the field they make an instance
  for, and 599 frees only through a listed field.
- **The module's field** is read and written at its own width
  (`SV_JK2Ghoul2FieldBytes`): a QVM's is a 32-bit word. `Store::AddModel` had
  read and written a pointer's width there, overrunning the next field on a
  64-bit host.
- **590 and 595** return 0, as their handlers do; they returned 1.

**Not reproduced.** A bolt no build has reached is refused with nothing
written, where retail answers its record's uninitialized words; a new bone
record is zeroed where retail's carries its stack's; a failed 588 leaves the
field as it was where retail leaves an empty instance; a source 596 does not
hold is refused before anything is made; 593 answers nothing for a removed
model's record; frames outside the skeleton are refused where retail reads
past its frame table; flags given to 592 outside 0xf8 (which retail would read
as angle controls) are refused when posed; and bone names still compare by
case. Doubles keep x87 extended intermediates under GCC for i686 without SSE
math, where a last bit can differ from the engine's 53-bit arithmetic.

## Runs

The retail QVM on fnTech3's dedicated server, two bots added by an exec'd
cfg (`addbot Reborn 3`, `addbot Kyle 3`), `+set net_enabled 0`, isolated
`fs_homepath`, `+set logfile 2`, `+set developer 1`, private desktop, no
input; runner `.tmp/jk2-g2-anims/run-bots.py`. Every run ended at its
deadline with the level still running, no error and no unserved import, and
left the installation unchanged.

| Run | Build | Map | Seconds | 586 | 587 | 591 | 592 | Refused |
| --- | --- | --- | --: | --: | --: | --: | --: | --- |
| `committed-1` | committed, `6c5c7ed9` | `ffa_bespin` | 75 | | | | | none logged |
| `committed-280adf57` | committed, `280adf57` | `ffa_bespin` | 75 | | | | | none logged |
| `diagnostic-1` | diagnostic | `ffa_bespin` | 75 | 1,968 | 1,903 | 5,963 | 138 | 20 587s |
| `diagnostic-2` | diagnostic | `ffa_bespin` | 300 | 6,630 | 10,314 | 32,535 | 493 | 20 587s |
| `diagnostic-duel-1` | diagnostic | `duel_pit`, `g_gametype 3` | 180 | 6,967 | 6,105 | 20,959 | 941 | 20 587s |

- **The client setup completes** for both bots: 584 answers 1 for the Kyle,
  596 gives each its own instance, 589 finds `*r_hand` (0), `*l_hand` (1)
  and the seven bone bolts `cranium` to `Motion` (2 to 8), the saber's
  `*flash` (0), 592, 591, 590 and 595 are served. 599 and 593 were not
  reached: the bots' fields started empty, and the module's animation table
  was already loaded.
- **Every 586, 591 and 592 was served.** The 20 refused 587s are the first ten
  of each bot, which ask for the `Motion` bolt before any 586 has built: no
  build has reached it, which retail answers from an uninitialized record.
  Every later 587 answered.
- **The saber's query** (586, model 1 bolt 0) answered points 10 to 38 units
  from the bot's origin, from 9 below it to 31 above, with unit rotation rows
  whose axes turn from frame to frame: the saber in an animating hand (58
  answers logged). **`Motion`** (587, no angles) answered 5.7 to 13.5 units
  above the origin and within 2.5 of it across, its rotation rows 0.52 to
  0.64 long, unnormalized as Outcast's placement leaves them (67 logged).
- **The hit-location queries** were not reached: the seven bone bolts are
  asked for only in the module's saber damage path (procedure 129321, from
  126435 through 130381), and no bot attacked another in 555 seconds; nor did
  their AI reach its one bot snapshot call (import 209). Which game behaviour
  these answers drive is not observed.

## Presentation (invariant 11)

None. A dedicated server draws nothing; this slice changes only the answers
its game module receives.

## Tests

`tests/jka_server_attachment_tests.py` (Meson's `fntech3_jka_server_attachment`),
in ordinary and fast-math builds:

- the store: `outcast::Age`, `Truncate` and `Control` pinned frame for frame
  for loops, freezes, an ended clip, reverse clips, a clip of no length and
  refusals; the kernel keeping a bone on its parent's frames; the setter's
  sample, same-time replacement, reverse sample, clamps and set frame; a clip
  on model 0 zeroing its build time; and Outcast's kept answers for a clip
  equal to Academy's bits where the two timings reach the same frame;
- the production `SV_JK2GameCall` cases driven as the retail QVM drives
  them: 584 finding the word's models, the client setup in order, a guard
  word after every 32-bit field surviving 588 and 596, 596 leaving a held
  field alone, 593's name, 592's ten arguments reaching the store in retail's
  order, the per-frame clips with their transitions, and 599 freeing through
  listed fields only; and natively, the same imports with pointer-sized
  fields.

The pinned "584 answers no models" check is replaced deliberately. Nineteen
deliberate mutations each fail the file (`.tmp/jk2-g2-anims/mutate.py`, run
on a 64-bit host so the field width shows); they are listed in the JSON.

On a clean export of HEAD `280adf57` with this slice, the tree committed, all
1,499 targets build with no warnings; Meson's 250 project tests other than
the pytest wrapper give 248 passes and 2 skips for this machine, and the
complete Python suite passes 2,577 tests with 129 skipped. On the earlier
export of `6c5c7ed9` with the slice the same Meson tests gave 248 and 2, and
the Python suite passed 2,575 with 129 skipped.

## What stays open

- A retail answer: Outcast's dedicated server needs the disc.
- The hit-location queries and saber damage, which need bots that fight.
- REPLACE bends, case-blind bone and bolt names, a copy past an instance's
  end, the listen server's smoothing, and 597, 598 and 594, which the module
  never calls.
