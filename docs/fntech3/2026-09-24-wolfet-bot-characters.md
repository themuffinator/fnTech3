# Enemy Territory's dedicated server makes bots

## Scope

An ET dedicated server started with `bot_enable 1` makes a bot when its
console runs the game's `spawnbot`. On fnTech3 the bot library could not
load ET's own bot character, and the retail game then faulted.
`2026-09-24-wolfet-bot-broadcasts.md` found this and left it open. This
record traces the failure and the two further differences behind it. It
makes the `wolfet` profile load, set up and pace bots as `etded.exe` does:

- **Character includes.** ET reads a character file's includes from the game
  root.
- **Configured subsystems.** ET sets up Quake III's weapon, item, chat and
  movement configuration when its bot library starts.
- **Think time.** ET's engine registers `bot_thinktime` as 50 ms.

Only the `wolfet` profile changes. Quake III, Quake Live, RTCW and Elite
Force keep their character rule, their subsystems and their `bot_thinktime`
of 100. Every other loader keeps its folder while it parses, in every
profile, as before.

## Observed

### Retail files

| File | SHA-256 |
| --- | --- |
| `etded.exe` (ET 2.60b) | `7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf` |
| `et.exe` (ET 2.60b) | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` |
| `etmain/qagame_mp_x86.dll` | `160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b` |
| `etmain/pak0.pk3` | `712966b20e06523fe81419516500e499c86b2b4fec823856ddbd333fcb3d26e5` |
| `quake3.exe` (`Q3 1.32`) | `80b0add0e014b863cf47cab6c997e0a54939236219a56910b7a3eb77dc2d44d4` |

The executables were read with capstone. Addresses are virtual addresses
in each image.

### ET's botfiles

`pak0.pk3` holds every botfile the installation ships. `pak1.pk3`,
`pak2.pk3` and `mp_bin.pk3` hold none.

| File | Loaded by | Includes | Resolved below `botfiles`? |
| --- | --- | --- | --- |
| `bots/default_c.c` | character | `"botfiles/chars.h"` (line 9) | no: from the game root |
| `bots/default_i.c` | item weights | `"inv.h"`, `"fw_items.c"` | yes |
| `bots/default_w.c` | weapon weights | `"inv.h"`, `"fw_weap.c"` | yes |
| `bots/default_t.c` | chat file | `"teamplay.h"` | yes |
| `items.c` | item configuration | `"inv.h"` | yes |
| `weapons.c` | weapon configuration | `"inv.h"` | yes |
| `match.c` | match templates | `"match.h"` | yes |
| `syn.c` | synonyms | `"syn.h"` | yes |

`rnd.c`, `rchat.c`, `chars.h`, `inv.h`, `fw_items.c`, `fw_weap.c`,
`match.h`, `syn.h` and `teamplay.h` include nothing. `bots.txt` in
`scripts/` names `bots/default_c.c` as the only character, whose skill
blocks are 1 to 4. `chars.h` numbers its characteristics 0 to 48, inside
the library's 80.

### How each engine resolves an include

The two engines resolve an include in the same way.

- `LoadScriptFile` (`etded.exe` 0x460200) opens `<folder>/<file>` when the
  script folder at 0xcb4520 is set, and `<file>` when it is empty.
- `PC_Directive_include` (0x437cc0) tries the include's own path, then
  the source's empty include path followed by it.
- `PC_ConvertPath` (0x438f10) makes every separator a backslash first.
- fnTech3's `l_script.cpp` and `l_precomp.cpp` do the same.
- The filesystem's lookup folds `\` into `/`.

So what differs is only which folder is set while a file is parsed.

### Where ET sets and clears the folder

`PS_SetBaseFolder` is 0x461750 in `etded.exe`. It is called 15 times:
seven `"botfiles"`/`""` pairs, and one `""` in `PC_LoadSourceHandle`
(0x43ab5b).

| Loader | Set | Cleared | Between them |
| --- | --- | --- | --- |
| `BotLoadCharacterFromFile` (0x45f5d0) | 0x45f5e7 | 0x45f5fc | `LoadSourceFile` alone (0x45f5f0); parsing starts at 0x45f64a |
| `BotLoadItemWeights` (0x453c90) | 0x453caa | 0x453cbe | `ReadWeightConfig` (0x4522f0), parse included |
| `BotSetupGoalAI` (0x453eb0) | 0x453ec0 | 0x453ee4 | `LoadItemConfig` (0x4544c0) |
| `BotLoadWeaponWeights` (0x45aa20) | 0x45aa42 | 0x45aa57 | `ReadWeightConfig` |
| `BotSetupWeaponAI` (0x45aac0) | 0x45aac5 | 0x45aae9 | `LoadWeaponConfig` (0x45abe0) |
| `BotLoadChatFile` (0x45c200) | 0x45c32e | 0x45c34b | `BotLoadInitialChat` (0x45c5c0) |
| `BotSetupChatAI` (0x45e690) | 0x45e699 | 0x45e735 | synonyms, random strings, match templates and reply chats |

So only a character is parsed with no folder, and ET's GPL `be_ai_char.c`
reads the same way. Every other loader parses with `botfiles` set. That
suits every include in the table above.

`quake3.exe` 1.32 calls its setter (0x498f60) nine times, always with
`"botfiles"` and each directly before `LoadSourceFile` (0x498c20). Its
character loader (0x4991e0) sets it at 0x4991f7. No call clears it, so a
Quake III character's includes resolve below `botfiles`, as fnTech3's did
in every profile.

### What ET's bot library sets up

`etded.exe`'s `Export_BotLibSetup` (0x436ed0) runs six stages in order:

1. AAS (0x43bf20)
2. actions (0x451600)
3. weapons (0x45aac0)
4. goals (0x453eb0)
5. chat (0x45e690)
6. movement (0x457a50)

It stops at the first stage that fails. `et.exe`'s (0x46c570) runs the same
six. In both, the weapon stage loads `weapons.c` and the goal stage
`items.c`, the chat stage begins with `syn.c`, and the movement stage sets
`sv_step`.

fnTech3's `wolfet` profile set up AAS and actions only, because its bot
policy named no configured subsystem. It had done so since the profile was
added in `602f3e7`. `BotLoadItemWeights` needs the item configuration and
`BotLoadWeaponWeights` the weapon configuration. By fnTech3's source, its
movement code outside RTCW (`BotCheckBlocked`, `BotCheckBarrierJump`) also
reads `sv_step` and `sv_maxbarrier` through variables that only the
movement stage sets, so an ET bot that moved would have read a null
pointer. The ET import dispatch already served every goal, weapon, chat and
movement import.

### ET's think time

Each ET engine registers `bot_thinktime` with default `"50"` and no flags:

- `etded.exe` at 0x425615..0x425621;
- `et.exe` at 0x43f245..0x43f251.

ET's GPL `sv_bot.c` does the same. Quake III's registers 100, and
fnTech3 registered 100 in every profile.

The retail game's `BotAIStartFrame` (0x200203b0) paces its bots by that
value:

- It adds the time since its last call to a residual (0x20020454..0x20020471).
- It takes the elapsed time as its think time instead when that is longer.
- Once the residual reaches the think time, it updates the bot library.
  With no AAS loaded it then returns (`je 0x20020690` at 0x200204b7).
- Otherwise it goes on to `BotAIThinkFrame` at 0x20020688.

A dedicated server calls the frame every 50 ms, so at 50 the residual
reaches the think time on every call, and no bot ever thinks without
navigation. At 100 the residual reaches it on every other call, so bots
think on the calls between. A thinking bot drains and acknowledges its
server commands through import 210. It also asks for its area through
imports 308 and 310, which log `AAS_PointAreaNum: aas not loaded` and
`AAS_LinkEntity: aas not loaded`; `etded.exe` prints the same two lines
(0x43d7b0, 0x43cb24) whenever that is asked with no AAS loaded.

### The retail game's fault

`BotAISetupClient` (0x2001eb40) loads the bot's character with the settings'
`float` skill converted to an integer (0x2001eb9d..0x2001ebaa). When that
fails, it passes the same float to `"couldn't load skill %d from %s\n"` as
an eight-byte double: `sub esp, 8` and `fstp qword ptr [esp]` at 0x2001ebc6,
after the file name is pushed. So `%d` reads the double's low word and `%s`
its high one.

The fault is `cmp byte ptr [eax], 0` at 0x20090ae6, in the runtime's
string-length loop that follows its `(null)` substitution. A debugger
caught the before-build's first-chance fault there with `eax = 0x40100000`.
`G_SpawnBot` defaults the skill to 4, and 4.0 is `0x4010000000000000`. So the
game prints a bad pointer on its own failure path, as its GPL `ai_main.c`
reads. The fix below keeps stock bots off that path. A character that fails
to load still reaches it in both engines.

### The runs

`scripts/probe_wolfet_bot_broadcast.py` runs one dedicated server on a
private desktop, with an isolated home, loopback networking and no
clients. It loads `goldrush` with `bot_enable 1`, runs `spawnbot` and
lists the clients. It then says 300 lines in one frame, the burst. After
that, 300 times, ten frames apart, it changes `sv_hostname`, which is a
configstring update addressed to the bot, and says a line; that is the
flood. The run passes when:

- the bot is listed before and after the burst;
- the burst overflows nothing;
- the flood overflows the bot with configstring updates and no broadcast;
- the server quits cleanly and the installation is unchanged.

Every fnTech3 server was built from a `git archive` of `2d35919` with
`2026-09-24-wolfet-bot-broadcasts.md`'s change. That change is staged but
not committed, and it leaves ET bots out of broadcasts. The builds added
none, some or all of this record's change:

| Server | SHA-256 | Outcome |
| --- | --- | --- |
| retail `etded.exe` | `7eb60691…` | Passes. The bot joins and is dropped when the update carrying `fntech3-flood-248` overflows its 256-command window. |
| before this change | `d65a87a9…` | Eight `file botfiles\chars.h not found` errors, `couldn't load any skill from bots/default_c.c`, then the fault at `qagame_mp_x86.dll+0x90ae6`. No bot; the server idles until the 90 s timeout. |
| the include rule alone | `575f59bb…` | The character and `bots/default_i.c` load, then the game prints `BotAISetupClient failed` and `Unable to add bot: BotConnectfailed`. No fault, no bot. |
| the include rule and the subsystems | `527b56be…` | The library logs `loaded weapons.c`, `items.c`, `syn.c`, `rnd.c`, `match.c` and `rchat.c`, as `etded.exe` does. The bot joins, and 15 `aas not loaded` lines follow. It survives all 300 flood steps. |
| this change | `43d103ee…` | Passes. The bot joins with no AAS error, gets no broadcast, and is dropped when the update carrying `fntech3-flood-54` overflows its window. |

In the fourth row the bot thinks: a debugger stopped at the first
`AAS_PointAreaNum` with the module's frames on the stack. They are
`vmMain`, `BotAIStartFrame`'s call to `BotAIThinkFrame` (return 0x2002068d),
its call to `BotAI` (0x20020360), and the area lookup that calls import
308 (0x20015252).

It was not the server's frame timing. The same build called the game's bot
frame 124 times. After the first call, one came with an unchanged time as
the map finished loading, 119 came 50 ms apart and three came 100 ms apart.
At a think time of 100, 50 ms steps reach the residual only on every other
call.

At the drop, retail and this change hold the same kinds of pending commands:

- serverinfo updates carrying the flood's hostnames;
- `cs 22`, `23`, `24`, `690` and `755`;
- one `aftc` and two `tinfo`;
- no broadcast.

Retail held 256 of them, and fnTech3 64: a bot's window is still the
legacy Quake III count in a fresh slot. See Open.

The bot's class, team, joins and userinfo changes match retail's. Its name
and latched weapon vary from run to run in both engines: five retail runs
latched weapon 24 or 8, and so did fnTech3's.

## Change

- **Character includes.** `BotLoadCharacterFromFile` (`code/botlib/be_ai_char.cpp`)
  clears the script folder after opening a character when the profile is
  `wolfet`, before parsing it, as `etded.exe` does. The file is still opened
  below `botfiles`. Other profiles keep the folder while parsing.
- **Configured subsystems.** The `wolfet` bot policy (`code/qcommon/game_profile.cpp`)
  names `COM_BOT_AI_QUAKE`. The bot library then sets up weapons, goals, chat
  and movement after AAS and actions, as `etded.exe` and `et.exe` do. At map
  load it now also initializes level items once AAS has loaded, as
  `etded.exe`'s `Export_BotLibLoadMap` (0x436e10) does.
- **Think time.** `SV_BotInitCvars` (`code/server/sv_bot.cpp`) registers
  `bot_thinktime` with default 50 for `wolfet`. Every other profile keeps 100,
  and the flags stay 0.

None of these runs unless `bot_enable` is set, which ET's profile leaves at
0. The ET, Quake III and RTCW sources, the retail executables and the retail
module were read as reference observations. No text from them is
incorporated.

## Evidence

- `tests/wolfet_bot_character_tests.py` builds the production character
  and weight loaders, with the real precompiler and script reader, into a
  host (`tests/fixtures/bot_character_host.cpp`). The host prints every
  path the loaders ask for, and runs over two synthetic trees: one whose
  character includes `"botfiles/chars.h"`, one whose includes `"chars.h"`.
  - Under ET's rules the first tree's character loads from
    `botfiles/bots/default_c.c` and `botfiles/chars.h`. Under Quake III's
    it fails with the before-build's error, after looking for
    `botfiles/botfiles/chars.h`.
  - The second tree loads under Quake III's rules. Under ET's it looks for
    `chars.h` at the root and fails, as retail ET would.
  - After either character, a weight file that includes `"inv.h"` and
    `"fw_items.c"` beside it still loads.
  - Skill 2 interpolates skills 1 and 4, each parsed by ET's rules.

  Four mutations of the production loader must each fail these checks,
  and do:

  - no ET clear;
  - a clear in every profile;
  - the clear in every profile but ET;
  - ET opening the file from the root.
- The same file checks every profile's configured subsystems through the
  profile table. ET's are Quake III's; the other six profiles at `2d35919`
  keep theirs.
- `tests/rtcw_botlib_tests.py` now expects ET's `Export_BotLibSetup` to
  start all six stages, and its map load to initialize level items.
  `tests/bot_profile_defaults_tests.py` now checks `bot_thinktime`'s
  default in every profile, and stubs `Com_GameIsWolfET`.
- The runs above. The
  [evidence file](evidence/2026-09-24-wolfet-bot-characters.json) holds each
  server's hash, command line, bot rows, pending-command summary and outcome.
- Every probe run left the installation's file inventory unchanged. The
  debugger runs used isolated homes too, and after all the runs no file in
  the installation had been modified that day.
- The fnTech3 servers were built for Windows x86 with MSVC,
  `debugoptimized` and strict warnings, with the RTCW, Elite Force and ET
  development options on. They built without a warning.
- A second export of `2d35919`, with the bot-broadcast change and this
  change, built all 1,200 targets the same way without a warning.
  `fntech3.exe` had SHA-256
  `618d7aa93682f40b092ecce1739b09aef5d754fedaae8cc02aee6989884bd94f` and
  `fntech3.ded.exe`
  `0e8ade03513fa85ac02ad97f2ec9a92d2add0b850b0f1742750cc899d2709001`.
  - Its 116 Meson C++ tests pass and two skip: `fnql_audio_loopback`,
    because OpenAL's `ALC_SOFT_loopback` was unavailable, and
    `fnql_retail_factory_probe`, which finds no Quake Live without
    `FNQL_RETAIL_QL_PATH`.
  - The whole Python suite passes there: 1,836 tests, with 76 skipped.
    `tests/wolfet_bot_broadcast_tests.py` ran at its staged version.

## Open

- **A bot's window.** It is still the legacy Quake III count of 64 in a
  slot no network client has used, where both ET engines give every client
  256. So this change's bot is dropped after 64 unacknowledged commands,
  not 256. The second Open item of `2026-09-24-wolfet-bot-broadcasts.md`
  names the same difference; its first, that no ET bot could be made, is
  the one this record resolves.
- **Console lines.** fnTech3 prints five lines while making the bot that
  `etded.exe` does not:
  - `loaded skill 4 from bots/default_c.c`;
  - `loaded cached skill 4.000000 from bots/default_c.c`;
  - `loaded bots/default_i.c`;
  - `loaded bots/default_w.c`;
  - `loaded WolfBot from bots/default_t.c`.

  ET's source comments out or compiles out each one, and neither ET
  executable contains the `loaded %s from %s` or `weights loaded` strings.
  It also broadcasts `Server command overflow` for the dropped bot, which
  retail does not.
- **Bots with navigation.** The installation ships no `.aas` file, so bots
  on stock maps neither move nor think, in either engine. Bots on a map
  with navigation have not been run.
- **The retail game's failure print.** It remains, as in retail. A
  character that fails to load, such as one written with Quake III's
  `#include "chars.h"`, faults the retail game on both engines.

## Unrelated observations

- **RTCW's bot library.** RTCW MP's GPL `be_ai_char.c` and loaders never set
  a script folder, so its bot files load from the game root, while fnTech3
  opens them below `botfiles` in both RTCW profiles. Only the source was
  read; the retail RTCW executables were not.
- **The server console.** fnTech3's `status` layout (`cl score ping name
  address rate steamid`) is not `etded.exe`'s (`num score ping name lastmsg
  address qport rate`). Its pending-command dump numbers entries from 0,
  where retail numbers them by their sequence.
