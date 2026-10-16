# Jedi single-player snapshot walk, 2026-10-16

The [portal recursion record](2026-10-15-snapshot-portal-recursion.md) found
that fnTech3's shared snapshot walk served `jka-sp` with none of the rules
in Raven's single-player engines. This slice reads the whole walk, its add
routine and its snapshot builder in retail `jasp.exe` (Jedi Academy 1.01)
and `jk2sp.exe` (Jedi Outcast 1.02a), lists every rule, and serves both
profiles by them. The [JSON companion](2026-10-16-jedi-sp-snapshot-walk.json)
records each binary, address and outcome.

Neither profile builds a client snapshot yet, so the evidence is static and
unit only. `jka-sp` refuses its client at `GAME_CLIENT_CONNECT` and serves
no player state; `jk2-sp` stops after its module's Init, because fnTech3
has no entity or frame adapter for Outcast's single player (see below). No
compatibility gate closes.

Implementation is independently authored. The retail engines were read with
capstone and pefile (`.tmp/portal-recursion/walk.py`, ranges dumped to
`.tmp/portal-recursion/jasp.txt` and `jk2sp.txt`). OpenJK's
`code/server/sv_snapshot.cpp` and its single-player headers (pinned
`1a6a6434`) were used only to name routines and flags (reference
observation; no text incorporated). Where OpenJK and retail differ, retail
is followed.

## Observed, `jasp.exe`

`SV_AddEntitiesVisibleFromPoint` is at `0x43c710`, found from its
`FIXING ENT->S.NUMBER` string (`0x5114e8`). It takes the viewpoint, the
frame, the entity numbers and a portal argument. In order:

| Address | Rule |
| --- | --- |
| `0x43c721` | Nothing when the server is dead. |
| `0x43c72e..0x43c759` | Leaf, area and cluster of the viewpoint; the frame's area bits are rewritten for each pass. |
| `0x43c762` | Force Sight is on for this pass only when the portal argument is 0 and the frame's player state has bit `0x8000` (Force Sight, power 15) in `forcePowersActive` (frame `+0x298`, player state `+628`). |
| `0x43c790` | Every entity number from 0 to the game's entity count, read in place. |
| `0x43c7a3` | Skipped when not in use (`+0x114`). |
| `0x43c7a9` | Skipped when its state's `eFlags` has `0x80000`, the game's `EF_PERMANENT`: the client game draws it from the game state alone. |
| `0x43c7b6` | Its `s.number` is repaired to its index, with a developer message, linked or not. |
| `0x43c7c9` | Skipped when not linked (`+0x118`). |
| `0x43c7d7` | Skipped when `svFlags` (`+0x11c`) has `SVF_NOCLIENT` (1). No other client flag is read. |
| `0x43c7f1` | Skipped when already added to this snapshot. |
| `0x43c801` | Added, and not walked, when `SVF_BROADCAST` (`0x20`). |
| `0x43c80a` | Added, and not walked, when it is entity 0, the player. |
| `0x43c80e` | Added, and not walked, when its state's `isPortalEnt` (`+0x10c`) is set: the client game draws it in the sky portal's view. |
| `0x43c82a..0x43c858` | With Force Sight on, added, and not walked, when the Force Sight test below sees it. |
| `0x43c85d..0x43c88b` | Skipped when neither of its areas connects to the viewpoint's. |
| `0x43c891..0x43c91c` | Skipped when none of its clusters, nor its overflow range, is in the viewpoint's PVS (Quake III's test, its last-cluster quirk included). |
| `0x43c91e` | Added. |
| `0x43c931..0x43c941` | When `SVF_PORTAL` (`0x40`), walked again from its `origin2` with the portal argument 1. No `generic1` range is read. |

The Force Sight test (`0x43c600`) asks the client game's `vmMain` for its
camera position (command 5) and angles (command 6) through `0x43ea90`,
which calls the game library's `vmMain` (looked up beside `dllEntry` at
`0x4403a0`) and answers -1 when there is none. Either answer 0 means unseen.
It then measures the distance from the camera to the entity's
`currentOrigin` (`+0x158`). Unless the state's `eFlags` has `0x800000`, the
game's `EF_FORCE_VISIBLE`, an entity closer than 128 is seen and one past
the range is not. The range comes from a table at `0x527d10` indexed by the
player state's Force Sight level (`+4768`, read at `0x43c836`): 0, 1024,
2048, 4096, 4096, 4096. Otherwise the entity is seen when the camera's
forward vector and the direction to it give a dot product at least
`distance / range * 0.74 + 0.25` (`0x43c6d9..0x43c6ed`).

The add routine (`0x43c5c0`) marks the entity added before anything else,
then stops at 1,024 numbers (`0x43c5d6`). On a snapshot whose counter is
odd it also stops at 511 (`0x43c5dd..0x43c5ef`), one fewer than the 512
states its ring holds (`0x439bb7`). The counter goes up once per client
snapshot (`0x43c97d`) and starts at 0 with each level.

The builder (`0x43c970`) copies the player state, takes the viewpoint from
the client game's camera position when it answers (`0x43c9d3`), and
otherwise from the player's origin raised by its view height and moved by
its lean (`0x43c9e3..0x43ca8e`). It writes that viewpoint into the frame's
and the game's player state (`serverViewOrg`, `0x43ca94`, `0x43cabd`) and
walks from it with the portal argument 0 (`0x43cadb`). It marks nothing
sent first.

## Observed, `jk2sp.exe`

The walk is at `0x43a170` (string `0x4de14c`), its add routine at
`0x43a380`, its builder at `0x439f10`. The walk is Academy's without three
rules: it never reads its portal argument, has no `eFlags` test and no
`isPortalEnt` add. It skips an entity not in use (`+0x100`, `0x43a1f1`),
repairs its number (`0x43a1fd`), skips it unlinked (`+0x104`, `0x43a210`)
or with `SVF_NOCLIENT` (`+0x108`, `0x43a21e`) or already added
(`0x43a242`), adds it unwalked when broadcast (`0x43a24e`) or entity 0
(`0x43a25b`), and otherwise runs the area and cluster tests, adds it and
walks a portal's `origin2` with the argument 1 (`0x43a33a`). The add stops
at 1,024, and at 255 on an odd counter, its ring holding 256 states
(`0x43a3a5`, `0x437092`). The builder asks the client game's camera
position (command 5, `0x439f78`) and otherwise uses the leaning eye
(`0x439f8a..0x43a037`).

OpenJK's shared single-player server keeps the permanent-entity test for
Outcast as well; `jk2sp.exe` has none, and retail is followed.

## fnTech3 before this slice

- `jka-sp` and `jk2-sp` went through the multiplayer client-flag tests.
  The single-player games' `0x2`, `0x100` and `0x800` are `SVF_INACTIVE`,
  `SVF_TRIMODEL` and `SVF_NPC_PRECACHE`, so every inactive entity and every
  NPC spawner was left out as Quake III's client mask and "not single
  client" rules read them.
- Entities not in use, and Academy's permanent ones, were sent when linked.
  Only a linked entity's number was repaired.
- The builder marked the player's entity sent before the walk, as the
  multiplayer games do, so entity 0 never reached the client.
- Academy's sky-portal entities and Force Sight were not served, and the
  camera viewpoint was not asked for.
- A portal's pass was walked with the flag false, as the eye's, which
  Academy reads for Force Sight.
- The add stopped at Quake III's 256 entities.

## Design

- `SV_BuildCommonSnapshot` applies the per-entity tests every pass would
  repeat, in retail's order: in use (`SV_ReadGameEntityInuse`, through the
  view's existing in-use accessor), Academy's permanent bit, the number
  repair, then linked and `SVF_NOCLIENT`.
- `SV_EntityPassesSnapshotClientFlags` reads only `SVF_NOCLIENT` for the
  two profiles.
- The walk adds entity 0, Academy's `isPortalEnt` entities and what Force
  Sight shows after the broadcast test, without walking them, and passes
  the portal flag true into a portal's pass. Quake3e's second view now keys
  on Quake III and Quake Live rather than on "not RTCW", which is the same
  set for every profile that reaches it.
- Force Sight is read from the frame's `raven.fd_forcePowersActive` and
  `raven.fd_forcePowerLevel_FP_SEE`, the fields Outcast's multiplayer
  adapter already fills, and decided by
  `fntech3::jedi::sp::snapshot::ForceSightSees`
  (`code/server/server_jedi_sp_snapshot.hpp`) from the camera
  `SV_JKASPCameraView` asks the game library's `vmMain` for. A level past
  the table has no range here, where retail reads past it; no cgame is no
  camera, where retail reads one it never received.
- The builder marks no entity sent first for the two profiles, counts
  their snapshots per level (`sv.jediSPSnapshotCounter`) and sets the odd
  cap. Academy's builder takes its viewpoint from the camera when it
  answers.
- The walk's frame stays free of records: 56 bytes of locals under
  win32-dev's MSVC flags, against 60 measured for the previous slice.

## Unit evidence

`tests/jedi_sp_snapshot_walk_tests.py` compiles the production builder,
walk, common snapshot, viewpoint and `SV_JKASPForceSightSees` over the
shared fake game data, with `q3` and `jka-mp` as controls. Twenty-four
cases cover the entity rules and number repair in all four profiles, the
player left out when unlinked or not in use, portal chains, Force Sight's
close, ranged, angled, force-visible, turned-camera, power-off and
no-camera cases, a portal Force Sight shows being added unwalked while one
it does not show is walked with no camera question from its pass, the
511/1,024/511 and 255/1,024/255 caps against Quake III's 256, and the
camera viewpoint. Twelve mutations of the production source (each rule
removed in turn) each fail it.

`wolfet_snapshot_tests.py`, `wolfet_viewpoint_tests.py`,
`jedi_snapshot_walk_tests.py`, `snapshot_portal_chain_tests.py`,
`stvef_sp_snapshot_tests.py`, `stvef_sp_cgame_tests.py` and
`alice_snapshot_source_tests.py` pass (the last now pins the new cap
line). `sv_snapshot.cpp` and `sv_game.cpp` compile with win32-dev's
`/W4 /WX` flags, and the `jka-sp` bridge test, whose fake loader now also
answers the `vmMain` lookup, passes.

## Remaining

- No live evidence: `jka-sp` serves no client, so no snapshot is built, and
  its player state has no adapter. Force Sight reads two internal fields no
  `jka-sp` adapter fills yet; the record's `+628` and `+4768` are where that
  adapter must take them from.
- Academy's builder also leans the eye and writes `serverViewOrg` back into
  the game's player state; both wait for that adapter.
- `jk2-sp` cannot read entities at all: `ServerGameDataView` has no
  `COM_GAME_JK2_SP` layout, so `ReadEntityState` and the in-use read fail,
  and `server_jk2_sp_game.hpp` stops after Init with "entity and frame
  adapters are not implemented". Its walk rules are in place with unit
  evidence only; its camera viewpoint needs a cgame hook of its own.
- Retail's even snapshots can add up to 1,024 numbers into a ring of 512
  (256 in Outcast), overwriting their own first states; fnTech3 sends them
  intact.

## Unrelated findings

None new.
