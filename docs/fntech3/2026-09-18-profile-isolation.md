# One writable home shared by four profiles

The Q3/QL matrix's cross-profile isolation row asks for restart, config
persistence, cached-module, protocol-state and sidecar checks, and the RTCW
matrix asks for mode-specific configs and modules. Earlier evidence ran the
profiles sequentially with a separate home each. This record runs `q3`, `ql`,
`rtcw-sp` and `rtcw-mp` from the same client against one shared writable home,
the case where profiles could see each other's files. No engine code changed;
the [machine-readable record](2026-09-18-profile-isolation.json) holds
identities, commands and every value read back.

## Method

[`scripts/probe_profile_isolation.py`](../../scripts/probe_profile_isolation.py)
starts the fnTech3 client once per step, each profile against its own
installation, all with the same `fs_homepath`:

1. `q3`, `ql`, `rtcw-sp`, `rtcw-mp`: each archives a marker cvar named after
   itself, the player name `iso-<profile>` and a `cl_maxpackets` value of its
   own (41–44), then quits.
2. The reverse order, so each profile starts after all the others have written
   to the home: each prints every marker, its name, `cl_maxpackets` and
   `protocol`. `q3` then archives one more marker.
3. `ql` again, printing that later `q3` marker.

Runs are windowed on unswitched private desktops with sound and mouse/joystick
input disabled; commands come from startup configs only. The installations'
file lists are compared before and after.

## Result

All nine runs exit normally. In round 2:

| Profile | Markers seen | Name | `cl_maxpackets` | Protocol | Modules loaded |
| --- | --- | --- | --- | --- | --- |
| `rtcw-mp` | its own | `iso-rtcw-mp` | 44 | 60 | `cgame_mp_x86.dll`, `ui_mp_x86.dll` from native cache `29982d22…` |
| `rtcw-sp` | its own | `iso-rtcw-sp` | 43 | 49 | `cgamex86.dll`, `uix86.dll` from native cache `d7bf0446…` |
| `ql` | its own and `q3`'s first | `iso-ql` | 42 | 91 | `uix86.dll`, `cgamex86.dll`, `qagamex86.dll` from native cache `9e9ae977…` |
| `q3` | its own | `iso-q3` | 41 | 68 | `vm/ui.qvm`, `vm/cgame.qvm` |

Each profile persisted its values only in its own files: `baseq3/q3config.cfg`
for Q3, `qzconfig.cfg` and `repconfig.cfg` at the home root for QL,
`main/wolfconfig.cfg` for SP and `main/wolfconfig_mp.cfg` for MP. Each loaded
only modules of its own API, and the three native-module caches are distinct
hash-named directories. The installations are unchanged.

`ql` holding `q3`'s first marker is deliberate. When QL starts in a home without
`qzconfig.cfg`, `Com_ExecuteCfg` imports the legacy `baseq3/q3config.cfg` once
as a one-way migration for FnQ3/FnQL users and resets the module choices to
native. Round 3 shows the import is one-off: `ql` still has `q3`'s first marker
and not the later one. With default homes the case does not arise, because the
Q3 profile writes into its own installation and QL into `<install>/<SteamID>`.
It happens only when a user points both profiles at one `fs_homepath`.

## Observations

- Q3 and QL both keep their server-browser cache as `baseq3/servercache.dat`,
  preserving the legacy layout noted in `cl_ui.cpp`, so a shared home gives them
  one list of cached and favorite servers. The record layout is the same, so
  neither corrupts the other. RTCW MP and ET use their own file names. Not
  changed here.
- Both RTCW modes name `wolfconfig_server.cfg` as their dedicated-server config.
  Retail SP has no dedicated server, and this probe ran no server.

## Not covered

Downloads, Steam state, dedicated servers, mods and concurrent processes sharing
a home were not exercised.
