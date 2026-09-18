# WolfMP favorites carried into fnTech3's MP browser

The RTCW matrix kept "legacy MP favorites import" open for the MP filesystem row.
fnTech3's `rtcw-mp` profile keeps its browser cache as
`main/fntech3-rtcw-mp-servercache.dat`, in fnTech3's own record layout, so a
player's favorites from retail `WolfMP.exe` were not carried over. This record
adds a one-way import and checks it against the retail executable. The
[machine-readable record](2026-09-18-rtcw-mp-favorites.json) holds identities,
commands and results.

## Original cache

Original WolfMP keeps its browser cache as `servercache.dat` at the root of its
home path, which for retail is the installation directory (original
[RTCW MP](https://github.com/id-Software/RTCW-MP) `src/client/cl_ui.c`, read
only). The file holds the global, mplayer and favorite counts, the size of the
three arrays, then 2,048 global, 128 mplayer and 128 favorite `serverInfo_t`
records, 470,032 bytes in all. On x86 each record is 204 bytes and starts with a
20-byte `netadr_t`: type, IPv4 address, IPX address and port in network order,
then the host name. WolfMP's loader checks only the stored array size and
discards every count when it differs.

## Correction

[`code/compat/rtcw_server_cache.hpp`](../../code/compat/rtcw_server_cache.hpp)
reads the IPv4 favorites from a complete WolfMP cache. It needs the exact file
length, the stored array size and a favorite count within the array, skips
other address types (the retired IPX among them), and cuts host names to fit.
When the MP profile starts with no fnTech3 cache, `LAN_LoadCachedServers` reads
WolfMP's `servercache.dat` from the root of the home path, falling back to the
installation and then the Steam installation as the engine's other root-file
reads do, and adds each favorite through the ordinary favorite-add path. fnTech3 then saves the favorites in its own cache when the UI shuts down,
so later starts do not import again. WolfMP's file is only read. Q3, QL and SP
never import it.

## Evidence

[`scripts/probe_rtcw_favorites_import.py`](../../scripts/probe_rtcw_favorites_import.py):

- **Layout.** The original MP `netadr_t` and `serverInfo_t` declarations,
  compiled unchanged for x86 in a disposable fixture, measure a 204-byte record
  with the address type at 0, IPv4 at 4, port at 18, host name at 20 and
  `NA_IP` = 4. These match the header's constants.
- **Retail round trip.** A generated cache with two IPv4 favorites and one IPX
  favorite goes into an isolated home for retail `WolfMP.exe`, which starts to
  its main menu and quits. Retail rewrote the file byte-for-byte identical, with
  three favorites. A control copy whose stored array size is one record short
  came back with every count zeroed, so retail was checking the layout.
- **Import.** The retail-written file goes into a fresh home for the fnTech3
  client. The first start logs "Imported 2 favorite servers from WolfMP's
  servercache.dat" and saves both IPv4 favorites in fnTech3's own cache. The
  second start imports nothing and still has both. WolfMP's file is unchanged.

All runs are windowed on unswitched private desktops with sound and
mouse/joystick input disabled, and their commands come from startup configs.
The client was built from a clean worktree of the commit named in the JSON plus
this change only.
[`tests/server_cache_tests.py`](../../tests/server_cache_tests.py) now covers
the import against controlled disk I/O on the x64 and x86 targets. It checks
which favorites are added, the skipped IPX entry, the cut host name, records
past the count, that an existing fnTech3 cache (valid or damaged) is never
merged, that incomplete or iortcw-sized files are rejected, and that the other
profiles never import.

## Remaining

- iortcw MP writes the same file name in a different layout (ioq3 addresses,
  two more fields, no mplayer list) that also varies by platform. No iortcw MP
  is installed here, so its favorites are not imported.
- The imported favorites were not viewed in the MP browser menu, which needs
  menu input.
