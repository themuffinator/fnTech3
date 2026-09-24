# Client slots are cleared in place, so a running server can change sv_maxclients

## Scope

Changing `sv_maxclients` on a running fnTech3 server crashed it with
`STACK_OVERFLOW` during the next level spawn. The defect is in server code
that every profile, and both `fntech3.exe` and `fntech3.ded.exe`, share, and
no profile contract is involved: as in Quake III's GPL server,
`SV_ChangeMaxClients` allocates a client array of the new size and copies the
connected clients into it. Runtime evidence here is for the `q3`, `rtcw-mp`
and `wolfet` dedicated servers.

## Observed

### The crash

The shared developer build `meson/build/win32-dev/fntech3.ded.exe`, SHA-256
`d2c23bd15f0fe2c2bf09b340dfa978e52703a5b2a4502a78cdb2c3b889218d11` (linked
at 07:39 local from the shared tree, then `75561b9` plus uncommitted work),
ran this on a private desktop with an isolated `fs_homepath`, loopback only:

```text
fntech3.ded.exe +set com_gameProfile <profile> +set fs_basepath <installation>
  +set fs_homepath <home> +set dedicated 1 +set net_ip 127.0.0.1 +set logfile 2
  +map <map> +wait 50 +sv_maxclients 12 +wait 5 +map <map>
  +wait 50 +sv_maxclients +serverinfo +quit
```

The last line lets a surviving server print the new value and quit by itself.
For `q3` on `q3dm1`, `rtcw-mp` on `mp_beach` and `wolfet` on `goldrush`, with
no client connected, the log reads `sv_maxclients will be changed upon
restarting.`, then `------ Server Initialization ------`, then
`ERROR: Unhandled exception caught`, `Exception Code: STACK_OVERFLOW`,
`Exception Address: fntech3.ded.exe@1ffbe7`. `llvm-symbolizer` with the
build's PDB places that address in `_chkstk` (`chkstk.asm:99`). The server
then waits in its error console. The installation was unchanged after every
run.

### Frames

Both Windows executables reserve 1,048,576 bytes for the main thread's stack
(`SizeOfStackReserve`). The PDBs' `S_FRAMEPROC` records give these frames, in
bytes:

| Build | `SV_AllocClients` | `SV_ChangeMaxClients` | `SV_DirectConnect` |
| --- | --- | --- | --- |
| `d2c23bd1`, the crashing build | 627,440 | 627,452 | 629,316 |
| `2d35919` exported, dedicated only (`bd72ab96`) | 527,320 | 527,332 | 529,196 |
| The same export with this change (`4ff08f0a`) | 0 | 12 | 5,988 |

`client_t` is 527,320 bytes at `2d35919` on x86: 256 reliable commands of
1,024 bytes (262,144), 32 snapshot frames (188,288) and the netchan (65,624).
Another session's uncommitted Alice fields raise it to 627,440 in the shared
tree. Each of the large frames above is one whole `client_t` temporary.
`client = {};`, `oldClients[slot.index] = {};` and `*newcl = {};` build the
`{}` value in the caller's frame and then copy it, and MSVC does so even at
`/O2`. The copies of connected clients, `oldClients[slot.index] = slot.client`
and back, need no temporary: `SV_ChangeMaxClients` keeps both and now has a
12-byte frame.

`SV_SpawnServer` calls `SV_ChangeMaxClients` when `sv_maxclients` has changed,
and it calls `SV_AllocClients`. At `2d35919` those two frames alone take
1,054,652 bytes, more than the whole reserve before any caller's frame, so
that commit overflows too without the uncommitted fields. `SV_Startup` calls
`SV_AllocClients` by itself, so the first level loads.

With `win32-dev`'s compile command and `/Od` in place of `/O2`, objects only,
`SV_DirectConnect` keeps both of its temporaries: 1,060,872 bytes for
`2d35919`'s sources and 1,261,112 for the shared tree's. An unoptimized
build, such as the `meson: build Win32 debug (Steam)` task's, would therefore
overflow on every client connection. That is inferred from the frames. No
such build was run.

### client_t can be zeroed as bytes

Compiled with MSVC x86 against the shared tree's headers, `client_t` is
trivially copyable, trivially default constructible, trivial and standard
layout. A default member initializer anywhere in it, or in any member's type,
would make its default constructor non-trivial, so it has none. Its members
are integers, enumerations, floats, byte-array unions, arrays, plain
structures and data pointers, with no pointer to member. Zero bytes therefore
give every member the value `{}` gives it. Filling the bytes also zeroes
padding and the rest of each union, which `{}` does not promise to.

### Reference baselines

FnQuake3 at its pinned `7d8c962` already clears slots in place with
`Com_Memset` in all three functions. FnQL at its pinned `fce44fe` has the
`{}` assignments that fnTech3 inherited. This is a reference observation; no
FnQuake3 text was used.

## Change

- `SV_ClearClient` (`server.h`) zeroes one client slot with `std::memset`,
  after a `static_assert` that `client_t` is trivial. A member that gains a
  default member initializer, or a type that stops being trivial, stops the
  build there rather than silently changing what a cleared slot holds.
- `SV_AllocClients` and `SV_ChangeMaxClients` (`sv_init.cpp`) clear slots
  with it. `SV_DirectConnect` (`sv_client.cpp`) clears the new slot with it,
  and clears it again when the game refuses the connection.
- A cleared slot holds the same values as before, and connected clients are
  copied as before. Nothing else changes.

## Evidence

- Runtime, Windows x86. The same command against
  `meson/build/win32-dev/fntech3.ded.exe` rebuilt in place with this change,
  SHA-256 `fb3d555eac0be29ef88df0b5f1562b02b53b5b73847060c5d76b1259215f0264`
  (linked at 08:33 local from the shared tree, then `2d35919` plus
  uncommitted work; the SHA-256 of `git diff HEAD --binary` at build time is
  in the evidence file). Its PDB gives the fixed frames above.

  | Profile, map | `d2c23bd1` | `fb3d555e` |
  | --- | --- | --- |
  | `q3`, `q3dm1` | Crash in `_chkstk` during the second spawn | Two spawns; `sv_maxclients` prints 12 and the serverinfo carries 12; `quit`, exit 0 |
  | `rtcw-mp`, `mp_beach` | The same crash | As `q3` |
  | `wolfet`, `goldrush` | The same crash | As `q3` |

- Runtime with a retail client connected. `scripts/probe_restart_variable_change.py`
  (another session's, uncommitted when this was written; its record is
  `2026-09-24-wolfenstein-restart-variable-change.md`) runs a server and one
  windowed retail client, each on its own private desktop with no input, and
  steps the server over loopback `rcon`. Its `restarts` plan ends with
  `sv_maxclients 12` and `map_restart`. Against `fb3d555e`, with `WolfMP.exe`
  on `mp_beach` and with `et.exe` on `goldrush`, that step printed
  `sv_maxclients variable change -- restarting.`, spawned the level once and
  sent the client one gamestate, which it loaded. The client stayed connected,
  saw a marker said after the restart, and `sv_maxclients` read 12. Both runs
  passed, with the installations unchanged. That session's own baseline and
  candidate servers had crashed at the same step.
- `tests/server_client_stack_tests.py` compiles `sv_init.cpp` and
  `sv_client.cpp` at `-O0` with `-Wframe-larger-than=262144` and fails on any
  frame that large, after a one-line canary shows the compiler keeps a
  `client = {}` temporary. It fails both files at `2d35919` (x86_64 Q3-only:
  658,840, 659,032 and 1,324,024 bytes) and passes with this change, with
  clang++ for i686 in the x86 developer shell and for x86_64 with
  `-DFNTECH3_Q3_ONLY`.
- A clean `git archive` export of `2d35919` with this change builds all
  1,200 targets with strict warnings. Its `fntech3.exe` PDB has the fixed
  frames and no frame of 256 KiB or more. Its meson C++ tests pass (116 ok,
  2 skipped with exit status 77, none failed), as does the whole pytest suite
  (1,832 passed, 77 skipped for runtime opt-ins and tools that export did not
  build, 37,810 subtests), and `scripts/release_workflow.py check` agrees. The
  commands, identities and per-run results are in the
  [evidence file](evidence/2026-09-24-client-slot-stack-overflow.json).

## Not established

- Quake Live, RTCW single player, Quake III IHV, Elite Force, both Jedi
  Knight games and Alice were not run. The three functions are the same for
  every profile, and the frames do not depend on the profile.
- No listen server ran. `fntech3.exe` compiles the same functions, and its
  frames were read from the PDB only.
- No unoptimized build was built or run; its connect-time overflow is
  inferred from the `/Od` frames above.
- The exported builds were measured, not run. The runtime comparison used the
  shared developer build before and after, whose tree also carried other
  sessions' uncommitted work.
- No Quake III client was connected across the change; the probe covers only
  RTCW multiplayer and Enemy Territory.
