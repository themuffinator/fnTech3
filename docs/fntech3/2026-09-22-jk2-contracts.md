# Jedi Outcast: what a legitimate installation says about itself

## Scope

Star Wars Jedi Knight II: Jedi Outcast is the seventh title. This record pins
its startup contracts and its module boundary against a legitimate retail
**1.02a** installation. Every number below was read from that installation --
from its paks, or from its own two engine binaries -- and each says where.

Nothing is claimed about gameplay, rendering or the wire. The multiplayer
module loads and runs; it stops at the first import that is not named here.

## The installation

    jk2sp.exe  jk2mp.exe  jk2gamex86.dll  version.inf
    base/assets0.pk3  base/assets1.pk3

Two paks, 14,136 files. `version.inf` reports `Version=1.02a`, and both
binaries agree: `JK2: v1.02a` and `JK2MP: v1.02a`.

Both configurations sit in `assets0.pk3` -- `default.cfg` at 3,048 bytes and
`mpdefault.cfg` at 3,442 -- so the pak names the installation the way Jedi
Academy's `assets2.pk3` names that one, and one installation carries both
modes.

## What each engine says about itself

Every value here is a bounds check, a clamp or a cvar registration read out of
the retail binary that enforces it.

| | Single player | Multiplayer | Read from |
| --- | --- | --- | --- |
| Protocol | 40 | 15 | `va_("%i", N)` in each binary's own `protocol` registration, three call sites each |
| Configuration | `jk2config.cfg` | `jk2mpconfig.cfg` | the binaries' strings |
| Executes | `default.cfg` | `mpdefault.cfg` | the same |
| Game directory | `base` | `base` | the same |
| MAX_CONFIGSTRINGS | 1,024 | 1,400 | each `SV_SetConfigstring: bad index` bound, twice apiece |
| MAX_SUBMODELS | 512 | 256 | each `MAX_SUBMODELS exceeded` bound |
| MAX_CLIENTS | 1 | 32 | multiplayer clamps `sv_maxclients` with `Cvar_Set("sv_maxclients", va("%i", 32))` |
| Bot library | none | Quake III's, whole | `bot_enable` defaults to `1`, with `bot_minplayers`, `botlib.log` and `.aas`; single player has none of them |
| Modules | `jk2gamex86.dll` in the root | `vm/jk2mpgame.qvm`, `vm/cgame.qvm`, `vm/ui.qvm` inside `assets0.pk3` | the installation |

The module difference is the important one. Jedi Academy shipped native
libraries; **Jedi Outcast shipped Quake III bytecode**, so its multiplayer
module needs no native host at all -- the interpreter fnTech3 already has runs
it.

Protocol 40 is now spoken by three single players: Elite Force's, Jedi
Academy's and this one. A version names every family that uses it, and each
profile admits only its own.

## Its formats are Jedi Academy's

Everything the format layer was built for carries over, and a second
installation is the check that layer needed:

| | Jedi Outcast | Jedi Academy |
| --- | --- | --- |
| Maps | `RBSP` version 1, 41 of them | `RBSP` version 1, 57 |
| Meshes | `2LGM` version 6, 71 | `2LGM` version 6, 127 |
| Skeletons | `2LGA` version 6, 21 | `2LGA` version 6, 66 |
| Motion paths | ROFF version 2, 68 | ROFF version 2, 92 |
| Scripts | `IBI` 1.57, 1,356 | `IBI` 1.57, 1,018 |

**Every one of Jedi Outcast's 1,557 assets reads through fnTech3's own readers
with no refusals**, and the ICARUS grammar holds over its scripts
independently: 33,767 blocks consumed and none refused, no script badly
nested at a depth reaching 29, and all 318 of its ranges ordered. Across both
titles that is 487 ranges of 487.

Because both titles use it, the engine's map identity for `RBSP` is named for
the format and not for either game.

## The multiplayer module boundary

A developer server mounts both paks, loads a retail map's collision, and
compiles and runs the retail `vm/jk2mpgame.qvm` out of the pak -- 865,928
bytes of code. It then calls the engine, and the engine answers or refuses by
number.

Retail `jk2mp.exe` settles what those numbers are. Its dispatcher bounds the
argument at 599 and indexes a byte table, so the imports run **0 to 599**, with
**221** of them handled and the rest falling to its own
`Bad game system trap`. The handled ones lie in runs that begin at round
numbers, as Raven's other titles do:

    0-51, 100-111, 200-211, 300-318, 400-425, 500-599

**The first two runs are Quake III's own, at Quake III's own numbers.** That is
not an assumption; it is what the handlers do:

| Trap | What its handler does | Which is |
| --- | --- | --- |
| 0 | takes one argument and passes it to the console printer with `%s` | `G_PRINT` |
| 1 | the same, with a drop-level error | `G_ERROR` |
| 2 | tail-calls the millisecond clock with no arguments | `G_MILLISECONDS` |
| 3 | takes four arguments | `G_CVAR_REGISTER` |
| 15 | takes five | `G_LOCATE_GAME_DATA` |
| 29 | reaches the message `area >= cm.numAreas` | `G_AREAS_CONNECTED` |
| 34, 35 | reach `SV_BotFreeClient: bad clientNum: %i` | the bot client pair |
| 100 | takes three arguments and passes them to a memory fill | `G_MEMSET` |
| 103 | is a single `fsin` | `G_SIN` |

Jedi Academy's numbering is **not** this one -- Raven inserted two precision
timers at 3 and 4 there, so its cvar calls sit two higher -- which is exactly
why nothing here is run through another title's table on the assumption that
they agree.

The run at 100 is served whole: its twelve are Quake III's memory and
arithmetic calls in Quake III's order, and the shared dispatcher already
answers them. Jedi Outcast stops two short of Quake III's fourteen, with no
handler for the two debug prints that would follow.

Of the run at 0, the thirty calls that carry nothing of this title's own are
served as well -- the console and error calls, the clock, the five cvar calls,
the two argument calls, the four filesystem calls and the seek, the console
command, the drop and server commands, the configstring pair, the three
userinfo and serverinfo calls, the two visibility checks, the area check, the
entity-token reader, the file list, the real-time clock and vector snapping.
Each passes only strings, numbers, vectors or the engine's own buffers.

The cvar pair is served on evidence rather than resemblance: retail jk2mp.exe
reads the handle at offset 0 and the modification count at 4, and both its
register and its update copy 256 bytes of value, so this title's cvar record is
Quake III's 272-byte one exactly.

**With those, the retail module initializes.** It prints its own banner and
writes its own serverinfo:

    ------- Game Initialization -------
    gamename: basejk
    gamedate: Mar 04 2002
    InitGame: \dmflags\0\fraglimit\20\g_gametype\0\mapname\ffa_bespin
              \protocol\15 ... \g_blueteam\Rebellion \g_redteam\Empire
              \g_maxForceRank\6 \duel_fraglimit\10 \gamename\basejk

`gamedate` is Raven's own build date, printed by the shipped bytecode. The
module then asks for trap 15, and that is where it stops: 15 is the call that
publishes its entity and client arrays, and this title's records are not frozen,
so it is refused rather than read at another game's offsets.

## What its records measure, so far

The module states its own strides when it publishes its arrays, and the
refusal of that call reports them rather than keeping them. Against a retail
`ffa_bespin`:

| | Bytes | Measured from |
| --- | --- | --- |
| `gentity_t` | **828** | the module's own locate call |
| `gclient_t` | **1,756** | the same |

The engine settles where its own half of an entity sits. `SV_SetBrushModel`
writes four members through the entity pointer, at 312, 316, 328 and 340, which
are a brush-model flag, two bounds vectors and a contents mask at exactly
Quake III's spacing from a base of **296**. `SV_SvEntityForGentity` reads the
entity's number at offset 0 and bounds it at **1,024**, this title's
`MAX_GENTITIES`.

The two record sizes come from the engine's own delta tables, which it keeps
as arrays of a name, an offset and a bit width. Recovering those gives every
networked member of both records, Raven's own among them -- `bolt1`,
`trickedentindex`, `modelGhoul2` and `forceFrame` on an entity;
`fd.forcePowersKnown`, `saberMove`, `zoomMode` and `fallingToDeath` on a
player -- and with them the last offset in each:

| Record | Last networked member | So the record is | Confirmed by |
| --- | --- | --- | --- |
| `entityState_t` | `generic1` at 292 | **296** | the shared half beginning there |
| `playerState_t` | one at 1,364 | **1,368** | the engine copying 1,368 bytes whole, at three sites |

Each size is measured twice, by unrelated means. Nothing about either rests on
a single reading.

**With those, the call that publishes the module's arrays is served.** The
engine's checked view binds them at the measured strides -- an 828-byte entity
whose engine-owned half starts 296 bytes in, and a 1,756-byte client whose
player state is the 1,368 at its front -- and the bind's own validation passes.
The module then runs on to **trap 588**, deep in the run at 500 that is Raven's
own and has no names yet.

One member is not measured: the player record's ping. It appears in no delta
table, because this lineage does not send it. The layout claims no offset for
it and the accessor refuses to write one, rather than putting it where another
game keeps it.

## What this does not show

- Forty-three of this title's imports are served and 178 of the 221 its own
  engine handles are not. The module initializes, publishes its arrays and
  stops at trap 588.
- The nine members of the entity's shared half that are not directly evidenced
  are placed at Quake III's spacing, which the four that are confirm at four
  separate offsets. That is strong, and it is not the same as measured.
- The first run is shown to be Quake III's by what its handlers do, but the
  records those calls carry are Raven's, not Quake III's, and none of them is
  frozen yet. That is why the calls that carry one are refused rather than
  handed to a dispatcher that would read them at Quake III's offsets.
- Single player has not been started; its `GetGameAPI` table is unwritten.
- The runs at 200, 300, 400 and 500 have no names at all.
- Nothing is drawn, no packet is encoded, and no client exists.
- One installation, one edition, one platform.
