# Damaged RTCW MP demos against retail WolfMP

The [damaged-demo record](2026-09-18-demo-faults.md) played fault variants of
two RTCW MP demos in fnTech3 but finished only 16 of the 47 variants of one,
and never ran retail `WolfMP.exe` on any. Both are done here: fnTech3 and
retail `WolfMP.exe` 1.41 each played all 94 variants of the two recordings,
byte for byte the same inputs. fnTech3 completed or rejected every variant,
with no crash, hang or stall, as its checks require. Retail crashed on two
variants and stalled on two more that fnTech3 rejects; on every other variant
the two engines ended alike, except a record length of -2. No engine code
changed. The [machine-readable record](2026-10-08-rtcw-demo-faults.json)
holds identities, commands and every variant's outcome in each engine.

## Method

[`scripts/probe_demo_faults.py`](../../scripts/probe_demo_faults.py) is the
earlier record's probe, with the same seven fixed faults and 40 seeded body
corruptions per demo: the retail recording with seed 11, fnTech3's with seed
13, as before. Its `--retail` option, which ran `quake3.exe`, now also runs
the installation's `WolfMP.exe` for `rtcw-mp`, reading its console log from
`main/rtcwconsole.log`. Each variant plays in a fresh process with an
isolated home, under `timedemo 1`, windowed on an unswitched private desktop
with sound and mouse/joystick input off, through console commands only. The
installation was unchanged after every run. fnTech3's client was a Windows
x86 build of `6cb358f9` plus the [entity-query change](2026-10-08-rtcw-entity-queries.md),
which demo playback does not reach.

## Results

| Demo | Played by | Completed | Rejected | Stalled | Crashed | Hung |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| RTCW MP, recorded by retail WolfMP | fnTech3 | 4 | 42 | 1 | 0 | 0 |
| RTCW MP, recorded by retail WolfMP | retail WolfMP | 5 | 40 | 1 | 1 | 0 |
| RTCW MP, recorded by fnTech3 | fnTech3 | 5 | 42 | 0 | 0 | 0 |
| RTCW MP, recorded by fnTech3 | retail WolfMP | 6 | 38 | 2 | 1 | 0 |

Every fixed variant ended as fnTech3's checks require: truncations and the
flipped bodies complete, invalid lengths are rejected. The variants on which
the engines differ:

- **A record length of -2**, in both sets: retail ends the demo as a
  truncated file, fnTech3 rejects it, as the
  [invalid-length checkpoint](2026-09-08-demo-invalid-length.md) chose for
  every profile. Retail `quake3.exe` does the same as `WolfMP.exe`.
- **Two corruptions crash retail** with an access violation (0xC0000005),
  one in each set, 11 and 14 seconds in. fnTech3 rejects both: "invalid or
  truncated player-state delta".
- **Two corruptions stall retail** in the fnTech3 recording: playback waits
  on a damaged snapshot while the console stays alive. fnTech3 rejects one as
  a truncated player-state delta and the other for an area mask of 254 bytes,
  more than the protocol allows.

The one stall fnTech3 shows, in the retail recording, is a stall in retail
too: the same variant waits there on its own data.

## Not covered

- RTCW SP demos are still refused before any record is read, as the
  [RTCW demo record](2026-09-18-rtcw-demos.md) explains.
- Seeking and protocol switching within a damaged demo remain open.
