# Jedi Academy's trace with pass entity -1, 2026-10-16

The [menu slice](2026-10-15-jka-ui-menus.md) took Jedi Academy multiplayer
(`jka-mp`) siege games past the class menu. On `mp/siege_korriban`
(`g_gametype 7`), with `team red` and `addbot Tavion 1 blue` from the
console, the round then counted down and the server stopped at "Jedi Academy
trace cannot read pass entity -1 or the entities it touched". The retail
game module passes -1 to the server trace for "ignore nothing". This slice
measures what retail does with that number and serves it as no pass entity,
as fnTech3's Jedi Outcast trace already does
([Outcast records](2026-09-22-jk2-records.md)). The
[JSON companion](2026-10-16-jka-trace-pass-none.json) records the
binaries, retail identities, call sites, commands and outcomes. No
compatibility gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 and the
two retail `jampgamex86.dll` builds were read with capstone and pefile.
Pinned OpenJK was used only to name the module's routines (reference
observation; no text incorporated).

## Retail

### Observed, `jamp.exe` 1.01

- The server trace's entity loop (`0x462490`) reads the pass entity once,
  before the loop (`0x4624e6..0x46252e`). It compares the number only with
  1023 (`cmp eax, 0x3ff` at `0x4624f8`). Any other number, -1 included,
  is multiplied by the record size (`[0xc31798]`) and added to the game's
  entity array (`[0xc31794]`): the record's owner is read at `+0x294`
  (`0x462504`) and its flags at `+0x238` (`0x462522`). An owner of 1023
  becomes -1 (`0x462517`). No bound is checked, so -1 reads the record in
  front of the array.
- In the loop, a pass entity other than 1023 brings in Raven's owner rules
  (`0x462583..0x4625e2`): a touched entity equal to the pass number, owned
  by it, owned by its owner, or a missile of its owner, is skipped.
- The client game's trace imports carry no pass entity. Import 30
  (`CG_CM_BOXTRACE`, served at `0x412746`) hands the collision model its
  start, end, box, model handle and mask (`0x435fe0`), and imports 31 to 33
  are its capsule and transformed forms. -1 can reach only the game's
  imports 27 and 28.

### Observed, `jampgamex86.dll`

Both retail builds were read: 1.01 from `assets3.pk3` and 1.00 from
`assets2.pk3`, which `assets3.pk3` shadows.

| | 1.00 (`assets2.pk3`) | 1.01 (`assets3.pk3`) |
| --- | --- | --- |
| `g_entities`, handed to import 17 | `0x20548e40` | `0x20630fe0` |
| record size | `0x5ec` | `0x5ec` |
| record -1 | `0x20548854` | `0x206309f4` |
| owner read (`+0x294`) | `0x20548ae8` | `0x20630c88` |
| flags read (`+0x238`) | `0x20548a8c` | `0x20630c2c` |
| sites passing a literal -1 | 7 | 7 |

- Both import-17 sites hand the same array: the second (`0x200b0c72` in
  1.01) loads `level.gentities` (`[0x207eea64]`), which the first site's
  function sets to `0x20630fe0` (`0x200904a3`).
- In front of the array lie the module's own cvar records (`vmCvar_t`,
  each found through the cvar table's entry). In both builds the owner word
  read is the float value (`+8`) of `g_forceBasedTeams`, default "0", and
  the flags word is byte 188 of `g_podiumDist`'s string, default "80".
- 234 calls to the trace wrapper (import 27) in 1.01: 27 pass 1023, 2 pass
  0, 7 pass -1, 195 pass a register and 2 could not be read; the 6 calls to
  import 28 pass a register. The seven -1 sites all pass `MASK_SOLID`
  (`0x1001`). OpenJK names them as the bot's melee and saber combat ground
  checks below itself and its enemy (`MeleeCombatHandling`,
  `SaberCombatHandling`, three each) and the seeker drone's line of fire
  (`SeekerDroneUpdate`). The 1.00 build has the same seven.

### Inferred

- With either retail module, retail reads -1's owner as the bit pattern of
  `g_forceBasedTeams`' value: 0 at the default 0, `0x3f800000` at 1. The
  flags read 0 unless `g_podiumDist` holds more than 188 characters.
- At the defaults, a retail -1 trace therefore skips, besides what 1023
  skips, any entity owned by -1 and any shared entity (no `0x1000` flag)
  owned by client 0. With `g_forceBasedTeams 1` it skips only those owned
  by -1. Which entities are skipped depends on an unrelated cvar and on the
  module's memory layout; the module's source asks for no pass entity.
- A register-passed number cannot be settled statically; a computed -1
  would reach the same code.

## fnTech3 before

`fntech3::jka::trace::Run` (`code/compat/jka_trace_clip.hpp`) read the
pass entity through the world's `Read`, which refuses a number outside the
game's records. A -1 failed the read, `Run` returned false, and
`SV_JKATrace` (`code/server/server_jka_trace.hpp`) stopped the server.

## Implemented

- **-1 is no pass entity.** `Run` now reads `PassNothing` (-1) as
  `EntityNone` (1023) before the pass read. The trace then sweeps everything
  in its way, as it does for 1023. This is the reading the module was
  written for, and the one Outcast's trace gives.
- **Retail's out-of-bounds read is not reproduced.** What retail reads in
  front of the array is the module's own memory. It depends on a cvar that
  has nothing to do with the trace, and for another module build or a mod
  it could be anything, or unmapped. At the defaults the difference is
  only for the seven `MASK_SOLID` traces above: retail lets them pass
  through solid entities owned by client 0 or by -1, and fnTech3 does not.
- **Every other number is still refused.** -2 and below, and numbers past
  the game's records, still fail `Read`, and the server stops with the same
  message: the server cannot read them, and retail would read memory
  outside the array for them too.
- **Presentation.** None: this is the server's collision contract.

## Tests

- `tests/jka_g2_trace_tests.cpp`, built by Meson as `fntech3_jka_g2_trace`,
  includes the production header. A new case puts client 0, an entity owned
  by client 0, one owned by -1 and another in the way of a fake world with
  no record -1. A -1 trace sweeps all four in order, hits the nearest, and
  matches the 1023 trace; a shot mask with the Ghoul2 flag sweeps all four
  too. -2, 1024 and -1024 are refused.
- Three mutations of the header each fail it: no -1 reading (the read
  fails), any negative number read as none (-2 and -1024 accepted), and -1
  read as client 0 (client 0 and its entity skipped).
- No Python fixture slices `Run`, `SV_JKATrace` or `ServerJKATraceWorld`;
  the C++ test is their only test consumer.

## Live

The client ran windowed at 800x600, on a private desktop, with no network
sockets, sound off and no input; the console commands came from the probe's
own config (`.tmp/round-2026-10-15/jka-local-sets/probe_live.py`). Each run
had its own home, and the installation was unchanged. Captures are the
engine's `screenshotPNG`. The two runtimes were built from a `git archive`
export of `d17f578a`, without and with this slice; the slice rebuilds only
`sv_game.cpp`.

| Run | Runtime | Outcome |
| --- | --- | --- |
| `trace-pass-before-1` | before | Tavion connects; the server stops at the -1 trace before either player enters |
| `trace-pass-after-1` | after | both enter, the round counts down (`cs 293 "2|900"`) and begins (`cs 293 "0|5950"`); plays to the probe's end |
| `trace-pass-after-long-1` | after | the round plays about 40 s; Tavion dies, the server sends `ircg 1 32 0 0`, and the client stops at client-game import 315 (`CG_G2_CLEARSKINGORE`) |

The `trace-pass-after-1` captures show the siege class menu still open over
the game, now counting one player on each side.

**The next stop** is client-game import 315 (`CG_G2_CLEARSKINGORE`), asked
for when the server's `ircg` command reaches the client game for client 1
after the bot's death. The menu slice's diagnostic runs stopped at import
215 first; HEAD now serves 214 to 216 ([zoom record](2026-10-15-jka-zoom.md)).

## Remaining and unrelated findings

- Client-game import 315 is the next siege stop on `mp/siege_korriban`.
- `mp/siege_hoth` and `mp/siege_desert` still stop at client-game import
  312, as the menu slice recorded.
- The engine's own botlib trace (`BotImport_Trace`, `code/server/sv_bot.cpp`)
  already reads -1 as no pass entity for every profile.
