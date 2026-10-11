# Menu sounds as interface audio, 2026-10-08

The [Jedi Academy UI local sound record](2026-10-08-jka-ui-local-sound.md)
left a presentation question open: Academy's menus play their `play` script
command on the automatic channel, so Fn's tone policy gave those clicks the
`self` class, the listener's own sound inside the level, with a send into the
listener's room, while their focus sounds stayed in the dry `local` class.
This record surveys which channels every retail menu module hands the engine,
decides the policy across profiles, and implements it: a sound a game's menu
module starts through its local-sound import is interface audio on any
channel. Channel numbers, channel rules, module ABIs and the client games'
sounds are unchanged. The [JSON companion](2026-10-08-ui-sound-policy.json)
holds every module identity, address, call site, command line and run
result. No gate closes.

## Survey

**Observed** (retail modules, read only; DLLs with Capstone and pefile, QVMs
with a bytecode decoder; no retail code is incorporated). For each module the
local-sound import's syscall wrapper, table slot or struct copy is found and
every direct call listed with the channel it passes; every store of the import
into a display context is followed to the context's base (the address the
module's init hands to `Init_Display`), and every instruction using that slot
through the context pointer is read, since some calls build the slot's address
first (`lea esi, [ecx + 0x90]; call [esi]`).

| Profile | Module | Channels the menus pass (calls) | Fn's class before |
| --- | --- | --- | --- |
| `q3` | baseq3 `pak8.pk3:vm/ui.qvm` (q3_ui 1.32) | 6 (4), 1 (1), 7 (8), all direct | local, local, announcer |
| `q3` (Team Arena) | missionpack `pak3.pk3:vm/ui.qvm` (1.32); Steam `pak0.pk3` the same | 6 (3, display context +0x70), 7 (1), 1 (1) | local, announcer, local |
| `ql` | `bin.pk3:uix86.dll` | 6 (3, display context +0x74), 7 (4, import slot +0x88), 1 (1) | local, announcer, local |
| `rtcw-sp` | `uix86.dll` | 6 (3, display context +0x70), 7 (1), 1 (1) | local, announcer, local |
| `rtcw-mp` | `mp_bin.pk3:ui_mp_x86.dll` | 6 (3, display context +0x74), 7 (1), 1 (1) | local, announcer, local |
| `wolfet` | `mp_bin.pk3:ui_mp_x86.dll` | 6 (4, display context +0x88), 7 (1), 1 (1), every one at volume 127 | local, announcer, local |
| `stvef-sp` | `efuix86.dll` | 8 (11), 10 (13), 11 (6), 1 (2) in single player's numbering; the looping-sound import is never called | local (local and menu channels) |
| `stvef-mp` | `BaseEF/pak2.pk3:vm/ui.qvm` (1.20) | 6 (6), **8 (7)**, 1 (3), 7 (4), all direct | local, **self**, local, announcer |
| `jk2-mp` | `base/assets0.pk3:vm/ui.qvm` (1.02a) | 6 (2), **0 (1)**, display context +0x88 | none: fnTech3 refuses UI import 32 |
| `jka-mp` | `base/assets3.pk3:uix86.dll` (1.01) | 1 (4), 8 (2), **0 (1)**, in Academy's numbering | local, local, **self** |

- Quake III's, Team Arena's, Quake Live's, both RTCW's and Enemy Territory's
  menus pass only the local channel 1, the local-sound channel 6 and the
  announcer 7 (names from id's source releases and RTCW's and ET's, for names
  only): menu moves and clicks, the focus sound and `play` on 6; the player
  preview's weapon change (`sound/weapons/change.wav` in RTCW and ET) on 1;
  awards, the winner, the skill voices, a new high score and Quake Live's
  announcer previews (`vo_default`, `vo_female`, `vo_evil`) on 7.
- Elite Force single player's menus play on its local-sound channel (8), its
  two menu channels (10 and 11) and its local channel (1); fnTech3 already
  translates them to the engine's local-sound channel and to numbers of their
  own that its Elite Force voice path treats as interface channels.
- Holomatch's menus play sounds from seven call sites on 8, the first number
  past the announcer's, which no reference here names: the holodeck intro's
  computer voice (`sound/voice/computer/misc/intro_01.wav`), the logout voice,
  the sensor ping, two button sounds (one from two of the sites) and one
  handle taken from a table. Its main
  menu (instruction 27764) opens the intro the first time it runs and plays the
  voice there (28208, 28154), so it plays at every start with no input. Its
  client game's 43 local sounds (`pak2.pk3:vm/cgame.qvm`, import 29) use 6
  and 7 only, so 8 is the menus' own.
- Jedi Outcast's menus play `play` on 0 and both focus paths on 6; the
  module's own script table ties `play` (string 0x81b6) to procedure 43651 and
  `setfocus` to 43312. `jk2mp.exe` serves UI import 32 at 0x40ffb0 by handing
  the handle and channel to `S_StartLocalSound` (0x45d370), and its channel
  picker (0x45d090) treats only channel 0 apart, so 6 means what Outcast's
  modules agree it means. fnTech3 does not serve the import yet.
- Jedi Academy's are the import-33 record's, re-read with this survey's
  scanner: `play` on 0, the automatic channel; the two focus paths on 8,
  Academy's local-sound channel; the move previews on 1.
- The other profiles' menus are engine code, and fnTech3's own: the Q3Test
  releases' UI import takes a sound by name and fnTech3 plays it on channel 1
  (and 1.05's engine menus the same); Alice's and F.A.K.K.2's menus reach
  Alice's menu channel at the listener, an interface role in their own voice
  path; Medal of Honor's menus play on the local-sound channel. The IHV
  Test's menus and both Jedi single-player menus are not served.

So every retail menu sound already played as interface audio in Fn except two
groups, Academy's `play` and Holomatch's channel 8, and Outcast's `play` would
join them once its import is served.

## Decision

**Adopted.** A sound a game's menu module starts through its local-sound
import is interface audio whatever channel it names: Fn's `local` class, dry,
with no send to the listener's room, no shaping by water or occlusion,
head-relative at the listener. The announcer channel keeps the announcer
class, already outside the level, and authored stereo or surround keeps its
own direct path, unsent.

Invariant 11 makes this Fn's call. Retail pins what the module hands over --
the channel, which decides which sound replaces which on the listener's
entity, and the level -- and fnTech3 keeps both exactly. How the engine
presents a menu sound is Fn's, and Fn's tone policy already says what a menu
sound is: its `local` class is the non-diegetic class, "UI, feedback, and
chat that plays outside the level" (`AudioTonePolicy.h`). The policy inferred
"menu" from the channel, which holds for Quake III's numbering and fails for
two retail menus that pick other numbers. The origin of the sound is the
direct fact, so the policy now takes it from there.

Two alternatives were set aside. Treating the automatic channel at the
listener as local everywhere would also move the client games' own sounds at
the listener, which Fn deliberately keeps inside the level (the `self` class,
muffled under water with everything else). Translating Academy's or
Holomatch's menu channels to a local channel would change which sound
replaces which, a channel rule retail pins.

## What changed

- **Policy.** `AudioTonePolicy.h` takes an `interfaceSound` input; for it
  `ClassifyToneClass` returns `local`, or `announcer` on the announcer
  channel, after the authored-stereo check and before the channel class. No
  other input's result changes.
- **OpenAL voices.** A voice carries the mark from its start: the front end
  marks the next one-shot, `PlayOneShot` takes the mark (one mark, one
  one-shot) and a voice released for reuse drops it; a reset or a stop of
  every sound clears a mark no one-shot took. `ApplyVoice` plays a marked
  voice as local-only on any channel, even after the listener has moved to
  another entity, and Elite Force single player's own voice path hears it at
  the listener as an interface sound.
- **Front end.** `S_StartInterfaceSound` marks, starts the profile's own local
  sound -- RTCW's, the Ritual menu channel or the generic one, at the level
  given -- and unmarks; `S_StartWolfETInterfaceSound` does the same with
  Enemy Territory's volume and refuses every other profile. Both backends
  provide the mark; the legacy mixer's does nothing, as it has no classes or
  room sends.
- **Menu hosts.** The shared UI dispatcher (Quake III, Quake Live,
  Holomatch), RTCW's and Enemy Territory's audio adapters (their UI cases
  only), Academy's import-33 case (keeping its channel table and level),
  Elite Force single player's menu import and Q3Test's start menu sounds
  through those entries. No client game's local sound does.

**Differences recorded, not reproduced.** Retail engines mix a menu sound
like any other sound at the listener; no retail mixer reproduces Fn's
classes, and no retail capture was taken to compare.

## Live check

Both affected profiles reach their sounds with no input. Academy's runs use
the import-33 record's steps unchanged: `ui_openmenu quitMenu` plays the
focus sound on channel 8, and closing `videowarningMenu` plays `button1.wav`
through `play` on channel 0. Holomatch's client plays the intro voice on
channel 8 as its main menu opens; its cfg only dumps the voices. Each run is
an fnTech3 client at its main menu on an unswitched private desktop: windowed,
`net_enabled 0`, no server, an empty config in its own home. The private
desktop mutes the client, so nothing was heard.

| File | SHA-256 |
| --- | --- |
| `fntech3.exe`, before (`.tmp/runtime/jka-ui-sound-after`, the import-33 record's own after runtime) | `9e26146d89f3c6dd3483329a854a094cd2454071db89f5529711d134184a0229` |
| `fntech3.exe`, after (`.tmp/runtime/ui-sound-policy-after`) | `8bea54d7a1205e197c8c561ede63c3f101c4e858a58a709c87cf701098aae3ef` |
| `soft_oal.dll`, OpenAL Soft 1.25.2 x86, beside both | `011d2cdb7cdc2a97858f414956955df5a2514fecefe1982928c4cd2b1a2e0c8a` |

The before runtime is `6cc18d02` with the uncommitted import-33 slice; the
after runtime is a clone of that slice's export with only this record's edits,
built with win32-dev's options (MSVC 2022 x86, `debugoptimized`, strict
warnings, every development gate). Only `fntech3.exe` was rebuilt; the
renderer modules, engine package, web pak and OpenAL Soft are the before
runtime's files. Both installations were read only and unchanged after every
run.

**Observed** (OpenAL Soft with HRTF and its limiter; the menu's environment is
the generic zone's small room, wet 0.30 once blended in):

| Sound | Before | After |
| --- | --- | --- |
| Academy `button1.wav` (`play`, channel 0) | `self`, gain 0.75, send 0.17 (12 dumps) | `local`, gain 0.75, no send (12 dumps) |
| Academy `menuroam.wav` (focus, channel 8) | `local`, gain 1.00, no send | the same |
| Holomatch `intro_01.wav` (channel 8) | `self`, gain 1.00, send 0.10 to 0.17 as the room blends in (21 and 39 dumps in two runs) | `local`, gain 1.00, no send (21 and 39 dumps) |
| Holomatch `button2.wav` (the intro's first frame) | `local`, gain 1.00, no send | the same |

The legacy mixer plays every one at the same levels before and after
(`s_show 2`: menuroam 127/127, button1 95/95, the intro voice and button2
127/127), and both clients run their menus with sound off. Every run exited by
its own `quit`.

## Presentation (invariant 11)

Menu sounds play through Fn's mixers: the OpenAL backend's interface class,
HRTF, limiter and output settings, or the legacy mixer, the retail-exact
option. They take no room reverb, audio zone, occlusion, distance model or
water shaping, by design: those belong to sounds inside the level, and a menu
plays outside it. The channel and the level the module passes are kept; no
retail mixer is reproduced and no option replaces Fn's.

## Tests

- `tests/ui_interface_sound_tests.py` runs the real policy over every channel
  class, sample prefix and placement, with and without the mark; the real
  `ApplyVoice` on every engine channel, Holomatch's 8 and Academy's own
  numbers, for Quake III's line and Wolfenstein's one-shots, at both of
  Academy's levels, after the listener has moved, under water, for authored
  stereo and muted; the real `ApplySTVEFVoice`; the real `PlayOneShot` taking
  one mark for one one-shot; the real front-end entries for each profile's
  route and refusal; and checks that every menu host, and no client game,
  starts its sounds through them, and that no other code does.
- `tests/audio/tone_policy_tests.cpp` (meson's `fnql_audio_tone_tests`) adds
  the policy case.
- The fixtures that compile the voice paths or pin the changed lines follow
  the change: `wolfenstein_openal_voice_tests.py`,
  `stvef_sp_openal_voice_tests.py`, `rtcw_one_shot_tests.py`,
  `rtcw_music_tests.py`, `stvef_sp_cgame_tests.py` and the import-33 slice's
  `jka_ui_local_sound_tests.py`, which now records the interface entry.

## What stays open

- **Audible play.** The clients were muted on the private desktop; the mix is
  shown by dumps and voice lists, not heard or captured.
- **Paths that need input.** Holomatch's other channel-8 sounds, and every
  click or focus sound that follows the pointer or keys, reach the same
  served path but were not run; menus opened during a session or under water
  are covered by the unit tests only.
- **Jedi Outcast multiplayer.** Its UI import 32 is still refused. When it is
  served, it should start its sounds through `S_StartInterfaceSound` with
  Outcast's own channel numbers.
- **Profiles that did not change.** Every retail menu sound of Quake III, Team
  Arena, Quake Live, both RTCW modes, Enemy Territory and Elite Force single
  player was already interface audio; their unchanged classes are shown by
  the unit tests, not by new runs.
