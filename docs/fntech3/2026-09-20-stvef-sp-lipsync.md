# Elite Force single-player lip-sync

## Scope

This record covers how wide a mouth Elite Force single player opens while a
voice plays: what the engine measures, how often, what it measures against,
and how the number reaches the game that moves the faces. Bink movies and
single player's scissor belong to later slices. The profile stays behind
`-Dstvef-development=true`, and Holomatch has none of this — retail's
`stvoyHM.exe` registers none of the four cvars and measures nothing.

The measurement lives in `code/client/audio/shared/STVEFLipSync.h`, which both
of the engine's sound backends share, beside the
[distances](2026-09-20-stvef-sp-sound-distances.md) the same voices carry over.

## The contract

| Contract | Retail `stvoy.exe` 1.20 | fnTech3 |
| --- | --- | --- |
| What the game reads | One number an entity, 1,024 of them, through the `S_Override` slot of its import table (`0x42562b`) | The same array, through the same slot; the client hands over what it measured and a server with nothing playing leaves every mouth shut |
| When it is measured | As each mix ends, after the samples have been submitted (`0x4218c3`) | The same in the legacy mixer; the device does the mixing for the OpenAL backend, so the measurement is taken where each voice has played to, the moment the game asks |
| Which channels | A voice and an attenuated voice, and nothing else (`0x4214b4`) | The same two, on the numbers those channels are marshalled to: the engine's own voice channel, and single player's attenuated voice above the engine's |
| What is measured | Ten samples of the sound, one every hundred from where the channel has reached, the top byte of each squared and averaged as a whole number (`0x421520`, `0x421630`) | The same. The samples are read signed and shifted arithmetically (`movsx`, `sar`), so each byte is −128 through 127 |
| A channel past its end | Counts no samples and answers with none at all, which is neither a width nor a closed mouth (`0x421617`) | The same, and nothing is remembered for it |
| What it is measured against | `s_threshold1` through `s_threshold4`, `0.5`, `4.0`, `7.0` and `8.0`, unarchived, compared in that order (`0x41fb5d`), each times the loudest byte the sound reaches | The same four cvars with the same defaults and the same order, registered for single player alone |
| The answer | A closed mouth at −1, then 5, 6, 7 and 8 (`0x421651` onwards) | The same five |
| The loudest byte | Raven's own `#MAXVOL` in the comment field of a trailing ID3v1 tag; a file without one counts as 128 (`0x408c70`, `0x408df2`) | The same, read by the MP3 codec and carried on the sound |
| How often | No channel is measured again until 800 samples have been mixed, but one the mix has reached the very start of is always measured (`0x42153e`, `0x4214fc`) | The same wait, counted in painted samples by the legacy mixer and against the device's own rate by the OpenAL backend |
| Between measurements | The entity answers with what it last measured, which retail keeps in a second array that survives the mix (`0x696168`) | The same second array, kept beside the one the game reads |
| A voice starting | Closes that entity's mouth (`0x4208ec`) | The same, in both backends |
| An entity saying nothing | Reads no answer at all: the array the game holds is cleared at the start of every mix and written only for the channels playing a voice then (`0x421493`) | The same, so a mouth is never held open by a voice that has stopped |
| The trace | `s_show 3` prints `(entity)channel name vol = level` for every mouth measured (`0x4214e8`) | The same line from the legacy mixer, which is where retail's own mixer prints it |

Retail keeps two arrays, and they say different things. Conflating them would
hold a mouth open after its voice stopped, since the memory of the last
measurement never empties on its own; the array the game reads is cleared
every mix instead, and only the memory persists.

One guard in retail's loop could not be identified: alongside the test that a
channel has a sound, it requires a field just past the sound pointer to be
clear, in a channel structure of its own that carries a decoder state fnTech3
has no equivalent of. The engine measures a channel with a sound the mixer
still considers playing, which is the same set of channels for every sound
single player plays.

## What it touches

- `code/client/audio/shared/STVEFLipSync.h` — the measurement, the widths, the
  two arrays, the wait between measurements, and the top bytes the OpenAL
  backend keeps in place of samples it does not hold.
- `code/client/audio/codecs/snd_mp3_frames.h`, `snd_codec_mp3.cpp`,
  `snd_codec.h` — Raven's `#MAXVOL` read from the tag and carried out of the
  codec.
- `code/client/audio/snd_local.h`, `legacy/snd_mem.cpp` — the sound carries the
  loudest byte it reaches, and the four thresholds are read in retail's order.
- `code/client/audio/legacy/snd_dma.cpp`,
  `code/client/audio/openal/AudioSystemWorld.inl`, `AudioSystemBackend.inl` —
  both backends measure, both close a mouth as a voice starts, and both answer
  the same interface entry.
- `code/client/audio/snd_main.cpp`, `snd_public.h` — the cvars, the interface
  entry, and the hand-over as the client updates its sound.
- `code/server/sv_game.cpp`, `code/qcommon/qcommon.h` — what the client
  measured copied into the array the game reads.

A sound costs the OpenAL backend one byte a sample more in single player,
since the device keeps the samples and the engine does not; the top byte is
all a measurement looks at, so that is all the engine keeps.

## Results

A developer client with the `stvef-sp` profile, 1,024 by 768 in a window, on
`voy1`, with an isolated writable home path and the installation left
unchanged:

- The legacy mixer traced 88 measurements of Kim's
  `sound/voice/kim/cin/03/conduitsrupturing.wav` on entity 116 as he speaks
  across the bridge: 40 closed, 42 at the narrowest width, one at the next,
  four at the one above, and one answer of none — the first, before the
  entity had ever been measured, which is what retail answers from an empty
  memory. The mouth moves as the line is spoken rather than sitting open.
- The OpenAL backend measured the same voice on the same entity and reported
  the same widths, and reported the loudest byte it was measuring against as
  116. Raven's tag in `pak0.pk3` holds `#MAXVOL 116` for that file, so the
  number the measurement uses is the file's own rather than the fallback of
  128.
- The ambient loop on the bridge was not measured, and neither was anything
  else off the two voice channels.
- The installation was byte-identical to the snapshot taken before the runs.

## Tests

- `stvef_sp_lipsync_tests` compiles the measurement: the mean of the squares
  of the top byte at full scale both ways and at silence, that only the
  samples inside the sound are counted and a channel past the end counts
  none, that the mean stays a whole number, each of the four thresholds at
  its own edge, the fallback of 128, both arrays and the wait between
  measurements, a channel at the very start always being measured, a mouth
  closing as a voice starts, nothing read or written outside the array, the
  top bytes measuring as the samples they came from, and Raven's tag with
  its keyword missing, its number missing and its digits running to the end
  of the field.
- It pins the array the game reads against the ABI and the game module, the
  four cvars with retail's defaults and order registered for single player
  alone, the two channels measured in both backends, the loudest byte's path
  from the codec to each backend, and every backend having to supply the
  interface entry.

## Remaining

- Bink movies.
- Single player's scissor.
- A comparison of lighting values and RDM5 characters with the retail
  renderer.
