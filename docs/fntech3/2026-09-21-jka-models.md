# Jedi Academy models: reading Ghoul2

## Scope

Every character, weapon and vehicle in Jedi Academy is a Ghoul2 pair: a `.glm`
mesh of weighted surfaces, and the `.gla` skeleton it names, which holds the
bones and a pool of compressed poses its frames index into. This slice adds
bounded readers for both and the checks that hold them to Raven's layout.

Every Ghoul2 model in a legitimate retail installation now reads through the
readers below, each mesh checked against the skeleton it actually names.
Nothing is posed, skinned or drawn: this is a format boundary, not a renderer,
and no runtime or rendering claim follows.

## The format

Both files carry Raven's identifiers, `2LGM` and `2LGA` in file order, at
version 6 (`codemp/rd-common/mdx_format.h:42`-`:51`). The traversal facts below
are how the retail renderer itself walks them
(`codemp/rd-vanilla/tr_model.cpp:740`-`:1030`).

A mesh is a 164-byte header naming the model and the skeleton it animates with,
then two structures:

- A surface hierarchy: an offset table of one entry per surface immediately
  after the header, then variable-sized records, each 144 bytes plus one index
  per child. Each record names its parent and its children by surface index.
- One block per level of detail, chained by a forward offset. Each block holds
  its own offset table and then a 40-byte surface record per surface, itself
  chained forward, carrying bone references, triangles, vertices and, straight
  after the vertices, their texture coordinates.

A vertex is 32 bytes: a normal, a position, one packed word and four bytes of
weight. The packed word holds the weight count in its top two bits, a five-bit
reference into the surface's own bone list per weight, and the top two bits of
each weight; the last weight is whatever the others leave, so weights always
sum to one. A surface therefore references at most 32 bones and a vertex is
weighted to at most four of them (`mdx_format.h:84`-`:88`).

A skeleton is a 100-byte header, an offset table of one entry per bone
immediately after it, then variable-sized bone records of 172 bytes plus one
index per child, each with a name, a parent, a base pose matrix and its
inverse. After the bones come the frames: three bytes per bone per frame,
least significant byte first, each a pose index. Those indices reach into a
pool of 14-byte compressed poses, whose size the highest index decides, because
the pool is not required to sit at the end of the file.

## What the readers guarantee

`code/compat/jka_model_format.hpp` reads both. Every offset and count is
checked against the supplied buffer before anything is read, and the readers
follow the retail renderer where it and the stored offsets could disagree: each
offset table is read from immediately after its header, as the renderer reads
it, the header's stated offset must be the first record straight after that
table, and every table entry must point at the record the walk actually
reaches. An offset that lies is refused rather than trusted.

Beyond bounds, the readers refuse what the renderer would misread:

- a surface index that is not its own position, or a surface whose back-pointer
  does not reach the file header;
- a parent, child or bone index outside the model's own surfaces or bones;
- a triangle corner that names a vertex the surface does not have;
- a vertex weighted to a bone its surface never references, or with a weight
  count outside one to four;
- a mesh that references more bones than the skeleton it names;
- a forward step -- to the next surface or the next level of detail -- that does
  not move forward, which is how a malformed file would loop.

## Evidence

| Check | Evidence |
| --- | --- |
| Retail models | All 127 meshes and 66 skeletons in a legitimate 1.01 installation read: 529,664 vertices across the meshes, 131,782 animation frames across the skeletons, and none refused. 86 meshes name a skeleton the installation ships and were read against that skeleton's own bone count; the other 41 name Raven's `*default` marker for a mesh with no skeleton, and none names a skeleton that is missing. The largest pair is `models/players/jedi_hf/model.glm` -- 214 surfaces, 3 levels of detail, 25,236 vertices, 29,873 triangles, four weights on its heaviest vertex -- with `_humanoid`, and the deepest skeleton is `_humanoid_kor2.gla` at 53 bones, 24,788 frames and 465,269 poses (`scripts/probe_jka_assets.py`) |
| Mesh | A synthetic two-surface, two-level mesh with a parent and child surface reads with its totals, its deepest bone reference and its heaviest vertex |
| Weights | The packed word round-trips: four weights with their references and top bits decode to the values written, and always sum to one; a single-weight vertex is entirely its bone |
| Skeleton | A synthetic three-bone, two-frame skeleton reads with its scale, its pose-pool depth and its widest bone, and one with no frames still describes its bones |
| Malformed models | Wrong identifiers and versions, unterminated names, negative and oversized counts, offsets that lie, indices that leave the model, a triangle past the vertex list, a vertex weighted to an unreferenced bone, a frame past the pose pool, and every truncation of both files are refused |

Tests: `tests/jka_model_tests.cpp` (`meson test fntech3_jka_models`). The
installed sweep is `scripts/probe_jka_assets.py --installation <path>`, which
reads the installation, writes only below its `--output` directory and keeps no
retail payload.

The installed sweep also settled one thing the sources alone did not: the
offset table after each header is followed by the records it points at, and the
header's own stated offset points at the first record rather than at the table.
Every retail model was refused until the readers matched that, which is the
kind of detail only an installation can settle.

No retail model, animation or asset is required, incorporated or shipped.
