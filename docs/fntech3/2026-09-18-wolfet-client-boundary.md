# Enemy Territory cgame import boundary

## Scope and authority

This work follows `a34d3ea`, with a clean worktree at the start except for
another session's in-flight RTCW files. It opens the first half of the ET client
boundary: the records ET's cgame passes, the renderer imports the shared
renderer can answer, ET's key vocabulary, and the dispatcher that routes them.
The ET client is still gated and nothing here has run inside one.

Layout and ordinal facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## What the boundary carries

ET's cgame hands the engine records that are its own, not another profile's.
Four were missing and are now declared and checked: the font, the glyph inside
it, the polygon buffer, and the client state its UI reads. Alongside them are
ET's render flags, scene flags, reference-entity ordinals, parser token types,
renderer driver and hardware ordinals, and connection states.

Two of ET's records differ from every other profile's in ways that would be
silent defects if shared:

- **The reference entity** carries a model handle beside *every* frame index —
  current, torso, old and old torso — because an ET pose names both an MDM mesh
  and the MDX skeleton it is posed with. RTCW's single MDS needs no such pairing
  and its record has none. ET also numbers its scene types in its own order: a
  splash is fourth and a tapered rail core sits between the two rail types.
- **The scene definition** sets bit 8 for a skybox portal where Quake means
  no-first-person, and bit 16 for underwater. Reading ET's word as Quake's would
  suppress the view weapon whenever a map used a sky portal.

Both now translate into engine-owned ET state kept separate from the RTCW state
beside it, so neither profile can inherit the other's fields.

## Keys

ET's key vocabulary ends at `K_AUX16`, four keys short of this engine's. From
the mouse wheel upward every ET number is four below the engine's number for the
same key, so passing one through unchanged would bind the wrong key — a wheel
scroll would arrive as a joystick button. ET's numbers are recorded in their own
table and translated in both directions, and a module number above ET's range is
refused rather than reaching an engine binding. ET's four key catchers are
reported without this engine's own two.

Against the pinned original, ET's key numbers turn out to equal RTCW's for every
key both name. That is an observation about two separate contracts, not a shared
one; each profile keeps its own table.

## Coverage

Of the 147 imports ET's cgame declares, **146 are handled**, counting what the
[sound](2026-09-18-wolfet-audio.md), [light](2026-09-18-wolfet-light.md),
[client services](2026-09-18-wolfet-client-services.md),
[text and files](2026-09-18-wolfet-text-and-files.md) and
[camera](2026-09-18-wolfet-camera.md) and
[scene geometry](2026-09-18-wolfet-scene-geometry.md) and
[texture identity](2026-09-18-wolfet-texture-id.md) and
[global fog](2026-09-18-wolfet-global-fog.md) records added after this one. The dispatcher
routes ET's own records and renderer services first, then its key imports, then
a table of imports whose meaning and argument shape are already this engine's.
An import that reaches the end of that chain is refused by name and ordinal. No
ET argument is dropped anywhere: an import this engine cannot answer exactly is
not answered approximately.

The 1 that remains open, and why:

| Ordinals | Imports | Why |
| --- | --- | --- |
| 177 | render to texture | A per-backend framebuffer copy that two of this engine's three renderers do not have; retail ET never calls it. |

The global fog (68) was open until the renderer gained ET's world-covering
fog volume; see the [global fog record](2026-09-18-wolfet-global-fog.md). The
decal imports (34, 35) were open until the renderers gained ET's decals; see
the [map load record](2026-09-18-wolfet-map-load.md).

## Verification

Clean Windows x86 RelWithDebInfo build; no errors or warnings. The complete
automated suite passes.

A new executable regression drives every translation this record adds: the four
new record sizes and the offsets inside them, all ten reference-entity ordinals
and the three ET-only render flags, an out-of-range type and a non-boolean axis
flag being refused, each frame index arriving with its own model handle, a tag
lookup carrying the pose and nothing else, the scene flags landing in ET's state
rather than Quake's, the fog fields crossing intact and never reaching the RTCW
view state, the polygon vertex and tag orientation, ET's gradient colouring only
the lower two corners, its rotated picture placing corners on a half-diagonal
circle in turns, non-finite inputs being refused, the renderer configuration
refusing an out-of-range enumerated value and terminating an unterminated engine
string, the parser token refusing both out-of-range types, the client state
refusing an unknown connection state, and the gamestate refusing an offset that
does not address its own data or a count past ET's 16,000 bytes. Every key in
ET's vocabulary round-trips in both directions, and the four keys above its range
are refused.

A source regression holds the coverage honest: it recomputes which of the 147
imports the dispatcher handles, checks that the open set is exactly the table
above, and checks that the refusal path still names the import.

## Remaining gates

- **Nothing here runs.** The ET client stays gated, so no ET module has called
  any of these imports. The evidence is source and unit only.
- The renderer had no MDM/MDX model type when this was written, so the model
  handles this boundary carries were transported, not drawn; it has both now,
  by the [drawing record](2026-09-18-wolfet-drawing.md).
- ET's demo and reverse interoperability surfaces remain untouched. Its UI
  imports are opened by the [UI record](2026-09-18-wolfet-ui-boundary.md), its
  sound surface by the [sound record](2026-09-18-wolfet-audio.md) and its camera
  paths by the [camera record](2026-09-18-wolfet-camera.md).
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

Three things outside the ET profile changed, each because ET now has a path
through shared code and none of them altering another profile's behaviour:

- The engine's reference entity and scene definition gained ET's own animation,
  effect and view state beside the RTCW state already there. Nothing reads the
  new fields yet; no RTCW or Quake field moved or changed meaning.
- `CL_FinishMove` publishes ET's cgame-supplied command flags and spectated
  client into the command's own ET tail, in a branch that runs only for ET.
- Two RTCW regressions compile slices of the client sources into their own
  fixtures. Both were extended to know that an ET path exists: one stubs the ET
  dispatch entry, and one declares the ET command fields its slice of
  `CL_FinishMove` now mentions. Neither change alters what those regressions
  assert about RTCW.

## Unrelated observations

None new in this slice.
