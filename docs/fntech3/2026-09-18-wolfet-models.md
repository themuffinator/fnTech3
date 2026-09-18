# Enemy Territory skeletal model decoding

## Scope and authority

This work follows `0af2a62`, with a clean worktree at the start except for
another session's in-flight RTCW files. It adds bounded readers for ET's MDM and
MDX skeletal formats, the layout facts behind them, and a probe that decodes
every such file in a legitimate installation. Decoding is one of the things
standing between fnTech3 and an ET client; this record closes the decode, not
the rendering. The ET client and default-release gates are unchanged.

Layout facts come from the pinned original
[ET file formats](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/qcommon/qfiles.h).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## The formats

ET splits what RTCW keeps in one MDS file. **MDM** version 3 holds the meshes:
surfaces of vertices, each weighted onto bones by index with an offset, plus
triangles, a collapse map and tags. **MDX** version 2 holds the skeleton: a bone
table and, per frame, a bounds record followed by one compressed pose per bone.
Version 2 is where the parent offset moved from the mesh to the skeleton.

Two differences from RTCW's MDS matter for the reader. ET's tag carries its own
axis where the MDS tag carries a torso weight, and ET's vertex is variable
length: its single declared weight is followed by as many more as the vertex
claims, so a surface cannot be indexed by a fixed stride and must be walked.

The layout table records every field offset and width for all eleven records,
and a regression checks that each record's fields tile it exactly, with the
reader's own size constants matching.

## The readers

Both readers take a buffer and a length and never trust the file. Every count is
checked against the format's own limit before use, every offset must be
non-negative, and every record extent is checked against the buffer with the
count multiplication bounded first so it cannot wrap. Fixed-width names must
terminate inside their own field. Triangle indices must address a vertex of
their own surface, and every bone index a bone that exists — optionally
cross-checked against the bone count of the MDX the mesh will be posed with.
MDX additionally requires that a bone's parent precede it, so a cycle cannot be
described, and that the torso parent be a real bone or absent.

Failures are named rather than boolean: wrong identifier, wrong version, count
out of range, offset out of range, record past end of file, unterminated name,
weight count out of range, vertex index out of range, bone index out of range.

## Verification

Clean Windows x86 RelWithDebInfo build; no errors or warnings. The complete
automated suite passes.

`scripts/probe_wolfet_models.py` extracts every MDM and MDX from an
installation's paks into a scratch directory and decodes each one. Against a
legitimate installation, **all 21 files parse**: 11 MDM and 10 MDX, with the
installation verified byte-for-byte unchanged.

| Kind | Files | Observed |
| --- | --- | --- |
| MDM | 11 | version 3, 4 surfaces each, 17 tags on the ten player bodies and 1 on the naked body, 818 to 875 vertices, 1,138 to 1,268 triangles, highest bone reference 52 |
| MDX | 10 | version 2, 53 bones, torso parent 1, 21 to 4,887 frames |

The highest bone a mesh references, 52, is inside the 53 the skeletons declare,
which is the cross-check the reader can make between the two halves.

A new executable regression drives both readers over a minimal valid file of
each kind and then malformed ones: null and every truncation short of a full
header, each format given the other's identifier, every wrong version, names
that never terminate, negative and over-limit counts, negative offsets, records
that would run past the file, a torso parent that is not a bone, and a bone
whose parent is itself, a later bone or out of range.

## Remaining gates

- **Decoding is not rendering.** Nothing here draws, skins, poses or animates
  anything; the bone poses are validated as present and addressable, not
  decompressed. No visual claim is made.
- The ET client is still gated, so no model has been loaded through a running
  client. The renderer has no MDM/MDX model type, and the cgame and UI surfaces
  that would ask for one are not implemented.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Unrelated observations

None new in this slice.
