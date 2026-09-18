# Enemy Territory camera paths

## Scope and authority

This work follows `eaaea62`. It carries ET's four spline camera imports, taking
the cgame boundary to 140 of its 147 imports; the seven that remain are all
renderer features. The ET client is still gated and no path has been followed.

Behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## Retail ET ships no camera paths

Every pak in a legitimate installation was enumerated: it contains **no camera
path files** at all. The same enumeration finds 11 MDM, 10 MDX, 9 TAG and 81
configuration files, and — as the [navigation record](2026-09-17-wolfet-navigation.md)
already reported — no AAS files either.

So this surface, like the navigation one, cannot be reached from stock content.
It is carried because a third-party ET map may ship a path, and because refusing
the import would kill a client the moment a map placed a spline entity. The
evidence for it is source and unit only, and no camera has been followed.

## The ordering ET actually uses

The shared spline evaluator distinguishes two orderings that the Wolfenstein
lineage disagrees on: whether the position is started before or after the
timeline is built, and whether the feather event ramps the speed at both ends.

Read against the pinned original, ET clears the position's velocities and starts
it **before** building the timeline, and its feather event ramps the speed up
over the first second and down over the last. Both are the behaviours the
evaluator calls `SinglePlayer`. That name is the evaluator's, not a claim that
ET is another profile's single player: ET keeps its own 64 slots and its own
camera-mode flag, and neither profile can see the other's.

## What crosses

- Loading refuses a path name that begins with a separator, carries a drive
  letter or contains a parent reference, and a failed load leaves no older path
  in the slot.
- An invalid slot examines no filename, no time and no output pointer at all,
  which is the original's behaviour.
- Only the primary slot drives the camera input policy, as ET's own client does,
  and stopping releases that policy without stopping a path already running.
- A sample writes an origin, a pitch and a yaw, and a field of view. Roll
  belongs to the module and is never written. Overlapping output buffers would
  make the result depend on write order, so they are refused.
- Fade events reach the console as ET's do. The path file supplies only typed
  numeric parameters, never executable text, and fades are dispatched after
  evaluation so a module callback cannot invalidate a path the evaluator still
  holds references into.

## Coverage

The cgame boundary now handles **140 of the 147** imports ET declares. The seven
still open are decal projection and clearing, the polygon buffer, the global
fog, dynamic shaders, render to texture and texture identity — all renderer
features this engine does not have.

## Verification

Clean Windows x86 RelWithDebInfo build; no errors or warnings. The complete
automated suite passes.

A new source regression checks that all four imports are handled, that the two
profiles keep separate slots and camera-mode flags, that ET uses the ordering
its own source shows and not the other, that an invalid slot is rejected before
anything is read, that only the primary slot touches the input policy, that
overlapping outputs are refused and roll is never written, that a path name
cannot escape the search path and a failed load clears the slot first, and that
cameras are cleared with the cgame.

## Remaining gates

- **No path has been followed.** The ET client stays gated, and a legitimate
  installation ships no camera files to follow even if it were open.
- The renderer still has no MDM/MDX model type.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

The client's connection state gained one ET-only flag. The shared spline
evaluator is unchanged; ET holds a second instance of it.

## Unrelated observations

None new in this slice.
