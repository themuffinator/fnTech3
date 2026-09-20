# Elite Force single-player ambient sound sets

## Scope

This record covers Elite Force single player's ambient sound: the sets in
`sound/sound.txt`, the room tone the player walks through, the sets that
sound around a point, and the sounds a mover starts, runs and stops with.
Retail's own per-channel sound distances, lip-sync and Bink movies belong to
later slices. The profile stays behind `-Dstvef-development=true`.

The file's grammar is `code/compat/stvef_sp_ambient.hpp`, the sets and what
they play `code/client/client_stvef_sp_ambient.hpp`, and the loops' record
and gains `code/client/audio/shared/STVEFAmbient.h`. The cgame that asks for
them is described in the [cgame record](2026-09-19-stvef-sp-cgame.md).

## The file

| Contract | Retail `stvoy.exe` 1.20 | fnTech3 |
| --- | --- | --- |
| Where the sets live | `sound/sound.txt`, read once for each kind of set, so a keyword that belongs to another kind ends the set it appears in (`AS_ParseFile` `0x41d660`, `0x41d3c0`) | The same file and the same three readings, with the same keyword for each kind |
| The kinds | `generalSet` is a room's tone, `localSet` sounds around a point within its `radius`, `bmodelSet` holds a mover's sounds | The same |
| A set | `timeBetweenWaves` in seconds, `subWaves` as a directory and up to eight names, one `loopedWave`, and `volRange` from 0 to 255 (`0x41d1a0`, `0x41d260`, `0x41d330`) | The same, and a general set takes no radius and a mover set only its sounds |
| A wave's file | The sound directory, the name the file gives and `.wav` (`0x41d030`) | The same |
| Corrections | Swapped times and volumes are put right, each with a warning; more than eight sounds warns (`0x41cdf0`, `0x41d100`, `0x41ce60`) | The same warnings in retail's words. Retail writes the ninth sound and those after it past the end of its set, over the set's own fields; fnTech3 keeps the eight and reports each one beyond, so the retail file's `messhall`, which names thirteen, is read without damage |
| Unknown keywords | Reported once, and the line left (`0x41d1a0`) | The same |
| What is loaded | Every set the file names is kept, but only the ones the level asked for have their waves loaded (`AS_AddPrecacheEntry` `0x41d6e0`); a wave that will not load warns | The same, with retail's warning |

## What plays

| Contract | Retail `stvoy.exe` 1.20 | fnTech3 |
| --- | --- | --- |
| The room the player is in | The cgame hands the engine the set named by the ambient-set configstring and the player's own position every frame (`S_UpdateAmbientSet` `0x41dd20`) | The same, once the sound system has started |
| Walking into a room | The set the player leaves starts at full volume and falls to nothing over a second while the one they walk into rises from nothing; both play until the one behind is silent (`0x41d9f0`, `0x41d910`) | The same second, the same two sets playing |
| A looping wave | Submitted every frame at the volume its set has faded to, with at most 32 loops a frame and those playing one sound summed (`S_AddAmbientLoopingSound` `0x420bd0`, `S_AddLoopSounds` `0x420ca0`) | The same record and the same merging, in both of the engine's sound backends |
| A sub-wave | Once a fresh random delay of `timeBetweenWaves` seconds has passed, one of the set's sounds at a random volume from its range scaled by the set's volume, never above it, on the ambient channel (`0x41dc00`, `S_StartAmbientSound` `0x420680`) | The same delay, volume and channel |
| A set around a point | Full volume within half its radius, then falling to nothing at the radius; its sounds play at the point and belong to the entity that asked. The answer is when the set last played, which the game keeps for each of its sounding entities (`S_AddLocalSet` `0x41dda0`, `0x41da60`) | The same, and the client's own time for a set this engine does not know, as retail answers |
| A mover's sound | The set's sound for the stage asked for, or none where the set is not one the engine read (`AS_GetBModelSound` `0x41ddf0`) | The same. The sets are read as the cgame starts, so a level's movers ask before there are any and take their looping sound from the sets the level before left behind, as they do in retail |
| Distances | Full volume within 256 units of the listener, then falling by 0.0008 a unit to silence at 1,506, panned by the listener's right axis and the stereo separation, or both channels at three quarters on a mono device (`S_SpatializeOrigin` `0x420510`) | The same for the ambient loops, and, since the [sound distance record](2026-09-20-stvef-sp-sound-distances.md), for the rest of single player's sounds, each channel at the distance retail gives it and spread by retail's own `s_separation` |

## Results

A developer client with the `stvef-sp` profile, 1,024 by 768 in a window,
with an isolated writable home path, on `tutorial`:

- The retail `sound/sound.txt` reads as 103 sets, the number retail's own
  console reports for the same file, and the thirteen sounds of `messhall`
  are reported and the first eight kept.
- The level's 19 named sets load their waves: `s_list` shows 60 sounds under
  `sound/ambience` and `sound/movers`, among them
  `sound/ambience/voyager/conferenceamb.wav`,
  `sound/ambience/mishmash/mashroom.wav` and
  `sound/movers/doors/klingondoor.wav`.
- The room the player stands in is `voyconference`; it rises from 0 to 255
  over a second and stays there, and the software mixer carries its looping
  wave at 127 on each channel, the pan of a sound on the listener. The
  OpenAL backend holds the same loop as a voice of its own,
  `sound/ambience/voyager/conferenceamb.wav` at full gain and centred,
  belonging to no entity.
- The installation was left unchanged.

## Tests

- `stvef_sp_ambient_tests` compiles the file's grammar and the loops: a
  fixture covering every keyword, both corrections, a keyword of another
  kind ending a set, an unknown keyword, and more sounds than a set can
  hold; the loops' distances at the listener, at 256 units, at half volume
  and past silence, the pan on both sides, the mono device, the 32 a frame
  and the summing of loops playing one sound. It pins the cgame's five
  calls, the file being read as a level loads with only the level's sets
  loaded, the crossfade, the timing and volume of sub-waves, the local
  set's radius, and both sound backends.

## Remaining

Lip-sync, which the game reads from the engine's per-entity voice volumes,
and Bink movies. Retail's own distances for single player's other sounds are
in the [sound distance record](2026-09-20-stvef-sp-sound-distances.md).
