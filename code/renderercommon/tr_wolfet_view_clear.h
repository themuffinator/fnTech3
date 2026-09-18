/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_VIEW_CLEAR_H
#define FNTECH3_TR_WOLFET_VIEW_CLEAR_H

/* Enemy Territory clears every world view to the colour of the map's global
 * fog, before any other rule it has for clearing. A sky shader may then have
 * no stages at all and still show the fog, rather than whatever the previous
 * frame left there. The fog volume's colour already carries the identity
 * light, as ET's clear does, and follows any module retuning. Included by a
 * renderer's back end. */
static qboolean RB_WolfETGlobalFogClearColor( vec4_t color ) {
	const fog_t *fog;
	int index;
	if ( ri.gameId != COM_GAME_WOLFET || !tr.world || ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		return qfalse;
	}
	index = tr.world->wolfetGlobalFog.index;
	if ( index <= 0 || index >= tr.world->numfogs ) {
		return qfalse;
	}
	fog = &tr.world->fogs[index];
	color[0] = Com_Clamp( 0, 1, fog->color[0] );
	color[1] = Com_Clamp( 0, 1, fog->color[1] );
	color[2] = Com_Clamp( 0, 1, fog->color[2] );
	color[3] = 1;
	return qtrue;
}

#endif
