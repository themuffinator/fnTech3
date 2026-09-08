# Preserve unmatched speakers on older OpenAL implementations

The clean Linux media package exposed silent six-channel center-only Ogg output
with OpenAL Soft 1.19.1: both loaded sounds and music lost the center channel on
stereo output. The three-channel stereo-downmix control remained audible. A
standalone six-channel OpenAL control also played the center correctly, locating
the fault in the engine's source routing policy.

The [direct-channel specification](https://openal-soft.org/openal-extensions/SOFT_direct_channels.txt)
defines `AL_TRUE` as dropping channels absent from the output layout. The engine
previously selected that value when the newer remix extension was unavailable.
It now retains normal mixing in that case, and selects
[`AL_REMIX_UNMATCHED_SOFT`](https://openal-soft.org/openal-extensions/SOFT_direct_channels_remix.txt)
only when supported. Existing guards and modern remix behavior remain intact.
No external implementation text was incorporated.

An executable regression extracts the production method and checks every
combination of readiness, source validity, extension capabilities and routing
request. It fails before this correction and passes afterward.

| Runtime | Fixture | Loaded left/right RMS | Music left/right RMS |
| --- | --- | --- | --- |
| Linux Q3, OpenAL 1.19.1, baseline | Six-channel center | 0 / 0 | 0 / 0 |
| Linux Q3, OpenAL 1.19.1, corrected | Six-channel center | 0.1062 / 0.1062 | 0.1062 / 0.1062 |
| Linux Q3, OpenAL 1.19.1, corrected | Three-channel center | 0.1260 / 0.1260 | 0.1261 / 0.1261 |
| Windows Q3 and QL, OpenAL 1.25.2, corrected | Six-channel center | 0.1261 / 0.1261 | 0.1261 / 0.1261 |

All corrected windows peak at 997 Hz in both channels. All six engine runs
(two Linux baseline, two Linux corrected, two Windows corrected) exit zero;
the final output second is silent. The independently encoded synthetic fixtures
contain no retail content. Older-library mixing has different gain and may use
virtualization; this evidence does not establish gain parity.

Both Windows x86 build variants pass. The complete native Windows suite passes
110 targets with zero failures and one skip (1,307 Python tests, five skips).
The complete Linux x86 suite passes 108 targets with zero failures and one skip
(1,300 Python tests, 21 skips). All 300 Linux candidate runtime files remain
unchanged after probing. The Linux candidate replaces the prior package's six
ELFs and two sidecars; it is not claimed as a new clean packaged release.

Probes use isolated homes, read-only retail inputs, private unswitched Windows
desktops or private Xvfb, windowed input-disabled clients, and file-only audio.
They use the diagnostic front-buffer control. No OS recording or input control
occurs. Linux QL client modules are unavailable, so Linux runtime evidence covers
Q3 only. Hardware surround, device lifecycle and normal presentation continuity
remain open. No new unrelated issue was found; full compatibility remains unproven.

The [structured evidence](2026-09-08-openal-unmatched-speakers.json) records source,
library, fixture, command, binary, output and validation identities. Raw outputs
and retail inputs remain outside tracked distribution files.
