# Enemy Territory skeletal meshes reach the renderer

## Scope and authority

This work follows `7c9aa4d`. The previous slices built ET's skeletal arithmetic
— reading, posing, detail, collapse, tags — with nothing to drive it. This one
gives the renderer somewhere to put it: two model types, a surface type, a
loader for each file, a frontend path and a draw call, in all three renderers.

Behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698),
chiefly its `renderer/tr_animation_mdm.c`, `renderer/tr_model.c` and
`renderer/tr_main.c`. The reference is GPL-3.0-or-later with additional terms.
No implementation text is incorporated into fnTech3.

## Two models, not one

ET keeps a mesh and the skeleton that poses it in separate files, so each is a
model of its own here: `MOD_WOLFET_MESH` owns the surfaces that are drawn and no
frames at all, `MOD_WOLFET_SKELETON` owns the frames and no surfaces. Both
extensions register through the ordinary loader table.

This is why ET's meshes cannot share the frontend path the other formats use.
That path reads its bounds, its radius, its detail and its fog volume from
frames belonging to the model being drawn, and an ET mesh has none. Every one of
those comes instead from the skeleton the entity names beside the frame index,
so ET's meshes take their own path, as they do in ET.

A skeleton asked to draw itself draws nothing. ET's own dispatch has no case for
one and drops the client; a module naming the wrong handle is a module bug, not
a reason to end the session.

## The entity's torso

A gap in the previous slice is closed here. ET applies the entity's own torso
rotation as a pass over the posed skeleton: every bone that carries torso weight
is turned about the torso parent's resolved position, scaled towards the
identity by that weight. Without it a player's upper body never turns towards
what they are aiming at, whatever the module asks for.

Two details are ET's own. The scaling is a straight blend between the identity
and the rotation, not a partial rotation, so a bone at half weight carries an
axis that is no longer orthonormal — visible in the probe below, and what ET
computes. And the separate treatment ET's renderer keeps for attachment bones is
commented out of the shipped source, where the other Wolfenstein profile still
takes it: an ET tag turns with the torso exactly as a mesh bone does.

Only bones carrying torso weight are turned. A child of a turned bone that
carries none of its own stays where the hierarchy put it, because ET's pass
walks the weighted bones rather than the tree.

## One flag that had arrived and done nothing

ET's render flag for forced full detail reached the engine's record already and
nothing read it. It is not a skeletal setting: ET applies it where a model has
separate detail files, pinning such a model to its finest one, which is what its
limbo panel asks for when it draws a player's head, hat and rank. It is honoured
at that choice now, in all three renderers, and stays out of the skeletal path —
where ET reads detail from the reference entity's own flags instead.

## What the shipped skeletons say

Running the arithmetic over the installed models answered two questions the
source could not.

**Every frame writes the greater corner of its box first.** All 59,400 frames in
the ten shipped skeletons store `bounds[0]` above `bounds[1]`; not one stores
them the other way round. The frame's own `localOrigin` is the exact midpoint of
the pair, which is what rules out a misread field. ET merges two frames' boxes
by taking the lesser of the firsts and the greater of the seconds, so on this
data it computes the two boxes' *intersection*; its box test then enumerates all
eight corners, to which the order makes no difference. The merge here is written
the same way and produces the same box, and is only ordered low to high
afterwards — which changes no corner — so that the callers reading the pair as a
range, such as the dynamic light test, read it correctly rather than treating
every model as an empty box.

`R_ModelBounds` answers nothing for either ET type, as ET's does, rather than
handing a module a box that is inside out.

**A weight's bone index is bounded, a triangle's vertex index is checked.** A
mesh names no skeleton, so the bone a weight names can only be bounded by ET's
own maximum at load; the pose refuses an index its own skeleton does not hold. A
triangle index and a collapse entry are both checked when the surface is read,
because the draw call indexes the collapse map with one and follows the other.

## Verification

Clean Windows x86 RelWithDebInfo build across all three renderers; no errors or
warnings. The complete automated suite passes.

A new probe, `scripts/probe_wolfet_skeletal_draw.py`, pairs every installed mesh
with every installed skeleton — 110 pairs — and runs the renderer's own pose,
skinning, detail reduction, collapse and tag placement over them. The
installation is read only and is checked unchanged afterwards. Over 64,284
poses, 4,152,288 skinned vertices and 75,924 placed tags:

- Nothing was refused, and no value was other than finite.
- At rest, every posed bone, every skinned vertex and every placed tag lands
  inside the box its own frame declares, but for two of the ten skeletons:
  `body.mdx` reaches 3.58 units outside over its 4,887 frames, and `swim.mdx`
  2.14 over its 65. The other eight are exact. A model is about a hundred units
  tall, and the poses are stored as sixteen-bit angles, so a few units of
  disagreement with an authored box is what that storage costs.
- At rest, a placed tag's axis is orthonormal to within 1.5e-06.
- Under a quarter turn of the torso a tag's axis departs from orthonormal by up
  to 0.495, which is the blend described above and not an error.
- Every detail level from nothing to full, corpse and otherwise, produces a
  collapse map in which every triangle still indexes a retained vertex.

A new executable regression checks that a surface naming a triangle vertex it
does not have, a collapse entry that does not point backwards, a weight naming a
bone past ET's maximum and a bone reference past it are each refused when the
surface is read. Source regressions check that all three renderers carry both
model types, that the surface dispatch table lines up with the surface
enumeration, that both extensions load, that only the mesh draws, that a forced
full-detail entity skips the detail choice while the shadow-caster pass still
draws at full detail, and that nothing on this path consults the running game or
reaches for the other Wolfenstein profile's entity record.

## Remaining gates

- **No image has been produced.** The evidence is source, unit tests and
  arithmetic over retail assets. Nothing has been drawn on screen, because the
  ET client is still gated; no visual claim is made.
- ET meshes do not cast dynamic light shadows: neither the surface nor the model
  type is in the caster lists, so they are skipped rather than mishandled.
- ET queues no stencil or projection shadow for a skeletal mesh, as ET does not.
- `R_LerpTag`, which has no entity to name skeletons with, answers nothing for
  an ET mesh; `R_LerpTagEntity`, which does, answers from the entity.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

`tr_mesh_tags.h` gains an ET branch ahead of the existing ones. The shared model
readers gain offset-taking walkers that the index-taking ones now call, leaving
their behaviour unchanged. The shared mesh path's detail choice gains the
forced-full-detail flag described above, which only ET's record ever sets.

Four regressions that build the renderer's model record standalone now also
include ET's model types, because that record names them; their own checks are
unchanged.

## Unrelated observations

ET decides whether to interpolate the torso by comparing the entity's old torso
skeleton against its old *leg* skeleton rather than against its torso one. The
comparison here is between the two torso skeletons, which is the only reading
under which the test means anything; the two differ only when an entity's old
torso skeleton matches its legs' but not its own current one.
