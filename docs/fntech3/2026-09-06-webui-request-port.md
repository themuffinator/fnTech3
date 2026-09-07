# WebUI request and avatar retry port — 2026-09-06

## Source and scope

This incorporates the two completed updates in the FnQL task
[Investigate user stutters](codex://threads/01a077dc-beac-71a0-8004-3d6e5f31f0ba):
request notifications/bulk reads and more efficient pending-avatar retries.
The reviewed source is the local [FnQL](https://github.com/themuffinator/FnQL)
working tree on top of `040c90e613041890bd1b0d5bf0e743031808e886`. The WebUI
updates were **uncommitted**; that commit is not represented as containing them.
Only the task's WebUI changes and supporting tests/probe are incorporated.

The source LICENSE and new implementation headers permit GPL-2.0-or-later,
compatible with fnTech3's GPL license. Original copyright/license headers remain
intact; [credits](../../CREDITS.md) and [notices](../../THIRD_PARTY_NOTICES.md)
identify this as incorporated/adapted implementation. The fnTech3 worktree
already contained merger work at HEAD
`e3cfc5914cfbd8eaeea7134fe102e8338778dfdc`; those existing changes were preserved.

Raw SHA-256 values pin the uncommitted production source files:

| FnQL source file | SHA-256 |
| --- | --- |
| `code/client/awesomium_backend_win32.cpp` | `95914430f4b60ba49d90e96398e285672f2d2b47a49b221dc35a65de8a3b901d` |
| `code/client/cl_webui.cpp` | `02c667e17f91aafd1577f08e5fbd035fb55a086b7d36b6e6127f8a6c7b6521c8` |
| `code/client/webui_backend.hpp` | `6a65a068e6f0ebeabbf31257e07f75d87dbae31a1eb30ae1c92b9b8aa4172dc5` |
| `code/client/webui_native_request.hpp` | `a2c024dc2ac0c5433aa580563ef40181a9618938d12b3c91c9cea8eba7c3ca79` |
| `code/client/webui_request_schedule.hpp` | `5c4a72fc144e8d8f45fb7b9b85041636289f8683d0b1af8fee2ce4252d451abb` |
| `code/client/webui_resource_queue.hpp` | `c02ff7c1fd261f673d8c0b521da928f37aa8b1f47c7cd41c2591ffee3be72cae` |

The local [source manifest](../../.tmp/fnql-webui-port-20260906/source-manifest.json)
also records test/script hashes, selected files, and the initial worktree
status; the reviewed diff is saved beside it. Disposable investigation output remains under
`.tmp/fnql-webui-port-20260906/`; no proprietary fixture was copied into it.

## Resulting behavior

- A browser-owned asynchronous notification wakes the engine's existing trusted
  native-command queue. Once the notification handshake succeeds, an idle view
  needs no synchronous empty-queue reads. Until then, a 60 ms fallback preserves
  delivery independently of FPS. Hidden menus still deliver lobby/connection
  commands, and the eight-request frame budget continues draining backlogs.
- Each request crosses the browser boundary in one bounded string read. The
  ASCII envelope preserves UTF-16 units and a terminal marker so malformed
  Unicode, embedded NULs, truncation, and commands exceeding 1023 UTF-8 bytes
  are rejected without executing a partial prefix. Failed notifications have
  a browser-side watchdog, including for valid requests behind rejected heads.
- Avatar requests share materialization by path, ignoring case and query/hash
  suffixes as the existing avatar resolver does. A steady clock schedules
  retries after 250 ms, 500 ms, 1 second, then at most every 2 seconds. Each
  browser pump processes at most four paths, including initial attempts.
- Avatar-loaded/persona-avatar events wake the affected player's supported size
  variants, and provider recovery wakes all pending paths. Event handlers defer
  pixel fetching/PNG encoding to the budgeted pump. Navigation/reload clear
  abandoned requests, reused request IDs are checked against generations, and
  overflow/invalid resource routes finish with an empty image response.

fnTech3 retains its `Com_GameIsQuakeLive()` guards for WebUI activation,
initialization, per-frame processing, commands, and transition handling. Q3
does not opt into the browser by setting `cl_webuiEnable`. The browser-neutral
backend interface advances to version 3; no Q3/QL retail module ABI, wire
protocol, or Steam provider ABI is changed.

## fnTech3 validation

These results use the ported fnTech3 source, separately from the earlier FnQL
task's evidence. Host: Windows; engine and retail probe: x86. Existing release
build directory: `.tmp/meson-msvc-x86-release`, Meson 1.10.2, MSVC 19.44.35222,
`buildtype=release`, `strict-warnings=true`, static CRT, all three renderer
modules. This was an incremental build of the dirty merger worktree.

```powershell
# x86 MSVC developer environment:
python -m mesonbuild.mesonmain compile -C .tmp/meson-msvc-x86-release
python scripts/verify_webui_requests.py --retail-path "C:\Program Files (x86)\Steam\steamapps\common\Quake Live" --work-dir .tmp/fnql-webui-port-20260906/retail

# Normal shell, so standalone Clang fixtures select matching system libraries:
python -m mesonbuild.mesonmain test -C .tmp/meson-msvc-x86-release --no-rebuild --print-errorlogs
python -m unittest discover -s tests -p 'webui*_tests.py'
```

- **Unit:** all 79 focused WebUI Python tests passed, including execution of the
  shipped wake/queue JavaScript under Node. C++ tests exercise request scheduling,
  Unicode/buffer boundaries, exact avatar retry times, fairness, wakeups, queue
  overflow, and stale/reused IDs.
- **Complete suite:** 103 Meson test groups passed, zero failed, and the optional
  OpenAL loopback group skipped because `ALC_SOFT_loopback` was unavailable.
  The recursive Python group ran 1,218 tests with seven optional skips, covering
  both Q3 and QL profile regressions as well as the new bridge tests.
- **Build:** strict x86 release compilation/linking passed. Built client
  `.tmp/meson-msvc-x86-release/fntech3.exe` SHA-256:
  `f64790d716d88e75836ec08535e91276f689e32689b3a5e175a14097843c470b`.
- **Fixture:** all 12 offscreen retail probe checks passed: handshake/idle,
  paused backlog, bounded strings, malformed heads with valid tails, watchdog,
  malformed notification routes, invalid resource completion, paused avatars
  and shared buffers, fallback delivery, four-path budget, stale-avatar cleanup,
  and notification resets across navigation/reload/shutdown.

The probe compiled the production adapter and extracted fnTech3's production
queue/wakeup script. It used a generated one-pixel PNG and fake host resource
callbacks; it did not access Steam accounts or real avatars. It opened no game
window, controlled no keyboard/mouse input, and captured no operating-system
images. No gameplay or engine-renderer observation is claimed.

| Artifact/fixture | SHA-256 |
| --- | --- |
| `webui_retail_request_probe.exe` in local probe directory `80179c5034f841ef8f52561579eb1f0a` | `07fa66d4d8436af9bce34f1f67e21ade6390f4a900575df100b7f32a30eca307` |
| External retail `awesomium.dll` | `de27851602cb1387ef50086761b417d3d39a2a2648b5490939f843a711ff7f6e` |
| External retail `awesomium_process.exe` | `3067bdb00878ebf271f210d06257dc5f0fd4d4c34bb17195fbad067db3174c6f` |
| External retail `web.pak` | `338862a0384f71fb962a71e0c77a686ae5d2204c88eebe9ccea3a8dfee13793b` |

Local logs: [build](../../.tmp/fnql-webui-port-20260906/build.log),
[full tests](../../.tmp/fnql-webui-port-20260906/tests.log), and
[retail probe](../../.tmp/fnql-webui-port-20260906/retail.log).

The runtime-tag install refreshed `.install/windows-x86` with the same eight
engine-owned build artifacts. All staged hashes match the build; Windows
dependency checks passed for all six PE files, and root-package validation
passed (149 audio-zone sidecars, 149 fog sidecars, one sound shader). The staged
client has the hash above. Updated credits, notices, ledger, and this record
accompany it. The stage's `ARTIFACTS.json` separates this update's validation
from the earlier engine-session evidence; no new Q3/QL game session was run.
The local [validation manifest](../../.tmp/fnql-webui-port-20260906/validation-manifest.json)
records current source and staged-artifact hashes.

## Limits and unrelated findings

- The original Linux/Proton stutter report still requires gameplay confirmation
  on the affected setup. This task does not transfer FnQL timing measurements
  into a fnTech3 frame-time claim. Live Steam callbacks through the full engine,
  renderer interaction, and broader lifecycle/interop gates remain open.
- FnQL's task reported that the query-suffixed document URL
  `asset://ql/index.html?fnql_avatar_probe=1` reached `chrome://chromewebdata/`
  in retail DataPak, breaking the next reload handshake. That separate limitation
  is inherited and was not repaired or independently reproduced here. The
  successful fnTech3 probe uses and asserts canonical `asset://ql/index.html`.
- The first complete suite invocation inherited the x86 MSVC build environment.
  Thirteen fixture/subtest failures involved Clang's default x64 output linking
  against x86 CRT libraries (`LNK4272`/`LNK1120`), outside the WebUI changes.
  The complete suite passed from the normal shell; no unrelated test implementation is
  changed to conceal that toolchain conflict. The first run is preserved in
  [its log](../../.tmp/fnql-webui-port-20260906/tests-msvc-environment.log).
