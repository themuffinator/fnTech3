# Enemy Territory skeletal models in the renderer

## Scope and authority

This work follows `1656656`. It gives the renderer its own byte reader for ET's
MDM meshes and MDX skeletons, alongside the compatibility reader the
[model record](2026-09-18-wolfet-models.md) added, and checks the two against
every skeletal file in a legitimate installation. Nothing is drawn yet: this is
the reader the drawing will sit on.

Layout facts come from the pinned original
[ET file formats](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/qcommon/qfiles.h)
and from its `renderer/tr_animation_mdm.c`. The reference is GPL-3.0-or-later
with additional terms. No implementation text is incorporated into fnTech3.

## Why a second reader

The compatibility reader answers one question — is this file well formed — and
is written in C++ for the decode probe. The renderer needs something different:
a borrowing view it can walk surface by surface and vertex by vertex while
building geometry, in the C the renderers are written in, with no allocation and
no copy. So the renderer has its own, and the probe now runs **both** over every
installed file and fails if they disagree on a single count.

## Two things the shipped assets settle

Writing the second reader against real files turned up two facts that the
format's own header does not state, and one that it states wrongly:

- **Surfaces and tags are chained, not strided.** Both carry variable-length
  data of their own — a surface its vertices, a tag its bone references — and
  each ends with an offset to the next. Indexing either by its fixed record size
  reads the wrong record: it works for the one model that has a single tag and
  fails for the ten that have seventeen.

- **A vertex weight names a global bone.** ET's `qfiles.h` says the weight's
  bone index is "an index into the boneReferences, not the global per-frame bone
  list". The shipped assets say otherwise: a surface with 14 bone references
  carries weights naming bone 29, and one with 10 references names bone 52. ET's
  own renderer agrees with the assets, indexing its posed bone array directly.
  Following the comment would have weighted every vertex to the wrong bone.

- **A vertex may carry many weights.** More than a handful, in the retail player
  bodies; the reader bounds them by the skeleton's own bone limit rather than a
  small guess.

## Verification

Clean Windows x86 RelWithDebInfo build; no errors or warnings. The complete
automated suite passes.

Against a legitimate installation, **all 21 files parse** through both readers
with every count agreeing, and the installation is byte-for-byte unchanged:

| Kind | Files | Observed |
| --- | --- | --- |
| MDM | 11 | version 3, 4 surfaces each, 17 tags on the ten player bodies and 1 on the naked body, 818 to 875 vertices, 1,138 to 1,268 triangles, highest bone reference 52 |
| MDX | 10 | version 2, 53 bones, torso parent 1, 21 to 4,887 frames |

A new executable regression drives the renderer's reader over a minimal valid
file of each kind and then malformed ones: null and every truncation short of a
full header, each kind given the other's identifier, every wrong version, names
that never terminate, negative and over-limit counts, offsets past the end, a
torso parent that is not a bone, a bone whose parent is itself or a later bone,
a frame stride that follows the bone count, and a count large enough to wrap its
own stride. A source regression holds that every variable-length record is
walked rather than strided, and that the probe still compares both readers.

## Remaining gates

- **Nothing is drawn.** The renderer can read these files; it has no model type
  for them, no bone posing and no surface drawing. No visual claim is made, and
  the ET client remains gated.
- The bone poses are validated as present and addressable, not decompressed.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

Nothing. The reader is a new header, used only by the probe and its regression.

## Unrelated observations

None new in this slice.
