# Broadcast echo without localization tokens: RTCW MP and ET

## Scope

A dedicated server prints every broadcast `print` command to its own console
as `broadcast: ...`, through `SV_ExpandNewlines`, which also writes each
newline as a backslash and an `n`. The RTCW multiplayer and Enemy Territory
game modules mark the text of many such commands with `[lon]` and `[lof]`.
The client game reads them: its translation is on by default, `[lof]` turns it
off and `[lon]` turns it back on (`CG_LocalizeServerCommand` in both games'
`cg_servercmds.c`). Retail `WolfMP.exe` and ET 2.60b leave the tokens out of
the console echo. fnTech3 printed them. For the same `timelimit` change on
2026-09-23, from `scripts/probe_rtcw_restart_game_type.py`:

| Server | Console line |
| --- | --- |
| Retail `WolfMP.exe` | `broadcast: print "Server: timelimit changed to 10\n"` |
| fnTech3 `fntech3.ded.exe` `12bf0310465e1b98c70e1ecb4d975ab6ce52be9082c515dd2c32a316972a10dc` | `broadcast: print "Server:[lof] timelimit [lon]changed to[lof] 10\n"` |

This record covers the `rtcw-mp` and `wolfet` profiles. Only the console echo
changes. What each client receives is unchanged in every profile, and every
other profile's echo is unchanged.

## Observed

### Sources

- The [RTCW MP](https://github.com/id-Software/RTCW-MP/blob/master/src/server/sv_main.c)
  and [ET](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_main.c)
  `SV_ExpandNewlines` compare each byte that is not a newline with `[lon]`
  and `[lof]` through `Q_strncmp` for five bytes. On a match they advance the
  read position by five and copy nothing. A comment calls it a hack that keeps
  the tokens out of the system console window. `SV_SendServerCommand` prints
  the result only when `com_dedicated` is set and the command starts with
  `print`, and hands every client its own formatted message.
- The [RTCW SP](https://github.com/id-Software/RTCW-SP/blob/master/src/server/sv_main.c)
  and [Quake III](https://github.com/id-Software/Quake-III-Arena/blob/master/code/server/sv_main.c)
  expanders only expand newlines. iortcw keeps the test in MP and has none in
  SP.
- RTCW MP's own `SV_DropClient` broadcasts `print "[lof]%s^7 [lon]%s\n"`.
  ET's broadcasts `cpm "%s^7 %s\n"` instead, with the tokenized line
  commented out.

### Retail executables

Read with capstone. Addresses are virtual addresses in each image.

| | `WolfMP.exe` (`Wolf 1.4-MP`) | ET 2.60b `et.exe` | ET 2.60b `etded.exe` | `WolfSP.exe` (`Wolf 1.32`) | Quake III `quake3.exe` (`Q3 1.32`) |
| --- | --- | --- | --- | --- | --- |
| SHA-256 | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` | `7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf` | `07d206f61da5c80562ff60d68c645bca349b31adb5d7e0e7f186d61ad46683c7` | `80b0add0e014b863cf47cab6c997e0a54939236219a56910b7a3eb77dc2d44d4` |
| Expander | `0x439e00` | `0x447ce0` | `0x42e4d0` | `0x437720` | inlined in `SV_SendServerCommand` at `0x43977a` |
| Output bound | `cmp esi, 0x3fd` at `0x439e0e`: 1,021 bytes | `0x447cf0`, the same | `0x42e4e0`, the same; a range check against `0x400` at `0x42e549` gives the buffer's 1,024 bytes | `0x43772c`, the same | `0x439788`, the same |
| Token test | `Q_strncmp( in, "[lon]", 5 )` at `0x439e2d`, then `"[lof]"` at `0x439e41`; either match jumps to `add edi, 5` at `0x439e61` | `0x447d12` and `0x447d26`; skip at `0x447d4d` | `0x42e502` and `0x42e516`; skip at `0x42e53d` | none | none |
| Tokens, compare | strings `0x4efaf8`, `0x4efaf0`; `Q_strncmp` at `0x42d190` | `0x562f0c`, `0x562f14`; `0x41faa0` | `0x4c4634`, `0x4c463c`; `0x404aa0` | neither string is in the image | neither string is in the image |
| Echo | in `SV_SendServerCommand` (`0x439f40`): `com_dedicated->integer` at `0x439f90`, `strncmp( message, "print", 5 )` at `0x439fa9`, the expander at `0x439fba`, `broadcast: %s\n` at `0x439fc0` | `0x4487c0`: `0x448821`, `0x44883a`, `0x44884d`, `0x448853` | `0x42f010`: `0x42f088`, `0x42f0a1`, `0x42f0b4`, `0x42f0ba` | expander at `0x437895`, format at `0x43789b` | buffer at `0x4397b5`, format at `0x4397ba` |
| What clients get | the formatted message on the stack (`esp+8`), to `SV_AddServerCommand` (`0x439e80`) at `0x43a000` for a broadcast and at `0x439f7f` for one client | `ebp-0x8000`, to `0x447820` at `0x448895` and `0x448815` | `ebp-0x8004`, to `0x42e010` at `0x42f100` and `0x42f071` | -- | -- |

The three compare helpers are the case-sensitive `Q_strncmp`: each stops at
the count or at a terminator, and returns zero only when every byte matched,
with no case folding. What follows from the loops:

- The test runs at every read position whose byte is not a newline, before
  that byte is copied, so the command's name is scanned too.
- A skipped token writes nothing and does not count toward the bound.
- The output is never scanned again: `[lo[lon]n]` echoes as `[lon]`.
- All five bytes must match: `[LON]`, `[lonx]` and a trailing `[lon` stay.

No other supported retail engine contains either token. A byte search finds
neither in Quake Live's `quakelive_steam.exe`
(`c926fe9f6c851e00b3b9332e88903ad01f28fdd60454873891c0158f5ded1299`), the IHV
test's `quake3.exe`
(`e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37`), Elite
Force's `stvoy.exe`
(`8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74`) and
`stvoyHM.exe`
(`5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff`), Jedi
Outcast's `jk2sp.exe`
(`0d3c635ee5b20ac32fc957e00cf21e65ebc1584112b11f6ae026706dcc0d0e0c`) and
`jk2mp.exe`
(`2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a`), Jedi
Academy's `jasp.exe`
(`9351a8de230fcd577e2b474889d08f38982450306767a6823dd659482566d6ce`) and
`jamp.exe`
(`e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b`), or
`alice.exe`
(`8aa22917b7707f0e1a7c4dbe9a3c38332b1873ae2059174ac5329de26921fc18`).

## Change

- `SV_ExpandNewlines` (`code/server/sv_main.cpp`) skips a `[lon]` or `[lof]`
  at the read position when the profile is `rtcw-mp` or `wolfet`, as the
  retail loop does: before copying a byte that is not a newline, five exact
  bytes, nothing written and no effect on the bound.
- `SV_SendServerCommand` is unchanged. It still hands every client its own
  formatted message; only the `broadcast:` line goes through the expander.
- Every other profile's echo is unchanged, and so is fnTech3's bound, 2,045
  output bytes in a 2,048-byte buffer, in every profile.

The RTCW, ET, iortcw and Quake III sources and the retail executables were
read as reference observations. No text from them is incorporated.

## Evidence

- `tests/server_broadcast_echo_tests.py` compiles the production
  `SV_ExpandNewlines`, `SV_AddServerCommand` and `SV_SendServerCommand`, with
  a `Com_Printf` that keeps what it prints. It runs every profile in the
  catalog, as a dedicated and as a listen server, with two active clients
  (one taking long strings), one primed and one connected. It sends:
  - nine commands whose echoes, with and without the tokens, are written out
    in the test: the logged `timelimit` change, `WolfMP.exe`'s drop notice,
    adjacent tokens, the token the single pass leaves behind, wrong case and
    broken tokens, newlines beside tokens, a token and part of one at the
    end, and a token in the command's name. Each is also broadcast as a `cp`
    command and addressed to one client;
  - a 1,138-byte command whose echo meets the bound, with and without its
    twenty tokens;
  - 300 deterministic commands built from pieces of tokens, whose echoes must
    equal the retail loop above, transcribed, with fnTech3's bound.

  Only a dedicated server's broadcast `print` is echoed, and only `rtcw-mp`
  and `wolfet` leave the tokens out. Every primed client stores exactly the
  command that was sent, tokens included; the 1,138-byte command reaches only
  the long-string client, cut to a reliable slot's 1,023 bytes as before. The
  test ran over 13 profiles in the shared tree, and over 7 in an export of
  `2d35919` that carries only this change.
- Twelve in-memory mutations of the compiled production code each fail it:
  no skip, as before; a skip in every profile; in RTCW MP only; in ET only;
  in RTCW SP as well; `[lon]` only; `[lof]` only; a case-insensitive compare;
  a four-byte skip; a second look after a skip; clients handed the echo text;
  and a bound two bytes shorter.
- `tests/rtcw_cast_messages_tests.py`, which also compiles the production
  expander, passes.
- Runtime, Windows x86 `meson/build/win32-dev` with the RTCW and ET
  development options: `75561b9` plus this change and other sessions'
  uncommitted work, `fntech3.ded.exe`
  `d2c23bd15f0fe2c2bf09b340dfa978e52703a5b2a4502a78cdb2c3b889218d11`.
  `scripts/probe_broadcast_echo.py` runs one dedicated server on a private
  desktop, with an isolated home under `.tmp/rtcw-et-broadcast-echo/`,
  loopback only, no masters, no clients and no input. It execs `map`,
  `wait 60`, `timelimit 17`, `wait 60`, `timelimit 23`, `wait 60` and `quit`,
  with two `echo` markers, and keeps every `broadcast:` line.

  | Server | Map | `broadcast:` lines |
  | --- | --- | --- |
  | fnTech3, `rtcw-mp` | `mp_beach` | `print "Server: timelimit changed to 8\n"`, then 17, then 23 |
  | Retail `WolfMP.exe` | `mp_beach` | the same three lines |
  | fnTech3, `wolfet` | `goldrush` | `print "Server: timelimit changed to 30\n"`, then 17, then 23 |
  | Retail `etded.exe` | `goldrush` | the same three lines |

  The first line in each is the time limit the map sets itself. The servers
  ran the installations' `qagame_mp_x86.dll`, RTCW MP's
  `d8975b5c4e14488283c5eca7d549005304b04f1690705c13c174a171cc24c387` and
  ET's `160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b`, and
  loaded the maps from `mp_pak0.pk3`
  (`c0e090e7b4245313bea64b5fb20b24d69786e593716cfe6bf85ee9b4f3c794b3`) and
  ET's `pak0.pk3`
  (`712966b20e06523fe81419516500e499c86b2b4fec823856ddbd333fcb3d26e5`). Every
  server quit by itself with exit code 0, and each installation's file
  inventory was unchanged.
- Earlier fnTech3 ET servers printed the tokens as well. A 2026-09-17 probe
  logged `broadcast: print "[lof]fntech3-et-probe^7 [lon]entered the game\n"`.
- The tree of `2d35919` with this change alone was exported and built for
  Windows x86 with MSVC, `debugoptimized` and strict warnings, with the RTCW,
  Elite Force and ET development options on, as in `meson/build/win32-dev`.
  All 1,200 targets built without a warning. `fntech3.exe` had SHA-256
  `e22fb8a33f574e75266ddfc96b33d9dfae679f759f033e390df380a1e28695d6` and
  `fntech3.ded.exe`
  `72ad4e4f0ac93c00751a681dde71edc996d5a442451aa181b707f30f88a3b05d`. Its 116
  Meson C++ tests pass and two skip: OpenAL's `ALC_SOFT_loopback` was
  unavailable, and the retail factory probe found no Quake Live without
  `FNQL_RETAIL_QL_PATH`. The whole Python suite passes in the same export:
  1,833 tests, with 76 skipped.

## Open

- The bound. Retail stops at 1,021 output bytes in a 1,024-byte buffer.
  fnTech3 stops at 2,045 in 2,048 in every profile, Quake III's included,
  and this change leaves that alone.
- The drop notice. Retail RTCW MP broadcasts `print "[lof]%s^7 [lon]%s\n"`
  when a client leaves (`WolfMP.exe` pushes the string, `0x4eeea4`, at
  `0x434f3d` in `SV_DropClient`), so its client game translates the reason
  but not the name. ET 2.60b broadcasts `cpm "%s^7 %s\n"` (`et.exe` at
  `0x441937`, `etded.exe` at `0x427f57`), a pop-up message rather than a
  console print. fnTech3 broadcasts `print "%s^7 %s\n"` for both. That is
  what clients receive, so it needs its own change.
- Bots. Retail ET's broadcast loop also skips a client whose entity has
  `SVF_BOT` (`test byte ptr [eax+0x128], 8` at `et.exe` `0x448884` and
  `etded.exe` `0x42f0ef`), as its GPL source does. fnTech3 skips only RTCW's
  cast AI, so an ET bot is sent every broadcast.
- `echo`. Retail ET's `echo` queues a `cpm` command instead of printing
  (`Cmd_Echo_f` in the GPL `cmd.c`), and `etded.exe` logged neither of the
  probe's markers. fnTech3's ET profile prints them.
- No client was connected in any run. That clients still receive the tokens
  rests on the unit test and on the disassembly above.
