/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_SKY_RENDER_H
#define FNTECH3_TR_Q3_IHV_SKY_RENDER_H
#include "tr_q3_ihv_sky.h"

/* One stream shared by IHV skies, independent of gameplay's random generator.
 * It lives for this renderer module's lifetime, rather than resetting per map. */
static uint32_t ihvSkySeed;

static void RB_PrepareQ3IHVSky( void ) {
	int stage;
	skyParms_t *sky = &tess.shader->sky;
	tess.ihvSkyActive = ( ri.gameId == COM_GAME_QUAKE3 || ri.gameId == COM_GAME_QUAKE3_IHV ) &&
		( sky->ihvClouds || sky->ihvLightning ) && sky->cloudHeight > 0 ? qtrue : qfalse;
	if ( !tess.ihvSkyActive ) return;
	for ( stage = 0; stage < MAX_SHADER_STAGES; ++stage ) {
		tess.ihvSkyColors[stage] = (byte)Q3IHV_SkyStageColor( &ihvSkySeed,
			&sky->ihvLastFlash, backEnd.refdef.time, sky->ihvLightning, stage );
		if ( !tess.xstages[stage] ) break;
	}
}

/* IHV cloud draws use the iterator's constant RGB and opaque alpha, including
 * stage zero, instead of evaluating the later renderer's color generators. */
qboolean RB_Q3IHVSkyColors( const shaderStage_t *pStage, byte *colors ) {
	int stage, vertex;
	if ( !tess.ihvSkyActive || ( ri.gameId != COM_GAME_QUAKE3 && ri.gameId != COM_GAME_QUAKE3_IHV ) ) return qfalse;
	for ( stage = 0; stage < MAX_SHADER_STAGES && tess.xstages[stage]; ++stage ) {
		if ( tess.xstages[stage] != pStage ) continue;
		for ( vertex = 0; vertex < tess.numVertexes; ++vertex, colors += 4 ) {
			colors[0] = colors[1] = colors[2] = tess.ihvSkyColors[stage];
			colors[3] = 255;
		}
		return qtrue;
	}
	return qfalse;
}
#endif
