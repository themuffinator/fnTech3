# Compatibility checks and gap resolutions, 2026-10-02

This round checks the Windows x86 shared development build, preserving the
sixteen profile gates. Its [JSON companion](2026-10-02-compatibility-round.json)
records the source and executable identities, commands, tests and runtime
results. The bounded checks are complete; no full compatibility gate is closed.

## Enemy Territory world-model entities

ET's multi-entity sweep and point-content queries exclude an entity whose
collision handle is zero. The actual world has already been sampled and
must retain its own collision identity. fnTech3 previously sampled that
entity as a transformed copy of the world; its result could supersede the
real world hit and attach the entity's number or extra contents to it.

This rule is observed in the original
[ET server world source](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_world.c)
and the earlier hash-pinned native observations in the
[entity box contents record](2026-09-23-entity-box-contents.md). The local
source's GPLv3 licence was checked; it is used for reference observations
only. The two guards and fixture are independently authored, with no
implementation text incorporated.

The production sweep and point-query fixture fails before the correction
on ET's duplicate world sweep. It checks all sixteen catalog profiles,
the nearest collision's identity, combined contents, box and inline-model
queries, unchanged module records, invalid model rejection, and retained
explicit single-entity traces. This is Source/Unit evidence. No retail
gameplay scenario has been compared for this rule.

## Navigation and evidence checks

The [cache-limits record](2026-10-02-wolfet-routing-limits.md) resolves ET's
16 MiB default, configured frame-limit miss deferral and two-cache/table-order
eviction. Sixteen production regressions pass, including RTCW controls and
the existing ET scalar/matrix fixtures. Retail cache comparisons remain open.

Release probes previously accepted any status response on a candidate port.
They now require challenged `getinfo` and `getstatus` replies from the queried
endpoint, with the selected protocol and map, plus the child server's observed
bound port. Engine `SVC_Info` supplies protocol; `SVC_Status` need not include
that key. Six parser/socket/port regressions pass. Profile probes now keep
temporary homes under the project's `.tmp`, rejecting links and junctions;
three regressions pass.

The strengthened runtime probe checks Q3 `q3dm1`, QL `campgrounds`, RTCW SP's
`escape1` briefing and RTCW MP's `mp_beach` spectator view with one client
binary. All four clients exit normally and write their engine captures;
all three multiplayer dedicated servers answer the challenged queries on
their observed bound ports and exit normally. Installation inventories and
client/server/GL-module hashes are unchanged. These are bounded startup
checks, without gameplay, audio listening or retail image comparisons.

The optional retail QL factory check initially skips because its default `C:`
Steam path is absent. Rerunning it read-only with `FNQL_RETAIL_QL_PATH` pointing
to the installed `E:` Steam library passes all five cases: the factory member's
size, schema, 22 stock IDs/order/base game types, hidden IDs and loose
`server.cfg` host wiring. The companion records the input member/config hashes;
no retail content is copied. OpenAL loopback remains skipped because the detected
library lacks `ALC_SOFT_loopback`.

Q3 `q3dm1`, QL `campgrounds` and Holomatch `hm_borg1` also initialize installed
navigation and list a bot, with normal exits. Elite Force SP `borg1` and
Holomatch reach their first snapshots at 1024 by 768 with Vulkan and muted,
initialized OpenAL; the requested 36 MiB hunk becomes 160 and 128 MiB
respectively. The SP capture shows the opening story. An earlier ET checkpoint,
before the download framing correction, joins both teams with pure admission
and visibility filtering on retail `railgun`; its engine captures show the
spawned views. The final-build ET download sessions are recorded below.
None of these closes a full gate.

## Enemy Territory downloads

ET reserves block `-1` for URL, size and flags. Retail servers can send a
fallback URL even to a client that does not advertise www support. The shared
fnTech3 parser previously treated the URL bytes as a chunk length. It now
validates the block header and refuses ET's redirect marker with a clear
UDP requirement before touching the download data. Production parser cases
cover six exercised protocols (49, 60, 68, 71, 84 and 91), truncated headers, ET's marker and retained
block 65535 behavior outside ET. The original
[ET download parser](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/client/cl_parse.c)
is reference observation only, with no GPLv3 implementation text incorporated.
The runtime fallback test observes that refusal, a live main loop two seconds
later, and no temporary/final pak. Its client is deliberately stopped by the
harness after recovery; this is not a normal-exit or HTTP-download claim.

The new probe sends an independently authored 32,916-byte ZIP from an isolated
server home to an empty client home. fnTech3-to-fnTech3 and retail-server-to-
fnTech3-client UDP transfers finish with the exact SHA-256, no temporary file,
pure admission, `ClientBegin` and normal exits for both peers. The installation
is unchanged. The pak contains generated numbers, with no retail assets or
modules, and is not committed.

The reverse direction initially exposed a blocker: retail `et.exe` received the
first 2,048 bytes from fnTech3's server, sets `com_errorMessage` to
`VM_Call with NULL vm`, and never finishes the download or enters the game.
A 120-frame delayed connection reproduces the failure. A retail-to-retail
control with the same fixture, mod and settings finishes normally. These
observations rule out a deterministic inability of retail to use the fixture
and the tested startup-delay explanation. The download writer and network
sender each wrote an EOF. Retail stops at the first, then treats the remaining
bytes as a binary cgame message, calling a cgame VM that does not yet exist
during the download. The caller now leaves ET's single terminator to the
network sender at both ordinary and error download sites. Other profiles keep
their existing framing. The production Huffman/message fixture covers ordinary,
subsequent, empty-final and error blocks, multiple lengths, seven compressed
profiles including MOHAA's different opcode, and raw Holomatch. Restoring the
duplicate ET EOF fails the fixture.

On the final server, the original immediate-connect retail scenario completes
all 32,916 bytes with the exact hash, no temporary file, pure `ClientBegin`, no
VM error and normal exits for both peers. This confirms the framing correction
resolves the reproduced blocker. Earlier attempts with an unsuitable readiness marker or too
many startup arguments are retained as probe setup failures, separately from
this reproduced runtime failure.

The final strict Windows x86 build enables all sixteen development profiles
and GL, Vulkan and RTX renderers. Its generated asset package passes the
release-layout check with 456 audio zones, 203 fog sidecars and three sound
shaders. The initial complete Meson attempt passes 178 checks and skips two;
recursive Python discovery reaches its 1,800-second bound without a completion
summary. That attempt establishes no complete suite pass. Its captured stream
contains no unit-test failure markers. The recursive-only rerun completes
2,397 tests in 1,953.577 seconds, with 121 skips and no failures, under a
3,600-second bound.
Across the unchanged-build original run and that rerun, all 181 Meson checks
are accounted for: 179 pass and two skip. The separate retail factory supplement
above exercises one skipped check with its correct installation path. The
recursive gate's default is increased to 3,600 seconds after this measured
32.6-minute completion; individual fixture limits and test selection are retained.

Reconfiguring for that timeout refreshes the executable/module identities. All
180 nonrecursive checks are rerun on those rebuilt binaries with the correct QL
path: 179 pass and only OpenAL loopback skips. Together with the complete
production-source recursive run, all 181 checks are accounted for with 180 passes
and one skip. The shared-client/server, bot-navigation, Elite Force startup and
four ET download/refusal scenarios repeat successfully on the rebuilt binaries.
The companion
selects these `policy` results as current and retains the earlier fixed-build
identities separately. No engine source changed during the timeout correction.

## Remaining gates and presentation

Loaded ET retail navigation and live bot paths remain open: the stock
installation ships no AAS/RCD navigation files. Sustained gameplay,
campaign, visual parity and full bidirectional interoperability gates
remain open. ET HTTP redirects, broader downloads and stale tactical
search markers remain open. Alice remains deferred, as in the preceding round.

No rendering, spatial audio, input or interface presentation path changes
in this round. The existing FnQ3/FnQL feature scope and explicit retail
mix options are retained.

Unrelated observations remain: 155 historical dated records lack adjacent JSON
companions; QL logs the `sv_cylinderScale` default `1.1f` numeric warning and
temporary-hunk nonfinal-block warnings; RTCW MP logs six top-down TGA-header
warnings. Their original evidence is retained rather than reconstructed.
The sign-off paragraph's obsolete five-profile scope was corrected to require
all sixteen registered profiles and every applicable game/mode matrix.

Disposable ET download homes and earlier runtime homes were removed after
absolute-path and reparse-point checks. Logs, engine captures, results and
cleanup manifests remain under `.tmp/compatibility-round-2026-10-02`.
Automatic approval review rejected removal of two sets of seven release-probe
homes, with only `blocked by policy` as its reason. Narrower per-file alternatives
were also rejected for the first set; the second rejection was not retried.
All fourteen homes remain at their original project-local paths.
The final removal of six disposable helper scripts was also rejected with the
same reason. Those scripts remain beside the evidence; no further deletion
attempt was made.
