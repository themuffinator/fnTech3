# Preserve converted WAV samples across positive short reads

Converted WAV streaming previously decoded each filesystem read independently.
A positive read ending inside a 24-bit PCM or float32 scalar discarded the
partial scalar, so the next read began at the wrong byte. A one-byte read could
also return zero decoded bytes while more payload remained. The bounded source
buffer now accumulates positive short reads before conversion. EOF or a read
error still stops accumulation; only complete available scalars are decoded.
The 12,288-byte scratch bound and decoded-byte accounting are unchanged.

The executable WAV payload regression now varies filesystem read limits from
one through nine bytes and decoded requests of 2, 6, 38 and 128 bytes across
converted PCM8, PCM24 and float32 fixtures and multiple channel counts. Every
partition must reconstruct the whole-load output exactly and preserve output
sentinels. The new case fails on the predecessor and passes with this correction.
Existing exhaustive payload truncation, negative read, nonpositive request and
simulated endian cases still pass. Native PCM16 partial-byte reconstruction and
recovery after a terminal filesystem error are outside this correction's scope.

The native Windows and SDL x86 builds pass. The complete native-Windows suite
passes 110 targets, zero failures and one skip. Five WAV/speaker tests pass on
Linux under AddressSanitizer and UndefinedBehaviorSanitizer. Independent retail
payload comparison retains identical decoded hashes for 631 Q3 and 538 QL WAV
paths; this whole-load corpus is non-regression evidence, while synthetic short
reads exercise the changed stream path. See the
[structured record](2026-09-08-wav-short-read-alignment.json) for source identity,
commands, log hashes and corpus summaries.

No game or hardware audio probe was required for the injected filesystem-read
condition. Retail inputs remain local and unchanged. Audio continuity under
normal back-buffer presentation and other compatibility gates remain open.
No new unrelated issue was found and no external implementation was incorporated.
