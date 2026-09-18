# Enemy Territory detail reduction and tags

## Scope and authority

This work follows `ffc33a9`. It adds the last two pieces ET's skeletal drawing
needs before a draw call: how a mesh sheds detail with distance, and where a tag
sits once the skeleton is posed. Nothing is drawn: there is still no model type,
no surface and no draw call.

Behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698),
chiefly its `renderer/tr_animation_mdm.c`. The reference is GPL-3.0-or-later
with additional terms. No implementation text is incorporated into fnTech3.

## Detail

Detail follows the projected size of the model's bounding sphere, scaled by the
viewer's setting and the model's own, then reduced by the bias and the model's
own bias. A model that meets the near plane — a view weapon, say — projects to
nothing and is drawn whole.

ET's reference entity carries two flags that bear on this, from its own
`tr_types.h`: bit 8 asks for a lower detail than the distance would choose and
halves the fraction, and bit 32, which ET's own comment attaches to corpses,
takes four fifths. The other Wolfenstein profile reads bit 8 differently in
single player; ET has only the one reading, and this code has only ET's.

The surface's own minimum detail is honoured, with one exception ET made
deliberately: a mesh flagged as a corpse may fall below that minimum, but never
below the 35% floor ET chose for it.

## Collapse

Every vertex past the retained count folds onto an earlier one, following the
chain all the way back to a vertex that survives. A map entry that does not
point backwards would let a mesh fold onto itself or read a vertex that has not
been resolved, so it is refused rather than followed.

## Tags

ET's tag is not simply a bone. It carries its own axis and its own offset, both
in the space of the bone it hangs from, so a tag's placement composes the posed
bone with the tag's own frame. The other Wolfenstein profile's tag *is* its
bone, and using that rule here would attach weapons at the joint rather than
where the model asks.

A named tag is found from a starting index, as ET's lookup does, so a caller can
walk past one it has already used.

## Verification

Clean Windows x86 RelWithDebInfo build; no errors or warnings. The complete
automated suite passes.

A new executable regression checks: a model projecting to nothing drawn whole;
detail following the projected size, both scales and both biases; each flag's
effect and the two together; flags this engine knows but ET does not attach to
detail changing nothing; results staying inside zero and one; a non-finite input
drawing whole; the surface minimum honoured and the corpse exception with its
floor; never returning more vertices than a surface holds; the collapse chain
followed to a surviving vertex; a map entry that does not point backwards, a
zero retained count, one past the surface and a small buffer all refused; a tag
composing its own quarter turn and offset with its bone's transform; a named tag
found from a starting index and not found past it; and a tag naming a bone the
pose does not hold refused.

## Remaining gates

- **Nothing is drawn.** Reading, posing, detail, collapse and tags all exist as
  arithmetic; the renderer still has no model type for ET's meshes, no surface
  and no draw call. No visual claim is made and the ET client remains gated.
  *Closed by the [drawing record](2026-09-18-wolfet-drawing.md).*
- None of this has been run against a retail mesh; the evidence is hand-built
  fixtures whose expected values are derivable. *Closed by the
  [drawing record](2026-09-18-wolfet-drawing.md), which runs it over every
  installed mesh and skeleton.*
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

Nothing. Both are new headers used only by their own regression.

## Unrelated observations

None new in this slice.
