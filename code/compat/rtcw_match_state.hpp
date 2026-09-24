// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rtcw_game_abi.h"

namespace fntech3::rtcw {

// Independently expressed transition contract observed in RTCW MP's
// SV_TransitionGameState/SV_CheckTransitionGameState. See the campaign ledger.
// Enemy Territory keeps these rules but plays single-player and cooperative
// restarts straight through, and resolves a reset to warmup alone.
inline bool RestartMatchState(int current, int requested, bool lockedTeams, int &result,
	bool intermissionToWarmup = true) {
	if ( current < FNTECH3_RTCW_MP_GS_INITIALIZE || current > FNTECH3_RTCW_MP_GS_RESET ||
		requested < FNTECH3_RTCW_MP_GS_PLAYING || requested > FNTECH3_RTCW_MP_GS_RESET ) return false;
	if ( intermissionToWarmup && current == FNTECH3_RTCW_MP_GS_INTERMISSION && requested == FNTECH3_RTCW_MP_GS_PLAYING )
		requested = FNTECH3_RTCW_MP_GS_WARMUP;
	// Rows: initialize, playing, countdown, warmup, intermission, waiting, reset.
	// Bits identify accepted requested states; reset is resolved after validation.
	constexpr unsigned accepted[] = { 63, 63, 61, 59, 4, 4, 20 };
	if ( !(accepted[current + 1] & (1u << requested)) ) return false;
	result = requested == FNTECH3_RTCW_MP_GS_RESET
		? (lockedTeams ? FNTECH3_RTCW_MP_GS_WAITING_FOR_PLAYERS : FNTECH3_RTCW_MP_GS_WARMUP)
		: requested;
	return true;
}

// Independently expressed game-type check observed in retail WolfMP.exe's
// SV_MapRestart_f (0x432f22..0x432f7c). A map forbids game types through its
// world record's worldflags. A forbidden type becomes objective, or checkpoint
// when the map forbids objective as well, even if it forbids checkpoint too.
// `allowed` is written only when the requested type is forbidden.
inline bool MapForbidsGameType(int gameType, int worldFlags, int &allowed) {
	const bool forbidden =
		( gameType == FNTECH3_RTCW_MP_GT_WOLF && ( worldFlags & FNTECH3_RTCW_MP_WORLD_NO_GT_WOLF ) ) ||
		( gameType == FNTECH3_RTCW_MP_GT_WOLF_STOPWATCH && ( worldFlags & FNTECH3_RTCW_MP_WORLD_NO_STOPWATCH ) ) ||
		( ( gameType == FNTECH3_RTCW_MP_GT_WOLF_CP || gameType == FNTECH3_RTCW_MP_GT_WOLF_CPH ) &&
			( worldFlags & FNTECH3_RTCW_MP_WORLD_NO_CHECKPOINT ) );
	if ( !forbidden ) return false;
	allowed = ( worldFlags & FNTECH3_RTCW_MP_WORLD_NO_GT_WOLF ) ? FNTECH3_RTCW_MP_GT_WOLF_CP : FNTECH3_RTCW_MP_GT_WOLF;
	return true;
}

} // namespace fntech3::rtcw
