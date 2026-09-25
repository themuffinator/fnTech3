# Alice's sound and music

## Scope

This record covers how American McGee's Alice plays sound and music, as the
retail November 2000 `alice.exe` does it, and what fnTech3 serves of it: the
client game's sound imports (slots 30 to 41), its three music imports (42 to
44), the soundtrack files behind them, and the MP3 track retail plays beside
each cinematic. It builds on the
[client-game record](2026-09-23-alice-cgame.md), which named the slots.

Addresses are into `alice.exe`, image base `0x00400000`, unless they start
`0x300` (`base/cgamex86.dll`) or `0x100` (`base/fgamex86.dll`). Each section
keeps what was read from a binary, at the address it was read at, apart from
what is inferred; inferences are said to be. Nothing from Ritual's F.A.K.K.2
SDK or from any reconstruction is incorporated as text. Retail's audio
backend is Miles Sound System 6.0b; nothing of Miles is used or needed here.

The profile stays behind `-Dalice-development=true`. Until the client game's
slots 30 to 41 are wired to the entry points below, a level still stops at
the first of them, so the sound effects have unit evidence only.

## The installation's audio

- All 1,896 `.wav` files in `pak0` are PCM, 16-bit and mono: 1,883 at
  22,050 Hz and 13 at 44,100 Hz. The formats the engine already reads cover
  them.
- All 29 songs in `Pak2` are MPEG-1 layer III at 44,100 Hz, 27 of them joint
  stereo and two stereo. The MP3 codec that joined for Elite Force now joins
  for Alice too, and for no other title.
- 38 soundtracks, `sound/music/*.mus`, name the songs. The shipped ones use
  only `path`, a `normal` song and, twice, a `suspense` one, with `loop`,
  `fadetime` and `volume`.
- `Pak3` carries `video/ealogo`, `roguelogo`, `opening` and `ending`, each as
  a `.roq` and an `.mp3`. The four RoQ files hold no sound chunks at all: only
  their info, codebook and picture chunks.
- `global/sound0.txt` is the one sound-info file that ships. It sets the
  default min distance and volume and gives one sound a min distance of its
  own.

## Music

### Soundtrack files

A level's soundtrack is loaded by `MUSIC_LoadSoundtrackFile` at
`0x0044eef0`: `%s.mus` when the name has no dot, the name itself otherwise
(on `garden1` the game module names `sound/music/garden1.mus`). A file that
cannot be read changes nothing (`Couldn't load %s`, a developer print). One
that can prints `SOUNDTRACK: Loading %s` (developer), stops every song, frees
the old songs' images (`0x0044ee60`) and parses.

The tokenizer is Ritual's own (`0x0044b640`), not Quake III's
`COM_ParseExt`:

- whitespace is a byte from 0 to 32 read as a signed char, so bytes from
  `0x80` up belong to tokens;
- a line break, with lines not crossed, answers an empty token and is left
  for the next call;
- `//` runs to the line break; a block comment runs to the first `/` whose
  previous byte is `*`, counting the opener's own star, so `/*/` is a whole
  comment; it crosses lines whatever is asked, and **the rest of the line
  after it is skipped too** (`0x0044b74c`);
- a quoted string turns `\"` into `"` and `\n` into a line break, and keeps
  1,024 bytes;
- a word of 1,024 bytes or more comes back empty (`0x0044b888`).

Each line's tokens go into one array of sixteen 64-byte arguments that lives
for the whole file, so an argument a line leaves out is what an earlier line
put there: a bare `path` takes the previous line's second token.

- `path <dir>` sets the directory songs are found in. A slash is appended
  every time: the test on the last character compares it with `/` and with
  `\` joined by the wrong conjunction (`0x0044f13a`).
- `<name> <file>` defines a song; a bare `<file>` is named by the file less
  its last four characters. At most sixteen songs (`Too many songs`). A song's
  name picks its mood from the table of sixteen at `0x00519100`: `none`,
  `normal`, `action`, `suspense`, `mystery`, `success`, `failure`,
  `surprise`, `special`, `aux1` to `aux7`.
- `!<song> volume|fadetime <value>`, `!<song> loop`, `restart` (flag 2) and
  `interrupt` (flags 2 and 4, fade time zero) adjust a song; an unknown song
  or command is a warning and skipped. A song starts at volume 1 with a
  one-second fade. The warnings are plain console prints.

### Moods

`MUSIC_UpdateMood` (`0x0044f860`) takes the current and fallback moods the
client game reads from its player state. It hands them over on its first
snapshot (`0x300275ec`) and afterwards only when one changes (`0x30027a0f`).
The four words the manager keeps start at -1. It prints `MUSIC: changing from
[ %s | %s ] to [ %s | %s ]`, and returning from `action` to exactly the pair
that preceded it skips a song marked `interrupt` in favour of the fallback
(`restoring music from action state`). A freshly loaded soundtrack with no
mood yet starts on `normal`.

`MUSIC_Update` (`0x0044f5b0`), once a frame, first checks the players
(`0x00472400`): a song that does not loop hands back to the fallback mood once
it is within the fallback song's fade time of its end. Only then does it
reload a soundtrack that lost its songs. Then, if the wanted mood is not the
playing song's mood and a song of that name exists, it plays it (`Playing
%s.`, written to the debugger alone by `0x00438a90`); a wanted mood of `none`
stops everything.

### Two players and a crossfade

`MUSIC_PlaySong` (`0x00471fc0`) keeps two players. A new song crossfades with
the playing one over the **new** song's fade time in whole seconds -- the old
player fades out, the other fades in from silence -- unless the new song is
marked `interrupt`, which stops the old one at once. The seconds are the
float fade time through the CRT's `_ftol` (`0x0047205c`, `0x0047219a`), which
sets the rounding to chop, stores a 64-bit integer and keeps its low 32 bits
(`0x004e6d40`): 2.9 is 2, 1e10 is `0x540BE400`, and NaN, an infinity or
anything past 64 bits -- a `fadetime` of `1e39` is an infinity once `atof`'s
result is stored as a float (`0x0044f257`) -- stores the integer indefinite,
whose low word is 0. A song started with no crossfade starts at its own
volume times `s_musicvolume` (`0x004721c7` to `0x004721e9`) and ignores the
master music volume until the volumes are next applied (below). Stopping a
sounding song without `restart` keeps its place (`0x00471f20` saves the
millisecond); stopping a player that has already fallen silent changes
nothing.

Every song starts, and every loop begins again, at byte
`floor(rate × 0.063)` of the compressed file (`0x00472113` to `0x0047217f`):
2,778 for every shipped song. Miles's granularity for an ASI-decoded sample
is 1 (`mss32.dll` `0x211151a0`) and its sample position is a byte position
into the file image (`0x21115200`). The frames that begin before that byte
are passed over: seven for a 128 kbps song, about 183 ms; two for
`final/alice.mp3`, about 52 ms.

A player's volume (`0x00472210`) is the song's volume times the music volume
cvar times the master volume, as Miles's 0 to 127. The fades compare against
the song's volume times the cvar, so with `s_musicvolume` at 0 a fade-out
ends at once and a fade-in waits until the cvar rises. The master volume is
what `MUSIC_UpdateVolume` (`0x0044f980`) fades toward over its fade time in
seconds, from where it is when fading up -- and **from one when fading
down**: the down branch subtracts from the constant 1.0 at `0x004ff408`
rather than from the starting volume. `s_volume`'s value never reaches the
music: no music routine reads it, and `AIL_set_digital_master_volume`, though
resolved, is never called.

The volumes are applied to every player that sounds and is not fading when
the music volume cvar is modified or the flag at `0x006a0064` is up
(`0x0047221f` to `0x0047222a`; the volume itself is `0x0047226a` to
`0x00472289`), which clears both. The master fade raises the flag on each of
its steps (`0x0044f708`), and `S_Update` raises it every frame while a
`fadesound` fade is on (`0x0047112c`) and whenever `s_volume` is modified
(`0x0047113f`), before it runs the music's update (`0x00471255`). So a song
started without a crossfade takes the master volume at the first of these.
The update returns before the volumes while the music is inactive
(`0x0044f5b0`), and the flag waits.

### Songs are read once

`MUSIC_RegisterSongs` (`0x0044fad0`) reads every song's whole file when the
soundtrack loads, through `S_RegisterSong` (`0x0044cc30`), which prints
`Couldn't load song: %s` for a missing one and keeps the song in the table;
the next registration tries it again. Freeing the old songs registers each
again first, so a missing song is reported once more then.

### Who frees the music

`S_StopAllSounds(1)` (`0x0044ce60` to `0x004713f0`) frees the music through
`MUSIC_FreeAllSongs` (`0x0044fa40`): every song stops, the moods go to none,
the soundtrack loses its name and its songs count as unregistered; the song
table stays. The callers that pass 1 are the client flush (`0x0042aa67`,
reached by every map load, disconnect and `killserver`), the client state
clear (`0x0042ab88`), `vid_restart` (`0x0042b592`), the cinematic start
(`0x00427303`) and `S_Init`. So every level restarts its soundtrack from the
top. After `vid_restart`, and after `snd_restart`, outside a level
(`cls.state` not 8) retail names `music/intro` (`0x0042b5d8`, `0x0042b62b`),
a file no pak has; the load fails and the old table stays.

Retail's `snd_restart` (`0x0042b5f0`) frees nothing of the music: it is
`S_Shutdown(0)` and `S_Init(0)` (`0x0042b611` to `0x0042b61a`), and
`S_Init(0)` skips its `S_StopAllSounds(1)` (`0x0044c87f`). Only when
`s_initsound` has changed since the sound system started (`0x0046e850`) does
it restart with 1 and run `vid_restart` too (`0x0042b5f9` to `0x0042b60a`),
which frees it.

The Miles shutdown (`0x0046e260`) forgets only the current song
(`0x0046e2a5`) and never calls `MUSIC_StopSong`; the soundtrack's name,
table and moods survive `S_Shutdown`, and so does every song's saved place.
That place (`+0x70`) is written only by the loader, as 0 (`0x0044f4c5`), and
by `MUSIC_StopSong` (`0x00471f79`, `0x00471f97`), and `MUSIC_PlaySong`
resumes from it when it is not 0 (`0x0047213c` to `0x00472153`). A song that
sounds at the shutdown keeps the place it last saved, if any.

### Commands

`playsong <song alias>` (`0x00471e90`) and `loadsoundtrack <sound track file>`
(`0x00471e60`, which loads and registers without naming a soundtrack or
making music active) come with Miles. There is no `music` or `soundtrack`
command; `S_Shutdown` removes a `music` command nothing adds.

## Sound effects

### The contract

| Contract | Retail `alice.exe` | fnTech3 |
| --- | --- | --- |
| Voices | 32 3-D Miles handles for one-shots and loop groups alike (`0x0046e685`), and 4 2-D dialog handles (`0x00aa8f10`) | 36 voices of the engine's own in `shared/AliceSound.h`, played by either backend |
| Starting a one-shot (30) | A handle out of range is `S_MILES_StartSound: handle %i out of range` (ERR_DROP); 0 plays nothing (`0x0044cdb0`). The sound waits for the next respatialize, which starts it only inside 16 × its min distance and drops it otherwise (`0x00471510`); the menu channel, 9, starts at once | The same |
| A voice for it | A non-zero channel takes over the same entity's channel whatever it holds; channel 0 never does; with nothing free the sound is dropped, nothing stolen (`0x0046f7c0`) | The same |
| How many at once | No more than `maxnumber` (default 5) of one sound, pending ones counted (`0x00471460`) | The same |
| Min distance and volume | A negative argument takes the sound's record; volume is `s_volume` times the fade times the volume, the menu channel `s_volume` alone (`0x0046fc27`) | The same, the volume truncated to Miles's 127 steps |
| Local sounds | The menu channel, a min distance of exactly 30,101, or a record's `localsound` play at the listener | The same |
| Fixed origins | Entity 1023 or -1 is stored as entity 0 (`0x0046fd2b`), so entity 0 on that channel takes it over and `S_StopSound(0, channel)` stops it | The same, as the maintainer chose to keep it |
| Following an entity | The entity's origin once the entity is newer than the sound's own; a looping sound whose entity goes stale stops; an entity marked for the listener plays at the listener; a sound 90 units under the listener from its own entity is the listener (`0x00470a12`) | The same |
| Ending | A one-shot is ended 250 ms after its length (the constant at `0x00500278`) | The same |
| Stopping (32) | The first voice with that entity and channel, playing or not, else a dialog; loops untouched (`0x00471340`) | The same |
| Loops (33, 34) | 64 records submitted each frame, reused by sound, not entity; stationary ones merged by sound into one voice at the leader's volume and distance, summed, capped by `maxfactor`, placed at half the min distance toward the loudest; reaped when not submitted, only if playing (`0x0046fe70`, `0x00470010`, `0x0047125a`) | The same |
| Loop blocks | A record's `loopstart` other than -1 makes a one-shot loop: after the offset goes to 0, `AIL_set_3D_sample_loop_block(loopstart, loopend)` and a loop count of 0, so it plays to the block's end and then repeats the block (`0x0046fd79` to `0x0046fd9d`). The two are byte offsets into the sample's data (`+0x48` and `+0x4c` of the record, stored from `atoi` at `0x0044ddc9` and `0x0044de20`). Loop groups set up their handles with no block (`0x004704e4`, `0x00470619`) | The same, the ends in whole frames; see below |
| The ambient level (41) | Scales the stationary loops; `S_Update` sets it every frame to 0.5 while a key catcher is up and 1 otherwise (`0x00471090`) | The same |
| Entities (39) | 0 to 1024 accepted, else `S_UpdateEntityPosition: bad entitynum %i` (ERR_DROP) | The same; 1024 is kept nowhere |
| Registration (36 to 38) | Handle 0 is `***DEFAULT***`, never loaded (`0x0044c893`); names over 63 characters print `Sound name exceeds MAX_QPATH`; a stereo WAV is refused (`%s is a stereo wav file`) and tried again at the next registration (`0x0044e560`); end of registration frees what the level did not register again | The same, except that the backends keep what they loaded at the end of registration, and fold case in names, where retail's sfx names are case-sensitive |
| Fade | `fadesound <ms>` fades every new sound and the loops to silence over that time and holds it; 0 cancels, which the client flush and state clear do (`0x00472530`, `0x0040eed0`). While it is on, `S_Update` has the music apply its volumes every frame (`0x0047112c`) | `S_AliceFadeSound`, and the same cancels and music request |
| `s_volume` | A change rescales what plays by the master volume against the last one used (`0x00471145`), and has the music apply its volumes (`0x0047113f`); the fade alone rescales nothing | The same |
| Pause | The `paused` cvar stops what plays where it is and resumes it (`0x0046f6c0`, `0x0046f740`) | The same, on `cl_paused` |
| Obstruction | Once a second of server time, a point trace from the sound to the listener against `CONTENTS_SOLID`; blocked and not the listener's own entity sets 0.5 (`0x00470c84`) | The same test on the legacy mixer; the OpenAL backend traces its own occlusion instead; see below |
| Reverb (40) | Kept; applied only with `s_reverb 1`, which ships at 0 (`0x00471000`) | Kept; the OpenAL backend plays its own room, and a room named here replaces it while `s_reverb` is 1, its default; see below |
| Dialog voices | A sound whose name says `.mp3` plays in 2-D: panned by `s_separation` (0.3), scaled by min over distance with no far limit (`0x0046f860`, `0x00470e00`) | The same bookkeeping on the dialog voices; the legacy mixer pans them in 2-D and the OpenAL backend places a line at its speaker; no shipped sound reaches them |
| Defaults | `s_volume 1.0`, `s_musicvolume 0.60` (`0x0044c6f0`) | The same for this profile; the other profiles keep 0.8 and 0.25 |

### Inferences

- Miles attenuated one-shots with the two distances it was handed and no
  rolloff call. Its own curve was not read; the engine's own loop curve --
  full inside the min distance, min over distance, nothing at 64 times it --
  stands in for it, as the maintainer chose.
- `[0x00c301e8]`, which halves the ambient level, is taken to be the key
  catchers: its readers are menu and key code.
- A volume above Miles's 127 is taken to be clamped by Miles.
- The sound-info loader reads its files through Ritual's Script class, which
  was not read; its comment and quoting rules are taken to be the engine
  tokenizer's.

### The trace never finds the listener in the way

Retail's `CM_BoxTrace` (`0x00437130`) clears its work area (`0x0043716b`) and
nothing in it sets the trace's entity number, so the number is 0; the
respatialize then asks that it is not the listener (`0x00470cad`). In single
player the listener is entity 0, so **retail never obstructs a sound**.
fnTech3's engine keeps that test, and its one 0.5 step is what the legacy
mixer plays. The OpenAL backend does not use it: it traces its own
occlusion, as for every profile (below). The maintainer asked for the
obstruction to be reset for each new sound rather than stick to the handle as
retail's does; that is done, and it only matters for a listener that is not
entity 0.

### Reverb

Retail has no reverb unless `s_reverb` is 1, which it ships at 0, and then
only the room its client game names: the EAX 1.0 environment index, 0 to 25,
whose volumes are the 26 floats at `0x0051f944`, sent at the level the client
game names times that environment's volume (`0x00470f50`). The client game
takes the room from its player state; no shipped map raises it, because the
`TriggerReverbs` section of each of the thirty-six `maps/*.snd` files is
empty.

fnTech3 does not copy that. The OpenAL backend plays the room it plays for
every profile -- the map's audio zones, which every official Alice map now
ships ([sidecar record](2026-09-24-alice-map-sidecars.md)), or the traced room
where a map has none -- under `s_alReverb`. A room the client game names, at
a level above 0, is authored, so it replaces that room while it is named, as
an authored zone outranks a generated one; it plays with the nearest of the
backend's presets at retail's software-provider level. `s_reverb` keeps its
name and now chooses only that: 1, the default here, lets a named room in,
and 0 keeps the backend's own. The legacy mixer has no reverb, as retail's
own DMA path had none.

### The engine's own features

Under the project's rule that every profile runs on fnTech3's own features
(invariant 11 of `AGENTS.md`), the engine above keeps retail's bookkeeping --
which voice a sound takes, when it starts, how the loops merge, a sound's
min distance, volume and `maxnumber` from its record -- and the OpenAL
backend renders what it decides as it renders any profile's sounds
(`ApplyAliceVoice`, `code/client/audio/openal/AudioSystemWorld.inl`). Each
voice arrives with its placement (`Placement` in `shared/AliceSound.h`):
where it is heard, where it comes from, its volume with and without its
distance, its min distance and the distance where it falls silent, its
motion, its role and its channel.

| Feature | Alice's voices |
| --- | --- |
| Distance model | OpenAL's, with the sound's min distance as the reference and 64 times it as the far end; the clamped inverse model is retail's own law, full inside the min distance and min over distance beyond, and it is faded out toward the far end as for every profile. A dialog line has no far end, as retail's has none. Where the device has no distance model, the engine's measured gain plays instead |
| Air absorption | `s_alAirAbsorption`, beyond the min distance |
| HRTF and output modes | Every positional voice |
| Occlusion | The backend's traced occlusion and liquid boundaries, on the direct and wet paths, under `s_alOcclusion`. A merged loop group is heard from near the listener, so its occlusion is traced from its loudest member |
| Room reverb | The audio zones or the traced room, with the backend's per-voice sends; a named room replaces it (above) |
| Tone classes | By channel: Ritual's weapon and weapon-idle channels are weapon, its voice and two dialog channels are voice, item and body keep theirs. Ritual's 6 and 7 are dialog where Quake III has its local and announcer channels, so the channel is mapped before it reaches the backend's rules |
| Doppler | A moving loop's motion, under `s_doppler`. No shipped loop moves |
| `s_pvs` | Honoured; off by default for every profile |
| Interface sounds | The menu channel is dry and head-relative, as the backend plays any interface sound |
| Sounds at the listener | A local sound, or one from an entity marked for the listener, takes the room but no occlusion |

Left out: the weapon sound shaders (`sound/fnql-weapon-sounds.sndshd`), which
tune Quake III's and Quake Live's weapon samples by name and have no Alice
rules, so there is nothing for them to apply. Retail's 2-D dialog pan is kept
only where a line cannot be placed, a stereo one, and by the legacy mixer.

### Loop blocks

The shipped `global/sound0.txt` gives no sound a `loopstart`, so only custom
content reaches this. What Miles makes of a block was read in the software
provider the game ships, `SndDrivers/mssfast.m3d` ("Miles Fast 2D Positional
Audio", image base `0x22600000`), which `s_milesdriver software` selects: the
name `miles` (`0x0046f195`), matched against the start of each provider's
name ignoring case (`0x0046f2ac`). The hardware providers were not read.

- `M3D_set_3D_sample_loop_block` (`0x22601fb0`) rounds the start to the
  nearest whole sample -- `(start + size / 2) / size * size`, the size being
  the sample's bytes per sample -- and keeps the end as given.
- Its mixer (`0x22601af9` to `0x22601b35`) plays to the end of the data, or,
  while the loop count is not 1, to the block's end when that is neither -1
  nor at or past the data's end (an unsigned compare, so any negative end is
  the data's end); there it goes back to the block's start and counts the
  loop down, 0 meaning for ever.
- `M3D_set_3D_sample_data` (`0x22602aa0`), which `AIL_set_3D_sample_file`
  calls with the parsed file (`mss32.dll` `0x2112d5ef`), takes linear mono
  PCM only and resets the loop count to 1 and the block to the whole sample
  (`0x22602b28` to `0x22602b2e`); the loop groups' handles therefore loop
  whole samples.

fnTech3 converts the block in `shared/AliceSound.h` (`ResolveLoopBlock`): each
end goes to the nearest whole frame of the file, then to the frames the
backend plays, which the legacy mixer has resampled. What retail would read
outside the data is bounded: a negative start is the sample's start, and a
block that is empty or inverted once rounded, or starts at or past the end --
which the provider would repeat at no length for ever, or read past the data
for -- loops the whole sample. The engine hands the block to the voice after
it prepares it and before it starts it, as retail does.

- The legacy mixer paints such a voice as a loop channel that runs from its
  start to the block's end and then round the block, counted from where the
  voice started, so a pause keeps its place in the block.
- The OpenAL backend gives the voice a second buffer of the sample, made on
  the block's first use by reading the file again, whose `AL_SOFT_loop_points`
  are the block: a buffer's loop points cannot change while any source holds
  it, and the sample's own buffer stays for loop groups and one-shots, which
  loop whole or not at all. A runtime without the extension (OpenAL Soft has
  it) plays the voice looping the whole sample, with a developer warning.

## The cinematic track

Retail plays an MP3 beside each RoQ cinematic through one extra Miles handle
(`0x00aa8ecc`):

- `CL_PlayCinematic` (`0x004272e0`) frees the music, opens the movie, and
  reads `video/<name less its extension>.mp3` (`0x0042745d` to `0x0046eef0`),
  printing `ERROR: Could not preload mp3 file %s.` if it cannot. Once the
  first frame is decoded it plays the track once (`0x00427569` to
  `0x0046ef90`, `Playing mp3`), printing `ERROR: Could not play preloaded
  MP3.` if it cannot.
- The cinematic's shutdown frees the track (`0x004257f4` to `0x004725b0`).
- No volume is ever set on the handle, so the track plays at Miles's full
  scale, whatever `s_volume` and `s_musicvolume` say.
- `playmp3 <mp3 file>` (`0x0046f030`) plays a file the same way, looping for
  ever (`Playing mp3 - %s`, or `Could not start mp3 - %s`); `stopmp3`
  (`0x0046f080`) ends it and keeps its image.
- The RoQ decoder decodes a movie's own sound chunks and discards them: the
  two cases (`0x00425aaa`, `0x00425ace`) call only the decoders, which make no
  calls. The shipped movies have no sound chunks.

fnTech3 does the same: the track is read once, mixed into the music stream at
full scale, and a RoQ's own sound is not played for this profile.

## In fnTech3

`code/client/audio/shared/AliceMusic.h` is the soundtrack manager and
`code/client/audio/shared/AliceSound.h` the sound-effect engine: both are the
retail arithmetic and bookkeeping, with no file, device or cvar access.
`code/client/audio/snd_alice.cpp` joins them to the engine:

- **Music.** Each song's file is read once per soundtrack, as retail reads
  it, and decoded in place by an MP3 decoder that can start at a byte offset
  (`S_MP3_OpenMemory`, `S_MP3_SeekFileOffset`); a song starts and loops at
  its 2,778th byte with no file read. A stopped song that keeps its place
  keeps its decoder. The two players and the cinematic track are mixed at
  44.1 kHz into the one music stream the running backend plays, at the
  volumes Miles would have been handed -- `s_musicVolume` included, `s_volume`
  not -- with each volume change spread across the frames of the mix call
  rather than applied as a step. The backends play that stream as it is, and
  go on reading it with the music volume at 0, so songs run out and hand over
  as retail's do. `S_AliceSoundUpdate`, which `S_Update` runs before the
  backend mixes, asks the manager to apply its volumes again while the fade
  is on and when `s_volume` changes, and `S_AliceMusicUpdate`, which runs
  after it in the same frame, serves the request once the music is active,
  as retail's update does.
- **Sound effects.** The entry points below keep the engine's state and hand
  each voice to the running backend's `AliceDevice`. The legacy mixer plays
  a voice on the channel of its own number, a looping one as a loop channel
  counted from where it started, and spreads each between the two channels
  evenly, as it spreads any sound; a voice heard at the listener's own
  position is played on both at full. The OpenAL backend renders each voice
  with its own distance model, occlusion, room and tone, described above.
  Loop blocks are described above. Every other start is turned into
  an Alice one under this profile, so no other code takes an Alice voice.
- **Freeing.** `S_AliceMusicFreeAll` is called where retail passes 1 to
  `S_StopAllSounds`: `CL_ShutdownAll`, `CL_Disconnect`, `CL_ClearState`,
  `CL_Vid_Restart` and the cinematic start. The first three also cancel the
  fade. `S_AliceMusicSoundtrackAfterRestart` ends `vid_restart` and
  `snd_restart`. `S_Shutdown` lets the backend and the two live players go
  and keeps the soundtrack -- its name, moods and table, and the waiting
  decoder of every song stopped without `restart`, which is that song's
  place and uses nothing of the backend -- as retail's does, so the next
  start picks up where the manager is. `S_AliceMusicFreeAll` does nothing
  while no backend runs.
- **Commands.** `play` (a name without a dot gains `.wav`, then plays as
  import 31 plays it), `sounddump`, `playsong`, `loadsoundtrack`, `playmp3`
  and `stopmp3`; no `music` or `stopmusic`. `s_reverb` (1; see Reverb) and
  `s_show_sounds` are registered, and `s_separation` at 0.3.

### Entry points

With the semantics of the client game's imports; each raises ERR_DROP
outside the profile and does nothing while no backend runs:

| Slot | Entry point |
| --- | --- |
| 30 | `void S_AliceStartSound( const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx, float volume, float minDist )` |
| 31 | `void S_AliceStartLocalSound( const char *name )` |
| 32 | `void S_AliceStopSound( int entnum, int channel )` |
| 33 | `void S_AliceClearLoopingSounds( void )` |
| 34 | `void S_AliceAddLoopingSound( const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx, float volume, float minDist )` |
| 35 | `void S_AliceRespatialize( int entnum, const vec3_t origin, vec3_t axis[3] )` |
| 36 | `void S_AliceBeginRegistration( void )` |
| 37 | `sfxHandle_t S_AliceRegisterSound( const char *name )` |
| 38 | `void S_AliceEndRegistration( void )` |
| 39 | `void S_AliceUpdateEntity( int entnum, const vec3_t origin, const vec3_t velocity, qboolean useListener )` |
| 40 | `void S_AliceSetReverb( int type, float level )` |
| 41 | `void S_AliceSetGlobalAmbientVolumeLevel( float volume )` |
| 42 to 44 | `S_AliceMusicNewSoundtrack`, `S_AliceMusicUpdateMood`, `S_AliceMusicUpdateVolume` |
| `fadesound` | `void S_AliceFadeSound( float ms )` |

### Where fnTech3 differs, and why

- **The presentation is fnTech3's, not Miles's.** On the OpenAL backend every
  voice gets the engine's distance model, air absorption, traced occlusion,
  room reverb from the audio zones or the traced room, tone classes and
  HRTF, with Alice's own min distances, volumes and channels; see The
  engine's own features. Reverb is on by default, where retail's is off.
  The legacy mixer keeps retail's measured gain, obstruction step and pan.
- The obstruction is reset for each new sound (the maintainer's choice); in
  retail it sticks to the handle. For Alice's listener neither ever applies.
- A menu sound is placed at the listener as it starts; retail starts it
  wherever the handle last was until the next respatialize, which the menus
  never reach.
- Retail outputs 8-bit 22 kHz by default (`s_loadas8bit 1`, `s_khz 22`); that
  is not emulated.
- A loop's phase is counted in the sample's own frames, where retail counts
  in the output's. Retail writes a loop's time on every submission
  (`0x0046ff7a`), so a group whose leader was submitted that frame starts from
  the beginning either way.
- There is no sfx cap; retail's is 700. The end of registration frees
  nothing, and names are matched ignoring case.
- A dialog voice's 2-D pan is taken as a balance by the legacy mixer, full on
  both channels at the centre; the OpenAL backend keeps it positional at the
  speaker. Miles's own pan law was not read.
- Retail skips the respatialize while a loading flag (`[0x00c2394c]`) is set;
  fnTech3 has no such flag.
- A song's end is measured where the decoder is, which is ahead of what is
  heard by what the backend has queued (a fraction of a second).
- `Could not setup mp3 file!` gains a line break.
- `snd_restart` in fnTech3 is `S_Shutdown` followed by a whole video
  restart -- renderer, client game and interface -- where retail's is
  `S_Shutdown(0)` and `S_Init(0)` alone before it names `music/intro`. The
  music comes through it as retail's does: the video restart's
  `S_AliceMusicFreeAll` finds no backend after `S_Shutdown` and does nothing,
  so the soundtrack's name, moods and table survive, as `S_Init(0)` keeps
  them. Retail frees the music when `s_initsound` has changed since the sound
  system started (`0x0046e850`); fnTech3 does not.
- The soundtrack parser bounds what retail overruns: a seventeenth token, a
  name over 31 bytes, a path over 63, a bare file under four bytes.
- A song that ends naturally, or that sounds when the sound system shuts
  down, starts from its offset the next time it plays; retail resumes it
  from the last position it saved, if any, since neither case saves one.
  Songs stopped before the shutdown keep their places in both.
- A loop block's ends both go to the nearest whole frame; retail's provider
  rounds only the start. A block that is empty or inverted once rounded, or
  starts at or past the end, loops the whole sample, and a negative start is
  the sample's start, where retail's provider would repeat a block of no
  length for ever or read outside the data.
- The OpenAL backend repeats a loop block only where the runtime has
  `AL_SOFT_loop_points`; elsewhere such a voice loops the whole sample. The
  block's buffer is a second copy of the sample, read from the file on the
  block's first use.

### Still open

- The client game's slots 30 to 41 and `fadesound` are to be wired to the
  entry points above.
- The menu soundtrack, `sound/music/intro`, is named when the intro sequence
  reaches the main menu (`0x0040fdd7`); fnTech3's Alice interface does not
  have that sequence yet.
- Retail also cancels the fade when the server count changes
  (`CL_SetCGameTime` `0x00424bb2` to `0x0042d89f`), with an
  `S_StopAllSounds(0)` that keeps the music.
- The dialog path, the obstruction, a named room and the pause have unit
  evidence only; the zone reverb, occlusion and tone classes have the fourth
  run below and no listening test. Loop blocks, which no shipped record has, have unit and
  fixture evidence and a runtime start (see Results); `AL_SOFT_loop_points`
  was not available to run, so the OpenAL block path has none.
- The music's reapplication has unit evidence only. A song's kept place
  across `snd_restart` has fixture evidence, and a runtime run that shows
  the song set up again without error, not that it resumed where it
  stopped.

## Results

A developer client with the `alice` profile, built with
`-Dalice-development=true`, 800 by 600 in a window, with an isolated
writable home path under `.tmp/`, on both backends:

- `+devmap garden1`: the sound system starts (`Loading global/sound0.txt`,
  `Sound initialization successful`); the client game's configstring pass
  reaches import 42, which prints `SOUNDTRACK: Loading
  sound/music/garden1.mus` and moves the moods to `normal`, with no song
  missing; the client game then stops at import 37, which is not wired yet.
  The disconnect that follows frees the music (`[ normal | normal ] to [
  none | none ]`).
- From the menus, with no input, a script of commands: `music` is an unknown
  command; `loadsoundtrack sound/music/garden1.mus` and `playsong normal`
  hand the mixed stream to the backend (OpenAL: two buffers queued, then
  four; legacy: a background stream held); `play sound/ui/click_1` gains its
  `.wav`, starts on the menu channel at `s_volume` (0.8, from the
  installation's own saved config) and shows in `sounddump` as channel 0;
  `playmp3 video/ealogo.mp3` prints `Playing mp3 - video/ealogo.mp3`; and
  `cinematic ealogo.roq` frees the music, reads `video/ealogo.mp3`, prints
  `Playing mp3` once the first frame is up, and frees it at `finished
  cinematic`.
- The installation was left unchanged.

A second run on 2026-09-24, from the menus with no input: the same kind of
developer client (commit `2d35919` plus the uncommitted Alice work,
`fntech3.exe` SHA-256 `7AB97E59...`, Windows 11 x86 build), windowed at 800
by 600, `+set com_gameProfile alice`, `s_volume` and `s_musicvolume` at 0.1,
on each backend, with an isolated home holding a `global/sound1.txt` that
gives `sound/ui/projector_on.wav` a block of bytes 20,000 to 60,000 and a
two-song `sound/music/probe.mus` of shipped songs whose second is
`interrupt`:

- `play sound/ui/projector_on` starts the sound on channel 0 with its block.
  The OpenAL runtime on this machine is the system `OpenAL32.dll` ("Generic
  Software"), which has no OpenAL Soft extensions, so the voice loops the
  whole sample and says so (`loops whole: the OpenAL runtime has no
  AL_SOFT_loop_points for its loop block`); on the legacy mixer the start
  succeeds with the block, which nothing prints. What is heard was not
  checked, and the loop-points path itself has no runtime evidence.
- `playsong one`, then `playsong two`, stops and parks the first song;
  `snd_restart` then restarts the sound system and the video, and `playsong
  one` afterwards sets the song up again with no error and hands the mixed
  stream to the new backend (OpenAL: four buffers queued; legacy: a
  background stream held). Both clients quit cleanly.
- The installation was left unchanged.

A fourth run on 2026-09-24, after the backend began rendering Alice's voices
as it renders every profile's: a developer client from
`meson/build/win32-alice-content`, windowed on a private desktop, `+devmap
garden1`, `skipcinematic`, then `s_info` and `s_alDebugDump` with
`s_alSourceClassDebug 1`. The same steps on an earlier build
(`meson/build/win32-alice-vk`) show the difference (`.tmp/alice-audio-fnql/`):

- **Before:** no audio zones (that package had none), the environment
  unused, every voice `mode=3d tone=alice/none` with no occlusion, and the
  class summary `class=alice ... wet=0.00`.
- **After:** `Audio zones: 512 loaded from maps/garden1.azb, active
  auto-a1-hall-511->auto-a1-underwater-499 0.45, material soft`; the
  environment a hall blending toward an underwater zone (`wet 0.58`); the
  ambient loops in the `world` tone class and the creatures' and Mock
  Turtle's lines in `voice`; occlusion traced on the sounds behind geometry
  (`occ=0.55` on `uwater2.wav`, `occ=0.96` on `army_ant/idle1.wav` 2,296
  units away); and the Mock Turtle's line, heard at the listener, played
  head-relative (`mode=relative`). Run again with `s_muteWhenUnfocused 0`
  at `s_volume 0.02`, so that the unfocused window did not mute it and
  nothing was loud, the sends show: `class=world voices=4 ... wet=2.11` and
  `class=voice voices=2 ... wet=1.23`, the Mock Turtle's line `dry=1.00
  wet=0.32`.
- No `Alice room` line was printed: the client game named no room, so the
  zones stood.
- The legacy mixer (`s_backend legacy`) still starts the level's sounds on
  its own channels (`sounddump`).

No listening test has been made.

## Tests

- `tests/alice_music_tests.cpp`: Ritual's tokenizer (a comment's line tail,
  `/*/`, quoted escapes, the 1,024-byte word, signed bytes), the parser's
  stale arguments, moods, crossfades, the master fade including a fade down
  from 0.5, the music volume in every comparison, `S_Update`'s request to
  apply the volumes again (a song started without a crossfade taking the
  master, a fading player left alone), `_ftol` at its edges (1e10, -1e10,
  2^31, NaN, the infinities, the 64-bit range) and the fade times it gives a
  crossfade from `.mus` text, FreeAll keeping the table, and the 2,778-byte
  start and loop over synthetic MPEG-like frames with a resyncing fake
  decoder.
- `tests/alice_sound_tests.cpp`: the sound-info grammar and its range
  warnings, record 0's end-of-run defaults and the 1,000-record cap; the gain
  and cull at their boundaries; the axis transpose against the client game's
  `(pitch, -yaw, roll)`; the fade; the EAX table; a fake device driven
  through starts, culls, overrides, the no-steal rule, `maxnumber`, fixed
  origins stored as entity 0, entity following, the menu channel, the ending
  margin, obstruction, `s_volume` rescaling, pause, loop groups, the ambient
  level, `maxfactor`, reaping and the dialog voices; loop blocks from bytes
  to frames (rounding, -1 and out-of-range ends, negative starts, empty and
  inverted blocks, 8-bit files, resampled devices), the offset round a block,
  and the engine handing a block to a voice before it starts, dropping it
  with a new sample, and never to a loop group; and each voice's placement
  -- a world sound's volume, min distance and far end, a sound at the
  listener, an interface sound, a merged loop group's summed volume and its
  loudest member as its source, a moving loop's min distance and motion, and
  a dialog line with no far end.
- `tests/alice_audio_backend_tests.py`: the legacy mixer's own painter
  (sliced from `snd_mix.cpp`) repeating a block across paint-buffer passes,
  refusing one past the sample's end and ignoring it under another profile;
  the song players (sliced from `snd_alice.cpp`) keeping a parked song's
  decoder across `CloseLive` and dropping it with `Clear`; and the order of
  `S_Update`'s request and the music update.
