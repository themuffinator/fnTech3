# The IHV Test's demos are recorded, 2026-10-08

The developer client refused `record` with "Recording an IHV Test demo is not
supported yet." It now records the build's own length-only `.dm3`, as
`quake3.exe` does. Recordings made on fnTech3's server and on the original
server play to their end in both clients. The original client's own recording
plays in fnTech3. The [JSON companion](2026-10-08-q3-ihv-demo-recording.json)
holds identities, commands and results.

## Observed in the original

Addresses are virtual addresses in `quake3.exe` (SHA-256 `e9fcaa0a...5135d37`),
read with pefile and Capstone. No original code is incorporated.

**Starting** (`CL_Record_f`, `0x404560`).

- The console command takes one name. It refuses to start twice, or before
  the client is active.
- It opens `demos/<name>.dm3`, prints `recording to %s.`, and marks the
  recording as waiting.
- The first record is a gamestate built from the client's own state
  (`0x404650..0x404707`): every set configstring, then every baseline that
  carries a model (byte `0x95`). The server's own gamestate also sends a
  baseline with only a looping sound, effects or the byte at 2 (`0x4187d1`),
  so this test is narrower.
- The original's menu names its recordings `demos/demo%03d.dm3` (`0x463a3c`).

**Messages.** Each message the channel accepts goes to the recording as a
record after it is parsed (`0x405812`, `CL_WriteDemoMessage` `0x4044a0`). A
record is the message's length, then the bytes after the channel's header.
While the recording waits, nothing is written, and the client's moves ask for
a full frame: the delta frame is -1 while waiting (`0x4035f9`). The first frame
that is not a delta ends the wait (`CL_ParseFrame` `0x406af4`), and its
message is the first one written.

**Stopping.** `stoprecord` (`0x4044f0`) writes one -1 length and closes the
file. A disconnect stops a recording (`0x404b0d`). While recording, the
original draws `RECORDING %s: %ik` (`0x4076a0`).

**Downloads.** The original client's message parser has no download message:
its jump table (`0x407188`) sends opcode 6 to "Illegible server message",
the same as an unknown opcode. The original server answers `download` with
opcode 6 (`0x418a63`) and refuses any name containing `maps` (`0x4189ba`),
but no IHV client can read that answer. So no IHV peer can carry a download,
and downloads are not a gate for this profile.

## The fnTech3 client

`code/client/cl_q3ihv.cpp` follows each of these.

- **Starting.** `record` builds the first record from the session's decoded
  gamestate, with configstring changes applied, and only the baselines that
  carry a model. The shared writer then encodes it.
- **Messages.** A message is written after it is parsed, once a full frame has
  ended the wait. The moves already ask for a full frame while
  `clc.demowaiting` is set.
- **Stopping.** `stoprecord` and a disconnect write the single -1.
- **Without a name**, `record` takes the first free `demos/demoNNN.dm3`, as the
  original's menu does; the original's console command insists on a name.
- **The recording indicator** is the engine's own (invariant 11).

## Runs

All runs were on private desktops, windowed, with no input. The installation
was only read. The client and server were built from HEAD (`b8b04b91`) plus
this change.

| Recording | Made by | Records | fnTech3 plays it | The original plays it |
| --- | --- | --- | --- | --- |
| `fnrec.dm3` | fnTech3 client on fnTech3's server | 289: one gamestate, 288 frames over 14.35 s | to the end (`CL_NextDemo`) | to the end: `1662 frames, 14.3 seconds` |
| `origrec.dm3` | fnTech3 client on the original server | 289, 14.35 s | to the end | to the end: `5475 frames, 14.3 seconds` |
| `origcl.dm3` | the original client on fnTech3's server | 206, 10.2 s | to the end | (its own) |

Each recording decodes offline without failure. Each starts with a gamestate
of 12 configstrings and 23 baselines, all with models, and a first frame whose
delta is -1.

**Byte check.** The original client's own gamestate record re-encodes
byte-identically through the shared writer, and so do fnTech3's. The selection
plus the writer therefore produces the original's bytes for the same state.

The original client's captures during playback show `tim_dm1` with its HUD.
After its timedemo summary it stays at the demo's last frame, as it does with
its own demos, so those runs ended at the harness's time limit.

## Level changes

A recording continues through a level change in both engines: the new
gamestate is a message like any other.

- **fnTech3's recording** (`lvl.dm3`, made with `rcon map km_portal` during
  it) holds two gamestates and 156 frames, and fnTech3 plays it through both
  levels.
- **The original's recording** (`origlvl.dm3`) also holds both levels. Its
  record 162, right after the second gamestate, is empty: the original records
  the empty keepalive message the server sends a reconnecting client. fnTech3
  read only up to that record and stopped there as if the demo had ended. It now
  reads past an empty record, an empty message carrying nothing, and plays the
  recording through `km_portal` to its end (129 s).
- **The original client** stops at "Awaiting snapshot..." after a demo's second
  gamestate, in fnTech3's recording and in its own. It reads demo records only
  from its frame loop, which waits for the level to be active (inferred from
  `0x4059b0`; the stall is observed). Its reader would also end a demo at an
  empty record: a read of zero bytes counts no item, so it reports the file as
  truncated (`0x404886`). That point is not reached here, as the stall comes
  first.

`tests/q3_ihv_client_tests.py` checks the reader: records, the empty one, -1,
truncation, the size limit and a missing file.

## Open

A frame-by-frame comparison of playback in the two engines remains open. The
profile stays a Windows x86 developer path, and release builds still refuse
it.
