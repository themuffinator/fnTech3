# Quake III point releases, Q3Test and demos

Support for **all** historical Quake III releases is not complete. This ledger
includes the public Q3Test builds, Demo Test, the standalone demo, retail point
releases and their recordings. Recorded-demo decoding is a separate gate from
running that release's game modules or joining its servers.

The archive reference is Jason Brownlee's
[Quake3OfficialArchive](https://github.com/Jason2Brownlee/Quake3OfficialArchive/tree/960182ae296be8cbcd6b59db78b31aaa9d28ed9f),
pinned at `960182ae296be8cbcd6b59db78b31aaa9d28ed9f`. The
[2026-09-20 evidence](2026-09-20-q3-point-releases.json) records archive and
executable hashes, original-engine protocol observations, pak/module/demo
metadata, commands and probe outcomes. No archive, retail executable, module or
asset is redistributed.

## Release ledger

`Interop` below means short active sessions in both directions with the named
original Windows engine, with `sv_pure` both off and on, followed by recording
and demo interchange. It does not close sustained gameplay, rendering, audio,
download, Internet-service or cross-platform gates.

| Release | Server protocol | Recorded demos | Game/server evidence and remaining gate |
| --- | --- | --- | --- |
| IHV 4.01, February 12 1999 | 33 (original server reply) | Length-only `.dm3`; `sli` plays in the original engine; `berj` is incomplete; `candy.dm2` is Quake II | **OPEN:** IHV module, wire/demo and rendering contracts. BSP 43 map-load probes pass with retail Q3 QVMs; see the [IHV ledger](Q3_IHV.md). |
| Q3Test 1.02, 1.03 | 35 (binary constants; local original-server query unsuccessful) | No recording shipped in the inspected Windows server packages | **OPEN:** native module contracts, raw wire codec and BSP 44 |
| Q3Test 1.05 | 36 (binary constant and shipped recordings) | `q3test1.dm3`, `q3test2.dm3`: protocol 36 | **OPEN:** native module contracts, raw wire/demo codec and BSP 44 |
| Q3Test 1.06 | 38 (original server reply) | `q3testdemo1.dm3`, `q3testdemo2.dm3`: protocol 38 | **OPEN:** native module contracts, raw wire/demo codec and BSP 45 |
| Q3Test 1.07 | 39 (original server reply) | Ships the protocol-38 recordings from 1.06 | **OPEN:** native module contracts, protocol 39 and BSP 45; filenames do not identify the recording's protocol |
| Q3Test 1.08 | 40 (original server reply) | `q3demo1.dm3`, `q3demo2.dm3`: protocol 40 | **OPEN:** native module contracts, raw wire/demo codec and BSP 45 |
| Demo Test 1.09 | 43 (original server reply) | Two `.dm3` recordings, byte-identical to the 1.11 demo's recordings; playback with 1.32 Q3 modules passes | **OPEN:** standalone demo installation, old module ABI and live protocol 43 |
| Demo Test 1.10 Mac/Linux | Unobserved in this audit | Archive lists these packages as missing | **OPEN:** obtain legitimate fixtures; do not infer successful compatibility from adjacent releases |
| Standalone demo 1.11 | 43 (original server reply) | `demo001.dm3`, `demo002.dm3`: playback with 1.32 Q3 modules passes | **OPEN:** standalone demo installation, old module ABI and live protocol 43 |
| Retail 1.11 | 43 (reference family; original retail executable not supplied by this archive) | Existing retail `.dm3` playback evidence | **OPEN:** old module ABI, pak policy and live protocol 43 |
| Point Release Beta 1 / 1.14 | 43 (original server reply) | Raw 43 decoder available | **OPEN:** old module ABI, update pak variants and live protocol 43 |
| Point Release Beta 2 / 1.15b | 43 (original server reply) | Raw 43 decoder available | Same gates as 1.14 |
| 1.15c | 43 (original server reply) | Raw 43 decoder available | Same gates as 1.14 |
| 1.16h, 1.16i, 1.16j, 1.16m, 1.16n | 43 (each original executable queried) | Raw 43 decoder available | **OPEN:** old module ABI, pak/pure policy and live protocol 43 |
| 1.17 | 45 (original server reply) | A recording made by the original 1.17 engine completes playback with 1.32 Q3 modules | **OPEN:** release-specific module, live codec, pak/pure and interoperability proof |
| 1.25 / 1.25p, 1.25y | 46 (each original executable queried; archive's `Q3PointRelease_125.exe` identifies itself as 1.25p) | `demo000.dm3`, `demo001.dm3`; the corresponding recordings shipped under `.dm_48` in 1.27 complete playback | **OPEN:** old game/module and live protocol-46 contracts |
| 1.27g, 1.27h | 48 (each original executable queried) | `demo127.dm_48` is protocol 48; `demo000.dm_48` and `demo001.dm_48` in pak4 are actually protocol 46; all three complete playback | **OPEN:** live raw protocol 48 and release-specific game/module proof |
| 1.29f, 1.29g, 1.29h | 66 (each original executable queried) | `four.dm_66` completes playback; recordings made by both engines play in both engines | **PARTIAL — Interop:** all three original engines pass QVM startup, pure/non-pure admission and recording interchange; exact pak6 variants admitted |
| 1.30 | 66 | `.dm_66` recording/playback in both engines passes | **PARTIAL — Interop:** QVM gameplay startup and pure/non-pure admission pass |
| 1.31 | 67 | `.dm_67` recording/playback in both engines passes | **PARTIAL — Interop:** QVM gameplay startup and pure/non-pure admission pass |
| 1.32, 1.32b, 1.32c | 68 (1.32 and 1.32c executables queried; GPL 1.32b source contract) | `.dm_68`; existing shipped-demo and peer evidence | Existing [protocol-68 evidence](2026-09-07-q3-reference-interop.md); full gates remain open |
| ioquake3-compatible extensions | 71 | `.dm_71`, with the existing dm_68 recording fallback where applicable | Existing [protocol-71 evidence](2026-09-07-q3-protocol71.md); this is not an official id point release |

The supplied IHV build is audited separately in the [IHV ledger](Q3_IHV.md).
Team Arena is an additional
module/content set under Q3; the archive's Team Arena demo and platform-specific
patch variants still require their own fixtures and gates. A known protocol
number alone cannot establish those contracts.

Named field tables in the archived executables confirm separate Q3Test player
layouts: 1.05 has 35 scalar fields, 1.06/1.07 have 42, and 1.08 has 44, compared
with 45 in the 1.11 demo. These are table observations, not working decoders or
complete module layouts. The original 1.02/1.03/1.05 executables also failed a
fresh isolated startup retry with Windows exception `0xC0000005`; no live-server
success is inferred from their protocol constants.

## Implemented boundaries

- Live Q3 contracts are explicitly 66, 67, 68 and 71. Protocols 66/67 have
  separate wire identities so recordings retain their proper extensions.
  The QL, RTCW and Elite Force profiles do not gain these protocols.
- Original Q3 challenge replies omit the protocol. The client requests it from
  the same endpoint with an independent nonce, validates that nonce and the
  complete numeric reply, and only updates a connection still being established.
  A default protocol-68 client was tested joining original 1.30/1.31 servers,
  including pure servers with their older QVMs selected from a full 1.32 tree.
- Protocols 66/67 use `cp cgame ui @ references... mixedChecksum`. Protocol 68
  includes the server ID before those checksums. The verifier still checks both
  modules, duplicate/unloaded references, bounds and the mixed checksum.
- With explicit startup protocol 66, the filesystem requires pak0 through
  pak6; protocol 67 requires pak0 through pak7. Defaults 68/71 still require
  pak0 through pak8. Final release checksums remain mandatory except for the
  exact protocol-66 pak6 variants: 1.29f/g `1913835400`, 1.29h `1324612836`.
  The final 1.30 pak6 checksum is `3709064859`. No general checksum bypass was
  added. Older installations are not automatically reclassified by missing paks.
- Raw demo formats are exactly 43, 45, 46 and 48. Playback accepts `.dm3` and
  the matching `.dm_NN` names. It reads the opening gamestate's structured
  `CS_SERVERINFO` protocol key before stopping the current session. It permits
  the observed `.dm_48` / protocol-46 exception, rejects missing, unsupported
  or mismatched protocols, and never guesses 43 from a filename or a substring
  in a hostname. A later gamestate cannot switch the chosen decoder.
- Truncated raw entity/player deltas cannot publish partial state. Unknown
  protocol values cannot select a nearby legacy field table. Q3Test recordings
  currently return an explicit unsupported-format diagnostic; their rejection
  is a safety check, **not playback support**. Q3Test protocol 40 must also remain
  distinct from Elite Force SP's unrelated protocol 40.

## Reproduction

Build the normal Windows x86 client and server with the existing Meson preset.
The complete Meson suite passed 120 targets with two skips; its Python suite
passed 1,828 cases with 72 skips. The final historical-pak additions were also
rebuilt and checked with the focused contract test and original 1.29 peers.
The final demo compatibility-family assignment was rebuilt and both engines'
66/67 recordings were replayed again in fnTech3; all four completed, and the
focused demo, pure-proof and version-discovery tests passed.
For an older complete installation, select its protocol before filesystem
startup, for example:

```text
fntech3.exe +set com_gameProfile q3 +set protocol 66 +set fs_basepath <1.30-installation> +set r_fullscreen 0
fntech3.ded.exe +set com_gameProfile q3 +set protocol 67 +set fs_basepath <1.31-installation> +map q3dm1
```

The repeatable peer/recording probe is
[`scripts/probe_q3_point_releases.py`](../../scripts/probe_q3_point_releases.py):

```powershell
python scripts/probe_q3_point_releases.py `
  --client meson/build/win32/fntech3.exe `
  --server meson/build/win32/fntech3.ded.exe `
  --reference <original-1.30-quake3.exe> --installation <complete-1.30-tree> `
  --modern-installation <complete-1.32-tree> --protocol 66 `
  --output .tmp/q3-point-releases/my-1.30-probe
```

Use protocol 67 for 1.31. The output must be new and disjoint from the source
installations. The probe makes independent pak copies, uses fresh homes,
disables downloads and input capture, and does not switch the user's desktop.
It retains a JSON report and logs. Its commands are generated configs and
startup arguments, not injected keyboard or mouse events.

## Remaining implementation work

The pre-1.29 live codecs need complete client and server framing, delta writers,
command encodings, acknowledgement placement, packet scrambling/XOR and pure
rules. The current demo reader and its translation to modern Q3 cgame data are
not a substitute for those bidirectional contracts.

The older QVM and native module boundaries need separately frozen layouts,
trap/export mappings, renderer imports and filesystem policy. In particular,
Q3Test uses native modules and BSP 44/45, while Demo Test/1.11 uses older QVMs
and UI API 3. Simply accepting their protocol or UI version would expose the
wrong memory layouts. The GPL-2.0-or-later
[Lilium Arena Classic reference](https://github.com/clover-moe/lilium-arena-classic/tree/2fc883cbd244fa58046542a63518e51a7e01479f)
helps identify the protocol-43/old-QVM boundary; no implementation text from it
was incorporated in this checkpoint.

Full historical-release support remains an open requirement in the
[main compatibility ledger](COMPATIBILITY.md). Windows x86 observations cannot
be promoted to Mac/Linux release compatibility or normal-speed visual, audio
and gameplay parity.
