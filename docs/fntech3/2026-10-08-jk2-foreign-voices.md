# Jedi Outcast's German and French voices, 2026-10-08

Jedi Outcast ships German and French recordings of its characters' voices
beside the English ones, and both of its retail engines play them when
`s_language` asks for them. fnTech3 registered `s_language` for Elite Force
only and never looked for them, so a German or French setting played the
English voices: the open item the [MP3 record](2026-10-08-jk2-mp3-sounds.md)
left. This record pins how both retail engines choose a voice's language,
registers `s_language` for `jk2-sp` and `jk2-mp` as they do, and loads each
voice from `sound/chr_d` or `sound/chr_f` by their rule, falling back to the
English recording. Live, without input, a multiplayer client at its main menu
loads Mon Mothma's announcer line and three other voices in German and in
French, falls back to English where a language has no recording, and changes
language through the setup menu's own commands; the build before the change
played English throughout. The [JSON companion](2026-10-08-jk2-foreign-voices.json)
holds every address, count, command line and run result. No gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jk2mp.exe` 1.02a | `2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a` |
| `jk2sp.exe` 1.02a | `0d3c635ee5b20ac32fc957e00cf21e65ebc1584112b11f6ae026706dcc0d0e0c` |
| `base/assets0.pk3` | `e8e466f219bb2faed536021bb0d10aa6b7f5cd687302aa43080da2debdae307c` |
| `base/assets1.pk3` | `c3a9aeaf09c93e57847290e7e5cd6c1a071a560045fa8c5e8c6df3688df841c1` |
| `fntech3.exe`, before (runtime `jk2-voices-before`) | `e3f7cdaa8b6969972830381315fb391e6ed7e8ad1272bfcf19d0df663ec7cbea` |
| `fntech3.exe`, after (runtime `jk2-voices-after`) | `d97e887534c1d9fc3a397a0401fac890bdce81367aacca1efb26f8419343f4c6` |
| `soft_oal.dll`, OpenAL Soft 1.25.2 x86, beside both | `011d2cdb7cdc2a97858f414956955df5a2514fecefe1982928c4cd2b1a2e0c8a` |

The live menu steps need other sessions' uncommitted work, Outcast's menu
rendering and UI import 32, and this change stands on the uncommitted MP3
slice. So both runtimes come from one byte copy of the shared working tree on
`37b87dce`, built as `win32-dev` is (MSVC 2022 x86, Meson `debugoptimized`,
strict warnings, every development gate on). The "before" runtime is that
copy as taken; the "after" runtime is the same copy with this change, whose
two files alone recompiled. The renderer module, engine package and web pak
are the same files in both. The installation was read only and its inventory
was unchanged after every run.

## The installation

**Observed** (`base/assets1.pk3`, which holds every voice, read with
Python's `zipfile`):

- `sound/chars/` holds 2,895 files, 2,864 MP3 and 31 WAV; `sound/chr_d/`
  2,358 and `sound/chr_f/` 2,356, all MP3. The MP3 record's 3,018, 2,434 and
  2,432 also counted the pak's 123, 76 and 76 folder entries.
- 2,352 English voices have a German recording and 2,350 a French one, the
  same 2,350 with both; 543 have neither, among them all 31 WAVs. Every
  English voice with a recording in another language is an MP3, and no voice
  exists as both a `.wav` and an `.mp3`.
- Six recordings exist only in German and French: `bartender/misc/choke1`
  and `choke2`, `jedi1/misc/anger3`, `worker2/misc/giveup4`, and
  `video/cinematic_5` and `cinematic_6`.
- Every pak entry spells its folder `chars`, `chr_d` or `chr_f` in lower
  case; 78 file names carry capitals, Mon Mothma's `40MOM` lines among them.
  No sound name that the installation's modules, maps, scripts or
  executables ask for spells `chars` in another case (the MP3 record's scan
  of requested names).
- The retail multiplayer client game registers Mon Mothma's announcer lines,
  `sound/chars/mothma/misc/40MOM001` and on, each recorded in all three
  languages.
- The setup menus of both modes (`ui/setup.menu`, `ui/ingamesetup.menu`,
  `ui/jk2mp/setup.menu`, `ui/jk2mp/ingame_setup.menu`) set the voice
  language through `s_language`: English writes `english`, Francais
  `francais` and Deutsch `deutsch`. Multiplayer's then shows a Set Language
  button that runs `ui_load ; snd_restart`. The text language is a separate
  setting, `sp_language`.

## How retail picks a voice's language

**Observed** (`jk2mp.exe` and `jk2sp.exe`, read with Capstone; no retail code
is incorporated). Addresses are multiplayer's, then single player's.

1. **Registration.** Each `S_Init` registers `s_language` as `english` with
   flags 0x401 (0x45ca1a, 0x423350), before its `s_initsound` check (0x45ca68,
   0x42339d), so it exists with sound turned off. 0x1 is archived: the config
   writer saves cvars with that bit (0x42b1f0, 0x416af0). 0x400 is Quake III's
   `CVAR_NORESTART`: each engine's `cvar_restart` handler (0x42b3b0,
   0x416ca0) leaves alone the cvars with 0x450 set, read-only, init and
   0x400. fnTech3's `CVAR_ARCHIVE` and `CVAR_NORESTART` have the same values.
   The menus register it again with the same default and flags: the
   multiplayer `ui.qvm`'s cvar table (its entry at data offset 0xdc4) and
   single player's built-in menus (`UI_Init` 0x4994b0, at 0x49953f).
2. **Every sound effect passes one loader.** `S_RegisterSound` (0x45ce20,
   0x423c20), and the calls that start a sound whose data is not in memory,
   load it through one helper (0x45cf20, 0x423cf0), whose call (0x45cf26,
   0x423cf6) is the only one of `S_LoadSound`; `S_LoadSound`'s call (0x45f0d6,
   0x42a9da) is in turn the only one of the file loader (0x45f520,
   0x42adb0). Music never reaches it.
3. **The rule.** The loader finds the first `chars` in the sound's name with
   the C library's case-sensitive `strstr` (0x4d4fe0 called at 0x45f52f;
   0x4b33f0 at 0x42adbf). When there is one, it compares `s_language` with
   `DEUTSCH` and then `FRANCAIS` through `_stricmp`, so without regard to
   case (0x4e2480 at 0x45f655 and 0x45f685; 0x4c55f0 at 0x42aef4 and
   0x42af24), and overwrites those five letters with `chr_d` or `chr_f`
   (0x45f669, 0x45f699; 0x42af08, 0x42af38). Any other value leaves the name
   alone (0x45f6a3, 0x42af42). It then reads the name's `.wav`, then its
   `.mp3`; only when a substituted name finds neither does it put `chars`
   back and read the English `.wav`, then `.mp3` (0x45f707 to 0x45f748,
   0x42afa6 to 0x42afe7). Nothing is printed when a recording is missing.
4. **Case.** Multiplayer's name is the module's own text, cut at its first
   `.` with `.wav` appended (`S_LoadSound` 0x45f050; `S_FindName` 0x45cc00
   keeps the name's case), so a `Chars` or `CHARS` is not found there. Single
   player lowercases the name first (`_strlwr` 0x4c6323, called at
   0x42a96a), so it finds any case.
5. **Data builds.** With `com_buildScript` set (cvar pointers 0xaa1d88,
   0xa852b0), the loader also opens and closes the German and French file of
   every voice, `.wav` or else `.mp3` (0x45f545 to 0x45f640, 0x42add5 to
   0x42aee0), so a data-building run touches them. Multiplayer then puts back
   `chars` but not `.wav` (0x45f633), so where the French `.wav` is missing
   the voice goes on with its name ending `.mp3`. **Inferred** from that
   path, not run: an English-only WAV voice, such as the client game's
   `sound/chars/turret/move.wav`, then fails to load in such a run. Single
   player puts back both (0x42aed3).
6. **Other readers.** Multiplayer reads `s_language` nowhere else. Single
   player reads it twice more:
   - Its cinematic player (0x404a8f) gives `video/jk05.roq` and
     `video/jk06.roq` the separate voice track
     `sound/chars/video/cinematic_5` or `cinematic_6` when `s_language` is
     neither empty nor `english`. It registers the track through
     `S_RegisterSound` (0x403c50), so the rule above turns it into the German
     or French recording, two of the six that exist in no English form.
   - Its handler for the server's settings (0x40a970, which reads
     `sv_serverid` and `helpUsObi` from them) sets `g_dismemberment` to `0`
     when `s_language` is `DEUTSCH`, `sp_language` is 2 or `ui_iscensored`
     is 1.

## fnTech3 before

`S_Init` registered `s_language` for Elite Force only. In `jk2-mp` the retail
menu module's cvar table created it, as `english` and archived; in `jk2-sp`
nothing did. Neither profile's sound loading read it, so a German or French
setting played the English voices, and the six recordings that exist only in
German and French never loaded.

## The change

- `S_Init` (`code/client/audio/snd_main.cpp`) registers `s_language` for both
  Outcast profiles as both retail engines do: `english`, `CVAR_ARCHIVE |
  CVAR_NORESTART`, before the `s_initsound` check. Elite Force keeps its own
  registration, with its empty default; no other profile registers it.
- `S_CodecLoad` (`code/client/audio/codecs/snd_codec.cpp`), which both sound
  backends use to load every sound effect, takes an Outcast sound's
  `<name>.wav` from the MP3 slice's rule. When `s_language` is `deutsch` or
  `francais`, in any case, it turns that name's first `chars` into `chr_d` or
  `chr_f` and loads it first, without a warning, through the codec list:
  `.wav`, `.mp3`, then Fn's Ogg Vorbis. Only when that finds nothing does it
  load the English name as before, which warns when it is missing too.
  Multiplayer finds `chars` as the name is written, single player in any
  case. Music is untouched, as in retail. Every other profile, Elite Force's
  German voices included, loads exactly as before.

**Differences recorded, not reproduced:**

- Fn's OpenAL backend lowercases every sound name it registers before loading
  it, so there a multiplayer name with a capitalized `Chars` would find its
  German or French recording, where retail multiplayer would not. The legacy
  mixer keeps the name's case and matches retail. No installed name has such
  a `chars`.
- The data-build pass of step 5 is not reproduced: it only lets a
  data-building run touch the foreign files, and nothing in fnTech3 builds
  Outcast's data. Its multiplayer slip, an English-only WAV voice failing in
  such a run, is not reproduced either.
- The foreign name also tries Fn's Ogg Vorbis after the `.mp3`, as every
  Outcast name does; no Outcast voice is Ogg Vorbis.
- Diagnostics: with `developer` set, each voice loaded from a foreign folder
  shows in Fn's `Sound fallback: ...chr_d/x.wav -> ...chr_d/x.mp3` note,
  since every foreign recording is an MP3. A missing foreign recording prints
  nothing, as in retail.
- Single player's cinematic voice track and its dismemberment setting (step
  6) are not served: `jk2-sp` has no client yet. Open below.

## Live, without input

Each run starts an fnTech3 `jk2-mp` client on an unswitched private desktop:
windowed, `developer 1`, `net_enabled 0`, no input injected, no server, an
empty `base/jk2mpconfig.cfg` in its own home, and `s_language` set on the
command line, or not set at all. At the main menu one startup cfg prints
`s_language` and `cvarlist s_language`, then runs `play` on five sounds, each
followed by eight voice dumps (`s_alDebugDump`, or `s_show 2` with the legacy
mixer), then `s_list`. The restart run starts in English, plays Mon Mothma's
line and lists it, then does what the multiplayer setup menu's voice item and
Set Language button do, `set s_language deutsch` and `ui_load ;
snd_restart`, and plays it again. The private desktop mutes the client, so
nothing was heard.

The probe sounds and the lengths their files give, from Raven's `#UNCOMP`
tags (44,100 Hz mono samples):

| Sound | English | German | French |
| --- | --- | --- | --- |
| `sound/chars/mothma/misc/40MOM038` | 42,624 | 35,712 | 32,256 |
| `sound/chars/mothma/misc/taunt` | 23,040 | none | none |
| `sound/chars/bartender/misc/choke1` | none | 36,864 | 40,320 |
| `sound/chars/kyle/misc/taunt1` | 78,336 | 57,600 | none |
| `sound/weapons/saber/saberon.wav` (not a voice) | 72,576 | | |

| Run | Runtime | Mixer | `s_language` | `40MOM038` | `taunt` | `choke1` | `taunt1` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `before-de-openal-1` | before | OpenAL Soft | `DEUTSCH` | English, 966 ms | English | missing | English, 1,776 ms |
| `after-de-openal-1` | after | OpenAL Soft | `DEUTSCH` | **German, 809 ms** | English | **German, 835 ms** | **German, 1,306 ms** |
| `after-fr-openal-1` | after | OpenAL Soft | `francais` | **French, 731 ms** | English | **French, 914 ms** | English, 1,776 ms |
| `after-en-openal-1` | after | OpenAL Soft | not set (`english`) | English, 966 ms | English | missing | English, 1,776 ms |
| `before-de-legacy-1` | before | legacy mixer | `DEUTSCH` | English, 21,312 | English | missing | English, 39,168 |
| `after-de-legacy-1` | after | legacy mixer | `DEUTSCH` | **German, 17,856** | English | **German, 18,432** | **German, 28,800** |
| `after-restart-openal-1` | after | OpenAL Soft | not set, then `deutsch` | English, 966 ms, then **German, 809 ms** | | | |

**Observed** (fnTech3):

- Every run reached every step and quit by itself (exit 0), with the
  executable's hash unchanged and the installation untouched.
- After the change, each voice named in bold loads from `sound/chr_d` or
  `sound/chr_f`: the developer note names that file, and `s_list` gives the
  length of that recording, in milliseconds with OpenAL or in samples at the
  legacy mixer's 22,050 Hz, half of the file's own count. The sound keeps its
  English name in `s_list`, as retail's does.
- Where the language has no recording (Mon Mothma's `taunt`, and Kyle's
  `taunt1` in French), the English recording loads, with no word about the
  missing one. `choke1`, recorded only in German and French, is missing in
  English and loads in both other languages. `saberon`, not a voice, loads
  the same in every run.
- Every sound that loaded plays: each shows in all eight dumps after its
  `play`, as an OpenAL `local` voice at gain 1.00 with no send, or in the
  legacy mixer's list at 127/127. Missing sounds show in none.
- `s_language` prints `english` as its default in every run, `cvarlist`
  shows it archived, and only the "after" runtime prints its description.
  In the "before" runtime the default can only have come from the menu
  module's cvar table (inferred: nothing else registers it there).
- The restart run's `ui_load` reloads the menus and `snd_restart` restarts
  the sound system; afterwards `s_language` is `deutsch` and Mon Mothma's
  line loads in German.
- Before the change, every voice loads in English whatever `s_language`
  says, and `choke1` is missing in German too.

**Single player.** A `jk2-sp` client stops at `CL_InitUI` before any startup
command runs, as the MP3 record found, so no voice is registered there. Its
registration and its any-case rule are the shared code the unit tests run for
both profiles.

## Presentation (invariant 11)

The voices play through Fn's mixers as every Outcast sound does: the OpenAL
backend's classes, distance model, occlusion, rooms, HRTF and limiter, or the
legacy mixer, the retail-exact option. Only which file a name loads is
retail's.

## Tests

- `tests/audio_codec_profile_tests.py` compiles the production
  `S_CodecInit`, `S_CodecGetSound`, `S_CodecLoad` and the three name helpers
  for every profile, with and without MP3, and runs ten `s_language` values
  against six names and eight sets of present files: German and French
  recordings as `.wav` or `.mp3`, and the English one. Both Outcast profiles
  read the foreign name `.wav`, `.mp3`, `.ogg` quietly, then the English one;
  `DEUTSCH ` with a trailing space, `german` and `italiano` read English
  only; only the first `chars`, anywhere in the name, is replaced; single
  player alone finds `CHARS`; a missing voice warns once; every other profile
  loads as the codec list does. Mon Mothma's line is also spelled out.
- `tests/stvef_language_tests.py` adds Outcast to the voice probe: the German
  and French names first and quiet, the English fallback and its warning,
  English and other settings, names without `chars` and the multiplayer and
  single-player case rule, with Elite Force's German order unchanged. Its
  source check pins both Outcast profiles' registration in `S_Init`, before
  `s_initsound`, beside Elite Force's.
- Fourteen mutations each fail them (`.tmp/jk2-foreign-voices/mutations.log`):
  no foreign voices; a warning for a missing foreign recording; no English
  fallback; German and French swapped; the language compared with case; every
  other language played as German; any case in multiplayer; lower case only
  in single player; the last `chars` instead of the first; only a `/chars/`
  folder; foreign voices for every profile; and the registration with an
  empty default, without `CVAR_NORESTART`, or for multiplayer only.
- Linux: both fixtures, generated from these tests, build and pass with GCC
  13.3 and Clang 18.1 (`-m32`; the voice probe also native and with
  `-Werror`).
- An export of `37b87dce` with the earlier slices' records and ledger row,
  the MP3 slice and this change applied passes the documentation, changelog,
  manual and packaging tests and every test that reads `snd_codec.cpp` or
  `snd_main.cpp`: 27 files, 269 passed and 1 skipped (symbolic links need a
  privilege this machine lacks). Both changed engine files compile there
  with `win32-dev`'s strict options.

## What stays open

- **Single player live**: `jk2-sp` has no client menu, so its voices were
  not played; its cinematic voice track for `jk05` and `jk06` and its
  `g_dismemberment` setting in German are not served.
- **In-game voices**: Outcast's client-game sound imports are still refused,
  so the client game does not start Mon Mothma's lines yet; here the console
  played them.
- **Audible play and retail playback**: the clients were muted, and retail
  `jk2mp.exe` was not run for comparison.
