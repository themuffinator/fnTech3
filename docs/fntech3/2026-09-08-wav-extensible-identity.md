# WAV extensible format identity

The WAV reader now validates the complete PCM/IEEE-float subformat GUID and
requires the declared extension size to be at least 22 bytes and fit inside the
format chunk. Previously it inspected only the first two GUID bytes and ignored
the extension-size declaration, allowing an unrelated vendor GUID beginning
with the PCM or float tag to be decoded as that format.

[Microsoft's WAVEFORMATEXTENSIBLE description](https://learn.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-waveformatextensible)
establishes the extension-size minimum and full GUID identity. Installed Windows
SDK 10.0.26100.0 `shared/ksmedia.h` supplies the PCM and IEEE-float GUID values;
the [record](2026-09-08-wav-extensible-identity.json) pins both. These are format
facts used by an independent implementation; no external implementation text
was incorporated. No Windows SDK dependency is added to the portable codec.

Executable parser fixtures now corrupt each of the fourteen GUID bytes after
the format tag, reject unsupported tags and inconsistent extension lengths,
and preserve supported PCM/float plus bounded longer extensions. The prior
parser fails the new cases. Both focused WAV tests pass on Windows and with
Linux address/undefined-behavior sanitizers. A separate complete parser/load
harness checks eight synthetic extensible files: PCM8/16/24 and float32, mono
and stereo. Output matches independently calculated sample bytes, including
signed PCM endpoints and float clipping/non-finite handling.

All 631 installed Q3 and 538 installed QL WAV paths still match Python's WAV
reader byte-for-byte and retain the previous decoded corpus hashes. As clarified
in the [container checkpoint](2026-09-08-wav-container-bounds.md), the Q3 set
includes 18 add-on paths. These installed files are ordinary PCM; they establish
non-regression, while synthetic inputs exercise the corrected extensible path.
The private harness uses production parsing/loading with memory-backed file and
allocator adapters. It is not audible engine/backend evidence.

The Windows x86 client rebuilt successfully. The complete Meson suite passed
110 checks with one skip and no failures; its Python target passed 1,298 tests
with five skips. Documentation packaging and link checks also pass. This is an
incremental build. Valid-bit precision, channel-mask/multichannel semantics,
streaming CRC and audible/backend behavior remain open; the audio gate stays
partial. Review of current normal music callers did not establish a separate
small-buffer stream-alignment defect. No new unrelated defect was established;
existing optional-service and UI limitations remain open.
