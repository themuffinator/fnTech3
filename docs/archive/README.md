# Upstream archive

These files come from the engines fnTech3 descends from. They are kept for
provenance and as background; they do not describe fnTech3's current
behaviour, supported games or build steps. For those, start at the
[documentation index](../README.md).

| File | Origin | Notes |
| --- | --- | --- |
| [`id-readme.txt`](id-readme.txt) | id Software's Quake III Arena GPL source release | The original source release notes. |
| [`README.Q3Test`](README.Q3Test) | id Software, Q3Test 1999 | Operating instructions for the 1999 test release. |
| [`README.Linux`](README.Linux) | id Software, Q3Test Linux 1999 | Historical Linux installation notes; not fnTech3 instructions. |
| [`LinuxSupport/`](LinuxSupport/INSTALL) | id Software, Quake III 1.32 Linux release | The 1.32 Linux installer notes, change list and UDP broadcast patch. |
| [`quake3e-README.md`](quake3e-README.md) | [Quake3e](https://github.com/ec-/Quake3e) | Quake3e's project README, describing its renderers and engine features; its links are relative to the Quake3e repository. |
| [`fnql.htm`](fnql.htm) | Quake3e's engine reference, as carried by FnQuake3 and FnQL | The inherited reference for many engine commands and cvars, such as `com_affinityMask`, `fs_locked`, `in_minimize`, `r_renderWidth` and conditional shader stages. Most still apply to fnTech3's shared engine, but the per-game guides and the [compatibility ledger](../fntech3/COMPATIBILITY.md) take precedence. |
| [`fnql-changes.txt`](fnql-changes.txt) | Quake3e, FnQuake3 and FnQL change logs | Dated engine changes before fnTech3. fnTech3's own changes are in the root [CHANGELOG.md](../../CHANGELOG.md). |
| [`fnquake3-FAQ.txt`](fnquake3-FAQ.txt) | Quake3e / FnQuake3 FAQ | Short answers about fullscreen modes, raw mouse input and keyboard charsets. |

The [FnQL baseline documents](../fnql/README.md) are kept separately because
several of them remain the reference for the Quake Live profile and for shared
subsystems.
