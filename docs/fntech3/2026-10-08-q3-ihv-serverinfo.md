# The IHV Test's serverinfo keys, 2026-10-08

fnTech3's IHV server published its own serverinfo: every cvar this engine marks
serverinfo, sorted by name. The original's keys, the last open point of the
server protocol row besides play, are now published instead. The
[JSON companion](2026-10-08-q3-ihv-serverinfo.json) holds identities and the
replies.

## Observed in the original

fnTech3's client printed `serverinfo` on the original server's `tim_dm1`:

```text
\teamflags\0\maxclients\8\gamedate\Feb 12 1999\gamename\baseq3\sv_maxclients\8
\privateclients\0\mapname\tim_dm1\protocol\33\timelimit\0\fraglimit\0\dmflags\0
\version\4.01 x86 Feb 12 1999 RELEASE\hostname\noname\cheats\1
```

(one string, wrapped here). The original's `getstatus` reply carries the same
string.

Its `SV_Init` (`0x419a40`) registers seven cvars with its serverinfo bit, 8:

- `timelimit` (flags 8);
- `cheats` (`0x28`);
- `protocol` and `mapname` (`0x88`);
- `privateclients` (8);
- `hostname` (9);
- `sv_maxclients` (`0x28`).

Its download switch, `sv_allowdownload`, is registered with no flag
(`0x419b66`). The version comes from the engine's start, and the game module
adds `teamflags`, `maxclients`, `gamedate`, `gamename`, `fraglimit` and
`dmflags`. Of all these, the client game reads only `dmflags`, `fraglimit`,
`mapname`, `maxclients`, `teamflags` and `timelimit`, from its own strings.

## What fnTech3 published

fnTech3 published 21 keys, sorted by name. It had no `hostname` or
`privateclients`; it published `sv_hostname` and `sv_privateClients` instead.
It added seven keys the IHV Test does not have: `g_gametype`,
`sv_allowDownload`, `sv_dlRate`, `sv_floodProtect`, `sv_maxRate`,
`sv_minRate` and `sv_playingDemo`.

The client game read everything it needs either way. A server browser, though,
read no `hostname` from `getstatus`, and the `hostname` an operator set never
reached the serverinfo.

## The change

For this profile, `SV_IHVServerinfo` (`code/server/server_q3_ihv_net.hpp`)
builds the serverinfo from two sets of keys:

- the original engine's keys: `sv_maxclients`, `privateclients`, `mapname`,
  `protocol`, `timelimit`, `version`, `hostname` and `cheats`;
- the game's own serverinfo cvars.

Keys come in the order the original's cvar list gives them with the build's
game. Any other game serverinfo cvar follows. `SV_SetConfigstring` uses it, as
it uses Q3Test's builder, and so does the `getstatus` reply. `hostname` and
`privateclients` now carry the serverinfo flag, so a change to either is
published.

The version stays this engine's, so a reader can tell the two servers apart. The
original's `getinfo` reply ends in a word the original never sets (91536724 in
the earlier record); fnTech3 sends its count of connected clients there, as
before.

## Results

| Run | Result |
| --- | --- |
| fnTech3 client, `serverinfo` on fnTech3's server | the original's 14 keys in its order; the version is fnTech3's |
| `getstatus` to fnTech3's server and to the original | the same string; the version and player pings differ |
| `+set hostname IHVProbe`, then `rcon set hostname Renamed` | `getstatus` and `getinfo` report each name |

`tests/q3_ihv_serverinfo_tests.py` compiles the builder against the original's
string with this engine's extra cvars set, and checks every place that reads it.
`tests/rtcw_cast_messages_tests.py`, which compiles `SV_SetConfigstring`, gains
a stub.

## Open

Fought play and remote peers remain open. The profile stays a Windows x86
developer path, and release builds still refuse it.
