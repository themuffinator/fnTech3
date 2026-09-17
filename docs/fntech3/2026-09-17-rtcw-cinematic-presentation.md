# RTCW startup movies and SP letterboxing

This checkpoint adds profile-owned startup movies and removes the SP letterbox
import guard. RTCW remains an incomplete development profile. Movie presentation
does not establish campaign, multiplayer, codec, audio, or renderer parity.

## Contract and implementation

The immutable game catalog now owns the startup logo/intro commands and whether
an intro repeats. Q3 and QL retain the engine's existing `idlogo.RoQ` → `intro.RoQ`
sequence and mark `com_introplayed`. This preserves the QL baseline; it is not a
new retail QL startup-parity claim. SP requests `wolfintro.RoQ 3`, omits a separate
logo, and leaves the intro preference unchanged. MP requests `gmlogo.RoQ` followed
by `wolfintro.RoQ`, marking the intro played. Explicit startup commands still
bypass the default sequence, and the existing skip-logo preference can bypass
the catalog's logo. An unavailable cinematic advances `nextmap` instead of
abandoning a queued intro or map.

SP alone interprets flag `0x20`. The console's mode `3` selects a 640×270 movie
rectangle at (0, 105) in 640×480 coordinates. Module-supplied rectangles remain
module owned. Bars use 105/480 of the screen height, the scaled movie width, and
screen-relative top/bottom anchors; the lower bar retains the reference's one
pixel overlap. Color is reset before drawing the movie through the shared
renderer interface. RTCW system/UI movies use independent axis scaling, like
the existing RTCW module imports. Q3/QL retain their cinematic aspect preference.
Flag state is initialized on each new movie and preserved when reusing a live
non-system handle for the same name.

The first installed startup probe exposed two inherited timing problems. A
movie launched before the first client frame starts on system time, then sees
the smaller client-clock epoch. The old unequal-frame loop consumed it to EOF.
The decoder also calculated every movie at 30 fps, despite the installed SP
intro's 24 fps header. The corrected path rebases backward clock changes while
retaining elapsed movie time, handles ordinary 32-bit wrap, uses bounded 64-bit
frame arithmetic, and waits when a decoded frame is already ahead of its target.
SP uses the header rate. Original MP and the existing Q3/QL 30 fps behavior remain
distinct. A decoder reset refreshes the clock before calculating the new target.
Shader-movie suspension after gaps over 100 ms remains supported.

These facts were observed in the original
[SP cinematic source](https://github.com/id-Software/RTCW-SP/blob/master/src/client/cl_cin.c),
[SP startup source](https://github.com/id-Software/RTCW-SP/blob/master/src/qcommon/common.c),
[MP cinematic source](https://github.com/id-Software/RTCW-MP/blob/master/src/client/cl_cin.c),
[MP startup source](https://github.com/id-Software/RTCW-MP/blob/master/src/qcommon/common.c),
and [iortcw SP](https://github.com/iortcw/iortcw/blob/master/SP/code/client/cl_cin.c).
Their GPLv3 notices were checked. Production changes are independently authored
under GPL-2.0-or-later; no reference implementation text or proprietary movie
bytes were incorporated.

## Evidence

Local evidence is under `.tmp/rtcw-manager/cinematic-presentation/`. The executable
presentation tests run actual playback, draw, extent, timing and command bodies,
plus the actual catalog/startup scheduler, with controlled decoder/render/file
boundaries. They cover both RTCW module ABIs in native/QVM memory, all profiles,
flag combinations, handle reuse, invalid handles, missing/invalid headers,
resampling, aspect controls, fractional resolutions, startup preferences,
clock handoff/rewind/wrap, shader gaps, rate selection, looping and shutdown.

The six focused tests pass on Windows x86 and x64 and on Linux x86 with ASan/UBSan.
The final build, full-suite and runtime identities are retained with the reports
in that evidence directory. These are local builds from the dirty worktree based
on `1c2b716a5bd21f1c17ea9368c98717af220e7f7f`, not clean release builds.

The first `startup/rtcw-sp-glx` run produced menu captures because the movie had
already ended. Its generic capture checks passed, but it is **failed movie
evidence**. `startup-v2` establishes a visible letterboxed logo and active
cinematic state at 3/8/12 seconds; the later two captures are dark transitions.
`startup-v3` extends observation to 48 seconds and records decoded frame counts.
All three raster renderers display the logo and later film scenes. Logged target
frames equal decoded frames and match the SP 24 fps clock calculation. These
probes add only timed engine screenshot/quit commands and clock observations;
production files are restored and the ordinary client rebuilt before testing.
No input is sent, and RTX raster success is not ray-tracing evidence.

Installed MP has neither startup movie in its mounted search paths. The final
MP startup probe records both missing-file attempts, then reaches its menu and
exits normally. That is missing-asset fallback evidence, not MP startup movie
playback. The SP movie exists in `sp_pak1.pk3`, which MP intentionally excludes.
The profile's archive isolation is preserved.

All runtime captures use the engine render target in a windowed, isolated home,
at 1600×1200 PNG with `r_picmip=0`, `r_picmip2=0`, and CSM disabled. Installation
inventories are checked and writable homes removed. Sound is disabled in this
round; audible output and movie A/V synchronization remain unverified.

## Remaining limits

Full cinematic compatibility remains open: original-engine visual/timing
comparisons, complete malformed RoQ coverage, seek/restart/device lifecycle,
movie audio and game-script fades, and all campaign transitions. The shared
`ROQ_QUAD_JPEG` branch still does no decoding; the installed SP intro contains
one such chunk. Successful VQ playback and normal completion cannot prove that
chunk's presentation. No JPEG behavior is changed here.

The later [RoQ bounds checkpoint](2026-09-17-roq-bounds.md) establishes that the
original SP and MP engines also ignore JPEG chunks. It preserves that policy
and adds malformed-stream, installed-movie and same-process recovery evidence;
the historical note above is not an established missing original-engine feature.

The previously recorded unrelated dormant legacy wavelet mixer defect remains:
its first compressed chunk selects ADPCM decoding while the loader is disabled.
This checkpoint does not change that path.
