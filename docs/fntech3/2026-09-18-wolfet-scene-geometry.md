# Enemy Territory polygon buffers and runtime shaders

## Scope and authority

This work follows `564c943`. It carries ET's batched scene geometry and its
runtime shader definitions, taking the cgame boundary to 142 of its 147 imports.
The five that remain are decal projection and clearing, the global fog, and
render to texture with its texture lookup. The ET client is still gated and
nothing here has been drawn.

Behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## The polygon buffer

ET's cgame batches its trail and flame geometry into one indexed buffer and
submits it once a frame. Its own renderer keeps a **pointer to the module's
buffer** and reads it again when the frame is drawn, so the module must leave
the memory alone until then. Retaining module memory is not something this
engine does, so the batch is expanded into its triangles at submission and
copied into the engine's own scene.

The triangles, their positions, texture coordinates, per-vertex colours and
their shader are identical. One thing differs and is worth naming: the engine
chooses a fog volume per triangle where ET chooses one for the whole batch, so a
batch that straddles two fog volumes would fog per triangle rather than as a
unit.

A batch is refused rather than drawn from whatever happened to be in the record
when its counts exceed ET's own limits, when its index count is not whole
triangles, or when an index names a vertex the batch does not carry.

## Runtime shader definitions

ET lets its cgame define a shader at runtime instead of shipping it in a
`.shader` file, and looks those definitions up **before** the loaded shader text.
All three renderers now do the same.

The list is engine-owned: the module's text is copied in, so nothing retains
module memory, and the list empties when the renderer stops. The three argument
forms behave as ET's do — neither argument empties the whole list, a name alone
removes one definition, text with no name is refused, and a name already present
is refused rather than redefined. Two guards go beyond the original: a
definition that does not declare the name it was submitted under is refused,
because nothing could ever find it, and the list is bounded in both count and
per-definition size.

The lookup answers only for ET, so no other profile's shader resolution changed.

## Coverage

The cgame boundary now handles **142 of the 147** imports ET declares. The five
still open are decal projection and clearing, the global fog, render to texture
and texture identity — each a renderer feature that needs its own work in all
three backends, not a boundary question.

## Verification

Clean Windows x86 RelWithDebInfo build across all three renderers; no errors or
warnings. The complete automated suite passes.

A new source regression checks that a polygon buffer is copied rather than
referenced and reaches the scene as triangles, that its counts are bounded and
its indices checked, that runtime definitions are searched before the loaded
shader text in every renderer, that a definition is copied rather than
referenced, that the three argument forms behave as ET's do, that the list is
bounded, that every renderer registers the entry point, and that the lookup
answers only for ET.

## Remaining gates

- **Nothing here has been drawn.** The ET client stays gated, so no batch has
  been submitted and no runtime shader resolved. No visual claim is made.
- The renderer still has no MDM/MDX model type.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

Each of the three renderers gained one entry point, its registration and one
call at the top of its shader lookup that returns immediately for any other
profile. No other profile's shader resolution or scene submission changed.

## Unrelated observations

None new in this slice.
