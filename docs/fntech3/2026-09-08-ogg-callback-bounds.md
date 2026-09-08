# Correct Ogg filesystem callback bounds and positions

The Ogg read callback now checks element-count multiplication before converting
to the filesystem's integer byte count, bounds reads to the compressed file,
returns zero on an I/O error without decrementing the position, and reports only
complete elements. A trailing partial element is consumed but not rounded up.
Zero-length requests remain harmless.

The seek callback validates 64-bit offsets before addition or narrowing. It
rejects targets outside the compressed file and converts accepted requests to
an absolute seek. Success requires the actual filesystem cursor to match the
target; positive packed-file seek return values are accepted. Current-relative
seeks use the real file position. Stream initialization no longer resets its
tracked position after the decoder has already read and sought within the file.

Executable regressions cover overflow, partial elements, negative reads,
end-of-file bounds, all seek origins, extreme signed offsets, failed/mismatched
seeks, and zero/positive seek success values. The predecessor fails the partial
element case. Both Ogg callback and payload tests pass under Linux
AddressSanitizer and UndefinedBehaviorSanitizer. The native Windows and SDL x86
builds pass; the complete native-Windows suite passes 110 targets, zero failures
and one skip.

A Linux harness links the production callbacks to the installed Vorbis decoder
and compares each of 559 retail QL Ogg paths against the same decoder using its
standard file I/O. Every decoded byte matches. The callback fixture limits reads
to 509 bytes and returns a positive value on successful seeks. This is an I/O
path comparison using one decoder, not independent decoder conformance. Q3 has
no installed Ogg corpus. Inputs remain local; the structured summary retains
inventory/output hashes rather than proprietary content.

Two Windows runtime probes then exercise one legitimate Ogg fixture inside a
compressed private pak as both a loaded sound and streaming music. Q3 and QL
use the same unchanged staged client. Both exit zero normally, produce nonzero
post-startup file output, and finish with a silent final second and no codec
errors. No loose copy shadows the pak fixture. Probes are windowed and
input-disabled on private unswitched desktops with isolated homes and OpenAL
Soft file output. The diagnostic front-buffer control is used; normal
presentation and hardware playback are not established. No input control,
OS recording or retail installation writes occur.

The [structured record](2026-09-08-ogg-callback-bounds.json) pins source and
binary identities, commands, corpus method, runtime measurements and validation
logs. Broader malformed-container and audio continuity gates remain open. No
new unrelated issue was found and no external implementation was incorporated.
