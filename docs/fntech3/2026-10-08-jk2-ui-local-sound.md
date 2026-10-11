# Jedi Outcast UI local sound (UI import 32), 2026-10-08

fnTech3's `jk2-mp` client refused the UI module's import 32,
`UI_S_STARTLOCALSOUND`, by name, so the first menu sound stopped the menus.
This slice pins how Outcast multiplayer numbers its sound channels, serves
the import through Fn's entry for a menu's sound, the interface path of the
[menu sound record](2026-10-08-ui-sound-policy.md), and reaches it without
input from the retail UI's own console commands, before and after the
change, with both sound backends and with sound off. It also finds that
fnTech3 cannot decode Outcast's sounds at all, an open blocker outside this
slice. The [JSON companion](2026-10-08-jk2-ui-local-sound.json) holds the
binary identities, every address read, the module call sites, command lines
and each run's results. No gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jk2mp.exe` 1.02a | `2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a` |
| `base/assets0.pk3:vm/ui.qvm` | `19e372d0381b8b53965bd0fc38cae0eef266b3aab68e9088f915894fadca3a4f` |
| `base/assets0.pk3:vm/cgame.qvm` | `82c6543ad48508e153672673b02f94a7e264a91a8be9e01e83c46687a0033466` |
| `base/assets1.pk3` (the menus) | `c3a9aeaf09c93e57847290e7e5cd6c1a071a560045fa8c5e8c6df3688df841c1` |
| `fntech3.exe`, before (runtime `jk2-ui-sound-before`) | `d6d0bf6c9de6c97c5f06fefc5e214198f90b5db8ac622ffaa17b93a13ea6a652` |
| `fntech3.exe`, after (runtime `jk2-ui-sound-after`) | `7b095d422a8dba82da1123bbaa920f0549c26f440cc57b51ca1a8861de1d98be` |
| `soft_oal.dll`, OpenAL Soft 1.25.2 x86, beside both | `011d2cdb7cdc2a97858f414956955df5a2514fecefe1982928c4cd2b1a2e0c8a` |

Outcast's menu reaches these steps only with other sessions' uncommitted
work: its menu rendering and string services (the second-round audit,
`2026-10-07-jedi-compatibility-round2.md`) and the menu sound policy's
`S_StartInterfaceSound`. So the "before" runtime is a byte
copy of the shared working tree on `cac46a13`, all of that included, built
as `win32-dev` is (MSVC 2022 x86, Meson `debugoptimized`, strict warnings,
every development gate on). The "after" runtime is the same build with this
slice's `client_jk2_ui.hpp`; only `cl_ui.cpp` recompiled, and the renderer
module, engine package and web pak are the same files. The installation was
read only and its inventory was unchanged after every run.

## Import 32

**Observed** (retail, read with Capstone; no retail code is incorporated):

- `jk2mp.exe`'s UI dispatcher (0x40fb70) bounds the import at 0xca and jumps
  through `idx = byte [0x410894 + N]`, `target = dword [0x4106fc + idx*4]`.
  Import 32 lands at 0x40ffb0: it pushes `args[2]`, the channel, and
  `args[1]`, the handle, calls `S_StartLocalSound` (0x45d370) and returns
  zero. Client-game import 33 (0x401c7f, dispatcher 0x4017a0) is the same
  hand-off; client-game 32 (0x401c56) is `S_StartSound` (0x45d230).
- `S_StartLocalSound` does nothing while sound is stopped or muted
  (`[0x1a20c80]`, `[0x1a20c88]`). For a handle outside the registered range
  (`[0x1a3efb8]`) it prints and returns, but the call passes `"^3"`
  (0x4fdc40) as `Com_Printf`'s (0x427cf0) format and the message
  `S_StartLocalSound: handle %i out of range` (0x4fdc44) as an unused
  argument, so only a colour code is printed. Otherwise it calls
  `S_StartSound` with no origin, the listener (`[0x1a20cb4]`), the channel
  and the handle.
- `S_StartSound` stores one level, 240 (0xf0), as the master level and both
  side levels of every sound it starts (0x45d315 to 0x45d333), whatever its
  channel or entity, and respatializing keeps a channel on the listener's
  entity at that level on both sides (0x45dc2c to 0x45dc3a). Its channel
  picker (0x45d090) drops on a negative channel (`ERR_DROP`,
  `S_PickChannel: entchannel<0`), lets channel 0 never replace a sound and
  any other channel replace the same entity's sound on that channel; no other
  number has a rule. Loops are placed at 240 or 180 and clamped at 255; the
  mixer scales by `s_volume` (default 0.8) times 256.

**Channel numbering.** Since the engine gives no number but 0 a meaning, the
numbers mean what Outcast's modules use them for. Its retail client game
makes 17 local-sound calls (import 33, read with a QVM decoder): ten on 7,
every one an announcer sound -- Mon Mothma's lines, the warmup countdown's
three among them (procedure 21576, beside `WAITING_FOR_PLAYERS`), the
round-over and objective sounds and the buffered announcements; two on 6,
both `sound/player/talk.wav`, for the `chat` and `tchat` server commands; two
on 1 (`music/goodsmall.mp3`, `music/badsmall.mp3`), one on 3 (voice chat)
and two on 0 (the disruptor's zoom). Its 75 positional sound calls name
channels 0 to 5 only, `sound/player/roll1.wav` on 5 (four of them through a
temporary set to 0 or 2), apart from two that pass a field of the event's
entity state. That is Quake III's numbering (id's source release, for names
only): 6 its `CHAN_LOCAL_SOUND`, "chat messages, etc", 7 its
`CHAN_ANNOUNCER`, 5 its `CHAN_BODY`. The
single-player numbering Academy uses would put the chat beep on the body
channel and the announcer on the ambient one. Outcast's menus play `play` on
0 and both focus paths on 6, through their display context's slot +0x88 (the
menu sound record's survey). So Outcast's numbers are the engine's and, unlike
Academy's, need no translation.

**Level.** Retail starts every one-shot at the same level and gives the
listener's own sounds none of their own, where Academy plays them at 0.75
below its ambient channel. A menu sound therefore plays at the profile's
ordinary one-shot level, Fn's full level 1.0; 240 against the clamp's 255 is
the retail mixer's own scale, which Fn's mixers do not reproduce.

**Served.** Import 32 now refuses a word outside the module's `int` and
otherwise hands the handle, the module's channel unchanged and the full
level to `S_StartInterfaceSound`, which starts Fn's local sound on that
channel and plays it as interface audio; the result is zero. The shared UI
dispatcher never sees the number.

**Differences recorded, not reproduced.** An out-of-range handle plays
nothing in either engine; Fn's backends print their yellow
`S_StartLocalSound: handle %i out of range` where retail's call prints only
the colour code. A negative channel drops retail's module when the sound
would play; Fn plays it on that number, which takes no engine channel's
rule, as its Quake III and Academy paths do. No retail module passes one.

## Outcast's sounds do not decode in fnTech3

The installation stores its sounds as MP3: 7,987 of the 8,296 files under
`sound/`, the menus' `button1` and `menuroam` among them, and all 139 under
`music/`. `S_CodecInit` (`code/client/audio/codecs/snd_codec.cpp`) registers
the MP3 codec for Elite Force, the Ritual titles, Medal of Honor and Academy
only, so in both Outcast profiles a `.wav` name never falls back to its
`.mp3`: `menuroam.wav`, `button1.wav` and `feedback/hit.wav` fail to load and
register as 0. The focus sound's handle is then 0, so `setfocus` makes no
call, and `play` hands import 32 handle 0, which Fn plays as its generated
fallback tone. Retail plays these sounds, which exist only as MP3, and its
engine carries MP3 decoding state (`S_StartSound` copies a block from the
sound into its channel, 0x45d336 to 0x45d35c; loop setup refuses "streamed
MP3 files" by name, 0x45d7ba). This is an open blocker for every Outcast
sound and is not changed here. To show both retail channels reaching the
served case, a diagnostic pair of runtimes adds one line registering the
codec for both Outcast profiles; that line is not part of this change.

## Reaching it without input

Outcast's UI has no Academy-style `ui_openmenu`: its console handler
(procedure 33186) opens a menu by name only while `developer` is set
(instructions 33279 to 33299) and closes nothing first. Two of its own
commands reach import 32 from one startup cfg:

1. `ui_openmenu quitMenu`: the menu's `onOpen` runs
   `setfocus quitgame_cancel`, which plays the item-focus sound
   (`sound/interface/menuroam.wav`, from `main.menu`'s asset definitions) on
   channel 6 when its handle is nonzero.
2. `ui_openmenu videowarningMenu`, then `ui_test` and `ui_openmenu main` in
   one frame: `ui_test` shows the post-game menu, whose branch (29340 to
   29356) first closes every menu, running each visible menu's `onClose`, so
   the warning's `play "sound/interface/button1.wav"` reaches import 32 on
   channel 0 whatever handle the registration returned. Outcast ships no
   `endofgame` menu, so `ui_openmenu main` reopens the main menu. The UI does
   not claim `ui_test`, and the console also prints `Unknown command`.

**Installation** (the 35 menus under `ui/jk2mp/`, comments stripped): 552
`play` commands, 532 in item actions, 17 in `onESC`, one in a `doubleClick`
and two in `onClose`, those of `videowarningMenu` and `videodriverMenu`, both
`button1.wav`. `quitMenu`, `creditsMenu` and `findplayer_popmenu` run
`setfocus` from their `onOpen`.

Each run starts an fnTech3 `jk2-mp` client at its main menu on an unswitched
private desktop: windowed, `developer 1`, `net_enabled 0`, no input
injected, no server, an empty config in its own home. With OpenAL each step
is followed by twelve `s_alDebugDump`s two frames apart; the legacy runs list
voices with `s_show 2`. The private desktop mutes the client, so nothing was
heard.

| Run | Runtime | Sound | Result |
| --- | --- | --- | --- |
| `before-openal-1` | before | OpenAL Soft | step 1 passes, its focus handle being 0; step 2 stops: `Unsupported jk2-mp UI import 32: Jedi Outcast's menu service boundary is open.` |
| `before-legacy-1` | before | legacy mixer | the same |
| `before-none-1` | before | off | step 2 stops with the same refusal |
| `after-openal-1` | after | OpenAL Soft | both steps pass; the client quits normally |
| `after-legacy-1` | after | legacy mixer | both steps pass; the client quits normally |
| `after-none-1` | after | off | both steps pass; the client quits normally |
| `diag-before-openal-1` | MP3 diagnostic, before | OpenAL Soft | step 1 stops with the same refusal |
| `diag-after-openal-1` | MP3 diagnostic, after | OpenAL Soft | both steps pass; the client quits normally |
| `diag-after-legacy-1` | MP3 diagnostic, after | legacy mixer | both steps pass; the client quits normally |

**Observed** (fnTech3, after):

- OpenAL: step 2 starts handle 0, `sound/feedback/hit.wav`, which did not
  load either, so Fn's fallback tone, as a one-shot on the listener's entity
  0, listener-relative at pan 0: Fn's `local` class with its high-pass tone,
  gain 1.00, dry 1.00 and no send, in 9 of the 12 dumps.
- OpenAL, MP3 diagnostic: step 1 starts `menuroam.wav` (its `.mp3`) on
  channel 6 and step 2 `button1.wav` on channel 0, each the same way: `local`
  class, gain 1.00, dry 1.00, no send, for 7 and 9 dumps. The menu's
  environment is the generic zone's small room (wet 0.30), of which neither
  takes any.
- Legacy mixer: `s_show 2` lists the menu sounds at 127/127, full level
  (diagnostic: `menuroam` for 16 updates, `button1` for 20; real: the default
  sound for 5).
- Sound off: both steps run and nothing plays.

## Presentation (invariant 11)

Menu sounds play through Fn's mixers: the OpenAL backend's interface class,
HRTF, limiter and output settings, or the legacy mixer, the retail-exact
option. They take no room reverb, audio zone, occlusion, distance model or
water shaping, by design, as every game's menu sounds do under the menu
sound record. The channel and the level the module passes are kept; no
retail mixer is reproduced and no option replaces Fn's.

## Tests

- `tests/jk2_ui_local_sound_tests.py` compiles the production import-32 case
  and the host's own refusal branch against a recording interface entry:
  every engine channel and numbers no channel uses, arriving unchanged at
  level 1.0 with a zero result; the retail UI's channels 0 and 6;
  unregistered handles; words wider than `int` (no word is wider on the x86
  target, so a 64-bit build of the same case runs that refusal); and other
  numbers still refused. A source test ties the case to the interface entry
  and the host's single entry to the `jk2-mp` guard in `cl_ui.cpp`. Seven
  mutations -- Academy's channel table, a 0.75 level, retail's 240/255, no
  width check, a truncating handle cast, a nonzero result and routing 32
  through the shared dispatcher -- each fail it.
- `tests/ui_interface_sound_tests.py` adds Outcast's case to the menu hosts
  that start their sounds as the interface's.
- The fixtures that slice or stub the Outcast UI host
  (`jk2_ui_definition_tests.py`, `jk2_ui_render_tests.py`,
  `rtcw_cinematic_tests.py`, `rtcw_module_cvar_tests.py`,
  `rtcw_render_import_tests.py`) and `jka_ui_local_sound_tests.py` pass
  unchanged, and `cl_ui.cpp` compiles with `win32-dev`'s own command.
- The whole Python suite on the working-tree copy with only this slice added:
  2,648 passed, 129 skipped and one failed,
  `rtcw_speech_tests.py`'s default-handle case, which fails the same way
  without this slice: its fixture does not stub another uncommitted change's
  Academy lip-sync lines in the OpenAL sample loader.

## What stays open

- **Outcast's sounds.** Neither Outcast profile decodes the installation's
  MP3 sounds, so no Outcast sound but its few `.wav` files plays.
- **Audible play.** The clients were muted on the private desktop; the mix
  is shown by dumps and voice lists, not heard or captured.
- **Mouse and key paths.** Focus sounds as the pointer or keys move need
  input and were not run; they reach the same served case.
- **In a session.** The in-game menus' sounds were not run during a session,
  and retail `jk2mp.exe`'s own menu sounds were not observed for comparison.
- **The client game's sounds.** Outcast's client-game sound imports (31 to
  41) are still refused; the channel numbering pinned here is theirs too.
