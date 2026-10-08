# Damaged demos under every demo-playing profile

The Q3/QL Demos row still asked to broaden truncation and malformed-input
coverage, and the RTCW matrix asked for malformed RTCW demos. Earlier
checkpoints ran hand-made truncation and invalid-length cases under `q3` and
`ql`. This record plays fixed envelope faults and 40 seeded body corruptions of
each of five demos: two Q3, one QL and two RTCW MP. One RTCW MP run stopped
after 16 of its 47 variants. Both Q3 sets are also played in retail
`quake3.exe` 1.32. No engine code changed; the
[machine-readable record](2026-09-18-demo-faults.json) holds identities, seeds,
commands and every played variant's outcome.

## Method

[`scripts/probe_demo_faults.py`](../../scripts/probe_demo_faults.py) builds each
variant from one valid demo and plays it in a fresh process and isolated home:

- **Envelope faults.** The file ends four bytes into a record header, or halfway
  through the last record's body, or lacks the final −1/−1. A record claims a
  length of −2, one byte over the profile's message limit (16,384 for Q3/QL,
  32,768 for RTCW), or 2,147,483,647. Bits are flipped in the middle of every
  later record.
- **Body corruption.** 1–24 seeded random bytes inside record bodies, so the
  envelope stays valid and every change reaches the message decoder.

Demos play under `timedemo 1`, which advances demo time 50 ms per rendered frame.
Once the demo has loaded, a console loop prints a tick every 100 frames for as
long as the main loop runs; an engine error clears it. A run is `completed` when
the demo completion path prints its timedemo line with no error, and `rejected`
when an engine error is printed; in both cases the process must still be running
one or two seconds later. It is `stalled` when there is no completion or error
but the tick loop keeps running to the 60-second limit, `crashed` when the
process dies with a failure status, and `hung` otherwise. The tick loop keeps the
command buffer busy, so `nextdemo` and the automatic quit never run. Every run is
windowed on an unswitched private desktop with sound and mouse/joystick input
disabled, and uses console commands only.

## Results

| Demo | Records | Played by | Seed | Variants | Completed | Rejected | Stalled | Crashed | Hung |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Q3 `four`, shipped with 1.32 | 1,262 | fnTech3 | 20260918 | 47 | 3 | 44 | 0 | 0 | 0 |
| Q3 `four`, shipped with 1.32 | 1,262 | retail 1.32 | 20260918 | 47 | 4 | 17 | 0 | 26 | 0 |
| Q3, recorded by fnTech3 | 121 | fnTech3 | 7 | 47 | 4 | 43 | 0 | 0 | 0 |
| Q3, recorded by fnTech3 | 121 | retail 1.32 | 7 | 47 | 5 | 27 | 1 | 14 | 0 |
| QL, recorded by fnTech3 | 240 | fnTech3 | 20260918 | 47 | 5 | 41 | 1 | 0 | 0 |
| RTCW MP, recorded by retail WolfMP | 146 | fnTech3 | 11 | 47 | 4 | 42 | 1 | 0 | 0 |
| RTCW MP, recorded by fnTech3 | 121 | fnTech3 | 13 | 16 of 47 | 4 | 12 | 0 | 0 | 0 |

Each run is the seven fixed variants and 40 corruptions from the seed shown.
The run of the fnTech3 RTCW MP recording stopped 3.6 seconds into its 17th
variant, while the map was loading, and wrote no result file; the cause was not
recorded. Its 16 finished rows are rebuilt from the outcomes the probe printed
and the console logs it saved. Earlier that day, an earlier fnTech3 build and an
earlier revision of the probe played all 47 variants of that recording, with
corruption seed 20260918, in real time rather than under `timedemo` and with no
`stalled` outcome. Its fixed variants ended the same way, and all 40
corruptions were rejected.

Every fixed variant behaves as fnTech3's earlier checkpoints require:
truncations complete normally and invalid lengths are rejected. No fnTech3 run
crashed or hung. Rejections name the decoder's own checks, most often an invalid
entity-state field count, a truncated entity or player-state delta, or an area
mask larger than the protocol allows.

A `stalled` run is playback waiting on its own data. In the QL example, reading
stops at the first damaged record (record 32, five bytes into its body, where
the snapshot's server time is written) while the engine keeps running.
Original id Tech 3 playback reads messages only while demo time has reached the
current snapshot's time, so a damaged time far in the future holds playback at
that point. The console stays usable; `disconnect` or another `demo` command
leaves it.

## Retail comparison

Retail `quake3.exe` 1.32 played the same 94 Q3 variants, byte for byte. It
crashed with an access violation (0xC0000005) on 40 of them: 26 of the 40
corruptions of its own shipped demo and 13 of the 40 corruptions of the fnTech3
recording, plus that recording's flipped-body variant. fnTech3 rejected all
of those: 27 with an entity-state field count larger than the field table and 7
with a player-state field count larger than its table, checks 1.32 lacks, and
the rest as truncated entity deltas or a bad movement angle. Every variant
retail rejected, fnTech3 also rejected, and every variant retail completed,
fnTech3 completed, with two exceptions:

- A record length of −2, in both sets: retail treats it as a truncated file
  and ends the demo normally, while fnTech3 rejects it, as the
  [invalid-length checkpoint](2026-09-08-demo-invalid-length.md) chose.
- One corruption of the fnTech3 recording gives a 128-byte area mask: retail
  accepts it and then stalls, while fnTech3 rejects the size.

## Not covered

RTCW SP demos are refused before any record is read (see the
[RTCW demo record](2026-09-18-rtcw-demos.md)), so they have no fault corpus.
This build has no result for the last 31 variants of the fnTech3 RTCW MP
recording. Retail QL and retail WolfMP were not run on these variants; the
[RTCW demo-fault record](2026-10-08-rtcw-demo-faults.md) plays both RTCW MP
sets in full, in a later build and in retail WolfMP.
Seeking and protocol switching within a damaged demo remain open.
