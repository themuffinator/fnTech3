/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_SKIN_REGISTER_H
#define FNTECH3_TR_Q3_IHV_SKIN_REGISTER_H

#include "tr_q3_ihv_skin.h"

/* Only the immutable q3-ihv profile selects these registration rules. The
 * additional internal slot preserves 1024 original registrations beside the
 * engine's reserved fallback skin; other profiles keep their existing limit. */
static qhandle_t R_RegisterIHVSkin( const char *name, size_t nameLength ) {
	qhandle_t handle;
	skin_t *skin;
	fntech3IhvSkinNames_t parsed;
	void *buffer = NULL;
	int bytes, i;
	for ( handle = 1; handle < tr.numSkins; ++handle ) {
		skin = tr.skins[handle];
		if ( !strcmp( skin->name, name ) ) return skin->registered ? handle : 0;
	}
	if ( tr.numSkins >= MAX_SKINS + 1 ) {
		ri.Printf( PRINT_WARNING, "IHV skin registration limit reached\n" );
		return 0;
	}
	skin = ri.Hunk_Alloc( sizeof( *skin ), h_low );
	memset( skin, 0, sizeof( *skin ) );
	Q_strncpyz( skin->name, name, sizeof( skin->name ) );
	tr.skins[handle] = skin;
	++tr.numSkins;
	if ( nameLength < 5 || strcmp( name + nameLength - 5, ".skin" ) ) {
		skin->surfaces = ri.Hunk_Alloc( sizeof( *skin->surfaces ), h_low );
		memset( skin->surfaces, 0, sizeof( *skin->surfaces ) );
		skin->surfaces[0].shader = R_FindShader( name, LIGHTMAP_NONE, qtrue );
		skin->numSurfaces = 1;
	} else {
		bytes = ri.FS_ReadFile( name, &buffer );
		memset( &parsed, 0, sizeof( parsed ) );
		if ( buffer && ( bytes < 0 || !FnQ3IHV_ParseSkin( (const char *)buffer, (size_t)bytes, &parsed ) ) ) {
			ri.FS_FreeFile( buffer );
			ri.Printf( PRINT_WARNING, "Invalid or oversized IHV skin '%s'\n", name );
			return 0;
		}
		if ( buffer ) ri.FS_FreeFile( buffer );
		skin->numSurfaces = parsed.count ? parsed.count : 1;
		skin->surfaces = ri.Hunk_Alloc( skin->numSurfaces * sizeof( *skin->surfaces ), h_low );
		memset( skin->surfaces, 0, skin->numSurfaces * sizeof( *skin->surfaces ) );
		if ( !parsed.count ) skin->surfaces[0].shader = tr.defaultShader;
		for ( i = 0; i < parsed.count; ++i ) skin->surfaces[i].shader = R_FindShader( parsed.names[i], LIGHTMAP_NONE, qtrue );
	}
	skin->registered = qtrue;
	return handle;
}

#endif
