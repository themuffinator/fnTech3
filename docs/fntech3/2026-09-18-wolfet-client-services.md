# Enemy Territory client services

## Scope and authority

This work follows `075bddd`. It carries five more of ET's client-side services:
its binary message channel in both directions, its screen-space polygons, the
one saved view its portal sky needs, its pipeline finish and its in-game popups.
The cgame boundary reaches 134 of 147 imports and the UI boundary 112 of 121.
The ET client is still gated and nothing here has run inside one.

Behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## The binary message channel

ET gives its modules a private channel that the engine carries but never reads:
a cgame or game module hands over an opaque payload, the engine appends it after
the end of its next packet, and the module asks whether it has gone. Until now
only the server's outgoing half existed, which would have looked like a working
channel while nothing ever arrived.

All four halves are wired:

- The cgame's payload is stored, appended to the outgoing packet after the end
  marker, and reported as empty, waiting, or waiting-after-overflow using ET's
  own numbering from its `cg_public.h`.
- A payload that no longer fits is **kept** and marked overflowed rather than
  truncated, so the module can decide what to do; a length outside ET's own
  32 KiB is refused rather than clipped.
- The server parses whatever follows the end of a client packet and hands it to
  the game module with the command time the client's last movement carried.
- The client parses whatever follows the end of a server message and hands it to
  the cgame with the current snapshot time.

Both receiving halves pass a pointer into the packet, which is what ET's own
engine does. Only a native module may be handed one; a bytecode module would
need the bytes copied into its own memory, which this engine does not do for ET,
so a bytecode module receives nothing rather than a pointer it cannot use.

The client's buffer is ET's own 32 KiB and is kept in ET-only storage, so no
other profile's connection state widened by that much.

## Screen-space polygons

ET's only variable-shape 2D submission is a triangle fan of textured points,
each carrying its own colour. It is built exactly as ET's own backend builds it:
every triangle shares the first point, the screen depth is zero, and each point
keeps the colour it was submitted with instead of taking the current 2D colour.

The points travel inside the render command, so a fan cannot outlive the frame
that submitted it and no module memory is retained. A fan of fewer than three
points, or more than the recorded limit, is dropped the way ET's own renderer
drops a batch that will not fit its pool — a half-drawn fan is a different
picture from no fan at all. A non-finite point is refused outright.

## The saved view

ET renders its portal sky as a second scene and asks the engine to keep the
first scene's view parameters across it, so the detail-level computation is not
thrown away. One saved set is all ET ever asks for, and one is what is kept.
Restoring before anything was saved would publish an all-zero view, so it is
refused rather than done.

## Popups and finish

ET names an in-game menu by its own ordinal where the other Wolfenstein profile
names it by string, and its own client refuses to open one while disconnected or
replaying a demo; both are reproduced. Closing a popup is a no-operation in ET's
own client and is one here. The pipeline finish becomes the engine's own render
synchronisation, which is the same wait.

## Coverage

The cgame boundary now handles **134 of the 147** imports ET declares and the UI
boundary **112 of the 121**. The remaining ones are tabled in the
[cgame](2026-09-18-wolfet-client-boundary.md) and
[UI](2026-09-18-wolfet-ui-boundary.md) records.

## Verification

Clean Windows x86 RelWithDebInfo build across all three renderers; no errors or
warnings. The complete automated suite passes.

A new source regression checks that the message channel is wired in all four
directions rather than one, that neither receiving half hands a packet pointer
to a bytecode module, that an over-long payload is refused rather than
truncated, that an overflowed payload keeps its length so the module can retry,
that the three message states carry ET's own numbering, that a popup names a
menu by ordinal and is refused during a demo, that the polygon fan shares its
first point and keeps its own colours at zero depth, that a batch is bounded and
finite and travels inside its command, that restoring an unsaved view is
refused, and that every renderer registers the new entry points.

## Remaining gates

- **Nothing here runs.** The ET client stays gated, so no ET module has sent a
  message, drawn a polygon or opened a popup. The evidence is source and unit
  only, and no visual claim is made.
- The renderer still has no MDM/MDX model type.
- What remains open on the cgame side is file deletion, decals, the polygon
  buffer, global fog, the camera path surface, string translation, dynamic
  shaders, render to texture and texture identity; on the UI side, file deletion
  and copying, collision model loading, the superseded LAN counters, string
  translation and opening a URL. Six of those are ordinals retail ET refuses
  itself.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

Each of the three renderers gained one render command, its backend case, three
entry points and their registrations. Every one refuses a profile that is not
ET. The client's outgoing packet and incoming message paths each gained one ET
call that returns immediately for any other profile, and the server's client
message parser the same.

## Unrelated observations

None new in this slice.
