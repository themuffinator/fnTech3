# Reported launch and compatibility faults, 2026-10-01

This slice addresses the reported RTCW SP sound loss, Elite Force SP/MP hunk
failures, ET command encoding during a display change, and VS Code launch
options. Alice is deferred at the user's request. The companion
[JSON record](2026-10-01-observed-compatibility-fixes.json) pins binaries,
commands, installations, source hashes and results. These observations do not
close the remaining campaign, renderer or interoperability gates.

## RTCW SP sound requests

SP's modules may submit handle zero for an absent effect. Its original engine
ignores those requests outside the weapon channel; a null weapon request can
still cut an actor's gunfire when that actor dies. This is a contract
observation from [RTCW SP's sound entry point](https://github.com/id-Software/RTCW-SP/blob/master/src/client/snd_dma.c),
not incorporated implementation text. The GPLv3 reference was used only to
observe this rule; the correction and fixtures are independently written.

Both fnTech3 backends had allocated these silent effects. A production-code
fixture submits 200 null effects in one frame, then an enemy weapon sound.
Before the correction, no channel plays that weapon; afterward it plays.
SP now rejects null non-weapon effects before channel or stream bookkeeping.
Null weapon cutoff and RTCW MP's distinct null-sound behavior remain intact.
`tests/rtcw_one_shot_tests.py` exercises both backends and the shared policy.

The `escape1` runtime probe dismisses the briefing's Ready button, allows the
opening script to finish at accelerated time, then runs combat at normal
time. It uses console teleport commands to approach retail guards, without
synthesizing gameplay input. The final packaged Vulkan run records 46 weapon
events and matching 22 Luger and 24 MP40 firing starts, plus eight MP40 echoes.
All 183 traced starts report `AL_PLAYING` and no OpenAL error; none is rejected
for sample loading, channel capacity or source capacity. The installation is
unchanged and the process exits normally.

This repairs a reproducible cause of missing gunfire. The specific
intermittent loss reported during interactive play is not proven to have
only this cause. The private desktop mutes output, so these probes establish
playback state, not a listening comparison. `s_alSoundStartDebug 1` now logs
requests, rejection reasons, sample, source state, error, gain, distance and
free source count for further diagnosis; its default is zero.

The default OpenAL path retains positional sources, the distance model,
occlusion, EFX environments, tone classes and engine audio zones (397 loaded
on `escape1`). HRTF depends on the runtime: this machine's Creative Generic
Software driver has no HRTF or OpenAL Soft extensions. `s_backend legacy`
remains the explicit retail-mix option. No presentation features were disabled
to repair the request rule.

## Elite Force client memory

With Vulkan and a small retail hunk setting, both `borg1` and `hm_borg1`
reproduce `Hunk_AllocateTempMemory: failed on 33554440`: a 32 MiB renderer
scratch allocation plus the hunk header. The shared engine retains larger
records and renderer data than the retail engine, so its client allocation
cannot use retail's 36/47 MiB budget.

Both client modes now enforce a 128 MiB minimum, including for an existing
config or explicit smaller command-line setting. Larger values are retained.
Other client profiles and dedicated-server minimums are unchanged, verified
by executing the production policy for all sixteen profiles in client and
dedicated builds (`tests/profile_hunk_minimum_tests.py`).

That was this checkpoint's budget. The later
[complete installed-map sweep](2026-10-01-stvef-hunk-map-sweep.md) found two
additional SP failures at 128 MiB and sets SP's floor to 160 MiB. Holomatch
retains 128 MiB. The binary identities and measurements below belong to the
earlier 128 MiB build.

The packaged Vulkan client loads `borg1` with the requested 36 MiB setting
raised to 128 MiB and 70,580,480 bytes retained on the hunk. Holomatch loads
`hm_borg1` with 47 raised to 128 MiB and 49,794,240 bytes retained. Both reach
their first snapshot and exit normally with the installation unchanged.
The SP capture shows its opening story; the map and native modules have loaded.
The shared rendering and audio features remain enabled by default. This is
memory evidence for these maps, not whole-campaign or renderer-parity sign-off.

## ET input and native pure proof

ET's main command buttons occupy a byte. The client had generated Q3's
`BUTTON_ANY` value (2048) when a gameplay key was held, and the frozen ET
adapter correctly rejected that command. The error text mentioned an invalid
frozen profile even though profile selection remained intact. A UI/display
transition can expose the held key to gameplay on the next frame.

ET now uses its own any-key bit (128), seven primary action bits and its
secondary action byte. Activate, sprint, reload, zoom, lean, alternate fire
and prone are registered and encoded for ET. Prone is secondary bit seven.
The existing input timing and ownership system remains in use. Executable
fixtures run the production registration and command builder through held,
tapped and released buttons, unbound keys, UI catchers and all affected
profiles. Commands outside the ET byte contract still fail validation.
The constants are observations from [ET's original input source](https://github.com/id-Software/Enemy-Territory/blob/master/src/client/cl_input.c);
no GPLv3 implementation text was incorporated.

The windowed restart probe uncovered another defect: fnTech3's ET client
omitted its native UI/cgame checksums from the pure response. Native pak
reference flags and cached-module re-verification now include ET. Verification
requires the selected allowed pak's exact length and payload checksum, and
restores its reference flags after a filesystem restart. Tampered, excluded,
duplicate-priority, truncated and mismatched bytes are exercised by the
existing native-module proof fixture for both RTCW and ET.

A Vulkan `railgun` listen-server run with retail's default `sv_pure 1` now
joins and re-enters after `vid_restart`, with clean shutdown. The first
approved fullscreen attempt remained windowed because `in_nograb 1` forbids
fullscreen. It is retained as a rejected test setup, not fullscreen evidence.
The corrected run reaches 2560×1440 fullscreen, then restores the 800×600
window. All three cgame starts reach their first snapshot with pure admission
intact; all three engine captures are written and the process exits normally.
No mouse or keyboard injection is part of that transition test. Broader
gameplay and display coverage remain open.

The separate retail-server probe exposes an additional handshake gap: its
bare challenge reply had entered Q3's compatibility fallback, causing the
client to advertise protocol 68. The retail ET server rejects that version.
ET now uses its own profile protocol (84) through challenge and connect, and
its optional `sv_onlyVisibleClients` token uses the same measured positions as
RTCW MP. The shared formatter also gives that value to ET clients of fnTech3
servers. The client applies the seven visibility traces and 600 ms grace
period from ET's interpolated viewpoint. Missing, malformed and overflowing
integer tokens are rejected by the shared reply reader. These are observations
from [ET's client handshake](https://github.com/id-Software/Enemy-Territory/blob/master/src/client/cl_main.c),
[server handshake](https://github.com/id-Software/Enemy-Territory/blob/master/src/server/sv_client.c)
and [visibility rule](https://github.com/id-Software/Enemy-Territory/blob/master/src/client/cl_parse.c);
the existing independently authored Wolfenstein helpers implement the shared
contracts. `tests/rtcw_handshake_tests.py` covers both titles' reply layouts,
malformed replies and profile-specific viewpoints.

The rebuilt client passes pure admission to the original `etded.exe` on all
six stock maps (`oasis`, `battery`, `goldrush`, `fueldump`, `radar`, `railgun`),
joins Axis and Allies, and receives the enabled `sv_onlyVisibleClients` value.
The engine captures show each team's spawned view. These are short admission
sessions with one client; they do not establish combat, objectives or live
culling of other players. An additional pure UDP session against fnTech3's
dedicated server verifies its extended nonce/protocol reply. RTCW MP's pure
retail-server admission, both teams and visibility option pass again on
`mp_beach` after the shared reader change. Every installation is unchanged
and each client exits normally. Broader ET demo coverage and sessions remain
open.

A retail `et.exe` client also passes pure admission to the rebuilt fnTech3
server on `goldrush`, joins both teams, disconnects and leaves both processes
to exit normally. This checks the reverse direction after the reply formatter
change; the earlier six-map inbound record remains separate evidence.

The separate [demo follow-up](2026-10-01-wolfet-demo-regressions.md) adds
current-build recording, normal/timedemo playback, retail demo exchanges,
both teams' player states and bounded damaged-input runs. It expands the
existing partial demo evidence; full-match coverage remains open.

## Launch options and validation

The existing named menu and starter-map launches remain quick presets.
Non-Alice map pickers now offer windowed resolution, audio backend, console
logging and developer diagnostics. RTCW SP adds difficulty, Q3/Holomatch add
arena game type, and RTCW MP/ET add their own game types. ET's picker offers
pure or unrestricted local verification. All settings precede the map
invocation, and writable homes remain isolated from installed assets.
Alice's launch entries are unchanged.

`meson: verify all games` builds the existing x86 development preset and runs
the complete automated suite explicitly. Launching and normal build tasks
still do not run tests implicitly. Runtime binaries retain the stable
`meson/build/win32-dev` paths. The companion records the completed test result
and retained local evidence under `.tmp/observed-compatibility/`. The complete
Meson run passes 179 targets, skips two and fails none. Its recursive Python
gate runs 2,369 tests in 1,330.776 seconds, with 121 capability/platform skips.
The package contains the verified 456 audio-zone, 203 fog and three
sound-shader sidecars.

The first run ended at the original 900-second discovery timeout and included
failures while this evidence bundle was incomplete. A verbose discovery rerun
then passed, and the final complete Meson run passed. The recursive gate now
allows a bounded 1,800 seconds so the full Windows x86 native-fixture suite can
finish. All three logs are retained; the interrupted run is not counted as
passing evidence.
