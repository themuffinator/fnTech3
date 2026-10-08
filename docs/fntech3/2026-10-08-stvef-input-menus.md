# Elite Force played and set up through the engine's own input, 2026-10-08

The Elite Force input gate had input-disabled preflights only: moving,
looking and firing, rebinding a key in the menus, and saving and loading
through them were left to a person at the machine. The user approved the
engine's own synthetic input for this task's Elite Force probe windows on
private desktops (2026-10-08). They chose not to run the hands-on checks
with a real keyboard and mouse, or by ear. No force-feedback device is
available here. With synthetic input, both modes now move, look, fire,
rebind a key through their controls menus and keep it, and single player
saves, lists, loads and respawns through its own menus. The save list then
showed that every single-player save fnTech3 wrote had its menu line as
stray bytes from the engine's memory. Saves now carry retail's line. The
[JSON companion](2026-10-08-stvef-input-menus.json) holds the identities,
commands, replies, screenshots and save bytes.

## Method

`scripts/probe_stvef_input.py` starts a windowed client on an unswitched
private desktop with an isolated home, no sound and `+set in_synthetic 1`.
That switch is init-only: it lets `in_synthesize` queue key, character and
relative mouse-motion events in the engine's own event queue, where the
platform layer puts real input. Nothing reaches the operating system's
input. Each step is driven over loopback rcon, running small exec'd cfgs.

- **Play:** the probe holds the key bound to `+forward`, sends 400 units of
  motion and holds the key bound to `+attack`. The cgame's `viewpos`
  reports the position and yaw. In single player, firing is measured by the
  ammunition the engine carries out of a level: `maptransition` to the
  same level, before and after, writes `playerammo*` from the live player.
  In Holomatch, engine screenshots show the HUD. A key bound from the
  console must run its binding when pressed.
- **Menus:** scenario files point and click at the 640 by 480 menu
  coordinates. The synthesized cursor lands where it is pointed, as the
  screenshots show. Each screen is captured with the engine's `screenshot`
  command.

## Results

| Check | Holomatch (`hm_borg1`) | Single player (`borg2`) |
| --- | --- | --- |
| Default bindings | `w`, `uparrow` `+forward`; `mouse1` `+attack` | the same |
| Forward key held 1.5 s | moved 588.5 units | moved 458.5 units |
| 400 units of motion | yaw turned 18 degrees | yaw turned 15 degrees |
| Attack key held | HUD ammunition 50 to 38, a scorch on the wall at the crosshair | compression rifle, 2 s: carried ammunition 300 to 288 |
| A console binding | ran when its key was pressed | ran |
| Rebind through the menus | Esc, Configure, Controls, Weapons, Weapon 9, then K: the screen shows `9 OR K`, `bind k` reports `weapon 9`, and `hmconfig.cfg` keeps `bind k "weapon 9"` | Esc, Configure, the same: `bind k` reports `weapon 9`, kept in `efconfig.cfg` |
| Save through the menus | | Esc, Save Game, a typed description, Save: `Saving game "eforce0"`, `saves/eforce0.sav` |
| Load through the menus | | Esc, Load Game: the save is listed with its time and date; selected, it shows its picture and level; Load: `Loading game "eforce0"`, borg2 restored |
| Death | | `kill`, then the attack key: `Loading game "auto"`, borg2 starts again |

Twenty-two menus single player opens by console command, with no input,
are in the campaign record (`2026-10-08-stvef-sp-campaign.md`).

## A save's menu line

A single-player save begins with a 128-byte comment chunk. Retail clears it,
writes the comment, `-------> <map>` for an autosave or the time, date and
level when a save has no comment, and then copies a second line to byte 64:
`_strtime`, `_strdate` with two spaces and the level, as `"%s %s %s"` within
63 characters (`stvoy.exe` `SG_WriteComment` 0x426f40; cleared at 0x426f72,
the line built at 0x4270d6 and copied at 0x42710e). The load and save menus
list a save by its comment and that line. A retail save in the installation
holds `16:58:59  07/15/20    scav5` and, at byte 64,
`16:58:59 07/15/20   scav5`.

fnTech3 wrote the comment into an uncleared buffer and no second line. Every
byte after the comment was whatever the stack held, and the menus showed
`ÿ ÿ` beside every save. The chunk is now cleared, and the line is written
as retail writes it. The same menu save's chunk before and after:

| Bytes | Before | After |
| --- | --- | --- |
| 0 to 63 | `probe menu save`, then stack bytes | `probe menu save`, then zeros |
| 64 to 127 | stack bytes | `20:23:48 10/08/26   borg2`, then zeros |

Saves written before this change still load: neither engine reads the second
line to load a save. Their list line stays as it was written.

## Evidence

- `tests/stvef_sp_savegame_tests.py` builds the chunk from the production
  helpers: an autosave and a dated save with their lines at byte 64, retail's
  own scav5 save reproduced, a level name cut to 63 characters with the last
  byte clear, and a comment longer than 63 characters running into the line,
  as in retail. A source check pins the cleared buffer and the order of the
  writes. Both savegame test files pass (19 tests).
- The edited files compile with `meson/build/win32-dev`'s exact MSVC
  commands. Runtimes come from a `git archive` export of `45444bd3` with this
  change; the probes ran on its client.

## Not covered

- Physical devices: a real keyboard, mouse or joystick, and listening.
  The user chose not to run them. The probes enter input through the
  engine's event queue, so the platform layer's own capture, which every
  profile shares, is not exercised here.
- Force-feedback output: no device is available. The bridge's rules have
  their own evidence ([feedback record](2026-09-21-stvef-force-feedback.md)).
