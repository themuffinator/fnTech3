# Medal of Honor's speech and mouse sensitivity, 2026-10-31

After the [frame-time record](2026-10-25-mohaa-frame-time.md), Medal of
Honor still felt sluggish and unresponsive in the truck ride that opens
`m1l1` and on foot after it, launched from VS Code's debugger, and some
speech went missing or was cut short. Two causes are fixed here. The
mouse turned the view two and a half times more slowly than the game
expects, because fnTech3 gave the `sensitivity` cvar Quake3e's default of
2 where Medal of Honor uses 5. The OpenAL backend also ignored or clamped
each sound alias's distances. It refused to start any sound more than about
1,530 units away, so a line meant to be heard across the whole level went
unheard, and it played nearby speech more quietly than the alias asks. The
[JSON companion](2026-10-31-mohaa-speech-and-sensitivity.json) holds the
identities, commands and counts. This is Source, Unit, Build and Live
evidence. No compatibility gate closes.

## What it was not

The frames were not slow. On the visible desktop at the default cap, with
no input, the truck ride held 125 frames a second in GL and in Vulkan, at a
mean of 2.3 and 2.8 ms of work a frame. Vsync is not forced, and the engine
writes nothing to `OutputDebugString` that a debugger would have to take.
Four uncapped runs that compared a debugger launch with a plain one drove
the player with engine-queued input that this task had no approval for, so
they are left out of this record.

## Mouse sensitivity

`CL_InitInput` registered `sensitivity` at 2 for every profile but Alice's
and Elite Force's. Medal of Honor registers it at 5, archived, with `cl_run`
at 1, archived (OpenMoHAA `cl_main.cpp` CL_Init **[O]**). Every player
config in 1.00's `Pak0.pk3` (`av.cfg`, `decapitator.cfg`, `jason.cfg`,
`nate.cfg`, `powzer.cfg`) saves `sensitivity "5.000000"`. The Controls menu
(`ui/controls.urc`) links a slider from 1 to 20 to the cvar, and its Default
button runs `ui_resetcvars;exec default.cfg`, and `default.cfg` sets no
sensitivity (Observed). With acceleration off, a mouse count turns the view
by the sensitivity times the client game's own scale, `fov_y / 75`
([contracts](COMPATIBILITY.md#medal-of-honor-gate-matrix)), so 2 turned it
at 40% of the game's rate.

The `mohaa` profile now registers both as the game does. A saved value
stays: a home whose config holds `seta sensitivity "2"`, which every home
the old default ever wrote does, keeps turning slowly until the slider or
`sensitivity 5` raises it. In a fresh home, the build with this change saves
`seta sensitivity "5"` and `seta cl_run "1"`, and HEAD's build saves `"2"`
(Live, `tests/input_system_source_tests.py`).

## Speech

### What the backend did

Every sound a Medal of Honor alias plays carries the alias's min and max
distance (soundparms' fifth and sixth numbers) to the OpenAL world. Before
this change:

- **Starts.** A one-shot farther from the listener than 1.15 times the
  engine's audible horizon, 1,330 units scaled by the sample's sound-shader
  range, was never started. Medal of Honor ships no sound shaders, so that
  was 1,530 units for every sound, whatever its alias said. Loops past
  1,330 units kept no source.
- **Volume.** The alias's distances scaled the engine's own 80-unit
  reference and 1,330-unit range, clamped to at most twice and three times
  them: a reference of at most 160 units and a range of at most 3,990. A
  max no farther than the min fell back to 1,330 units.
- **Eviction.** When every voice was busy, each was judged against the
  80-unit reference, so a line inside a 300-unit full-volume radius looked
  quieter than it was.

So a 10,000/10,000 line was refused beyond 1,530 units and otherwise fell
off from 160. In the default inverse model, a 300/1,000 line 500 units away
played at about a quarter of its volume, where it plays at 60% now (71% on
the game's straight line).

### What the aliases ask

1.00's 6,443 aliases all give a min above zero. 6,290 give a max beyond the
min, and 153 give 10,000/10,000, lines meant to be heard across the whole
level. None gives a negative distance or a max nearer than the min
(Observed). `m1l1`'s aliases are mostly dialogue: 1,528 at 160/1,200, 80 at
300/1,000, 14 at 10,000/10,000 and 6 at 100/500, all of them streamed
(Observed).

OpenMoHAA's engine **[O]** sends a streamed alias's sound (all dialogue), an
MP3, and the local and menu channels down its 2D path. That path always
starts the sound and sets its volume each frame on a straight line from
full at the min to silent at the max (`S_OPENAL_Start2DSound`,
`S_OPENAL_Respatialize`). Any other sound starts only while the max squared
exceeds the distance squared (`S_OPENAL_ShouldStart`). It then plays on
OpenAL's linear clamped model, from the min out to the max (`set_3d`). Either
way, a new sound on an entity's channel takes that channel over before it
is known to start. 1.00 itself mixes through Miles (`snddrivers/Mss32.dll`
and `Mssfast.m3d` in the installation), whose law is unmeasured **[I]**.

### What it does now

`code/client/audio/shared/MOHAADistance.h` holds the alias's law: its span,
with a distance below zero standing for the game's "none" (200 units, and
64 times that far); where a sound may start; and the straight-line gain. The
OpenAL world applies it to every Medal of Honor voice:

- The client game's `streamed` flag, which the engine had discarded, reaches
  the world. A streamed sound starts wherever the listener is, so a line
  begun out of earshot is heard once the listener comes within range. Any
  other sound is refused only beyond its max, and the listener's own sounds
  and the local channels never are.
- The device's distance model takes the alias's span unclamped: full volume
  inside the min, the model's rolloff out to the max, faded out toward the
  max as Return to Castle Wolfenstein's and Elite Force's spans are, and
  nothing traced beyond it. With `s_alDistanceModel linear_clamped` the
  fade is the game's straight line. A device with no distance model plays
  that line itself.
- A loop is silent, keeping no source, from its max. Eviction judges a
  voice against its own min.
- A span whose max is 0 is silent everywhere, so it neither starts nor
  loops. The channel takeover is unchanged and still happens for a sound
  that then does not start.

This follows invariant 11. The voice keeps fnTech3's spatial pipeline,
placed in 3D with occlusion, EFX rooms, HRTF and the tone classes, and the
aliases' distances are adapted into it. OpenMoHAA's unpanned 2D presentation
of speech is not copied. The classic mixer (`s_backend legacy`) still
ignores the distances, pitch and streaming. A voice without a Medal of Honor
span takes the same paths as before, as `tests/wolfenstein_openal_voice_tests.py`
and the Elite Force, RTCW and Jedi audio tests run them
(Source, Unit: `tests/mohaa_sound_distance_tests.py`).

### On the truck ride

The client ran `m1l1` on a private desktop with no input for about 80
seconds from the truck ride's start, under `cdb`. Breakpoints logged each
dialogue-channel request from the client game, on 2015's channels 4, 7 and
8, and each voice the world gave the engine's voice channel. HEAD's build,
from a `git archive` of `a5b9ba53`, gave 53 of 85 requests a voice and none
to the other 32. The changed build gave all 57 of its requests a voice, and
no voice was evicted in either run (Live). All 32 refused requests were
soldiers' shouts:

- 21 name files whose aliases are all 160/1,200. They now start, and they
  are heard if the speaker comes within 1,200 units.
- 8 name files that an alias also carries 3,000 to 10,000 units. Wherever
  that alias was the one playing, the line was lost.
- 3 are at 300/1,000.

The two battles played out at different paces, so the runs' requests are
not the same lines. Requests to the world entity through `script *1022
playsound` raised none in either run, so they are not exercised.

### Lines cut short

A line ends early when the same character starts another sound on its
channel, as the game's channel rule asks. On the changed build's ride that
happened to 2 of the 57 lines, each a shout replaced by a death cry 0.45
and 0.85 seconds in. On HEAD's build it happened to 30 of 85, 25 of them
within 40 ms of starting: bursts of shouts sent to one soldier at once, most
of them already refused for distance. An earlier no-input trace of the
same ride, with the build of 2026-10-29, found 16 of 77, mostly a pain or
death cry replaced by the next. The rule is kept. Whether 1.00 cuts the
same lines is unmeasured **[I]**.

## Open

Nobody has listened to the result. 1.00's own distance law, and its
treatment of a takeover, remain inferred from OpenMoHAA **[I]**. A player's
saved `sensitivity` is never rewritten.
