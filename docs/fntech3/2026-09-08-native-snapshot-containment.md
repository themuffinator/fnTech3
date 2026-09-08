# Exercise native snapshot containment with exact output bounds

The synthetic native cgame fixture formerly allocated 1 MiB for snapshots and
accepted any change in the first four bytes. It now allocates the known native
0x16488-byte envelope with eight-byte guards before and after it. This improves
runtime evidence for the production snapshot adapter without changing engine
behavior or claiming to understand the unresolved retail player-state tail.

The probe first requests snapshot -1 and requires rejection without changing
any poisoned output or guard byte. It then fetches the current snapshot and
checks both guards, the queried server timestamp, entity count at 0x27c
(maximum 384), and entity numbers at the 0xec-byte stride (less than 1,024).
It verifies that the currently unknown bytes from 0x224 through 0x27b and
unused entity storage through 0x1647f are zero. These checks describe fnTech3's
existing containment policy; zero values do not establish retail field meaning.

The validator regression covers timestamp mismatch, excessive count, invalid
entity number, nonzero bytes at either end of the unknown tail, unused-slot
contamination, a full-capacity array, and first-byte-only writes. It passes on
Windows and Linux hosts. The strengthened fixture compiles as an MSVC x86 DLL
and passes through the actual import table of the current Windows x86 client.
All twelve native ABI/callback markers pass, followed by normal engine exit.
The full Windows suite with the probe enabled passes 110 configured checks,
zero failures and one unavailable OpenAL skip; the aggregate passes 1,269 tests
with five skips.

The opt-in runner uses a packed legitimate QL installation read-only, an
isolated writable home, a private unswitched desktop, windowed GLx, disabled
input/optional services, and engine-controlled startup/quit commands. No input
injection or screen capture occurs. Pure remains disabled for this synthetic
module, so the run is not retail pure or peer interoperability evidence.

The [evidence record](2026-09-08-native-snapshot-containment.json) contains
commands, binary/fixture hashes and observed markers. The snapshot tail's
meaning, full-capacity live scenes, retail-peer behavior and broader ABI gates
remain open. No new unrelated engine issue was found.
