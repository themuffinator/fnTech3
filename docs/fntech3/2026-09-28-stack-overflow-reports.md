# Stack overflows reported, not fatal, on Windows

The [node-tree record](2026-09-27-bsp-node-trees.md) found that a runaway
recursion could not be survived on Windows: damaged maps whose node trees loop
back on themselves made `CM_TraceThroughTree` recurse until the main thread's
stack ran out, and all eight such crashes ended in an access violation at
`_chkstk`, below `Com_sprintf`, inside the engine's exception filter. The
filter ran on the exhausted stack, its report needed more stack than was
left, and that second fault ended the process, where any other access
violation becomes an `ERR_DROP`. The loaders now refuse such trees, but any
other runaway recursion would have ended the same way.

The filter now handles a stack overflow on the main thread without the stack
it no longer has. It notes where the overflow was raised and leaves the frame
at once; the report runs from the frame's abort point, where the stack is
whole again, puts the stack's guard page back, and raises the same `ERR_DROP`
every other fault gets, naming the exception `STACK_OVERFLOW`. Every other
exception takes the path it took before. A harness test overflows a small
process on purpose eight ways and passes where each of eleven mutations of
the change fails it; on the stable `win32-dev` build, 14 deliberate faults in
Quake III and Quake Live clients and dedicated servers are all reported and
survived, each server taking four in a row and then quitting cleanly; and on a
build without the node-tree check, the eight damaged trees that crashed every
profile are now reported and survived, where the same build without this
change still crashes on all eight. The
[machine-readable record](2026-09-28-stack-overflow-reports.json) holds every
measurement and run.

## The change

[`win_main.cpp`](../../code/win32/win_main.cpp):

- `ExceptionFilter` keeps a small frame. For `EXCEPTION_STACK_OVERFLOW` on the
  main thread it stores the exception's address and calls
  `Com_ErrorAfterUnwind( ReportStackOverflow )`, which does not return.
- `ReportStackOverflow` runs at the abort point. It calls `_resetstkoflw` to
  put back the guard page the overflow used up, without which a later
  overflow would end the process, then `ReportException`.
- `ReportException` is the old filter's body unchanged (restore the gamma ramp,
  hide a fullscreen window with no client left, name the module and offset,
  `Com_Error( ERR_DROP, "Unhandled exception caught\n..." )`), kept out of the
  filter's frame by `noinline`. Every exception but a stack overflow goes
  straight to it, so an access violation, a breakpoint and the rest behave as
  before.
- On any other thread there is no abort point to unwind to and no stack for a
  report, and `Com_Error` could only longjmp across threads: the filter puts
  the desktop back and returns `EXCEPTION_CONTINUE_SEARCH`, so Windows ends
  the process with `0xC00000FD`, where it used to fault again with
  `0xC0000005`. The engine's own code starts one such thread, the WASAPI
  mixer's; the others are its libraries'.
- `InstallExceptionFilter`, called where `WinMain` set the filter before,
  records the main thread and asks Windows to keep 32 KB of its stack back
  for handling a stack overflow (`SetThreadStackGuarantee`), where Windows
  keeps one page by default.

[`common.c`](../../code/qcommon/common.c):

- `Com_ErrorAfterUnwind( report )` leaves the frame as `Com_Error` leaves it,
  by `longjmp` to `abortframe`, but prints, shuts down and formats nothing
  first. It unlinks native modules' exception registrations first, as
  `Com_Error` does, so a module's frames are abandoned the way an `ERR_DROP`
  abandons them and no module code runs on what is left of the stack, and it
  moves the print redirect's text into a buffer of its own (below).
- `Com_Init` and `Com_Frame` call `Com_RaiseUnwoundError` first at their abort
  point; it calls the report left there. During initialization the
  `ERR_DROP` then becomes `Error during initialization: Unhandled exception
  caught ...`, as an `ERR_DROP` there always has.
- `crash stack` joins the developer-only `crash`: `Com_OverflowStack` recurses
  until the stack runs out. It is the harness the stable build is probed
  with, since the loaders now refuse the maps that found the problem.

The report is `Unhandled exception caught`, `Exception Code: STACK_OVERFLOW`
and `Exception Address: module@offset`, the same three lines every caught
fault prints, so `scripts/probe_bsp_faults.py` and the map-load sweep still
count it as `faulted`.

### What the abandoned frames may still hold

`Com_Error` does its work while the frames it leaves are still on the stack.
Here the report's own calls reuse that memory, so nothing the report uses may
point into it. The print redirect can: rcon prints into a buffer that is a
local of `SVC_RemoteCommand`, and the ZMQ rcon's is a local too. The first run
of the stable-build probe showed it. Quake Live's dedicated server, sent
`crash stack` by rcon with a map running, answered with the tail of the
level's shutdown and nothing before it; `Com_sprintf`'s 32,000-byte buffer
and the rest of the report had overwritten the buffer while `Com_Printf` was
still writing through the redirect. Quake III's replies happened to come back
whole. Under the same conditions `crash` and the no-fault `error drop`
returned the whole reply. `Com_ErrorAfterUnwind` now copies the redirect's
text into a buffer of its own, with its flush, before it leaves the frame, and
the report reaches the rcon client just as an `ERR_DROP`'s does. With that,
Quake Live's replies to an overflow in a running map hold the whole report
too. The harness test's `redirect` case pins it.

## The stack on the path

Frames from the stable build's program databases (`llvm-pdbutil dump
-symbols`, each procedure's `S_FRAMEPROC`), bytes of locals plus callee-saved
registers:

| After a stack overflow, on the reserve | Client | Server |
| --- | ---: | ---: |
| `ExceptionFilter` | 0 + 4 | 0 + 4 |
| `Com_ErrorAfterUnwind`; `Q_strncpyz` for the redirect copy | 0; 0 + 4 | 0; 0 + 4 |
| `VM_UnlinkNativeExceptionRecords` | 16 | 16 |
| `Sys_UnlinkModuleExceptionRecords`; the list walk's `NativeSEH_UnlinkOwned` and `NativeSEH_HandlerInModule` | 40 + 16; 0 + 16 and 28 + 8 | the same |
| `longjmp`, then the CRT's `_global_unwind2` and `ntdll!RtlUnwind` | 0, then not in the databases | the same |

| The old report, which ran on the reserve | Client | Server |
| --- | ---: | ---: |
| the old filter, now `ReportException` | 392 + 12 | 392 + 12 |
| `Com_sprintf`, where it faulted | 32,004 + 16 | 32,004 + 16 |
| `Com_Error`, then its first call `Sys_CancelRelaunch` | 0 + 4, then 131,072 | 0 + 4, then 131,072 |
| `Com_Printf`, then `Conbuf_AppendText` | 8,236 + 12, then 16,388 + 8 | the same |

The whole report now runs at the top of the stack, after the unwind.

The operating system's part, the exception's dispatch and `RtlUnwind`, is not
in the databases, so it was measured. In the test's harness, built from the
same production code, the deepest write that the dispatch, the filter, the
unlink and the unwind made was 3,132 bytes below the recursion's deepest
frame; 3,208 through C++ frames whose destructors the unwind ran (3,136 and
3,212 for the dedicated variant). Windows raised the overflow with the deepest
frame 40,980 to 41,044 bytes above the stack's reservation base; the handling
went no lower than 37,808 bytes above it, so 33,712 bytes of the 36,864 that
the reserve and the guard page hold stayed untouched. Without the reserve, on
this host (Windows 11 26200, the x86 builds under WoW64), the overflow is
raised with the deepest frame 12,328 bytes above the base and the filter had
6,423 bytes left. The bare path fits in that too; the reserve is for what the
unwind runs besides it, destructors of C++ objects in the frames it leaves,
which the harness's `cleanup` case needs 16 KB for and a reserve of 8 KB
fails.

The reserve raises the point where an overflow is caught by about 28 KB (KB
here are 1,024 bytes). Read from outside after a map loaded and ran 120
frames, the main thread had committed 880,640 to 909,312 bytes of its
1,048,576-byte reservation on nine of ten maps across nine profiles (442,368
on `grabtex`): the GLx and Vulkan renderers' patch subdivision,
`R_SubdividePatchToGrid` and `R_GridInsertRow`/`Column`, has frames of
734,532 and 733,276 bytes. The deepest, Enemy Territory's `radar`, reached
about 172 KB above the base, leaving about 132 KB before an overflow, where it
left about 160 KB before.

## Tests

[`tests/windows_stack_overflow_tests.py`](../../tests/windows_stack_overflow_tests.py)
compiles the production filter, report and unwind (sliced from
`win_main.cpp`, `common.c` and `q_shared.c`, the real `Com_sprintf` and
`Q_strncpyz` among them, and `Com_Frame`'s own abort point) into an x86 process
built with MSVC like the engine (its Clang fallback, `i686-pc-windows-msvc`,
passes the same cases), and runs one process per case, for the client and the
dedicated server:

| Case | What must happen |
| --- | --- |
| `overflow` | `Com_OverflowStack` runs the stack out; the report names `STACK_OVERFLOW` and the harness's module at the faulting instruction inside `Com_OverflowStack`; the whole reservation but its last page is committed, so the stack really ran out; the process carries on |
| `repeat` | three overflows in a row, each reported, so the guard page came back each time |
| `cleanup` | about 12,570 C++ frames, each with an object whose destructor the unwind runs, all run before the report; the outermost cleanup takes 16 KB of stack |
| `redirect` | a frame prints through the redirect into a buffer of its own and overflows; what it printed and the report reach the redirect's owner in one flush |
| `native` | a stand-in native module's frames, about 19,340, each link an exception registration; the dispatch calls every one (the filter would not run otherwise), all are unlinked, none is unwound, and the report names the module |
| `access-violation` | still reported on the faulting stack, as before |
| `breakpoint` | still no report: the desktop goes back and the process ends with `0x80000003` |
| `thread` | an overflow on another thread: the desktop goes back and the filter returns `EXCEPTION_CONTINUE_SEARCH` |

A source check holds `Com_RaiseUnwoundError` first at both abort points and
`WinMain` installing the filter through `InstallExceptionFilter` before
`Com_Init`. The file passes: 2 tests, 16 subtests.

`8e8de10` with this change, exported as a repository of its own and built
whole with `win32-dev`'s options (1,430 targets, strict warnings), passes the
whole Python suite (2,329 passed, 128 skipped) and `meson test` (175 native
tests, two skipped, besides the wrapper that runs the Python suite). Linux
i686 GCC 13 and Clang 18 compile the new `common.c` with the same warnings as
`8e8de10`'s, for the client and the dedicated server, and MSYS2's MinGW-w64
i686 GCC compiles the new `win_main.cpp` (dedicated) with the same warnings as
its own.

Eleven mutations were built from the sliced sources in memory, and each fails
it:

| Mutation | Runs that fail, of 16 |
| --- | --- |
| the old filter with the old one-page reserve | 12: every overflow and the thread case end with `0xC0000005`, the original failure |
| the old filter, the new reserve kept | 12: the reports come from the faulting stack, without the address the filter now keeps; the thread case ends with `0xC0000005`. The harness's `Com_Error` is lighter than the engine's, whose first call alone takes 128 KB |
| no `_resetstkoflw` | 2: the second overflow in `repeat` ends the process |
| no reserve | 2: `cleanup` ends with `0xC0000005` |
| a reserve of 8 KB | 2: `cleanup` ends with `0xC0000005` |
| the redirect left in the abandoned frame | 2: `redirect`'s flushed text is not what was printed |
| native registrations not unlinked | 2: the module's handlers run in the unwind |
| the unwind taken on every thread | 2: `thread` ends with `0x80000026` |
| an abort point without `Com_RaiseUnwoundError` | 10: no report |
| the unwound error never cleared | 10: reported again and again |
| the report left to another thread | 2: `thread` ends with `0xC0000005` |

## On the stable build

[`scripts/probe_stack_overflow.py`](../../scripts/probe_stack_overflow.py)
runs `meson/build/win32-dev`'s client and dedicated server on unswitched
private desktops, windowed, with isolated homes, driven by startup configs and
loopback rcon. Each client meets one fault, since an error clears the command
buffer: `crash stack` at the menu, `crash stack` with a map running, and
`crash` there. A dedicated server still takes rcon with no map running, so one
server meets five steps: `crash stack` in a running map, `crash stack` with no
map, `crash` with no map, the map again and `crash stack` in it, then `quit`.

| Profile | Map | Client faults | Server faults | Server `quit` |
| --- | --- | --- | --- | --- |
| `q3` | `q3dm1` | 3 of 3 reported | 4 of 4 reported, answering rcon after each | status 0 |
| `ql` | `campgrounds` | 3 of 3 reported | 4 of 4 reported, answering rcon after each | status 0 |

Every overflow names `Com_OverflowStack` (`fntech3.exe@fec47`,
`fntech3.ded.exe@cb707`) and every `crash` names `Com_Crash_f`
(`fntech3.exe@fb099`, `fntech3.ded.exe@c84a9`), by `llvm-symbolizer`. A
server's reports come back in the rcon reply, since an rcon command's prints
go to its sender: the report, the server's shutdown and the game's own
`ShutdownGame`, as for any `ERR_DROP` there. After its drop each client
restarts its renderer and loads its user interface again.

The same build still refuses the damaged trees at load, as the node-tree
record's after build did: `tree-cycle` and `tree-self` of `q3dm1` and
`campgrounds` end `rejected` with `CMod_LoadNodes`' own messages.

## The damaged trees without the node-tree check

A build of `8e8de10` with the five code files `ab42450` changed taken from
its parent, so without the node-tree check, and with this change, ran
`probe_bsp_faults.py`'s `tree-cycle` and `tree-self` for the four profiles
the node-tree record ran. The control is the same build with `win_main.cpp`
and `common.c` from `8e8de10`, relinked; the two share their renderer.

| Profile | Map | Control, each variant | With this change, each variant | Where it overflowed |
| --- | --- | --- | --- | --- |
| `q3` | `q3dm1` | crashed, `0xC0000005`, after 10-11 s | `faulted`: `STACK_OVERFLOW` reported, client alive | `CM_TraceThroughTree` (`fntech3.exe`) |
| `ql` | `campgrounds` | crashed, `0xC0000005`, after 14-15 s | `faulted`: `STACK_OVERFLOW` reported, client alive | `R_SetParent` (`fnql_glx_x86.dll`) |
| `rtcw-sp` | `escape1` | crashed, `0xC0000005`, after 13 s | `faulted`: `STACK_OVERFLOW` reported, client alive | `R_SetParent` (`fnql_glx_x86.dll`) |
| `rtcw-mp` | `mp_beach` | crashed, `0xC0000005`, after 15 s | `faulted`: `STACK_OVERFLOW` reported, client alive | `R_SetParent` (`fnql_glx_x86.dll`) |

Every control crash left a dump whose fault is `_chkstk+0x27`, the node-tree
record's; `q3`'s `tree-cycle` dump holds `Com_sprintf` and `ExceptionFilter`
just above the fault and 1,322 `CM_TraceThroughTree` return addresses (that
record counted 1,281). With the change each run was over 3.2 to 4.6 s after
launch, two seconds after its report. In Quake Live and both RTCW modes the
client loaded the damaged world before the server's traces met the cycle, and
the renderer's own walk of the tree overflowed first, in another module but on
the same thread; Quake III's server traces met it first. After each drop the
client restarted its renderer.

## What this does not show

- An overflow on another thread still ends the process, now with its own code
  and with the desktop's gamma put back, but with no line in the log. It was
  exercised only in the harness; no engine thread was overflowed.
- The unwind runs whatever cleanup the frames it leaves register, on the
  reserve. The harness's C++ frames ran theirs, but a cleanup that needs more
  than about 33 KB would fault again. In the engine the compiler registers
  less than the harness: rcon's report came back through the redirect, so the
  cleanup that ends it in `SVC_RemoteCommand` did not run in the unwind,
  presumably because `/EHsc` drops the bookkeeping around calls to C
  functions, as it does for every `ERR_DROP`'s `longjmp`.
- A normal `ERR_DROP` shuts down while the frames it leaves are intact; here
  the report's calls overwrite them. The print redirect is the one pointer
  into them that the engine's own error path follows, and it is moved first.
  A native module that kept a pointer into its own abandoned frames and used
  it while shutting down would read what the report left there. None is known.
- Only the Quake III and Quake Live profiles ran the stable-build probe, and
  the damaged trees ran under the GLx renderer only; the mechanism is shared
  by every profile, and the high-water loads ran nine.
- Windows x86 only. The x64 Windows build's `Q_longjmp` is its own routine,
  with no registration list to unwind, and was not built. A MinGW build's is
  `__builtin_longjmp`, which leaves the exception registration list as it
  was, for this path as for every `ERR_DROP` from the filter; it was not
  built either. Linux's fatal-signal handler still runs on the overflowed
  stack, with no alternate signal stack, so an overflow there still ends the
  process.
- The report names the instruction that met the guard page, which for a
  recursion is inside the recursing function; it does not name the caller
  that started the recursion.

## Identity

- Stable build: `meson/build/win32-dev`, every development gate on, built
  05:47-05:48 UTC 2026-09-28 from `8e8de10` and the working tree, which held
  this change and another session's uncommitted botlib index checks
  (`code/botlib/be_aas_file.cpp`, `rtcw_aas_loader.hpp`,
  `aas_index_checks.hpp`). Client `fntech3.exe`
  `2488edd565011399d60f93990e2271679c0a7adc860c1ae84a0f61ab77e6335e`, server
  `fntech3.ded.exe`
  `c2fac4e686f0bfa4a8f81af60e1701627ce4ede7e96e6f904114400ce25f1cc5`,
  `fnql_glx_x86.dll`
  `49a042021f9a51445d3028e092cab8b5dcd0a4851e07eb4fccfd2c066d9f0404`. Both
  stable-build probes hashed them unchanged at their start and end.
- Another session rebuilt the same folder from the same working tree at 05:52
  UTC; the damaged-tree refusal check, the frame tables and the high-water
  loads used that build (client
  `865878308f6c54f40ff7a08d87d764e8a5c6b40715f9bffec5b7f587cfe1daa3`), whose
  threads report the 32 KB guarantee.
- Build without the node-tree check, with this change:
  `.tmp/runtime/stack-overflow-precheck-after`, client
  `4c3395af932bab15a5c8d01ee927b8137e0c5ef10191e02a414798685a3e3ccb`; its
  control, `.tmp/runtime/stack-overflow-precheck-before`, client
  `e9509a145afd54dd9fbfeac870a7b33f3a74cf680bd7c84dd1225a2cf982b96e`; both
  with `fnql_glx_x86.dll`
  `fe3279edc12ff2b6c32f9d6c82b071e6530a2bda566cf202612e327e5f4329a8`.
- MSVC 19.44 x86, debugoptimized, strict warnings; LLVM 20.1.7 tools. Windows
  11 Home 10.0.26200 x64 host.
- Runs, 2026-09-28 UTC: stable-build probes 05:48-05:50, damaged trees with
  the change 05:56-05:57 and the control 05:57-05:59, refusal check about
  06:00, high-water loads 06:02-06:03.
- Installations, read only: `C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena`,
  `E:\SteamLibrary\steamapps\common\Quake Live`,
  `E:\SteamLibrary\steamapps\common\Return to Castle Wolfenstein`,
  `E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory`,
  `E:\Games\Star Trek Voyager Elite Force`, `E:\Games\Alice`,
  `E:\Games\Medal of Honor Allied Assault`; F.A.K.K.2 from the layout under
  `.tmp/fakk2-install/game`.
