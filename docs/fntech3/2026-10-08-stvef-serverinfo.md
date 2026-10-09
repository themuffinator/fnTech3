# Holomatch server details carry retail's keys, 2026-10-08

A retail Holomatch server and fnTech3's, started with the same settings on the
same map, listed different keys in `serverinfo`, the details every client and
server browser reads and the game writes to its log at `InitGame`. Retail's
carried `sv_pure`; fnTech3's lacked it and added three of its own,
`sv_dlRate`, `sv_minRate` and `sv_playingDemo`. Both servers' `systeminfo`
already had the same seven keys. fnTech3's Holomatch server now lists retail's
keys, and registers `sv_pure` as retail does. The
[JSON companion](2026-10-08-stvef-serverinfo.json) holds identities and both
servers' replies.

## Retail

`stvoyHM.exe` 1.20 (`5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff`)
registers its engine's serverinfo in `SV_Init` (0x433e00..0x434180):
`g_pMod*`, `sv_keywords`, `protocol`, `mapname`, `sv_privateClients`,
`sv_hostname`, `sv_maxclients`, `sv_maxRate`, `sv_minPing`, `sv_maxPing`,
`sv_floodProtect`, `sv_pure` and `sv_allowDownload`, beside `version`,
`dmflags`, `fraglimit`, `timelimit` and `g_gametype` and the game module's
own. `sv_pure` is `"1"`, archived, in both the serverinfo and the systeminfo,
and not latched (flags 0x0d at 0x433f88). It has no `sv_dlURL`, `sv_dlRate`,
`sv_minRate` or `sv_playingDemo`.

## Change

- `SV_RegisterPureMode` (`code/server/sv_init.cpp`) registers Holomatch's
  `sv_pure` archived and in its serverinfo, and the Holomatch profile no
  longer latches it (`code/qcommon/game_profile.cpp`). Admission still uses
  the value the level started with, and the gamestate's systeminfo still
  carries that value, as for the profiles that never latched it.
- `SV_HolomatchInfoKeys`, run from `SV_Init` under Holomatch alone, takes
  `sv_dlURL`, `sv_dlRate`, `sv_minRate` and `sv_playingDemo` out of the
  serverinfo; they keep working as server settings.

The other profiles are unchanged. Key order still differs: retail lists the
newest registration first, fnTech3 its sorted order; every reader looks keys
up by name. No retail text or code was incorporated.

## Evidence

- `tests/stvef_serverinfo_tests.py` compiles the production
  `SV_HolomatchInfoKeys` with stubbed cvars, and checks that `SV_Init` calls it
  under Holomatch after each of the four is registered and that no other
  server source registers them into the serverinfo again.
  `tests/rtcw_server_policy_tests.py`, which compiles `SV_RegisterPureMode`
  for every profile, expects Holomatch's `sv_pure` archived, in the
  serverinfo and unlatched, and every other profile's as before. Leaving out
  the call, keeping one key, latching `sv_pure` again or dropping its new
  flags each fails them.
- `scripts/probe_stvef_info_keys.py` starts retail's dedicated server and
  fnTech3's on `hm_borg1` with the same settings and asks each for
  `serverinfo` and `systeminfo` over rcon. Before the change
  (`fntech3.ded.exe` `b05b3b50…`, asked by the same queries before the probe
  was committed) the serverinfo keys differed by `sv_pure` and the three of
  fnTech3's own; with it (`663f4162…`, a `git archive` export of `8da1234c`
  with this change and the [frame-time change](2026-10-08-stvef-frame-time.md))
  both servers list the same 30 serverinfo and 7 systeminfo keys.
- `scripts/probe_legacy_sessions.py` ran Holomatch's pure main and kick
  sessions with this build: fnTech3's client on the retail server, and the
  retail client on fnTech3's dedicated and listen servers. Every step of all
  six sessions passed.
- The retail installation was unchanged by every run.
