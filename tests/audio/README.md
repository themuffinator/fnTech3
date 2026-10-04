# Audio Tests

`fnql_audio_loopback_tests` is a deterministic OpenAL Soft loopback harness for the modern audio backend work. It uses `ALC_SOFT_loopback` to render into an in-memory stereo float buffer, so it does not need a real playback device or game assets.

`fnql_audio_zone_tests` exercises the engine-independent `.azb` runtime parser
and selection helper with synthetic sidecars. It covers version 1 compatibility
defaults, version 2 material/flag/portal metadata, version 3 per-portal blend
tuning, priority and smaller-volume selection, portal blend bounds, and invalid
sidecar rejection.

`fnql_audio_zone_authoring` and `fnql_audio_zone_authoring_audit` compile and
audit a metadata-rich `.audiozones` fixture so hand-authored material, flag, and
portal tuning syntax stays covered.

`audio_zone_sweep_tests.py` covers the deterministic parts of the bulk migration
script: map discovery, map-relative output planning, override matching, audit
summary parsing including confidence/anomaly fields, dry-run manifests, and
CSV/JSON report writing.

`audio_zone_material_map_tests.py` builds a tiny synthetic IBSP map and verifies
that `--material-map` can override generated material, preset, and flag metadata
before the sidecar is dumped back through the compiler.

`fnql_audio_tone_tests` exercises the per-voice tone policy in
`code/client/audio/shared/AudioTonePolicy.h` without an OpenAL device. It covers
source classification, the diegetic boundary that keeps announcer, UI, and
feedback audio out of the environment, underwater shaping of the listener's own
sounds, and the unchanged world, authored-stereo, and strong-occlusion paths.

`fnql_audio_recovery_tests` exercises the OpenAL device recovery policy without needing real hardware disconnects. It covers poll timing, retry suppression, one-shot disconnect/reconnect messages, disabled auto-recovery behavior, successful recovery reset, refresh-query failure behavior, and manual force/skip decisions.

The harness currently checks:

- OpenAL library and loopback availability.
- HRTF status reporting and mode switching when `ALC_SOFT_HRTF` is present.
- Inverse-clamped distance attenuation.
- Stereo, quad, 5.1, 6.1, and 7.1 direct-channel routing when the runtime supports those layouts.
- Optional UHJ and B-Format buffer acceptance when the runtime exposes those extensions.
- Idle loopback silence.
- The production `StreamPlayer` retaining PCM through deferred starts and
  underruns, producing nonzero loopback output, and recycling buffers over
  200 restart cycles.
- Runtime-selected source sinc conversion, with wanted-tone preservation and
  at least 60 dB image/alias rejection when the optional API is available.
- EFX low-pass, high-pass, and band-pass filter behavior when `ALC_EXT_EFX` is present.

Build and run it with Meson:

```sh
meson compile -C meson/build fnql-audiozonesc fnql_audio_zone_tests fnql_audio_recovery_tests fnql_audio_loopback_tests
meson test -C meson/build -R "fnql_audio_(zones|zone_authoring|zone_authoring_audit|zone_sweep_script|zone_material_map|recovery|loopback)" --print-errorlogs
python tests/audio/audio_zone_sweep_tests.py
python tests/audio/audio_zone_material_map_tests.py meson/build/fnql-audiozonesc
```

The loopback test exits with code `77` when OpenAL or `ALC_SOFT_loopback` is unavailable. CTest treats that as a skip. The zone runtime, tone policy, and recovery policy tests do not require OpenAL.

The recursive Python suite also executes `audio_stream_lifecycle_tests.py`,
`stream_resampler_tests.py`, `mohaa_audio_stream_tests.py` and `alsa_transfer_tests.py`. They compile production
code with fault fixtures for queue exhaustion, decoder endings, byte order,
numeric saturation, partial writes and bounded conversion allocations. The
resampler checks chunk-independent output, exact counts and endpoints, and gain
changes against the same envelope in a continuous buffer. It also requires at
least 60 dB of rejection just above the output Nyquist limit during downsampling
and for upsampling images, with less than 0.1% amplitude error at 90% of the lower
Nyquist limit and at 20 kHz for 44.1 → 48 kHz. Ritual fixtures also check non-native
score rates, moved/parked converter state, gain ramps and recovery from NaN gain.
MOHAA fixtures bound empty/skipped loops, preserve playable loops and EOF tails,
drain failed/incompatible reopen tails, and check playback positions and PCM safety.
`audio_source_resampler_tests.py` checks runtime-defined index ordering, quality
tiers, malformed inventories, optional API failures, cleanup, and source reuse.

`resampler_allocation_benchmark.cpp` and `resampler_downsample_benchmark.cpp`
are standalone performance witnesses, outside the timing-sensitive test gate.
Compile them with C++17, optimization enabled and
`-I code/client/audio/shared`; write executables under a project-local temporary
directory. The allocation witness can use a saved baseline header through a
different include directory. It counts 1,000 transactional upsampling chunks
including EOF flush. The rate-conversion witness counts 100 chunks including flush
and checks the exact final sample count; elapsed time is reported without a
machine-specific pass/fail threshold.
