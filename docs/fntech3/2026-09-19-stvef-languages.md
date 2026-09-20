# Elite Force languages and key numbers

## Scope

This record covers the engine's part in Elite Force's languages: the German
voice recordings chosen by `s_language`, the translated key names chosen by
`g_language`, the `k_language` cvar, and the key numbers Elite Force's UI is
given. The installation carries English, German and French text
(`ext_data/*_deutsch.dat`, `*_francais.dat`) and German recordings of most
voices under `sound/vox_d`; the game modules pick their own text files from
`g_language`, so the text needs nothing more from the engine.

Not covered: the single-player movie player, which retail tells to use a
German sound track when `s_language` is `DEUTSCH` (`stvoy.exe` `0x403e08`),
belongs to the Bink work. The profiles stay behind `-Dstvef-development=true`.

## Retail behaviour and this engine

| Contract | Retail 1.20 | fnTech3 |
| --- | --- | --- |
| German voices | `S_LoadSound` (`stvoyHM.exe` `0x42c690`, `stvoy.exe` `0x4232f3`) lowercases the name. When it contains `voice/` and `s_language` is `DEUTSCH` in any case, the first `voice/` becomes `vox_d/` and retail reads that `.wav`, then that `.mp3`; only when neither exists does it read the name as given, `.wav` then `.mp3` | `S_CodecLoad` does the same for both sound backends. The codec layer tries each name's `.wav`, then its `.mp3` (then `.ogg`, which this engine adds); a missing German file prints no warning |
| Build scripts | With `com_buildScript` set, retail also reads each voice's German files, whatever the language, so that a content build references them | This engine's build-script mode copies no files, so it does not read them; play is unchanged |
| `s_language` | Registered in `S_Init` (`0x429b46`) as `""`, archived and kept through `cvar_restart` (flags `0x401`), before the `s_initsound` check | The same, for both Elite Force profiles. Holomatch's language menu sets it and runs `snd_restart`, which reloads every sound |
| `g_language` | Registered in `CL_Init` (`0x40fa14`) as `""`, flags `0x401`. It picks the key names below; the modules read it to choose their text files | The same |
| `k_language` | Registered in `IN_Init` (`0x43ed12`) as `american`, flags `0x401`. The Win32 input maps scan codes through tables for `deutsch`, `francais`, `espanol` and `italiano` (`0x4441f0`) | Registered the same, so the menu shows retail's value. Characters come from the operating system's keyboard layout, so the value changes nothing |
| Key names | The UI's key-name import (case 32 of the UI dispatcher, `0x412a67`) calls `Key_KeynumToString` with translation (`0x40b830`; SP `0x406bd0`). A printable character, `;` included, is itself; otherwise the table for `g_language` (`deutsch` German, `francais` French, compared without regard to case, anything else English); otherwise `0x` and two lowercase hex digits | `fntech3::stvef::keys::NameForModule`. Retail's three tables are original RTCW's English, French and German columns, down to Holomatch's `UMSCHALLT` for Shift where single player has `UMSCHALT`, so the RTCW table serves both |
| Key numbers | Elite Force numbers its keys as original Quake III and RTCW do: `K_MWHEELDOWN` 183, `K_MWHEELUP` 184, `K_JOY1`-`K_JOY32` 185-216, `K_AUX1`-`K_AUX16` 217-232, `K_CHAR_FLAG` 1024. Its menus take joystick buttons 1-4 as Enter | This engine numbers four more mouse buttons before the wheel, so its wheel was 187 and 188, which Elite Force's menus read as joystick buttons 3 and 4. `stvef_key_adapter.h` now converts the UI's key events and its four key imports; a key Elite Force has no number for, such as mouse buttons 6-9, is not delivered |

## Results

### German voices at runtime

An isolated Holomatch client with developer output played four voice lines,
once with `s_language deutsch` and once with it unset. The developer log
names the file each one loaded:

| Line | `s_language deutsch` | Unset |
| --- | --- | --- |
| `sound/voice/computer/misc/1_frag.wav` | `sound/vox_d/computer/misc/1_frag.mp3` | `sound/voice/computer/misc/1_frag.mp3` |
| `sound/voice/computer/misc/flagret_blu.wav`, no German recording | `sound/voice/computer/misc/flagret_blu.mp3` | The same |
| `sound/Voice/computer/misc/2nd.wav`, mixed case | `sound/vox_d/computer/misc/2nd.mp3` | `sound/voice/computer/misc/2nd.mp3` |
| `sound/voice/alexa/misc/gasp.wav`, German recording only | `sound/vox_d/alexa/misc/gasp.mp3` | Not found, as in retail |

With the language set, the UI's own intro line also loaded from
`sound/vox_d`. Of the 3,916 voice lines in the retail paks, 3,802 have a German
recording and 114 exist only in English; 13 more exist only in German. The
installation was left unchanged.

### Key names and numbers against retail

`scripts/probe_stvef_key_names.py --installation <root> --output <json>`
reads the three tables from each retail executable, after checking that the
key-name function loads their addresses. It then compares retail's behaviour
with this engine's for every key number from -3 to 300, 13 values of
`g_language` and both modes: all 7,904 names match. The same run checks the
key number adapter over 304 numbers: every number retail names reaches the
engine key of that name (`MWHEELDOWN`, 183, reaches `K_MWHEELDOWN`), numbers
below 128 reach themselves, the rest of Elite Force's range reaches an engine
key, and nothing above 232 does.

| Executable | SHA-256 | Table rows (English, German, French) |
| --- | --- | --- |
| `stvoy.exe` | `8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74` | 107, 106, 106 |
| `stvoyHM.exe` | `5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff` | 107, 106, 106 |

### Tests

- `stvef_language_tests` compiles `S_CodecLoad` against recording stubs and
  checks the names it tries and their order: the German name first and
  silently, then the name as given, only for `DEUTSCH` in the Elite Force
  profiles, only the first `voice/`, and only within retail's 64 bytes. It
  also checks the key numbers and names on known values, and pins where the
  three cvars are registered and that the UI's key events and imports use Elite
  Force's numbers.
- `stvef_client_module_dispatch_tests` checks that the four key imports reach
  the Elite Force key functions with the module's own numbers.
- `rtcw_key_names_tests` still passes over the shared table.

## Differences

- Retail refuses a stereo `.wav` ("is a stereo wav file"); this engine plays
  it. All 481 retail `.wav` files are mono.
- This engine had no `KP_STAR` key name, a choice it shared with Quake3e for
  every game, so a retail config line `bind KP_STAR ...` had to use `0xb0`. The
  [key-name record](2026-09-20-key-names.md) restores the name for every game,
  as both retail tables carry it; the keypad key itself still reports `*`.
