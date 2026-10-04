# Elite Force's installed-map memory sweep

The [reported-fault repair](2026-10-01-observed-compatibility-fixes.md) made
`borg1` and `hm_borg1` load with Vulkan by replacing retail's 36/47 MiB hunk
budget with a 128 MiB minimum. This wider check covers all 66 installed SP
maps, including the tour, and all 33 Holomatch maps. It found two further SP
failures and sets the final client minimum to 160 MiB for SP, retaining
128 MiB for Holomatch. SP also reserves a 96 MiB initial main zone. The
[companion JSON](2026-10-01-stvef-hunk-map-sweep.json)
pins each build, map result, command, installation and retained evidence.

## Observed failures and budget

At 128 MiB, 97 maps reached their first snapshot, wrote an engine PNG and
exited normally. SP's `borg6` and `stasis2` instead reported
`Hunk_AllocateTempMemory: failed on 16777224`, a 16 MiB temporary allocation
plus its header. Their processes returned to the menu and were deliberately
stopped at the timeout. This is an allocation failure, not a process crash.

An intermediate 192 MiB floor loaded those two maps, but exposed another
limit on `scav1`, `tour/deck10` and `tour/deck15`: the 32-bit Vulkan process
could not allocate a contiguous 66 MiB main-zone block. A read-only address-
space probe of the isolated `scav1` process found a largest free region of
63,565,824 bytes at its tightest sampled point. It still had 677,650,432
free bytes overall. That is fragmentation within its 2 GiB address space,
not evidence of exhausted physical RAM. The executable is not marked large-
address-aware. Raising the hunk further would increase that pressure.

At 160 MiB, `scav1` loads and exits normally in the same address-space probe.
All 99 maps then pass at 800 by 600 with sound disabled. A further 1024 by
768 check with OpenAL initialized exposes two remaining tour-map allocations
when the zone still starts at 64 MiB. SP now reserves a 96 MiB initial zone
before renderer startup. This gives the large allocation room without adding
a late segment after the address space has fragmented.

The first zone-policy probe still reported a 64 MiB zone: profile selection
had run after zone initialization, so the SP predicate was false there.
Startup now freezes the profile before allocating the main zone. Profile
command registration stays after zone and command initialization, because it
uses the main zone. The production-startup regression checks those dependencies
for all sixteen profiles in client and dedicated builds. It makes no claim
that every profile has been launched.

The corrected six-map check reports the 96 MiB SP zone and passes with OpenAL
at 1024 by 768. The final corpus run repeats all 99 maps with those settings;
every map reaches its first snapshot, writes an engine PNG and exits normally.
The same six selected maps also pass with the legacy audio backend and,
separately, with OpenAL and an explicit 256 MiB hunk. These controls cover
`borg6`, `stasis2`, `scav1`, `tour/deck10`, `tour/deck15` and `hm_borg1`.
Larger explicit user settings remain intact, and other client and dedicated-
server floors are unchanged. The compiled production-policy test exercises
all sixteen profiles in both client and dedicated builds, including settings
below, at and above the new boundary. Other hunk settings, resolutions, mods
and renderer combinations still need their own memory coverage.

## Probe and validation

`scripts/probe_stvef_hunk_maps.py` runs the stable Win32 client on private,
unswitched desktops, windowed with Vulkan and isolated home
paths. It requests 36 MiB, so the engine itself supplies each profile's
minimum. Its retained configs queue `meminfo`, `screenshotPNG`, a completion
marker and `quit` after the first snapshot. It checks normal exit, no forced
stop, the snapshot, marker, exact allocated hunk and one engine image.
The final result and complete map list are in the JSON.

The installation inventories and tested executable/renderer hashes must stay
unchanged during each sweep. The probe injects no mouse or keyboard input.
The installation includes `BaseEF/zSTVEFtex.pk3` alongside `pak0` through
`pak3`; its archive and module identities are recorded with the results.
This is evidence for that installed asset set, including its texture archive.
The initial corpora disable sound; the final corpus uses OpenAL at 1024 by
768 with effects and music explicitly muted. The log confirms the requested
backend, and the engine PNGs have that size. The tests establish bounded map startup and
rendering, not campaign traversal, combat, audible output, save/load parity
or complete profile compatibility.

The first baseline validator looked for a megabytes line, while this engine's
`meminfo` reports bytes. Rechecking the unchanged logs and engine-image hashes
corrected that parser result to 97 passes and two failures. A later validator
also recognizes fatal-allocation shutdown lines. Original results, corrected
evaluations and rejected 192 MiB runs are retained separately. No failed run
is relabeled as a successful runtime test.

The final startup-order check also exercises controlled failure exits. An
unknown profile exits with status 1 before the main zone is allocated. An
explicit valid profile with an unsuitable or empty installation exits with
status 1 at the later `default.cfg` lookup. These runs establish orderly
failure handling; explicit selection still overrides installation detection.

The final Win32 MSVC build succeeds with GL, Vulkan and RTX modules and all
development profiles enabled. The complete Meson suite passes 179 checks,
skips two and fails none. Its recursive Python gate runs 2,374 tests in
1,733.527 seconds, with 121 capability/platform skips and no failures. The
package layout also passes with 456 audio-zone, 203 fog and three sound-shader
sidecars. Test logs and every runtime identity are pinned in the companion.

The change keeps fnTech3's renderer features and explicit renderer/backend
choices. It neither reduces rendering features nor adds a retail-exact
presentation path. Alice remains deferred; RTCW's intermittent interactive
gunfire still awaits a listening check. The broader release gates stay open.
