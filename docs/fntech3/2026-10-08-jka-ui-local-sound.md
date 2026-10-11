# Jedi Academy UI local sound (UI import 33), 2026-10-08

fnTech3's `jka-mp` client refused the UI module's import 33,
`UI_S_STARTLOCALSOUND`, by name, so the first menu sound would have stopped
the menus. Its client-game sibling, import 38, has been served since the
[live-join record](2026-10-06-jka-live-join.md). This slice serves import 33
through the same channel adapter and the same Fn local-sound path, and
reaches it without input from the UI's own `ui_openmenu` console command,
before and after the change, with both sound backends and with sound off.
The [JSON companion](2026-10-08-jka-ui-local-sound.json) holds the binary
identities, every address read, the menu counts, command lines and each
run's results. No gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jamp.exe` 1.01 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `base/assets3.pk3:uix86.dll` | `1179892f84f56805457455e0e01dbaab83f524180f2440b0f8157484eb711429` |
| `fntech3.exe`, before (runtime `jka-ui-sound-before`) | `1cdacf443e81f1961b7b5a52e018141a5151d3e030b742904b3b34a63c68cd47` |
| `fntech3.exe`, after (runtime `jka-ui-sound-after`) | `9e26146d89f3c6dd3483329a854a094cd2454071db89f5529711d134184a0229` |
| `soft_oal.dll`, OpenAL Soft 1.25.2 x86, beside both | `011d2cdb7cdc2a97858f414956955df5a2514fecefe1982928c4cd2b1a2e0c8a` |

The "before" runtime is a clean export of `6cc18d02` built as `win32-dev` is
(MSVC 2022 x86, Meson `debugoptimized`, strict warnings, every development
gate on). The "after" runtime is the same build with this slice's
`client_jka_ui_host.hpp`; only `cl_ui.cpp` recompiled, and the renderer
modules, engine package and web pak are the same files. The installation
was read only and its inventory was unchanged after every run.

## Import 33

**Observed** (retail, read with Capstone; no retail code is incorporated):

- `jamp.exe`'s UI dispatcher (0x423f50) bounds the import at 0xeb and jumps
  through a two-level table, `idx = byte [0x425384 + N]`,
  `target = dword [0x425150 + idx*4]`. Out-of-range numbers reach 0x425136,
  which drops with `Bad UI system trap: %i` (0x562b50). Import 33 alone lands
  at 0x424603: it passes `args[1]`, the handle, in a register and pushes
  `args[2]`, the channel, to `S_StartLocalSound` (0x46aed0), then returns
  zero, instruction for instruction as the client-game import 38 case at
  0x412934 does. Those two are the only direct callers of 0x46aed0, and no
  data refers to it.
- The retail UI module's wrapper for import 33 (0x40026e50) forwards both
  words unchanged. Four direct calls pass channel 1 (0x4000f592,
  0x400242a3, 0x400242de, 0x40024321). A run of function-pointer stores
  puts the wrapper in the display context's sound slot (0x4047d370, offset
  0x90 of the context at 0x4047d2e0; store at 0x40017658), and three calls
  go through it, read from the global context pointer (0x403ddcc4): the
  `play` script command (0x4001b2d0) registers the named sound through the
  context's register slot (offset 0xc8, the import-32 wrapper) and plays it
  on channel 0 whatever handle comes back; `setfocus` (0x40023c10) plays the
  context's item-focus sound (offset 0x160) on channel 8, only when that
  handle is nonzero; and a third call (0x40023f84) plays a focus sound on
  channel 8. A scan of every
  instruction with that offset, and of every reference to the slot, finds
  no other use. The retail UI therefore sends channels 0, 1 and 8.
- The UI's `ui_openmenu` console command (0x4000477c) closes every menu
  (0x400234e0, which first runs each visible menu's `onClose` script), opens
  the named menu (0x40025ec0, which runs its `onOpen` script) and takes the
  key catcher.

**Installation** (read from the paks): the 49 menus under `ui/jamp/` hold
711 `play` commands outside comments, in 45 menus: 678 in item actions, 29 in
`onESC`, two in `doubleClick` and two in `onClose`, those of
`videowarningMenu` and `videodriverMenu`, both
`play "sound/interface/button1.wav"`. None is in an `onOpen`. Five menus run
`setfocus` from their `onOpen`: `quitMenu`, `multiplayermenu`,
`findplayer_popmenu`, `createfavorite_popmenu` and `ingame_voicechat`.
`main.menu` and `ingame.menu` name `sound/interface/menuroam.wav` as the
item-focus sound; no item names a focus sound of its own.

**Reference** (OpenJK `1a6a643`, a maintained port): `codemp/ui/ui_shared.c`
names the three context calls `Script_Play` (`CHAN_AUTO`), `Script_SetFocus`
and `Item_SetFocus` (`CHAN_LOCAL_SOUND`); its `ui_main.c` and `ui_shared.c`
play the move previews' jump and roll sounds on `CHAN_LOCAL`; and
`codemp/ui/ui_atoms.c` lists `ui_openmenu`. They agree with the numbers
above; only these names are taken from it. Quake III Team Arena's
`ui_shared.c` (id's source release) plays its `play` command on
`CHAN_LOCAL_SOUND` instead.

**Inferred.** Because both imports reach `S_StartLocalSound` through the same
code, a menu sound meets everything the live-join record reads there and in
`S_StartSound`: nothing while sound is stopped or muted, an `ERR_DROP` on an
out-of-range handle, and the listener's own sounds below the ambient channel
7 at 0.75 of full volume (0x46ae4d to 0x46ae88; a local sound is always the
listener's own). With sound off, fnTech3's sound registration answers zero,
so `setfocus` makes no call; the sound-off run below agrees.

**Served.** Import 33 now takes import 38's path: a word outside the module's
`int` is refused, and the handle, the channel translated by
`client_jka_sound.hpp` and retail's level (0.75 below channel 7, otherwise
full) go to `S_StartLocalSoundVolume`, fnTech3's own local-sound entry; the
result is zero. The retail UI's channels 0, 1 and 8 arrive as the engine's
auto, local and local-sound channels, at 0.75, 0.75 and full level. The
canonical dispatcher, which would pass Academy's raw channel through, never
sees it.

**Differences recorded, not reproduced.** As for import 38: retail ends in an
`ERR_DROP` on an out-of-range handle while sound runs; fnTech3's backends
print a warning and play nothing. With sound disabled both engines play
nothing.

## Reaching it without input

No menu plays a sound as it opens by itself, and the remaining calls follow
the mouse or keys. The UI's own `ui_openmenu`, run from a startup cfg,
reaches import 33 both ways the retail code allows:

1. `ui_openmenu quitMenu`: the menu's `onOpen` runs
   `setfocus quitgame_cancel`, which plays the item-focus sound on channel 8
   when its handle is nonzero, so only with sound on.
2. `ui_openmenu videowarningMenu`, then `ui_openmenu mainMenu`: closing the
   warning runs its `onClose` `play`, which reaches import 33 on channel 0
   with sound off as well.

Each run starts an fnTech3 `jka-mp` client at its main menu on an unswitched
private desktop: windowed, `net_enabled 0`, no input injected, no server.
With OpenAL each step is followed by twelve `s_alDebugDump`s two frames
apart; the legacy run lists its voices with `s_show 2`. The private desktop
mutes the client, so nothing was heard.

| Run | Runtime | Sound | Result |
| --- | --- | --- | --- |
| `before-openal-1` | before | OpenAL Soft | step 1 stops: `Jedi Academy multiplayer UI import UI_S_STARTLOCALSOUND (33) is not served yet: menu service boundary is open` |
| `before-none-1` | before | off | step 1 passes, no focus sound being registered; step 2 stops with the same refusal |
| `after-openal-1` | after | OpenAL Soft | both steps pass; the client quits normally |
| `after-legacy-1` | after | legacy mixer | both steps pass; the client quits normally |
| `after-none-1` | after | off | both steps pass; the client quits normally |

**Observed** (fnTech3, after):

- OpenAL: step 1 starts `sound/interface/menuroam.wav` (the installation
  holds `menuroam.mp3`, which fnTech3's sound fallback loads) as a one-shot
  on the listener's entity 0, listener-relative at pan 0: Fn's `local` class
  with its high-pass tone, gain 1.00, dry 1.00 and no send. It holds for six
  dumps. Step 2 starts `sound/interface/button1.wav` the same way in Fn's
  `self` class, gain 0.75, dry 1.00, with a 0.17 send into the menu's
  environment (the generic zone's small-room reverb). It holds for all
  twelve dumps. OpenAL Soft ran with HRTF (`stereo-hrtf`) and its limiter.
- Legacy mixer: `s_show 2` lists `menuroam.wav` at 127/127 for 15 updates
  and `button1.wav` at 95/95 for 44. 95 is 0.75 of 127, as retail's 191 is
  of 255.

**Presentation (invariant 11).** The sounds play through fnTech3's mixers,
with Fn's channel rules for the translated engine channel, its source and
tone classes, the OpenAL backend's HRTF, limiter and output settings, or the
legacy mixer. Retail supplies the channel's meaning and the level; no retail
mixer is reproduced and no option replaces Fn's. One consequence is recorded,
not changed: Academy plays its `play` command on the auto channel where
Quake III Team Arena's UI uses the local-sound channel, so Academy's menu
clicks take Fn's `self` class, the listener's own sound inside the level,
and a little of the listener's room, while its focus sounds stay in the dry
`local` class. Whether every sound a UI module starts should be non-diegetic
is a choice for Fn's audio policy, left open here.

## Tests

- `tests/jka_ui_local_sound_tests.py` compiles the production import-33 case
  and the host's own refusal branch against a recording engine entry: every
  Academy channel's translation and level, both levels around channel 7,
  numbers no channel uses, the three channels the retail UI sends,
  unregistered handles, words wider than `int` (no word is wider on the
  x86 target, so a 64-bit build of the same case runs that refusal), and
  that no other profile resolves number 33, so the refusal branch takes it
  and no sound starts. A source test ties the case to the channel table and
  the host's single entry to the `jka-mp` guard in `cl_ui.cpp`. A raw
  channel, a full level, a missing width check and a truncating cast each
  fail it.
- The fixtures that slice the UI host (`jka_parser_global_tests.py`,
  `jka_ui_binding_tests.py`, `jka_ui_cvar_tests.py`,
  `jka_ui_metadata_tests.py`, `jka_ui_render_tests.py`,
  `jka_render_host_tests.py`, `jka_ui_ghoul2_tests.py`,
  `jka_module_refusal_tests.py`) and `jka_cgame_local_sound_tests.py` pass
  unchanged.

## What stays open

- **Audible play.** The clients were muted on the private desktop; the mix
  is shown by dumps and voice lists, not heard or captured.
- **Mouse and key paths.** Focus sounds as the pointer or keys move, and the
  move previews' channel-1 sounds, need input and were not run; they reach
  the same served case.
- **In a session.** The in-game menus' sounds were not run during a session
  against retail `jamp.exe`; the retail client still needs disc 1, so
  retail's own menu sounds were not observed for comparison.
- **Menu clicks' class**, the presentation choice above, is settled by the
  [menu sound record](2026-10-08-ui-sound-policy.md): every sound a menu
  module starts is interface audio, whatever channel it names.
