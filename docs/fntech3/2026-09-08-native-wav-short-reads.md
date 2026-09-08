# Accumulate positive short reads for native WAV streams

Both music backends request whole interleaved frames. The native PCM stream
path nevertheless returned a short filesystem read directly, so a one-byte
read could be discarded by the caller's frame-count calculation and shift
later decoding. On big-endian hosts it could also split a scalar before byte
swapping. The stream path now shares bounded read accumulation between native
PCM and [converted sources](2026-09-08-wav-short-read-alignment.md).
Positive short reads fill the requested buffer before returning; EOF or an
error stops accumulation. Returned byte counts and buffer bounds remain exact.

The executable regression extends read limits of 1–9 bytes and multiple request
sizes to native PCM8/PCM16. It also checks simulated big-endian PCM16 output
with limits of 1–5 bytes. The predecessor fails the new native short-read case;
the updated implementation passes. Existing malformed-payload, sentinel,
nonpositive request and error checks remain green. This does not redefine the
codec's byte-oriented API or claim reconstruction across arbitrary odd-sized
PCM16 requests or a terminal read failure.

A separate corpus harness runs the actual header and streaming implementations
against all 631 Q3 and 538 QL installed WAV paths. It caps each filesystem read
at 509 bytes, reads decoded output in 32,768-byte requests, and compares the
complete result with independent Python WAV extraction. Every byte matches,
with the same aggregate decoded hashes as the previous corpus checkpoint.
Unlike the earlier whole-load comparison, this directly exercises the changed
stream path. It is a codec harness, not running-engine or hardware playback
evidence. Retail data remains local and unchanged.

Native Windows and SDL x86 builds pass. The complete native-Windows suite
passes 110 targets, zero failures and one skip. Five WAV/speaker tests pass
under Linux AddressSanitizer and UndefinedBehaviorSanitizer. Source identities,
commands, corpus results and log hashes are in the
[structured record](2026-09-08-native-wav-short-reads.json).

Normal presentation-related audio continuity and the broader compatibility
gates remain open. No new unrelated issue was found and no external code was
incorporated.
