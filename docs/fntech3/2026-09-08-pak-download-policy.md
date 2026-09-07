# Profile-owned retail-pak download exclusions

The inherited `FS_idPak` classified `baseq3/pak00` as a protected retail pak
under both profiles. That name is QL's base archive, but Q3's pinned baseline
does not give it special status. Because both client missing-pak planning and
server download authorization call this function, the unconditional exception
prevented Q3 from downloading an otherwise ordinary add-on with that name.

The QL exception now requires the QL profile. Existing `pak0` through `pak8`
and Missionpack exclusions remain unchanged for both profiles. This is an
independently authored profile guard informed by inspection of
`E:/Repositories/FnQuake3` at `7d8c9629cdc7979f09697266470359101ffd9eda`;
no reference implementation text was incorporated.

## Executable regression

`tests/profile_pak_download_tests.py` executes production `FS_idPak`,
`FS_ComparePaks`, filename comparison, extension handling and traversal checks
with controlled search paths and file-existence results. Before the change,
the Q3 classification assertion failed. Afterward, Windows and Linux pass:

- Q3 add-on versus QL retail classification, including case and separators.
- Missing-pak download and diagnostic lists with omitted, PK3 and PAK suffixes.
- Existing checksum matches and same-name/different-checksum destinations.
- Numbered retail and Missionpack exclusions, traversal and small output buffers.

The shared Windows MSVC x86 client/dedicated build passes. The complete Meson
suite passes 110 tests with zero failures and one unavailable OpenAL-loopback
skip; its aggregate Python test includes 1,254 tests with seven skips. The
separate pinned-runtime loopback result remains documented in the
[audio checkpoint](2026-09-08-audio-restart.md).

## Runtime scope

Runtime records are in the [download evidence](evidence/2026-09-08-pak-download-policy.json).
Both probes use the rebuilt shared x86 binaries, isolated writable homes,
windowed GLx on an unswitched private desktop, disabled input capture and
disabled Steam integration. Screenshots come from the engine render target.
The Q3 server's temporary `pak00.pk3` contains `maps/q3dm6.bsp` from the local
legitimate retail installation. The archive and downloaded copy are deleted
with their temporary homes; no retail content is committed or published.

The first Q3 script exited normally during the transfer (the engine screenshot
showed 18%). This established request/authorization but was not a completed
download. The next probe completed transfer and pure admission, but its server
quit before the client's script finished; the watchdog terminated that client.
Neither probe is recorded as a clean end-to-end pass.

With corrected probe timing, Q3 completed the transfer and the downloaded
archive's SHA-256 matched the server fixture. The server recorded pure
admission followed by an active client; snapshots continued and the engine
screenshot showed world/HUD gameplay. Both processes exited normally. QL's
separate pure session also reached active state, rendered its native join UI
over the world and exited normally. QL's retail-pak download exclusion is
verified by the executable regression, not by attempting to transfer retail
QL content.

Remote-network fault cases, interrupted-download recovery and bidirectional
retail-peer coverage remain open. Existing unrelated image/font warnings
remain unresolved.
