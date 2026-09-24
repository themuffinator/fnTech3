/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_FOG_RENDER_H
#define FNTECH3_TR_Q3_IHV_FOG_RENDER_H

qboolean RB_Q3IHVFog( int fogNum ) {
	return ( ri.gameId == COM_GAME_QUAKE3 || ri.gameId == COM_GAME_QUAKE3_IHV ) && tr.world && tr.world->fogs &&
		fogNum > 0 && fogNum < tr.world->numfogs && tr.world->fogs[fogNum].parms.ihv.enabled ? qtrue : qfalse;
}

/* Shared by the fog overlay and all transparent-stage attenuation paths. */
void RB_CalcFogFactors( float *factors ) {
	int i;
	if ( RB_Q3IHVFog( tess.fogNum ) ) {
		const fog_t *fog = tr.world->fogs + tess.fogNum;
		double plane[4];
		const float depth = Q3IHV_FogDepth( &fog->parms.ihv, fog->parms.depthForOpaque, backEnd.refdef.time );
		Q3IHV_FogPlane( fog->parms.ihv.gradient, depth, fog->bounds[0][2], fog->bounds[1][2],
			backEnd.or.origin, backEnd.or.axis, plane );
		for ( i = 0; i < tess.numVertexes; ++i )
			factors[i] = Q3IHV_FogFactor( plane, backEnd.or.viewOrigin, tess.xyz[i] );
	} else {
		float texCoords[SHADER_MAX_VERTEXES][2];
		RB_CalcFogTexCoords( texCoords[0] );
		for ( i = 0; i < tess.numVertexes; ++i ) factors[i] = R_FogFactor( texCoords[i][0], texCoords[i][1] );
	}
}

void RB_Q3IHVFogColors( byte *colors, float *texCoords ) {
	const fog_t *fog = tr.world->fogs + tess.fogNum;
	float factors[SHADER_MAX_VERTEXES];
	int i;
	RB_CalcFogFactors( factors );
	for ( i = 0; i < tess.numVertexes; ++i, colors += 4, texCoords += 2 ) {
		memcpy( colors, fog->colorInt.rgba, 3 );
		colors[3] = (byte)( 255.0f * factors[i] );
		texCoords[0] = texCoords[1] = 0;
	}
}
#endif
