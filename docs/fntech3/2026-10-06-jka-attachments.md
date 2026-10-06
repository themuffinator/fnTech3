# Jedi Academy Ghoul2 attachments in the client game, 2026-10-06

The [live-join record](2026-10-06-jka-live-join.md) left fnTech3's `jka-mp`
client stopped in the client game's first frame after a map change, at a
Ghoul2 instance holding two models: Kyle's `models/players/kyle/model.glm`
and the Bryar pistol's world model
`models/weapons2/blaster_pistol/blaster_pistol_w.glm` in its second slot.
This slice serves such instances. A model linked to another is posed from
its parent's bolt with its own skeleton and drawn through Fn's skeletal
path, and the client game's bolt queries answer from the same posing. The
bolt link, the model order, the parent-bolt root and the surface tag's frame
are measured in retail `jamp.exe` 1.01 with Capstone and then checked
against retail's own answers. The [JSON companion](2026-10-06-jka-attachments.json)
holds the identities, addresses, comparison and run results. No gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jamp.exe` 1.01 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `base/assets1.pk3:models/players/kyle/model.glm` | `3c02518cb8920ec6205b9f3981f70204f7d6588f8753e6887bc67647029cfa28` |
| `base/assets1.pk3:models/players/_humanoid/_humanoid.gla` | `7c9c6ee6a37848623563b2c0156d080f65c87cfce196bae78e81163b2f6fbad6` |
| `base/assets1.pk3:models/weapons2/blaster_pistol/blaster_pistol_w.glm` | `74474f37dbde7dfa5f37e9b75a0ca840f8126f0ee6196cfbf05b652a47f66cd9` |
| `fntech3.exe` (runtime `jka-live-join`, rebuilt in place) | `0c68056b09e5e7084029ee4d82b288e8e490269d45e230208188b0f4458b3614` |

The runtime is HEAD `192c321f` plus this slice, built from a clean export
(MSVC 2022 x86, Meson `debugoptimized`, strict warnings, every development
gate on; all 1,499 targets, no warnings). The three asset hashes are of
read-only extractions used by the offline comparison below; nothing was
written to the installation, and its inventory was unchanged after every
run.

## Observed in retail

Read with Capstone; no retail code is incorporated.

- **The link.** Each model record of an instance is 0xc8 bytes, and its link
  is the word at +0x3c. `InitGhoul2Model` stores -1 there (0x4de93e), so a new
  model stands on its own. `SetBoltInfo`, cgame import 319 (0x413d44 into
  0x4df940), stores the module's word unchanged (0x4df96f); it checks that the
  index is below the model count but not that it is non-negative.
  `CopySpecificGhoul2Model` (import 308, 0x4e04b0) replaces the destination
  through the record's assignment, which carries +0x3c over (0x4dea73). So
  the weapon instance's link, set before the copy, arrives in the player's
  slot 1.
- **The order.** A sort at 0x498be0 lists every present model whose link is
  -1, by index, then repeatedly every linked model whose parent
  `(link >> 10) & 0x3ff` is in the previous round. A model whose chain of
  parents never reaches an unlinked model, such as one linked to itself, to a
  missing model or into a cycle, is never listed.
- **Posing.** The skeleton build (0x49a050) and the renderer (0x499bf0) walk
  that list. Each model's root is composed onto a root matrix: the fixed
  multiplayer axes at 0x547d5c for a model of its own (0x4999d0 returns them
  when no model asks for a new origin), or, for a listed model after the first
  whose link is not -1, the bolt that `G2_GetBoltMatrixLow` (0x499850)
  returns for parent `(link >> 10) & 0x3ff`, bolt `link & 0x3ff`.
  `G2_TransformGhoulBones` (0x498240) keeps that matrix in the bone cache
  (+0x40, 0x498461..0x498473), and the bone transform composes it with the root's own
  frame for bone 0 (0x497c02..0x497c10; the multiply at 0x497160 is
  `out = A x B`). The renderer uses the same parent bolt (0x499e3e..0x499ea9).
- **Bolts.** `G2_GetBoltMatrixLow` reads the bolt list with no bound
  (0x49986b..0x499878). A bone bolt is the bone's final matrix times its base pose
  (`skel + 0x48`). A surface bolt goes through 0x498df0, whose model-tag
  branch (0x499476..0x499848) skins the surface's **first three vertices**,
  not a triangle's corners. Its frame has columns `|p0 - p2|`,
  `|p1 - p0|` made square to the first, and `-|(p1 - p0) x (p0 - p2)|`, with
  the origin at `p2`. A model with no bone cache answers the fixed axes.
- **Bolt queries.** The query core (0x4dfa00) bounds the model and bolt
  indices, builds the whole instance's skeleton (0x4dfae2 into 0x49a050)
  and then reads the requested model's bolt (0x4dfaf9). A query on an
  attached model therefore answers where its parent has put it.

**Reference** (OpenJK `1a6a643`, a maintained port): `codemp/ghoul2/G2.h`
names the same widths (`MODEL_SHIFT` 10, `BOLT_SHIFT` 0, 10 bits each);
`G2_Sort_Models`, `G2_ConstructGhoulSkeleton`, `R_AddGHOULSurfaces` and
`G2_ProcessSurfaceBolt2` (`iG2_TRISIDE_LONGEST` 0, `iG2_TRISIDE_SHORTEST` 2,
`MDX_TAG_ORIGIN` 2) agree with every reading above, and its
`G2API_InitGhoul2Model` sets the link to -1. The bolt-list size check that
OpenJK's `G2_GetBoltMatrixLow` adds is not in retail. Only these format
facts are taken from it.

## Retail's own answers

A synthetic game module, independently written and carrying no game logic,
ran inside retail `jamp.exe` as a dedicated server on loopback, windowed on a
private desktop with no input and an isolated home. It loads Kyle, adds the
`*r_hand` and `*l_hand` tags and the `rhand` bone as bolts 0 to 2, and copies
three pistols into slots 1 to 3. Slot 1 is linked to the right hand's tag,
slot 2 to its bone, and slot 3 to slot 1's `*flash` tag, a chain. It then asks
for 60 bolts across six phases, among them a bend of `thoracic`, the three
query variants with entity placement and scale, and a bend of an attached
model's own root. Its answers show the measured semantics directly. Each attached root equals its parent's bolt bit for bit (slot 1's
root is `*r_hand`'s matrix, slot 2's is `rhand`'s, slot 3's is slot 1's
`*flash`), in every phase. The pistol instance's own model, linked to itself
and never posed, answers the fixed axes with success.

The same operations replayed offline through fnTech3's store, against
read-only extractions of the same three files, give:

| Store | Answered | Rotation, max difference | Translation, max difference |
| --- | --- | --- | --- |
| Attachment-serving (this slice) | 54 of 60, all within tolerance | 0.0000075 | 0.000107 units |
| Earlier store | 60 of 60, 6 within tolerance | 1.99 | 56.2 units |

The largest translation residuals are on the chained pistols' 17 to 51-unit
offsets (about 6e-6 of the value), where retail's x87 arithmetic and
fnTech3's single precision part. The six answers not given are the self-linked
instance, refused by name as explained below.

## What this slice serves

- **Links in every Raven store.** A model's link starts at retail's -1.
  Stores that do not serve attachments still keep the word and read nothing
  into it.
- **Attachments, opted into by the multiplayer client game's store.**
  `PoseOrder` reproduces retail's sort. A linked model is posed from its
  parent's bolt with its own skeleton, so a weapon on the generated
  `*default` skeleton sits on the hand's tag. Its own angle bends then
  post-multiply that frame, as retail's did. Surface bolts take retail's tag
  frame. A parent posed once per instance serves all its attachments.
  `pose::Evaluate` takes the root matrix as a parameter, defaulting to the
  fixed axes.
- **The scene.** `client_jka_cgame_render.hpp` poses every model retail
  would, in retail's order, and registers each as its own engine-owned MDR
  keyed by instance and model index, with that model's skin and visibility.
  Each model becomes one Fn entity with the instance's placement. A model the
  sort never reaches is not drawn.
- **Bolt queries.** Import 294 and its no-rotation variant answer an
  attached model from its parent's bolt.

**Differences recorded, not reproduced.** A link naming a bolt its parent
lacks is refused by name, where retail reads past its list. A bolt query on
a model retail's sort never reaches is refused by name. Retail answers from
whatever bone cache that model last had: the fixed axes if it never had one,
otherwise a stale pose. A collapsed tag is refused, where retail carries
zero vectors on.

**Not changed, for other profiles.** The server's stores (Academy and
Outcast multiplayer) and the UI's and single player's keep their earlier
behaviour: links are kept but not posed through, and surface bolts keep this
engine's earlier frame (origin at the first triangle's first corner). The
comparison above shows that frame is not retail's, by up to 56 units for
attached models. A server's bolt queries on an attached saber or weapon
therefore still differ from retail's. That correction belongs to the server
gameplay gate, against `jampded.exe` and Outcast's engine, and is not made
here. The UI serves neither `UI_G2_ADDBOLT` nor `UI_G2_SETBOLTON`, so no UI
model carries a link and its renderer keeps its one-model limit.

**Later the same day** Academy's server was given a store of its own with
these semantics, measured through retail `jamp.exe`'s game imports. Its
queries on attached models had in fact been refused, the shared server store
lacking the `*default` skeleton, not answered 56 units off; Outcast's store
is unchanged ([server attachments record](2026-10-06-jedi-server-attachments.md)).

## Presentation (invariant 11)

Every model, attached or not, is drawn through the existing Fn skeletal
runtime-MDR path. It uses Fn materials (the GLM's shaders or the model's
registered skin), entity lighting, dynamic lights and shadows, and the
canonical FOV, HUD, capture, liquid and enemy-highlight hooks, as the body
already was. LOD 0 is kept. Retail's renderer may smooth bone matrices
between frames (`mSmoothingActive`, bone cache +0x84). Fn poses the body and
its attachments from one unsmoothed palette, so a weapon cannot drift from
the hand; no retail smoothing option is added.

## Live sessions

`scripts/probe_legacy_sessions.py --profiles jka-mp --directions join
--runtime .tmp/runtime/jka-live-join --sessions main kick`. The fnTech3
client runs against retail `jamp.exe` on loopback, windowed on a private
desktop, with pure admission and no sound or input.

| Run | Client | Steps |
| --- | --- | --- |
| `sess-1` | this slice | join, client chat, server chat, **map change** pass; `map_restart`, reconnect, server quit fail; kick passes |

The two-model instance no longer stops the client. On `mp/ffa3` its first
frame now stops at a plain model entity whose render flags carry 0x800,
`Jedi Academy cgame unsupported Raven renderer entity flags (type=0,
flags=0x800)`. In Academy multiplayer 0x800 is `RF_RGB_TINT` and 0x400 is
`RF_FORCE_ENT_ALPHA` (pinned OpenJK `codemp/rd-common/tr_types.h`; single
player numbers its flags differently). The pinned OpenJK client game's
`CG_Item` sets both on every item for its first second after it spawns, to
fade it in, and on placeholder and force-side-greyed items; that the retail
client game does the same is inferred from it, not measured. If so, the first
frame after any map change or `map_restart` with items present meets them. OpenJK's renderer
replaces every stage's colour source with the entity's colour under
`RF_RGB_TINT` (`tr_shade.cpp`). Serving it needs a measured retail contract
and an Fn adaptation in all three renderers' stage colouring. That is
recorded as the next boundary, a renderer slice of its own; it was entered
before the scene flush, so this run does not itself pose the attachment.

Diagnostic variants, each from the same export with temporary changes that
are not committed and with the committed binaries restored bit for bit
afterwards:

- With 0x800 ignored on plain entities (`sess-2-diag`), the same frame stops
  at 0xc00, both flags together.
- With both ignored (`sess-3-diag`), `mp/ffa3`'s frame stops at the ambient
  set update (import 48) for its present `city_outdoor` set, active ambient
  playback being a named refusal.
- With both ignored and `mp/ffa1` again as the second map (`sess-4-diag-ffa1`,
  through `.tmp/jka-live/probe_variant.py --maps mp/ffa1,mp/ffa1`), the scene
  flushes. A diagnostic print shows six flushes of the two-model instance, in
  order Kyle (link -1, root at height -24, as retail's bolt queries put it in
  the earlier record) then the pistol (link 0, model 0's bolt 0). The
  pistol's root follows the animated hand from frame to frame. `map_restart`
  then passes, and the client game's restart stops at import 201,
  `CG_R_CLEARDECALS`. Retail's 201 only calls the renderer's `ClearDecals`
  and returns zero (0x412c71..0x412c7f). fnTech3 serves no Academy decal
  (import 205 is unserved), so its pool is empty; that import is the boundary
  after the tint.
- An engine screenshot taken after that map change (`sess-5-diag-capture`)
  shows the client is a spectator, so the posed instance is not in view.
  Which entity carries it remains unestablished, and no capture shows the
  attached pistol.

## Tests

- `tests/jka_cgame_attachment_tests.py`, in ordinary and fast-math builds:
  - link decoding;
  - retail's order, with cycles, self links and missing parents;
  - that copies and duplicates carry the link;
  - hand-solved and re-derived tag frames, collapsed tags, and a first
    triangle that is not the first three vertices;
  - attached roots equal to tag and bone bolts and to a chain;
  - parent animation and bends of the attached root;
  - named refusals for a missing parent bolt, an unreached model and an
    unserved parent pose;
  - earlier stores keeping their frame and ignoring links;
  - the production scene path: a body with a linked pistol registers two Fn
    MDRs, the pistol's one bone being the body's tag; import 294 on the
    attached model; an unreached link drawing only the body; a replaced slot
    re-registering.

  All nine deliberate mutations of the served semantics fail it (tag origin,
  column order, the square-up step, the normal's sign, triangle corners,
  links ignored, cycles drawn, a 0 default, a 9-bit shift).
- `tests/jka_cgame_render_tests.py` follows the per-model cache key. A
  self-linked model is no longer refused at queueing; it is not drawn. Two
  unlinked models are drawn as two entities.
- `tests/raven_ghoul2_server_tests.cpp` expects retail's -1 default.

On the export of HEAD `192c321f` with this slice, Meson's 249 project tests
other than the pytest wrapper give 246 passes and 2 skips for this machine.
`fntech3_jka_pose_host` exceeded Meson's 30-second limit at 35.8 s in that
parallel run; alone it passes in 19.3 s, as the previous slice's export does in
18.4 s. The Python suite passes 2,539 tests with 129 skipped.

## What stays open

- **`RF_RGB_TINT` and `RF_FORCE_ENT_ALPHA`** on plain entities, the next
  refusal in this direction; after them, `ffa3`'s active ambient set and
  import 201. `map_restart`, reconnect and server quit stay unobserved with
  the committed build.
- The server's stores' links and tag frame (see above; Academy's since
  served), and multi-model UI instances.
- A capture of an attached model in view; which entity carries the live
  instance.
- Retail's frame-to-frame bone smoothing is not reproduced (see
  presentation); LODs other than 0, model scale and embedded skeletons
  remain outside this path.
