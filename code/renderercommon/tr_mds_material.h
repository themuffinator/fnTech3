/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDS_MATERIAL_H
#define FNTECH3_TR_MDS_MATERIAL_H

static inline int R_MdsSurfaceHidden( const refEntity_t *entity, const mdsMeshSurface_t *surface ) {
	return ri.gameId == COM_GAME_RTCW_SP && !entity->rtcw.blink && !Q_stricmp( surface->name, "h_blink" );
}

static shader_t *R_MdsSurfaceShader( const refEntity_t *entity, const mdsMeshSurface_t *surface ) {
	if ( entity->customShader ) return R_GetShaderByHandle( entity->customShader );
	if ( entity->customSkin > 0 && entity->customSkin < tr.numSkins ) {
		const skin_t *skin = R_GetSkinByHandle( entity->customSkin );
		int i;
		if ( ri.gameId == COM_GAME_RTCW_MP && entity->rtcw.blink ) {
			char blinkName[MAX_QPATH + 2];
			Com_sprintf( blinkName, sizeof( blinkName ), "%s_b", surface->name );
			for ( i = 0; i < skin->numSurfaces; ++i ) {
				if ( !strcmp( skin->surfaces[i].name, blinkName ) ) {
					if ( skin->surfaces[i].shader != tr.defaultShader ) return skin->surfaces[i].shader;
					break;
				}
			}
		}
		for ( i = 0; i < skin->numSurfaces; ++i )
			if ( !strcmp( skin->surfaces[i].name, surface->name ) ) return skin->surfaces[i].shader;
		return tr.defaultShader;
	}
	return R_GetShaderByHandle( surface->shader );
}

#endif
