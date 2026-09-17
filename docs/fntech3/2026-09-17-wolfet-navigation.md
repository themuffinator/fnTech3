# Enemy Territory navigation imports and chat matches

## Scope and authority

This work follows `79ff9a4`, with a clean worktree at the start except for
another session's in-flight RTCW files. It adds ET's own blocking-entity
behaviour, its area centre, ladder and waypoint queries, its two inert route
imports, the bot-library self test, the movement-state avoid-reach reset, and
its chat-match record. ET game-import coverage goes from 189 to 198 of 212. The
ET client and default-release gates are unchanged.

Interoperability facts come from the pinned original
[ET blocking entity and area queries](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_sample.c),
[reachability](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_reach.c),
[routing](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_route.c),
[route table](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_routetable.c),
[alternative routes](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_routealt.c),
[movement prediction](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_aas_move.c)
and [chat](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/botlib/be_ai_chat.c).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3; each contract below was read and reimplemented.

## A defect the retail module reaches

ET's `AAS_SetAASBlockingEntity` differs from **both** RTCW bodies, and fnTech3
was choosing between the two RTCW ones by asking `Com_GameIsRTCWSP()`. Under ET
that picked the multiplayer rule, so:

- **Reset.** ET resets routing when the box is empty and `blocking` is
  *negative*. RTCW MP resets when it is *zero*. The retail ET game module calls
  `trap_AAS_SetAASBlockingEntity( vec3_origin, vec3_origin, -1 )`, so fnTech3
  took the blocking branch on a call that meant "enable everything".
- **Blocked value.** ET passes `( blocking & ~1 ) | !( blocking & 1 )`, keeping
  every caller bit but the inverted low one. fnTech3 passed `!blocking`.
- **Movers.** ET can restrict a block to areas with `AREACONTENTS_MOVER` via its
  `BLOCKINGFLAG_MOVER` bit and retries once without the restriction when no area
  matched. RTCW has no such mode.

ET now has its own `AAS_WolfETSetBlockingEntity`; the RTCW body is untouched.

## Contracts

**Area centre and ladder** read the loaded area and area-settings arrays
directly, bounded by `numareas` and `numareasettings`. As in the original, an
invalid area leaves the caller's vector untouched rather than writing zeros.

**Area waypoint** returned false at this checkpoint, because fnTech3 had no
waypoint table and false is the original's own answer for that state. The
[visibility follow-up](2026-09-17-wolfet-visibility.md) derives the table and
this query now answers from it.

**Two route imports are inert in the shipped engine.** `AAS_RT_ShowRoute`'s
entire body sits inside `#ifdef DEBUG`, and `AAS_RecordTeamDeathArea` opens with
an unconditional `return`. fnTech3 validates their arguments and does nothing,
which is what a retail ET server does.

**`BOTLIB_TEST`** forwards to the bot library's own test entry with a bounded,
optionally null string argument. **`BOTLIB_AI_INIT_AVOID_REACH`** clears the
movement state's avoid-reach arrays through the engine-internal
`BotInitAvoidReach`; the frozen bot export has no entry for it.

**Chat matches.** ET's `bot_match_t` is 224 bytes: a 150-byte string, type and
subtype, then eight variables of a raw pointer and a length. This engine stores
a byte offset into its own 256-byte string instead. `BOTLIB_AI_FIND_MATCH` runs
the engine match, narrows the string to ET's 150 bytes with termination, and
publishes each variable as an address inside the module's own copy — dropping
any variable whose text did not survive the shorter string rather than handing
the module a pointer past its record. The address is checked to fit the 32 bits
ET's record reserves. `BOTLIB_AI_MATCH_VARIABLE` reads that record back and
refuses a pointer that does not lie inside the string the module passed.

## Verification

Clean Windows x86 RelWithDebInfo build with `-Dwolfet-development=true` and
`-Drtcw-development=true`; no errors or warnings. The complete automated suite
passes.

A new executable regression pins every ET botlib record size and the match
record's field offsets, and checks that a variable addresses only the record's
own string with a null pointer meaning absent. Source regressions pin ET's
blocking rules against RTCW's, that the two inert imports stay inert with the
release-body reason recorded beside them, that the area queries are bounded and
the waypoint query answers false, and that the set of still-unhandled imports is
exactly the eight retail ET refuses plus the six navigation items below.

No live ET bot session is claimed. The retail ET game module reaches only the
blocking-entity import from this set, and it ships with `bot_enable 0`.

## Remaining gates

Six imports stayed refused at this checkpoint. The
[visibility follow-up](2026-09-17-wolfet-visibility.md) then closed five of
them by deriving ET's waypoints and area visibility from the loaded navigation
world rather than from the route table this record blamed, and corrects the
attribution below. Only `BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT` remains: ET's
`aas_clientmove_t` embeds a full world trace with a plane, a surface and
contents, which this engine's prediction does not produce, and zeros there
would report a surface a module could act on.

Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix) remains
as recorded, including the gated ET client.

## Unrelated observations

None new. The blocking-entity divergence described above is an ET defect, not a
finding in another profile: RTCW's own body is unchanged and still selects its
single-player and multiplayer sentinels as before. This record's attribution of
the five routing imports to a missing route table was itself wrong; see the
visibility follow-up.
