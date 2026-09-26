# One pair of binaries under four profiles

Commit `c71aa54` added `scripts/probe_release_profiles.py` on 2026-09-18 but
no record of a run. This record runs it at `3089df2` on the shared developer
build: one client and one dedicated-server executable, started once per
profile, with no other change between runs than the command line. It is
runtime evidence for the same-executable claim in the cross-profile rows of the
[ledger](COMPATIBILITY.md); it is not release-package evidence. The
[machine-readable record](2026-09-26-release-profiles.json) holds identities,
commands and every outcome.

## Method

[`scripts/probe_release_profiles.py`](../../scripts/probe_release_profiles.py)
starts `meson/build/win32-dev/fntech3.exe` four times, as `q3`, `ql`,
`rtcw-sp` and `rtcw-mp`, each against its own installation and a fresh
`fs_homepath`. Each client loads a stock map with `devmap` (`spdevmap` for RTCW
single player) and, from `activeAction`, waits, takes the engine's own
`screenshotPNG` and quits; RTCW multiplayer first closes its limbo menu with
the cgame's `CloseLimboMenu`. Then `fntech3.ded.exe` starts each multiplayer
profile on the same map, must answer a direct `getstatus` query, and quits from
a `wait 400` config. Every run is windowed on an unswitched private desktop
with sound, mouse and joystick input, Steam integration, the WebUI, bots and
pure mode off, on loopback only, with console commands only. A run passes when
it exits with status 0, writes no `ERROR:` line and, for a client, writes
exactly one screenshot.

## Results

| Profile | Installation | Map | Modules | Client | Server |
| --- | --- | --- | --- | --- | --- |
| `q3` | Steam Quake III Arena 1.32 | `q3dm1` | `vm/cgame`, `vm/qagame`, `vm/ui` QVMs | exit 0 in 8.5 s, one screenshot | exit 0 in 20.5 s, answered `getstatus` |
| `ql` | Steam Quake Live | `campgrounds` `ffa` | `cgamex86.dll`, `qagamex86.dll`, `uix86.dll` | exit 0 in 10.5 s, one screenshot | exit 0 in 11.0 s, answered `getstatus` |
| `rtcw-sp` | Steam RTCW | `escape1` | `cgamex86.dll`, `qagamex86.dll`, `uix86.dll` | exit 0 in 13.0 s, one screenshot | none (single player) |
| `rtcw-mp` | Steam RTCW | `mp_beach`, `g_gametype 5` | `cgame_mp_x86.dll`, `qagame_mp_x86.dll`, `ui_mp_x86.dll` | exit 0 in 9.5 s, one screenshot | exit 0 in 21.1 s, answered `getstatus` |

All seven runs passed, in 96 seconds. No run printed an engine error, and no
process had to be stopped.

The screenshots, inspected by eye and kept only as hashes, show what each
profile reaches without input: Quake III's `q3dm1` in play with its HUD;
Quake Live's warmup panel ("Free For All - Campgrounds", Join Match and
Spectate) over the level; RTCW single player's mission briefing for
"Ominous Rumors", which only a click dismisses; and RTCW multiplayer's
`mp_beach` from the spectator view, in warmup, with the limbo menu closed.

## What this does not show

- It runs the developer build, not a staged `.install/` or release package,
  and one Windows x86 host; no Linux or other platform run is included.
- No input is given and nothing is played: no movement, no combat, no mission
  past RTCW's briefing, and no client connects to the dedicated servers.
- The four profiles are run one after another, each with its own home. Shared
  homes and switching modes inside one process are outside it; the
  [profile-isolation record](2026-09-18-profile-isolation.md) covers a shared
  home.
- Enemy Territory, Elite Force and the other gated titles are not part of the
  probe.

## Identity

- Commit `3089df2e0758a287ee2142316cf3b3945e53cc71`.
- Client `meson/build/win32-dev/fntech3.exe`, SHA-256
  `b9531242c0d971d1528f1b80a9d59666d2fab48061e6b8d780cc071fe7cba527`; server
  `fntech3.ded.exe`, SHA-256
  `37e548bd0f30ffc07d98ac302740cb9dc65e7d459ae7a042c537f144fca430f4`. MSVC
  19.44 x86, debugoptimized, strict warnings, all three renderers as modules,
  the RTCW, Enemy Territory and Elite Force development gates on.
- Windows 11 x64 host. Runs between 20:45:28 and 20:47:03 UTC.
- Installations: `C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena`,
  `E:\SteamLibrary\steamapps\common\Quake Live` and
  `E:\SteamLibrary\steamapps\common\Return to Castle Wolfenstein`, read only.
