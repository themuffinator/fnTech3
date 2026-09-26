# Muted audio backend restart — 2026-09-08

At `4fc31c5`, the same Windows MSVC x86 client completed local pure sessions
under Q3 and QL with each supported audio backend requested: `legacy` and
`openal`. Each initialized successfully, loaded game sound resources, executed
one `snd_restart`, initialized again, continued snapshots/rendering and exited
normally. Both server processes also exited normally. No engine change was
needed for these probes.

The legacy backend reported SDL DirectSound playback and a signed 16-bit
mixer stream. Logs show the playback device closing and reopening across the
restart. OpenAL reported `OpenAL32.dll`, Generic Software Compatibility Mode,
an active 44,100 Hz context and 40 Hz refresh. The requested 48,000 Hz/100 Hz
values were not reported as active. The runtime warned that OpenAL Soft
extensions were absent; HRTF, device clock and parts of the spatial filter
capability set are therefore not covered. The runtime DLL's resolved path/hash
was not captured; this identifies its reported backend, not an independently
pinned OpenAL installation.

Game, music and voice volumes were zero, and focus-based muting remained
enabled on the unswitched private desktop. These are device/resource/lifecycle
observations, not audible or PCM-output validation. No input was captured or
injected. Windowed GLx engine screenshots after restart were visually inspected
and showed Q3 world/HUD or QL world/native join UI. WebUI and Steam remained
disabled; homes were isolated and installations unchanged.

The [evidence record](2026-09-08-audio-restart.json) contains exact
engine hashes, commands/configs and audio diagnostics. Proprietary resources
and screenshots were not committed. Existing resource/font warnings remain;
the OpenAL capability limitation is now recorded explicitly. Audible quality,
resampling/parity, OpenAL Soft features, device-loss recovery, prolonged stress
and the full audio gate remain open.

## Pinned x86 OpenAL Soft follow-up

At `fa5efd2`, the cached [OpenAL Soft 1.25.2 source](https://github.com/kcat/openal-soft/tree/1.25.2)
was built with MSVC x86 in an isolated CMake directory. Its archive SHA-256
matches the repository wrap, and every upstream source file matched the
archive. The source's LGPL terms are compatible with the GPL engine; no
implementation source was copied into project-owned files. The generated
DLL was used only in a disposable test directory and was not added to the
release stage or system installation. Existing local reference DLLs were x64
and could not validate the shared x86 contract.

The previously skipped OpenAL loopback executable passed all 41 assertions
with that runtime. Its synthetic PCM tests cover silence, HRTF status/mode
switching, distance attenuation, stereo/multichannel layout and isolation,
encoded sound-field acceptance, filters, reverb/send calibration and air
absorption. This tests the runtime with synthetic signals, not complete game
audio through the engine mixer.

Both profile clients also completed their muted local pure sessions and one
`snd_restart` using the exact x86 DLL. Logs report HRTF enabled, stereo-HRTF
output, limiter enabled and an active 48,000 Hz/100 Hz context before and after
restart. Snapshots continued, engine screenshots showed world/HUD or native
join UI, and client/server exits were normal. The same window/input/home
restrictions above apply. The [OpenAL Soft record](2026-09-08-openal-soft.json)
pins the build command, source/runtime hashes, complete loopback results and
profile runtime observations.

This adds the missing loopback evidence without changing the default system
runtime or retroactively turning earlier suite skips into passes. Audible
gameplay parity, device-loss recovery, prolonged stress and release-runtime
distribution remain open. No new unrelated issue was found.

## Unavailable requested device

At `550e104`, both profiles started with the pinned x86 OpenAL Soft runtime
and an intentionally nonexistent `s_alDevice`. The backend reported the
requested-device failure (`ALC_INVALID_VALUE`) and explicitly identified its
working system-default fallback. The startup script then cleared `s_alDevice`
and executed `snd_restart`. Afterward, `s_info` reported the default request
and a connected active device without a stale fallback label. Snapshots
continued and both clients and servers exited normally.

The [device-fallback record](2026-09-08-audio-device-fallback.json)
contains exact arguments/configs, binary/runtime hashes and before/after
diagnostics. Output remained muted, homes isolated and input capture disabled;
no system device settings were changed. This validates unavailable-name
fallback and setting recovery when a default device exists. Physical device
loss and failure of all devices remain separate open cases. The following
probe addresses live reopening without `snd_restart`. No new unrelated issue
was found.

## Connected-device guard and live reopen

At `067646d`, both profiles repeated the unavailable-name startup with the
same pinned x86 OpenAL Soft runtime. While each pure session was active,
`s_alRecoverDevice` correctly skipped the still-connected default device.
`s_alRecoverDevice force` then reported a successful default-device reopen.
The nonexistent requested name remained configured, so the subsequent
`s_info` correctly retained the fallback label and reported HRTF enabled.

Both sessions had one sound initialization and one server-observed transition
to an active client. No initialization was logged between the recovery
markers; snapshots continued after reopening and after the ending marker.
Engine screenshots showed Q3 world/HUD and QL native join UI/world. Both
clients and servers exited normally. The [live-reopen record](2026-09-08-audio-live-reopen.json)
pins commands/configs, executable/runtime hashes and log observations.

Output remained muted, input capture disabled and writable homes isolated on
an unswitched private desktop. No system device settings were changed. This
establishes the connected guard and explicit reopening of a working fallback
device, not physical disconnection, automatic recovery or audible continuity.
No new unrelated issue was found; existing image/font warnings remain open.
