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

## The engine's own features (2026-09-24)

Under invariant 11 of `AGENTS.md`, the OpenAL backend no longer pans ET's
voices as retail's mixer does. It renders ET's data through the engine's own
audio, as it renders every profile's sounds. The [Wolfenstein OpenAL voices
record](2026-09-24-wolfenstein-openal-voices.md) has the design, tests and
runtime evidence.

Kept from retail, as above:

- the one-shot's submitted volume and the shared allocation;
- the loop's start time and the virtualization by retail's gains;
- ET's own master fade, as each loop's submitted volume captures it;
- retail's span for how far a sound carries: full volume to 6.4% of its
  range, silent the range beyond, 1,250 for a one-shot and the submitted
  range for a loop;
- the loops' retail level, 90 over 256 of their volume.

Used:

- **Positional rendering, with HRTF and the output mode**, for every voice
  away from the listener.
- **The OpenAL distance model** across that span, faded out toward its end,
  with air absorption.
- **Traced occlusion and liquid boundaries**, on the direct and wet paths.
- **The room reverb** from the six stock maps' audio zone sidecars or the
  traced room.
- **Tone classes** by channel. ET's channels 0 to 7 are Quake III's numbers.
- **`s_pvs`**, when it is asked for.

At the listener:

- A one-shot takes the room but no occlusion.
- A local channel there is a dry interface sound.

Left out:

- **Retail's left/right balance.** It stays in the legacy mixer
  (`s_backend legacy`), the explicit retail-exact option.
- **Loop Doppler.** ET's retail mixer pitches a moving loop, but fnTech3's ET
  loop path has never carried a loop's motion. It stays disabled and is open.
- **The weapon sound shaders.** They have no ET rules.

Found while reading ET's mixer, and open in both backends; none of it is new
with this change:

- ET merges same-sample, same-phase loops into one channel;
- ET accepts a loop volume up to 65,535 before its 255 channel cap;
- ET honours the one-shot flag `SND_NO_ATTENUATION`.

On `oasis`:

- the 512-zone sidecar sets a hall;
- the loops play positionally at 0.70 with sends;
- two map one-shots were placed as `item` sounds and were silent past the
  span.

## Remaining gates

- **No listening test.** The ET client has since loaded every stock map in a
  developer build, and its voices reach the OpenAL backend (above). No
  listening test has been performed.
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
