# Recognize base pak metadata before selecting a profile

Automatic selection previously accepted any regular file named `baseq3/pak0.pk3`
or `baseq3/pak00.pk3`, including a one-byte placeholder. The early probe now
requires a bounded ZIP central directory with known retail entry metadata.
An unrecognized canonical pak stops automatic selection at that candidate's
priority and reports its evidence source, instead of silently choosing another
installation. An explicit `+set com_gameProfile q3|ql` still takes precedence.

The independently authored reader uses the central-directory and end-record
layouts in [PKWARE APPNOTE 6.3.10](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT),
sections 4.3.12 and 4.3.16. It reads at most 65,557 tail bytes plus an 8 MiB
directory, checks record and offset bounds, and rejects split archives,
encryption, unsupported compression methods and unsupported directory layouts.
It does not inflate entries or authenticate their contents. The filesystem
remains responsible for payload loading and validation.

## Observed identity metadata

These values were read from user-owned Steam installations on 2026-09-08.
CRC values are directory declarations, not newly calculated payload checksums.

| Profile | Entry | Uncompressed bytes | Directory CRC32 |
| --- | --- | ---: | --- |
| Q3 | `default.cfg` | 1737 | `9a18b2fd` |
| Q3 | `maps/q3dm6.bsp` | 4408212 | `9fef9a4b` |
| QL | `default.cfg` | 1580 | `c6c9f233` |
| QL | `maps/campgrounds.bsp` | 2924532 | `0ddc0f1d` |

The observed Q3 pak is 479,493,658 bytes with 3,539 entries; QL is
962,052,238 bytes with 9,285 entries. Each recognized archive must contain one
matching default config and one matching map anchor, with no opposing map
anchor. Synthetic tests contain only constructed metadata and dummy bytes;
no proprietary payloads are included. This intentionally demonstrates the
limit of the heuristic: forged matching metadata can pass it. Historical,
modified or repacked archives with other metadata require an explicit profile.

## Validation and limits

Both Windows x86 and Linux i686 dedicated binaries pass automatic and explicit
Q3/QL startup, legitimate module/map loading, loopback protocol 68/91 responses,
and normal shutdown. Each run uses an isolated writable home and read-only
installation inputs. Linux also rejects a one-byte pak with exit 1 and the
specific early identity diagnostic, before filesystem startup. The equivalent
Windows fatal-path probe remained alive until its 20-second timeout and was
terminated by the probe; it is not recorded as an unattended shutdown pass.
Source inspection confirms that Win32 `Sys_Error` deliberately waits in a
message loop for the user to close the error console. No input was injected.

Regression fixtures cover renamed archives, malformed and truncated directory
metadata, comments up to the format limit, invalid paths, candidate precedence,
and explicit override. The Steam discovery test now distinguishes locating a
candidate installation from establishing archive identity.

Windows passes 110 configured tests with zero failures and one unavailable
OpenAL skip; the aggregate ran 1,261 tests with seven skips. The initial Linux run exposed the same stale Steam test expectation
and its source aggregate exceeded the 300-second harness deadline. The corrected
suite passed 84 checks with zero failures and one unavailable retail-factory
fixture skip using a threefold timeout. Its aggregate passed 1,256 tests with
15 skips in 235 seconds (243 seconds measured by Meson). Builds are incremental development builds, not clean
release evidence. Linux uses the b5776c9 investigation checkout with the earlier
Unix/renderer corrections and current profile sources copied in; the evidence
records hashes rather than claiming that checkout equals its base commit.

The [evidence manifest](2026-09-08-pak-identity.json) records binaries, commands,
installation hashes, outcomes and source hashes. Full profile, payload,
interoperability and release gates remain open.
