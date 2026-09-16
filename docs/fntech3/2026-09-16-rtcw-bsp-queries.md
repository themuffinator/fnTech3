# RTCW BSP entity and property queries

The shared executable now implements RTCW game imports 311–315: BSP entity
iteration and string, vector, float and integer property reads. The installed
SP module passes these queries after loading both `escape1` AAS8 hulls. Its next
stop is import 205, `BOTLIB_START_FRAME`; campaign and AI routing remain open.
RTCW is still gated in default builds. This evidence describes the dirty tree
at `c75573c7375e44c908256020ada2d7e12970cacf`, not that commit alone.

## Behavior and bounds

The adapter uses the real shared BSP table loaded by the bot library. It checks
signed 32-bit entity/capacity arguments and exact QVM output extents. Native
arguments reject null/address wrap without claiming to validate arbitrary
native allocations. Keys are copied before output can overwrite them, so
overlapping key/output buffers are supported. Scalar/vector results use aligned
local storage before copying to possibly unaligned module output.

Property names are case-sensitive, and the first matching linked property wins.
On a successful string lookup RTCW zero-pads the whole supplied capacity, as
original `strncpy` does. A missing property clears only the first output byte
and returns false. Truncation still reports a found property. Q3/QL retain their
existing string-copy behavior. Zero-capacity output is a checked no-op returning
false; a negative capacity is rejected at the module boundary.

Numeric lookups retain RTCW's 128-byte intermediate buffer and success status
when a property exists but its text has no numeric prefix. Vector conversion
uses the host C library's three double conversions, including whitespace,
partial conversion and initially zero missing components. The Q3/QL token
splitter remains unchanged. Negative zero is retained. Numeric overflow and
nonfinite values instead produce a logged failure and zero output; these
malformed cases are intentionally stricter than the original engine. Floating
values must fit the finite binary32 magnitude range before conversion.

An inherited shared-library issue is also fixed: entity iteration no longer
increments `INT_MAX` before checking its bounds. Invalid entities return zero.
The raw shared string getter also checks null output and nonpositive capacities
before writing, while preserving valid Q3/QL behavior.

## Evidence

`tests/rtcw_bsp_query_tests.py` executes the production getters and real import
adapter, with minimally sized trap-argument arrays. It checks all four profiles,
native/QVM callers, padding and trailing bytes, partial vector conversions,
case-sensitive/missing keys, unaligned and overlapping output, capacities above
the source token limit, exact VM ends, nonfinite values, integer overflow and
empty BSP state. The fixture runs at `-O2 -ffast-math`.

Six focused query/bootstrap/navigation/loading tests pass on Windows x64
(7.519 s), Windows x86 (9.375 s), and Linux x86 with ASan/UBSan and leak detection
(35.074 s). Logs are `bsp-query-{x64,x86,sanitizers}.log` under
`.tmp/rtcw-manager/`.

`scripts/probe_rtcw_bsp_queries.py` compares unchanged original SP, original MP,
iortcw SP and iortcw MP getter functions against the production implementation.
Every reference passes 250,200 string-buffer comparisons, 93,825 numeric
comparisons and 36 iterator comparisons on Windows x64/x86 and Linux x86 with
sanitizers. The corpus covers long/truncated strings, duplicate/missing keys,
decimal and hexadecimal floating-point forms, incomplete conversions, signed
zero and bounded random components. Undefined reference calls such as an
overflowing integer conversion, `INT_MAX` iterator increment or zero-capacity
write are excluded and tested against the checked implementation separately.
Reports are `bsp-query-reference-{x64,x86,sanitizers}.json`.

The experimental Windows x86 client/server build passes. Its client SHA-256 is
`edb5dce10eed3545fd9452bccf0900705ef54caa7dbe8857afa9a3aa903ba6fc`.
`bsp-query-map/manifest.json` records SP's advance to import 205 and MP's passing
deployment-menu/briefing-movie boot with normal exit. SP is still a failed map
boot, not campaign or rendering evidence. Both probes used windowed unswitched
private desktops, disabled input/audio, isolated writable homes and unchanged
installed assets.

The default Windows x86 client/server and all three renderer builds also pass.
Its client SHA-256 is
`a2ce5b0ae4b7c1121b5d2f1b278cedeae6bcd811a61283e1c6aa8acdee68a11c`.
The complete suite passes 111 Meson tests with two skips, including 1,380 Python
tests in 351.961 seconds with 11 skips. Logs are `bsp-query-default-build.log`,
`bsp-query-full-tests.log` and `bsp-query-full-testlog.txt`.

All six engine screenshots from Q3 `q3dm6` and QL `campgrounds` pass visual
inspection across GLX, Vulkan and RTX, as does the MP briefing-menu screenshot.
Q3 shows the textured map, weapon and HUD; QL shows the textured map and join
menu. These are rendering/module smoke checks, not peer interoperability or
pixel-parity evidence. Commands, installation identities and artifact hashes
are recorded in `bsp-query-live/manifest.json` and `bsp-query-map/manifest.json`.

## Next routing work

Original RTCW frame startup visits both navigation worlds, advances their
clocks, invalidates shared entity state and continues each world's routing
initialization. It leaves world one selected and increments that world's frame
counter. Original initialization also creates or reads visibility/routing
caches. It calls the SP route-table builder, but that function returns
unconditionally before building anything. The installed archives contain 70
`.rcd` routing-cache files (134,377,347 uncompressed bytes), with none of the
separate `.rtb` route tables. The inherited Q3 path has different area
contents/travel flags, costs and cache contracts, so forwarding import 205 to
it is insufficient. Those are source observations for the next implementation,
not completed behavior or live AI evidence.

## Provenance

The code is independently written from behavior facts in the original
[SP bot library](https://github.com/id-Software/RTCW-SP/tree/master/src/botlib),
[MP bot library](https://github.com/id-Software/RTCW-MP/tree/master/src/botlib)
and [iortcw](https://github.com/iortcw/iortcw). Their GPLv3 implementation text
is used only in disposable reference fixtures with notices preserved. The
production changes extend the inherited GPL-2.0-or-later engine and add no
retail game modules or assets.
