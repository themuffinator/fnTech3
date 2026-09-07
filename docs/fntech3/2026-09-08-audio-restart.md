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

The [evidence record](evidence/2026-09-08-audio-restart.json) contains exact
engine hashes, commands/configs and audio diagnostics. Proprietary resources
and screenshots were not committed. Existing resource/font warnings remain;
the OpenAL capability limitation is now recorded explicitly. Audible quality,
resampling/parity, OpenAL Soft features, device-loss recovery, prolonged stress
and the full audio gate remain open.
