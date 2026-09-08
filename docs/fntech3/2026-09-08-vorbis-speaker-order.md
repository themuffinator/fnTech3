# Normalize Vorbis channel order at the codec boundary

Vorbis decoding returns channels in stream order, as documented by
[Xiph](https://xiph.org/vorbis/doc/vorbisfile/ov_read.html). The engine previously
passed that order to backends expecting its internal WAVE speaker order. A
synthetic six-channel center-only Ogg therefore played on the right under both
profiles, for both loaded sounds and streaming music.

The Ogg codec now permutes ordinary decoded frames into internal speaker-bit
order and publishes the corresponding layout mask for one through eight
channels. Mono, stereo and quad byte order is unchanged. Three- and five-channel
layouts can use the existing speaker-aware stereo fallback; larger standard
layouts reach backend-native playback or the existing fallback with correct
speaker positions. Layouts above eight channels retain unspecified metadata.
Module raw-audio contracts and backend gain policy are unchanged.

Existing filename-selected UHJ/B-format conventions for two through four
channels retain their encoded order and unmarked layout. These represent sound
field components rather than ordinary speakers. Regression cases preserve
case-insensitive delimited tags and avoid matching embedded non-tags. The
permutation copies scalar bytes, preserving requested endianness and unaligned
buffers. Production open/read/load functions are tested across all eight layouts,
multiple decoder packets, loaded and streamed output, and tagged exceptions.

The runtime fixture is independently encoded with libvorbis from a 997 Hz tone
in the Vorbis center slot and zeros elsewhere. It contains no retail content.
The six-channel baseline has left RMS zero and right RMS approximately 0.1783.
After correction, three- and six-channel fixtures produce balanced center output:

| Backend | Profiles | Loaded left/right RMS | Music left/right RMS |
| --- | --- | --- | --- |
| OpenAL file writer | Q3, QL | approximately 0.1261 / 0.1261 | approximately 0.1261 / 0.1261 |
| SDL legacy disk writer | Q3, QL | approximately 0.0623 / 0.0623 | approximately 0.1261 / 0.1261 |

Every measured channel peaks at 997 Hz and every final second is silent. The
different loaded-sound gain is reported without asserting backend gain parity.
All ten clients (two baseline, eight corrected) exit zero normally with unchanged
runtime stages. Runs use windowed input-disabled clients, private unswitched
desktops, isolated homes, file-only audio, disabled capture on SDL, and the
diagnostic front-buffer control. No input control, OS recording or retail
installation writes occur. Hardware surround and normal back-buffer continuity
remain open.

Both Windows x86 build variants pass. The final complete native-Windows suite
passes 110 targets, zero failures and one skip. Both Ogg tests pass under Linux
AddressSanitizer and UndefinedBehaviorSanitizer. The installed QL Ogg inventory
contains 533 mono and 26 stereo files, so it cannot establish surround routing;
Q3 has no installed Ogg files. Broader layouts have executable regression
evidence, while runtime routing is scoped to three and six channels. The
[structured evidence](2026-09-08-vorbis-speaker-order.json) pins source, fixture,
binary, command, output and validation identities.

No new unrelated issue was found. Channel-order facts were referenced from
Xiph documentation; the mapping and fixture encoder were independently written.
Full compatibility remains unproven.
