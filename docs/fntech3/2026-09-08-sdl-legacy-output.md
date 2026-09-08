# Measured legacy mixer output through SDL

A separate Windows x86 build of `6ebcc76` now provides actual legacy audio-output
evidence under both profiles. Center speaker masks mix equally to left/right,
front-right controls remain right-only, loaded center sounds play correctly, and
seven-channel center music maintains its declared routing. All ten processes
exit zero normally, stop to a silent final second, and leave staged files
unchanged. This closes the absence of measured SDL legacy output for these cases,
not the full audio gate.

Continuity does not pass. The streamed output contains exact-zero intervals inside
the signal span, including recurring 50 ms gaps. The initial probes overlap the
full automated suite; two seven-channel repeats after that suite finished still
show gaps. The quiet QL repeat has five interior 50 ms gaps plus an earlier longer
interval. The cause has not been localized between engine mixing, SDL delivery
and the private-desktop test environment. Q3 startup UI audio can extend beyond
the first second here, so its earliest measured zero interval is not necessarily
a music interruption. Frequency-selected routing windows remain valid evidence,
but are not proof of uninterrupted playback.

The first SDL-enabled Meson engine build completes 1,078 steps. Its pinned SDL
3.4.0 Meson recipe omits the disk driver. The same upstream source is then built
with CMake's supported `SDL_DISKAUDIO=ON` option into an isolated installation;
the engine is relinked against this test-only shared dependency in 83 steps.
Meson wraps retain ownership of normal bundled fallbacks. No SDL implementation
or engine source is changed, copied into tracked code, or substituted into a
public release. SDL's zlib license is compatible with the repository's GPL;
its source and license identities are pinned in the record.

The configured SDL engine suite passes 110 targets with zero failures and one
skip, including 1,303 Python tests with five skips. Native module opt-ins use the
new SDL client/server binaries. The [structured evidence](2026-09-08-sdl-legacy-output.json)
records build commands, dependency selection, source/binary hashes, runtime
commands, output metrics, gaps and log hashes. GLx is the only built renderer in
this configuration; these are separate artifacts from the main native-Windows
release build.

Each runtime explicitly selects the legacy backend, SDL's disk driver and
`s_sdlCapture 0`. Logs verify disk playback as stereo S16LE at 48 kHz. The driver
writes playback data directly to an isolated file; there is no OS recording,
microphone capture or speaker playback. Clients are windowed and input-disabled
on private unswitched desktops, with isolated writable homes and read-only retail
installations. Captured output and raw logs, potentially containing retail startup
audio, remain ignored. Native WASAPI/DirectSound hardware, surround devices,
continuous playback and broader lifecycle/performance gates remain open.
No unrelated issue was discovered.
