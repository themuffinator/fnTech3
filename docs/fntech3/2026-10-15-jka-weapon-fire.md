# Jedi Academy weapon fire for a local player, 2026-10-15

The [effect-sound slice](2026-10-15-jka-effect-sound.md#scripted-weapon-fire)
found that a scripted shot on `mp/ffa2` never reached a weapon. Four things
stopped it: the menu module kept the key catcher after a join, the module's
key event (UI export 3) was unserved, `+altattack` was not a command under
`jka-mp`, and `give all` stopped on a Ghoul2 refusal. This slice serves all
four, and a fifth found on the way, the client game's sound mute (import
36) that every weapon switch calls. A local player now joins, closes the
join menu with Escape, takes every weapon, switches to the concussion rifle
and fires both modes. Each shot plays its muzzle-flash effect and sound.
The listen server then stops on the game's Ghoul2 trace (game import 28),
which is outside this slice. The
[JSON companion](2026-10-15-jka-weapon-fire.json) records the binaries,
retail identities, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 (SHA256
`e8062d39...`) and `jk2mp.exe` 1.02a (`2f08dcf9...`) were read with
capstone and pefile. Pinned OpenJK `1a6a643` was used to locate routines
and name values (reference observation; no text incorporated).

## Observed, retail `jamp.exe` 1.01

### The key catcher after a join

Retail's engine never clears the menu module's catcher when a player
joins. Every write to the catcher (`0x8af104`) was listed by a linear
sweep of `.text`:

- The menu module's own Key_SetCatcher (UI import 42, `0x4246b8`) stores
  its argument as it is.
- CL_ShutdownUI (`0x425476`) clears bit 2, and CL_ShutdownCGame
  (`0x41ddde`, `0x44eacf`) clears bit 8.
- Connect and disconnect paths (`0x41d300`, `0x41d3c3`, `0x41d46c`,
  `0x41db49`) set it outright, before a client game exists.
- The console toggle clears the console bit while disconnected
  (`0x416ff1`). The message modes (`0x417039`..`0x41717e`) and Message_Key
  (`0x41ba6b`, `0x41bafd`) change bit 4 only.

CL_CmdButtons (`0x419e00`) sets BUTTON_TALK (2) whenever any catcher bit is
set (`0x419e66`..`0x419e6f`). So retail fires nothing while a menu holds
the keys, as fnTech3 did.

The menu that holds them is the client game's. At the first join the
server sends `nfr 6 1 3` (new force rank, open the menu). The client game
asks for menu 3 (UIMENU_PLAYERCONFIG), and the module opens its `ingame_player`
Player Configuration menu with KEYCATCH_UI. fnTech3's log shows the same
pair (`serverCommand: 8 : nfr 6 1 3` beside `Academy cgame requested UI
menu: 3`). A player leaves it with Escape or one of its buttons.

### Escape and the menu module's key event

CL_KeyEvent (`0x41c290`; `eax` down, then key and time on the stack)
handles a pressed Escape (27) by catcher bit. Bit 4 goes to the message
field and bit 8 to the client game's event export (`vmMain(8, 0)`). Bit 2
goes to `vmMain(3, 27, down)` on the menu module (`0x41c45b`..`0x41c465`).
With no bit set, an active player gets `vmMain(7, 2)`, the in-game menu,
and anyone else disconnects to the main menu. A released key goes to
`vmMain(3, key, 0)` while bit 2 is set (`0x41c504`..`0x41c508`). Otherwise
it goes to the client game's `vmMain(6, key, down)` while bit 8 is set
(`0x41c4c6`..`0x41c4ca`). CL_CharEvent (`0x41c640`) hands the module
`vmMain(3, character | 0x400, 1)` and drops `` ` `` and `~`. The export
takes two arguments: the key and the press. The time stays in the engine.

Retail has no `togglemenu` command (the name appears nowhere in the
executable); a player presses Escape. fnTech3's engine-owned `togglemenu`
sends the same Escape press and release.

The module closes every menu on Escape's press while a menu has focus.
Escape's release then finds no focused menu, and the module clears its own
catcher bit through import 42. The pinned OpenJK `UI_KeyEvent` behaves the
same way.

### Key numbers

The key-name table (`0x570bc8`, 320 rows of 16 bytes, the number at `+8`)
numbers keys as Academy's fakeAscii list does. Enter is 10, Backspace 8,
Tab 9 and Escape 27. The printable characters keep their own codes, and
Delete is 127. The function keys are 28-31, 132-135 and 149-152. The
wheel is 137 (up) and 139 (down), and the mouse buttons are 141, 142 and
166-168. The arrows are 170-173 and the keypad 11-25, 215 and 247. The
joystick buttons are 256-287 and the aux keys 288-319. The existing
binding-slot adapter (`jka_key_adapter.h`) already reads the same table.
Key events now use it in the other direction.

### Alternate attack and the button word

Both engines register `+altattack` with their `+button7` handlers:

| Executable | Registration | `+` / `-` handler | `+button7` |
| --- | --- | --- | --- |
| `jamp.exe` 1.01 | `0x41aae7` (`mov eax, name; mov ebx, handler; call 0x436da0`) | `0x4192b0` / `0x4192c0` | `0x41ac2c`, same handlers |
| `jk2mp.exe` 1.02a | `0x4080eb` (`push handler; push name; call 0x427980`) | `0x406f70` / `0x406f80` | `0x40823b`, same handlers |

Both CL_CmdButtons (`jamp.exe` `0x419e00`, `jk2mp.exe` `0x407660`) read
fifteen buttons (`0x883094`..`0x8831fc`, `0x81d9b4`..`0x81db1c`, 24-byte
records). They set `0x100` for "any key" when no catcher is set
(`0x419e85`, `0x4076b0`). That is Raven's BUTTON_ANY. Quake III's 2048,
which fnTech3 sent, is BUTTON_FORCE_DRAIN in both games. `jamp.exe` also
moves a force-power press (`0x200`) to use-holdable (4) when `+use` (32) is
held or the client game's export 20 says no force power is usable
(`0x419e34`..`0x419e63`).

### Copying a model past the end

The client game's import 308 (`0x413c13`) hands G2API_CopySpecificG2Model
(`0x4e04b0`) both handles and both indices. When the destination holds no
more models than the destination index (`0x4e04dd`..`0x4e04eb`), it grows
the instance to index + 1 places (`0x4de090`). It removes the destination
model's bone cache and assigns the `0xc8`-byte model record
(`0x4dea40`). The places it skips hold empty records. The game's import
600 reaches the same function.

### The sound mute

The client game's import 36 (`0x4128f8`) hands entity and channel to
S_MuteSound (`0x46ac30`). That zeroes the voice S_PickChannel (`0x46a110`)
would give a new sound on the same entity and channel. The pinned OpenJK
client game calls it with the player's weapon channel on every weapon
change (`cg_weapons.c`).

## Implemented

- **Escape reaches the menu module.** For `jka-mp` the UI dispatch in
  `vm.c` takes the engine's `(key, down, time)` and hands the module
  `(key, down)`. It refuses a press that is neither 0 nor 1. The count
  rule is `FNTech3_JKAUIForwardedArguments` (`jka_ui_exports.h`).
- **Key numbers.** `FNTech3_JKAKeyFromInternal` and
  `FNTech3_JKAKeyEventFromInternal` (`jka_key_adapter.h`) number key and
  character events as the retail table does. A key with no row is not
  delivered: mouse buttons 6-9, F13-F15, keypad equals and the pad
  buttons. `CL_ModuleKeyEvent` uses them for the menu module's and the
  client game's key events. Academy's character events drop `` ` `` and `~`.
- **`+altattack`** is button 7 for `jka-mp` and `jk2-mp`.
- **The button word.** Both Jedi profiles read fifteen buttons and set
  `0x100` for any key. Before, holding any key sent force drain.
- **Model copies past the end.** The shared Ghoul2 store grows the
  destination with empty places, as retail does. The client game's
  sparse-copy refusal is gone. A copy at or past the store's 32-model
  budget still refuses by name. The game servers for Academy and Outcast
  use the same store. Their copies past the end, which returned nothing
  before, now grow the instance too.
- **The sound mute.** Import 36 is served from the client game's
  sound-set host. It calls `S_MuteJKASound`, which ends that entity's
  sound on that channel in both mixers (a new `StopEntityChannelSound`
  entry in the OpenAL and legacy backends). Retail's automatic and
  less-attenuated channels never replace a sound (`0x46a15b`), so a mute
  on them ends nothing. When nothing plays on the channel, retail zeroes
  whichever voice its picker would take next. fnTech3 ends nothing then.

### Fn features (invariant 11)

Input stays fnTech3's: SDL key events, the engine's own `togglemenu`,
Fn's console, binding and menu-cursor paths. Only the numbers and the
argument count crossing the module boundary are retail's. The mute ends a
voice in Fn's OpenAL world or legacy mixer; the voice's spatialization,
occlusion, EFX and HRTF are unchanged up to that moment. No retail-exact
option is added.

## Not served, recorded

- **The game's Ghoul2 trace (game import 28).** Both concussion modes
  stop the listen server there, right after their muzzle flash. It is the
  next blocker for any shot that is traced on the server.
- **The client game's sprite (import 280, `CG_FX_ADDSPRITE`).** The Bryar
  pistol's charged alternate shot stops on it (`glx-fire-1`).
- **CameraShake.** The Bryar pistol's muzzle flash stops on it
  (`glx-fire-2`), as the effect-sound sweep predicted.
- **Force-power remap.** Retail's `0x200` to use-holdable move needs the
  client game's export 20; it is not served. The force-power and
  inventory commands (`+useforce`, `+force_grip`, `force_*`, `use_*`, the
  `generic_cmd` byte) are not registered for either Jedi profile.
- **Retail live run.** Retail `jamp.exe` 1.01 started as an isolated
  client (`fs_homepath` and `fs_cdpath` in `.tmp`, UDP and IPX off, sound
  off, windowed, no input) loads its menu module and stops at `NEED CD`
  before hosting. The installation was unchanged. Passing that check would
  bypass copy protection, so retail's catcher behaviour comes from the
  static reading above only.

## Live

All runs were on `mp/ffa2` with `g_gametype 0` and the GLx renderer, using
`listen.py` (from the effect-sound slice). The client is windowed on a private
desktop with `net_enabled 0`, sound on but muted offscreen, and no input.
The runtimes were built from a `git archive` of `34ecbab5` with this slice's
files laid over it, under `win32-dev`'s options (strict warnings). Every
run left the installation and binaries unchanged.

| Run | Steps after the join | Outcome |
| --- | --- | --- |
| `glx-catcher-1` | `togglemenu` three times, captures | Catcher `0x0002`, then `0x0000`, then `0x0002`. The Player Configuration menu shows before the first `togglemenu` and is gone after it. The second opens the in-game menu and the third closes it. The run exits normally. |
| `glx-fire-1` | `give all`, `weapon 15`, `+altattack` | `give all` completes. `weapon 15` names no weapon (the client game adds 2), so the Bryar's alternate charge stops on import 280. |
| `glx-fire-2` | `give all`, `+attack` | The Bryar fires; its muzzle flash stops on CameraShake. |
| `glx-conc-3` | `give all`, `weapon 13` | Before the mute was served: the switch stops on import 36. |
| `glx-conc-4` | `weapon 13`, `+attack` | Switch served. `effects/concussion/muzzle_flash.efx` plays with its sound (handle 373). The server then stops on game import 28. |
| `glx-conc-5` | `weapon 13`, `+altattack` | `effects/concussion/altmuzzle_flash.efx` plays with its sound (handle 374). The server then stops on game import 28. |

The concussion rifle's shot, alternate ring, hit and miss effects compile
in the sweep but are not reached live: the server's trace stops first.

## Tests

- `tests/jka_key_event_tests.cpp` (new, `fntech3_jka_key_event`) checks
  the retail table's numbers, unknown keys, and that every delivered
  number names the same key in the binding table. It also covers
  character events and the UI key export's argument count.
- `tests/jka_weapon_fire_tests.py` (new, `fntech3_jka_weapon_fire`)
  compiles `S_MuteJKASound` and the legacy mixer's stop from their
  sources. It checks the channels a mute ends and that other profiles are
  refused. It also checks the OpenAL stop, the import-36 case, the
  model-copy refusal and the UI key path.
- `tests/rtcw_player_interaction_tests.py` runs `+altattack` for both
  Jedi profiles. It checks button 7, fifteen buttons, the `0x100` any-key
  bit and BUTTON_TALK under a catcher.
- `tests/q3_key_numbers_tests.py` adds Academy to the key-event probe.
- `tests/raven_ghoul2_server_tests.cpp` covers copies past the end: the
  empty places, a later fill, a copy within one instance and the budget.
- `tests/jka_cgame_ghoul2_tests.py` now expects import 308 to grow the
  instance past its end, and to refuse a place at or past the budget.
- `tests/jka_cgame_ambient_tests.py` invokes import 36 through the host,
  with its entity bounds. `tests/jka_sp_vm_export_tests.py` gives its
  `VM_Call` fixture the new argument rule.

This slice laid over `750f070f` built all 1,484 targets under `win32-dev`'s
options with strict warnings. Every `fnTech3` meson test but the pytest
wrapper passed there, once the SP export fixture had its stub; one timeout
under load passed when rerun alone. HEAD then moved to `24bcfb49`, which
touches renderer files only. On that tree with this slice, 32 related
Python files passed (344 tests). The full Python suite on it passed 2,704
tests with none failing; 131 skipped for absent optional tools or retail
paths, and the 49 release-packaging tests, which need staged binaries,
were left out.
