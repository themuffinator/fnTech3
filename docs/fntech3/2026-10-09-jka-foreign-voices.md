# Jedi Academy's German, French and Spanish voices, 2026-10-09

Jedi Academy ships German, French and Spanish recordings of its characters'
voices beside the English ones, and both of its retail engines play them when
`s_language` asks for them. fnTech3 registered `s_language` for neither
Academy profile and never looked for them, so every setting played the
English voices. This record pins how `jamp.exe` and `jasp.exe` choose a
voice's language, registers `s_language` for `jka-sp` and `jka-mp` as they do,
and loads each voice from `sound/chr_d`, `sound/chr_f` or `sound/chr_e` by
their rule, falling back to the English recording. Live, without input, a
multiplayer client at its main menu loads Kyle's lines and three other voices
in German, French and Spanish, falls back to English where a language has no
recording, loads a taunt of Tavion's that was recorded only in the three other
languages, and changes language through the setup menu's own commands; the
build before the change played English throughout. It is the Academy
counterpart of the [Outcast foreign voices record](2026-10-08-jk2-foreign-voices.md).
The [JSON companion](2026-10-09-jka-foreign-voices.json) holds every address,
count, command line and run result. No gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jamp.exe` 1.01 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `jasp.exe` 1.01 | `9351a8de230fcd577e2b474889d08f38982450306767a6823dd659482566d6ce` |
| `base/assets0.pk3` | `ffea9e7489bad7175411d122e0652b4272efb67c0e71805d1f4d8058084b5939` |
| `base/assets1.pk3` | `5f196826e226b1115bd46f54697334ca970c8261dd76864c1600179ff38e4c4f` |
| `base/assets2.pk3` | `8f21cdd89afe78fdf916d103117f4b2621c6f4781dee003716057dc705ef98af` |
| `base/assets3.pk3` | `0f542dccb6858fc10a8083ca71f581c9eba93173386e03a9e5e05d3a56118f78` |
| `fntech3.exe`, before (runtime `jka-voices-before`) | `a83dd8fc84f54cfdcd66e77f9435e6d4bc5daa94ee525c35b8b6709a821ed8ed` |
| `fntech3.exe`, after (runtime `jka-voices-after`) | `13d16f659039fc830173f30399f073f68187475c6efc42e1544b36bb071bcc63` |
| `soft_oal.dll`, OpenAL Soft 1.25.2 x86, beside both | `011d2cdb7cdc2a97858f414956955df5a2514fecefe1982928c4cd2b1a2e0c8a` |

Both runtimes come from one `git archive` export of `18e6b541`, built as
`win32-dev` is (MSVC 2022 x86, Meson `debugoptimized`, strict warnings, every
development gate on). The "before" runtime is that export as taken; the
"after" runtime is the same export with this change's two engine files, whose
objects alone recompiled. The renderer module, engine package and web pak are
the same files in both. The installation was read only and its inventory was
unchanged after every run.

## The installation

**Observed** (the four paks in `base`, read with Python's `zipfile`; folder
entries are not counted):

- `base/assets0.pk3` holds every voice. `sound/chars/` has 3,576 files, 3,531
  MP3 and 45 WAV; `sound/chr_d/` 2,873 and `sound/chr_f/` 2,877, all MP3;
  `sound/chr_e/` 2,873, 2,870 MP3 and 3 WAV. No other pak holds any, and
  none is loose. Every entry's name is in lower case, and no voice exists as
  both a `.wav` and an `.mp3` in any folder.
- 2,868 English voices have a German recording, 2,871 a French one and 2,867
  a Spanish one; 2,857 have all three and 695 have none, among them 42 of the
  45 English WAVs. Every foreign recording of an English MP3 is an MP3. The
  three Spanish WAVs are Boba Fett's jetpack sounds
  (`boba/bf_jetpack_lp`, `jethover`, `jeton`), byte-for-byte copies of his
  English WAVs; the single-player game (`jagamex86.dll`) asks for the first
  as a `.wav`, and multiplayer's client game, game and menus the other two.
- Some recordings exist in other languages only: `alora/misc/taunt`,
  `kothos/misc/taunt`, `rockettrooper1/misc/falling1`, `tavion/misc/taunt`
  and `worker2/misc/giveup4` in all three, `worker2/misc/confuse3` in French
  only and `sab1/08is3002` in Spanish only. Twenty-four English voices have
  some of the three: `worker1`'s eight pain, choke and death sounds and
  `worker2/misc/confuse1` and `confuse2` have German and French but no
  Spanish, `stofficer1/misc/cover5` German and Spanish but no French, nine
  (five of Boba Fett's jetpack sounds, two protocol droid footsteps and two
  rebel pilot taunts) Spanish alone and four stormtrooper body falls French
  alone.
- The only other file whose name contains `chars` is `gfx/2d/charsgrid_med.tga`,
  not a sound.
- The setup menus of both modes set the voice language through `s_language`:
  single player's `ui/setup.menu` and `ui/ingamesetup.menu`, multiplayer's
  `ui/jamp/setup.menu` (in `assets1` and, revised, in `assets3`) and
  `ui/jamp/ingame_setup.menu`. English writes `english`, the French entry
  `francais`, Deutsch `deutsch` and Español `espanol`. Multiplayer's then
  shows a Set Language button that runs `ui_load ; snd_restart`; single
  player's in-game one runs `snd_restart`. The text language is a separate
  setting, `se_language`. The installation's own `base/jaconfig.cfg` sets
  both to `english`.
- There is no `video/jk05.roq` or `video/jk06.roq` and no
  `video/cinematic_5` or `cinematic_6` recording in any language.

## How retail picks a voice's language

**Observed** (`jamp.exe` and `jasp.exe`, read with Capstone; no retail code is
incorporated). Addresses are multiplayer's, then single player's.

1. **Registration.** Each `S_Init` (0x469420, 0x425860) registers
   `s_language` as `english` with flags 0x401 (0x4695cb, 0x425a2f), before its
   `s_initsound` check (0x46962d, 0x425a73), so it exists with sound turned
   off. 0x1 is archived: each config writer saves the cvars with that bit
   (0x439e30, 0x41c4a0). 0x400 is Quake III's `CVAR_NORESTART`: each
   `cvar_restart` handler (0x43a010, 0x41c660) leaves alone the cvars with
   0x450 set. fnTech3's `CVAR_ARCHIVE` and `CVAR_NORESTART` have the same
   values. Single player's built-in menus register it again with the same
   default and flags when they start (0x494ae0, at 0x494b6f); multiplayer's
   menu module never names it.
2. **Every sound effect passes one loader.** Every caller that loads a
   sound's data, `S_RegisterSound`, `S_StartSound`, `S_StartAmbientSound`,
   `S_AddLoopingSound` and the loop-sound pass among them, does it through
   one helper (0x46a0e0, 0x42e410), whose
   call (0x46a0ed, 0x42e41b) is the only one of `S_LoadSound` (0x4726c0,
   0x42df70); `S_LoadSound`'s call (0x47278f, 0x42e031) is in turn the only
   one of the file chooser (0x4723e0, 0x42dc80). Music never reaches it.
3. **Lower case first.** `S_LoadSound` refuses a name shorter than five
   characters or starting with `*`, copies it into 64 bytes and lowercases
   the copy with the C library's `_strlwr` (called at 0x472726, 0x42dfc0).
   When the copy's fourth character from the end is not a `.`, it appends
   `.wav` (0x444350, 0x422a40), so the name always ends in a three-letter
   extension.
4. **The rule.** The chooser finds the first `chars` in that copy with the C
   library's case-sensitive `strstr` (0x52a630, 0x4df020), so, the name being
   lowercased already, a `chars` in any case. When there is one, it compares
   `s_language` with `DEUTSCH`, `FRANCAIS` and then `ESPANOL` through
   `_stricmp`, so without regard to case (multiplayer at 0x47253d, 0x47256d
   and 0x47259d; single player at 0x42ddd2, 0x42de02 and 0x42de32), and
   overwrites those five letters with `chr_d`, `chr_f` or `chr_e`. Any other
   value, or no `s_language`, leaves the name alone (0x4725bb, 0x42de50). It
   then reads the name as it stands, then with `mp3` written over its last
   three letters; only when a substituted name finds neither does it put
   `chars` back and read the English `.wav`, then `.mp3` (0x47260c to
   0x47265b, 0x42dead to 0x42defe). Nothing is printed when a recording is
   missing: neither engine has a message for it.
5. **Data builds.** With `com_buildScript` set (cvar pointers 0xb43ce4,
   0x88fef0), the chooser also opens and closes the German, French and
   Spanish file of every voice, `.wav` or else `.mp3`, putting `.wav` back
   after each language and `chars` back at the end (0x472402 to 0x472524,
   0x42dc9e to 0x42ddb9). Unlike Outcast's multiplayer engine, neither leaves
   a name ending in `.mp3` behind.
6. **The English name decides the rest.** Whether a long MP3 stays
   compressed is decided from the sound's own, English name: a
   `sound/chars/` prefix compared without case (0x472680, passed the
   English name at 0x4727fe; 0x42df20), and so is whether it gets lip-sync
   data (0x47283a; 0x42e0d5). A foreign recording is treated as its English
   name says.
7. **Other readers.** Multiplayer reads `s_language` nowhere else: its cvar
   pointer (0xc717bc) is read only by the chooser, and the name is pushed
   only by `S_Init`. Single player's cinematic player (0x404e80) gives
   `video/jk05.roq` and `video/jk06.roq` the voice track
   `sound/chars/video/cinematic_5` or `cinematic_6` when `s_language` is set
   and neither `english` nor empty (0x404fdf to 0x4050d9): Outcast's code,
   for videos and recordings Academy does not have, so it never plays.
   It registers that track through `S_RegisterSound` (0x40464a), so the rule
   above would apply to it. Unlike Outcast's single player, `jasp.exe` has no
   German dismemberment rule: its handler for the server's settings
   (0x40ad60) reads `sv_serverid` and `helpUsObi` and sets each setting as a
   cvar, nothing more; besides the chooser and the cinematic player nothing
   reads the cvar (pointer 0x8d24b4), and no retail module names `s_language`.

Outcast's rule (the record linked above) is the same search, comparison,
order and silence. Academy adds Spanish, lowercases the name in both engines
where Outcast's multiplayer engine does not, reads the name's own extension
first where Outcast reads `.wav` first whatever the name says, and puts
`.wav` back in its build pass. So Outcast's helper is not shared.

## fnTech3 before

At `18e6b541`, `S_Init` registered `s_language` for Elite Force alone (the
uncommitted Outcast slice adds Jedi Outcast). In `jka-mp` the menu's voice
item or a `+set` created it as a user cvar, whose default is the value that
created it; in `jka-sp` nothing did. Neither profile's sound loading read it,
so every setting played the English voices, and the recordings that exist
only in other languages never loaded.

## The change

- `S_Init` (`code/client/audio/snd_main.cpp`) registers `s_language` for both
  Academy profiles as both retail engines do: `english`, `CVAR_ARCHIVE |
  CVAR_NORESTART`, before the `s_initsound` check. Elite Force and Outcast
  keep their own registrations; no other profile registers it.
- `S_CodecLoad` (`code/client/audio/codecs/snd_codec.cpp`), which both sound
  backends use to load every sound effect, has a rule of its own for the two
  Academy profiles, `S_JKAForeignVoice`. When `s_language` is `deutsch`,
  `francais` or `espanol`, in any case, it turns the first `chars` in the
  sound's name, in any case, into `chr_d`, `chr_f` or `chr_e` and loads that
  name first, without a warning, through the codec list: the extension the
  name asks for, then the others, `.wav`, `.mp3` and Fn's Ogg Vorbis. Only
  when that finds nothing does it load the English name as before, which
  warns when it is missing too. Music is untouched, as in retail. Every other
  profile, Outcast's and Elite Force's rules included, loads exactly as
  before.

**Differences recorded, not reproduced:**

- A name asked for as `.mp3`: retail reads the foreign `.mp3` twice, then the
  English `.wav` and `.mp3`; Fn reads the foreign `.mp3`, `.wav` and `.ogg`,
  then the English `.mp3`, `.wav` and `.ogg`. The two differ only for a
  voice stored in both formats, or a foreign `.wav` asked for as `.mp3`. No
  installed voice is stored in both, and the three Spanish WAVs are asked
  for as `.wav`, so every installed name loads the same file.
- Fn finds `chars` in any case without lowercasing the rest of the name, as
  its English loads keep the name's letters too; lookups in the paks ignore
  case, so the same file loads. Fn's OpenAL backend lowercases every name it
  registers anyway; the legacy mixer keeps the name as given.
- The Ogg Vorbis attempt after the `.mp3` is Fn's own, as for every name; no
  Academy voice is Ogg Vorbis.
- The data-build pass of step 5 is not reproduced: it only lets a
  data-building run touch the foreign files, and nothing in fnTech3 builds
  Academy's data.
- Diagnostics: with `developer` set, each voice loaded from a foreign folder
  shows in Fn's `Sound fallback: ...chr_e/x.wav -> ...chr_e/x.mp3` note when
  it is asked for as a `.wav`, since nearly every foreign recording is an
  MP3. A missing foreign recording prints nothing, as in retail.
- Single player's cinematic voice track (step 7) has nothing to play in
  Academy and is not served.

## Live, without input

Each run starts an fnTech3 `jka-mp` client on an unswitched private desktop:
windowed, `developer 1`, `net_enabled 0`, no input injected, no server, empty
`base/jampconfig.cfg` and `base/jaconfig.cfg` in its own home, and
`s_language` set on the command line, or not set at all. At the main menu one
startup cfg prints `s_language` and `cvarlist s_language`, then runs `play` on
seven sounds, each asked for as a `.wav` and followed by eight voice dumps
(`s_alDebugDump`, or `s_show 2` with the legacy mixer), then `s_list`. The
restart run starts in English, plays Kyle's taunt and lists it, then does
what the multiplayer setup menu's voice item and Set Language button do,
`set s_language espanol` and `ui_load ; snd_restart`, and plays it again. The
private desktop mutes the client, so nothing was heard.

The probe sounds and the lengths their files give, from Raven's `#UNCOMP`
tags (44,100 Hz mono samples):

| Sound | English | German | French | Spanish |
| --- | --- | --- | --- | --- |
| `sound/chars/kyle/misc/taunt1.wav` | 51,840 | 59,904 | 63,360 | 56,448 |
| `sound/CHARS/kyle/misc/death1.wav`, asked for in capitals | 73,728 | 34,560 | 86,400 | 100,224 |
| `sound/chars/tavion/misc/taunt.wav` | none | 96,768 | 57,600 | 87,552 |
| `sound/chars/chewbacca/misc/death1.wav` | 57,600 | none | none | none |
| `sound/chars/worker1/misc/death2.wav` | 34,560 | 26,496 | 42,624 | none |
| `sound/chars/stofficer1/misc/cover5.wav` | 39,168 | 50,688 | none | 36,864 |
| `sound/weapons/saber/saberon.wav` (not a voice) | 72,576 | | | |

| Run | Runtime | Mixer | `s_language` | `taunt1` | `death1` | Tavion | Chewbacca | `worker1` | `stofficer1` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `before-de-openal-1` | before | OpenAL Soft | `deutsch` | English, 1,175 ms | English, 1,671 ms | missing | English | English, 783 ms | English, 888 ms |
| `before-fr-openal-1` | before | OpenAL Soft | `francais` | English, 1,175 ms | English, 1,671 ms | missing | English | English, 783 ms | English, 888 ms |
| `before-es-legacy-1` | before | legacy mixer | `espanol` | English, 25,920 | English, 36,864 | missing | English | English, 17,280 | English, 19,584 |
| `after-de-openal-1` | after | OpenAL Soft | `deutsch` | **German, 1,358 ms** | **German, 783 ms** | **German, 2,194 ms** | English | **German, 600 ms** | **German, 1,149 ms** |
| `after-fr-openal-1` | after | OpenAL Soft | `francais` | **French, 1,436 ms** | **French, 1,959 ms** | **French, 1,306 ms** | English | **French, 966 ms** | English, 888 ms |
| `after-es-openal-1` | after | OpenAL Soft | `espanol` | **Spanish, 1,280 ms** | **Spanish, 2,272 ms** | **Spanish, 1,985 ms** | English | English, 783 ms | **Spanish, 835 ms** |
| `after-en-openal-1` | after | OpenAL Soft | not set (`english`) | English, 1,175 ms | English, 1,671 ms | missing | English | English, 783 ms | English, 888 ms |
| `after-de-legacy-1` | after | legacy mixer | `DEUTSCH` | **German, 29,952** | **German, 17,280** | **German, 48,384** | English | **German, 13,248** | **German, 25,344** |
| `after-es-legacy-1` | after | legacy mixer | `espanol` | **Spanish, 28,224** | **Spanish, 50,112** | **Spanish, 43,776** | English | English, 17,280 | **Spanish, 18,432** |
| `after-restart-openal-1` | after | OpenAL Soft | not set, then `espanol` | English, 1,175 ms, then **Spanish, 1,280 ms** | | | | | |

**Observed** (fnTech3):

- Every run reached every step and quit by itself (exit 0), with the
  executable's hash unchanged and the installation untouched.
- After the change, each voice named in bold loads from `sound/chr_d`,
  `sound/chr_f` or `sound/chr_e`: the developer note names that file, and
  `s_list` gives the length of that recording, in milliseconds with OpenAL
  or in samples at the legacy mixer's 22,050 Hz, half of the file's own
  count. The sound keeps its English name in `s_list`, as retail's does.
- Where the language has no recording (Chewbacca in every language,
  `worker1` in Spanish and `stofficer1` in French), the English recording
  loads, with no word about the missing one. Tavion's taunt, recorded only in
  the three other languages, is missing in English and loads in each of them.
  `saberon`, not a voice, loads the same in every run.
- The legacy mixer keeps a name's letters, and there
  `sound/CHARS/kyle/misc/death1.wav` loads the German or Spanish recording,
  as retail's lowercasing lets it; `DEUTSCH` in capitals chooses German.
- Every sound that loaded plays: each shows in all eight dumps after its
  `play`, as an OpenAL `local` voice at gain 1.00 with no send, or in the
  legacy mixer's list at 127/127. Missing sounds show in none.
- After the change `s_language` prints `english` as its default, `cvarlist`
  shows it archived, and its description prints. Before it, the cvar exists
  only when the command line makes it, as a user cvar whose default is the
  value given.
- The restart run's `ui_load` reloads the menus, restarting the renderer and
  the menu's video, and `snd_restart` restarts the sound system; afterwards
  `s_language` is `espanol` and Kyle's taunt loads in Spanish.
- Before the change, every voice loads in English whatever `s_language`
  says, and Tavion's taunt is missing in every language.

**Single player.** A `jka-sp` client stops with `jka-sp is not implemented:
Jedi Academy's single-player engine menu boundary is open.` before any
startup command runs (run `after-sp-de-openal-1`), so no voice is registered
there. Its registration and rule are the shared code the unit tests run for
both profiles.

## Presentation (invariant 11)

The voices play through Fn's mixers as every Academy sound does: the OpenAL
backend's classes, distance model, occlusion, rooms, HRTF and limiter, or the
legacy mixer, the retail-exact option. Only which file a name loads is
retail's.

## Tests

- `tests/audio_codec_profile_tests.py` compiles the production
  `S_CodecInit`, `S_CodecGetSound`, `S_CodecLoad` and the four name helpers
  for every profile, with and without MP3, and runs sixteen `s_language`
  values against seven names and 64 sets of present files: German, French,
  Spanish and English recordings, each as `.wav` or `.mp3`. Both Academy
  profiles read the foreign name quietly through the codec list, keeping the
  name's own extension or none, and then the English one; each language is
  chosen in any case (`DEUTSCH`, `deutsch`, `Deutsch`, `Espanol` among the
  values), while `espanol ` with a trailing space, `español`, `spanish`,
  `german` and `italiano` read English only; the
  first `chars` in any case, anywhere in the name, is replaced; a missing
  voice warns once; every other profile loads as the codec list does, and
  Outcast by its own rule. Kyle's Spanish taunt is also spelled out.
- `tests/stvef_language_tests.py` adds Academy to the voice probe, in both
  profiles: the German, French and Spanish names first and quiet, the
  English fallback and its warning, the extension kept, any case, the first
  `chars` only, names without `chars`, other settings, and neither Elite
  Force's `voice/` nor Outcast's `.wav`; Spanish plays English in Outcast and
  Elite Force. Its source check pins both Academy profiles' registration in
  `S_Init`, before `s_initsound`, beside Elite Force's and Outcast's.
- Twenty mutations each fail them (`.tmp/jka-foreign-voices/mutations.log`):
  no foreign voices; a warning for a missing foreign recording; no English
  fallback; Spanish playing German; German and French swapped; no Spanish;
  the language compared with case; every other language played as Spanish;
  `chars` found only in lower case; the last `chars` instead of the first;
  only a `/chars/` folder; the foreign name cut to its stem as Outcast's is;
  foreign voices for every profile, or for one Academy mode only; and the
  registration with an empty default, without `CVAR_NORESTART`, unarchived,
  for multiplayer only, or missing.
- Linux: both fixtures, generated from these tests, build and pass with GCC
  13.3 and Clang 18.1 (`-m32`; the voice probe also native, with `-Wall
  -Wextra -Werror`).
- An export of `3cb9d35c` with the earlier slices' records and ledger row,
  the Outcast MP3 and foreign-voice slices and this change applied passes the
  documentation, changelog, manual and packaging tests and every test that
  reads `snd_codec.cpp` or `snd_main.cpp`: 27 files, 269 passed and 1 skipped
  (symbolic links need a privilege this machine lacks). Both changed engine
  files compile there with `win32-dev`'s strict options.
- In the shared working tree, the 22 test files that read `snd_codec.cpp` or
  `snd_main.cpp` pass (155 tests, 272 subtests), as do the documentation,
  changelog, manual and packaging tests (110 passed, 1 skipped), and both
  engine files compile with `win32-dev`'s strict options.

## What stays open

- **Single player live**: `jka-sp` has no client menu yet, so its voices were
  not played.
- **In-game voices**: the client game's own voice lines in a session were not
  exercised; here the console played them.
- **Audible play and retail playback**: the clients were muted, and the
  retail clients need disc 1, so retail `jamp.exe` was not run for
  comparison.
