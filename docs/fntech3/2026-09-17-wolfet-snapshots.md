# Enemy Territory snapshot visibility and development workflow

## Scope and provenance

This slice follows `602f3e7b5ed1d5b2eaa20968ddc9929fbcec5eaf`. It adds ET's
per-client snapshot callback and single/multiple visibility proxies to the
experimental dedicated server. It also adds six stock-map VS Code launches,
enables ET in the existing all-games build tasks, and adds the ET emblem to
the repository's GitHub social-banner asset.

Compatibility facts were checked against the original
[ET snapshot selection](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_snapshot.c)
and [game callback](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/game/g_missile.c).
The reference's GPL-3.0-or-later license and additional terms were checked;
its implementation text is not incorporated into this GPL-2.0-or-later project.
The implementation extends fnTech3's existing snapshot storage and checked
native-memory access. See [credits](../../CREDITS.md) and
[notices](../../THIRD_PARTY_NOTICES.md).

The ET client remains gated. This is neither a playable fnTech3 ET client nor
full multiplayer compatibility. Callback and proxy behavior has Source/Unit
evidence; the runtime observations below are narrower connection regressions.

## Implemented contracts

- Read `r.snapshotCallback` through the selected ET shared-entity layout,
  validating the entity generation, record bounds and client index. A disabled
  flag avoids the module call; changed flags are read afresh. Invalidated
  bindings and missing game modules fail explicitly.
- Dispatch `GAME_SNAPSHOT_CALLBACK` through ET export ordinal 10, with entity
  number followed by the snapshot viewpoint's client number. The module return
  value determines whether that entity enters the snapshot. The original game
  uses this callback for landmine visibility. Other profiles reject the export.
- Mark an entity visited before calling the callback, so a rejected entity
  cannot be retried through another proxy or portal in the same client snapshot.
  Full snapshots also stop calling the module. ET keeps its 512-entity limit;
  the other four profiles keep 256.
- A visible single proxy selects its linked `otherEntityNum` target without
  requiring the target's own PVS. Original ET permits this target even when it
  has `NOCLIENT` or a different single-client restriction. ET common storage
  therefore retains linked `NOCLIENT` records, while normal per-client selection
  still rejects them. An invalid target number is an error.
- A multiple proxy selects linked entities whose `otherEntityNum` identifies
  the proxy, excluding the proxy itself and `NOCLIENT` targets. Targets enter
  through the same duplicate/callback checks and the final sort restores entity
  order. Broadcast handling keeps its existing precedence over proxy handling.

The new callback/proxy logic is ET-only. It does not activate RTCW proxy behavior
or expose ET native records to another profile.

## Automated evidence

Executable regressions compile the actual engine functions with synthetic
records and controlled collision/module responses. They cover callback argument
order and ordinal/arity translation, acceptance/rejection, flag changes, stale
bindings, missing modules, bounds, destination sentinels, duplicate proxies,
per-client reset, broadcast filtering, linked/unlinked targets, the single-proxy
`NOCLIENT` exception, multiple-proxy exclusions and all five profile limits.
The actual common-storage builder is also exercised under all five profiles;
only ET retains hidden proxy targets, and normal selection never leaks them.
Malformed snapshot indices and missing storage are rejected before module calls.

The complete Windows x86 Meson run reports 112 passes, two expected skips and
one failure in the newly added native test fixture: it incorrectly reused an
ET-sized record for Q3, whose shared prefix is larger. The boundary correctly
rejected that binding. Replacing it with genuine Q3 fixture records and rebuilding
passes the affected Meson check (one pass, zero failures). Thus all 113 applicable
Meson checks have passing results after the fixture correction.

The recursive Python suite passes 1,548 tests with 62 skips in 492.208 seconds.
The expanded snapshot regressions subsequently pass all three executable tests
with the Windows x86 compiler environment. No runtime source changed after the
complete build. The two top-level skips are the unavailable OpenAL loopback
extension and opt-in QL retail factory probe; neither concerns the ET changes.

## Installed observations

The tested shared Windows x86 dedicated executable has SHA-256
`c04579d1b6434fee604da7cdf06caf56ded3e529e0537472b48519d171d347e5`.
It was built from the parent commit above plus the changes recorded here, using
MSVC `RelWithDebInfo` with both RTCW and ET development options. The shared
client and GLx/Vulkan/RTX raster modules also build.

The ET installation is Steam application `1873030`, build `7997832`, at
`E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory`. Its retail client
SHA-256 is `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e`;
the native game DLL SHA-256 is
`160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b`.

The retail client reaches `CS_ACTIVE`, joins Axis and Allies, and disconnects
normally on non-pure Gold Rush. Both processes exit with status 0 in 55.719
seconds. Installation file sizes and modification timestamps remain unchanged.
The probe uses isolated writable homes, unswitched private desktops, windowed
mode and disabled mouse/joystick input. Team changes are ordinary console
commands; no device input or screen capture is used. This session does not
deploy landmines or establish live callback/visibility correctness.

The same dedicated binary also passes Q3 and QL pure startup with both automatic
and explicit profile selection. These are startup regressions, not new Q3/QL
gameplay or interoperability claims.
All six ET stock maps also repeat explicit-profile startup successfully, answer
protocol-84 loopback queries and exit normally after 100 server frames.

Reproduction:

```powershell
.\.vscode\build-release.ps1 -Configuration RelWithDebInfo `
  -BuildDir meson\build\win32-dev -Target both -Renderer all `
  -RTCWDevelopment -WolfETDevelopment -RunTests
python scripts/probe_wolfet_retail_client.py `
  --server meson/build/win32-dev/fntech3.ded.exe `
  --installation "<your legitimate ET installation>" `
  --output .tmp/wolfet-next/interop-new --team-check
```

Local evidence is under `.tmp/wolfet-next/`: `verification.log`, `testlog.txt`
(inherited environment omitted), `fixture-retest.log`, `snapshot-regressions.log`,
`interop-teams/result.json`, map-specific `maps/*/result.json` and
`nonreg/manifest.json`. Reports include executable/module identities,
commands and process outcomes. These logs, generated client identifiers and
retail inputs are not committed.
Automatic approval review rejected cleanup of the disposable probe homes and
scratch directories with `blocked by policy`; those files remain ignored under
the same evidence directory. Console logs were also retained outside the homes.

## Workflow and branding

The VS Code **Wolfenstein: Enemy Territory** group offers dedicated launches for
`battery`, `fueldump`, `goldrush`, `oasis`, `radar` and `railgun`. They prompt for
the installation, use a separate writable home per map, bind to loopback and
disable pure mode, bots and PunkBuster. They run the existing development binary
without an implicit build. The all-games setup/build tasks enable
`-WolfETDevelopment`; ordinary release defaults stay gated.

The [social-banner asset](../../assets/logos/fntech3-social-banner.jpg) now has
four orange game emblems. The exact built-in image-generation prompt and export
settings are in [the prompt record](../../assets/logos/PROMPTS.md), with visual
reference attribution in [the asset README](../../assets/logos/README.md).
Updating the repository asset does not upload it to GitHub's social-preview
setting.

## Remaining gates

Live landmine/proxy observations, ET origin-only brush-model visibility,
self-portal/exclusive viewpoint flags and leaning need further evidence or
implementation. Movement, combat, objectives, restart/rotation, remaining game
imports, pure admission, downloads and demo playback also remain open. UI/cgame,
MDM/MDX rendering, audio/input and reverse interoperability keep fnTech3's ET
client gated. The [ET ledger](COMPATIBILITY.md#wolfet-gate-matrix) remains partial.

No new unrelated product issue was discovered in this slice. Existing retail
Gold Rush script warnings remain as documented in the initial ET record.
