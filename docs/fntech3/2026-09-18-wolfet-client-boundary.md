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

Of the 147 imports ET's cgame declares, **106 are handled**. The dispatcher
routes ET's own records and renderer services first, then its key imports, then
a table of imports whose meaning and argument shape are already this engine's.
An import that reaches the end of that chain is refused by name and ordinal. No
ET argument is dropped anywhere: an import this engine cannot answer exactly is
not answered approximately.

The 41 that remain open, and why:

| Ordinals | Imports | Why |
| --- | --- | --- |
| 36–51, 100, 111, 112 | the sound surface | ET's sound carries a per-sound volume, a start time on looping sounds, a stop-sounds flag on fades, and queries for a sound's length and the current sound time. The shared audio interface has none of these, and answering without them would silently discard what ET asked for. |
| 113–116 | the camera path surface | ET's camera paths need an ET-owned camera contract; the existing one is another profile's. |
| 34, 35 | decal projection and clearing | No shared renderer service. |
| 64 | the polygon buffer | No shared renderer service; the record is declared and checked but nothing consumes it. |
| 65 | the dynamic light | ET's light carries a radius and an intensity separately, plus a shader handle and a force flag. The shared light has one magnitude and no shader. |
| 68 | the global fog | No shared renderer service. Per-map fog (67) is handled. |
| 70, 71 | saving and restoring view parameters | No shared renderer service. |
| 165 | 2D polygons | No shared renderer service. |
| 176–179 | dynamic shaders, render to texture, texture identity, finish | No shared renderer service. |
| 162, 163, 170 | in-game popups and string translation | These reach ET's UI, which this record does not open. |
| 174, 175 | the binary message channel | The server half exists; the client half needs ET's client-side message state. |
| 16 | file deletion | ET's own delete also removes a directory tree, which only its UI uses. Implementing half of it would be worse than refusing it. |

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
- The renderer still has no MDM/MDX model type, so the model handles this
  boundary carries are transported, not drawn.
- ET's 121 UI imports are untouched, as are its sound, camera, demo and reverse
  interoperability surfaces.
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
