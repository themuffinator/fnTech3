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

} // namespace fntech3::rtcw
