# Jedi server Ghoul2 attachments, 2026-10-06

The [attachments record](2026-10-06-jka-attachments.md) posed models linked
to another model from their parent's bolt, with retail's model order and tag
frame, in Jedi Academy's client game only. The servers kept a shared store
that did neither. This slice reads the server path in both Jedi engines,
gives Jedi Academy's server a store of its own that serves what retail's
server serves, and compares fnTech3's dedicated server with retail's through
one synthetic game module. Jedi Outcast's engine is read, but its store is
not changed. The [JSON companion](2026-10-06-jedi-server-attachments.json)
holds the identities, addresses, runs and comparison. No gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jamp.exe` 1.01 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `jk2mp.exe` 1.02a | `2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a` |
| `base/assets3.pk3:jampgamex86.dll` (Academy 1.01 game) | `b9e5ad492d35a3ec2fc7f6933df42ce9d3b05d3ec437e3bf12243a5d0d690850` |
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
seconds with no marker). What follows is read with Capstone only.

- **The same link.** Import 590 (0x44c74a) stores the module's word at +0x3c
  of the 0xbc-byte model record (0x4a75f0). A model `InitGhoul2Model`
  places in a free slot takes -1 (0x4a70b6); the record's constructor
  (0x4a8520) zeroes the word, and whether every other path then stores -1
  was not followed. Import 595 (0x44c83a) passes four unconverted numbers
  to 0x4a7bf0, the shape of Academy's model copy; that body was not read.
- **The same order.** The sort at 0x479010 runs Academy's rounds: present
  models with no link by index, then repeatedly the linked models whose
  parent `(link >> 10) & 0x3ff` is in the previous round. It tests only the
  model index for presence, where Academy also tests a valid byte.
- **The same root and frame.** The skeleton build (0x4790f0) roots a model
  with no link on the fixed axes at 0x4eaa14 (Academy's values) and a linked
  one on its parent's bolt `link & 0x3ff` (0x479577..0x4795d4). Its tag
  frame (0x4778ae..0x477ca8) skins the surface's first three vertices and
  stores columns `|p0 - p2|`, `|p1 - p0|` made square to it and
  `-|(p1 - p0) x (p0 - p2)|`, origin `p2`: Academy's frame.
- **Different answers.** Outcast's engine does not work a bolt out when it
  is asked for. Its build computes every bolt and keeps each matrix in the
  bolt's own 64-byte record, and returns at once when model 0 was already
  built for the same time (+0x98). Import 586's core (0x4a7620) rebuilds
  and then reads the kept matrix; 587 (0x44c687) first sets a flag
  (0x1ae6634) that skips the rebuild, so it answers whatever the last build
  left. A bone bent between two queries at the same time is not seen until
  the time changes. The build also reads a cvar (0x1a4fb80) and, for values
  between 0.005 and 0.995, blends bone matrices within 200 ms; that was not
  followed. Outcast has no `NOREC_NOROT` import.

**Jedi Outcast's store is not opted in.** The posing it would serve matches
Outcast's engine as far as it was read, but Outcast's answers are kept per
build in a way the store does not reproduce, no retail answer could be
observed, and fnTech3's Outcast server serves none of the imports that
would ask (586, 587, 591 and 595 are refused by number). Opting in would
change nothing a module can observe. It belongs with a slice that serves
those imports and their per-build answers.

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
  tag and bone, and a bend of its bone is served. Animation blends stay off
  on the server: they were measured through the client game, whose pose
  clock this server path does not share (see below), so a query during an
  active blend still answers nothing.
- **Jedi Outcast's server store** is the earlier one, unchanged.

**Gameplay effect.** The retail game's 52 bolt-query sites read the matrix
whatever the answer. On fnTech3's Academy server every such query on a
weapon or saber was refused, leaving those sites with whatever their
matrices held, and surface-tag answers on bodies were up to 0.59 units off.
Both now match retail's server, outside active blends. Which game behaviour
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
this server writes nothing. The server poses at the time word the module
passes, where retail's core reads its own clocks; in this probe the two are
the same level time. Those two remain the open server bolt failure-output
and time items in the ledger.

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
  - a measured forward transition refuses on the server store;
  - Outcast's store keeps `*default` metadata-only, ignores links and keeps
    the earlier frame;
  - a map change clears both stores;
  - each dispatcher binds its own title's store.

  Six deliberate mutations fail it: Academy's dispatcher bound to Outcast's
  store; Academy's store without attachments, without the `*default`
  skeleton, or with blends; Outcast's with attachments; and a map change
  clearing one store.
- `tests/raven_ghoul2_server_tests.cpp` keeps its tag test as the earlier
  frame on purpose, now pinned element by element, for the stores that keep
  it (Outcast's server, the UI's and single player's), and adds retail's
  frame for a store that serves attachments on the same tag: no turn at
  (-21, 10, 30), where the earlier frame turns a quarter at (-20, 10, 30).

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

## What stays open

- Animation blends, the time a server query poses at, and the fixed
  fallback a failed query writes, on Academy's server.
- Jedi Outcast: a retail answer (its server needs the disc), its bolt
  imports, its per-build answers and its smoothing cvar.
- Live play with sabers on fnTech3's Academy server, which no session here
  has exercised.
