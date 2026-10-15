/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_JKA_RANGED_FOG_SHADE_H
#define FNTECH3_TR_JKA_RANGED_FOG_SHADE_H
#include "tr_jka_ranged_fog.h"

/* Included by each renderer's tr_shade_calc.c after tr_local.h. The fog volume
 * pass's distance scale and starting distance for one volume: its own unless
 * Academy's zoom fog is set and this is the volume covering the whole world,
 * the only one retail's ranged fog touches (jamp.exe 0x4aeda4..0x4aedb8). */
static void RB_JKARangedFog( const fog_t *fog, float *scale, float *start ) {
	float distance;
	*scale = fog->tcScale;
	*start = 0.0f;
	if ( ri.gameId != COM_GAME_JKA_MP || !tr.world || fog->originalBrushNumber != -1
		|| tr.world->jkaRangedFog == 0.0f || !RE_GetJKADistanceCull( &distance ) ) return;
	FnJKA_RangedFogScale( tr.world->jkaRangedFog, fog->parms.depthForOpaque, distance, scale, start );
}
#endif
