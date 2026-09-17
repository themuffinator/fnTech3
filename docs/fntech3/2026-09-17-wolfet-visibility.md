# Enemy Territory area visibility and routing

## Scope and authority

This work follows `34e10c5`, with a clean worktree at the start except for
another session's in-flight RTCW files. It derives ET's per-area waypoints and
area-to-area visibility from the loaded navigation world, then implements the
five reachability floods that read them. ET game-import coverage goes from 198
to 202 of 212, leaving one. The ET client and default-release gates are
unchanged.

Interoperability facts come from the pinned original
[ET routing](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_route.c),
[alternative routes](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_routealt.c),
[route table](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_routetable.c),
[movement prediction](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_move.c)
and [bot imports](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_bot.c).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3; each contract was read and reimplemented.

## What this is reachable by

**Retail Enemy Territory ships no `.aas` files.** Every `.pk3` in a legitimate
`etmain` was scanned and none contains one, which matches the game having no
built-in bots. Nothing in this record can therefore be exercised by stock ET
content: a server only builds this data once a game module asks the bot library
to load a map, and no navigation world exists for it to load. The evidence below
is source and unit level by necessity, and no live behaviour is claimed.

It still belongs in the engine: these are declared ET imports that previously
killed the server with an unsupported-import error, and a mod shipping its own
`.aas` files is exactly the case the ET contract exists for.

## Derivation

An earlier record said the five routing imports waited on ET's `.rtb` route
table. Reading further showed that is wrong: the waypoints and visibility they
read come from `AAS_CreateVisibility`, which derives them from the loaded world.
The route table caches other data. No route-table reader is needed.

**Waypoints.** For every reachable area, a player-sized box is dropped 256 units
from the area centre against solid, player-clip and monster-clip. A blocking
entity is retried once as that entity's own pass entity. The landing point is
kept only when the trace started outside solid, moved, and ended in the same
area; otherwise the area has no waypoint, which the original spells as a cleared
vector and fnTech3 reports as none.

**Visibility.** Every pair of areas that produced a waypoint is tested: the
potentially visible set first, then a point trace against solid. The result is
symmetric, and an area sees itself. The original compresses each row and caches
it; fnTech3 keeps a bit matrix, which is engine storage no module or file
observes — the only contract is what the query answers. A world needing more
than 64 MiB for that matrix is refused with a warning rather than truncated, and
then every area simply reports no visibility.

## The five floods

| Import | Behaviour |
| --- | --- |
| `BOTLIB_AAS_LISTAREASINRANGE` | Waypoints of reachable areas within range that the source area can see, skipping ladder and disabled areas. Each area is considered once. |
| `BOTLIB_AAS_AVOIDDANGERAREA` | Cheapest area far enough from the danger position; a candidate under one second is taken at once, otherwise the cheapest within five. |
| `BOTLIB_AAS_RETREAT` | Walking distance from every danger spot first, then the first reachable area far enough from both the source and the danger. |
| `BOTLIB_AAS_NEARESTHIDEAREA` | Cheapest area the enemy cannot see, penalising moves towards the enemy and rejecting endpoints within 40 units of it. |
| `BOTLIB_AAS_ALTROUTEGOALS` | Route and cluster portals inside a `500 + 3 × goal` window, clustered through shared faces, each published at the cluster's most central area's waypoint. Goals whose first reachability towards the start and the goal share an area are dropped. |

Three deliberate differences from the original, none observable through an
import:

- The cluster flood walks an explicit stack rather than recursing, so a large
  cluster cannot exhaust the server thread's stack.
- The danger distances use a standard heap rather than the original's own
  priority queue; the shortest distances are the same.
- The hide search asks the game module through an engine callback rather than
  through the frozen bot-library import table, so the bot library never holds a
  pointer into a game module. An absent callback means no candidate can be
  proved hidden, and the search reports none.

The hide search keeps the original's per-frame work budget of 3,000 steps and
its negative-source-number restart.

## Verification

Clean Windows x86 RelWithDebInfo build with `-Dwolfet-development=true` and
`-Drtcw-development=true`; no errors or warnings. The complete automated suite
passes.

A new executable regression drives the real visibility and waypoint accessors
against a synthetic world: nothing is visible before a bit is set, an area sees
itself only when told to, symmetry is the caller's to establish, area zero and
out-of-range areas are outside the world, a cleared waypoint reports none, and
another profile or an unbuilt world sees neither. Source regressions pin the
waypoint trace and its retry, the symmetric build with its size refusal, that
each flood refuses a world without the tables and reads the derived ones, the
hide search's budget and callback requirement, the iterative cluster flood, ET's
own portal selection and travel window, and that only ET builds this while the
RTCW route cache loader stays RTCW's.

A retail ET client still spawns, moves and exits cleanly against the rebuilt
server, which confirms the routing work did not disturb the paths a stock ET
server actually takes. That run also shows no visibility being built, because
no navigation world is loaded, as described above.

The RTCW bot fixtures gained declarations for the new ET entry points they now
compile past; those tests pass unchanged otherwise.

## Remaining gates

One import stayed refused at this checkpoint; the
[prediction follow-up](2026-09-17-wolfet-prediction.md) closes it by writing
ET's loop. `BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT` returns ET's
`aas_clientmove_t`, which embeds a full world trace carrying a plane, a surface
and contents. ET produces it because its prediction loop traces with the bot
library's world trace; this engine's prediction traces client bounding boxes and
produces a navigation trace with none of those fields. Filling them with zeros
would report a surface a module could act on, so the import fails instead.
Closing it means writing ET's prediction loop, not converting a record.

Nothing here has run against a navigation world, for the reason given above.
Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix) remains
as recorded, including the gated ET client.

## Unrelated observations

The [navigation record](2026-09-17-wolfet-navigation.md) attributed these five
imports to a missing route table. That was wrong and is corrected here; its
`AAS_WolfETAreaWaypoint` now answers from the derived table rather than always
reporting none.
