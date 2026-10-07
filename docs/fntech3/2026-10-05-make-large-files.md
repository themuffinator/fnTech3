# Large-file support in the Make and CMake builds

Date: 2026-10-05. Evidence: Source / Build / Unit / Live (Linux i686
dedicated server). Companion:
[machine-readable record](2026-10-05-make-large-files.json).

The legacy Makefile builds only 32-bit x86, and on Linux and the other Unix
platforms it never defined `_FILE_OFFSET_BITS=64`; CMake did not either.
Meson adds `-D_FILE_OFFSET_BITS=64` to every compile outside MSVC and macOS
(Meson 1.10.2, configuring this tree's i686 dedicated server with Linux CI's
options, put it on all 338 compile commands), so only Make and CMake builds
were affected. The Linux release
archive is one of them: the release workflow's `ubuntu-x86` job builds it
with this Makefile, and Linux CI builds Make and CMake dedicated servers.

## The defect

Without the define, 32-bit glibc gives `readdir()` and `stat()` a 32-bit
`ino_t` and `off_t`, and they fail with `EOVERFLOW` (errno 75) for an inode
number past 32 bits or, in `stat()`, a file of 2 GiB or more. WSL 2's drvfs
mount (9p) reports 64-bit inode numbers, as XFS, btrfs and NFS can.

Observed: one i686 probe source, built both ways with GCC 13.3 against glibc
2.39, listed `baseq3` of the Steam Quake III installation through `/mnt/c`
the way `Sys_ListExtFiles` does (`opendir`, then `readdir` and `stat` per
entry). Without the define the first `readdir()` returned NULL with
`EOVERFLOW` and nothing was listed. With it all 29 entries were listed and
stat'ed; their inode numbers run from 562,949,953,521,985 to
158,751,886,864,819,321, every one past 2^32 (`pak0.pk3`:
1,125,899,906,943,293). `off_t`, `ino_t`, `struct stat` and `struct dirent`
went from 4, 4, 88 and 268 bytes to 8, 8, 96 and 276.

`Sys_ListExtFiles` (`code/unix/unix_shared.cpp`) ends its listing at the
first failed `readdir()` and skips an entry whose `stat()` fails, so a
dedicated server built from HEAD found no paks there (below). A 32-bit
`stat()` of `pak0.pk3` also returns `EOVERFLOW`, as observed earlier the
same day; on this mount the listing stops before any `stat()` runs.

## The change

- `Makefile`: `BASE_CFLAGS += -D_FILE_OFFSET_BITS=64` in the *NIX branch
  (Linux and the BSDs). `BASE_CFLAGS` feeds the release and debug C and C++
  flags, so the define reaches every object: client, dedicated server,
  botlib, QVM, Medal of Honor and all three renderer modules.
- `CMakeLists.txt`: `ADD_COMPILE_DEFINITIONS(_FILE_OFFSET_BITS=64)` under
  `IF(UNIX AND NOT APPLE)`, before the first target.
- Neither applies to MinGW or macOS. macOS's `off_t` and `ino_t` are 64-bit
  already. The Windows platform code lists files with `_wfindfirst64` and
  reads sizes with `_wstat`, which the define does not change; Meson's
  MinGW builds do get it, where it widens only the engine-internal
  `fileOffset_t`.
- `tests/platform_port_safety_tests.py` pins both, and has GNU Make
  evaluate the Makefile's Linux, MinGW and macOS branches to check that the
  define reaches the release and debug C and C++ flags on Linux alone.

## Module and file contracts

On 32-bit glibc the define widens `off_t`, `ino_t`, `blkcnt_t`,
`fsblkcnt_t`, `fsfilcnt_t`, `rlim_t` and `fpos_t`, and the structures that
hold them (`struct stat`, `struct dirent`, `struct statfs` and `statvfs`,
`struct rlimit`, `struct flock`). A search of `code/` finds:

- `struct stat` and `struct dirent` only as locals, in `unix_shared.cpp`
  and `code/platform/process_relaunch.cpp`;
- no `fseeko`, `ftello`, `fgetpos`, `fsetpos`, `lseek`, `rlimit`,
  `statvfs`, `flock` or `glob` use, and none of glibc's internal
  `__off_t`-style names;
- one engine type over `off_t`, `fileOffset_t` (`code/qcommon/qcommon.h`),
  used by `pack_t` and the pk3 cache file in `files.c`, `sv_filter.cpp`'s
  ban-file statistics, a local existence check in `server_alice_game.hpp`,
  and `Sys_GetFileStats`. None of them is part of a module or renderer
  interface, whose tables carry integers, floats and pointers.

Inference: no game, cgame, UI, botlib or renderer interface, wire format,
demo or save changes. The layout tests check the measured part: built by
CMake as i686 with the define, `fntech3_game_abi_layouts`,
`fntech3_game_abi_adapters`, `fntech3_cgame_usercmd_abi`,
`fntech3_ui_module_abi`, `fntech3_jka_sp_abi` and
`fntech3_jka_sp_player_abi` pass, as they do without it.

The one persistent format the define reaches is the engine's own
`pk3cache.dat` in `fs_homepath`, whose fourth header byte packs
`sizeof(fileOffset_t)` and `sizeof(fileTime_t)`: `0x33` from a Make build
before this change, `0x73` after it and from Meson. A cache with the other
byte is ignored and rewritten (observed below). `time_t` stays 32-bit on
i686: the change does not define `_TIME_BITS`.

## Verification

Host: WSL 2, Ubuntu 24.04 (Linux 6.6.87.2-microsoft-standard-WSL2), GCC
13.3.0, glibc 2.39, GNU Make 4.3, CMake 3.28.3, Ninja 1.13.0. The control
tree is a `git archive` of HEAD `8cf605c7`; the candidate is the same
export with this change's two hunks, and nothing else, in `Makefile` and
`CMakeLists.txt` (hashes in the companion). No other uncommitted work was
built.

Compile commands carrying the define:

| Configuration | Control | Candidate |
|---|---|---|
| `make -n release BUILD_CLIENT=0 BUILD_SERVER=1 USE_CURL=0 ARCH=x86` | 0 of 118 | 118 of 118 |
| `make -n release` with the release job's `ARCH=x86 COMPILE_ARCH=x86 USE_SDL=0 USE_RENDERER_DLOPEN=1 USE_GLX=1 USE_VK=1 USE_RTX=1` | 0 of 397 | 397 of 397 |
| CMake configure with CI's flags, every target (`compile_commands.json`) | 0 of 434 | 434 of 434 |

The release job's 397 are 142 client, 83 dedicated, 6 QVM, 32 Medal of
Honor and 52, 41 and 41 GLx, Vulkan and RTX renderer objects.

Builds: `make release V=1 BUILD_CLIENT=0 BUILD_SERVER=1 USE_CURL=0 ARCH=x86
ZLIB_LIBS=/usr/lib/i386-linux-gnu/libz.so.1` built both trees with no
errors and the same 36 warnings. The candidate's dynamic imports differ
from the control's in six names: `fcntl`, `fopen`, `mmap`, `open`,
`readdir` and `stat` became `fcntl64`, `fopen64`, `mmap64`, `open64`,
`readdir64` and `stat64`, at GLIBC_2.28, 2.1, 2.1, 2.1, 2.2 and 2.33; the
newest, 2.33, is the version the control's `stat` already needed. CMake
built the candidate's `fntech3.ded` with CI's Debug flags.

Runs: each server had its own `fs_homepath` holding an empty
`q3config_server.cfg` and ran against the Steam Quake III installation
through `/mnt/c` with `+set dedicated 1 +set net_enabled 0 +map q3dm17
+quit`:

| Server | Result |
|---|---|
| Make, control | `0 files in 0 pk3 files`, `Legacy Quake III Point Release files are missing.`, `Sys_Error`, exit 1 |
| Make, candidate | `4984 files in 15 pk3 files` (all 15 `.pk3` files in `baseq3`), `q3dm17` with the retail `qagame.qvm` and `maps/q3dm17.aas`, `Server quit`, exit 0 |
| CMake, candidate | the same, exit 0 |

The installation's inventory (path, size and time of every file) had the
same SHA-256 before and after the runs.

pk3 cache: the candidate started once with the `pk3cache.dat` it had
written (header `00 00 2f 73`) and once with that file's fourth byte set to
`0x33`. The first reported `...found 15 cached paks`. The second ignored the
file, reported `synced FS cache on startup`, rewrote the byte as `0x73` and
loaded `q3dm17`. Both exited 0.

Tests:

- CMake's suite, run in both trees: 80 of 82 pass, with identical per-test
  results; the same two fail in both (see below).
- The new test passes on Windows with MSYS2's GNU Make 4.4.1 and on Linux
  with Make 4.3 in the candidate tree, and fails in the control tree.
  `platform_port_safety_tests.py`, `x86_build_constraints_tests.py` and
  `macos_support_source_tests.py` pass on Windows: 42 passed, and the
  existing skip of the accepted x86 Make path, which needs a MinGW cross
  compiler.
- On an export of HEAD with only this change's files, those three files with
  `changelog_tests.py` and `release_workflow_tests.py` pass (69 passed, the
  same skip), and so does `release_packaging_tests.py`, which validates the
  evidence links (48 passed; its symlink case skipped without the
  privilege).

## Not covered

- No client ran; the client's and renderers' flags rest on the dry run.
- No BSD, macOS or MinGW build ran.
- The release container (Ubuntu 20.04, glibc 2.31) did not run. Inference:
  a build against glibc 2.31 cannot reference a symbol version newer than
  2.31, so the release job's `--max-glibc 2.31` check cannot fail on these
  names; there `stat64` resolves to `__xstat64@GLIBC_2.2`.
- Files of 2 GiB or more: `stat()` now succeeds on them, but the engine
  still takes file lengths from `long` `ftell()` (`FS_FileLength`), which
  fails past 2 GiB on i686, as in Meson and Windows builds.

## Unrelated observations

At HEAD `8cf605c7`, identically in both trees, CMake's full Linux i686 build
stops in four places and two of its tests fail:

- `code/renderervk/tr_init.c:3957` and `code/rendererrtx/tr_init.c:3922`:
  `GL_CLAMP_TO_EDGE` is undeclared. CMake gives `USE_OPENGL_API` to the
  client and `q3ui` targets but not to the Vulkan and RTX renderer modules.
- `tests/jka_render_tests.cpp` includes
  `code/compat/render_record_adapter.hpp` from the source root, but CMake's
  JKA test loop adds only `code/compat` to the include path, so
  `fntech3_jka_render` does not build and is not run.
- The client link: CMake's `USE_SYSTEM_JPEG=OFF` libjpeg-turbo build came
  out x86-64 though `CMAKE_C_FLAGS` held `-m32`, and `ld` refuses it for an
  i386 output.
- `fntech3_jka_sp_game_bridge` aborts at its line 385,
  `candidateResult.trace.entityNum==2&&candidateResult.trace.fraction==0.025f`.

Linux CI's CMake job builds only `fntech3.ded`, so it shows none of these.
