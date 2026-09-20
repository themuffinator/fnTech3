# Quake III key numbers

## Scope

This record covers the key numbers the `q3` profile gives Quake III's UI and
cgame modules: key events, and the key imports that take or return a key
number. It found that the profile handed its modules this engine's numbers,
which are Quake Live's and differ from Quake III's from the mouse wheel upward,
and corrects that. Quake Live's modules keep this engine's numbers. The
[machine-readable record](2026-09-19-q3-key-numbers.json) holds the retail
identities and the comparison.

Not covered: key names beyond the differences listed below, key catchers, and
runtime input. No game was given mouse or keyboard input for this record.

## Observed

### Retail key tables

Each engine's `Key_StringToKeynum` and `Key_KeynumToString` search one
`{name, keynum}` table. `scripts/probe_q3_key_numbers.py` finds it from its
`MWHEELDOWN` row, walks it from its first row (`TAB`, 9) to its NULL
terminator, and records the code that loads its address.

| Executable | SHA-256 | Table, rows | Code loading it | Mouse | Wheel | Joystick, aux |
| --- | --- | --- | --- | --- | --- | --- |
| `quake3.exe` "Q3 1.32" win-x86, built Oct 7 2002 | `80b0add0e014b863cf47cab6c997e0a54939236219a56910b7a3eb77dc2d44d4` | `0x4bf950`, 107 | `0x40a72e`, `0x40a735`, `0x40a7c0`, `0x40a7c5` | `MOUSE1`-`MOUSE5` 178-182 | `MWHEELDOWN` 183, `MWHEELUP` 184 | `JOY1`-`JOY32` 185-216, `AUX1`-`AUX16` 217-232 |
| `quakelive_steam.exe` | `c926fe9f6c851e00b3b9332e88903ad01f28fdd60454873891c0158f5ded1299` | `0x565b88`, 111 | `0x4b65b6`, `0x4b65bc`, `0x4b6bd2`, `0x4b6bd9` | `MOUSE1`-`MOUSE9` 178-186 | `MWHEELDOWN` 187, `MWHEELUP` 188 | `JOY1`-`JOY32` 189-220, `AUX1`-`AUX16` 221-236 |

Below 178 the two tables give every key the same number.
[`code/client/keycodes.h`](../../code/client/keycodes.h) numbers keys as
Quake Live does, inherited from FnQL at its pinned baseline; FnQuake3 at its
pinned baseline, Quake3e, the GPL release, RTCW, ET and Elite Force number them
as Quake III does.

### Quake III's modules

From the [id Software GPL release](https://github.com/id-Software/Quake-III-Arena)
(read only). q3_ui, Team Arena's UI and both cgames are built against
`code/ui/keycodes.h`, whose numbers are quake3.exe's above; the 1.17 source's
`keycodes.h`, the UI API 4 generation this profile also loads, has the same
numbers.

- q3_ui ignores releases (`UI_KeyEvent`, `q3_ui/ui_atoms.c:862`). Its
  `Menu_DefaultKey` (`q3_ui/ui_qmenu.c:1678`) activates the item under the
  cursor on `K_JOY1`-`K_JOY4` and `K_AUX1`-`K_AUX16`, as on Enter, unless the
  item is mouse-only, greyed or inactive. `RadioButton_Key`
  (`ui_qmenu.c:464`) toggles on `K_JOY1`-`K_JOY4`, and `MenuField_Key`
  (`q3_ui/ui_mfield.c:402`) moves to the next field on them. q3_ui has no
  mouse wheel handling.
- Team Arena's `Menu_HandleKey` (`ui/ui_shared.c:2716`) ignores `K_JOY1`-`K_JOY4`
  and `K_AUX1`-`K_AUX16`. The same menu code runs in Team Arena's cgame through
  `CG_KeyEvent` (`cgame/cg_newdraw.c:1786`); baseq3's cgame key event is empty
  (`cgame/cg_main.c:1991`).
- Quake III's engine sends the UI or cgame every press and release with its own
  numbers, and characters with `K_CHAR_FLAG` 1024 (`client/cl_keys.c:1115`-`1149`,
  `1219`). The UI's key name, binding lookup, bind and key-down imports and the
  cgame's `CG_KEY_ISDOWN` and `CG_KEY_GETKEY` take or return those numbers.

### fnTech3 before this change

- The `q3` profile loads only QVMs and `vmMain` libraries; the `ql` profile
  prefers Quake Live's structured libraries
  ([`game_profile.cpp`](../../code/qcommon/game_profile.cpp)).
- `cl_keys.cpp` sent every UI and cgame key event with this engine's numbers.
  Only Elite Force's UI presses were converted (`CL_UIKeyDownEvent`), and
  RTCW's and ET's calls in `vm.c`. The UI and cgame key imports in
  `cl_ui.cpp` and `cl_cgame.cpp` took and returned this engine's numbers for
  Quake III exactly as for Quake Live.

So a Quake III module read these engine keys as other keys. The q3_ui column
is inferred from the source above, not observed in a running game:

| Engine key | Engine number | Quake III reads | q3_ui |
| --- | --- | --- | --- |
| `MOUSE6`, `MOUSE7` | 183, 184 | `MWHEELDOWN`, `MWHEELUP` | Nothing |
| `MOUSE8`, `MOUSE9` | 185, 186 | `JOY1`, `JOY2` | Enter on the item under the cursor |
| `MWHEELDOWN`, `MWHEELUP` | 187, 188 | `JOY3`, `JOY4` | Enter on the item under the cursor, radio buttons toggle, fields move on |
| `JOY1`-`JOY28` | 189-216 | `JOY5`-`JOY32` | Nothing |
| `JOY29`-`JOY32` | 217-220 | `AUX1`-`AUX4` | Enter |
| `AUX1`-`AUX12` | 221-232 | `AUX5`-`AUX16` | Enter |
| `AUX13` and above | 233 and up | No key | Nothing |

Team Arena's menus ignored the misread wheel. Binding a key through either
UI stayed consistent, because the module hands the number it was given back to
the binding import; a module that tests a key by its own constant, such as
`trap_Key_IsDown( K_MWHEELUP )`, or reads `trap_Key_GetKey`'s answer as one,
got another key.

## Correction

- [`q3_key_adapter.h`](../../code/compat/q3_key_adapter.h) converts between
  Quake III's numbers and this engine's. It uses RTCW's table
  ([`rtcw_keycodes.def`](../../code/compat/rtcw_keycodes.def)), whose numbers
  are Quake III's, as Elite Force's adapter already did. ET's table holds the
  same numbers in its own file.
- `CL_ModuleKeyEvent` in [`cl_keys.cpp`](../../code/client/cl_keys.cpp)
  numbers every UI and cgame key event, press and release, for Quake III and
  Elite Force. A key Quake III has no number for, mouse buttons 6-9 and the 43
  engine keys above `AUX16`, is not delivered. Characters keep `K_CHAR_FLAG`
  1024, the same in every profile.
- In the `q3` profile, the UI's `UI_KEY_KEYNUMTOSTRINGBUF`,
  `UI_KEY_GETBINDINGBUF`, `UI_KEY_SETBINDING` and `UI_KEY_ISDOWN`, and the
  cgame's `CG_KEY_ISDOWN`, `CG_KEY_GETKEY` and name and binding imports, take
  and return Quake III's numbers. A number Quake III has no key for is never
  bound or down, and is named as quake3.exe names it: two lowercase hex digits,
  `<KEY NOT FOUND>` for -1 and `<OUT OF RANGE>` outside 0-255. The key bound to
  a command is found in Quake III's order, so an engine-only key holding the
  same command never displaces one Quake III can name.
- Quake Live: unchanged. The rule follows the profile, not the module's kind,
  so a legacy module loaded in the `ql` profile also keeps this engine's
  numbers, as it did before.
- Elite Force: its UI presses were already converted and its UI releases are
  not sent. Its cgame key events now follow the same rule; Holomatch's cgame
  exports no key event, so nothing changes there today.

## Results

### Retail comparison

`python scripts/probe_q3_key_numbers.py --quake3 <quake3.exe> --quakelive
<quakelive_steam.exe> --output <json>` compiled the adapter, `keycodes.h` and
`CL_Q3KeyString` over the engine's name table with Clang 20.1.7 and compared them
with both tables. It passed:

- For the 304 numbers from -3 to 300, every number quake3.exe names reaches
  the engine key of that name and that key returns the same number. Numbers
  below 128 reach themselves, 128-232 reach engine keys, and nothing above 232
  or below 0 does.
- All 111 numbers quakelive_steam.exe names are this engine's numbers for those
  keys, so Quake Live's modules are handed the numbers they expect unchanged.
- The 47 engine keys Quake III has no number for, `MOUSE6`-`MOUSE9` and every
  key above `AUX16`, are never delivered to a Quake III module.

### Build and tests

- `cl_keys.cpp`, `cl_ui.cpp` and `cl_cgame.cpp` compile with the developer
  build's MSVC 19.44 x86 flags (`/W4 /WX`, RTCW, ET and Elite Force
  development on).
- [`tests/q3_key_numbers_tests.py`](../../tests/q3_key_numbers_tests.py)
  compiles the adapter and checks Quake III's and Quake Live's numbers on known
  values; compiles the event helpers against a recording `VM_Call` and checks
  what each profile's UI and cgame are handed for presses and releases;
  compiles the import helpers over the engine's own name table and checks
  names, bindings, key state and the key bound to a command; and pins the call
  sites, including that Quake Live's imports reach the same handlers
  unconverted and that the `q3` profile never loads structured libraries.
  Removing the Quake III branch, passing raw numbers to the imports, or
  dropping the hex names each fails it.
- `tests/stvef_language_tests.py` now finds Elite Force's conversion in
  `CL_ModuleKeyEvent`.
- On a copy of this commit alone, the three files compile the same way and
  the key, UI, cgame and Elite Force test files, 212 tests with 69 subtests,
  pass.

## Differences

- Names: this engine named 130 `POWER` and 157-159 `F13`-`F15`, which
  quake3.exe leaves as hex, and had no `KP_STAR` name for 176, the choice also
  recorded for [Elite Force](2026-09-19-stvef-languages.md). Those were the
  engine's names for every profile and were unchanged here; the
  [key-name record](2026-09-20-key-names.md) corrects all five rows, so the
  `quake3Names.differences` rows in the
  [machine-readable record](2026-09-19-q3-key-numbers.json) are the ones that
  stood before that correction.
- A Quake III menu can no longer bind mouse buttons 6-9 or keys above `AUX16`;
  before, they reached it under other keys' numbers. The console still binds
  them (`bind MOUSE6 ...`). Retail Quake III has no such keys, and the RTCW, ET
  and Elite Force adapters make the same choice.
- A hexadecimal key name in a config is still this engine's number:
  `bind 0xe9` binds `AUX13` here, and no key in quake3.exe. Neither retail
  engine writes a hexadecimal name above `0x9f`, so no config either of them
  writes can carry one; see the [key-name record](2026-09-20-key-names.md).

## Remaining gates

- Wheel and joystick behaviour was not observed in a running q3_ui or Team
  Arena menu; no input was given to a game. Runtime input gates stay open.
