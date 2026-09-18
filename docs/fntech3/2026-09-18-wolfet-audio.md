# Enemy Territory sound boundary

## Scope and authority

This work follows `a020f95` and closes the surface both previous records named
as the next blocker: ET's sound imports, open on the cgame and UI boundaries
alike. With it, the cgame reaches 125 of its 147 imports and the UI 110 of its
121. The ET client is still gated and none of this has run inside one.

Behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## Three values nothing else submits

ET's sound contract is not the other Wolfenstein profile's. Three values it
sends had no place to go, which is why these imports were refused rather than
answered approximately:

- **A volume with every one-shot.** ET's modules pass 127 unless a caller chose
  otherwise, and `trap_S_StartSoundVControl` exists precisely so they can. The
  engine now carries that volume to the mixer, where 127 is the level the other
  profile always plays at, so an unchanged volume sounds unchanged.
- **A start time with every looping sound.** ET says when a loop is considered
  to have begun so that every listener hears it at the same point in its cycle
  rather than wherever the local mixer clock happens to be. The submitted time
  becomes a sample offset in the software mixer and an `AL_SAMPLE_OFFSET` in the
  OpenAL backend. A loop with no submitted time keeps the shared behaviour
  exactly, so the other profile is untouched.
- **A stop-sounds flag on fades.** ET can ask a fade to stop what it faded once
  the fade completes, and its own client stops everything at once if a second
  stopping fade begins before the first finished. Both backends now do that.

ET also asks for two things nothing else does: the length of a sound in
milliseconds, and the mixer's current sound time, which it reports one second
ahead of its own sample clock. Both come from the backend's real state; neither
is a figure this engine invented. An unknown handle answers -1, as ET's does.

ET's master fade is kept in its own record beside the other profile's, so
neither can see the other's level.

## What is not claimed

`CG_S_GETVOICEAMPLITUDE` answers zero. ET declares the import but its own client
never fills the amplitude table, so a talking animation is never driven by a
figure this engine invented. `CG_S_STARTSTREAMINGSOUND` returns zero for the
same reason: ET's own client sets no stream identity there, and this engine does
not issue one.

## Coverage

With the sound surface carried, the cgame boundary handles **125 of the 147**
imports ET declares and the UI boundary **110 of the 121**. Every value ET
submits on those paths is passed through; nothing is answered approximately.

The imports that remain open are unchanged from the
[cgame](2026-09-18-wolfet-client-boundary.md) and
[UI](2026-09-18-wolfet-ui-boundary.md) records apart from the sound ordinals
this one removes, and each of those records carries its own table.

## Verification

Clean Windows x86 RelWithDebInfo build; no errors or warnings. The complete
automated suite passes.

A new regression compiles the shared loop model and checks that a submission
without a start time keeps none, that ET's submitted time is carried through
untouched, that a refused submission stores nothing, and that the phase helper
stays inside the sample for a range of clocks and refuses a zero rate, a zero
length and a negative clock. Source checks hold that every sound ordinal on both
boundaries is handled, that the volume, start time and stop-sounds flag actually
reach the mixer, that the one-shot volume word moves with the extended form's
extra argument, that every ET entry point refuses a profile that is not ET, that
both audio backends implement the surface, and that the two profiles keep
separate master fades.

## Remaining gates

- **Nothing here runs.** The ET client stays gated, so no ET module has played a
  sound. No listening test has been performed and no audio claim is made.
- The renderer still has no MDM/MDX model type, so ET players would have no
  bodies even with sound carried.
- ET's remaining open imports are the decals, polygon buffer, dynamic light,
  global fog, view parameters, 2D polygons, dynamic shaders, render to texture,
  camera paths, popups, string translation, binary messages and file deletion
  named in the two boundary records.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

The shared Wolfenstein loop record gained a start time that only ET sets, and
the software mixer uses it only when it is set, so the other profile's loop
phase is bit-for-bit what it was. The mixer's master gain, previously applied
for single-player RTCW alone, now also applies for ET; no other profile's path
changed.

Four RTCW regressions compile slices of the audio backends into their own
fixtures. Each was extended to know that an ET path exists: three declare the ET
fade entries their slice now mentions, and one declares the loop channel's start
sample and the mixer rate. None drives the ET profile and none changed what it
asserts about RTCW.

## Unrelated observations

None new in this slice.
