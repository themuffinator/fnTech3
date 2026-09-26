# Elite Force Holomatch protocol 26

fnTech3's `stvef-mp` profile now speaks both retail protocol 24 and
Lilium Voyager/ioEF protocol 26. The game profile remains development-gated.
This record establishes the Windows x86 boundary; it does not establish full
Elite Force compatibility or close the separate single-player renderer gate.

## Contract and provenance

The implementation is independently written against the GPL-2.0-or-later
[Lilium Voyager reference](https://github.com/zturtleman/lilium-voyager/tree/0f7dcd89d142c8b635ce2756a80cb62411731848).
That checkout's identity and licence were checked before use. No reference
implementation text or proprietary game content is incorporated.

Protocol 26 has its own transport and message-layout entries. It uses a client
qport, a sequence checksum and Huffman-coded messages. It does not use retail's
packet scramble, raw-bit alignment, or Q3's reliable-command XOR. Connect
requests remain plain text, as both of Lilium's Elite Force modes require.
Message bodies use the same frozen 50-field entity and 48-field player records
as Holomatch, with different framing:

- Entities carry a last-changed field count, per-field flags and zero shortcuts.
- Players carry a last-changed field count and a flag around the four arrays.
- User commands remain **unkeyed**, with eight-bit buttons. Both Lilium
  `cl_input.c` and `sv_client.c` select the unkeyed functions even on protocol
  26. The reader normalizes movement bytes of -128 to -127; retail protocol
  24 retains its observed -128 behavior.
- Messages use EOF opcodes and leading reliable acknowledgements; pure proofs
  include the server ID. These already follow the selected framing policy.
- Downloads use a 48-block window of 1,024-byte blocks. Retail keeps eight
  2,048-byte blocks. Both have 16,384-byte messages and 64 reliable commands.

The client selects 26 when the challenge reply advertises it and proves the
client nonce. A bare retail reply selects 24. Malformed integers, overflowing
values, mismatched echoes and unsupported protocols are refused. Protocol 26
connect replies must echo the challenge. The server accepts either exact
Holomatch contract, independently of its advertised preference; its default
advertisement stays 24, with `+set protocol 26` selecting the modern preference.

Retail demos retain `.efdemo`. Modern demos record as `.dm_26` and play only
under `stvef-mp`. Extensionless lookup preserves the retail search first, then
tries `.dm_26`. Other profiles refuse `.dm_26` instead of reading it as Q3.

## Automated evidence

`tests/fixtures/stvef_ioef_reference.c` compiles against the pinned reference's
`code/qcommon/msg.c` and `huffman.c` with `ELITEFORCE` defined. Its shared
synthetic input exercises every entity and player field, all four player
arrays, zero transitions, removals, unchanged records, both timestamp forms,
movement normalization and high-byte strings. Its nine resulting byte vectors
are checked in as `stvef_ioef_vectors.inc`; no installed assets are needed.

The production message-layer tests compare encoded bytes and decode the
reference vectors at all eight bit offsets, with every byte truncation, both
separate and in-place destinations, and guarded exhausted output buffers.
They also reject excessive entity/player field counts without publishing
partial state. Existing protocol-24, SP, Q3, QL, RTCW and ET codec tests run in
the same executable. Handshake tests cover nonce/protocol mismatches and signed
integer boundaries; executable demo tests cover extension choice, recording,
playback lookup and rejection under other profiles.

The final Windows x86 build succeeds for the client, dedicated server and
all three renderers. The full Meson suite passes 120 targets with two skips;
its recursive Python target passes 1,832 tests with 72 skips. The QL demo
source assertion was updated to recognize the explicit Holomatch exclusion
from Q3's compatibility-protocol fallback.

## Live setup and results

The Windows x86 developer build uses all three renderer backends and all
development profile options. The live Holomatch probes use the GLx backend,
the installed retail QVMs and assets, Lilium Voyager 1.40's x86 client and its
x86-64 dedicated server. The reference engine's version is recorded separately
from the newer source checkout used to establish the contract.

Every process uses an isolated writable home. Clients are windowed at 640x480
with sound, mouse and joystick input disabled. Servers listen on localhost,
announce to no master and run two bots. A localhost UDP relay observes the
negotiated protocol and validates every protocol-26 sequence checksum.
Screenshots are produced by each engine's registered `screenshotJPEG` command;
no operating-system screen capture or user input injection is used.

Both live directions reach gameplay with pure checks enabled and disabled.
fnTech3 also connects to itself using protocol 26, and its protocol-24 pure
session still produces a retail `.efdemo`. The initial reverse-direction probe
exposed the unused keyed-function trap in Lilium's source; the codec and golden
generator were corrected to follow the actual call sites before the successful
sessions were repeated. Failed exploratory runs are not counted as evidence.

Downloads in both directions, with pure enabled and disabled, transfer a
672,850-byte test pak byte-for-byte and enter its map. The pak contains copies
of the installed `hm_noon` BSP and AAS under a temporary map name and exists
only in isolated test homes. Lilium's protocol-24 mode also joins a fnTech3
server advertising 26, exercising the per-client contract selection.

Two protocol-26 demos recorded during these sessions play to completion in
both engines: the fnTech3 recording gives 191 frames in each, and the Lilium
recording gives 96. The protocol-24 regression recording gives 190 frames in
each. Engine captures show the gameplay scene during each playback.

A later Lilium dedicated startup exited with signal 11 while initializing
its bot data, before any client connected. This is recorded as a reference
runtime issue, not a fnTech3 network failure. The final fnTech3 executable
completed an additional protocol-26 pure session against its own server.

Detailed final build, suite, demo and download results are recorded in the
[evidence manifest](2026-09-20-stvef-ioef.json). Local logs, captures,
isolated profiles and probe helpers are under `.tmp/stvef-gaps/ioef-*`; installed
content and generated demo payloads are not committed.
