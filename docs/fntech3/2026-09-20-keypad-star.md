# Keypad star

## Scope

This record covers what this engine's key producers report when the numeric
keypad's `*` is pressed, and why they keep reporting the character `*` rather
than `K_KP_STAR`. It closes the question the
[key-name record](2026-09-20-key-names.md) left open when it restored the
`KP_STAR` name for key 176.

Key numbers, key names, the per-profile adapters and the module key imports are
untouched, and so is every producer: this record changes no engine behaviour.
It replaces two commented-out fragments that read as unfinished work with the
reason they are absent, and pins that reason.

Not covered: runtime input. No game was launched and no key was pressed for
this record, in any profile. See [Remaining](#remaining).

## Observed

### What retail reports

Every original engine in scope maps the keypad star to the character `*` on
Windows and on X11. Only the Mac clients report `K_KP_STAR`.

| Engine | Producer | Keypad star |
| --- | --- | --- |
| Quake III Arena | `code/win32/win_wndproc.c` | `'*'` |
| Quake III Arena | `code/unix/linux_glimp.c` | `'*'` |
| Quake III Arena | `code/macosx/macosx_input.m` | `K_KP_STAR` |
| RTCW SP | `src/win32/win_wndproc.c` | `'*'` |
| RTCW MP | `src/win32/win_wndproc.c` | `'*'` |
| Enemy Territory | `src/win32/win_wndproc.c` | `'*'` |
| Enemy Territory | `src/mac/mac_event.cpp` | `K_KP_STAR` |

Read from the GPL source releases under `E:\_SOURCE\_CODE\`:
`Quake-III-Arena-master`, `RTCW-SP-master`, `RTCW-MP-master` and
`Enemy-Territory-original`. No retail executable was read for this record.

All four Win32 producers reach that result the same way. `s_scantokey[0x37]` is
`'*'`, and neither branch of `MapKey` remaps it: the non-extended branch ends in
`default: return result;`, and the extended branch matches only `K_PAUSE`,
`0x0D`, `0x2F` and `0xAF` before its own `return result;`. A scancode `0x37`
press therefore reports `'*'` whichever branch it takes. X11 is explicit:
`case XK_KP_Multiply: *key = '*'; break;`.

The ioquake3 lineage made the other choice in its SDL producer -
`case SDLK_KP_MULTIPLY:  key = K_KP_STAR;` in `iortcw-master` (SP and MP) and in
`lilium-voyager`, the Elite Force port. This engine's Quake3e lineage did not;
`Quake3e-master/code/sdl/sdl_input.c` carries the same `'*'` this engine has.

### What the keyboard layout reports

`scripts/probe_win32_keypad_star.py`, run on this Windows 11 machine against
its active layout `0x08090809`:

| key | virtual key | scancode | MapKey branch | character | `s_scantokey` |
| --- | --- | --- | --- | --- | --- |
| keypad `*` | `VK_MULTIPLY` | `0x0037` | not extended | `*` | `'*'` |
| keypad `/` | `VK_DIVIDE` | `0xE035` | extended | `/` | `'/'` |
| keypad `+` | `VK_ADD` | `0x004E` | not extended | `+` | `K_KP_PLUS` |
| keypad `-` | `VK_SUBTRACT` | `0x004A` | not extended | `-` | `K_KP_MINUS` |
| main `/` | `VK_OEM_2` | `0x0035` | not extended | `/` | `'/'` |

The full reading is in [`2026-09-20-keypad-star.json`](2026-09-20-keypad-star.json).
The probe queries the layout with a synthetic all-keys-up modifier state; it
reads no key press and injects none.

This is why the keypad slash needs the extended branch and the star does not:
both slashes share scancode `0x35` and only the extended bit tells them apart,
while the star owns `0x37`. The branch column is the layout's view, not a
message's: the same reading calls num lock non-extended `0x45`, where
`s_scantokey` expects the extended branch to turn `K_PAUSE` into
`K_KP_NUMLOCK`. That caveat does not reach the conclusion above, which holds in
either branch.

### What this engine reports

Three producers, all agreeing with retail's Windows and X11 behaviour:

- [`code/sdl/sdl_input.cpp`](../../code/sdl/sdl_input.cpp) -
  `case SDLK_KP_MULTIPLY:  key = '*';`. `sdl` is an `auto` feature, so this is
  the producer a build gets whenever SDL3 is found, on either platform.
- [`code/win32/win_wndproc.cpp`](../../code/win32/win_wndproc.cpp) -
  `s_scantokey[0x37]` is `'*'`, in the `-Dsdl=disabled` Windows client.
- [`code/unix/linux_glimp.cpp`](../../code/unix/linux_glimp.cpp) -
  `case XK_KP_Multiply: *key = '*';`, in the `-Dsdl=disabled` X11 client.

Typing the character does not go through any of them. Windows pairs the key
with a `WM_CHAR`, which `MapChar` answers with `*` by both of its routes
(`directMap` passes printable ASCII through, and `s_scantochar[0x37]` and
`[0x77]` are both `'*'`), and SDL pairs it with `SDL_EVENT_TEXT_INPUT`, which
`IN_QueueTextInput` turns into `SE_CHAR`. A console or chat field would keep
receiving `*` from the keypad whatever the key mapping said.

### The two commented-out fragments

Both were inherited from the Quake3e lineage and both read as an unfinished
change rather than a decision:

- `code/sdl/sdl_input.cpp` held `key = '*'; /*K_KP_STAR;*/`.
- `code/win32/win_wndproc.cpp` held `//case '*':` / `//	return K_KP_STAR;`.

The Win32 fragment would not do what it looks like it does. It sits in the
extended branch, which the keypad star does not take, and `'*'` is what both
branches return in any case. What does reach that branch with scancode `0x37`
was not observed; `VK_SNAPSHOT` is handed to `DefWindowProcW` before `MapKey`
([`win_wndproc.cpp:1745`](../../code/win32/win_wndproc.cpp)).

### What changing it would cost

`bind *` works today and the keypad star is what fires it. On both producers
the main keyboard's shift+8 reports the key `'8'` and delivers the asterisk
only as a character event: the SDL producer maps `SDL_SCANCODE_1`-`_0` to their
digits before anything else, and `s_scantokey[0x09]` is `'8'`. Reporting
`K_KP_STAR` instead would leave every existing `bind *` with no key to fire it.

## Decision

The three producers keep reporting `'*'`. The maintainer took this decision on
2026-09-20 with the evidence above.

- It is what retail Quake III, RTCW SP, RTCW MP and Enemy Territory report on
  Windows and X11, and Windows x86 is the shared client proof target. Moving to
  `K_KP_STAR` would depart from retail behaviour rather than restore it.
- It keeps `bind *` working for anyone who has it.
- `K_KP_STAR` stays named in [`keys.c`](../../code/qcommon/keys.c), so a retail
  config line `bind KP_STAR ...` still loads, round-trips and lists, which is
  what the [key-name record](2026-09-20-key-names.md) corrected. A name in the
  table and a key no producer reports is retail's own arrangement on Windows,
  not a gap this engine introduced.

The commented-out fragments are replaced by the reason they are absent, each
pointing here. No mapping changed.

## Results

### Tests

- [`tests/input_system_source_tests.py`](../../tests/input_system_source_tests.py)
  gains `KeypadStarProducerTests`: all three producers report the character and
  none assigns `K_KP_STAR`; the other keypad keys keep their own key numbers;
  the extended branch still carries `'/'` and no `'*'`; and both character
  paths still deliver the asterisk, including `s_scantochar`'s two entries.
  The class docstring carries the decision and links here, so the next reader
  of those two lines finds the reason before editing them.
- Five mutations were applied one at a time and reverted: `K_KP_STAR` in each
  of the three producers, the Win32 extended case uncommented, and the star
  removed from `s_scantochar`. Each failed the new tests.
- The key, RTCW key-name, WebUI and input source suites pass: 154 tests over
  `input_system_source`, `key_name_source`, `q3_key_numbers`, `rtcw_key_names`
  and `webui_wiring`, up from the 151 the key-name record recorded.

No build was run for this record; the change is comments and tests.

## Differences

- A `KP_STAR` binding loads, survives a rewrite and shows in `bindlist`, and
  the key never fires. This is retail Quake III's own behaviour on Windows, and
  RTCW SP, RTCW MP and ET behave the same way.
- On macOS this engine uses its SDL producer and reports `'*'`, where retail's
  Mac clients report `K_KP_STAR`. One config means the same thing on every
  platform here, which the retail engines cannot claim among themselves.
- [`cl_webui.cpp`](../../code/client/cl_webui.cpp)'s
  `case K_KP_STAR: return 0x6au;` stays correct and unreachable: no producer
  reports the key it translates.

## Remaining

- Quake Live is unobserved. `quakelive_steam.exe` names 176 `KP_STAR` in the
  table the [key-number record](2026-09-19-q3-key-numbers.md) walked, but what
  its producer reports for a keypad star press was not established; no
  disassembly was available for this record and no QL runtime observation was
  taken. If it reports `K_KP_STAR`, the QL profile differs from the other four
  here, and this decision would need revisiting for that profile alone.
- No runtime gate. What this engine reports for a keypad star press is
  unobserved at runtime in every profile; the claim rests on the three
  producers' source, as the key-name correction's did.
- The layout reading is from one machine and one layout. Layouts that place the
  keypad star elsewhere, or keyboards without one, are unobserved.
- Elite Force's own engine was not read; `lilium-voyager` is an ioquake3 port of
  the game, not its original client, and is cited only as lineage evidence.
