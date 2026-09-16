# RTCW limbo chat and cinematic imports — 2026-09-15

MP's cgame submission and UI retrieval imports now share seven lines of
map-owned chat history. New messages move older messages down one row. The
original contract counts 140 visible bytes, retaining colour escapes without
counting them toward that limit. Each stored line has 421 bytes. Retrieval
writes exactly 140 bytes, padding short strings with zero; a full-width result
does not promise a trailing zero. Clearing client map state clears the history.

The implementation preserves original RTCW's colour-escape rule. iortcw limits
colour escapes to alphanumeric bytes; that later difference is recorded rather
than applied to the original MP profile. Invalid negative indices return false
without touching the output. Excessive colour sequences are bounded to stored
capacity without splitting a pair. These deliberately avoid out-of-bounds
behaviour in the reference. A failed QVM read leaves all old messages intact.

SP/MP UI and cgame cinematic imports now have a shared checked adapter into the
existing RoQ player. It validates integer widths, copies bounded filenames from
explicit native/QVM memory, retains the original 256-byte filename/prefix
limit, and forwards the five common flags and exact status encodings. Both
RTCW modules use the original independent 640×480 axis scaling. Q3/QL retain
their existing dispatch and UI aspect option. Missing movies return the
player's failure result; the adapter does not manufacture playback success.

SP's additional letterbox flag is explicitly rejected pending implementation
and presentation evidence. This adapter does not establish RoQ frame, audio,
seeking, EOF/loop timing or campaign cinematic parity. The default RTCW release
gate remains closed.

## Evidence

`scripts/probe_rtcw_limbo.py` compiles the unchanged original MP and iortcw
history/getter functions in disposable fixtures with their GPLv3 notices.
It also uses each reference's own colour predicate. Windows x64, Windows x86
and Linux x86 ASan/UBSan each pass 8,468 submissions and 84,680 row reads.
All comparisons match original RTCW. The reports explicitly verify 192
iortcw colour-predicate differences. Source/implementation hashes and compiler
commands are in `.tmp/rtcw-manager/limbo-reference-{x64,x86,sanitizers}.json`.

The chat, client-state and UI-dispatch tests pass three cases on Windows x64
(1.812 s), x86 (2.410 s) and Linux x86 ASan/UBSan (10.352 s). The subsequent
cinematic/chat/client-state/UI/renderer boundary set passes five cases on
Windows x64 (3.048 s), x86 (4.158 s) and Linux x86 ASan/UBSan (25.735 s).
These execute actual dispatch prefixes and adapters, checking native/QVM
bounds, exact argument counts, integer overflow, return values, ownership,
history capacity, colour limits, atomic failures and profile isolation.
Logs are `limbo-chat-*` and `cinematic-boundary-*` in the same task directory.

The chat development client, SHA-256
`1e6ed8627c7c89e30aee7dfb554f1ebd70cd2a305b189917a4e222f0d8df13f7`,
passes installed MP's limbo-history retrieval and advances to UI import 82
(`UI_CIN_PLAYCINEMATIC`), which was not yet wired in that executable. SP remains
at game import 206 (`BOTLIB_LOAD_MAP`). `limbo-chat-map/manifest.json` records
both attempts, renderer API 23, unchanged installation inventory and cleaned
isolated homes. It contains no successful map-rendering claim.

With the cinematic adapter, development client SHA-256
`a06284fd2f46a7f8d006fe4c7c79d658a1649c4fc8d95fb0377dbcf634a3150f`
reaches the deployment menu and decodes `video/mp_beach.roq`, but hangs during
shutdown. The engine capture and failed exit are retained in
`cinematic-boundary-map/manifest.json`. The inherited movie owners could retain
file handles until static destruction, after filesystem teardown. Module
shutdown now closes all movies. Bulk movie cleanup also releases handles for
partial initialization, EOF and changed connection state, even where the
ordinary stop operation returns early.

The actual movie-owner/stop/shutdown code passes two targeted tests with the
cinematic adapter on Windows x64 (1.259 s), Windows x86 (2.031 s), and Linux
x86 ASan/UBSan (8.675 s). Logs are `cinematic-shutdown-{x64,x86,sanitizers}.log`.
The corrected development client, SHA-256
`8fb6a56f42529bece0411fa1952765c43a24a1921b3b8f0bb36b45416818b78b`,
passes MP local map boot, native modules/pure admission, deployment-menu movie,
engine capture and normal exit in GLX, Vulkan and RTX raster. Reports are
`cinematic-shutdown-map/manifest.json`,
`cinematic-checkpoint-mp-vk/manifest.json` and
`cinematic-checkpoint-mp-rtx/manifest.json`.

All three MP captures were inspected. They show a decoded briefing movie and
readable deployment UI, but white gradients/borders and clipped chat labels
remain. The UI obscures the world. The manifests record native boot success
separately from **visual failure**. This is not a playable-map or renderer
parity claim. SP still stops at game import 206 on the same executable. All
probes are windowed on unswitched private desktops with no injected input,
unchanged installation inventories and removed isolated writable homes.
The subsequent [material-directive checkpoint](2026-09-15-rtcw-material-compression.md)
fixes these menu defects with new renderer modules; the original failed visual
observations remain attached to the earlier artifacts.

The same development executable also passes SP-main-menu → MP-main-menu and
MP-active-briefing → SP-main-menu transitions. The latter closes a playing
movie before relaunch. Both directions observe two processes using the same
executable, private-desktop windows, correct destination menus, normal exit
and unchanged installation inventory. All four engine captures were inspected;
the MP source capture retains the known visual defects. Transition success and
visual scope are separate in `cinematic-checkpoint-handoff/manifest.json`.

The default Windows x86 client/server and all three renderers rebuild
successfully. The full Meson suite passes 111 checks with two skips. Its Python
aggregate passes 1,376 tests (11 skips, 337.485 s). Retained logs are
`cinematic-checkpoint-default-build.log`, `cinematic-checkpoint-full-tests.log`
and `cinematic-checkpoint-full-testlog.txt` under `.tmp/rtcw-manager/`.
This checkpoint includes the loop, viewpoint, chat, cinematic and file-owner
changes. The default client, SHA-256
`780ad7be9cf62586e3478c119645df37faca9b9152a747345fc505d5b8d0616c`,
passes six inspected Q3/QL captures across GLX, Vulkan and RTX raster, using the
same executable. Q3 shows the world, weapon and HUD; QL shows its native join
menu over the world without joining. `cinematic-checkpoint-live/manifest.json`
records module identities, renderer selection, windowed mode and image hashes.

Production implementation is independently authored under GPL-2.0-or-later
using reference behaviour facts. No reference implementation text, installed
game modules, chat logs or movies are incorporated in the repository.
