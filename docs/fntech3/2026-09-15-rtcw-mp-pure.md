# RTCW MP pure-admission checkpoint — 2026-09-15

The shared Windows x86 development executable now admits its installed RTCW MP
client to the local pure server and receives the first map snapshot. Rendering
then stops at cgame import 56 (`CG_R_SETFOG`). SP still stops at game import 206
(`BOTLIB_LOAD_MAP`). Neither mode is playable or release-ready.

## Contract and implementation

The immutable game profile now owns its pure-proof policy. Q3 retains its two
QVM pak slots; QL retains its single native x86 slot. RTCW MP selects both native
cgame and UI module names through the game manager, including platform naming.
SP pure verification is explicitly unavailable pending a coherent verified
contract; the original SP generator and verifier do not establish one together.
Original SP defaults to `sv_pure 0`. The later
[SP audio/default checkpoint](2026-09-16-rtcw-sp-loop-audio.md) found that
fnTech3 still inherited one and corrected its profile registration and probe;
the upstream default was not yet implemented at this checkpoint.

MP's client proof contains the server ID, cgame pak checksum, UI pak checksum,
`@`, referenced checksums and the feed/count checksum. Both module slots may
name the same pak. The server records the server ID associated with the feed
so a fast restart does not incorrectly invalidate the earlier proof. Duplicate,
foreign, missing and malformed fields remain rejected. Q3 and QL retain their
existing admission rules.

Loading a pinned MP client module revalidates its bytes against the eligible
module pak before creating a cache file or loading a library. Validation checks
the complete entry length, reads every byte and checks the archive payload CRC.
A rejected entry restores its prior reference flags. Successful validation
reestablishes the cgame/UI references after filesystem restart. Excluded and
non-pure archives cannot provide the proof, and a mismatched higher-priority
entry does not fall through to a different lower-priority entry.

The runtime probe now recognizes the engine's fatal-shutdown diagnostic
promptly, avoiding an unnecessary timeout while the private desktop contains
an inaccessible error dialog.

## Evidence

`scripts/probe_rtcw_pure.py` compares the production proof generator and server
verifier with original MP and iortcw MP functions. Each reference covers 80
generated proofs and 1,844 verification cases on Windows x64, Windows x86 and
Linux x86 with ASan/UBSan. Decisions agree in 1,840 cases. Four deliberate
differences reject numeric suffixes or an extended `@` delimiter accepted by
the references' permissive parsing. The iortcw fixture selects one Windows x86
module-path pattern; it does not establish iortcw's broader architecture policy
or peer interoperability. Reports, source hashes and compiler commands are in
`.tmp/rtcw-manager/pure-reference-{x64,x86,sanitizers}.json`.

Focused profile/proof/module tests pass on Windows x64 and x86 and sanitized
Linux x86. They exercise archive selection, collisions, short reads, bad CRCs,
byte mismatches, unsigned lengths, reference rollback, cache/load ordering,
restart reuse and Q3/QL isolation. Logs are `pure-focused-{x64,x86}.log` and
`pure-sanitizers.log` under `.tmp/rtcw-manager/`.

The default Windows x86 release build passes all 110 compiled Meson checks
(two separate checks are skipped). Its first Python aggregate run found a
source-test extraction boundary that selected an earlier `VM_GAME` block;
the test now scopes its search to `VM_CallNativeExports`. The four focused
QL native-game source checks pass after that correction. The aggregate rerun
passes all 1,365 Python tests in 377.857 seconds with 11 skips, completing
111 passing Meson checks overall. Results are recorded in
`pure-full-python-retry.log` and `pure-full-python-retry-testlog.txt`; the
original run is retained in `pure-full-tests.log`.

The default client SHA-256 is
`e58b1714b8714d972f4a845ab333598deae8cfa4f7154a59fa61a9b3c1f0d378`.
All six Q3/QL GLX, Vulkan and RTX raster probes pass and their engine-written
captures were inspected. Q3 shows the world, weapon and HUD; QL shows the native
join UI over the world without entering the match. Renderer/module identities,
commands, captures and the audit are in `pure-live/manifest.json`.

The development client SHA-256 is
`53fac38ba295f0dd2e9df93bed6a6f506c22d7fb039da2a1b87d3b6ddc7972b9`.
`pure-final-map/manifest.json` records both installed mode probes using this
same executable. MP reaches `CS_ACTIVE` and `CL_FirstSnapshot` on `mp_beach`
before the unsupported fog import. SP reaches the AAS map-load blocker on
`escape1`. No RTCW map screenshot is produced. These are bounded local client
and server observations with retail modules, not bidirectional interoperability.

All runtime probes used windowed, unswitched private desktops, disabled input
and sound, read-only installations and isolated writable homes. Homes were
removed and installation metadata remained unchanged. The recorded commit is
`c75573c7375e44c908256020ada2d7e12970cacf` with an uncommitted worktree.

Original and iortcw functions are GPL-3.0-or-later and run only in disposable
comparison fixtures with their notices. Production changes are independently
authored or extend the inherited GPL-2.0-or-later implementation. No retail
modules or assets are incorporated.
