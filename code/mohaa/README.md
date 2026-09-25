# code/mohaa — Medal of Honor: Allied Assault model runtime

This directory carries the model-definition (TIKI), skeletal-model (skeletor)
and alias-list code that Medal of Honor: Allied Assault's retail game and
client game modules call into through the engine. It is taken from
[OpenMoHAA](https://github.com/openmoh/openmohaa) at commit
`ab43a0def6c4feccedae36fdb58c2e007d7e4c35` (2026-03-06), which is distributed
under the GNU General Public License, version 2 or (at your option) any later
version. Every file taken from it keeps its original licence header and
upstream name; fnTech3 is GPL-2.0-or-later as well.

This is incorporated implementation text, not reference observation. It is
credited in `CREDITS.md` and `THIRD_PARTY_NOTICES.md`.

## What came from OpenMoHAA

| Here | Upstream |
| --- | --- |
| `tiki/*` | `code/tiki/*` |
| `skeletor/*` | `code/skeletor/*` |
| `corepp/tiki.h`, `tiki_main.*`, `tiki_script.*` | `code/corepp/` |
| `corepp/con_set.*`, `mem_blockalloc.*`, `mem_tempalloc.*`, `Linklist.h`, `str.*`, `container.h` | `code/corepp/` |
| `qcommon/alias.c`, `qcommon/alias.h` | `code/qcommon/` |
| `qcommon/mohaa_q_shared.c` | functions from `code/qcommon/q_math.c` and `q_shared.c` |

## What fnTech3 changed

The upstream sources were adapted mechanically to build inside fnTech3; no
behaviour was intentionally changed except where noted.

- `MAX_QPATH` is spelled `MOHAA_MAX_QPATH` (256): Medal of Honor's paths are
  longer than Quake III's, and fnTech3's own `MAX_QPATH` stays 64.
- fnTech3's `qboolean` is an enumeration, so `true` and `false` in the TIKI,
  skeletor and TIKI-script sources are `qtrue` and `qfalse`, and a handful of
  boolean expressions returned as `qboolean` are cast.
- `alias.c`'s `CHAN_` names are `MOHAA_CHAN_`, because Medal of Honor numbers
  its sound channels differently from Quake III.
- `tiki.h` uses fnTech3's unnamed `msg_t`; `con_set.cpp` drops the hash
  specialisations for OpenMoHAA's vector types, which nothing here uses.
- The TIKI sources no longer include OpenMoHAA's client and server headers.
- `tiki_parse.cpp` matches `$mapspec` and `includes` sections against the
  running map's name (`TIKI_MapName`), and no longer consults the
  Spearhead/Breakthrough server-type cvars, which 1.00 does not have.
- `SkelMat4::copy` copies its twelve floats instead of calling `MatrixCopy`,
  which copies sixteen (an upstream overrun).
- `SaveLODFile` writes with `FS_FOpenFileWrite`.
- `alias.h`'s record keeps 1.00's layout, which both 1.00 modules read: the
  subtitle straight after `streamed`, at +0xd4, and upstream's later
  `forcesubtitle` flag after it rather than before.
- `alias.c` does not add an alias whose name its 40-byte record cannot hold
  with a terminator, which no lookup could find anyway, and does not look one
  up: upstream's `Alias_ListFindNode` copied such a name into a 40-byte
  buffer on the stack, past its end. A real name past 127 characters and a
  list's name past 39 are cut and terminated, where upstream left them
  unterminated, and names fold case as unsigned characters. 1.00's alias
  names run to 34 characters and its files to 59; a model's list is named
  after its path, and 286 of 1.00's 1,378 model paths are longer than 39,
  which only a dump of the list prints.

## What fnTech3 added

- `qcommon/q_shared.h`, `qcommon/qcommon.h` — the definitions these sources
  expect from their own engine headers, over fnTech3's. Only this directory's
  translation units see them first on the include path. Outside Windows they
  map the Microsoft runtime's `stricmp` and `strnicmp` to POSIX's
  `strcasecmp` and `strncasecmp`, which fold case the same way.
- Every build system compiles these sources without strict aliasing:
  `SkelQuat.h`'s `Slerp` type-puns a float, as upstream does.
- `mohaa_engine.cpp` — the engine services the sources call: their memory
  (outside the zone, which is sized for Quake III), file reads, the map name,
  Ritual's path canonicaliser, and the resource reports the loading screen's
  bar counts (`UI_LoadResource`, handed to `MOHAA_LoadResource`).
- `mohaa_tiki.h`, `mohaa_tiki_api.cpp` — the narrow C view of the runtime the
  rest of the engine uses. The implementation file includes both the facade
  and the runtime's headers, so a declaration that drifts does not compile.

Retail evidence outranks OpenMoHAA wherever the two disagree; see
`docs/fntech3/2026-09-24-mohaa-contracts.md`.
