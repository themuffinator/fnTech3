/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Shared renderer policy for game-owned blob/stencil/projection shadows. */
#ifndef TR_SHADOW_LEGACY_H
#define TR_SHADOW_LEGACY_H

static ID_INLINE void R_DisableLegacyShadowsForShadowMaps( const shadowManager_t *manager )
{
	/* A requested cvar or a planned pass does not establish GPU support or
	 * successful allocation. Only a world view that published a usable atlas
	 * replaces legacy shadows. This also covers cached atlas publications. */
	if ( !manager || !manager->planned || manager->noWorldModel || !r_shadows ||
		!r_shadows->integer ) {
		return;
	}
	if ( !manager->csmAtlasPublication.published &&
		!manager->pointAtlasPublication.published &&
		!manager->spotAtlasPublication.published ) {
		return;
	}

	ri.Cvar_Set( "cg_shadows", "0" );
	ri.Printf( PRINT_ALL, "Shadow mapping active: setting cg_shadows to 0.\n" );
}

#endif
