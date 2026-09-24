/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_ENTITY_VIEW_H
#define FNTECH3_TR_Q3_IHV_ENTITY_VIEW_H

/* The saved primary camera is used even during a portal pass (0x42bb41).
 * Keep the frame-owned entity: queued draw commands retain this exact pose. */
static qboolean R_Q3IHVEntityView( trRefEntity_t *entity, int portalView, const float primaryAxis[3][3] ) {
	if ( ri.gameId != COM_GAME_QUAKE3_IHV || !entity->e.q3IHV.active ) return qtrue;
	if ( !Q3IHV_EntityVisible( entity->e.q3IHV.flags, portalView ) ) return qfalse;
	if ( entity->e.q3IHV.flags & Q3IHV_ENTITY_FACE_VIEW )
		memcpy( entity->e.axis, primaryAxis, sizeof( entity->e.axis ) );
	return qtrue;
}

#endif
