# RTCW key names in the player's language, and live language changes

The [RTCW matrix](COMPATIBILITY.md#rtcw-gate-matrix) kept "language changes
during a live session" open for MP localization. Checking it found a defect:
RTCW modules ask the engine for key names, and original RTCW answers in the
player's `cl_language`, while fnTech3 always answered with the console's English
names. The MP spectator hint showed it: in French, retail reads
"Appuyez sur CTRL ou **SOURIS1** pour suivre le joueur suivant" and fnTech3 read
"… ou **MOUSE1** …". This record fixes the key names for both modes and shows a
live language change behaving as retail does. The
[machine-readable record](2026-09-18-rtcw-key-names.json) holds identities,
comparisons and runs.

## Original behaviour

The UI's key-name import (both modes) and MP's cgame one hand the module
`Key_KeynumToString( key, qtrue )` of the module's own key number (original
[RTCW SP](https://github.com/id-Software/RTCW-SP) and
[RTCW MP](https://github.com/id-Software/RTCW-MP) `src/client/cl_keys.c`,
`cl_ui.c`, `cl_cgame.c`, read only). That translated path:

- returns `<KEY NOT FOUND>` for -1 and `<OUT OF RANGE>` outside 0–255;
- returns a printable character as itself, `;` included (only the untranslated
  console path names it `SEMICOLON`);
- otherwise looks the key up in one of five tables chosen by `cl_language`:
  1 French, 2 German, 3 Italian, 4 Spanish, anything else English;
- otherwise returns `0x` and two lowercase hex digits.

The tables hold 106 keys each, in the same order. SP and MP are identical except
German Shift, which retail MP spells `UMSCHALLT` and SP `UMSCHALT`.

## Correction

[`code/compat/rtcw_keynames.def`](../../code/compat/rtcw_keynames.def) records
the 106 keys' names in English, French, German (SP), German (MP), Italian and
Spanish, with non-ASCII letters as their Windows-1252 bytes. Every one of those
names is a NUL-terminated string in the matching retail `WolfSP.exe` or
`WolfMP.exe`. [`code/compat/rtcw_key_names.hpp`](../../code/compat/rtcw_key_names.hpp)
implements the translated lookup above, independently written. RTCW's module
key-name requests now use it with the module's key number, `cl_language` and the
mode; key bindings still come from the engine's binding table. `cl_language` is
registered and archived in both modes, as the originals do; MP already did.

[`scripts/probe_rtcw_key_names.py`](../../scripts/probe_rtcw_key_names.py)
compiles the original SP and MP functions and tables, unchanged, as C in a
disposable fixture next to fnTech3's lookup. For every key from -3 to 300 and
every `cl_language` from -2 to 6, in both modes, the 5,472 results are
byte-identical on the Windows x86 and x64 targets, and all 1,060 recorded names
are present in the retail executables.

## Runtime evidence

[`scripts/probe_rtcw_language_hint.py`](../../scripts/probe_rtcw_language_hint.py)
starts one fnTech3 dedicated `rtcw-mp` server on `mp_beach` and joins it as a
spectator with retail `WolfMP.exe` and with the fnTech3 client, windowed on
unswitched private desktops with isolated homes, sound and mouse/joystick input
disabled, console commands only. Each engine's own screenshot of the hint was
inspected:

| Run | Retail `WolfMP.exe` | fnTech3 |
| --- | --- | --- |
| French, before the correction | "… CTRL ou SOURIS1 …" | "… CTRL ou MOUSE1 …" |
| French, after | "… CTRL ou SOURIS1 …" | "… CTRL ou SOURIS1 …" |
| German, after | "STRG oder MAUS1 zum Folgen …" | "STRG oder MAUS1 zum Folgen …" |
| English, then `set cl_language 1` in the session | English, then French with SOURIS1 | English, then French with SOURIS1 |

In both engines a language change during a live session takes effect on the
next frame for the cgame's translated text and for key names. fnTech3 writes its
screenshot from the next frame it renders, so the probe lets that frame draw
before changing the language; retail captures the frame already drawn.

## Tests

[`tests/rtcw_key_names_tests.py`](../../tests/rtcw_key_names_tests.py) checks the
lookup's range strings, printable and `;` handling, each language column, the
German Shift difference between modes, out-of-range `cl_language` values, the
hex fallback and the call site. It passes on the x86 and x64 targets.

## Remaining gates

- The UI's own menu files are chosen by language when menus are loaded, as in
  the original, so menus follow a live change only after `ui_restart`; that
  path was not observed here.
- Italian and Spanish were compared by the reference probe but not captured at
  runtime.
- SP's key names were compared by the reference probe but not observed in a
  running SP UI.
