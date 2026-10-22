# The Jedi single-player level clock, 2026-10-16

The [level clock record](2026-10-15-level-clock.md) settled the level clock
at a map change for every Quake III-line multiplayer profile. It left
`jka-sp` and `jk2-sp` open. Their retail engines clear the clock at every
spawn, while fnTech3 still used the inherited Quake3e rule. That rule keeps
the clock whenever a client is connected, and restarts it at 0 when the
server is empty.

This slice reads both retail spawns. It finds that they do what `stvoy.exe`
1.20 does: every level starts at 1000, whoever is connected. All three
profiles now say so through one rule, `FRAME_LEVEL_TIME_1000`. The
[JSON companion](2026-10-16-jedi-sp-level-clock.json) records the binaries,
addresses and checks. No compatibility gate closes.

Implementation is independently authored. Retail `jasp.exe` 1.01 and
`jk2sp.exe` 1.02a were read with capstone and pefile. OpenJK (pinned
`1a6a6434`) was used only to name routines and fields: reference
observation, no text incorporated. Where it disagrees with retail, retail is
recorded.

## The retail spawns

### Observed: `jasp.exe` 1.01 and `jk2sp.exe` 1.02a

Each engine finds its `SV_SpawnServer` through the `Server Initialization`
banner (`jasp.exe` `0x439be0`, `jk2sp.exe` `0x4370b0`). The level clock is
`sv.time`, the third word of the per-level block (`0xc2a19c`, `0x126bb24`).
OpenJK's single-player source names no other game clock; it never uses an
`svs.time`.

| Step | `jasp.exe` 1.01 | `jk2sp.exe` 1.02a |
| --- | --- | --- |
| Clear the level block, clock included | `rep stosd` of `0x1721f` dwords from `0xc2a190` (`0x439d01..0x439d0d`) | `0x15d07` dwords from `0x126bb18` (`0x43721a..0x437226`) |
| Start the clock at 1000 | `mov [0xc2a19c], 0x3e8` (`0x439d2e..0x439d35`) | `mov [0x126bb24], 0x3e8` (`0x437250`) |
| Ghoul2 server time set to it | `0x439d3b` (`0x4ce3b0`, `eax` = 0) | `0x43725a` (`0x450e70`, args 1000, 0) |
| `GAME_INIT` levelTime argument | `sv.time` read at `0x4398dc`, pushed before `com_frameTime` (`0x8916b4`); call at `0x439908` | `sv.time` read at `0x436d9c`, pushed before `com_frameTime` (`0xa84244`); call at `0x436dc6` |
| Settling frames | three, each at the clock before adding 100 (`0x439dbd..0x439de6`) | three (`0x4372d8..0x437308`) |
| Clients reconnected, their three time counters zeroed | `+0x56c`, `+0x570`, `+0x574` (`0x439e0c..0x439e29`) | the same offsets (`0x43730f..0x43732c`) |
| One more frame | `0x439e69..0x439e83` | `0x43736c..0x437386` |

Each level therefore hands `GAME_INIT` 1000. Its settling frames run at
1000, 1100 and 1200, its last spawn frame at 1300, and play begins at 1400.
That holds whatever the last level's time was and whoever is connected.
Neither spawn's own body reads the old clock before the clear.

OpenJK's single-player source runs four settling frames
(`code/server/sv_init.cpp`). Both retail engines run three. fnTech3
already ran three for both profiles.

### Observed: a loaded save restores its own time after the spawn

Both engines load a save by spawning the level, which starts it at 1000.
For a full save (not an autosave) they then read the save's `TIME` chunk
over `sv.time`, and its `TIMR` chunk over `sv.timeResidual`
(`jasp.exe`: spawn call at `0x43bab8`, reads at `0x43bac1..0x43bace` and
`0x43bad3`; `jk2sp.exe`: spawn call at `0x43907f`, reads at
`0x43908b..0x439098`). Writing a full save stores the same two words
(`jasp.exe` `0x43b932..0x43b94a`, `jk2sp.exe` `0x438b96..0x438ba9`).

### Before this slice: fnTech3

Both profiles start their levels through the shared `SV_SpawnServer`
(`code/server/sv_init.cpp`). It loads the game through
`SV_JKASPLoadGame()` or `SV_JK2SPLoadGame()` and hands `GAME_INIT`
`sv.time`, which the adapters pass on as the game's levelTime. Neither had a
start-time rule, so the inherited rule applied:

- With nobody connected, the clock restarted at 0. `GAME_INIT` took 0, the
  settling frames ran at 0, 100 and 200, and play began at 400. This is
  every spawn today. `jka-sp` refuses the player's connection
  (`GAME_CLIENT_CONNECT` is not served), and `jk2-sp` stops after its
  game's Init.
- With a client connected, the clock carried on from the last level, unless
  `sv_levelTimeReset` was set. That case will arise once the single-player
  client adapters are served.

Retail starts at 1000 in both cases.

## Implemented

- `code/qcommon/profile_frame_time.h`: a new rule bit,
  `FRAME_LEVEL_TIME_1000`, for `stvef-sp`, `jka-sp` and `jk2-sp`. Each
  profile's other rules are unchanged.
- `code/server/sv_init.cpp`: `SV_LevelTimeStartsAt1000()` reads the bit.
  `SV_SpawnServer` sets `sv.time = 1000` for those profiles before the game
  loads, in place of the Elite Force-only test it replaces.
  - The assignment stays where Elite Force's was: after the map and world
    are loaded, before `SV_InitGameProgs`. Retail sets the clock right after
    the clear. For these three profiles nothing in fnTech3 reads it between
    the two points; Academy multiplayer's Ghoul2 clock is the only reader
    there, and it serves `jka-mp` alone.
- No other behaviour changes:
  - The settling and reconnect frames already matched retail.
  - `sv_levelTimeReset` still gives a connected client the old time as an
    offset, as it already did in `stvef-sp`. With it off, the default, a
    client gets no offset. OpenJK's single-player client record has no such
    field; retail's was not searched for one.

## Not matched, and why

- **Save time restore.** fnTech3 has no savegame path for `jka-sp` or
  `jk2-sp` yet, so retail's `TIME`/`TIMR` restore has nothing to attach to.
  It must follow the 1000 start, as Elite Force's and RTCW's restores do,
  when one is written.
- **Ghoul2 server time.** Retail sets its Ghoul2 server clock at the spawn
  and after every settling frame. fnTech3's Academy single-player Ghoul2
  services keep no server clock: `SV_JKAGhoul2SetTime` serves `jka-mp` only.
  Not examined further.
- **Runtime.** Neither profile reaches a map change in fnTech3 yet, and the
  retail single-player engines have no dedicated server to probe. The
  evidence is the binary reading above plus the production code compiled in
  the tests below. No live comparison was run.

## Other profiles

The bit replaces `stvef-sp`'s own test with the same assignment at the same
place, so Elite Force SP is unchanged. `tests/stvef_sp_host_tests.py` still
compiles the production pre-Init phase and finds its `GAME_INIT` at 1000. No
other profile has the bit: the frozen table pins every profile's other
rules, and no profile has both the keep rule and a restart rule. `q3-ihv`
and `q3test` still start at 1000 through their own blocks, which also
publish configstrings at that point. They are left as they were.

## Tests

- `tests/jedi_level_time_tests.py` now also compiles
  `SV_LevelTimeStartsAt1000()` and the production start statement after the
  spawn's clock block.
  - Across all seventeen profile ids, it checks the other rules, the keep
    and reset bits, and the new bit (three profiles).
  - For every profile, it runs with `sv_levelTimeReset` on and off, a
    client connected or not, and a restart pending or not. Raven's three
    single-player profiles start at 1000 in every case, keeping only the
    cvar's client offset.
  - A source check pins the statement after `SV_ClearServer` and before
    `SV_InitGameProgs`, once.
- `tests/stvef_sp_host_tests.py` is given the helper for its stand-in
  profiles.

Five mutations, each applied to a private copy of `code/`, were all caught
(`.tmp/jedi-sp-clock/mutate.py`):

- `jka-sp`'s bit dropped;
- both Jedi bits dropped;
- `stvef-sp`'s bit dropped;
- the start statement made a no-op;
- the helper reading the keep bit instead.

The [JSON companion](2026-10-16-jedi-sp-level-clock.json) (`validation`)
has the build and suite results.

## Remaining and unrelated findings

- **`GAME_INIT`'s seed.** Both retail single-player engines pass
  `com_frameTime` as the game's sixth Init argument, its random seed
  (`jasp.exe` `0x8916b4`, the same word the spawn stores as `sv.serverId` at
  `0x439d8b`; `jk2sp.exe` `0xa84244`, stored at `0x4372ac`). fnTech3's
  `GAME_INIT` hands both adapters `Com_Milliseconds()` there. It is an
  arbitrary seed either way. Not changed.
- **Client time counters.** Retail zeroes each reconnected client's three
  time counters at the spawn. fnTech3's single-player client path is not
  served yet, so this could not be compared.
