# Enemy Territory texture identity

## Scope and authority

This work follows `a3f5601`. It carries ET's texture lookup and records why the
four imports that remain on the cgame boundary stay refused. The boundary is now
at 143 of 147. The ET client is still gated.

Behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## The lookup

ET can ask for the index of an image it has already loaded, by the exact name it
was loaded under, and is answered -1 when the renderer holds no such image. The
comparison is exact, without case folding, because ET's is; a name differing
only in case is a different image to both. The index is a position in the
renderer's own image list and is valid only until that list is rebuilt, which is
also true of ET's answer. All three renderers answer it, and only for ET.

## The four that stay refused, and why

- **Render to texture (177).** ET copies a region of the framebuffer into a
  named image. That is a per-backend operation, and neither this engine's Vulkan
  renderer nor its ray-traced one has it; implementing it for one backend would
  make the import's behaviour depend on which renderer was running.

  Worth recording: **retail ET never calls it, nor the texture lookup above.**
  Both appear only inside a `TEST_API_RENDERTOTEXTURE` block in its limbo panel,
  a developer example that the shipped build does not define. The lookup is
  carried anyway because it is exact and cheap; the copy is not.

- **Decal projection and clearing (34, 35).** ET's decals are a renderer
  subsystem of their own: a four-plane projector clipped against world surfaces,
  grids and triangle soups, held per inline model, culled per frame and faded
  over its lifetime. This engine has a mark-fragment projector, which is a box
  projection with no lifetime, and building ET's decals on it would draw marks
  of a different shape in different places. That is the kind of approximate
  answer the rest of this boundary refuses to give, so it is refused here too.

- **Global fog (68).** ET's global fog is a fog volume the map's own shader
  flags as global; changing it means transitioning that volume's colour and
  depth over a duration and evaluating the transition each frame. This engine's
  global fog is a different, screen-space mechanism that no map flags, so there
  is nothing for ET's transition to act on without teaching the BSP loader ET's
  flag first.

## Coverage

The cgame boundary handles **143 of the 147** imports ET declares; the UI
boundary stands at 114 of 121, where six are ordinals retail ET refuses itself
and the seventh is a deliberate refusal to open a module-supplied URL.

## Verification

Clean Windows x86 RelWithDebInfo build across all three renderers; no errors or
warnings. The complete automated suite passes. A source regression checks that
every renderer registers the lookup, that it compares names exactly, that it
answers -1 for an unknown or empty name, and that it answers only for ET.

## Remaining gates

- **Nothing here has been drawn.** The ET client stays gated.
- The renderer still has no MDM/MDX model type, which remains the largest gap
  between fnTech3 and a usable ET client.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

Each of the three renderers gained one entry point and its registration, each
refusing any profile that is not ET.

## Unrelated observations

None new in this slice.
