# RTCW MP pure admission on every stock map, both directions

Pure admission for RTCW MP had been observed on `mp_beach` only: a retail client
joining fnTech3 in the [pure-name record](2026-09-17-rtcw-mp-pure-names.md), and
fnTech3 joining a retail server in the [handshake record](2026-09-18-rtcw-mp-handshake.md).
The [RTCW matrix](COMPATIBILITY.md#rtcw-gate-matrix) listed broader maps as open.
This record runs both directions on every official MP map in a legitimate
installation. The first sweep found that fnTech3 servers fell back to degraded
pure on one map; that is corrected here and the sweep repeated. The
[machine-readable record](2026-09-18-rtcw-mp-pure-maps.json) holds identities
and per-map results for both sweeps.

## Method

For each map found in the installation's `mp_pak*`/`mp_pakmaps*` archives, in
the game type its own `.arena` file advertises (objective where offered, else
checkpoint), [`scripts/sweep_rtcw_mp_pure.py`](../../scripts/sweep_rtcw_mp_pure.py)
runs:

- **inbound**: retail `WolfMP.exe` joins the fnTech3 dedicated server running
  `sv_pure 1`, and must print `Connected to a pure server.`, put the server's
  paks on its pure list, reach `CS_ACTIVE` without an unpure drop, and both
  processes must exit 0;
- **outbound**: the fnTech3 client joins a retail `WolfMP.exe` dedicated server
  running `sv_pure 1`, over loopback UDP so the full challenge exchange runs,
  and must print `Connected to a pure server.`, reach `CS_ACTIVE` and
  `ClientBegin` in retail's own log without an unpure drop, capture an engine
  screenshot and exit 0.

The two probes gained `--map` and `--gametype` for this. Everything runs
windowed on unswitched private desktops with isolated homes, sound and
mouse/joystick input disabled; console commands only, no injected input and no
operating-system capture. The installation is unchanged after every run.

## Degraded pure on `mp_sub`

The first sweep passed 14 maps in both directions. On `mp_sub` the retail
client was admitted but never took the pure path: it printed no
`Connected to a pure server.` and put nothing on its pure list, because the
fnTech3 server had logged `WARNING: skipping sv_paks setup to avoid gamestate
overflow` and published no `sv_paks`. Clients were therefore admitted without
any pak restriction.

That fallback is the Quake3e-lineage "degraded pure" guard: before publishing
`sv_paks`, the server estimates the room left in the gamestate message. The
estimate encoded every profile's configstrings and baselines with Quake Live's
tables inside Quake III's 16 KiB message. RTCW sends its gamestate in one
message of its own, twice that size, with its own entity tables, and original
RTCW MP publishes `sv_paks` without any such check. `mp_sub`'s gamestate fits
RTCW's message, which the retail client then received, but not the Q3 estimate.

The estimate now uses the active RTCW profile's own wire tables and message
limit. Q3 and QL keep their existing, conservative estimate unchanged. With the
correction `mp_sub` publishes all 15 checksums and the retail client puts all 15
on its pure list; no other map had taken the fallback.
[`tests/rtcw_gamestate_budget_tests.py`](../../tests/rtcw_gamestate_budget_tests.py)
pins the profile selection.

## Results after the correction

| Map | Game type | Pak | Inbound: pure list, admission | Outbound: admission | Both |
| --- | --- | --- | --- | --- | --- |
| `mp_assault` | objective | `mp_pak0.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_base` | objective | `mp_pak0.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_beach` | objective | `mp_pak0.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_castle` | objective | `mp_pak0.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_chateau` | objective | `mp_pakmaps3.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_dam` | objective | `mp_pakmaps5.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_depot` | objective | `mp_pak0.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_destruction` | checkpoint | `mp_pak0.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_ice` | objective | `mp_pakmaps1.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_keep` | objective | `mp_pakmaps2.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_rocket` | objective | `mp_pakmaps6.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_sub` | objective | `mp_pak0.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_tram` | objective | `mp_pakmaps4.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_trenchtoast` | checkpoint | `mp_pakmaps0.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |
| `mp_village` | objective | `mp_pak0.pk3` | 15 paks, CS_ACTIVE, no unpure drop | CS_ACTIVE, ClientBegin, pure server seen | pass |

All 15 maps pass both directions. On every map the fnTech3 server published the
same 15 loaded paks, recorded per run, and each retail client put all 15 on its
pure list. Every installation inventory is unchanged.

## Remaining gates

- Spectator admission only; team play, objectives and sustained sessions on
  these maps are not claimed.
- One installation, loopback only, stock maps only: mods with their own paks,
  remote networks and downloads under pure remain open.
- ET shares the estimate and was not changed or measured here.
- The SP pure contract is separate and unchanged.
