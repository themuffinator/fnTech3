# Jedi menus driven by mouse and keys (2026-10-17)

The [single-player menus record](2026-10-16-jedi-sp-menus.md) left the Jedi
single-player menus with source evidence only for their mouse and keyboard
handling: no click or key had been sent to them live. The user approved the
engine's own synthetic input (`in_synthesize`) for Jedi menu probes on a
private desktop, with no operating-system input, so the probes now click and
type through both single-player games' engine menus and both multiplayer
games' retail menu modules. This found and fixed two things in the
single-player menus. No gate closes.

Evidence levels follow the [ledger](COMPATIBILITY.md): Source (retail
disassembly read here), Unit (tests), Live (engine runs).

## Synthetic input for the Jedi profiles (Source, Unit)

`in_synthesize` queues key, character and mouse events in the engine's own
event queue, where the platform layer puts real input, and works only in a
client started with `+set in_synthetic 1` (an init-only switch). It was
registered for the Ritual, Medal of Honor, Wolfenstein and Elite Force
profiles; it is now registered for `jka-sp`, `jka-mp`, `jk2-sp` and `jk2-mp`
too (`code/qcommon/keys.c`). The Jedi menus take mouse motion as deltas on a
640x480 cursor, so a probe points by moving the cursor to the corner and then
out by the target's virtual coordinates.

## What driving the menus found (Source, Live)

- **Academy's text language** (setup, game options, "Text") was blank. A
  `cvarStrList feeder` item on feeder 23 lists the installed languages:
  `jasp.exe`'s item parser (`0x4a1d76..0x4a1e8b`) fills it with one entry
  per language, each shown as `@MENUS_MYLANGUAGE` with the language's name
  (`SE_GetLanguageName`, `0x434f00`) as its value. fnTech3 now fills it the
  same way once the menus are read; the line reads "English". The same
  parser fills feeder 16 with the player species, which belongs to character
  creation and stays unserved. The parser also confirms the choice lists'
  limit of 64 (`cvarStr` at `+0x100`, the count at `+0x300`).
- **The key-binding prompt** is `MENUS_WAITINGFORKEY` in both engines
  (`jasp.exe` `0x4998d1`, `jk2sp.exe` `0x49bb15`); fnTech3 had asked
  Outcast's packages for `MENUS3_WAITINGFORKEY`. Neither game's string
  packages contain the key, so the prompt is blank in the originals and in
  fnTech3 alike.

## Live

Each run: a client built from HEAD `f4794432` plus this change
(`fntech3.exe` `ca60e96e...`), windowed at 1280x720 on a private desktop,
`net_enabled 0`, an isolated home with an empty config, every input through
`in_synthesize`, engine `screenshotPNG` captures. The companion lists each
run's actions, executable hash, captures and outcome.

| Run | Profile | What the input did |
| --- | --- | --- |
| menus | `jka-sp` | hovering New lit its glow, echo and description; Controls opened; clicking Walk Forward greyed the rest and pulsed the binding; `k` bound it, which cleared `w` and `uparrow` as the originals' two-key rule does (`bind` printed `"k" = "+forward"`); Escape returned to the main menu; Setup's game options page cycled Draw Crosshair (`cg_drawcrosshair` 1); Escape to the quit menu and No back; Down twice walked the focus from the last hovered item round to New; Enter opened the skill menu |
| slider | `jka-sp` | a click on the effects volume bar set `s_volume` from 0.8 to 0.5 |
| menus | `jk2-sp` | the same steps through Outcast's own layout, with the same binding and crosshair results; Enter opened the new-game menu |
| text field | `jk2-sp` | the save menu's description field took the typed text (`ui_gameDesc` "probe saves") and ended on Enter |
| menus | `jka-mp` | the retail menu module: hovering Play lit it with its description, a click opened Setup, Escape went back and to the quit menu and back, and the arrows moved the focus |
| menus | `jk2-mp` | the same through Outcast's retail menu module, which opened its player set-up page |

Every run exited by itself, ran no menu script fnTech3 does not serve,
raised no error, and left the installation unchanged. A synthetic Backspace
key does not erase in a text field: like the originals' field code, the menus
erase on the typed Ctrl-H character that a real Backspace also produces, and
`in_synthesize key` sends no character.

## Found on the way

- Outcast multiplayer's retail menu module registers its video set-up cvars
  (`ui_r_glCustom` and the rest) with flag `0x200000` when its setup menu
  opens, and fnTech3's cvar check refuses that flag with a warning for each.

## Tests

`tests/raven_menu_tests.py` now checks the language list and the binding
prompt's key; `tests/ritual_synthetic_input_tests.py` pins `in_synthesize`
for the Jedi profiles.
