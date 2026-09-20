# Elite Force movie audio output

The client now distinguishes successful Bink sound-factory initialization
from an available movie sound buffer. When DirectSound refuses the buffer,
the movie still plays, but the engine reports audio as unavailable. Previously
it incorrectly reported sound as enabled.

## Native failure and state query

An independent local probe invokes the installed Bink DirectSound factory with
a recording COM device. Returning `DSERR_ALLOCATED` from buffer creation still
allows Bink's movie open to succeed and all 75 video frames to decode. Before
this change `Movie::Sound()` returned true, despite zero PCM output and no
play calls.

The installed runtime's `BinkSetSoundOnOff` export at `0x1000b260` accepts
`-1` to query the current sound state. Its DirectSound callback returns the
state without switching it. The native query returns zero with the refused
buffer and one with an available buffer. The client now uses that query,
while preserving the requested track so a later rewind can retry it.

Regression tests cover the missing export, partial sound initialization,
continued video, recovery on rewind and queries after cleanup. The existing
movie lifecycle, frame, volume, fade, crop, shutdown and RoQ checks also pass:
17 focused tests on Windows and 17 on Ubuntu/Clang.
The complete Windows x86 suite passes 120 targets with two skips; its Python
target passes 1,843 tests with 72 conditional skips.

## PCM and real-device observations

The optional `tests/stvef_bink_audio_native_probe.cpp` accepts a user-supplied
runtime and movie. It observes the installed DirectSound backend's exact PCM
write lengths. Its default COM sink is silent; `device` mode forwards to the
real device with a quiet -30 dB ceiling and reads only its own process's audio
session meter. No runtime, SDK header, movie or decoded audio is distributed.

For `st_01` and `st_04`, memory input, native file input and pause/resume
produce identical PCM per selected track. English and German `st_04` select
different tracks and produce different, repeatable hashes. German `st_01`
falls back to its only track. All observed output is stereo 16-bit PCM at
44,100 Hz: 882,000 bytes for `st_01` and 3,528,000 for either `st_04` track.

The actual sound backend issues the expected volume commands: native full
volume maps to 0 dB, mute to -100 dB and half volume to -6.09 dB. Device runs
retain the same PCM, advance the real playback cursor, produce a nonzero
signal in the probe's own session meter and report no DirectSound API errors.

Windowed engine runs exercise both `st_04` languages in GLx, Vulkan and RTX,
including the one-second fades and natural completion. A test-owned observer
DLL in an isolated installation forwards to the unchanged installed runtime.
Each run reaches frame 300, sends the selected track to DirectSound,
mutes during transitions and completes its continuation. The observer records
only that engine's movie buffers; it does not capture other applications.

Four engine recordings match the native-file PCM byte-for-byte. The German
Vulkan and RTX recordings each contain an additional 252 zero bytes at byte
196,608: 63 stereo frames, or 1.43 ms. Removing only that observed silent
insertion makes every other sample identical. Two control runs through the
installed retail engine also insert silence: each adds 252 bytes and another
512 bytes, totaling 4.33 ms, with all other samples identical. This establishes
that such insertions occur in the native playback path; synchronization is
the inferred cause. The manifest retains the raw hashes, lengths and every
insertion offset. Exact real-time sample delivery is not claimed.

Three further windowed runs refuse the movie buffer in each renderer. All
75 video frames still complete, the continuation runs and the client exits
cleanly. Each log reports `sound off` and `Bink audio device unavailable`;
the observer receives no movie PCM and records no buffer play calls.

## Scope and retained evidence

The [evidence manifest](evidence/2026-09-20-stvef-movie-audio.json) pins the
runtime, client, renderer, probe and fixture identities and records commands,
PCM hashes, native failure observations and output-meter results. Local source,
logs and captures remain in `.tmp/stvef-gaps` in the isolated worktree; test
homes are under `E:\Games\fnTech3-tests`.

These checks establish track selection and delivery through the observed
Windows device. The PCM comparison precedes device mixing and conversion;
the session meter establishes an output signal, not subjective listening
parity or coverage of other devices. The SP profile remains development-gated.
