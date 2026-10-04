# ET's AAS8 disk boundary

The [area-routing investigation](2026-10-01-wolfet-area-routing.md) found that
ET loaded area settings through Quake III's 28-byte records. ET stores eight
words, including ground steepness, in 32 bytes. This follow-up selects the
existing field-decoded AAS8 reader for ET, keeps that float in the engine's
per-world sidecar and refuses the incompatible Quake AAS writer. The
[companion JSON](2026-10-01-wolfet-aas8-loading.json) records the source facts,
generated comparisons, build identity and validation.

## Observed format and implementation

Compiling a standalone descriptor against the original ET `aasfile.h` gives
the identity `EAAS`, version 8, a 124-byte header, and these fourteen disk
strides: `32, 12, 20, 8, 4, 24, 4, 48, 32, 44, 12, 20, 4, 16`. Ground
steepness is at area-settings offset 28; the sixteen-bit travel time is at
reachability offset 40. Those layouts match the existing RTCW AAS8 decoder.
ET retains its single `maps/<map>.aas` lookup and one navigation world.

ET's original reader also recognizes a checksum marker of `0xffffffff` in
the encoded header, before undoing the header XOR. That marker bypasses only
BSP checksum equality. The shared parser accepts it only when the ET loader
explicitly opts in. Normal ET files and every RTCW load still check equality;
all files still undergo size, extent, stride, geometry, index and tree checks.
The marker is a source observation, not a retail runtime comparison.

The independently authored loader reuses its existing allocation ownership
and failure cleanup. The internal sidecar retains the historical name
`rtcwGroundSteepness`; it is not exposed as a module or disk record. ET's
geometry-to-cost consumer remains a separate open contract.

The original [ET format header](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/aasfile.h)
and [file reader](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_file.c)
are GPLv3 references used for numeric and behavioral observations. No source
text is incorporated. The descriptor includes the local reference header
only while compiling a disposable observation program; no reference header,
binary or proprietary navigation is staged.

## Validation and limits

Before the change, a generated AAS8 load through ET failed. The production
loader now loads and reloads its generated three-area world with distinct
settings and steepness values, unchanged reachability times and the correct
single filename. Every truncated length is refused. Allocation failures at
each load stage, short reads, oversized input, corrupt identity/version/
checksum, a Quake-version file, a nonfinite steepness and a partial record
leave resources reclaimable. The ET marker accepts a mismatched checksum but
does not admit malformed geometry; the same marker remains rejected by RTCW.
ET's write attempt returns false before opening a file or changing loaded data.

The existing RTCW tests retain both navigation hulls, partial loads, map
replacement and cleanup. Quake III, Quake Live and Holomatch controls retain
their own file versions and settings. Routing and index regressions pass.
The complete suite exposed one old routing fixture that still supplied ET
with Quake AAS5. It now supplies generated AAS8 to ET and retains AAS5 for
the other single-world controls; all ten related loader and routing tests
pass. This changes the fixture, not the tested engine binaries.
The full-suite and runtime non-regression results are pinned in the JSON.

On the final binaries, dedicated Q3, QL and Holomatch servers load navigation
and admit a bot. A windowed ET client also joins both teams on the original
retail `railgun` server with pure checks and visibility filtering enabled.
These are shared-startup non-regressions; the ET session contains no AAS
and does not establish loaded ET navigation.

The final complete Win32 suite passes 179 Meson checks with two skips and
no failures. Recursive Python discovery runs 2,374 tests in 1,733.527 seconds
with 121 capability/platform skips and no failures. Earlier checkpoint
identities stay separate from these final binaries and test results.

The stock ET installation has no navigation file. These generated loads do
not establish loaded retail ET navigation, geometry-to-cost calculation,
complete cache queries, bot gameplay or release compatibility. Writing ET
AAS8 remains explicitly unavailable. No presentation path or retail-exact
option is added; existing fnTech3 audio, renderer and input features keep
their scope. Alice remains deferred by the user.
