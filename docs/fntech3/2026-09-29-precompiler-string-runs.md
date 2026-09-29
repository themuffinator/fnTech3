# Strings a define expands to, joined without a frame for each

The botlib's script precompiler, `code/botlib/l_precomp.cpp`, reads bot
characters, chat, weights, items and weapons for every profile whose bots use
Quake III's botlib. It also reads the menus the UI modules read through the
`PC_*` imports: Team Arena's, Quake Live's, RTCW's, Enemy Territory's and the
Jedi games'. Both kinds of file come from paks, a downloaded one included.
The precompiler joins adjacent strings. The script reader joins the quoted
strings it sees side by side itself, in a loop (`PS_ReadString`).
`PC_ReadToken` joins the rest, such as the strings defines expand to, and it
read each following string by calling itself. A run of N such strings took N
frames, each holding a 1,024-byte token. A run of 1,000 overflowed a 1 MB
stack. A bot character file that spelled its name with 20,000 faulted the
dedicated server with `STACK_OVERFLOW`, and the level ended. `PC_ReadToken`
now joins the strings that follow in a loop, and every installed menu and bot
file reads to the same tokens as before. The
[machine-readable record](2026-09-29-precompiler-string-runs.json) holds the
runs.

## Observed

A host of the precompiler (`tests/fixtures/precompiler_host.cpp`, built for
the MSVC x86 target with its 1 MB main stack) read these under Quake III's
rules and Enemy Territory's:

| Input | `8e345c8` | With this change |
| --- | --- | --- |
| `start`, 100,000 `""`, `end` | `start` `end` | the same |
| `#define S ""`, `start`, 500 `S`, `end` | `start` `end` | the same |
| the same with 1,000 or 20,000 `S` | `0xC00000FD`, stack overflow | `start` `end` |
| `start`, 1,000 or 20,000 `S`, `"tail"`, `S`, `end` | `0xC00000FD` | `start` `tail` `end` |
| `#define B "b"`, `#define C "c" B`, `start "a" C "d" x B B end` | `start` `acbd` `x` `bb` `end` | the same |
| two 600-character strings a define expands to, side by side | `string longer than MAX_TOKEN`, reading stops after `start` | the same |

The plain quoted strings never reach `PC_ReadToken`'s join: the script reader
has already made them one token. Only the strings it cannot see together
recursed.

## Change

`PC_ReadExpandedToken` is the old loop without the join: it reads a token,
runs directives, skips a skipped branch and expands defines. `PC_ReadToken`
reads through it and, for a string, joins each following string in a loop,
reading back the first token that is not one. Between the strings,
directives, skipped branches and defines run as before, since each look-ahead
goes through the same loop. A join past `MAX_TOKEN` is refused with the same
error. Where the old recursion joined from the last string backwards, the
loop joins from the first forwards. That gives the same string whenever it
fits in a token, which is the only time a join succeeds.

## The installed files

[`scripts/compare_precompiler_streams.py`](../../scripts/compare_precompiler_streams.py)
builds the host twice, with `8e345c8`'s precompiler and with this one. It lays
out each installation's menus, their headers and its bot files from its paks,
later paks over earlier ones. Each file is read as an entry, from the game
folder, or for bot files from `botfiles/` as the botlib reads them. Both hosts
gave the same token stream, the same error and warning counts and the same
exit status for every one:

| Installation | Files | Tokens | Files ending in an error, in both |
| --- | ---: | ---: | ---: |
| Quake III and Team Arena | 245 | 401,405 | 3 |
| Quake Live | 212 | 484,205 | 2 |
| RTCW | 243 | 313,527 | 0 |
| Enemy Territory | 81 | 100,446 | 8 |
| Elite Force | 243 | 250,325 | 1 |
| Jedi Academy | 96 | 18,857 | 83 |
| Jedi Outcast | 63 | 11,913 | 55 |

The Jedi games' menus stop at the first token Quake III's script reader cannot
read, as they did before; neither Jedi profile runs a UI module yet.

## Tests

`tests/wolfet_menu_precompiler_tests.py` reads runs of 1,000 and 20,000
define-expanded strings under both rules, which overflowed the host before
and now give `start` `tail` `end`. It checks strings joined across defines,
and the refused join past `MAX_TOKEN`. Its source check now finds the skip
check ahead of the look-ahead, which reads each following string through
`PC_ReadExpandedToken`.

## Runtime

[`scripts/probe_precompiler_string_runs.py`](../../scripts/probe_precompiler_string_runs.py)
runs a Quake III dedicated server on a private desktop with networking off.
It loads `q3dm1` with bots on and adds Sarge at skill 1. Its isolated home
holds either Sarge's retail character file (`pak4.pk3`, unchanged) or a copy
whose skill 1 name is 20,000 `FNTECH3_EMPTY`, defined as `""`, before
`"Sarge"`.

| Server | Retail file | The run |
| --- | --- | --- |
| `8e345c8` | loaded, `Sarge` listed | `Unhandled exception caught`, `STACK_OVERFLOW` at `fntech3.ded.exe@5624b`, `PC_ExpandDefine+0x1b` by the build's PDB; the level ended |
| with this change | loaded, `Sarge` listed | loaded, `Sarge` listed |

The overflow is reported where it met the guard page, in the define
expansion that each level of the recursion runs; the frames above it were
`PC_ReadToken`'s. The engine's exception filter caught it, and no crash dump
was written.

## What this does not show

- Retail engines were not run with such a file. Quake III 1.32's
  `l_precomp.c` joins by the same recursion, so they would overflow too;
  that is inferred from the source.
- No UI module read such a menu in a running client. The menus ran in the
  host only, and the bot file on the dedicated server.
- Windows x86 only.

## Identity

- Tested commit `8e345c8f05bf66b748e85cbe3686cde78f4f3a05`, built as a
  `git archive` export with win32-dev's options, strict warnings and every
  development option, on Windows 11 x64, 2026-09-29.
- Server without the change `53e269ac165ecd1415651d33d483d8994e8372bf35b1ebf4b045bf36d476b15e`,
  with it `82d228b20a9ba2ecb682c4d7bf23c1e7772c8731a4ce7e0766f0a47a81f0e7af`.
- Installations read only: Quake III, Quake Live, RTCW and Enemy Territory
  under `Steam\steamapps\common`, and Elite Force, Jedi Academy and Jedi
  Outcast under `E:\Games`.
