# Exercise the legacy Q3 native qagame boundary

An independently authored test DLL now exercises the actual Windows x86
dedicated engine's legacy dllEntry/vmMain qagame path. It complements the
[cgame/UI fixture](2026-09-08-q3-native-ui.md), using public ABI observations
from the GPL-compatible [pinned FnQuake3 reference](https://github.com/themuffinator/FnQ3/tree/7d8c9629cdc7979f09697266470359101ffd9eda).
No retail implementation text or game behavior is incorporated.

The native fixture and opt-in runner cover:

- A 516-byte Q3 shared-entity prefix inside a larger, four-byte-aligned module
  stride. All 64 entity records and 64 allocated 468-byte player-state prefixes
  have eight-byte guards on either side. The fixture binds these arrays through
  G_LOCATE_GAME_DATA; this does not establish player-state snapshot marshalling.
- Module-to-engine link/unlink and area-query round trips on entity index 1,
  including updated linked/linkcount fields, expanded absolute bounds and
  preserved guards/private stride bytes.
- A guarded 56-byte trace that hits the linked fixture box, a matching point
  contents result, and removal from the area query after unlinking.
- A guarded 24-byte Q3 usercmd transfer for an unused client slot (all zero),
  a bounded configstring round trip and the first BSP entity token. No user
  commands are injected; nonzero live usercmd fields remain outside this probe.
- Init, run-frame, console-command and shutdown dispatch, followed by native
  unload/reload and repeated checks across map_restart.
- A deliberately invalid 515-byte entity stride. The server reports
  `entity stride too small`, shuts down and unloads that module before accepting
  a fresh valid binding. The recovery run then repeats the normal map/restart
  checks and exits with status zero without forced termination.

ERR_DROP intentionally leaves the dedicated process available for recovery and
clears its old command buffer. During shutdown, the test module uses the public
console-command import to queue a fresh valid test config. This proves a valid
map can load after rejection. It neither changes the engine's error policy nor
uses keyboard/console-window input. An initial probe incorrectly expected a
fatal exit and was stopped at its 30-second deadline; its log established the
recoverable-error behavior and prompted this harness correction.

Both final cases pass with the same unchanged fnTech3 dedicated executable,
stock pak0-pak8 inputs and separate temporary writable homes. sv_pure is disabled
for the synthetic DLL. Networking uses IPv4 loopback, master addresses are empty,
and Steam integration is disabled. Private desktops are never switched; input
is disabled and no screenshot is needed for this server-only probe. Proprietary
assets and generated DLLs remain outside Git.

Set `FNTECH3_Q3_QAGAME_RUNTIME_EXE` to the Windows x86 dedicated executable and
run `python tests/q3_native_qagame_runtime_tests.py`. Optionally set
`FNTECH3_Q3_RETAIL_PATH` to a legitimate asset root and
`FNTECH3_Q3_QAGAME_PROBE_OUTPUT` to a new output directory. The runner requires
MSVC x86 and skips explicitly on other platforms. The
[evidence record](2026-09-08-q3-native-qagame.json) preserves binary, source and
pak hashes, compile/run commands, config, callbacks, rejection diagnostics and
normal-exit state for both cases.

The full Windows Meson suite passes 110 tests with one OpenAL skip. Its Python
aggregate runs 1,273 tests with five skips, with Q3 qagame, Q3 cgame/UI and QL
native probes enabled. The final fixture also passes its focused stock-asset
run. The Linux runner import check reports the intended Windows-only skip.

This is synthetic native contract evidence, not a complete import/export audit,
native retail game equivalence or a live client/player-state interoperability
test. Those gates, QL snapshot-tail meaning and broader compatibility/release
requirements remain open. No new unrelated engine defect was found; the optional
missing autoexec warning remains.
