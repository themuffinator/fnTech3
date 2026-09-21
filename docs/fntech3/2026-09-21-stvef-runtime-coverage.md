# Elite Force full demos and ray-tracing boundary

All three effective retail Holomatch demos now complete on each explicitly
selected Windows renderer. These runs extend the short backend checks in the
[renderer selection audit](2026-09-21-stvef-renderer-verification.md).

| Demo | GLx | Vulkan | RTX raster | Retail 1.20 |
| --- | ---: | ---: | ---: | ---: |
| `demo0000` | 11,803 | 11,803 | 11,803 | 11,803 |
| `demo0001` | 6,248 | 6,248 | 6,248 | 6,248 |
| `extreme` | 2,268 | 2,268 | 2,268 | 2,268 |

These are the completed timedemo frame counts, totaling 20,319 per executable
and 81,276 across the four runs. The effective recordings come from `pak2.pk3`;
earlier paks also contain older recordings with the first two names. The runs
mount the four retail paks without the HD add-on. Each process chains the three
demos, writes three engine screenshots and exits normally. Backend identity is
checked from the actual renderer log, not the requested label.

The images show world geometry, characters, weapons, effects, HUD and
scoreboards on all three fnTech3 renderers. The `wait` command does not place
the retail screenshots at the same recorded instant, so these images establish
rendered playback coverage, not exact retail image parity. The RTX runs use
`r_hdr 0` and raster mode. These measurements used the production executable
before the separate [SP clock fix](2026-09-21-stvef-sp-clock.md); that change is
guarded to SP, and its post-change Holomatch preflight also passes.

Four further runs request `rtx_rt_mode 2` for SP and Holomatch, each with
`rtx_rt_require` off and on. The NVIDIA GeForce RTX 4060 Laptop GPU's tested
32-bit Vulkan driver reports no acceleration-structure, deferred-host-operation
or ray-tracing-pipeline extensions, and lacks the required pipeline/descriptor
limits. With fallback permitted, each profile explicitly reports disabled RT,
renders through RTX raster and exits normally. With strict requirements,
startup refuses with the missing capabilities listed. The harness terminates
those two expected fatal-error dialogs; they are not clean-exit or successful
ray-tracing runs.

This is an observed limit of this Windows x86 driver path (NVIDIA 591.74,
Vulkan 1.4.325), not a claim that the GPU lacks ray-tracing hardware. The retail
SP module requires Windows x86, so an x64 run cannot close that SP gate.
Native RT remains unavailable in this tested configuration. The fallback and
strict refusal behave honestly.

The [manifest](evidence/2026-09-21-stvef-runtime-coverage.json) records the
executable, renderer and installation identities, commands, settings, complete
frame counts, capability messages, logs and capture hashes. All tests use
isolated writable homes, windowed mode, disabled mouse/joystick input and a
private desktop that is never activated. Only engine-render-target screenshots
are used. No retail binaries, paks or images are committed.

The broader SP scene matrix also found and led to the startup-clock repair;
its before/after results belong to that record. Physical input and listening,
force-feedback support, broader campaign behavior and synchronized retail
image comparisons remain open. Both profiles stay development-gated.

## Other observations

- The installed `video/st_21.bik` has a bad ZIP CRC; its lower-resolution
  variant is valid. This asset defect is retained in the
  [Bink record](2026-09-20-stvef-bink.md); no retail asset was repaired or
  redistributed.
- The broader Linux suite still has unrelated Enemy Territory fixture/probe
  failures and a macOS packaging check that requires POSIX file modes not
  available on this DrvFS mount. See the
  [fixture follow-up](2026-09-21-stvef-linux-fixtures.md).
- With `fs_debug 1`, shutdown can reopen and truncate `qconsole.log`; final
  probes turn it off before quitting. No shared logging change is included.
- Retail SP can overflow its stack with `developer 1` and `logfile 2`
  together, and its default 10 MB zone is insufficient for the installed HD
  add-on. These harness controls use developer output off and a 128 MB zone.
- An earlier Lilium 1.40 x64 dedicated run exited with signal 11; the recorded
  successful interoperability uses the identified x86 reference. That isolated
  external-engine failure is not attributed to this clock change.
- A transient build failure in the separate RTCW checkout reported a missing
  `rtcw_routecache_writer.hpp`; that header is now present there. This isolated
  STVEF worktree did not modify that checkout.

Logs, captures and private test installations remain in the recorded project
and `E:/Games/fnTech3-tests` paths. Automatic approval review blocked recursive
cleanup of `.tmp/stvef-gaps/windows-fixture-tmp` and
`.tmp/stvef-gaps/test-tmp`, reporting only "blocked by policy"; those disposable
folders were left in place.
