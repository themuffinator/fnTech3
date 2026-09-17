# RTCW entity audio streams

SP and MP now use one bounded entity-stream controller and decoder service in
both audio backends. The checked cgame imports own both filenames, validate
32-bit scalar arguments and accept negative non-entity sentinels without
indexing entity storage. SP stop requests identify an entity, not a stream
slot. Music remains slot zero; SP has eleven entity slots and MP twenty-three.

The controller preserves channel matching, independent auto-channel starts,
filename truncation/default extensions, queued PCM on replacement, pending
stops, and SP stream fades. An SP entity stop immediately discards every
active stream buffer owned by that entity. Ordinary one-shot sounds mark a
matching entity stream for a draining stop. Clear, shutdown and registration
paths release stream files and playback resources.

The decoder checks sample rate, scalar width, channel count and whole-frame
reads. Each slot performs at most two reads per service, and at most one EOF
transition. Broken reads and unavailable output resources fail without leaving
half a stereo pair playing. Original mono/stereo PCM8 conventions are retained.

Legacy uses independent raw rings and saturating accumulation. OpenAL lazily
allocates two direct stereo sources per stream, each carrying one non-silent
channel. This permits independent left/right gains on buffered PCM without
clipping amplified source samples before mixing. Sources share the existing
buffer pool and cubic resampler and are released after draining. Resource
exhaustion can reject a stream; it never fabricates successful playback.

`streamingsound <file> [entity channel attenuation [loopfile]]` is available
in RTCW profiles, with `stopstreamingsound <entity>` in SP. Numeric console
arguments are checked. `s_info` reports active files and buffered entity lanes.

## Evidence

The implementation is independently authored from observations of the original
[SP sound system](https://github.com/id-Software/RTCW-SP/blob/master/src/client/snd_dma.c)
and [MP sound system](https://github.com/id-Software/RTCW-MP/blob/master/src/client/snd_dma.c).
The [iortcw SP backend](https://github.com/iortcw/iortcw/blob/master/SP/code/client/snd_dma.c)
contains entity-stream start/stop stubs and was not used as an implementation.
Unchanged original GPLv3 functions are compiled only in disposable comparison
fixtures with their notices. Production remains GPL-2.0-or-later.

Six new executable tests cover pool exhaustion, aliasing, negative sentinels,
native/QVM bounds, filename and integer limits, codec lifetime and malformed
reads, bounded empty loops, actual backend entrypoints, queued replacement,
immediate stops, asymmetric gains, fades, signed-clock wrap and console parsing.
Together with existing music, spatial, loop, one-shot and resampler tests,
26 focused tests pass on Windows x86 (22.532 seconds), Windows x64 (19.835
seconds), and Linux x86 with address/undefined-behavior sanitizers and leak
detection (114.115 seconds). Four complete Linux x86 audio translation units
also pass their checks.

`scripts/probe_rtcw_entity_streams.py` agrees exactly with both original sources
on Windows x86/x64 and sanitized Linux x86 SSE2. Per target, SP covers 35,826
starts, 53,214 state comparisons, 10,290 entity stops and 4,200 fade values;
MP covers 59,682 starts and 61,740 state comparisons. The fixture controls
codec reads and pending-stop servicing. It does not compare original decoding,
EOF loops, PCM output, device timing or live dialogue.

Both Win32 client/dedicated builds with GLX, VK and RTX pass. The default
executable SHA-256 is
`761f1897c37a51fbb9b4bf63d03c0342bfd4486d4a11fa322482bec232630edb`;
the experimental executable SHA-256 is
`a221d72d0b820ccf40b6f2c843922ab1f803ba2b63da0f0fc7089b42d67c1ea9`.
These contain recorded uncommitted work after `a80f240`.

The complete suite passes 111 Meson tests with two skips, including 1,455
Python tests with eleven skips in 524.802 seconds. Four muted installed-module
probes exercise SP and MP with both audio backends: two streams remain buffered
through channel replacement, then stop cleanly. All ten engine captures were
inspected, including six Q3/QL renderer checks. SP remains at its paused mission
briefing; MP reaches the deployment screen. These are not gameplay evidence.
Writable test homes were removed and the RTCW installation stayed unchanged.
Logs, capture identities and the source/build manifest are retained under
`.tmp/rtcw-manager/entity-stream-*`.

## Defined differences and open gates

The released SP source incorrectly starts background music when an entity loop
ends. fnTech3 keeps the requested loop on its entity lane. MP preserves the
original behavior of using the loop name as a repeat flag for the intro file.
SP fade admission uses bounded active slots instead of the original accumulated
start counter. The obsolete classic Mac MP source used eleven entity slots;
the current shared Windows/Linux contract uses twenty-three.

Modern backend resampling, mixer/device timing, soft muting and safe resource
exhaustion are not claims of original PCM parity. Source fades and allocation
agreement do not establish those behaviors. Measured output, scripted actor
dialogue, SP mouth amplitudes, device recovery under stream pressure and
save/load restoration remain open. Voice-amplitude import 37 is not admitted
by this checkpoint. SP remains paused at its briefing and default RTCW startup
remains gated; no full compatibility claim is made.

Unrelated source review found an existing big-endian concern in generic OpenAL
PCM16 stream conversion: WAV codec reads return native-endian samples, while
the generic converter reads little-endian bytes. The new entity path explicitly
encodes those bytes; generic music/raw conversion needs a separate audit and
big-endian runtime validation. The Q3/QL routing-cache reader bounds audit also
remains open.
