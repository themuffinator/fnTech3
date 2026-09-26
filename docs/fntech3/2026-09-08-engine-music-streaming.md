# Engine streaming music and error recovery

The same Windows x86 client passes measured streaming-music checks under Q3
and QL. Six launches cover stereo PCM16, PCM24 and IEEE float32 extensible WAVs
in both profiles. Two further launches reject a malformed extensible GUID and
then play a valid stream in the same process. All eight exit normally with code
zero, and runtime files remain unchanged. The [record](2026-09-08-engine-music-streaming.json)
pins inputs, commands, binaries, output/log hashes and quantitative results.

The existing `music` command streams a one-second intro with 997 Hz left and
1499 Hz right signals, followed by a half-second loop with 2111 Hz left and
2801 Hz right signals. Input amplitude is 0.25 at 48 kHz. The script later issues
`stopmusic` and leaves a settling interval before exit. OpenAL Soft's Wave File
Writer records output; only that playback backend is selected, and SDL's dummy
driver guards fallback. The pinned runtime and safety setup are the same as the
[initial engine-output checkpoint](2026-09-08-engine-audio-output.md).

Every run contains the expected intro frequencies before the expected loop
frequencies, with correct channel assignment. Each has nine identified 100 ms
intro windows. The ordinary runs have 49–61 identified loop windows, far beyond
the half-second source duration, demonstrating sustained looping. The recovery
runs have 48 loop windows. Interior 10 ms RMS measurements remain above 0.1765;
none of those measured windows is silent. Each final second after stopping is
exactly silent. Frame-based console waits take different wall-clock durations
across launches; the result does not assume a fixed playback duration from a
`wait` count or claim sample-perfect handoff timing.

The malformed fixture changes a GUID byte while preserving the low PCM tag.
Both clients report that `music/bad.wav` could not be opened between the bad-track
markers, then successfully play the valid intro/loop after the next marker.
The signal analysis finds the uninterrupted valid sequence and eventual silence.
This exercises the recent GUID rejection through the live streaming entry point
and establishes subsequent valid playback without restarting the client.

All rendering is windowed on unswitched private desktops, input capture is
disabled, and writable homes are isolated. Installations remain read-only.
No speaker output, microphone input, OS recording or OS screenshot is used.
Raw outputs/logs remain ignored because startup may include retail UI audio;
no game assets, proprietary output or external implementation are committed.
The synthetic signal formulas and hashes permit fixture reconstruction.

This extends earlier whole-load/local-play evidence through streaming,
intro/loop reopening, conversion, stop and rejected-open recovery. It does not
establish hardware behavior, arbitrary sample-rate conversion, multichannel
layouts, game-module sound triggers, positional sound, device loss or prolonged
music stress. The audio gate remains partial. No engine code changes or full-suite
rerun are claimed for this runtime checkpoint; documentation packaging and link
checks pass. No new unrelated defect was established; existing optional-service,
image and UI limitations remain unresolved.
