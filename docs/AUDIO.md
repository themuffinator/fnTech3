# Sound Guide

fnTech3 plays every game's sound through one modern sound system. The game
still decides *what* to play -- which sound, from where, how loud -- exactly as
the original did. fnTech3 decides how it *sounds*: in true 3D, with sounds
getting quieter with distance, muffled behind walls, echoing in big rooms,
and placed all around you on headphones. It uses each game's own sound
ranges, channels and loops, so every game still sounds like itself. The
original sound mixer is still there as an option.

This works in every game with a client: Quake III Arena, Quake Live, Return
to Castle Wolfenstein, Enemy Territory, Elite Force, Alice, F.A.K.K.2, Medal
of Honor and the Quake III test versions. Jedi Academy and Jedi Outcast are
not ready: their early profiles do not play the games' sound
properly yet.

Academy's profile now routes ordinary positional sounds and loops
through fnTech3's sound system and supplies voice-animation data. It reaches
the player menu and survives a map restart with either sound backend enabled;
complete playback and visible mouth animation are still being checked
([Jedi support audit](fntech3/2026-10-07-jedi-compatibility-audit.md)).

## Quick start

Most players need to change nothing: fnTech3 starts with its 3D sound on and
uses your computer's default speakers or headphones.

- **Headphones:** fnTech3 lets the sound system decide whether to use
  headphone surround (HRTF). To ask for it, use the
  [headphone recipe](#headphones-and-speakers).
- **Too loud or quiet:** change `s_volume` for everything, or
  `s_musicVolume` for the music.
- **Keep the sound when you Alt+Tab away:** `s_muteWhenUnfocused 0`.
- **Something is wrong:** type `s_info` in the console, then see
  [Troubleshooting](#troubleshooting).

Many sound settings only apply after `snd_restart`, which restarts the sound
without leaving your game. The guide says which ones.

## Volume, Music, And Focus Muting

- `s_volume`: the overall volume, from `0` to `2`. Default `0.8`, or `1.0` in
  Alice and F.A.K.K.2, as their originals had.
- `s_musicVolume`: the music, from `0` to `2`. Default `0.25`, or `0.60` in
  Alice and F.A.K.K.2.
- `s_voiceVolume`: other players' voice chat, from `0` to `2`. Default `1.0`.
  Only Quake Live has voice chat, through Steam.
- `s_muteWhenUnfocused`: `1` (the default) goes quiet when you switch to
  another window.
- `s_muteWhenMinimized`: `1` (the default) goes quiet when the game is
  minimised.

```cfg
seta s_volume "0.65"
seta s_musicVolume "0.15"
```

Keep playing sound while you are in another window:

```cfg
seta s_muteWhenUnfocused "0"
seta s_muteWhenMinimized "0"
```

If the music drowns out the game, lower `s_musicVolume` first. In Quake Live,
several players can talk at once without cutting each other off or the
movies' sound; `s_voiceStep` (default `0.02`) is best left alone.

## Headphones And Speakers

fnTech3 can give you surround sound on ordinary headphones, using *HRTF* --
a way of shaping sound so you can hear whether it is in front, behind, above
or below you. It can also play to 5.1 and 7.1 speaker systems. Restart the
sound with `snd_restart` after changing these.

- `s_alHrtf`: `auto` (the default) lets the sound system decide, `on` asks
  for headphone surround, `off` turns it off.
- `s_alHrtfId`: chooses one of the available HRTF sets by name or number;
  `s_alListHrtfs` lists them. Leave it empty for the default.
- `s_alOutputMode`: what you are listening on: `auto` (the default),
  `headphones`, `speakers`, `surround` or `5.1`, `quad`, `6.1` or `7.1`.

For headphones:

```cfg
seta s_alOutputMode "headphones"
seta s_alHrtf "on"
snd_restart
```

`s_info` shows what you asked for and what you got; a device that cannot do
HRTF turns it down. If headphone surround sounds wrong to you, `s_alHrtf off`
is a perfectly good choice -- some ears simply prefer it off.

Surround sound files play on surround speakers when your system supports it,
and are mixed down to stereo when it does not, so they always play.

## Choosing The Sound Device

fnTech3 uses your system's default playback device unless you choose another.

- `s_alListDevices`: lists the devices, marking the default and the one in
  use.
- `s_alDevice`: the device to use, by its name exactly as that list shows
  it. Leave it empty for the system default. Restart with `snd_restart`.
- `s_alAutoRecover`: `1` (the default) tries to reconnect by itself when a
  headset or HDMI device is unplugged and plugged back in.
- `s_alRecoverDevice`: reconnects the current device by hand. Add `force` to
  reopen it even when it says it is connected.

If a device you chose stops working after a hardware or driver change, set
`s_alDevice ""`, type `snd_restart`, and pick it again from
`s_alListDevices`. If fnTech3 cannot open the device you named, it uses the
default instead and says so.

## 3D Sound

All of this is on by default, and each part can be adjusted on its own.

- **Distance.** Sounds fade as they get further away, by each game's own
  ranges (see [Each Game's Sound](#each-games-sound)).
- **Walls.** `s_alOcclusion 1` (the default) muffles sounds that have walls,
  doors or floors between you and them, smoothly as you move.
  `s_alOcclusionStrength`, from `0` to `2` (default `1.0`), sets how much. If
  it sounds too dull, lower the strength rather than turning it off. An
  explosion is judged from just in front of the wall it hits, so a blast you
  can see is not muffled by the wall it went off against. Lava and slime
  between you and a sound muffle it like a wall, each game's own: the Jedi
  games no longer take their fog and invisible walls for liquids
  ([liquid feed record](fntech3/2026-10-22-liquid-feeds-all-profiles.md)).
- **Rooms.** `s_alReverb 1` (the default) adds the echo of the room you are
  in, blending smoothly from room to room. `s_alReverbGain`, from `0` to `2`
  (default `1.0`), sets how much. It needs a sound device with EFX -- OpenAL's
  effects, which most devices have, as `s_info` reports. Restart after
  changing `s_alReverb`.
- **Distance tone.** `s_alAirAbsorption` (default `2.0`) makes faraway sounds
  duller, as air does: `0` turns it off, `1` is like real air, higher is
  stronger. It needs EFX.
- **Moving sounds.** `s_doppler 1` bends the pitch of rockets and other fast
  projectiles as they fly past. `s_alDopplerFactor` (default `1.0`, up to
  `10`) strengthens it, and `s_alDopplerSpeed` (default `6000`, from `1000`
  to `20000`) is the speed of sound in game units -- lower values exaggerate
  the effect, and about `13500` is true to life at Quake III's scale.

Sounds that are meant to be "in your head" -- the announcer, menu sounds,
music, voice chat and movie sound -- are not placed in 3D, and your own
weapon keeps its close-up sound, though it is muffled with everything else
when you go under water. Menu sounds never take the room's echo or the
muffling under water, in any game's menus
([menu sound record](fntech3/2026-10-08-ui-sound-policy.md)).

### Sound zones

A *sound zone* file divides a map into rooms, each with its own echo, tone and
muffling. fnTech3 brings one for every map of seven games:

| Game | Sound zones |
| --- | --- |
| Quake III Arena | 35 maps, from FnQuake3 |
| Quake Live | one for each of the 149 BSP maps in the Steam release |
| Return to Castle Wolfenstein | all 32 single-player maps, including the cutscenes and the ending, and all 15 multiplayer maps |
| Enemy Territory | the six maps |
| American McGee's Alice | all 36 maps |
| Heavy Metal: F.A.K.K.2 | all 30 maps |
| Medal of Honor: Allied Assault | all 54 maps |

They are made automatically from each map's shape, and are still to be
checked by ear. Elite Force and the Jedi games have none yet, and other maps
-- custom maps, Workshop maps and Quake Live's optional extras -- fall back to
working out each room as you play, unless they bring a zone file of their own.

- `s_alAudioZones`: `1` (the default) uses the zone files; `0` ignores them.
  It works straight away.

Making your own zone files, for map and mod makers, is described with the
[zone compiler](../code/tools/audiozones/README.md); the
[package reference](fntech3/PACKAGES.md) explains where fnTech3 keeps them.

### Weapon sound tuning

For Quake III Arena, Team Arena and Quake Live, fnTech3 also brings the
weapon sound tuning of FnQuake3 -- each weapon's punch, reach, pitch and echo --
without changing the game's own sound files. A mod can bring its own tuning,
in `sound/fnql-weapon-sounds.sndshd`. Elite Force single player and the
Ritual games do not use it, and neither does the classic mixer.

## Each Game's Sound

Every game uses the same 3D sound -- distance, walls, rooms and headphone
surround -- but with its own sound ranges, as its original engine set them.
Where the original's exact behaviour matters, for comparing or for purists,
each game has an option:

| Game | How far sounds carry | Closest to the original |
| --- | --- | --- |
| Quake III Arena, Quake Live, Elite Force Holomatch | Full volume close by, silent beyond about 1,330 units, as in Quake III. | `s_backend legacy` |
| Return to Castle Wolfenstein, Enemy Territory | Each sound's own range, fading close to the original's straight line. | `s_alDistanceModel linear_clamped` fades exactly as the original; `s_backend legacy` also keeps its left-right placement. |
| Elite Force single player | The original's range for each kind of sound: most sounds, voices and quiet voices each have their own. Your own sounds play a little quieter; background sounds are placed where the level puts them. | `s_alDistanceModel linear_clamped` fades exactly as the original; `s_backend legacy` also keeps its placement. |
| Alice, F.A.K.K.2 | Each sound's own range, reaching 64 times its "full volume" distance. That this matches the original is a best guess. | `s_backend legacy` keeps the original's volume and muffling, with an even left-right spread. |
| Medal of Honor: Allied Assault | Each sound's own volume, range and pitch, from the game's sound lists. A character's new sound replaces its previous one on the same channel, as in the game. | None yet. Keep the default: the classic mixer ignores the game's ranges and pitch, loop volumes and stop requests -- also when fnTech3 switches to it by itself. |

Medal of Honor's sound fades now play. When a level ends, or restarts after
you are killed, the game asks for everything to fade out, music included, over
the time it names; both sound systems do so, and the next level brings the
sound back ([details](fntech3/2026-10-14-compatibility-round.md#medal-of-honor-sound-fades)).

Settings and commands only some games have:

- **Return to Castle Wolfenstein:** `s_defaultsound 1` plays a placeholder
  instead of silence when a sound file is missing (restart). Single player
  keeps the original's mouth movement, its music controls (see
  [Music And Sound Formats](#music-and-sound-formats)) and its streamed
  sounds, `streamingsound` and `stopstreamingsound`. Looping sounds have no
  Doppler effect.
- **Enemy Territory:** sounds the game leaves silent, such as jumping, stay
  silent; `s_defaultsound 1` plays a placeholder for them, for testing
  (restart). Looping sounds' Doppler effect and the game's "no fading with
  distance" flag are not carried over yet.
- **Elite Force:** `s_separation` (default `0.5`) is the original's
  left-right spread. `s_language DEUTSCH` plays German voices where your copy
  has them. In single player, `s_threshold1` to `s_threshold4` set how loud a
  voice must be for a speaker's mouth to open wider.
- **Jedi Academy:** `s_language` chooses the language the characters speak,
  as the game's setup menus do: `deutsch` plays the German voices, `francais`
  the French ones and `espanol` the Spanish ones, and anything else,
  `english` by default, the English ones. A line with no recording in that
  language plays in English. Type `snd_restart` after changing it to hear the
  new voices.
- **Jedi Outcast:** `s_language` chooses the language the characters speak,
  as the game's setup menu does: `deutsch` plays the German voices and
  `francais` the French ones, and anything else, `english` by default, the
  English ones. A line with no recording in that language plays in English.
  Type `snd_restart` after changing it to hear the new voices.
- **Alice and F.A.K.K.2:** `s_separation` (default `0.3`) spreads the
  characters' speech. `s_reverb 1` (the default) lets a room the level names
  set the echo; no map that came with the games does. `s_show_sounds 1`
  prints every sound as it starts and stops. Stereo WAV files are refused,
  as the original refused them. The sound commands are the original's:
  `play`, `sounddump`, `playsong`, `loadsoundtrack`, `playmp3` and `stopmp3`
  -- there is no `music` or `stopmusic`.
- **Medal of Honor:** `s_reverb 1` (the default) lets a room the level's
  script names set the echo.

## Music And Sound Formats

fnTech3 plays:

- **WAV** files in every game -- tried first when a sound is named without
  its extension;
- **Ogg Vorbis** files in every game;
- **MP3** files for the games that use them: Elite Force, including its
  speech and mouth-movement data; Alice and F.A.K.K.2, including their
  songs' loop points; Medal of Honor; Jedi Academy; and Jedi Outcast, which,
  like the original game, plays a sound's `.wav` file before an `.mp3` of
  the same name.

Music works as each game's original did:

- **Quake III Arena, Quake Live, Return to Castle Wolfenstein multiplayer and
  Elite Force:** `music <intro> [loop]` plays music, and `stopmusic` stops
  it. In Return to Castle Wolfenstein, `music` with one file loops it.
- **Quake Live** plays its menu music while the main menu is showing, stops
  it when a match starts, and plays it again when you return.
- **Return to Castle Wolfenstein single player** keeps its original music
  controls: fades, queued tracks (`queuemusic`) and the `s_currentMusic`
  readout. Enemy Territory starts its music the same way.
- **Alice and F.A.K.K.2** blend between the game's musical moods, and play a
  movie's soundtrack with it. `s_musicVolume` sets the songs' volume; a
  movie's soundtrack plays at full volume. Alice's menu music is not played
  yet.

When a piece of music finishes, it plays right to its end instead of being cut
off; `stopmusic` still stops it at once.

## The Classic Sound Mixer

fnTech3 has two sound systems:

- `s_backend openal` -- the default: the modern 3D sound in this guide.
- `s_backend legacy` -- the original software mixer, with each game's own
  volume and left-right rules. It still fades sounds with distance and places
  them left and right, but has no muffling by walls, room echo, sound zones or
  headphone surround. Use it if the modern sound will not work on your
  computer, or
  to hear a game as it originally sounded; check
  [Each Game's Sound](#each-games-sound) for what it leaves out in each game.

Change it, then type `snd_restart`:

```cfg
seta s_backend "legacy"
snd_restart
```

`s_backendActive` tells you which one actually started. If the modern sound
fails to start, fnTech3 switches to the classic mixer by itself rather than
leaving you in silence.

Settings for the classic mixer:

- `s_khz`: the sound quality, `8`, `11`, `22` (the default), `44` or `48`.
  Restart.
- `s_mixAhead`: how far ahead it mixes, default `0.2`. Raise it if the sound
  crackles; it adds a little delay.
- `s_mixPreStep`: Quake Live's timing lead, default `0.05`.
- `s_mixOffset`: a timing adjustment for testing, from `0` to `0.5`. Leave
  it at `0`.
- `s_device`: the sound device, on Linux builds that use ALSA (a Linux sound
  system) directly.

```cfg
seta s_backend "legacy"
seta s_khz "44"
seta s_mixAhead "0.2"
snd_restart
```

## Hearing Through Walls On Servers

Some sounds -- a loudspeaker, a humming machine -- are only sent to your
game when you can see where they come from. A server can choose to send
nearby sounds like these through walls too, without changing what anyone
sees. It is off by default, so servers play as the original games did.

- `sv_audioPVS 0`: off. This is the default.
- `sv_audioPVS 1`: also sends nearby sound-only things, such as speakers and
  background loops, that are out of sight but in a connected area.
- `sv_audioPVS 2`: also sends nearby out-of-sight things whose loops are
  gameplay sounds, which makes more of the game audible through walls.
- `sv_audioPVSRange`: how far, in game units. Default `1024`.
- `sv_audioPVSMaxEntities`: the most extra sounds per player. Default `16`.

Your own game then muffles them behind walls as usual. Every game's server
has it except Alice's and F.A.K.K.2's; outside Quake III Arena and Quake Live
its choice of sounds has not been checked.

On your side, `s_pvs 1`, from the original Quake Live, silences 3D sounds
from parts of the map you cannot see into. It is off by default. It works in
every game with the modern sound; the classic mixer ignores it in Return to
Castle Wolfenstein, Alice and F.A.K.K.2.

How fnTech3 decides which sounds you hear is described in the
[audio engine notes](fnql/AUDIO_ENGINE.md#audio-visibility-and-culling).

## Advanced OpenAL Settings

fnTech3's modern sound uses OpenAL. These settings are *requests* made when
the sound starts: your device or the OpenAL library may adjust them, and
`s_info` shows what you asked for beside what you got. Restart with
`snd_restart` after changing any of them.

- `s_alDistanceModel`: how sounds fade with distance: `inverse_clamped` (the
  default, a natural fade), `inverse`, `linear`, `linear_clamped`,
  `exponent`, `exponent_clamped` or `none`. Some games give `linear_clamped`
  and `none` their original meaning; see [Each Game's Sound](#each-games-sound).
- `s_alFrequency`: the mixing rate in Hz. Default `48000`.
- `s_alRefresh`: how often the mix updates, in Hz. Default `100`.
- `s_alMonoSources` and `s_alStereoSources`: how many sounds can play at once,
  in 3D and in stereo. Defaults `64` and `8`.
- `s_alOutputLimiter`: `1` (the default) stops loud moments from distorting.
- `s_alSourceResampler`: the quality of pitch and rate changes: `balanced`
  (the default), `fast`, `best`, or `library` to use OpenAL's own setting.
  Higher quality uses more processor time when many sounds play.
- `s_alSpatializeStereo`: `1` places a mod's two-channel sounds in 3D, for
  mods that expect that. Leave it at `0` for the original games.

The defaults are a good balance of quality and low delay. For more control,
the OpenAL Soft library has its own settings file; `s_alConfigHints` prints
where it is and the settings worth knowing. Smaller `period_size` or
`periods` values there lower the delay but can crackle on some devices, so
change them gradually. For example:

```ini
[general]
frequency = 48000
period_size = 256
periods = 3
output-limiter = true
```

`s_info` also shows the delay your device reports, where OpenAL can measure
it.

**Special sound files.** WAV files whose names include `uhj`, `uhj2`, `uhj3`
or `uhj4`, or `bformat2d`, `bformat3d` or `ambisonic`, are played as
surround-encoded audio where OpenAL supports it, and as stereo where it does
not.

## Useful Commands

- `s_info`: everything about the sound: which system and device are running,
  effects, zones, volumes, and what you asked for beside what you got.
- `s_alListDevices`: the sound devices you can choose.
- `s_alListHrtfs`: the headphone surround sets you can choose.
- `s_list`: every sound the game has loaded, and whether it was found.
- `s_stop`: stops every sound playing.
- `play <sound file>`: plays a sound, to check it, for example
  `play sound/player/jump1.wav`.
- `music <intro> [loop]` and `stopmusic`: start and stop music, in the games
  that have them.
- `snd_restart`: restarts the sound, after changing a setting that needs it.

### For tuning and testing

- `s_alDebugOverlay 1` or `2`: shows the room, zone and nearest sound on
  screen; `0` hides it.
- `s_alDebugVoice`: `-1` follows the nearest sound; a number follows one
  particular thing in the game.
- `s_alDebugDump`: prints a detailed snapshot of the 3D sound to the console.
- `s_alSourceClassDebug 1`: adds per-kind summaries to that snapshot, in
  developer mode.
- `s_alSoundStartDebug 1`: traces every sound Return to Castle Wolfenstein
  asks for, and any that fail. Use it with `logfile 2` to catch a sound that
  goes missing now and then, and set it back to `0` afterwards.

These only work with the modern sound system.

## Troubleshooting

### There is no sound at all

- Type `s_info`. It says which sound system is running, the volume, and
  whether the sound is muted -- and says plainly if nothing started. While a
  map is loading the sound is held back on purpose, and `s_info` says
  `registration`.
- Check `s_initsound` is `1` and `s_volume` is above `0`, then type
  `snd_restart`.
- Set `s_alDevice ""` to use your default device while you check; a saved
  headset or HDMI device name may be out of date.
- If the modern sound will not start, fnTech3 tries the classic mixer by
  itself. To choose it yourself, set `s_backend legacy` and `snd_restart`.
- If neither works, include the part of the console starting
  `------ Initializing Sound ------` in your report; it names what failed.

### I changed a setting and nothing happened

Many sound settings wait for `snd_restart`: `s_backend`, `s_alDevice`,
`s_alHrtf`, `s_alHrtfId`, `s_alOutputMode`, `s_alDistanceModel`,
`s_alFrequency`, `s_alSourceResampler`, `s_alRefresh`, `s_alMonoSources`,
`s_alStereoSources`, `s_alOutputLimiter`, `s_alSpatializeStereo`,
`s_alReverb`, `s_khz`, and Return to Castle Wolfenstein's and Enemy
Territory's `s_defaultsound`.
`s_alAudioZones` works straight away, and `s_language` applies to sounds
loaded after you change it.

### The 3D sound seems flat

- Type `s_info` and check that `s_backendActive` is `openal`.
- For echo, check that `s_info` says `EFX support: enabled`, and that
  `s_alReverb` and `s_alOcclusion` are `1`.
- To make the effects stronger, raise `s_alReverbGain` or
  `s_alOcclusionStrength` a little at a time.

### The wrong device is playing

- Type `s_alListDevices` and copy the device's name exactly.
- Set `s_alDevice` to it, type `snd_restart`, and check `s_info`.
- If the device is not listed any more, set `s_alDevice ""` to go back to the
  default.

### The sound did not come back after unplugging a device

- Keep `s_alAutoRecover 1`, so fnTech3 reconnects by itself.
- After plugging the headset, HDMI or Bluetooth device back in, type
  `s_alRecoverDevice`, or `s_alRecoverDevice force`.
- If that does not work, type `snd_restart`; if the device's name changed,
  set `s_alDevice ""` first.

### Headphone surround is off or sounds wrong

- Compare what you asked for with what you got in `s_info`.
- `s_alListHrtfs` shows whether your device offers any headphone surround.
- Try `s_alOutputMode headphones` with `s_alHrtfId ""`, then `snd_restart`.
- If it still says it is off, your device has declined it; use
  `s_alHrtf auto` or `off`.

### `s_info` shows different values from the ones I set

That is normal: the device adjusted some of your requests. Use `auto` for
headphone surround and output unless you are testing a device, clear
`s_alHrtfId` if its set is no longer listed, and keep the default source
counts.

### The modern sound keeps switching to the classic mixer

- Set `s_alDevice ""`, `s_alHrtf auto`, `s_alHrtfId ""` and
  `s_alOutputMode auto`, then `snd_restart`.
- On Windows, check that the OpenAL file that came with fnTech3 is still
  beside `fntech3.exe`; on Linux, that OpenAL is installed.
- If it still will not start, keep `s_backend legacy` on that computer.

### A map sounds different than expected

- Check the `Audio zones` line in `s_info`.
- `s_alDebugOverlay 2` shows which zone you are in.
- Set `s_alAudioZones 0` to compare with no zone file.
- A broken zone file is ignored with a warning, and a missing one is fine.

### The game goes quiet when I switch windows

That is on purpose: `s_muteWhenUnfocused 1` and `s_muteWhenMinimized 1` mute
the game while you are elsewhere. Set them to `0` to keep the sound playing.
On Windows, if `s_info` says `offscreen-muted`, the game's window is on a
desktop that is not showing, as testing tools use; it plays again when that
desktop is shown.

## Recommended Starting Points

**The default** -- fnTech3's 3D sound as intended:

```cfg
seta s_backend "openal"
seta s_alDevice ""
seta s_alHrtf "auto"
seta s_alOutputMode "auto"
seta s_alReverb "1"
seta s_alOcclusion "1"
seta s_alAudioZones "1"
seta s_volume "0.8"
seta s_musicVolume "0.25"
snd_restart
```

**Competitive** -- less echo and music, clear directions:

```cfg
seta s_backend "openal"
seta s_alReverb "0"
seta s_alOcclusion "1"
seta s_alOcclusionStrength "0.5"
seta s_musicVolume "0"
snd_restart
```

**Classic** -- the original software mixer:

```cfg
seta s_backend "legacy"
seta s_khz "22"
seta s_mixAhead "0.2"
snd_restart
```

## Related Guides

- [Console guide](CONSOLE.md) for typing commands and settings.
- [Games guide](GAMES.md) for which games run and where their files are.
- [Audio engine notes](fnql/AUDIO_ENGINE.md) for how the sound system works,
  for the technically minded.
