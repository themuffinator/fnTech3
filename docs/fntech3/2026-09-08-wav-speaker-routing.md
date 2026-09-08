# Preserve WAV speaker assignments through playback

The [measured center-to-right defect](2026-09-08-wav-speaker-mask-gap.md) is corrected.
WAV decoding now retains the extensible speaker mask in engine-private
`snd_info_t` metadata. Ogg explicitly initializes this field to zero. This changes
no game-module trap, raw-audio signature, packet or serialized game structure.

OpenAL preserves its established native layouts when the explicit mask matches
that channel order. Other explicit layouts use speaker-aware stereo mixing.
The legacy backend uses the same mixer for layouts it cannot play directly.
Both streamed music and loaded sound effects consume the metadata. An explicit
speaker assignment takes precedence over filename hints for encoded sound fields;
zero/unspecified masks keep existing behavior and raw module samples do not enter
this conversion path. Native surround rendering is not newly certified here.

Mask bit order follows Microsoft's
[WAVEFORMATEXTENSIBLE contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-waveformatextensible).
Mixing gains are engine policy: front speakers retain unity gain, center uses
0.7071 to each stereo output, LFE uses 0.25, lateral/rear speakers use 0.5, and
rear/height center positions use 0.35 to each side. Front-of-center uses 0.7071
on its side. Reserved positions and channels without an assigned bit contribute
no output; excess high mask bits are ignored. The mixer clips to PCM16 bounds.
The source declares the complete mapping; these gains are not attributed to
Microsoft or claimed as a universal downmix standard. No external implementation
text was incorporated.

Both music backends now request complete interleaved frames. Their old fixed-size
reads could consume part of the next multichannel frame and then discard those
samples. For example, a 32,768-byte buffer holds 32,760 complete bytes of six-channel
PCM16. Bounds/partition tests cover this, all 18 named speaker bits, reserved bits,
mask/channel count mismatches, unsigned mono PCM8, clipping and allocation-size
limits. Original WAV payload, truncation and endian fixtures remain enabled.

Measured Windows x86 file-output probes under both profiles cover front-right
controls, front-center intro/loop streams, loaded center sound effects, a center
intro changing to a right-only loop, and six- and three-channel center sources.
Center windows have equal left/right RMS (about 0.12497 for streamed sources);
right controls keep zero left RMS. The final second after stop is silent.
All processes exit zero normally and runtime stages remain unchanged.

The first ten recorded runs use the candidate before a final source-channel
admission correction. Final review found that a native-format channel-count guard
could still reject explicit three-channel sources before stereo conversion. The
last two runs use the final binary with that guard corrected. Binary hashes and
this distinction are retained in the structured record; earlier artifacts are
not relabelled as the final binary. Transition analysis excludes the single mixed
intro/loop boundary region before checking steady right-only output.

Clients remain windowed and input-disabled on private unswitched desktops, with
isolated writable homes and a pinned OpenAL Soft Wave File Writer. No speakers,
OS audio capture, input control or retail installation writes are used. Raw logs
and output containing possible retail startup UI audio remain ignored. Hardware
surround, legacy DMA output, broader speaker/mask combinations, and performance
remain unverified. No unrelated issue was discovered.

The final Windows x86 build passes all 110 configured targets with zero failures
and one skip, including 1,302 Python tests with five skips. Four WAV/speaker
fixtures pass Linux ASan/UBSan. Exact decoded output remains unchanged for all
631 Q3 and 538 QL installed WAV paths (the Q3 set includes 18 add-on paths).
The initial float-gain assertion failure and corrected quantization tolerance
are retained in the [structured evidence](2026-09-08-wav-speaker-routing.json), alongside commands,
source phases, binary/input identities, measurements and log hashes.
