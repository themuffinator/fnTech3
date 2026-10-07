# The full CMake Linux i686 build

Date: 2026-10-05. Evidence: Source / Build / Unit. Companion:
[machine-readable record](2026-10-05-cmake-i686-full-build.json).

Linux CI's CMake job (`.github/workflows/linux-verification.yml`, "Build
CMake i686 dedicated server") built the `fntech3.ded` target alone. Built
whole with that job's flags, HEAD `8cf605c7` stopped in four places and one
of its tests aborted; none of this reached CI. Each cause is below with what
was observed, what was changed and what was inferred. All four were first
seen during the large-file work (`2026-10-05-make-large-files.md`), with
and without that change.

## Observed at HEAD

Host: WSL 2, Ubuntu 24.04.3 (Linux 6.6.87.2-microsoft-standard-WSL2), GCC
13.3.0, clang 18.1.3, glibc 2.39, GNU ld 2.42, CMake 3.28.3, Ninja 1.13.0.
The tree was a `git archive` of `8cf605c72d7cf238376baf567f43e247b12e863a`
made by WSL's git (LF endings), with the untracked `subprojects/fontstash`,
`minimp3` and `libjpeg-turbo-3.1.4.1` checkouts copied in. Configured as CI
configures it, plus the 32-bit zlib, which this host has only as a runtime
library:

```
cmake -S <src> -B <build> -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 \
  -DCMAKE_EXE_LINKER_FLAGS=-m32 -DCMAKE_SHARED_LINKER_FLAGS=-m32 \
  -DFNQL_AUDIO_LOOPBACK_TESTS=OFF -DFNQL_GLX_LOGIC_TESTS=OFF \
  -DUSE_CURL=OFF -DUSE_OGG_VORBIS=OFF -DUSE_SDL=OFF -DUSE_SYSTEM_JPEG=OFF \
  -DZLIB_LIBRARY=/usr/lib/i386-linux-gnu/libz.so.1
cmake --build <build> --parallel 20 -- -k 0
ctest --output-on-failure -j8
```

Of 497 build steps, four failed:

| Step | Error |
|---|---|
| `fnql_vk_x86`: `code/renderervk/tr_init.c` | line 3957: `GL_CLAMP_TO_EDGE` undeclared |
| `fnql_rtx_x86`: `code/rendererrtx/tr_init.c` | line 3922: `GL_CLAMP_TO_EDGE` undeclared |
| `fntech3_jka_render_tests`: `tests/jka_render_tests.cpp` | `code/compat/render_record_adapter.hpp: No such file or directory` |
| `fntech3` link | ``i386:x86-64 architecture of input file `libjpeg-turbo/libjpeg.a(jcapimin.c.o)' is incompatible with i386 output``, and so for every member |

So `fntech3`, `fnql_vk_x86.so` and `fnql_rtx_x86.so` were never produced;
`fntech3.ded` and `fnql_glx_x86.so` were 32-bit Intel 80386 ELF, and
`libjpeg.a(jcapimin.c.o)` was `ELF 64-bit LSB relocatable, x86-64`. Of 82
tests, 79 passed, `fnql_retail_factory_probe` skipped (no installation),
`fntech3_jka_render` did not run, and `fntech3_jka_sp_game_bridge` aborted:
`line 385: candidateResult.trace.entityNum==2&&candidateResult.trace.fraction==0.025f`.

## Causes and changes

### The Vulkan and RTX modules' texture capabilities

`RE_GetTextureCapabilities`, added in `8cf605c7`, chose its branch with
`#ifdef USE_VULKAN_API` in both modules. That macro is the client's
window-system choice: Meson's common arguments and the Makefile's
`BASE_CFLAGS` define it for every object whenever a Vulkan or RTX module is
built (and `USE_OPENGL_API` whenever a GLx one is), while CMake defines
both for the `client` and `q3ui` targets only. Under CMake the two modules therefore
compiled the `#else` branch, written for OpenGL, which needs
`GL_CLAMP_TO_EDGE`; only `code/renderer/qgl.h` declares it, and neither
module includes it. No other renderer source tests either macro.

The change tests the module's own `USE_VULKAN`, which each module's
`tr_common.h` defines unconditionally. Giving the renderer targets
`USE_OPENGL_API` under CMake, the other way out, would not have compiled
(the branch tests `USE_VULKAN_API`, and `GL_CLAMP_TO_EDGE` is undeclared in
these modules under either), and a module's capabilities should not depend
on which window APIs the client was built for.

Observed: compiled with `meson/build/win32-dev`'s exact MSVC x86 commands
(`/W4 /WX`), both files build, and the function preprocesses (`/EP`) to the
same text at HEAD and after the change, since Meson defines both macros
there: the Meson, Make and MSVC modules are unchanged. Inference from the
source: had CMake's modules compiled, the OpenGL branch would have reported
no anisotropy and no edge clamp, since neither module ever sets
`maxAnisotropy` or `gl_clamp_mode`. Jedi Academy's client copies those two
values into its module's `glconfig_t` (`code/client/client_jka_glconfig.hpp`).
CMake's modules now report what Meson's do: the Vulkan sampler's
anisotropy and clamp to edge.

### The Jedi Academy render test's include

`tests/jka_render_tests.cpp` includes `code/compat/render_record_adapter.hpp`
from the source root, as the Python fixtures and several other C++ tests do.
Meson's implicit include directories put the source root on every target
(`-I..\..\..` in `win32-dev`'s commands); CMake's Jedi Academy test loop
added `code/compat` only. The loop now adds the source root too, so root
and `../code/...` includes both resolve as under Meson, and the test builds
and passes. Rewriting that one include as `../code/...`, as its fourteen
loop siblings spell theirs, was the alternative; it would have left the next
root-relative test to fail the same way.

### libjpeg-turbo's external build

With `USE_SYSTEM_JPEG=OFF`, CMake builds libjpeg-turbo through
`ExternalProject_Add`, a separate configuration that received the build
type, `CMAKE_C_COMPILER` and, on Apple, the OSX variables, and nothing
else. A Linux i686 build that selects `-m32` through `CMAKE_C_FLAGS`, as CI
does, therefore got a host x86-64 library. The change passes
`CMAKE_C_FLAGS` always, and `CMAKE_C_COMPILER_ARG1`,
`CMAKE_C_COMPILER_TARGET`, `CMAKE_SYSROOT` and `CMAKE_TOOLCHAIN_FILE` when
set. Probes with CMake 3.28.3 on the same host:

- `CC="gcc -m32"` leaves `CMAKE_C_FLAGS` empty and puts ` -m32` in
  `CMAKE_C_COMPILER_ARG1`; a configuration given
  `-DCMAKE_C_COMPILER_ARG1= -m32`, or `-DCMAKE_C_FLAGS= -m32`, with
  `-DCMAKE_C_COMPILER=/usr/bin/gcc`, detects a 4-byte pointer, as an
  external project given those arguments does.
- A relative `-DCMAKE_TOOLCHAIN_FILE` is held as an absolute path after
  `project()`, so passing it to a configuration in another directory is safe.

After the change `libjpeg.a(jcapimin.c.o)` is `ELF 32-bit LSB relocatable,
Intel 80386` and the client links. A build directory that still holds the
64-bit library recovers on its next build, without a clean: rebuilding
HEAD's failed directory with the change re-ran CMake, ran "Performing
configure step for 'fnql_libjpeg_turbo'", produced the 32-bit library and
linked `fntech3`.

On Windows (CMake 4.2.0-rc2 with Ninja under the MSVC 14.44 x86 developer
shell, configured from exports of HEAD and of HEAD with this change,
building only `fnql_libjpeg_turbo`), `jpeg-static.lib` is `14C machine
(x86)` in both. The library's configuration received `/DWIN32 /D_WINDOWS`
at HEAD (its own default) and `/DWIN32 /D_WINDOWS /W3` with the change,
this project's default, which its CMake 3.12 policies keep `/W3` in: a
warning level, no code change.

### The Jedi Academy SP bridge test's literal

An instrumented copy of the test printed, at line 385, `entityNum=2` and
`fraction=0x1.99999ap-6` (0.0250000004): the float `0.1f * 0.25f`, which is
the float nearest 0.025, exactly what the merge is meant to produce
(`best.fraction*=worldFraction`, multiplying by a power of two). The
comparison failed because the literal did not hold that float: cast to
double, `0.025f` read `0x1.999999999999ap-6`. The same copy built with
`-msse2 -mfpmath=sse`, or with clang 18 at `-m32`, passed; with
`-ffloat-store`, or at `-O2`, it failed.

A standalone probe compared a stored `0.1f * 0.25f` with spellings of the
literal:

| Compiler and flags | `FLT_EVAL_METHOD` | `== 0.025f` | `== (float)0.025f`, `float(...)`, `static_cast`, a named `float` |
|---|---|---|---|
| `g++ -m32 -std=c++17` (also `-O2`) | 2 | false | true |
| `g++ -m32 -std=gnu++17` | 2 | true | true |
| `g++ -m32 -std=c++17 -msse2 -mfpmath=sse` | 0 | true | true |
| `g++ -std=c++17` (x86-64) | 0 | true | true |
| `clang++ -m32 -std=c++17` | 2 | true | true |

Inference: GCC 13 implemented `-fexcess-precision=standard` for C++ and
selects it in strict ISO modes, where a floating constant takes the range
and precision of `long double` on x87. Meson (`cpp_std=c++17`), the
Makefile (`-std=c++17`) and CMake (`CMAKE_CXX_EXTENSIONS OFF`) all compile
C++ in that mode, so this is GCC i686 behaviour, not CMake's. MSVC x86 uses
SSE2 (`FLT_EVAL_METHOD` 0). The test now compares with `float(0.025f)` at
both sites (lines 385 and 415 at HEAD), the cast that
`code/renderercommon/tr_global_fog.h` already documents for `(float)0.1f`.
No engine code changes: the trace merge was right. Under MSVC x86 with
`win32-dev`'s flags the test passes.

## Regression tests

`tests/platform_port_safety_tests.py` gains three tests. Each fails on the
HEAD tree and passes with the change:

- `test_renderer_modules_select_their_backend_by_their_own_macros`: no
  source under `code/renderer`, `renderercommon`, `rendererglx`,
  `renderervk` or `rendererrtx` mentions `USE_OPENGL_API` or
  `USE_VULKAN_API`, and the Vulkan and RTX capabilities test `USE_VULKAN`.
- `test_cmake_libjpeg_turbo_build_inherits_the_target_selection`: the
  external configuration receives `CMAKE_C_FLAGS` and the four optional
  inputs.
- `test_cmake_jka_tests_find_source_root_includes`: the Jedi Academy test
  loop adds the source root, and every root-relative include its tests make
  names an existing file.

## Verification

The candidate tree is the same export with this change's hunks alone,
rebuilt from HEAD's text (hashes in the companion); no other uncommitted
work was built.

| | HEAD | HEAD with the change |
|---|---|---|
| Failed build steps (of 497) | 4 | 0 |
| `fntech3`, `fntech3.ded`, `fnql_glx_x86.so`, `fnql_vk_x86.so`, `fnql_rtx_x86.so` | 2 built | all 5, each 32-bit Intel 80386 ELF |
| `libjpeg.a(jcapimin.c.o)` | x86-64 | Intel 80386 |
| ctest (82 tests) | 79 pass, 1 skip, 2 fail | 81 pass, 1 skip |
| Unique GCC warnings | 14 | the same 14 |

Python source tests: `platform_port_safety_tests.py` and
`x86_build_constraints_tests.py` pass on Linux in the candidate tree (34
tests, the Windows-only MSBuild probe skipped) and on Windows with Python
3.11 in the shared working tree (35 passed, including another session's
uncommitted test). One Linux run of the second file timed out in its
60-second CMake configure probe while the machine was loaded (load average
above 10); it passed when repeated, and that probe stops at the x86 guard,
before anything this change touches.

The large-file change's CMake hunk, uncommitted at the time, composes with
this one: HEAD with both builds every target, passes the same 81 tests, and
puts `-D_FILE_OFFSET_BITS=64` on all 434 of its own compile commands.

HEAD moved to `9f85e4ab` (legacy-server hosting, which touches none of
these files) during the work. Rebuilt from it with the same hunks, the tree
again builds every target, passes the same 81 tests, and passes this
record's documentation and release tests on Windows (`release_packaging`,
`release_workflow`, `version_metadata`, `changelog`, `steam_identity_source`,
`generate_docs` and the two files above: 138 passed, with the symlink case
and the accepted-x86 Make path, which needs the FontStash checkout, skipped).

## Linux CI

Added 2026-10-06. The workflow's CMake step, now "Build every CMake i686
target", keeps its configuration and builds every target, checks that
`fntech3`, `fntech3.ded` and the three renderer modules are 32-bit Intel
80386 ELF, and a new step, "Run CMake i686 regression suite", runs
`ctest --test-dir .tmp/cmake-linux-server --output-on-failure
--no-tests=error --parallel 2`. Both jobs, Meson and Make, now install
`zlib1g-dev:i386`; the Make job adds the i386 architecture to do so.

Observed in CI: the workflow's last run that started,
[37173959426](https://github.com/themuffinator/fnTech3/actions/runs/37173959426)
(2026-10-04, `8cf605c7`), stopped both jobs on the same missing package.
The Meson job's dependency-isolated server (`--wrap-mode=nofallback`) found
no zlib, `Run-time dependency zlib found: NO (tried pkgconfig, cmake and
system)`, so no later step ran, the CMake step included; the Make job's
link said `skipping incompatible /usr/lib/x86_64-linux-gnu/libz.so when
searching for -lz` and `cannot find -lz`. Its apt log installed
`zlib1g:i386`, the runtime library, and no i386 zlib headers or `libz.so`.
The 59 earlier runs listed, back to 2026-09-08, all failed within 72
seconds; the four inspected (34191357278, 34192571734, 35312413213 and
36774610896) had started no step, refused for the account's billing.

Observed in WSL (the host above), with CI's configuration and no zlib hint
(`--debug-find-pkg=ZLIB`): CMake sets `CMAKE_LIBRARY_ARCHITECTURE` to
`i386-linux-gnu`, and FindZLIB searches `PATH`-derived directories, then
`/usr/local/lib/i386-linux-gnu`, `/usr/local/lib`,
`/usr/lib/i386-linux-gnu`, `/usr/lib`, `/lib/i386-linux-gnu`, `/lib` and
a few others, never `/usr/lib/x86_64-linux-gnu`. This host has
`zlib1g-dev:amd64` and `zlib1g:i386` alone, so the configuration stopped at
`Could NOT find ZLIB (missing: ZLIB_LIBRARY)` rather than taking the 64-bit
`libz.so`. `zlib1g-dev:i386` puts `libz.so` in `/usr/lib/i386-linux-gnu`
and its `zlib.pc` in the cross file's `pkg_config_libdir`. Inference: on the
runner, the CMake step without that package stops at configuration, as the
Meson step did, and with it FindZLIB takes the i386 library; a 64-bit one
found instead would stop the link, not pass silently. `gcc -m32` searches
`/usr/lib/i386-linux-gnu` for `-lz` as well (`-print-search-dirs`), so the
same package gives the Make job its library.

The two CMake steps' `run` text was extracted from the edited workflow with
PyYAML and run verbatim with `bash -e`, as GitHub runs a step with no
`shell`, from the root of a `git archive` of the shared working tree on
2026-10-06 (tree `665d38a3`, every tracked and untracked file over HEAD
`9f85e4ab`, so other sessions' uncommitted work too), plus the
ignored fontstash, imgui, libjpeg-turbo and minimp3 checkouts. A prefix in
`CMAKE_PREFIX_PATH` held a `libz.so` link to the i386 runtime library in
place of the package; the configuration took it
(`ZLIB_LIBRARY_RELEASE=<prefix>/lib/libz.so`). All 503 build steps passed
at `--parallel 2` in 152 seconds, the five binaries and
`libjpeg.a(jcapimin.c.o)` were 32-bit Intel 80386, and ctest ran 82
tests: 81 passed and `fnql_retail_factory_probe` skipped.

The Make job's build step was run the same way in the same export, twice.
With the runner's packages it stopped as CI did: `cannot find -lz`, after
skipping the x86-64 `libz.so` and `libz.a`. With a `LIBRARY_PATH` prefix
holding the same `libz.so` link in place of the package, it linked and its
own checks passed: `fntech3.ded` is a 32-bit Intel 80386 PIE executable.

On Windows, `platform_port_safety_tests.py`, `release_workflow_tests.py`
and `ci_regression_workflow_tests.py` pass with their workflow pins updated
(51 tests), and the updated pins fail against HEAD's workflow, the zlib
pins in both jobs included.

The CMake steps run after the Meson suite in the same job, so they report
only once that suite passes; it was not run here. The CI change depends on
this record's fixes: HEAD's tree would fail the new build step as above.

## Not covered

- Nothing built here ran; the client and the three modules were linked, not
  started.
- Forwarding of `CMAKE_C_COMPILER_TARGET`, `CMAKE_SYSROOT` and
  `CMAKE_TOOLCHAIN_FILE` to libjpeg-turbo, and the Apple and MinGW paths,
  were not exercised; `CMAKE_C_COMPILER_ARG1` only by the probe above.
- No full Windows CMake build was attempted; only its libjpeg-turbo target.
- The Linux CI change has not run in CI. The runner's CMake version and
  processor count were not reproduced, and `zlib1g-dev:i386` was not
  installed here.

## Unrelated observations

- The same literal precision stops four Meson-built tests on GCC i686.
  With `g++ -m32 -std=c++17 -O2` and the sources and include directories
  Meson gives them, `tests/alice_menu_tests.cpp` (line 306,
  `DefaultScrollSpeed == 0.2f`) and `tests/fakk2_hud_tests.cpp` (line 42,
  `FlashRate == 0.00133333332f`) fail to compile on their `static_assert`,
  and `tests/alice_hud_tests.cpp` (line 52) and `tests/alice_screen_tests.cpp`
  (line 171) exit 1; `alice_menu_tests.cpp` lines 54 and 379 and
  `alice_screen_tests.cpp` line 177 compare the same way. All four pass with
  `-msse2 -mfpmath=sse`. Meson builds and runs them unconditionally, and
  Linux CI's Meson job runs `meson test` with ubuntu-24.04's GCC, so it
  should stop on them too (inference; CI was not run). A scan finds no `==`
  or `!=` against an inexact float literal under `code/`; ordering
  comparisons were not examined.
- GCC 13 warns, without failing, at `code/compat/jka_state_adapter.hpp:193`
  (`-Wmissing-template-keyword`) and `code/compat/fakk2_convert.hpp:33`
  (`-Wenum-compare`).
