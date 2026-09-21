# Elite Force Linux fixture follow-up

All 93 Elite Force Python tests pass on Linux i686 and Windows x86. The 115
architecture failure reports from the [full Linux run](2026-09-21-stvef-linux-server.md)
are also covered by passing reruns. These are test-harness corrections; the
production client, dedicated server and renderer binaries remain unchanged.

The shared compiler helper now selects `-m32` on x86 Linux. A Meson cross file
does not automatically apply to compilers launched by Python fixtures, which
previously used the host's 64-bit default and failed the shared engine's
pointer-size guard. Windows retains its existing MSVC target selection.
Elite Force's environment-map fixture also links the system math library.

Running the standalone probes on i686 exposed two invalid float comparisons:
the movie-draw fixture compared a stored colour with an x87 intermediate,
and the patch-bounds fixture compared stored geometry with coordinates still
held at extended precision. The first now uses exact endpoint/midpoint values;
the second materializes the generated coordinates as floats before comparing
them. Neither changes production math or loosens a tolerance.

Validation covers 375 tests using the shared helper or Elite Force fixtures,
plus 61 checks that use the helper indirectly through RTCW fixtures. The first
group initially has ten failure reports; the two float-fixture corrections
pass focused reruns, including the complete 93-test Elite Force set on Linux.
The 61 additional checks all pass. Windows passes its 93-test Elite Force run
and a final ten-test group covering the changed float fixtures, generated
documentation and the packaging check affected by the earlier document update.

The broader Linux suite remains open. Eight reports in the targeted group
concern Enemy Territory LOD/precompiler fixtures. The original full run also
has an ET packet-probe target error and a macOS packaging check that requires
POSIX file modes unavailable on this DrvFS mount. These are retained rather
than suppressed or represented as an Elite Force pass.

The [manifest](evidence/2026-09-21-stvef-linux-fixtures.json) records source
hashes, counts, timings, log identities and remaining failure details. Logs
remain under `.tmp/stvef-gaps`. No full Linux suite pass or release
compatibility claim is made.
