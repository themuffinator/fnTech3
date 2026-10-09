# Holomatch's game types and Expansion Pack modes with bots, 2026-10-09

The retail Holomatch game (`vm/qagame.qvm`) plays every game type and each
Expansion Pack mode with bots on fnTech3's dedicated server as on
`stvoyHM.exe`'s. In thirteen cases, each run on both servers, the two leave the
same modes on after the engine's conflict rules, run the same game type, and
report no error. How a game ends depends on the bots' play, which varies from
game to game on either server. The [JSON companion](2026-10-09-stvef-holomatch-modes.json)
holds every case's settings and results, from this sweep and the first one.

## How it is played

`scripts/probe_stvef_modes.py` starts each server bound to 127.0.0.1 with an
isolated folder, sets the case's game type and any Expansion Pack mode
(`g_pModAssimilation`, `g_pModDisintegration`, `g_pModActionHero`,
`g_pModSpecialties`, `g_pModElimination`) before `devmap`, adds two to six
skill-2 bots and plays at `timescale 3` with a three-minute time limit, ten
frags or three captures. It reads the game's own log for the exit, kills and
the modes in `InitGame`. No client takes part; retail reads a copy of the
retail paks, as every Elite Force probe does.

## Results

On `bada7c65…` (`8da1234c` with the [frame-time](2026-10-08-stvef-frame-time.md),
[serverinfo](2026-10-08-stvef-serverinfo.md) and
[world-record](2026-10-09-stvef-world-record.md) changes) against `stvoyHM.exe`
1.20:

| Case | Map, game type | Modes on (both) | Retail | fnTech3 |
| --- | --- | --- | --- | --- |
| ffa | `hm_borg1`, free-for-all | none | time limit, 6 kills | time limit, 9 |
| ffa-disintegration | `hm_borg1`, free-for-all | Disintegration | time limit, 16 | frag limit, 28 |
| ffa-actionhero | `hm_voy1`, free-for-all | Action Hero | time limit, 9 | time limit, 15 |
| ffa-specialties | `hm_dn1`, free-for-all | Specialties | time limit, 9 | time limit, 14 |
| ffa-elimination | `hm_kln1`, free-for-all | Elimination | last man standing, 3 | last man standing, 3 |
| tournament | `hm_voy2`, tournament | none | time limit, 5 | time limit, 4 |
| team | `hm_for1`, team | none | time limit, 8 | time limit, 6 |
| team-assimilation | `hm_borg2`, team | Assimilation | assimilation complete, 7 | none in the probe's time, 2 |
| team-elimination | `hm_scav1`, team | Elimination | last man standing, 1 | last man standing, 1 |
| ctf | `ctf_voy1`, capture the flag | none | capture limit, 9 | time limit, 12 |
| ctf-specialties | `ctf_kln1`, capture the flag | Specialties | time limit, 5 | time limit, 2 |
| ffa-asks-assimilation | `hm_cam`, free-for-all | none: Assimilation needs team play | time limit, 3 | time limit, 1 |
| team-asks-actionhero | `hm_noon`, team | none: team play refuses Action Hero | none, 0 | none, 0 |

The differing exits come and go between runs on both servers: in the first
sweep (`088a4f40…`, before the frame-time change) and in four repeats of the
team cases, retail's Assimilation games also ran out of the probe's time
twice, retail's capture the flag ended on its time limit three times in five,
and both servers' free-for-all Disintegration games reached the frag limit.
The two-bot team game that asks for Action Hero ends unfought on both: on
each server both bots join the same team, so neither has an opponent and
the game runs past the probe's time. The frame-time record covers the bots' team
joins and kill rates under `timescale 3`, and the world-record record the
Elimination fault this sweep first found.

Every run left the retail installation unchanged.
