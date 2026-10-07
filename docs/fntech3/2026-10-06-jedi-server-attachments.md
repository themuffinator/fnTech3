# Jedi server Ghoul2 attachments, 2026-10-06

The [attachments record](2026-10-06-jka-attachments.md) posed models linked
to another model from their parent's bolt, with retail's model order and tag
frame, in Jedi Academy's client game only. The servers kept a shared store
that did neither. This slice reads the server path in both Jedi engines,
gives Jedi Academy's server a store of its own that serves what retail's
server serves, and compares fnTech3's dedicated server with retail's through
one synthetic game module. Jedi Outcast's engine is read too, and a second
slice on 2026-10-07 serves its bolt imports from one build per time, as its
engine answers them (see [Jedi Outcast](#observed-in-retail-jedi-outcast)).
The [JSON companion](2026-10-06-jedi-server-attachments.json)
holds the identities, addresses, runs and comparison. No gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jamp.exe` 1.01 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `jk2mp.exe` 1.02a | `2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a` |
| `base/assets3.pk3:jampgamex86.dll` (Academy 1.01 game) | `b9e5ad492d35a3ec2fc7f6933df42ce9d3b05d3ec437e3bf12243a5d0d690850` |
| `base/assets0.pk3:vm/jk2mpgame.qvm` (Outcast 1.02a game) | `4810670f4f6ec3890d510e294dfd9fd4fb7f692b8d415b496aa132a0a79dc105` |
| synthetic module, Academy build | `6f0b54ea01646a4e6f58c3d4b7edd92c32c5973a66559864338bb05e25102591` |
| synthetic module, Outcast build | `52660b6a474e2942673a70fb130a90c599c9047e770365c61ffb6540df981b5d` |
| `fntech3.ded.exe`, HEAD `5603d244` (before) | `4a7f0d9b0ba8a08944cc13ae6585c4adcc56b3de5f15f54c5f3aa1afe253ba36` |
| `fntech3.ded.exe`, HEAD plus this slice (after) | `9cab110f1b36a781563c8be38a472a8cff57bb2f8079dfb78774c35c2bec6182` |

Both servers were built from one clean export of HEAD `5603d244` (MSVC 2022
x86, Meson `debugoptimized`, strict warnings, every development gate on),
the second after this slice was laid over it, and run from
`.tmp/runtime/jka-server-attach-before` and `-after`. Neither `jampded.exe`
nor any Jedi Outcast dedicated binary exists on this machine (a search of
both drives found none), so retail's dedicated server here is
`jamp.exe +set dedicated 1`. Every run used a private desktop, an isolated
home and no input; both installations' inventories were unchanged after
every run.

## Observed in retail: Jedi Academy's server path

Read with Capstone; no retail code is incorporated.

- **One Ghoul2 core for both modules.** `jamp.exe`'s game-import dispatcher
  bounds the import at 0x27a (0x45a670) and jumps through 0x45d34c. Imports
  587, 588 and 589 (0x45ca6b, 0x45cabd, 0x45cb19) each convert five
  arguments and call 0x4dfa00, the bolt-query core the client game's 294 to
  296 reach (0x41386d, 0x4138bf, 0x41391b). Import 594 (0x45cc00) calls the
  link store 0x4df940, as the client game's 319 does; 600 (0x45cd44) calls
  the model copy 0x4e04b0, as 308 does. A server's bolt query therefore goes
  through the skeleton build, model order, parent-bolt root and tag frame
  the attachments record measured; there is no separate server version.
- **The retail game asks.** The 1.01 game module calls 587 through one
  wrapper (0x200a6ce0, its syscall at 0x200a6d12) reached from 52 call
  sites, and also calls 589, 590, 594 and 600; never 588. **None of the 52
  sites reads the result**: each reads the matrix straight after the call
  (for example 0x20008e0d, 0x20083991). A refused query leaves the game with
  whatever its matrix held.
- **The engine skeleton is the same in both engines.** The built-in
  `*default` skeleton (the `.gla` named `*default` with its one bone,
  `ModView internal default`) is a 294-byte image at 0x547b98 in `jamp.exe`
  and at 0x4eaac8 in `jk2mp.exe`, byte for byte the same
  (`2af819307e7c79774511242f688a88ca573327a2a12326f2fda94ce6d7788507`).

## Retail's answers through the server path

A synthetic game module, independently written and carrying no game logic
(source `.tmp/jka-server-attach/server-attach-probe.cpp`, not committed),
repeats the attachments record's probe through the game imports and adds a
lightsaber. It loads Kyle, adds the `*r_hand` and `*l_hand` tags and the
`rhand` bone as bolts 0 to 2, and copies three Bryar pistols into slots 1
to 3, linked to the right hand's tag, to its bone, and to slot 1's `*flash`
(a chain), and `saber_1.glm` into slot 4, linked to the left hand's tag. It
asks for 72 bolts across six phases: before and after a bend of `thoracic`,
with entity angles, position and scale through each of 587, 588 and 589,
and after a bend of pistol 1's and the saber's own root bone. The same
module ran in retail `jamp.exe` and in fnTech3's two servers:

`python .tmp/jka-server-attach/run-server-probe.py --game jka --engine retail|fntech3 [--executable ...] --output ...`

| Server | Answered | Within tolerance | Rotation, max difference | Translation, max difference |
| --- | --- | --- | --- | --- |
| Retail `jamp.exe`, second run | 72 of 72 | bit for bit the first run | 0 | 0 |
| fnTech3, before | 18 of 72 | 6 | 1.65 | 0.588 units |
| fnTech3, after | 66 of 72 | 66 | 0.0000075 | 0.000107 units |

Tolerances are 0.00005 for rotation and 0.0002 units for translation, as in
the attachments record; retail's x87 arithmetic and fnTech3's single
precision part by about 6e-6 of the 17 to 51-unit chained offsets. Each kind
of answer after the change, against retail:

| Query | Answered | Rotation | Translation |
| --- | --- | --- | --- |
| Body tags `*r_hand`, `*l_hand` | 12 of 12 | 0.0000038 | 0.0000076 |
| Body bone `rhand` | 6 of 6 | 0.00000012 | 0.0000038 |
| Pistols' tags and roots, chain included | 36 of 36 | 0.0000075 | 0.000107 |
| The saber's `*blade1` and root | 12 of 12 | 0.0000031 | 0.0000076 |

In retail's answers each attached root is its parent's bolt bit for bit in
every phase before that model's own root is bent (pistol 1 and the saber
from the fifth phase on; the others in all six), and the bend then
post-multiplies it. fnTech3's after-server shows the same identities.

**Before this change**, fnTech3's server refused all 48 pistol and saber
queries: its store did not serve the `*default` skeleton those models name,
so `G2_ADDBOLT` answered -1 for their `*flash`, `*blade1` and root bone
(retail: 0 and 1) and the bend of their root answered 0 (retail: 1). The
attachments record had inferred that these queries differed from retail by
up to 56 units, from an offline replay with that skeleton on; on the server
they were refused outright. The 12 body-tag answers took this engine's
earlier frame and were up to 0.588 units and 1.65 off; the 6 bone answers
matched.

**Not answered after the change**: the 6 queries on the pistol's own
instance, whose only model links to itself, so retail's model order never
poses it. Retail answers the fixed axes with success; fnTech3's server
writes nothing and returns 0, where its client game refuses them by name.

## Observed in retail: Jedi Outcast

`jk2mp.exe`'s dedicated server refuses to start a level without the game
disc: after loading `ffa_bespin` it prints `Server didn't have CD`, because
its CD check (0x442410) fails and 0x44d00c raises its disc error. The check
is skipped only when `fs_restrict` is set, the restricted filesystem of a
demo version; that was not used, and no copy protection was bypassed. So no
retail Outcast answer was observed (run `retail-jk2-1`, stopped after 30
seconds with no marker). What follows is read with Capstone only, the first
day's reading completed on 2026-10-07; no retail code is incorporated.

- **The same link.** Import 590 (0x44c74a) stores the module's word at +0x3c
  of the 0xbc-byte model record (0x4a75f0). `InitGhoul2Model` (0x4a6e90)
  stores -1 there on both of its paths, into a free slot (0x4a70b6) and
  into the record it appends (0x4a7185). The record's constructor (0x4a8520)
  zeroes the word, but no model is initialized with it so.
- **The same order.** The sort at 0x479010 runs Academy's rounds: present
  models with no link by index, then repeatedly the linked models whose
  parent `(link >> 10) & 0x3ff` is in the previous round. It tests only the
  model index for presence, where Academy also tests a valid byte.
- **The same root and frame.** The skeleton build (0x4790f0) roots a model
  with no link on the fixed axes at 0x4eaa14 (Academy's values) and a linked
  one on its parent's bolt `link & 0x3ff` (0x479577..0x4795d4). Its tag
  frame (0x4778ae..0x477ca8) skins the surface's first three vertices and
  stores columns `|p0 - p2|`, `|p1 - p0|` made square to it and
  `-|(p1 - p0) x (p0 - p2)|`, origin `p2`: Academy's frame. A bone bolt is
  kept at the end of the bone transform as the bone's matrix times its base
  pose (0x476ee0), the store's bone palette.
- **The same bends.** Import 591's handler (0x44c763) reads eleven
  arguments and converts the bone's name, the angles and the model list,
  Academy's order. Its core (0x4a7520) sets the model's build time to 0 and
  calls 0x4aaf20, whose matrix (0x4aab40) takes Academy's orientation remap
  entry for entry (switch tables 0x4aae8c, 0x4aaea4 and 0x4aaebc for up,
  left and forward), builds it with `Create_Matrix` (0x4abcc0, `AnglesToAxis`
  written as columns) and conjugates it by the bone's base pose; the copy at
  +0x68 is what the transform reads (0x4aae62). The transform (0x476380)
  applies PREMULT as parent times override (0x476d39) and POSTMULT as
  parent times local times override (0x476d99), Academy's two. It reads the
  angle's blend time only for REPLACE (0x476b8b), whose path also rescales
  the stored matrix in place on every build (0x476c18).
- **The same copy.** Import 595 (0x44c83a) converts none of its four. Its
  core (0x4a7bf0) grows the destination to `modelTo + 1` with empty records
  when it is shorter, assigns the whole record (0x4a7b10: link, bolts with
  their kept matrices, bones and build time), and, when it grew, sets
  model 0's build time to 0 (0x4a7caa).
- **Different answers: one build per time.** The query core (0x4a7620; 586's
  handler 0x44c691 converts five of nine, Academy's order) checks the
  instance, `modelIndex` against the count and `boltIndex` against that
  model's bolts (0x4a762f..0x4a7679), calls the build (0x4790f0) unless the
  engine flag 0x1ae6634 is set, clears the flag (0x4a76c1) and reads the
  bolt's kept matrix at +0x10 of its 64-byte record. 587's handler
  (0x44c687) sets the flag and falls into 586's, so 587 answers the last
  build without building; a 587 refused for its indices leaves the flag set
  for the next query. The build returns at once when model 0's +0x98 holds
  the time (0x479148). Otherwise every model in sort order is stamped with
  the time (0x479450), posed, and keeps the matrix of each bolt: bones at
  the end of the transform, tags in the surface walk (0x4780b0, surfaces
  flagged as bolts and not below a surface without descendants). The
  record's constructor sets +0x98 to -1 (0x4a8578), and a bolt record is
  appended with only its first four words set (0x4aa8b0), so a bolt no
  build has reached answers whatever the stack left there. A bend of model
  0 is therefore seen by the next query at the same time; a bend of another
  model, a new link (590), bolt (589) or model (588) is not until the time
  changes, unless a copy grew the instance. The previous reading's "a bone
  bent between two queries at the same time is not seen" held only for
  models other than 0.
- **Placement without normalizing.** After the build the core scales the
  kept translation by each nonzero scale component, builds the world matrix
  from the angles and position (0x4abdb0), multiplies, and turns the
  rotation alone by `Create_Matrix(0, 270, 0)`, restoring the translation
  (0x4a7772..0x4a7827). That is Academy's placement, except that Academy's
  engine normalizes the bolt's rows first and Outcast's does not. Outcast
  has no `NOREC_NOROT` import.
- **No smoothing on a dedicated server.** The build blends each bone 30%
  back towards its last matrix when the bone's stamp is within 200 ms of
  `tr.refdef.time` (0x4796fd..0x47979a), if `r_ghoul2animsmooth` lies
  between 0.005 and 0.995, and renormalizes the bones afterwards if
  `r_ghoul2unsqashaftersmooth` exceeds 0.5 (0x4797b9). Both are registered
  with defaults `.3` and `1` and no flags by `R_Register` (0x47ed00, at
  0x47f721 and 0x47f738), which only `R_Init` (0x47fd70) calls, from
  `RE_BeginRegistration` (0x4858c0, reached through the renderer's export
  table). With `dedicated` set (cvar 0xa9f368), `Com_Init` skips `CL_Init`
  (0x429e29), so the renderer never starts, both cvar pointers stay null,
  and the build skips both blocks (0x4795ee, 0x4797be). `tr.refdef.time`
  (0x1a504e4) is written only by `RE_RenderScene` (0x488035). A listen
  server, whose renderer is running, does smooth its server's answers.
  `r_noserverghoul2` (a cheat, default `0`, registered lazily by the server
  model path at 0x484fb0) would skip builds when set.
- **Names without case.** 589's lookup (0x4aa210) compares surface names
  (0x4ac140) and bone names with `_stricmp` (0x4e2480), surface first, and
  reuses a free bolt record. fnTech3 compares exactly; every name the retail
  module asks for matches its file's case.

### The retail module asks, and why it does not yet reach the answers

`vm/jk2mpgame.qvm` reaches the engine by calling a negative address, so
counting `CONST -(N+1)` before `CALL` over its 249,138 instructions (decoded
by `.tmp/jk2-server-bolts/qvm.py`, not committed) finds every Ghoul2 call:

| Import | Sites | What the module asks |
| --- | --: | --- |
| 584 | 2 | whether a client's old instance, and the precached Kyle, hold models |
| 586 | 2 | the body's bone bolts the setup stored in the entity (from `cranium` on); model 1's bolt 0, the saber's `*flash` |
| 587 | 1 | the body's `Motion` bolt at the client's origin, without angles |
| 588 | 4 | Kyle and the saber, precached at `worldspawn` and again by the setup if missing, with the server marker -20 |
| 589 | 11 | `*r_hand`, `*l_hand`, `*flash` twice, `cranium`, `thoracic`, `lradius`, `rradius`, `ltibia`, `rtibia`, `Motion` |
| 590 | 2 | the precached saber linked to model 0's bolt 0 |
| 591 | 5 | `upper_lumbar`, `lower_lumbar` and `thoracic` POSTMULT (+X, -Y, -Z), `cranium` POSTMULT (+Z, -Y, +X), blend time 0 |
| 592 | 7 | clips |
| 593 | 1 | the skeleton's name, for its `animation.cfg` |
| 595 | 1 | the precached saber into the client's second place |
| 596 | 1 | the precached Kyle duplicated into the client |
| 599 | 1 | the client's old instance cleaned |

All three bolt-query sites discard the result and read the matrix. The
setup procedure (QVM instruction 108882, called with the `model` userinfo
key from 110404, which then adds the seven bone bolts) cleans the client's
instance, asks 584 of the precached Kyle, duplicates it into the client,
names the skeleton, adds the hand bolts, sets a clip, bends two bones,
links the precached saber, adds its `*flash` and copies it in as model 1.

**fnTech3 answers that 584 wrongly.** Retail's handlers convert no instance
word; fnTech3's Outcast dispatcher converted every one as a module address.
For a bytecode module that address is the VM's data base plus the word, so
584 answers "no models" for the Kyle it holds, the setup procedure stops
there, and the client never gets an instance. That, not the absence of the
calls, is why the earlier two-bot run reached no refused Ghoul2 import. Read
correctly, 584 would lead the module on to 596, 593, 592 and 599, which are
refused by number, ending the level when a client's model is set up. This
slice therefore leaves 584 as it was, documented at the call and pinned by
the test, and reads the word as retail does everywhere else: 586, 587, 589,
590, 591 and 595. For the retail module that changes nothing observable
today, since none of them is reached with a held instance. A native module
(`jk2mpgamex86.dll`, which `jk2mp.exe` also loads) passes real pointers and
reaches all of them.

## Jedi Outcast's server, 2026-10-07

- **Its own store keeps builds.** `ravenGhoul2Outcast` now serves the
  engine `*default` skeleton and attachments, as Academy's does, and keeps
  builds: each model carries the time it was last built and each bolt the
  matrix that build left (`fntech3::ghoul2::Store::Build`, `KeptBolt`). A
  build runs once per time, judged by model 0, in retail's model order, and
  keeps every bolt of every model it reaches; a model it does not reach
  keeps what it had. 591 sets the bent model's build time to 0, a copy that
  grows an instance sets model 0's, and the no-rebuild flag behaves as the
  engine's global does, outliving even a map change.
- **The four imports.** 586 builds and answers the kept bolt; 587 answers it
  without building; both place it with `pose::PlaceOutcastBolt`, Academy's
  placement without the normalization. 591 bends a bone with Academy's
  arguments and the store's measured bend matrix. 595 copies a model with
  its link, bolts, kept matrices and build time. The four numbers are named
  in `server_jk2_game.hpp` beside 584 and 588 to 590.
- **What it refuses, writing nothing.** A bolt no build has reached (retail:
  stack contents, answered with success); a model whose pose is outside the
  served boundary, and the models linked to it; a REPLACE bend (retail's
  in-place rescaling was not followed) and any clip (Outcast's timing was
  not read, and 592 is not served); a link to a bolt the parent lacks; and
  a copy to a place beyond the one after the last (retail grows the
  instance with empty records). PREMULT and POSTMULT bends are posed at any
  blend time, as Outcast's transform ignores it for them.
- **Not reproduced.** Case-blind names; the listen server's smoothing; the
  build's new-origin and no-model flags (no served import sets them); and
  `r_noserverghoul2`.

**Gameplay effect.** None yet for the retail module, as above: its client
setup still stops at 584. When it reaches them, the saber blade query (586
on model 1's `*flash`) and the bone-bolt and `Motion` queries would answer from
the same kept builds as retail's dedicated server. No retail answer could
be compared, because Outcast's dedicated server needs the disc; the
answers rest on the reading above and on tests that hold Outcast's store to
Academy's measured posing for each pose it serves.

## What this slice serves

- **A store per title.** `code/server/server_raven_assets.hpp` defines
  `ravenGhoul2Academy` and `ravenGhoul2Outcast`, and each title's game-import
  dispatcher binds the name `ravenGhoul2` to its own. A map change clears
  both.
- **Jedi Academy's server store** serves the engine `*default` skeleton
  (already measured for the client game, and the same image in retail's
  server path) and attachments. A model linked with `G2_SETBOLTINFO` is
  posed from its parent's bolt in retail's model order, surface bolts take
  retail's tag frame, and a query on an attached saber or weapon answers
  where the hand has put it. `G2_ADDBOLT` on a weapon or saber now finds its
  tag and bone, and a bend of its bone is served. Animation blends stayed
  off on the server in this slice, taken to have been measured through the
  client game only; that measurement ran through this server path, and the
  [pose clock record](2026-10-07-jka-server-pose-clock.md) serves them.
- **Jedi Outcast's server store** was the earlier one on 2026-10-06; the
  second slice gives it kept builds and its four bolt imports (see
  [Jedi Outcast's server](#jedi-outcasts-server-2026-10-07)).

**Gameplay effect.** The retail game's 52 bolt-query sites read the matrix
whatever the answer. On fnTech3's Academy server every such query on a
weapon or saber was refused, leaving those sites with whatever their
matrices held, and surface-tag answers on bodies were up to 0.59 units off.
Both then matched retail's server outside active blends, and inside them
since the [pose clock record](2026-10-07-jka-server-pose-clock.md). Which game behaviour
each site drives is not measured here. In the pinned OpenJK game code (a
maintained port of the same game), `WP_SaberPositionUpdate` asks every frame
for each blade's tag on the saber model copied into the body's slot 1 or 2
and linked to a hand (`codemp/game/w_saber.c` 8892, `g_client.c`
1497..1532), and traces the blade from that answer, so saber hits are the
likely consumer. That is inference from the reference, not observation of
the retail game.

**Differences recorded, not reproduced.** As in the client game: a link
naming a bolt its parent lacks, and a model retail's order never reaches,
are refused where retail reads past its list or answers a stale or fixed
pose. Retail's failed query writes its fixed fallback placed at the entity;
this server wrote nothing. The server posed at the time word the module
passes, where retail's core reads its own clock; in this probe no pose moved
between the two, so they could not be told apart. The
[pose clock record](2026-10-07-jka-server-pose-clock.md) measures and serves
both.

## Presentation (invariant 11)

None. A dedicated server draws nothing; this slice changes only the answers
a server's game module receives.

## Tests

- `tests/jka_server_attachment_tests.py` (Meson's
  `fntech3_jka_server_attachment`), in ordinary and fast-math builds,
  compiles the production store definitions from `server_raven_assets.hpp`
  over a filesystem served from memory, with the production
  `G2_SETBOLTINFO` and bolt-query cases of `SV_JKAGameCall` and the store
  binding read from its body:
  - an attached root answers its parent's tag, bone or chained tag bit for
    bit through the real import;
  - the body's surface bolt is retail's frame of its skinned first three
    vertices, and the weapon's tag is its own tag carried by that frame;
  - a self-linked instance and a link to a missing bolt are refused with
    nothing written;
  - a measured forward transition is served at the server's own clock ([pose clock record](2026-10-07-jka-server-pose-clock.md));
  - Outcast's store, set up alike, answers Academy's bits for every pose it
    builds, builds once per time judged by model 0, sees a bend of model 0
    at the same time and not one of model 1, answers the last build for
    587 and for the query after a 587 refused for its indices, refuses a
    bolt added since the last build, rebuilds after a copy that grew the
    instance and not after one that replaced a model, and refuses REPLACE
    bends and clips;
  - a map change clears both stores;
  - each dispatcher binds its own title's store.

  Six deliberate mutations fail it: Academy's dispatcher bound to Outcast's
  store; Academy's store without attachments, without the `*default`
  skeleton, or without blends; Outcast's without kept builds; and a map change
  clearing one store.
- `tests/raven_ghoul2_server_tests.cpp` keeps its tag test as the earlier
  frame on purpose, now pinned element by element, for the stores that keep
  it (the UI's and single player's), and adds retail's
  frame for a store that serves attachments on the same tag: no turn at
  (-21, 10, 30), where the earlier frame turns a quarter at (-20, 10, 30).
- `tests/jka_server_attachment_tests.py` also runs Jedi Outcast's
  production Ghoul2 cases of `SV_JK2GameCall` and its handle reader, driven
  as the retail QVM drives them (handles as 32-bit words, every pointer an
  offset into a data segment) and as a native module would. 588 and 589
  find the body's tag and bone and the weapon's on the engine skeleton, 590
  and 595 link the weapon and copy it into the body, 586 places its root
  exactly on the body's tag, both kept matrices are placed as retail's
  placement re-derived in double with the bind scale of 0.5 left in the
  bone's rows, a 591 bend of model 0 is seen at the same time, 587 answers
  the last build, refusals write nothing, a map change empties the store,
  and 584 still answers the QVM's word as no instance, pinned until it is
  corrected with 592, 593, 596 and 599. It runs in ordinary and fast-math
  builds.

On a clean export of HEAD `d56f0384` with this slice, built as above with no
warnings, Meson's 250 project tests other than the pytest wrapper give 247
passes and 2 skips for this machine; `fntech3_jka_pose_host` exceeded
Meson's 30-second limit at 31.5 s on the loaded machine and passes alone in
25 s. The 51 Python test files for the Jedi games, the Raven stores, the
fixtures that slice the dispatchers, and the documentation and packaging
checks pass 142 tests with 1 skip. The complete Python suite passed 2,547
tests with 129 skipped on HEAD `2fcbcf3d` with this slice, before
`d56f0384` (client renderers only) landed; there three more Meson tests
timed out under load and passed alone.

The second slice was checked on a clean export of HEAD `23ebc087` with it,
built the same way: all 1,499 targets with no warnings. Meson's 250 project
tests other than the pytest wrapper gave 247 passes and 2 skips. The other
one, `fntech3_jka_server_attachment`, now compiles four fixtures and ran past
Meson's 30-second default at 31.35 s, so it takes the Jedi tests' 180-second
limit and then passed. Thirteen deliberate mutations each fail the test file:
Outcast's store without kept builds, without the `*default` skeleton or
without attachments; a build that ignores model 0's time; 587 building; the
no-rebuild flag cleared by a refused query; a bend or a growing copy that
leaves the build time; REPLACE bends or clips posed; Outcast's placement
normalizing rows; handles converted as module addresses; and 587 dispatched
as 586. The complete Python suite passed 2,568 tests with 129 skipped and 2
failures under load, a packaging test out of system resources (WinError
1450) and an Enemy Territory fixture whose crashed mutant Windows Error
Reporting still held; both passed alone. On an export of HEAD `b1db7a74` with
the slice, exactly the tree committed, the 23 Python test files that read
`meson.build`, this ledger, the records, `release.py` or the generated
documentation, with the attachment tests, passed 460 tests with 2 skipped.

## What stays open

- On Academy's server, what the [pose clock record](2026-10-07-jka-server-pose-clock.md)
  leaves open; it serves blends, the clock a query poses at and the fallback.
- Jedi Outcast: a retail answer (its server needs the disc); REPLACE bends;
  case-blind bolt names; a copy past an instance's end; and the listen
  server's smoothing. 584's handle conversion, which kept the retail module
  from its client setup, is corrected, and 592, 593, 596 and 599 are served
  with Outcast's clip timing, in the
  [server clips record](2026-10-07-jk2-server-clips.md).
- Live play with sabers on fnTech3's Academy server, which no session here
  has exercised.
