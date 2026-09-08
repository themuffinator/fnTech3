# Bound WAV extensible sample precision

The shared decoder now rejects WAVEFORMATEXTENSIBLE headers whose declared
valid precision exceeds their sample container. Previously a PCM16 file claiming
17 valid bits was accepted. Parsing still publishes metadata only on success.

Microsoft's [ksmedia.h format contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-waveformatextensible)
requires precision no greater than container width and places reduced-precision
PCM in the most significant bits. Tests cover oversized precision in 8-, 16-,
24- and 32-bit containers and exact signed 20-bit values in 24-bit containers.
Existing acceptance of zero or smaller precision is preserved; this does not
establish conformance for every such float/zero declaration. The original parser
fails the new rejection regression. No implementation text was copied.

Both running Windows x86 profiles reject a synthetic PCM16 track declaring 17
valid bits and then play a valid stereo intro/loop in the same process. File
output measures intro frequencies 997/1499 Hz followed by loop frequencies
2111/2801 Hz, with exact silence in the final second after stop. Both processes
exit zero normally; staged engine/provider files remain unchanged. Each run uses
an isolated home, a private unswitched desktop, windowed input-disabled settings
and a pinned OpenAL Soft Wave File Writer. No speakers, OS audio capture,
keyboard/mouse control or installation writes are used. Raw logs and captured
output, which can include retail startup audio, remain ignored.

Speaker-mask propagation remains unresolved. In particular the driver contract
allows zero direct-output masks and describes unequal mask-bit/channel counts;
rejecting every count mismatch would not implement that contract. This change
does not alter speaker routing, promise all extensible formats, or establish
hardware audio output. No new unrelated issue was found.

Windows x86 builds and passes 110 configured targets with zero failures and one
skip, including 1,301 Python tests with five skips. All three WAV regressions pass
under Linux AddressSanitizer and UndefinedBehaviorSanitizer. The
[structured evidence](2026-09-08-wav-extensible-precision.json) pins source, executable, input and log hashes,
commands and measured output.
