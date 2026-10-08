# RTCW SP level exits: carried players and the reload guard

Return to Castle Wolfenstein single player had been checked one level at a
time: every map starts, saves and loads, but no level had been left by its
own exit. This record plays nine level changes that the retail game makes
itself -- four exits that carry the player into the next level and five
cinematic levels that hand over to the next episode -- and finds one engine
difference on the way: retail ignores a map command while its game is
changing level, and fnTech3 did not. The
[machine-readable record](2026-10-08-rtcw-sp-level-exits.json) holds the
identities, every run's observations and the exit inventory.

## How a level ends

Observed in the retail data (`maps/<map>.ai` and the BSPs in `pak0.pk3`, read
only) and the [RTCW SP source](https://github.com/id-Software/RTCW-SP)
(`src/game/ai_cast_script_actions.c`, `g_main.c`, `g_save.c`, read only):

- Each level's player script holds a trigger whose body is
  `changelevel <next> [delay] [persistent] [silent]`. An `ai_trigger` brush
  named for the player fires it when the player touches the brush, or another
  script does.
- The game's `changelevel` does nothing unless every objective the player
  script asked for (`objectivesneeded`) is met; otherwise it sends
  `cp objectivesnotcomplete`. With `persistent` it writes the player's entity,
  client and cast state to `save/current.psw`, headed by the next map's name,
  and puts a random number in the `persid` cvar.
- It fades out and sets `g_reloading` to 4. When the delay (8 s unless given)
  and one more second have passed, it sets `g_reloading` to 2 and issues
  `spdevmap <next>` (`spmap` without cheats).
- On the next level, once the player is connected, the game reads
  `save/current.psw` if the map name and `persid` both match, then sets
  `persid` to 0. The level's briefing then holds the game, with `g_reloading`
  at 1, until its Ready button is clicked.

All 26 levels' exits were read from their own scripts and entities. Only four
can be reached from the console alone, with no use key, shooting, pickup or
fight: their objectives and exit are all `ai_trigger` brushes (escape1,
village1, dig, castle). The others need the use key on a door, button or
switch (escape2, crypt1, rocket, assault, factory, xlabs, dam), something
shot or blown up (church, baseout, norway), a script that follows a fight or
an earlier objective (tram, crypt2, boss1, forest, sfm, swf, boss2, end), an
objective set outside the player script (chateau, dark, village2), or an exit
that starts switched off (dark). The JSON lists each level's objectives,
their sources and its exit.

## The probe

[`scripts/probe_rtcw_transitions.py`](../../scripts/probe_rtcw_transitions.py)
starts the first level of a chain with `+spdevmap`, windowed at 800×600 on an
unswitched private desktop, with an isolated home, the GL renderer, OpenAL,
`net_enabled 0`, and `aicast_debug 2` limited to the player, so the player
script's events and commands are logged. Its client carries the private hook
[`tests/rtcw_transition_probe.hpp`](../../tests/rtcw_transition_probe.hpp),
built in a separate export of the tested source so the shared tree's client
is never instrumented. On each level the hook:

1. clicks the briefing's Ready button -- the standing AGENTS.md exception for
   intro and briefing screens, and the only input in these runs;
2. waits for the level's opening camera to finish;
3. runs the level's step file, console commands only: `god`, `notarget`,
   `give Venom`, `give armor`, then `setviewpos` into each objective trigger
   and finally the exit trigger, 240 frames apart. Each point is the centre of
   one of the trigger's own brushes (from the BSP's brush and plane lumps),
   at its floor plus the player's 24 units;
4. logs the player's state (health, armour, weapon bits, ammunition) when the
   level becomes active, at the briefing, when play starts and every 10 s;
5. on the chain's last level, takes an engine screenshot and quits.

Cinematic levels have no step file: their scripts end in `changelevel`
themselves. The give and teleport commands are cheats, so these runs skip the
levels' fights; they test the exit, the level change and the carried player,
not play.

## Results

Twelve chains ran on the corrected client, one process each, with one
Ready click per level:

| Chain | Level changes the game made | Objectives met before the exit | Held on arrival | Run |
| --- | --- | --- | --- | --- |
| `escape` | cutscene1 → escape1 (cinematic); escape1 → escape2 (persistent) | escape1: 2 of 2 | knife, Venom, 100 armour, 100 health | 304 s |
| `village` | village1 → crypt1 (persistent, 4 s delay) | 2 of 2 | Venom and its ammunition, 100 armour | 131 s |
| `castle` | dig → castle → end (both persistent) | dig: 2 of 2; castle: 1 of 1 | Venom and its ammunition, 100 armour, on both | 163 s |
| `forest` | cutscene6 → forest (cinematic) | — | nothing: not persistent | 147 s |
| `sfm` | cutscene9 → sfm (cinematic) | — | nothing: not persistent | 153 s |
| `norway` | cutscene11 → norway (cinematic) | — | nothing: not persistent | 115 s |
| `dam` | cutscene14 → dam (cinematic) | — | nothing: not persistent | 332 s |
| `control-escape2`, `-crypt1`, `-castle`, `-end` | none: each level started directly | — | no weapon, no armour | 17–123 s |
| `guard-briefing` | none: see the correction below | — | — | 18 s |

Facts from the logs:

- Every level change was the game's own: the player script's
  `changelevel` line appears and the next level's `Server:` line follows. In
  seven of the nine, a 10-second observation fell inside the fade and read
  `g_reloading` 4. No level was started by the probe.
- Each persistent exit wrote `save/current.psw`; on the next level `persid`
  arrived with the written value and read 0 at the briefing, so the game
  found the file, matched it and read it.
- The Venom, its ammunition and 100 armour, given on the first level of a
  chain, were held on every later level of it. Started directly, the same
  levels (escape2, crypt1, castle, end) begin with no weapon and no armour.
- Every cinematic level ran its script to its own `changelevel` and loaded
  the next episode's first level, whose briefing came up and was dismissed.
- No engine error was logged; every client quit by itself.

Inferences: the carried state is the retail game's own record, written and
read through fnTech3's file system, cvars, map command and level load, so
those engine paths serve a persistent level change as the game expects. No
retail `WolfSP.exe` run of the same chains was made -- its briefing needs a
real click on its window -- so the timing of the fades and loads is not
compared.

A first sweep, on a client without the correction, passed the same ten
chains it had, but village1 stayed on its level: the probe had aimed at
the centre of objective 2's bounding box, which falls between the five thin
brushes of that trigger, so the objective was never met and the exit refused
with `objectivesnotcomplete`, as the game should. Aiming inside one brush
fixed it.

## Correction: map commands while the game changes level

The retail Steam `WolfSP.exe` (`Wolf 1.32`, SHA-256 in the JSON) returns
from its map command without a word
while `g_reloading` holds anything but 2, unless `com_buildScript` is set:
`SV_Map_f` reads the cvar at `0x430c6d` and returns at `0x430c7c`. The GPL
source has the same test (`src/server/sv_ccmds.c`) and registers `map`,
`devmap`, `spmap` and `spdevmap` all on that function. The game holds 1 through each level's briefing,
4 through an exit's fade, 8 through a failed mission's reload and 16 at the end
of the campaign. Retail's disconnect command clears `savegame_loading` and
`g_reloading` before it disconnects (`CL_Disconnect_f`, `0x40b790`), so a level
left mid-change cannot keep refusing.

fnTech3 had neither. A map typed at a briefing started at once, under the
game's own level change. Since commit `f3f7b042` the RTCW SP map command
ignores it as retail does, and the RTCW SP disconnect command clears both
states first. The RTCW MP source has no such test. Enemy Territory's source
applies it only in its single-player game types; that profile is neither
changed nor measured here.

The `guard-briefing` chain issues `spdevmap crypt1` from the console while
escape2's briefing holds the game, then clicks Ready:

| Client | After the command | Levels |
| --- | --- | --- |
| before (uncorrected export) | crypt1 loads | escape2, crypt1 |
| after | nothing; escape2's briefing stays up until Ready | escape2 |

## Tests

- [`tests/rtcw_server_policy_tests.py`](../../tests/rtcw_server_policy_tests.py)
  runs the real map command in every profile with `g_reloading` at 1, 2, 4, 8
  and 16 and `com_buildScript` off and on: only RTCW SP refuses, and only
  when the game is not starting its next level and no build script runs.
- [`tests/rtcw_sp_disconnect_tests.py`](../../tests/rtcw_sp_disconnect_tests.py)
  runs the real disconnect command against stubs: RTCW SP clears both states
  right after the cinematic stops, before any shutdown or error, and no other
  profile touches them.

## Remaining

The campaign changes level 32 times, counting the final cutscene's end in
the [endgame record](2026-09-18-rtcw-sp-endgame.md). Ten have now been
played; the other 22 need the use key, shooting or fights. The played exits
skip their levels' fights, so full campaign traversal, sustained AI and
combat, and a retail comparison of the same chains remain open.
