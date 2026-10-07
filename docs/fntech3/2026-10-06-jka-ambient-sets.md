# Jedi Academy ambient sets, 2026-10-06

The [entity tint record](2026-10-06-jka-entity-tint.md) left fnTech3's
`jka-mp` client stopped in its first frame on `mp/ffa3`, at the map's
present ambient set: `Jedi Academy cgame active ambient playback is not
served (name=city_outdoor)`, client-game import 48 (`CG_S_UPDATEAMBIENTSET`).
Earlier slices parse the sets (imports 49 and 50) and answer an absent name
([boundary record](2026-10-02-jka-compatibility.md#ambient-definitions-and-mpeg-audio)).
This slice measures what retail `jamp.exe` 1.01 does with a present set each
frame, plays it through an engine-owned player written from those readings,
and hands its loops and waves to fnTech3's own audio. Live against retail
`jamp.exe`, the client now passes `mp/ffa3` and completes every step of the
standard session on both maps. A diagnostic build, with sound on, shows the
set's loop reaching OpenAL and fading in. The
[JSON companion](2026-10-06-jka-ambient-sets.json) holds the identities,
addresses, runs and results. No gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jamp.exe` 1.01 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `fntech3.exe` (runtime `jka-live-join`, rebuilt in place) | `314bed8f23eb2a1a7111285c8e8af7b1291bfb176e4aa7a27ffb78a040fbc242` |
| `fntech3.exe` (diagnostic, runtime `jka-ambient-diag`) | `6884b277bf4c9777394bfdf2cc0c9174373ccb87ac63b7e3cb9bbd64155f2c84` |

The runtime is HEAD `23ebc087` plus this slice, built from a clean export
(MSVC 2022 x86, Meson `debugoptimized`, strict warnings, every development
gate on; all 1,499 targets, no warnings). The binaries it replaced matched
the entity tint record's hashes. The diagnostic executable is the same
export with one change, described under the live sessions, and is not
committed. Nothing was written to the installation, and its inventory was
unchanged after every run.

## Observed in retail

Read with Capstone; no retail code is incorporated.

- **The frame.** Import 48's handler (0x412a67) calls
  `S_UpdateAmbientSet` (0x468420) with the set's name and the listener's
  view origin. It looks the name up (0x467360) and returns at once for a name
  it does not hold. Otherwise it makes that set current (0x468020), updates
  both slots' volumes (0x467ec0), then plays the current slot and after it
  the old one (0x4682c0), each with its own time of last wave (0x12a4d0c
  and 0x12a4d10).
- **A new set.** When the named set is not the current one, the current one
  becomes old (0x56d534) and the named one current (0x56d530). The old set's
  volume (set record +0x80) becomes 255 and the new set's 0, and both take
  the client's real time (`cls.realtime`, 0x8af224) as their fade time
  (+0x88) (0x468094..0x4680be).
- **Fades.** While the current set's volume is below 255 it becomes
  `ftol(float(float(elapsed) / float(1000)) * 255)`, clamped at 255, with
  1000 the fade time at 0x56d538; while the old set's volume is above 0 it
  becomes 255 less the same, and at 0 or below the set is 0 and the old slot
  is let go (0x467ec0). The runtime keeps the x87 at double precision: each
  whole number is stored as a float, as is the quotient, and `_ftol`
  (0x539584) truncates.
- **Playing a set** (0x4682c0). A set with a looped wave (+0x78) adds it as
  an ambient loop at its own volume, a byte (0x46b270). Then the time
  between waves is drawn, a whole number of seconds from the set's bounds
  (+0x44, +0x48), afresh every frame; if less real time than that has passed
  since the slot's last wave, the frame ends. Otherwise the slot's time
  becomes now and a volume is drawn between the set's bounds (+0x4c, +0x50),
  each first scaled by the set's volume over 255 and truncated, taken as a
  byte and held at or below the set's volume. A set with subwaves (count a
  byte at +0x54, handles from +0x58) then draws one and starts it with that
  volume at the listener's origin (0x468404).
- **Draws** come from the engine's one generator (0x56e760, starting at
  0x89abcdef): `x = x * 0x343fd + 0x269ec3`, a draw being `x >> 17`, and a
  number from lo to hi `lo + (((hi - lo + 1) * draw) >> 15)` in 32-bit
  arithmetic. A subwave's index is `(count * draw) >> 15`.
- **Loops.** The ambient loop call (0x46b270) keeps at most 32 loops a frame
  (count at 0xc717b0), with the volume it is given; the ordinary loop call
  (0x46b250) adds every loop at 255. The mixer (0x46b350) plays the loops of
  one sound as one channel whose volume is their sum, clamped at 255.
- **Subwaves.** `S_StartAmbientSound` (0x46aaf0) starts the wave on entity
  0, channel 7 (`CHAN_AMBIENT`), at a fixed origin, at the volume it is
  given. With a sound-system flag at 0x12a4c78 set it starts nothing for a
  volume of 0; what the flag means is not established here.
- **Lifetimes.** A parse (import 49, 0x412a8a to 0x467db0) adds its sets to
  the engine's under new numbers, and a name finds the newest set holding it
  (0x467c50). Nothing resets the current and old sets but the sound system
  shutting down (0x469c33..0x469c52 in 0x4699f0), so the set playing when a
  map ends fades out when the next map's first set fades in.

`sound/sound.txt` in `assets0.pk3` declares `city_outdoor`, `mp/ffa3`'s
worldspawn `soundSet`, as a loop of `ambience/outdoor/city_lp` with volumes
from 250 to 255, no time between waves and no subwaves. `mp/ffa1`'s
worldspawn names no set, so its client game asks for `default`, which the
file does not declare.

Import 51 (`S_AddLocalSet`, 0x412aae) measures the time between its waves
against the client's real time while its caller passes the client game's
time; it stays unserved, refused by name.

## What this slice serves

- **The player.** `code/compat/jka_ambient_player.hpp` holds the current and
  old sets, their volumes, fade times and the two slots' wave times, and the
  engine's generator, and plays a frame as retail does: the set changes, the
  fades with retail's arithmetic, the draws in retail's order, and each
  slot's loop and then its wave, the current slot before the old. It owns no
  sound; it calls a sink for each loop and wave.
- **The host.** Import 48 still returns before reading the origin for an
  absent set. For a present one it reads and checks the listener's origin
  and plays the set with `cls.realtime`. Each parse numbers its sets after
  those of every parse before it. The player and the numbering are the
  engine's, kept across client games, map changes and reconnects, and start
  over in `S_Init`, whose sound handles the player holds, as retail's do at
  its sound shutdown.
- **Loops.** A new sound-interface entry, `S_AddJKAAmbientLoopingSound`,
  takes an entity-less loop with its volume. Both backends keep the frame's
  loops in one list (`shared/JKAAmbient.h`): 32 a frame, those of one sound
  merged into one at their summed volume, clamped at 255, and silent ones
  dropped. OpenAL renders each as one of the engine's loop voices, after the
  entities' voices, at volume/255; a sound keeps the voice it has, so a loop
  passing from one set to the next plays on. The legacy mixer spatializes
  each as its own loops are, at volume/255 of their level.
- **Subwaves.** Each audible wave starts through `S_StartSoundVolume` on
  Academy's ambient channel from the world entity, at volume/255.

**Not changed, for other profiles.** The new entry and lists are Academy's:
the dispatcher refuses any other profile, and other profiles' loops, Elite
Force's ambient loops included, are rendered as before.

## Presentation (invariant 11)

Which sets play, when, what plays and how loud are data the client game and
`sound.txt` hand the engine, and follow retail. How it sounds is fnTech3's.
The loops are ordinary Fn loop voices: in OpenAL a 3D source at the
listener's view origin, so distance attenuation and occlusion have nothing
to act on, with the engine's room reverb (audio zones, or the traced room),
its tone classes, its limiter and output, and HRTF where OpenAL Soft
provides it, and no Doppler. The legacy mixer plays them as its own
loops. Subwaves are Fn one-shots on Academy's ambient
channel, so they take the same features. Two choices depart from retail:
waves start from the world entity rather than entity 0, which in
multiplayer is a player's slot, and a wave of volume 0 starts no voice
where retail, with the flag above clear, starts a silent channel. Retail's
own mixing, including the OpenAL and EAX libraries installed beside
`jamp.exe`, is presentation and is not reproduced; no retail-exact option
is added.

## Live sessions

`scripts/probe_legacy_sessions.py --profiles jka-mp --directions join
--runtime .tmp/runtime/jka-live-join --sessions main kick`. The fnTech3
client runs against retail `jamp.exe` on loopback, windowed on a private
desktop, with pure admission and no sound or input.

| Run | Maps | Steps |
| --- | --- | --- |
| `sess-1` | `mp/ffa1`, `mp/ffa3` | join, client chat, server chat, map change, **`map_restart`, reconnect, server quit**, and the kick session's join and kick all pass |

The first frame on `mp/ffa3` no longer stops the client. On `mp/ffa3` the
client game takes the `map_restart` without reloading, the reconnect loads
the client game and parses its sets again, and on the server's quit the
client prints `Server quit`, unloads the client game and stays up at its
menu. As in the entity tint record's runs, it has not exited 15 seconds after
the probe writes `quit` to its command file, and the harness stops it; in the
diagnostic runs below, which end before the server quits, the same `quit`
ends the client with exit code 0.

**With sound on** (diagnostic). A client with sound on stops earlier, at the
first voice-volume query (import 35), which fnTech3 refuses while a backend
is running because it does not yet measure a voice's level. The diagnostic
build answers that query with 0, the value retail's table holds while
nothing speaks, and changes nothing else. `.tmp/jka-ambient/probe_audio.py`
runs the same join with the client's sound on (OpenAL, the system's Generic
Software device, offscreen-muted on its private desktop) and dumps the
OpenAL world with `s_alDebugDump`. The dumps run three frames apart, at 60
frames a second, across the change to `mp/ffa3` and across a reconnect.

- On `mp/ffa1` no ambient voice plays; the loops there are movers'.
- On `mp/ffa3` one loop voice plays `sound/ambience/outdoor/city_lp.wav` on
  entity 1023 (`ENTITYNUM_NONE`): 3D, at distance 0, unoccluded, tone class
  `world`. Its gain rises from 0.04 to 1.00 over 22 dumps, about a second, by
  about 0.05 a dump (`diag-audio-3`; 0.05 to 1.00 over 21 in `diag-audio-2`), and
  stays at 1.00. Its reverb send follows the engine's room, moving from the
  previous map's hall to outdoors.
- Across the reconnect the voice is absent while the client reloads, then
  plays at 1.00 from the first frames of the new client game. That is what
  the player gives when the new parse's `city_outdoor` fades in as the old
  one fades out, the two being one sound: 255 less v plus v. The dumps alone
  cannot tell it from a set that never changed; the unit tests pin the
  renumbering. The sound system was not restarted by the reconnect.

No listening test was made, and the system device has no OpenAL Soft
extensions, so HRTF was not exercised.

## Tests

- `tests/jka_ambient_playback_tests.py`:
  - the player, frame by frame, against an independent model written from
    the readings above, over four generated sessions of 2,500 frames each:
    set changes inside and after fades, parses that renumber sets, long
    pauses, sets without loops or waves, single-value ranges, every loop and
    wave compared exactly;
  - the fade law, the generator and the x87 arithmetic on chosen values,
    including an elapsed time where storing the operand as a float changes
    the result, and the byte and cap on an out-of-catalog volume;
  - the frame loop list both backends share, and the OpenAL world's voice
    choice: a loop keeps its voice across a set change, two loops of one
    sound are one voice, silent loops keep none;
  - where the host, the sound interface and both backends serve it.
- `tests/jka_cgame_ambient_tests.py` plays a parsed set through the real
  host: the loop's volumes over the fade, a wave on the ambient channel from
  the world entity, a non-finite origin refused, the player kept across a
  client game's cleanup, and a new parse's cross-fade.
- `tests/rtcw_loop_audio_tests.py` and `tests/rtcw_music_fixture.py` follow
  the new list in the fixtures they slice from the backends.

All 33 deliberate mutations of the served semantics fail these tests, among
them a shorter fade, no cross-fade, wave times moving with the sets, the
interval drawn once, the generator's constants, the draws' order, the old
slot played first, the merge or its clamp dropped, a stale voice taken
before an idle one, the wave from entity 0 or on another channel, and the
player reset by a client game's cleanup or kept across `S_Init`. Two survived
the first pass, both unreachable from parsed sets, the operand rounding and
the cap; the chosen-value checks above were added for them.

On the export of HEAD `23ebc087` with this slice, Meson's 250 project tests
other than the pytest wrapper give 248 passes and 2 skips for this machine,
and the Python suite passes 2,573 tests with 130 skipped, for installations
and tools this machine does not have.

## What stays open

- Voice volume with sound on (import 35), and the first sound a joined
  player's client game starts (import 37).
- Local sets (import 51), and a listening test.
- Whether retail's sound system restarts on a reconnect, which decides
  whether a reconnect's set fades in from silence there.
