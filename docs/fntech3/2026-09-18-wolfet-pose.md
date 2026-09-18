# Enemy Territory bone posing

## Scope and authority

This work follows `4bc5ef6`. It adds the model-space bone posing and weighted
vertex placement ET's skeletal drawing will sit on, built on the renderer's own
reader from that record. Nothing is drawn: there is still no model type, no
surface and no draw call.

Behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698),
chiefly its `renderer/tr_animation_mdm.c`. The reference is GPL-3.0-or-later
with additional terms. No implementation text is incorporated into fnTech3.

## Four skeletons, not one

The other Wolfenstein profile keeps its mesh and its skeleton in one file, so a
pose reads every frame from the same place. ET does not. Its reference entity
carries a model handle beside **every** frame index, so a pose may read its
current frame, its previous frame and its two torso frames from four different
skeleton files — which is exactly what its own renderer does, fetching each
frame's skeleton by that frame's own handle.

Two consequences are carried here:

- The bone table — each bone's parent, its distance from that parent and its
  torso weight — comes from the skeleton the **current** frame came from, as
  ET's does.
- Skeletons that do not describe the same bones are refused rather than blended.
  Posing a frame from one against another's joint would place limbs somewhere
  neither animation asked for, and nothing in the file format prevents a module
  from asking.

A frame blended against itself is only treated as needing no interpolation when
it also came from the same skeleton; the same index in two different files is
two different poses.

## What the pose does

Angles are stored as sixteen-bit turns and wrapped by quantising to that turn
rather than by `fmod`, which is how the compressed poses are written. Bones
carrying torso weight blend their lower and upper angles, and a bone fully
driven by the torso takes the torso angles outright. A child sits along the
direction its own offset angles describe, at its authored distance from its
parent; a root sits at the frame's parent offset. Every intermediate is checked
for finiteness, and a pose that cannot be evaluated returns nothing rather than
a half-filled skeleton.

One bone and its ancestry can be evaluated alone, which is what a tag lookup
needs, without paying for the whole skeleton. A whole-skeleton pose always
requires its torso frames, as ET's own bone calculation resolves them before it
begins; a single bone that carries no torso weight, and whose ancestors carry
none either, does not.

## Placing a vertex

A vertex is placed by its weights: each names a bone, an offset in that bone's
space and a share, and the normal follows the first weight's bone alone — which
is what ET's own deformation does rather than blending normals.

The weight's bone index names a bone in the posed skeleton, **not** a position
in the surface's bone reference list. ET's own header says the opposite; its
renderer and its shipped assets agree with the code here, as the
[renderer model record](2026-09-18-wolfet-renderer-models.md) sets out.

## Verification

Clean Windows x86 RelWithDebInfo build; no errors or warnings. The complete
automated suite passes.

A new executable regression builds a three-bone chain over two frames by hand,
so every posed value is one that can be worked out on paper, and checks: the
chain's rest position and axes; a quarter turn of yaw moving a child onto the
other axis; one bone resolved with only its ancestry; a frame index outside
either skeleton refused; a non-finite blend or torso axis refused; a missing
skeleton refused; two skeletons that disagree on their bone count or their
torso parent refused; torso frames required for a whole-skeleton pose and for a
selected bone that depends on them, but not for one that does not; a weighted
vertex landing where two bones and two offsets put it; the normal following the
first weight's bone; and a weight naming a bone the skeleton does not have
refused. A refused pose is checked to leave the caller's storage untouched.

## Remaining gates

- **Nothing is drawn.** The renderer can read and pose these skeletons; it has
  no model type for them, no surface, no level-of-detail collapse and no draw
  call. No visual claim is made and the ET client remains gated.
- No pose has been evaluated from a retail skeleton against a retail mesh; the
  evidence here is a hand-built fixture whose expected values are derivable.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

Nothing. The pose is a new header used only by its own regression.

## Unrelated observations

ET defines `MDM_TRANSLATION_SCALE` beside its weight record and never uses it
anywhere in its own source.

