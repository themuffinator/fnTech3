# Measured WAV speaker-mask routing failure

At `d1d7478`, both profiles ignore a WAV extensible front-center assignment and
send that channel only to the right stereo output. This is an observed defect,
not merely missing format coverage; no routing correction is included here.

Two synthetic PCM16, 48 kHz, two-channel inputs differ only at byte 40, the low
byte of the speaker mask. Mask `0x3` declares front-left/front-right; `0x5`
declares front-left/front-center. Channel zero is silent. Channel one contains a
997 Hz intro followed by a repeating 2111 Hz loop, each at amplitude 0.25.
Microsoft's [speaker assignment contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-waveformatextensible)
places the declared channels in ascending mask-bit order. Thus the second
channel in the latter input represents center, not right. No particular center
downmix gain is assumed by this diagnosis.

All four actual-engine runs complete normally. In both profiles and for both
masks, measured steady tone windows have exactly zero left RMS and approximately
0.17677 right RMS. The final second after stop is silent. The
[structured evidence](2026-09-08-wav-speaker-mask-gap.json) pins input formulas,
hashes, runtime identities, commands, measurements and source hashes. Inputs
are verified identical except for the mask byte. Timing varies across runs;
whole captured files are not claimed identical.

Source inspection agrees with that observation: the WAV parser discards the
mask; `snd_info_t` has no field to carry it; and background playback selects
layout from channel count. Correcting this requires preserving explicit speaker
assignments through decoding and routing, including layout changes between
intro and loop. Unspecified masks and legacy/raw module channel conventions
need to retain their established behavior. Zero masks and unequal mask-bit and
channel counts cannot simply be rejected as universally invalid: Microsoft's
driver contract describes these cases. Static sounds, other layouts, backend
fallback and legacy DMA require coverage alongside the measured streaming path.

The probe uses private unswitched desktops, windowed input-disabled clients,
isolated writable homes and a pinned OpenAL Soft Wave File Writer. It does not
use speakers, input control or OS recording. Runtime files remain unchanged;
retail installations remain read-only. Raw captures/logs, potentially including
retail startup UI audio, remain ignored. This is stereo file-output evidence,
not a hardware surround or general audio-compatibility claim. No external
implementation was incorporated and no unrelated issue was discovered.
