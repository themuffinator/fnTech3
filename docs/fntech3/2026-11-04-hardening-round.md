# Hardening round: clean builds, fuzzed readers and review fixes

A round of checks over the whole tree at `bd209ffa`, with the defects they
found fixed. Nothing here changes a game's contract: every fix keeps the bytes
a module, peer or file sees, and no compatibility gate opens or closes.

## Baseline at `bd209ffa`

Observed on 2026-11-04 from a clean `git archive` export with its own build
directory, configured as `meson/build/win32-dev` is (MSVC x86,
`debugoptimized`, strict warnings, all three renderers, every profile):

- All 1,550 build steps built. Of the 284 native tests, 282 passed and two
  skipped as they do without their inputs (`fnql_audio_loopback`,
  `fnql_retail_factory_probe`, exit 77).
- All 645 Python test files passed, one pytest process per file, four at a
  time (1,014 s).
- Linux i686: `linux-verification.yml`'s two CMake steps, run verbatim in WSL
  Ubuntu (GCC 13, `-m32`) on the same tree, built all 518 steps and ctest
  passed 84 of 84 (one skipped). GitHub's runners are still refused for the
  account's billing, so this is the only Linux evidence.

## Fuzzing the readers that take untrusted bytes

Each reader below was built for i686 Linux with clang 18's libFuzzer,
AddressSanitizer and UndefinedBehaviorSanitizer, from the export's sources,
and fed inputs as the engine feeds them. `Com_Error` is the engine's defined
rejection of a bad message, so the harnesses catch it and go on; anything
else the sanitizers report is a finding.

| Reader | Entry points | Executions | Result |
|---|---|---|---|
| `msg.cpp` delta readers | Commands, entities and players for all 18 wire profiles; the RTCW SP/MP, ET, Jedi Academy (vehicle and pilot bodies), Jedi Outcast and Alice readers; strings after each, in all four bit-stream modes | 21.3 M | clean |
| `mohaa_msg.cpp` | Protocol 6 entities, snapshot entity lists, players, sounds, commands and eye angles | 9.9 M (shared) | clean |
| `MSG_SetJKAFieldOverrides` | A mod's `netf_overrides.txt`/`psf_overrides.txt` text, then an entity and a player read with the widths it set | (in the above) | clean |
| `net_chan.cpp` `Netchan_Process` | A channel of every wire profile, client and server side, fed sequences of datagrams up to 4 KiB | 34.1 M | clean |
| `cmd.c` tokenisers | `Cmd_TokenizeString` and its quote-ignoring form on command lines up to `BIG_INFO_STRING`, then every argument, `Cmd_ArgsFrom`, the buffer copies and `Cmd_Args_Sanitize` | 5.0 M | clean |
| `unzip.c` | An archive as a pk3 reaches it: global info, every entry's info and name, open, read to the end, close, a lookup by name, and `QZ_Uncompress` on the same bytes | 7.1 M | **misaligned, sign-extending header loads** |
| Wire codecs in `code/compat` | Alice, F.A.K.K.2, Outcast, Academy (with a pak's override text), RTCW SP/MP, ET, Holomatch and EF SP, ioEF: every entity, player and keyed command reader, with encode/decode round trips; `ieee_delta_float.hpp` checked over all 2^32 words | 5.1 M | **Alice's command time overflowed** |
| GameSpy queries and LAN answers (`gamespy_query.hpp`) | A server's raw UDP query, its reply, a browser's answers from many sources | 6.2 M | clean |
| IHV and Q3Test channels and the IHV server session | `Process` on hostile datagrams, reliable queueing and transmission with loss, move and string readers, snapshot planning, pings, configstrings, client records | 4.6 M | **the IHV ping sum overflowed** |
| Raven string packages (`.str`, `.sp`) | Read, lookup, copy | 2.2 M | clean |
| Ritual model and script formats | `.ska`, `.skb`, `.tan`, TIKI definitions and poses, level-of-detail records, `.urc` menus, bind scripts, `.RitualFont` | 23.3 M (fonts 13.5 M and menus 1.7 M after the fixes below, clean) | **NaN hue; font metrics overflowing an int** |
| Raven and id model and data formats | `.glm`, `.gla`, `.rof`, skins, ICARUS blocks, `animation.cfg`, font `.fontdat`, Raven menus, effects files, `.mdm`/`.mdx`, `.aas`, `.rcd`, BSP queries, Bink headers | 143 M | clean |
| Savegames and records | Alice and MoHAA `.ssv`, the RTCW save envelope and server cache, EF SP chunks read in the engine's order, MoHAA load records, ET gameinfo, both ambient-set formats, F.A.K.K.2 inventory, ICARUS values, the JKA SP model precache | 22 M | **ICARUS type cast; precache recursion** |
| `q_shared.c` text readers | Info strings (lookup, tokenised lookup, pairs, remove, validate, set), `COM_ParseExt`, `COM_ParseComplex`, `Com_Split`, `Q_CleanStr`, `Q_replace`, `Com_SkipTokens`, `Com_SkipCharset` | 23 M, then 69.8 M after both fixes (clean) | **`Com_SkipTokens` read past its string; `Info_Tokenize` wrote past its buffer** |

Harness sources and scripts are kept under `.tmp/round-2026-11-04/fuzz/`
(not committed). Five small headers that parse no untrusted bytes (key
names, the HUD and memory-pool helpers) stopped on harness faults and were
effectively not fuzzed. Seeds came from small hand-built files and from retail
files read out of the installations; no retail byte is kept or committed.

### `Com_SkipTokens` stepped over its own NUL

`Com_SkipTokens( s, n, sep )` stepped over a character before testing for the
end, so a string ending in separators with fewer than `n` tokens -- `bind x `
asked for two -- stepped over its NUL and went on counting separators in the
memory after it, and could return a pointer outside the string. Its callers
are the console's `bind`, `set`-family, and two client commands, all given a
command line's arguments. The loop now tests for the end first; every string
the old loop handled correctly gives the same result.
`tests/q_shared_text_tests.cpp` (Meson and CMake) passes `"x \0 a b"`, which
the old loop answered with a pointer into `"a b"`.

### `Info_Tokenize` had no bound of its own

`Info_Tokenize` copies an info string's keys and values into a static
`MAX_INFO_STRING` buffer and its pairs into arrays sized for that, and its
comment calls it "NOT suitable for big infostrings": a longer string wrote
past both. Its three callers -- the server's userinfo filters, at connect and
for a connected client, and the operator's `filter` command for one -- pass a userinfo held in a
`MAX_INFO_STRING` array, so no engine input reached it; the fuzzer, which
passes any length, did once `Com_SkipTokens` stopped hiding it. It now reads
at most `MAX_INFO_STRING - 1` characters, within which the output (never more
than one byte longer than its input) and the pairs (three characters each at
the least, but the last) fit; every string its callers pass tokenises as
before. `tests/q_shared_text_tests.cpp` tokenises strings four times that
size: with only this function as HEAD has it, ASan reports a global buffer
overflow in it, and the fixed one finds the pairs in the first 1,023
characters and none after.

### `unzip.c` loaded header fields through `short` and `int` pointers

The central-directory reader copies a record's 46 bytes into a buffer and
loaded each field with `LittleShort( *(short*)(buf+n) )` or
`LittleLong( *(int*)(buf+n) )`. Offsets 38 and 42 are not aligned for an
`int`, which is undefined behaviour (it aborted every UBSan run at the first
archive, so the reader's real coverage was only reached with the alignment
check off), and a load through `short` sign-extended any 16-bit size of
`0x8000` or more into a `uLong` of `0xffff8000` and up. The file-by-file
reader beside it, `unzlocal_getShort`, reads the same fields unsigned, as
the zip format defines them. Stepping to the next record adds the name,
extra-field and comment sizes, so an entry with a 32 KiB extra field moved
the reader back instead of forward and the entries after it were lost.
The fields are now assembled a byte at a time, unsigned
(`unzlocal_le16`, `unzlocal_le32`). No pk3 a game ships has a field that
large, so installed archives read as before. `tests/unzip_tests.c` builds an
archive whose first entry has a 0x8000-byte extra field: HEAD's reader
reports its size as `0xffff8000` and fails, and the fixed one reaches,
reads and finds the second entry (gcc i686).

### Elite Force SP: a save could set any cvar

Found by reading the save reader beside its fuzzed chunks.
`SV_STVEFSPReadCvars` called `Cvar_Set` for every name and value a `.sav`
held, and `Cvar_Set` forces past ROM, INIT, CHEAT and protection, so a save
someone handed a player could set `sv_cheats`, `fs_game`, or a GL driver path
that the next `vid_restart` loads. The writer (`SG_WriteCvars` in retail, and
fnTech3's) saves the userinfo cvars alone, so the reader now restores only an
existing cvar with `CVAR_USERINFO`: every save either engine wrote loads as
before. The name is also copied and freed before the value is read, so a
damaged value chunk, which drops the load, no longer leaks it.
`tests/stvef_sp_savegame_tests.py` drives the real reader with scripted
chunks: HEAD sets the forced cvars and fails, the fix sets `name` and `model`
only and leaks nothing on a drop.

### Alice: a command's time step overflowed an int

`alice_message_codec.hpp` formed a command's time step as a signed
difference and read it back as a signed sum: a client sending a full time of
`0x7fffffff` then a step of 255 (a server reads that), or a hostile server's
far-apart snapshot times (a client writes against those), overflowed. The
step is now taken modulo 2^32, as the other titles' codecs take it; a signed
step of 0 to 255 is exactly an unsigned one below 256, so the bits sent are
unchanged. `tests/alice_message_codec_tests.cpp` steps across `INT32_MAX`
and sends the widest difference whole (its own round-trip case had the same
overflow in its fixture, now modular too).

### Quake III IHV: the ping sum overflowed

`q3_ihv_server_session.hpp`'s `Ping` (retail `SV_CalcPings` 0x41a910) sums
acknowledgement minus send time over 32 frames, and an unused frame's send
time is 0: on a level some 19 hours old, a newly connected client acking
unused frames summed `sv.time` 32 times, overflowed an int and showed a
negative ping. The sum is 64-bit; `tests/q3_ihv_server_session_tests.cpp`
covers it. Beside it, `server_q3_ihv_game.hpp` held the snapshot ring's
64-bit cursor in an `int`; after 2^31 entity states the stale-frame check
would have taken an overwritten frame as current. It is a `std::uint64_t`.

### Jedi Academy SP: a mesh's animation could be another mesh

`jka_sp_model_precache.hpp` registers the `.gla` a mesh names, and nothing
stopped that file from being a mesh naming the next: a crafted chain
recursed once a link, 1,022 deep through every slot, using about 200 KB of
stack at clang `-O2` and more in debug builds (a map load already uses about
870 KiB of the main stack), and filled the registry so every later model
failed. The first link's skeleton check refused the chain anyway, so a
mesh's animation is now registered as a skeleton only and stops there.
`tests/jka_sp_model_precache_tests.cpp` builds a sixteen-link chain: two
slots used, `Malformed`.

### Float-to-int conversions from data files

Converting a float an int cannot hold is undefined. Three readers did it
with values a file sets: ICARUS's typed value number
(`jka_icarus_values.hpp`, from a pak's `.IBI`), the hue of a `.urc` or bind
script colour (`alice_menu.hpp`; `atof` reads `nan` and `inf`), and a
`.RitualFont`'s height and glyph sizes (`alice_ritualfont.hpp`, Alice's,
F.A.K.K.2's and Medal of Honor's fonts). On MSVC x86 each gave `INT_MIN`
and a refusal or a wrong layout rather than a crash. Now the ICARUS number is
range-checked before its conversion; a hue no sector holds takes retail's
unknown-sector path (every finite hue keeps retail's sector, a grey's -1
degree included); and a font whose height, aspect or glyph metrics are not
finite or lie beyond 65,536 is refused at load, as retail refuses a bad
font, with a long line's summed width clamped. All 40 `.RitualFont` files in
the Alice, F.A.K.K.2 and Medal of Honor installations still load. The font
and menu fuzzers, which had stopped at these, were run again after the
fixes. Tests: `tests/jka_icarus_value_tests.cpp`, `tests/alice_menu_tests.cpp`,
`tests/alice_ritualfont_tests.cpp`.

### Left as they are

- `server_stvef_sp_save.hpp` allocates a chunk read into its own memory at
  the length the file claims; a chunk larger than the zone ends the engine
  with a fatal error. The length is bounded by the file's own size, so this
  needs a save of tens of megabytes, and the game's own reads set no smaller
  limit to hold it to.
- `alice_hud.hpp` converts a float the retail game module writes into its
  shared HUD block; the module, not a file, sets it.
- `gamespy_query.hpp` takes a `gamever` of `nan` as 1.00's, as OpenMoHAA's
  browser does with the same `atof` comparison **[O]**.
- `jka_model_format.hpp`'s mesh parser can do work that grows with the
  square of a hostile `.glm`'s size; no timeout at 64 KiB.

## Review of the latest eight commits

`6894fe47`, `810eeb11`, `17456358`, `187ce9f3`, `d2a0756d`, `ba58f6a8`,
`ca905995` and `bd209ffa` were read for correctness (per-frame caches and
their invalidation, profile scoping, renderer parity). No high-severity
defect was found. Two low ones are fixed:

- **Linux's profile clock wrapped.** `R_ProfileMilliseconds()` takes
  `ri.Microseconds() / 1000` as an `int`. On Windows `Sys_Microseconds` counts
  from its first reading; on POSIX it was `gettimeofday` counted from the
  epoch (about 1.8e12 ms), so every Linux renderer profile reading (com_speeds'
  `rf`/`bk`, the shadow atlas timings) was cut down, and a wall-clock step
  moved any delta taken across it. The POSIX branch now reads
  `CLOCK_MONOTONIC` (with `gettimeofday` only where that clock is missing) and
  counts from its first reading, as Windows does. Every caller takes
  differences (the renderers, the RTX frame event, the WebUI request
  schedule). Run on i686 Linux, the first reading is 0 and one 20 ms later is
  20,111 us; `tests/renderer_profile_clock_tests.py` pins the branch.
- **The path tracer's partition scratch could overflow its size.**
  `RT_EnsurePartitionScratch` admitted up to 2^29 triangles but allocates five
  bytes each as an `int`, which goes negative past about 429 million. The
  bound is now `INT_MAX / 5`; `tests/rtx_partition_tests.py` shows 2^29
  refused and the existing scratch kept.

`R_MOHAATraceable` walks a model's surfaces for each of its draw surfaces'
ownership checks, so its cost is quadratic in a model's surface count. Each
step is a few comparisons; it is recorded, not cached, since a cache would
need invalidating on every view and portal.

## Warnings

GCC 13's i686 build of the tree printed 38 warnings, none in a hot path:
`-Wmissing-template-keyword` in `jka_state_adapter.hpp`, where
`entityNum < INT16_MIN || ... >` reads to GCC as a dependent template's
argument list; `-Wenum-compare` where a ternary or `static_assert` mixes two
titles' constant enums (`sv_init.cpp`, `server_alice_game.hpp`,
`client_alice_cgame.hpp`, `cl_fakk2_inventory.cpp`, `fakk2_convert.hpp`); and
`-Wformat-truncation` on Alice's save-picker date. The comparisons are
parenthesised or cast to `int` and the date buffer is 64 bytes. Values are
unchanged.
