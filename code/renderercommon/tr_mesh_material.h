/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MESH_MATERIAL_H
#define FNTECH3_TR_MESH_MATERIAL_H

/* Heads can be MD3/MDC attachments as well as skeletal MDS surfaces. */
static inline int R_MeshSurfaceHidden( const refEntity_t *entity, const char *name ) {
	return ri.gameId == COM_GAME_RTCW_SP && !entity->rtcw.blink && !Q_stricmp( name, "h_blink" );
}

static shader_t *R_MeshSkinShader( const refEntity_t *entity, const skin_t *skin, const char *name ) {
	int i;
	if ( ri.gameId == COM_GAME_RTCW_MP && entity->rtcw.blink ) {
		char blinkName[MAX_QPATH + 2];
		Com_sprintf( blinkName, sizeof( blinkName ), "%s_b", name );
		for ( i = 0; i < skin->numSurfaces; ++i ) {
			if ( !strcmp( skin->surfaces[i].name, blinkName ) ) {
				if ( skin->surfaces[i].shader != tr.defaultShader ) return skin->surfaces[i].shader;
				break;
			}
		}
	}
	for ( i = 0; i < skin->numSurfaces; ++i )
		if ( !strcmp( skin->surfaces[i].name, name ) ) return skin->surfaces[i].shader;
	return tr.defaultShader;
}

/* IHV's skin is an ordered shader list, selected by model surface ordinal.
 * Names and entity skinNum do not participate (original 0x441f56..0x441f67).
 * Native resource pointers are resolved to this owned skin before rendering. */
static shader_t *R_MeshSkinShaderAt( const refEntity_t *entity, const skin_t *skin, const char *name, int ordinal ) {
	if ( ri.gameId == COM_GAME_QUAKE3_IHV ) {
		if ( ordinal < 0 || skin->numSurfaces <= 0 || !skin->surfaces ) return tr.defaultShader;
		return skin->surfaces[ordinal % skin->numSurfaces].shader;
	}
	return R_MeshSkinShader( entity, skin, name );
}

#endif
