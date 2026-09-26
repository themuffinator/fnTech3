# Elite Force HUD aspect rules and audio zones

Elite Force now has an audio zone sidecar for every official map and a HUD
aspect rule file for each of its two profiles. This follows baseline `b9e167e`.

## What was missing

`pkg/games/` carried audio zones for Quake III, Quake Live, RTCW and Enemy
Territory, and HUD aspect rules for Quake III and RTCW. Elite Force had
neither, so its maps fell back to the generic trace-based OpenAL heuristics and
its HUD stretched with the frame on a non-4:3 display.

The HUD half was not a missing data file. The system is whitelisted by title in
two places: `CL_HudInit` decides whether the client corrects anything at all,
and `FS_RootArchiveAllowsFile` decides which files the engine's own package may
serve. Elite Force reached neither, so shipping a rules file alone would have
changed nothing — the first version of these files loaded as `HUD: no usable
script in fntech3-hud.json` until the second gate was found. Both predicates now
include Elite Force. Quake Live stays out of both, as it owns its own HUD
projection.

Nothing else needed to move. Elite Force's cgame draws already reach the HUD
path: its single-player module shares the engine's `CG_R_DRAWSTRETCHPIC`
handler and routes `CG_R_DRAWROTATEPIC` through `CL_HudAdjustPic`, and Holomatch
uses the shared trap directly. The menus draw without correction, which is what
every other title does, so the single-player UI adapter is unchanged.

## Audio zones

The zone compiler gained `stvef-sp` and `stvef-mp`. Elite Force is a Quake III
fork, so the profile selects the Quake material interpretation and pins IBSP 46
rather than Quake Live's 47.

No Elite Force material table was invented, because the maps say one is not
needed. Across all 99 official maps and their 10,676 shader records, Elite
Force sets only Quake III's flag bits with Quake III's meanings: `SURF_SKY` on
sky, `SURF_METALSTEPS` on the Borg, Dreadnought and Voyager metal that dominates
the game, and `SURF_FLESH` never at all. The one divergence, bit `0x40000`, marks
Borg force fields where Quake III means dust, and the compiler reads that bit for
neither. A `--material-map` remains available for later art-directed tuning.

`scripts/generate_stock_stvef_audio_zones.py` reads only `pak0.pk3` through
`pak3.pk3` and refuses to run if the inventory changes. The single-player and
Holomatch split is not taken from name prefixes: the shipped `scripts/arenas.txt`
and `scripts/xpack.arena` declare 33 Holomatch maps, which leaves the 66
single-player and tour maps the project already tracks, and the generator fails
if the metadata and the inventory ever disagree. Ten single-player maps live in
`maps/tour/`, so sidecars keep that directory.

All 99 maps generated and audited, none failed:

| Profile | Maps | Zones | Average | Warned |
| --- | ---: | ---: | ---: | ---: |
| `stvef-sp` | 66 | 20,331 | 308 | 32 |
| `stvef-mp` | 33 | 13,082 | 396 | 11 |

Grades are 32 excellent, 47 good and 20 fair, with overall confidence from
0.641 to 0.998 and a mean of 0.848. Zone materials come out 48% metal and 40%
neutral, with stone, soft, sky and liquid behind them, which is what a game set
mostly inside starships should look like. Per-map zone counts sit in the same
band as the already-shipped generated sets — RTCW, Enemy Territory and Quake
Live run 104 to 512 per map and frequently reach the cap — so this set is
consistent with what the project already ships rather than better or worse.

43 maps carry audit warnings, 40 of them that most lookups resolve to map-scale
zones. These are recorded per map in the manifest rather than suppressed.

## HUD aspect rules

The rules are authored from draws, not guessed. Owned single-player and
Holomatch sessions ran with `cl_hudDump 1`, and the engine wrote 37 and 97
deduplicated draw groups; those groups are tracked as
`tests/fixtures/stvef_hud_draws.json` and the shipped rules were derived from
their centre points.

Single player gets five rules: the crosshair centred, the health, armour and
ammunition caps, bar and numbers pinned bottom-left, and the upper-left text
pinned top-left. Holomatch gets thirteen, adding its bottom-right ammunition
cluster, the bottom-centre weapon selector and status text, the lagometer, the
right-hand pickup icons and the centreprint.

`tests/stvef_presentation_assets_tests.py` replays the recorded groups through
the engine's own matching order — the draw's centre point against each rule in
turn, first match wins — and asserts that each cluster resolves to its intended
anchor, that no rule is unreachable behind an earlier one, and that no rule
claims the loading emblem, which is one composition that pinning to different
edges would pull apart. Both files load in real sessions with no HUD warning.

## Validation

The full Windows suite passes 120 targets with 2 skips; its Python aggregate
runs 1,878 tests with 72 skips in 732.649 seconds. Three existing tests needed
updating with this change and are corrected in the same commit: the compiled HUD
probe needed a `Com_GameIsSTVEF` stub to link, the launch-and-transition source
check pinned the old enablement line, and the release packaging count rose from
447 to 548 required destinations — 99 sidecars and two rule files.

The packaging gate refused the new assets until `stvef-sp` and `stvef-mp` were
added to the canonical profile list, so the archive now carries and requires the
complete Elite Force set.

## Remaining gates

Generated zones are a first pass from BSP geometry and shader evidence. No
listening check has been made against any of them, and that remains the gate
this work does not close.

The HUD rules are pinned against recorded draws and the engine's matching order.
How the corrected HUD actually looks on a wide display is a manual check. A
wide-aspect screenshot pair was captured with the correction off and on; it is
not offered as evidence, because the two sessions render different frames and
the difference does not isolate the HUD transform.

Elite Force single player's weapon selector is opened by key input, which this
harness does not drive, so it was not recorded and keeps the centred default
rather than a guessed rule.

Both Elite Force profiles remain development-gated, and retail single player
remains a Windows x86 runtime boundary. The existing unrelated issues are
unchanged: the bad ZIP CRC in `video/st_21.bik`, missing asset path and
`messhall` subwave warnings, the Windows x86 native RT limitation, existing
compiler warnings, and previously recorded wider Linux-suite failures.

## Later work

When this branch merged on 2026-09-26, its HUD rules gave way to the presets
the main line had authored from its own runtime captures and pinned by hash
(`2026-09-25-widescreen-hud-and-fov.md`), which also stretch single player's
cutscene letterbox bars to the screen's edges. The draw groups recorded here
now check those presets: every captured health, armour and ammunition draw at
the bottom corners resolves to a rule pinned to its own corner, no rule is
shadowed by an earlier one, and none claims the loading emblem. One Holomatch
draw at the bottom centre (`gfx/interface/cap4`) matches no rule and keeps its
place under the default centred correction. The audio zone sidecars stand as
recorded above.
