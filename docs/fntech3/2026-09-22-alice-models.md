# Alice's model formats and their readers

## Scope

This record establishes all four of Alice's model formats and builds a bounded
reader for each: the `.tik` definitions that index everything, the `TAN `
version 2 vertex-animation models, the `SKL ` versions 2 and 3 skeletons and
base frames, and the `SKAN` versions 2 and 3 animations -- 801, 694, 79 and
1,402 files, 2,976 in all, every one of which a legitimate retail installation
ships and every one of which reads. Nothing here poses a model, applies a
weight or a skin, resolves a shader or draws anything: the readers locate and
bound records, and decode none of them. See the
[foundation record](2026-09-22-alice-contracts.md) for the profile and the
[compatibility ledger](COMPATIBILITY.md#alice-gate-matrix) for what remains
open.

## What Alice ships

Alice has no `.md3`. Its models are TIKI: a text `.tik` definition names the
binary geometry and animations it assembles, and those come in three binary
formats across two generations.

| Format | Ident | Version | Files | What it is |
| --- | --- | --- | --- | --- |
| `.tan` | `TAN ` | 2 | 694 | per-frame vertex animation with tags |
| `.skb` | `SKL ` | 2 | 15 | skeleton and base frame |
| `.skb` | `SKL ` | 3 | 64 | skeleton and base frame, later revision |
| `.ska` | `SKAN` | 2 | 104 | skeletal animation |
| `.ska` | `SKAN` | 3 | 1,298 | skeletal animation, later revision |

There are also 801 `.tik` definitions, which are text and index the rest.
Every one of the five binary groups begins the same way: a four-byte ident, a
version, and a 64-byte name field holding the file's own leaf name. That shared
prologue is what lets a loader tell them apart before reading anything else.

The engine's own strings name the split: `TIKI_LoadAnim: %s has incompatible
version %i (supported %d - %d)` and `TIKI_LoadSkel, Tried to load %s as a
skeletal base frame`. So the engine accepts a *range* of animation versions,
which is why both version 2 and version 3 appear in one installation.

## The TAN 2 format

A `.tan` holds one model's frames and surfaces together. The 176-byte header
declares the frame count, the tag count, the surface count, a total time and
delta, where the frames begin, where the first surface begins, one offset per
tag, and the file's own length. Each surface carries a 104-byte header followed
by its triangles, its collapse map, its texture coordinates and its vertex
positions, and declares where the next surface begins, so the surfaces form a
chain rather than an array.

Positions are quantized: three unsigned shorts and a packed normal per vertex
per frame, against the scale and offset that frame carries. Every surface
stores a full set for every frame, which is what makes these files large for
their vertex counts.

Layout and ordering come from Ritual's published F.A.K.K.2 v1.02 SDK
(`qcommon/qfiles.h`), whose `TIKI_ANIM_IDENT` and `TIKI_ANIM_VERSION` are
exactly Alice's `TAN ` and 2. It is used under its own non-free licence for
record names and layouts only; no implementation text is incorporated.

## How it was verified

The layout was not assumed from the SDK. All 694 installed models were checked
against it and all 694 pass:

- **Every file declares its own length**, and `ofsEnd` equals the real file
  size on all 694. That is the check that catches a truncated file before any
  record is read.
- **Every count is inside the compiler's own limits**: frames at most 2,048,
  tags at most 16, surfaces at most 32, vertices at most 1,200 per surface and
  triangles at most 4,096.
- **Every surface animates over the model's own frames.** A surface's frame
  count equals the header's on all 1,059 surfaces.
- **Every block stays inside the surface that declares it.** The triangles,
  collapse map, texture coordinates and vertex positions all lie between a
  surface's header and the end it declares, so no surface can read the next
  one's vertices.
- **The surface chain tiles the file.** Following each surface's declared end
  reaches the file's end exactly.
- **Every tag block holds a name and one orientation per frame**, inside the
  file, on all 457 tags.
- **Every name field is a clean, NUL-terminated string.**

One assumption did not survive that check, which is the reason for running it.
The SDK declares a `TIKI_MAX_LODS` of four, and the surface's `minLod` field
looks like a level number; it is not. Across the installed models it runs from
0 to 381 and never exceeds the surface's own vertex count, so it is how many
vertices survive at the lowest detail. A reader that bounded it by four
refused 624 of the 694 files.

## The reader

`code/compat/alice_tan_format.hpp` locates every record and bounds it against
the supplied buffer before reading. It refuses a wrong ident, a wrong version,
a file shorter than its header, a declared length that is not the real one, any
count outside the compiler's limits, an offset that leaves the file or reaches
back into the header, an unterminated name, a surface whose frame count
disagrees with the model's, a surface whose declared end leaves the file, and a
block that reaches past the surface that declares it. Nothing is decoded.

`tests/alice_tan_tests.cpp` covers it with independently authored fixtures; no
retail model is used. It builds a model with two frames, two surfaces of
different sizes and two tags -- enough that a wrong stride lands somewhere it
should not -- and checks the parse, then feeds it thirty-odd malformed
variations and checks each refusal by name.

Separately, the committed reader was run over all 694 extracted retail models
as a probe. All 694 read, with the same totals the independent check produced:
8,782 frames, 1,059 surfaces, 91,016 vertices, 110,875 triangles and 457 tags.

## The SKL skeleton and base frame

**Correction:** this record first said the reference SDK does not declare
this format. It does -- its `qcommon/qfiles.h` defines a `TIKI_SKEL_IDENT`
and a skeleton header -- so that was wrong. It changes nothing below: every
field was derived from the 79 installed `.skb` files and is admitted only
because all 79 agree, and the SDK's declaration was not used as a source.

A `.skb` is a 92-byte header, a flat array of bones, and a chain of surfaces:

| Where | Bytes | Contents |
| --- | --- | --- |
| header | 92 | ident `SKL `, version, name[64], surface count, bone count, bone offset, surface offset, file length |
| bone | 72 | parent index, a flag, name[64] |
| surface header | 96 | ident `SKL `, name[64], triangle count, vertex count, `minLod`, and offsets to the triangles, the vertices, the collapse map and the surface's end |
| triangle | 12 | three vertex indices |
| vertex | 24 + 20 per weight | fixed data with the weight count at +20, then the weights |
| weight | 20 | a bone index first |
| collapse | 4 per vertex | one index each |

Each field was established by something the files themselves settle:

- **The header size is 92** because the bone offset is exactly 92 on all 79
  files, and the bone block reaches the surface offset exactly on all 79.
- **The bone stride is 72** because `(surfaceOffset - 92) / boneCount` is
  exactly 72 on every one of the 79 files, for bone counts from 4 to 131.
- **The file declares its own length**, and it equals the real size on all 79.
- **A bone's first field is its parent.** It is -1 or an index, and on all
  3,910 bones it is always an *earlier* bone, never a later one and never
  itself. That is a strong result: a random integer field would not be
  topologically ordered. It also means a walk up the hierarchy terminates
  without tracking what it has visited, which the reader relies on and the
  test proves. The deepest chain in the installation is 21 bones.
- **A bone's second field is a flag**, because across 3,910 bones it takes
  exactly two values: 0 on 3,852 of them and 1 on 58.
- **Skeletons have more than one root.** 165 roots across 79 files, so a
  reader that assumed a single root would refuse most of the installation.
- **The surface header is 96** because the triangle offset is exactly 96 on
  every surface, and the triangles reach the vertex offset exactly.
- **The collapse map is four bytes a vertex**, because its offset plus four
  times the vertex count is the surface's declared end on all 278 surfaces.
- **`minLod` is a vertex count**, as it is in the `TAN ` models: it never
  exceeds the surface's own vertex count on any of the 278, and equals it on
  60 of them.
- **The vertex block has no stride.** It is the one part that has to be
  walked, and the walk is what fixes its shape: with 24 fixed bytes, the
  weight count at +20 and 20 bytes a weight, all 79 files land exactly on
  their collapse maps, every weight count is within the skeleton's bone count
  and every weight's first field is a valid bone index. Three other candidate
  shapes -- 20+16, 28+20 and 24+16 -- fail on the first file.

`code/compat/alice_skb_format.hpp` reads it under those rules, and
`tests/alice_skb_tests.cpp` covers it with authored fixtures: a four-bone
hierarchy with two roots and two surfaces whose vertices carry different
weight counts, so a fixed-stride walk lands somewhere it should not. The
refusals include a parent that is not earlier, a skeleton with no root, a
weight naming a bone that does not exist, a weight count that does not fit,
and a `minLod` larger than the surface has.

Run over all 79 extracted retail skeletons, the committed reader parses every
one: 3,910 bones with 165 roots and a deepest chain of 21, 278 surfaces,
46,965 vertices, 54,704 triangles and 59,145 weights, with every bone's walk
to a root terminating.

## The SKAN animations

The 1,402 `.ska` animations are the largest group in the installation. They are
the one format here that declares no file length and carries no offsets at all,
so nothing inside one can be found by following a directory. What makes them
readable anyway is that the storage is fixed-stride, and the file's own size
proves it.

Versions 2 and 3 differ by one inserted integer at the front, so the same four
fields sit four bytes apart between them:

| Field | Version 2 | Version 3 |
| --- | --- | --- |
| (inserted) | -- | +72 |
| frame count | +72 | +76 |
| bone count | +76 | +80 |
| total time | +80 | +84 |
| frame time | +84 | +88 |

Two independent checks fix all four at once:

- **`totalTime` equals `numFrames` times `frameTime`** on every one of the
  1,402 files, in both versions, to within float precision. Frame counts run
  from 1 to 320 and frame times cluster at 0.05 and 0.0333 seconds. A wrong
  field position would not reproduce that identity 1,402 times.
- **The bone count is a real skeleton's.** The animations declare 49 distinct
  bone counts and the installed skeletons have 46; 1,399 of the 1,402
  animations name a count some installed `.skb` actually has. The most common
  is 131, which is Alice's own skeleton. The three that do not match -- counts
  23, 24 and 30, across three files -- are animations for skeletons this
  installation does not ship.

### The storage

With the frame and bone counts fixed, the rest falls out of a single equation.
Solving `size = header + frames x (frameHeader + bones x pose)` over the whole
corpus gives exactly one triple per version, and each reproduces **every**
file's size to the byte:

| Version | Files | Header | Frame header | Pose |
| --- | --- | --- | --- | --- |
| 2 | 104 | 112 | 40 | 28 bytes |
| 3 | 1,298 | 108 | 40 | 16 bytes |

That is what the two generations differ by: version 3 compressed the pose and
nothing else. A 28-byte pose is a four-float quaternion and a three-float
offset; a 16-byte one is a four-short quaternion and four shorts more.

The quaternion reading is not an inference from the sizes. Taking the first
four floats of every version 2 pose, **all 241,629 of them are unit length**.
Taking the first four shorts of a version 3 pose and scaling by 1/32767, the
same holds. Across all 1,402 files and all 3,254,001 poses the committed reader
locates, **3,253,939 begin with a unit quaternion -- 99.998%**. Sixty-two do
not, which is an authoring outlier rate, not a layout error: a stride that was
wrong by even one byte would fail on essentially all of them rather than on one
in fifty thousand.

### The reader

`code/compat/alice_ska_format.hpp` reads the header, derives the stride and
locates every frame and pose. Because the format carries no length, the check
that does the work is that the file is **exactly** the size its counts require:
one byte more or fewer and it is not this layout. It refuses a wrong ident, a
version outside 2 and 3, a count outside the installed range, an unterminated
name, a size that disagrees with the counts in either direction, and a file
whose total time is not its frame count times its frame time -- the identity
that fixed those fields in the first place.

`tests/alice_ska_tests.cpp` covers both versions with authored animations,
checking that the located poses tile each frame and that the last one ends
exactly at the end of the file, and feeding it the malformed shapes: a file one
byte short and one byte long, a frame or bone count off by one in either
direction, and a broken timing identity.

Run over all 1,402 extracted retail animations, the reader parses every one:
104 version 2 and 1,298 version 3, 43,640 frames and 3,254,001 poses.

One incidental observation, recorded because it will look like a bug to
whoever reads these files next: one version 3 animation carries `0xcccccccc`
in that leading integer where the other 1,297 carry zero. That is MSVC's
uninitialized-stack fill, written into a shipped asset.

## The TIKI definitions

The 801 `.tik` files are the index the rest of the pipeline hangs from: each
one names the directory its binaries live in, maps every surface to a shader,
and maps every animation name to the `.tan` or `.ska` file that holds it.
`code/compat/alice_tiki_format.hpp` reads that structure and leaves every
command uninterpreted. It allocates nothing, holding views into the caller's
buffer, because a definition is read once and discarded.

Four things the corpus contains had to be carried into the reader, and each of
them would have broken a straightforward one:

- **There is a preprocessor.** Two definitions open with C-style `#define`
  lines *before* the `TIKI` magic, so the magic cannot simply be demanded of
  the first token. The reader skips a preprocessor line whole, because its
  tokens are not part of the grammar -- looking only at the first token of each
  line is what made those two files parse.
- **There are macros.** `$define kingworkdir /sound/character/king` declares
  one and `$kingworkdir$/pain1.wav` expands it, so the sound paths a definition
  names cannot be resolved without expanding them. They are declared at file
  scope, above the first block, which is the second thing that had to be fixed:
  a reader that only looked for them inside blocks found none of the 109.
- **One shipped definition does not close its last block.**
  `c_chess_red_king.tik` ends inside its `animations` block, one brace short.
  The retail engine accepts it, so the reader reports an unterminated file
  rather than refusing it, and still returns the entries it did read.
- **Not every reference resolves.** 49 of the definitions' animation
  references name a file the installation does not contain, all cut content --
  most of them Tweedledee's flying animations. A loader that treats a missing
  animation as fatal cannot load this installation.

All 801 definitions parse: 801 with a `setup` block and a `path`, 801 with an
`animations` block, 753 with an `init` block, one unterminated, 1,089 surface
to shader mappings, 3,447 animation entries of which 1,942 carry a block of
timed commands, and 109 macros, every one of which expands from its own name.

That command-block count is worth one sentence, because getting it wrong is
easy: a reader that marks an entry as having commands whenever a brace
follows it, without checking that no other token intervened, attributes 185
blocks to the wrong entry.

`tests/alice_tiki_tests.cpp` covers the reader with authored definitions
carrying every one of those wrinkles at once, plus the shapes a malformed file
takes: no magic, a closing brace with nothing open, a statement whose value
runs off the end, and more records than a definition may hold.

When this record was written no model was posed, no weight applied, no skin
resolved, no tag followed and nothing drawn; the
[TIKI runtime record](2026-09-23-alice-tiki.md) now does all of that.

## The compiled definitions

**Correction:** this section first called the 797 `.cik` files cinematic
model instances. They are not: each is a compiled image of the engine's
in-memory TIKI record, which retail loads in preference to the `.tik` text,
and the [TIKI runtime record](2026-09-23-alice-tiki.md) establishes the whole
layout. What was measured here still holds. Every one is
version 1 followed by a 128-byte name field, and the cross-reference is
perfect: **all 797 carry a clean NUL-padded name, all 797 name a `.tik`, and
all 797 of those definitions exist in the installation** -- not one dangling
reference, which is a stricter result than the `.tik` files' own animation
references manage.

So every image points only at things the readers above already cover. The
rest of the record -- counts, bounds, origin, the surfaces, the animations and
their commands -- is laid out in the runtime record, which reads it.
