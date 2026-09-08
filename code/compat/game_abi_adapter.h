/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_COMPAT_GAME_ABI_ADAPTER_H
#define FNTECH3_COMPAT_GAME_ABI_ADAPTER_H

#include "../qcommon/q_shared.h"
#include "q3_game_abi.h"
#include "ql_game_abi.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Translate every source-known Q3 public cvar bit by meaning, never by its
 * colliding numeric value. Unknown bits are deliberately discarded. */
uint32_t FnTech3_Q3CvarFlagsToInternal( uint32_t q3Flags );

void FnTech3_Q3GameStateFromInternal( fntech3_q3_gameState_t *destination,
	const gameState_t *source );
void FnTech3_QLGameStateFromInternal( fntech3_ql_gameState_t *destination,
	const gameState_t *source );

void FnTech3_QLUsercmdFromInternal( fntech3_ql_usercmd_t *destination,
	const usercmd_t *source );
void FnTech3_InternalUsercmdFromQL( usercmd_t *destination,
	const fntech3_ql_usercmd_t *source );

void FnTech3_Q3UsercmdFromInternal( fntech3_q3_usercmd_t *destination,
	const usercmd_t *source );
void FnTech3_InternalUsercmdFromQ3( usercmd_t *destination,
	const fntech3_q3_usercmd_t *source );

void FnTech3_Q3TrajectoryFromInternal( fntech3_q3_trajectory_t *destination,
	const trajectory_t *source );
void FnTech3_InternalTrajectoryFromQ3( trajectory_t *destination,
	const fntech3_q3_trajectory_t *source );

void FnTech3_Q3EntityStateFromInternal( fntech3_q3_entityState_t *destination,
	const entityState_t *source );
void FnTech3_InternalEntityStateFromQ3( entityState_t *destination,
	const fntech3_q3_entityState_t *source );

void FnTech3_Q3PlayerStateFromInternal( fntech3_q3_playerState_t *destination,
	const playerState_t *source );
void FnTech3_InternalPlayerStateFromQ3( playerState_t *destination,
	const fntech3_q3_playerState_t *source );

/* Marshal only the source-known QL fields. Unknown native player-state bytes
 * are deterministic zero; they are not inferred to be padding. */
void FnTech3_QLEntityStateFromInternal( fntech3_ql_entityState_t *destination,
	const entityState_t *source );
void FnTech3_QLSnapshotPlayerStateFromInternal(
	uint8_t destination[FNTECH3_QL_RETAIL_SNAPSHOT_PLAYERSTATE_BYTES],
	const playerState_t *source );

#ifdef __cplusplus
}
#endif

#endif /* FNTECH3_COMPAT_GAME_ABI_ADAPTER_H */
