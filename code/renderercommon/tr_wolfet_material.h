/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_MATERIAL_H
#define FNTECH3_TR_WOLFET_MATERIAL_H

/* Enemy Territory picks a skeletal surface's material from its own skin. A
 * blinking entity asks for the surface's name with a '_b' suffix first and
 * falls back to the plain name when the skin does not answer it. The other
 * Wolfenstein profile spells the same idea with its own entity record, so the
 * two are kept apart rather than sharing one gated helper. */

static inline shader_t *R_WolfETSkinShader( const refEntity_t *entity, const skin_t *skin,
	const char *name ) {
	int i;
	if ( entity->wolfet.blink ) {
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

static inline shader_t *R_WolfETSurfaceShader( const refEntity_t *entity,
	const wolfetMeshSurface_t *surface ) {
	if ( entity->customShader ) return R_GetShaderByHandle( entity->customShader );
	if ( entity->customSkin > 0 && entity->customSkin < tr.numSkins )
		return R_WolfETSkinShader( entity, R_GetSkinByHandle( entity->customSkin ), surface->name );
	return R_GetShaderByHandle( surface->shader );
}

#endif
