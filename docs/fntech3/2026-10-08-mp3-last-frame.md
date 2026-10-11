# The last MP3 frame before stray bytes, 2026-10-08

Fn's MP3 frame walker, which Elite Force, Jedi Academy, Jedi Outcast, Alice,
F.A.K.K.2 and Medal of Honor share, dropped a file's last frame when stray
bytes followed it. The Outcast MP3 slice (`2026-10-08-jk2-mp3-sounds.md`,
uncommitted when this was written) found the one case among Raven's 7,971
stored Outcast sizes: `sound/weapons/force/drain.mp3` decoded a frame short.
This record reads, in all five Raven engines, that each frame is decoded from
its own header and nothing after it, observes that Miles 6.0b, the decoder
Alice and F.A.K.K.2 ship, decodes such a frame too, and changes the walker to
hand that frame to the decoder alone. Every one of Raven's stored sizes in
Elite Force (8,074), Outcast (7,971) and Academy (12,510) is now reproduced.
Besides `drain.mp3`, which Academy ships as well, 675 of F.A.K.K.2's files
gain their last frame, as Miles decodes them; every other MP3 file of the six
titles decodes to the same samples as before. The
[JSON companion](2026-10-08-mp3-last-frame.json) holds every address, count
and result. No gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `stvoy.exe`, Elite Force single player v1.20 | `8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74` |
| `jk2mp.exe` 1.02a | `2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a` |
| `jk2sp.exe` 1.02a | `0d3c635ee5b20ac32fc957e00cf21e65ebc1584112b11f6ae026706dcc0d0e0c` |
| `jamp.exe` 1.01 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `jasp.exe` 1.01 | `9351a8de230fcd577e2b474889d08f38982450306767a6823dd659482566d6ce` |
| `mss32.dll` 6.0b, F.A.K.K.2's and Alice's alike | `14fb663762695a02e06b144c487254b08163185fa009efb32df733882cd570de` |
| `mp3dec.asi` 6.0b, F.A.K.K.2's and Alice's alike | `c42fa55d138d4f6616b0e962d122ac7d143cf1d41e09df8b4086dac00e814753` |
| Outcast `base/assets0.pk3` | `e8e466f219bb2faed536021bb0d10aa6b7f5cd687302aa43080da2debdae307c` |
| its `sound/weapons/force/drain.mp3` (19,773 bytes, CRC-32 `9dce80b5`), the same bytes in Academy's `assets0.pk3` | `02f5bc62fdd4eebef0180beeb0432cd268096837c790b090f0eaee59038fefe2` |

minimp3, the decoder Fn's MP3 codec drives, is at revision
`ea99364f61c14656440e8d77e9c233ccf3124633` (`subprojects/minimp3.wrap`). Every
installation was only read; Miles was loaded from the F.A.K.K.2 layout laid
down under `.tmp`.

## The file

**Observed** (read from the pak with Python): `drain.mp3` holds 93 MPEG-1
Layer III frames, 44.1 kHz mono, at 64, 80 and 96 kbps. The first frame is
208 bytes and unpadded; the last runs from byte 19,435 to 19,643, 209 bytes
with its padding byte. One byte, `00`, follows it at 19,644, and Raven's
ID3v1 tag starts at 19,645. The tag's `#UNCOMP 214272` is 93 frames of 2,304
bytes, 16-bit mono: every frame, the last included.

## How Raven's engines decode the last frame

**Observed** (`stvoy.exe`, `jk2mp.exe`, `jk2sp.exe`, `jamp.exe` and
`jasp.exe`, read with Capstone and pefile; no retail code is incorporated).
Each engine sizes an MP3 and unpacks it with one loop. It reads the first
frame's header, whose size it takes without the padding byte (`jk2mp.exe`
0x45a6a0 sizes an MPEG-1 Layer III frame as 2,880 times the kilobits per
second over the sample rate in 50 Hz units, with no padding term: 208 bytes
for `drain.mp3`'s first frame), drops a trailing 128-byte `TAG` block, and
then, while the bytes left are at least that size, hands the decoder the next
frame, adding the bytes the decoder consumed and produced; it stops when the
decoder consumes nothing or nothing is left. Sizing sets a flag the decoder
reads to measure a frame without decoding it.

| Engine | Sizing | Unpacking | Bytes left below the first frame's size | Decoder |
| --- | --- | --- | --- | --- |
| `stvoy.exe` | 0x43bf70 | 0x43c0a0 | 0x43c034, 0x43c163 | 0x4338b0 |
| `jk2mp.exe` | 0x45b280 | 0x45b3b0 | 0x45b333, 0x45b463 | 0x453000 |
| `jk2sp.exe` | 0x44d3f0 | 0x44d520 | 0x44d4a3, 0x44d5d3 | 0x4450c0 |
| `jamp.exe` | 0x4857a0 | 0x4858c0 | 0x485850, 0x485970 | 0x47c260 |
| `jasp.exe` | 0x44fa80 | 0x44fba0 | 0x44fb30, 0x44fc50 | 0x4478e0 |

The decoder passes each frame to the routine its initialization installed
for the stream; `jk2mp.exe` installs 0x456870 for an MPEG-1 Layer III header
(0x457c82 to 0x457c8d), and the other engines' MPEG-1 Layer III routines are
`stvoy.exe` 0x437320, `jk2sp.exe` 0x448a00, `jamp.exe` 0x47e2c0 and
`jasp.exe` 0x449830. Each reads twelve bits and requires the sync `0xfff` at
its own frame (`stvoy.exe` 0x43733c, `jk2mp.exe` 0x45688c, `jk2sp.exe`
0x448a1c, `jamp.exe` 0x47e2ef, `jasp.exe` 0x44984e), unpacks the side
information and consumes the frame: the stream's frame size plus this
frame's padding byte. The Jedi engines also hand the decoder the end of the
data, the byte before a trailing tag, and their routine returns nothing for a
frame that would end past it (`jk2mp.exe` 0x4568bf, `jk2sp.exe` 0x448a4f,
`jamp.exe` 0x47e323, `jasp.exe` 0x449882); Elite Force's has no such check.
**No routine reads past its own frame, and no loop looks for the next frame's
sync before decoding one.**

So every engine decodes all 93 of `drain.mp3`'s frames: at the last, 210
bytes are left, at least 208; the frame ends at byte 19,644, inside the data;
then one byte is left and the loop stops. That is Raven's 214,272 bytes.

## How Miles decodes it

Alice and F.A.K.K.2 play MP3 through Miles 6.0b; their `mss32.dll` and
`mp3dec.asi` are the same files. `alice.exe` loads Miles itself: among the 62
functions it looks up by name are `_AIL_set_named_sample_file@20` and
`_AIL_set_3D_sample_file@8`, which take a sound's file image.

**Observed** (`.tmp/mp3-last-frame/miles_probe.cpp`, not committed: Miles
loaded into a probe, `AIL_startup`, then `AIL_decompress_ASI` on a file image
named `.mp3`, which decodes through the same `mp3dec.asi`; no audio device is
opened):

- `drain.mp3` decodes to 107,136 samples, all 93 frames, with or without its
  stray byte.
- 675 of F.A.K.K.2's 1,340 `.mp3` files are MPEG audio wrapped in a RIFF
  WAVE file (format `0x55`), whose `data` chunk ends exactly at the last
  frame and is followed by a 26- or 82-byte `LIST` chunk. Miles decodes all
  their frames, whether handed the whole file or the `data` chunk alone, and
  each file's own `fact` chunk counts every frame too (`blooper.mp3`: 538
  frames of 576, 309,888 samples).
- Three of F.A.K.K.2's plain MP3s decode to the same count with 1, 4 or 128
  bytes appended.

So Miles, too, decodes a last frame whatever follows it.

## Why fnTech3 dropped it

**Observed** in minimp3 at the pinned revision. `mp3dec_decode_frame`
(`minimp3.h` line 1713) takes a frame directly only when the frame ends its
input or the four bytes after it are a header of the same stream (line
1723). Otherwise it clears its whole state, the bit reservoir included (line
1730), and searches, and its search refuses a frame followed by fewer than
four bytes (`mp3d_match_frame`, line 1657). Handed `drain.mp3`'s last frame
and the stray byte, minimp3 refused the frame and reported no frame in those
210 bytes, and the walker ended there: 92 frames, 211,968 bytes. The walker
reads a RIFF-wrapped file as MPEG data, skipping its header to the first
frame, so in those 675 F.A.K.K.2 files the `LIST` chunk played the stray
bytes' part.

## The change

`code/client/audio/codecs/snd_mp3_frames.h`:

- `Mp3HeaderFrameBytes` reads a frame's size from its header alone: MPEG-1,
  MPEG-2 and MPEG 2.5, which defines Layer III only; Layers I to III; the
  bitrate and sample-rate tables; and the padding byte, four for Layer I. A
  free-format frame, whose size no header holds, and an invalid header get
  none. `Mp3ContinuesStream` asks whether the bytes after a frame begin a
  frame of the same stream: a valid header with the same MPEG version, layer
  and sample rate.
- Where a frame is due, at the start or just after a frame the decoder
  found, the walker hands the decoder exactly that frame's bytes when the
  frame is complete and what follows does not continue its stream, and the
  rest of the data otherwise, as before. minimp3 then decodes the frame
  directly, as a frame that ends its input, with its bit reservoir.
- The frame goes alone on the first attempt: after a refusal minimp3 has
  already cleared its reservoir, so a second attempt would decode the frame
  without the bytes of earlier frames that its audio starts in.
- Effects (`Scan`, then `Read` into mono), streams (`Read`) and Alice's
  seeks (`SamplesBefore`) share the rule, so a sound's size, its samples and a
  seek agree. After a seek into the middle of a frame, as every Alice song
  makes, the decoder finds the next frame itself, as before: bytes there that
  only look like a header are not taken for a frame. A seek to the start is a
  start.
- Retail's size rule stays: a last frame shorter than the first frame's
  unpadded size is still not decoded.

**Differences recorded, not reproduced:**

- Retail stops at the first bytes after a frame that do not begin with a
  sync; Fn's decoder resyncs past them. No installed file has such bytes
  anywhere but after its last frame: in all six installations the walker
  hands a frame over alone 677 times, each the last frame of `drain.mp3` or
  of one of the 675 F.A.K.K.2 files.
- The Jedi engines refuse a frame that would end past the data, and Elite
  Force's decodes it, reading past the data. Fn decodes no incomplete frame,
  as before.
- Fn does not read the RIFF wrapper; it decodes the frames inside it, which
  are the `data` chunk's.

## Every MP3 file, before and after

`.tmp/mp3-last-frame/sweep.py` (not committed) compiles a probe twice, with
HEAD's walker and with this one, both with the minimp3 configuration of
`snd_codec_mp3.cpp`, for i686 with clang. For every `.mp3` of each
installation it decodes the file as an effect (mono) and as a stream (its own
channels), hashes the stream's samples, counts what Alice's seeks pass over
at the end and at a song's start offset, and plays the file as Alice plays a
song: opened, sought to that offset and read to the end. Pak members were
extracted under `.tmp` and deleted afterwards; F.A.K.K.2's are the laid-down
`pak0`, `pak1` and the 1.02 patch's `pak2`, and Medal of Honor's are its
loose `main/sound` files, read in place.

| Title | MP3 files | Raven sizes stored | Reproduced before | Reproduced after | Files that change | Frames handed alone |
| --- | --- | --- | --- | --- | --- | --- |
| Elite Force | 8,129 | 8,074 | 8,074 | 8,074 | 0 | 0 |
| Jedi Outcast | 8,126 | 7,971 | 7,970 | 7,971 | 1 | 1 |
| Jedi Academy | 12,902 | 12,510 | 12,509 | 12,510 | 1 | 1 |
| Alice | 33 | none | | | 0 | 0 |
| F.A.K.K.2 | 1,340 | none | | | 675 | 675 |
| Medal of Honor | 122 | none | | | 0 | 0 |

Each file that changes gains exactly one frame, 26 ms (1,152 samples at
44.1 kHz or 576 at 22.05 kHz), as an effect, as a stream and in Alice's seek
counts; its other samples are the same. Every other file decodes to the same
samples, sizes and seek counts as before. The 34 F.A.K.K.2 files whose
reading yields fewer samples than their sizing counts do so before and after
alike (below).

The committed `scripts/probe_stvef_mp3.py`, run on Elite Force with this
walker, decodes all 8,129 files and reproduces all 8,074 of Raven's sizes:
8,071 as effects and the three tagged music tracks as streams. The Outcast
slice's sweep (`.tmp/jk2-mp3-sounds/decode_sweep.py`), run with this walker,
decodes all 8,126 files and reproduces all 7,971.

`drain.mp3` now unpacks to 214,272 bytes, Raven's size, where it unpacked to
211,968. The first 211,968 bytes are the same; the 93rd frame is 26 ms of
digital silence, the sound's faded end.

Beside Miles, on all 1,340 F.A.K.K.2 files: the 675 that change now decode
to Miles's count where they decoded a frame fewer; 157 other RIFF files and
469 plain MP3s decode to Miles's count before and after. Miles decodes 34
plain MP3s to one frame fewer than Fn's sizing counts (in the three tried
with bytes appended, whatever follows their last frame). Five files begin
with a 320-byte Macintosh header (`MACR`) before their MPEG data: Miles,
handed them as `.mp3` images, hangs on four (three are copies of one file)
and does not decode the fifth. None of these changes.

## Presentation (invariant 11)

The change decides which frames reach the mixers, not how they are presented:
the sounds still play through Fn's OpenAL pipeline or the legacy mixer as
before.

## Tests

- `tests/stvef_mp3_frames_tests.cpp` gains three tests. `TestHeaderFrameSizes`
  sizes twenty headers (MPEG-1, MPEG-2 and MPEG 2.5; Layers I to III; padding
  and CRC; free format, reserved fields and short input) and checks which
  next headers continue a stream. `TestLastFrameBeforeStrayBytes` builds five
  real MPEG-1 Layer III frames at 64, 80 and 96 kbps, padded and not, ending
  in one stray byte, three, 100 or 300 zero bytes, a header of the stream or
  nothing, each with and without an ID3v1 tag, and decodes them through a fake
  decoder that restates minimp3's rule for trusting a frame. Sizing, reading
  at five chunk sizes and Alice's seek count all five frames; the last goes
  alone exactly when stray bytes follow it, and every other call gets the
  rest of the data. A shorter last frame is still dropped, a frame found and
  decoded to nothing still places the next, and one frame with a stray byte
  decodes, again after a seek to the start. `TestSeekIntoFrame` seeks into a
  frame onto bytes that look like a header and reads the real next frames.
- Twenty-one mutations of the walker each fail it: never or always handing a
  frame alone, handing it a byte short, the rule missing from any one of the
  three walks, alignment ignored, kept after a seek, lost at the start or
  after a frame decoded to nothing, any header taken as continuing, and seven
  wrong frame sizes (`.tmp/mp3-last-frame/mutations.py`).
- The test builds with `win32-dev`'s MSVC flags (`/W4 /WX /O2`) and with clang
  (`-Wall -Wextra -Werror`); `tests/alice_music_tests.cpp`, the other test of
  the walker, is unchanged and passes, as do the Python tests that compile the
  walker or the codec.

## What stays open

- Raven's decoded samples were not compared, only sizes; `drain.mp3`'s last
  frame is silence. Miles's samples were compared by count only.
- Outcast's sounds reach this decoder only with the Outcast MP3 slice
  (`2026-10-08-jk2-mp3-sounds.md`), uncommitted when this was written.
- Unchanged by this record: 34 of F.A.K.K.2's plain MP3 music files yield
  fewer samples when read than their sizing counts (one frame for most, the
  count Miles decodes; 54 frames for `sound/music/cave/normal.mp3`), so as
  effects they would not load; and Fn decodes the five files behind a
  Macintosh header that Miles hangs on or refuses as `.mp3` images, among
  them `sound/music/gith/githorch.mp3`.
