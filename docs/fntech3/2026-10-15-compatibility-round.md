# Compatibility gap resolutions, 2026-10-15

This round resolves two recorded gaps, Jedi Academy multiplayer's sound sets
heard around a point and Enemy Territory's close of an area portal that is
already shut, and corrects stale ledger text. Each change was built and
tested as HEAD plus this round's files alone, without another session's
uncommitted work. Its [JSON companion](2026-10-15-compatibility-round.json)
records the binaries, commands, retail identities and outcomes. No
compatibility gate closes.

## Jedi Academy sets heard around a point

The [ambient sets record](2026-10-06-jka-ambient-sets.md) left client-game
import 51 open, and fnTech3 refused it by name. The retail client game calls
it for every entity in its frame that has a sound set and is not a mover
(`cgamex86.dll` 1.01 from `assets3.pk3`, `0x300354f4..0x30035530`: the set
index at `+0xd4`, the type at `+4` compared with 6). It names the set by
its configstring, hands over the view origin, the entity's position, its
number and `cg.time`, and drops the answer. Effect runners and speakers
carry sets on shipped maps, among them `mp/ffa2`, `mp/ctf3`, `mp/duel6`,
`mp/duel7`, `mp/siege_hoth` and `mp/siege_korriban`. Before this round,
`mp/ffa2` and `mp/ctf3` both dropped at the client's first frame with
`Unsupported jka-mp cgame import 51 (CG_S_ADDLOCALSET)`.

**Observed, retail `jamp.exe` 1.01.** The import (`0x412aae`) calls
`AS_AddLocalSet` (`0x4684f0`):

- A name the set table does not hold (`0x467360`) answers the client's real
  time (`cls.realtime`, `0x8af224`) and reads nothing else. Otherwise
  `AS_PlayLocalSet` (`0x4680d0`) plays the set, and the import answers the
  time it leaves.
- The distance is `VectorLength` (`0x455e70`) of the differences stored as
  floats, summed at the double precision retail's runtime sets.
- The loop's volume is the set's own volume within half its radius (the
  0.5 at `0x565f90`). Beyond that, it falls in a straight line to nothing
  at the radius, each step stored as a float, and the product is truncated
  to its low byte. A set with a loop hands it to `S_AddAmbientLoopingSound`
  (`0x46b270`, sound in `eax`, place in `edi`, volume in `bl`), even at
  volume 0; only retail's OpenAL path drops a silent one.
- Every call draws a whole number of seconds from the set's bounds, from
  the generator the general sets draw from (`0x56e760`). When the real time
  less the handed time reaches it, the time becomes the real time. A volume
  is then drawn from the set's range scaled by the loop's volume over 255,
  never clamped (`0x4681eb`). If the set has subwaves, one is chosen and
  started by `S_StartAmbientSound` (`0x46aaf0`) at the point, on the entity.
- The set's volume is the field of its record that the general sets' fades
  write (`0x467f5b`, `0x468098`, `0x4680ae`): 255 as parsed (`0x4672c6`). A
  set that is also a level's general set is heard at its faded level.
- Handle 0 is the silent `sound/null.wav` registration starts with
  (`0x469ed0`); a mover set's `null` subwaves are handed over as 0.
- The legacy mixer plays all loops of one sound as one channel. Each loop
  is placed from its own origin, and the sides are summed and held at 255
  (`S_AddLoopSounds` `0x46b350`, `S_SpatializeOrigin` `0x46a940`).

The client game adds permanent entities in a loop of their own after the
snapshot's (`0x30035907..0x3003593c`). A permanent speaker that is also in
the snapshot is therefore named twice a frame, and its two loops at one
place sum.

**Retail run in-process.** Retail's own `AS_AddLocalSet` ran on 118
synthetic cases (`tests/jka_local_set_cases.h`). The method is the previous
round's: a fixed-base stub holds `jamp.exe`'s sections, the set lookup is
handed each case's set in retail's 0x8c-byte layout (`AddSet` `0x467270`),
the loop and subwave calls are recorded from their registers, and `sqrt` is
redirected. The x87 control word was `0x027f`. The cases cover:

- distances one float step either side of half the radius and of the
  radius, radius 0 and 1, and set volumes from 0 to 255;
- clocks before, at and past every interval, and across the 32-bit wrap;
- swapped bounds, a volume range past a byte, null subwaves and several
  generator states;
- four places where summing the distance in floats, rather than doubles,
  would move the volume by one.

Retail handed over 98 loops and 52 subwaves.
`tests/fixtures/jka_local_set_retail.json` holds them, generated from
synthetic input only.

**Change.** `code/compat/jka_ambient_player.hpp` adds `LocalDistance`,
`LocalVolume` and `PlayLocal`. `Player::MasterVolume` reports the level each
set's record holds, including sets the fades have let go. The host
(`code/client/client_jka_cgame_ambient.hpp`) serves import 51:

- A name it does not hold answers the real time before anything is read.
- Otherwise it reads the view origin and the entity's place, refusing a
  null or non-finite one and an entity outside 0 to 1023.
- It plays the set at its record's level, drawing from the general sets'
  generator.

`shared/JKAAmbient.h` now merges two loops of one sound only when they are
at one place. The general sets' loops, all at the listener, are merged as
before. The legacy mixer sums each sound's places side by side and holds
them at full level, as retail's does. The OpenAL backend gives each place a
voice of its own, and a place keeps the voice nearest it from frame to
frame.

**Fn presentation (invariant 11).** The set's loop is a loop of Fn's at the
entity's place, at the volume the law gives. Fn's distance model,
occlusion, room reverb and tone class apply to it as to any loop. Its
subwaves are Fn one-shots on Academy's ambient channel, from the entity.
Null subwaves are silence and are not played.

Retail compares its real time with the time the client game hands over to
decide when a subwave plays. Single player hands back each answer, but the
multiplayer client game hands over its own clock and drops the answer, so
in retail multiplayer a set's subwaves come either never or on almost every
frame, depending on how far apart the two clocks happen to be. The engine
instead keeps each entity's last subwave time, as retail keeps the general
sets' (`0x12a4d0c`), so subwaves come at the intervals `sound/sound.txt`
gives. The draws are retail's, and import 51's answer is retail's. No
retail-exact option is added for the subwave timing.

**Unit (Source).**

- `tests/jka_local_set_tests.py` plays all 118 cases through the production
  `PlayLocal` and matches every loop, subwave, answer and generator state
  retail left. It also checks the law at its edges, the record volumes
  through the fades, and the shared generator. It shows the engine's
  pacing beside retail's reckoning: with the game clock behind, retail's
  reckoning plays on every call and the engine's every ten seconds; with it
  ahead, retail's never plays. A last check runs the legacy mixer's
  production loop code on two places of one sound.
- `tests/jka_ambient_playback_tests.py` checks that loops of one sound
  merge only at one place, and that the OpenAL world keeps a voice for each
  place, whichever is added first.
- `tests/jka_cgame_ambient_tests.py` runs the production host in ordinary
  and fast-math builds. It checks that missing names answer the real time,
  the law's volumes at 0, 125, 187.5, 250 and 400 units, subwaves on the
  entity at the set's interval, the answer, the faded record level, the
  refusals, and that entity times are cleared with the sound system.

Twenty deliberate faults were laid into this code one at a time, and the
tests caught each one. Two had at first survived: summing the distance in
floats, and clamping a subwave to the loop. The four precision places and
the volume range past a byte were added for them.

**Live (subset).** The runtimes were built from an export of HEAD
`27273055`, before and after this slice; only the two executables differ.
Clients ran windowed at 800 by 600 on a private desktop, with an isolated
home, no network sockets and no input, as spectators of a `devmap` game.

- Before, `mp/ffa2` and `mp/ctf3` dropped at the first frame on import 51.
- After, `mp/ctf3` ran every probe to its end and exited normally. With
  sound on, the OpenAL dumps show a speaker's `yavin_exterior` loop as a 3D
  voice at the speaker. It is at full level from 14.6 to 157 units and has
  no voice at 250 units and beyond.
- A diagnostic build of the same code, with one log line added, shows why:
  the speaker is named twice a frame. Entities 196, 198 and 199 come once
  from the snapshot and once from the permanent loop. At 147.1 units each
  call carries the law's volume of 209, and the two sum to full level, as
  in retail's mixer.
- The set's birds started on the speaker's entity about 20 seconds apart:
  `bird5` at gain 0.96, then `bird2` at 0.97. The set's range is 240 to
  250, and its interval 20 to 50 seconds.
- After, `mp/ffa2` passed import 51 and stopped at its torches' fire
  effect, `effects/env/small_fire.efx`: a particle that names several
  shaders. The [particle-dynamics slice](2026-10-15-jka-particle-dynamics.md),
  committed meanwhile, serves that effect. Its `mp/ffa2` runs, which play to
  their scripted end, carried this round's five import-51 files as a
  labelled overlay, byte for byte the ones committed here.

## Enemy Territory area portals

**Observed.** Both of Enemy Territory 2.60b's executables skip the close of
an area portal whose count is zero, and flood the areas as after any
close. They read the count from the second area to the first and jump past
both decrements at zero (`etded.exe` `0x40c44c..0x40c45d`, `et.exe`
`0x426bdc..0x426bed`); the negative-count drop comes only after a
decrement. This is RTCW's rule ([RTCW record](2026-10-08-rtcw-area-portals.md)).
fnTech3 had kept Quake III's drop for this profile.

**Change.** `CM_AdjustAreaPortalState` (`code/qcommon/cm_test.c`) applies the
rule to Enemy Territory as well. `tests/area_portal_count_tests.py` runs
every catalog profile through the engine's own area-portal code. Enemy
Territory now ignores the close, keeps the count at zero and floods the
areas (Source, Unit). No Enemy Territory level was run to such a close: the
RTCW record's synthetic battery would need Enemy Territory's own module
records.

## Ledger corrections

- Academy's client row said voice volume (import 35) with sound on, the
  first client-game sound (import 37) and local sets (import 51) were open.
  The [October 7 audit](2026-10-07-jedi-compatibility-audit.md) serves 35
  and 37, and this round serves 51.
- Academy's module row counted 98 served game imports; the
  [bone animation record](2026-10-07-jka-server-bone-anim.md)'s count, kept
  by `tests/jka_module_refusal_tests.py`, is 157.
- Academy's introduction said retail peer admission was under
  investigation. The [legacy-server record](2026-10-05-legacy-server-hosting.md)
  shows retail `jamp.exe`'s server admitting fnTech3's client, pure.
- Outcast's identity row still said fnTech3's client stops at UI import
  200. Its [second round](2026-10-07-jedi-compatibility-round2.md) draws the
  menus and reaches client-game import 300 on a local map.
- Enemy Territory's client row still called release builds blocked, which
  the [availability policy](2026-10-11-profile-availability.md) ended.
- Alice's savegame row said fnTech3 reloads the game module on every spawn.
  The [Alice round](2026-10-08-alice-compatibility-round.md#a-load-or-a-restart-of-the-running-level)
  keeps it for a load or restart of the running level, as the same row says.
- Two older paragraphs, Medal of Honor's fades and Outcast's MP3 sounds,
  spoke of their gaps as open; they now point to the records that close
  them.

## Validation

An export of HEAD `599b09af` plus this round's files, configured as the
Windows x86 developer build with strict warnings, built all 1,541 targets at
four jobs with no warning. The native Meson suite passed 264 tests with two
skips: OpenAL loopback, which this machine's library lacks, and the optional
Quake Live factory fixture. The complete Python suite passed 2,717 tests and
66,824 subtests, with 131 skips for installations, privileges and tools
this machine does not have, and no failure. On the final documents, the
changelog, document, release, player-manual and this round's tests pass: 125
tests and 1,006 subtests, one skipped for a missing privilege.

HEAD then moved to `652efbf0`, the particle-dynamics slice. It changes
Academy's effects host, the three renderers' shader parsers and its own
tests, and none of this round's files. An export of it plus this round's
files again built all 1,541 targets with no warning. It passed the native
suite (265 tests, the same two skips) and every Academy test file with the
area-portal, document and release tests: 211 tests and 1,218 subtests, one
skipped for a missing privilege.

The live runs above used the earlier export of `27273055`. That export
lacks the play-blockers slice's Academy Ghoul2 and entity code, which a
spectator does not reach. Clients ran windowed with no network sockets, on a
private desktop, with no input injected; the installations were unchanged.

## Remaining and unrelated findings

- A siege game stops on the server at game import 43 (`G_SIEGEPERSGET`). On
  `mp/siege_hoth` in a free-for-all, the client stops at a view flag outside
  the three the Academy adapter serves (`0x1`, `0x4`, `0x10`).
- Academy's `glow` stage keyword, which this round found in 661 of the
  installation's 3,391 shaders (799 stages), is now read by the
  particle-dynamics slice, with Fn's bloom standing in for retail's glow
  buffer. Retail registers `r_DynamicGlow` off (`0x4a20d7`), with passes 5,
  delta 0.8, intensity 1.13 and a 320 by 240 glow buffer.
- Quake Live's `sv_cylinderScale` warning is not a compatibility gap: the
  retail executable registers the same `"1.1f"` string (`0x53fce0`, pushed
  at `0x4c66d2` and `0x4e3e60`). The warning is fnTech3's own range check.
- A census of open gaps found further small ones, listed in the companion:
  Enemy Territory's tactical route queue, Outcast's pure gamestate budget,
  a Q3-only x64 build of the Academy ABI tests, RTCW MP's top-down TGA
  warnings, typed signature tables for F.A.K.K.2, Holomatch's `sv_fps`
  clamp, Q3Test 1.05's info keys and F.A.K.K.2's plain MP3 music sizing.
- Older records still carry sentences that later records supersede (the
  2026-10-14 round's Emitter and JSON-companion notes, the IHV records'
  release-build notes, the 2026-10-06 Academy records' open imports). The
  records stay as written; the ledger is current.
