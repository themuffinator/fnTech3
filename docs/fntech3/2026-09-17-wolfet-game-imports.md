# Enemy Territory game-import coverage

## Scope and authority

This work follows `7f3b65d`, with a clean worktree at the start. It closes most
of the ET game system-call gap recorded in the
[initial ET record](2026-09-17-wolfet-contracts.md): the binary message channel,
temporary bans and their connection refusal, ET's three-argument drop-client
import, its preferred-slot bot allocation, the dedicated sound stubs, and the
elementary-action, AI-state, bot-library and navigation-query surfaces. The ET
client and default-release gates are unchanged; ET remains experimental and
dedicated-only behind `-Dwolfet-development=true`.

Interoperability facts come from the pinned original
[ET game imports](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/game/g_public.h),
[trap wrappers](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/game/g_syscalls.c),
[server system calls](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_game.c),
[temporary bans](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_ccmds.c),
[binary message transmission](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_net_chan.c),
[bot clients](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_bot.c),
[dedicated sound stubs](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/null/null_snddma.c)
and the ET bot-library headers. The reference is GPL-3.0-or-later with
additional terms. No implementation text is incorporated into fnTech3.

## Coverage

ET declares 212 game imports. Before this work fnTech3 handled 52 and dropped
the rest with an explicit "Unsupported WolfET game import" error. It now handles
189. Of the 23 that remain unhandled, 8 reproduce refusals that retail ET makes
itself, so 15 are genuinely open.

| Group | State |
| --- | --- |
| Core server imports | Complete, including the imports the retail game module issues. |
| Elementary actions | Complete: 30 ordinals, with ET's `EA_Walk`, `EA_Prone` and the fifteenth `ACTION_PRONE` input bit. |
| AI character, chat, goal, movement and weapon state | Complete except the two match-record imports below. |
| Bot-library lifetime and world updates | Complete. |
| AAS queries | Complete for the queries this engine's bot library exports. |
| ET-specific navigation extensions | Open: this bot library has no `AAS_AreaCenter`, `AAS_AreaWaypoint`, `AAS_AreaLadder`, `AAS_RT_ShowRoute`, `AAS_NearestHideArea`, `AAS_ListAreasInRange`, `AAS_AvoidDangerArea`, `AAS_Retreat`, `AAS_AlternativeRouteGoals` in ET's form, or `AAS_RecordTeamDeathArea`. |
| Client movement prediction | Open: ET's `aas_clientmove_t` embeds a `bsp_trace_t` this engine does not produce. |
| Match records | Open: ET's `bot_match_t` stores raw module pointers where this engine stores offsets. |
| `BOTLIB_TEST`, `BOTLIB_AI_INIT_AVOID_REACH` | Open: no corresponding export in this bot library. |

Retail ET itself has no case for `BOTLIB_AAS_ENTITY_VISIBLE`,
`BOTLIB_AAS_IN_FIELD_OF_VISION`, `BOTLIB_AAS_VISIBLE_CLIENTS`,
`BOTLIB_AI_SET_AVOID_GOAL_TIME`, `BOTLIB_AI_ADD_AVOID_SPOT`,
`BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL`, `BOTLIB_AAS_PREDICT_ROUTE` or
`BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX`; it drops with "Bad game system
trap". fnTech3 keeps refusing them and a regression pins that list, so a later
change cannot quietly invent behaviour retail ET never had.

## Contracts

**Drop client.** ET's import takes a third temporary-ban length in seconds that
Quake's two-argument import lacks. Routing it through the shared case silently
discarded the ban; ET now drops the client and, for a nonzero length, records
the address. Challenge and connection requests from a banned address are
refused with `sv_tempbanmessage` before any other connection work, matching
retail ET. Only the ET profile consults the list.

Retail ET stores `svs.time + length * 1000` for a free slot but the unscaled
`length` when every slot still holds an unexpired ban, so an evicted slot
expires a thousand times sooner. fnTech3 stores the requested duration in both
paths and refuses nonpositive or unrepresentable durations instead of
overflowing. No module import can observe a ban duration, so this changes no
module or wire contract; it is recorded here as a deliberate deviation.

**Binary messages.** `G_SENDMESSAGE` stores one pending payload of at most
32768 bytes per client, replacing any previous one. `SV_Netchan_Transmit`
appends it after `svc_EOF` as byte-aligned uncompressed data, exactly as retail
ET does, and clears it. A payload that no longer fits is kept and reported as
overflowed rather than truncated. `G_MESSAGESTATUS` returns empty, waiting or
waiting-overflow, and `qfalse` for a client outside the running server. The
storage is engine-internal ET state; it does not widen `client_t` for the other
four profiles.

**Bot clients.** ET's `G_BOT_ALLOCATE_CLIENT` carries a requested slot. A
positive request is used only when that slot is free; zero or negative scans
from the profile's first bot slot. The ET profile reserves slot zero and uses
retail ET's 16384-byte rate, which the profile catalog previously left at zero.

**Sound.** A dedicated ET server has no sound system: `G_REGISTERSOUND` returns
no handle and `G_GET_SOUND_LENGTH` drops, as ET's own dedicated stubs do. ET
client startup remains gated, so this is the only reachable path.

**Action bits.** ET's external action flags are RTCW's thirteen plus reload and
prone. They are translated to distinct engine bits rather than copied, unknown
module bits survive a reset/get round trip, and non-finite scalars or vectors
are refused before the bot library sees them. A new engine-internal
`ACTION_WOLFET_PRONE` bit carries prone; `ACTION_WALK` carries ET's walk import,
because the bot export has no `EA_Walk` entry.

**Chat.** ET's `BotSetChatName` has no client argument and its `BotEnterChat`
carries the speaking client, unlike Quake's. Two engine-internal entry points
implement that contract without changing the frozen bot export, and ET's
150-byte message limit is honoured where this engine uses 256: replace-synonyms
and unify-white-spaces round-trip through engine-sized storage and return at
most ET's limit, and console messages are narrowed and terminated rather than
copied from engine storage.

**Records.** ET's bot goal carries `urgency` and `goalEndTime` that this
engine's goal has no field for, and its movement setup adds `areanum`. Goals
therefore round-trip through ET's own 64-byte record, preserving both values
that the module stored. ET's entity state, movement result and weapon
information match the engine's layouts and are pinned with static assertions.

**Diagnostics.** The shared native-argument helpers now name the active profile
in their error text instead of always reporting RTCW.

## Verification

Clean Windows x86 RelWithDebInfo build with `-Dwolfet-development=true` and
`-Drtcw-development=true`; no errors or warnings.

New executable regressions compile the ET server state and action adapter
directly and check temporary-ban duration, expiry, address matching, slot
eviction and rejection of nonpositive or unrepresentable durations; binary
message queueing, replacement, overflow reporting, clearing, full-size
acceptance, oversize refusal and out-of-range client handling; and ET action-bit
translation including prone, unknown-bit preservation, distinct engine bit
assignment and non-finite refusal. Source regressions pin the 66 imports the
retail game module can issue, the 8 ordinals retail ET refuses, that ET's
drop-client import does not reach the two-argument shared case, and that the
binary message is written after `svc_EOF`.

Renaming the profile-neutral bot input helpers required matching updates in the
existing RTCW probe fixtures; those tests still pass unchanged otherwise.

Two installed runtime probes repeat with the rebuilt server. The bounded
loopback dedicated probe loads `goldrush` with the retail `qagame_mp_x86.dll`,
answers a protocol-84 info query and exits with status 0 in 5.6 seconds. The
retail-client probe, which exercises the connection path this work modified,
still reaches an active non-pure session, joins Axis and Allies and leaves both
processes exiting normally in 55.3 seconds, with the installation unchanged:

| Artifact | SHA-256 |
| --- | --- |
| `fntech3.ded.exe` | `cc7a7decd7009f8d154c1696dd84ead4b8c9acb2b92dcba6ba00b4cb60b630a7` |
| retail `ET.exe` | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` |
| retail `qagame_mp_x86.dll` | `160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b` |

Neither probe exercises a temporary ban, a binary message or a bot-library
import; the retail module issues none of them in these runs.

## Remaining gates

- The 15 open imports above, each of which needs a bot-library export or a
  record conversion this engine does not yet have evidence for.
- No live ET bot session is claimed. The retail ET game module ships with
  `bot_enable 0` and issues none of the bot-library imports beyond allocation,
  input reset and the blocking-entity update, so this coverage is checked by
  source and executable regressions, not by gameplay.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded: UI/cgame, MDM/MDX rendering, demos, pure admission,
  downloads, gameplay and bidirectional interoperability are still open, and ET
  client startup is still gated.

## Unrelated observations

Retail ET's temporary-ban eviction path is inconsistent with its free-slot path,
as described above. fnTech3's bot library has no `AAS_AreaCenter`,
`AAS_AreaWaypoint`, `AAS_AreaLadder`, `AAS_RT_ShowRoute`, `AAS_NearestHideArea`,
`AAS_ListAreasInRange`, `AAS_AvoidDangerArea`, `AAS_Retreat` or
`AAS_RecordTeamDeathArea`; RTCW MP declares none of them either, so the gap is
not ET-specific. RTCW's own `BOTLIB_AI_ENTER_CHAT` and
`BOTLIB_AI_SET_CHAT_NAME` imports remain unhandled and would need the same
legacy chat contract now added for ET.
