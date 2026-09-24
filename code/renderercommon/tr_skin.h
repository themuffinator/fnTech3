/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
/* Shared skin registration and independently authored RTCW metadata support. */
#ifndef FNTECH3_TR_SKIN_H
#define FNTECH3_TR_SKIN_H

#include "tr_q3_ihv_skin_register.h"

qhandle_t RE_RegisterSkin( const char *name ) {
	skinSurface_t parseSurfaces[MAX_SKIN_SURFACES];
	skinModel_t parseModels[MAX_SKIN_MODELS];
	const int rtcw = ri.gameId == COM_GAME_RTCW_SP || ri.gameId == COM_GAME_RTCW_MP;
	/* Enemy Territory attaches models the same way, but recognises a model or
	 * tag entry only by its prefix, and has no player scale. */
	const int wolfet = ri.gameId == COM_GAME_WOLFET;
	qhandle_t	hSkin;
	skin_t		*skin;
	skinSurface_t	*surf;
	union {
		char *c;
		void *v;
	} text = { NULL };
	const char	*text_p;
	const char	*token;
	char		surfName[MAX_QPATH];
	int			totalSurfaces;
	int totalModels = 0;
	size_t nameLength;
	int skinFile;

	if ( !name || !name[0] ) {
		ri.Printf( PRINT_DEVELOPER, "Empty name passed to RE_RegisterSkin\n" );
		return 0;
	}

	nameLength = strlen( name );
	if ( nameLength >= MAX_QPATH ) {
		ri.Printf( PRINT_DEVELOPER, "Skin name exceeds MAX_QPATH\n" );
		return 0;
	}
	if ( ri.gameId == COM_GAME_QUAKE3_IHV ) return R_RegisterIHVSkin( name, nameLength );


	/* Enemy Territory reads every skin name as a file; only Quake 3 made a
	 * shader of a name without the extension. */
	skinFile = wolfet || ( nameLength >= 5 && !strcmp( name + nameLength - 5, ".skin" ) );
	// see if the skin is already loaded
	for ( hSkin = 1; hSkin < tr.numSkins ; hSkin++ ) {
		skin = tr.skins[hSkin];
		if ( !Q_stricmp( skin->name, name ) ) {
			if ( !skin->registered ) {
				return 0;		// default skin
			}
			return hSkin;
		}
	}

	// allocate a new skin
	if ( tr.numSkins == MAX_SKINS ) {
		ri.Printf( PRINT_WARNING, "WARNING: RE_RegisterSkin( '%s' ) MAX_SKINS hit\n", name );
		return 0;
	}
	if ( skinFile ) {
		ri.FS_ReadFile( name, &text.v );
		/* RTCW retries missing files. Q3/QL retain their failed-name cache. */
		if ( !text.c && ( rtcw || wolfet ) ) return 0;
	}
	tr.numSkins++;
	skin = ri.Hunk_Alloc( sizeof( skin_t ), h_low );
	memset( skin, 0, sizeof( *skin ) );
	tr.skins[hSkin] = skin;
	Q_strncpyz( skin->name, name, sizeof( skin->name ) );
	skin->numSurfaces = 0;

	//R_IssuePendingRenderCommands();

	// If not a .skin file, load as a single shader
	if ( !skinFile ) {
		skin->numSurfaces = 1;
		skin->surfaces = ri.Hunk_Alloc( sizeof( skinSurface_t ), h_low );
		skin->surfaces[0].shader = R_FindShader( name, LIGHTMAP_NONE, qtrue );
		skin->registered = qtrue;
		return hSkin;
	}

	// load and parse the skin file
	if ( !text.c ) {
		return 0;
	}

	totalSurfaces = 0;
	text_p = text.c;
	while ( text_p && *text_p ) {
		// get surface name
		token = CommaParse( &text_p );
		Q_strncpyz( surfName, token, sizeof( surfName ) );

		if ( !token[0] ) {
			break;
		}
		// lowercase the surface name so skin compares are faster
		Q_strlwr( surfName );

		if ( *text_p == ',' ) {
			text_p++;
		}

		if ( wolfet ? !Q_stricmpn( token, "tag_", 4 ) : strstr( token, "tag_" ) != NULL ) {
			continue;
		}
		if ( ( rtcw && strstr( token, "md3_" ) ) || ( wolfet && !Q_stricmpn( token, "md3_", 4 ) ) ) {
			/* CommaParse reuses its token buffer; capture the key first. */
			char type[MAX_QPATH];
			Q_strncpyz( type, token, sizeof( type ) );
			token = CommaParse( &text_p );
			if ( skin->numModels < MAX_SKIN_MODELS ) {
				skinModel_t *model = &parseModels[skin->numModels++];
				Q_strncpyz( model->type, type, sizeof( model->type ) );
				Q_strncpyz( model->model, token, sizeof( model->model ) );
			}
			++totalModels;
			continue;
		}
		if ( rtcw && strstr( token, "playerscale" ) ) {
			volatile floatint_t scale;
			token = CommaParse( &text_p );
			/* Force a stored-bit check: finite-math assumptions can otherwise
			 * remove both isfinite and a memcpy bit test of the atof result. */
			scale.f = (float)atof( token );
			if ( ( scale.u & 0x7f800000u ) == 0x7f800000u ) {
				ri.FS_FreeFile( text.v );
				ri.Printf( PRINT_WARNING, "Invalid player scale in skin '%s'\n", name );
				return 0;
			}
			skin->scale[0] = skin->scale[1] = skin->scale[2] = scale.f;
			continue;
		}

		// parse the shader name
		token = CommaParse( &text_p );

		if ( skin->numSurfaces < MAX_SKIN_SURFACES ) {
			surf = &parseSurfaces[skin->numSurfaces];
			Q_strncpyz( surf->name, surfName, sizeof( surf->name ) );
			surf->shader = R_FindShader( token, LIGHTMAP_NONE, qtrue );
			skin->numSurfaces++;
		}

		totalSurfaces++;
	}

	ri.FS_FreeFile( text.v );
	if ( totalModels > MAX_SKIN_MODELS ) {
		ri.Printf( PRINT_WARNING, "Ignoring excess model references (found %d, max %d) in skin '%s'\n",
			totalModels, MAX_SKIN_MODELS, name );
	}

	if ( totalSurfaces > MAX_SKIN_SURFACES ) {
		ri.Printf( PRINT_WARNING, "WARNING: Ignoring excess surfaces (found %d, max is %d) in skin '%s'!\n",
					totalSurfaces, MAX_SKIN_SURFACES, name );
	}

	// never let a skin have 0 shaders
	if ( skin->numSurfaces == 0 && !( rtcw && strstr( name, "loper" ) && strstr( name, "upper" ) ) ) {
		return 0;		// use default skin
	}

	// copy surfaces to skin
	if ( skin->numSurfaces ) {
		skin->surfaces = ri.Hunk_Alloc( skin->numSurfaces * sizeof( skinSurface_t ), h_low );
		memcpy( skin->surfaces, parseSurfaces, skin->numSurfaces * sizeof( skinSurface_t ) );
	}
	if ( skin->numModels ) {
		skin->models = ri.Hunk_Alloc( skin->numModels * sizeof( skinModel_t ), h_low );
		memcpy( skin->models, parseModels, skin->numModels * sizeof( skinModel_t ) );
	}
	skin->registered = qtrue;

	return hSkin;
}

qboolean RE_GetSkinModel( qhandle_t handle, const char *type, char *name ) {
	const skin_t *skin;
	int i;
	char result[MAX_QPATH];
	if ( !type || !name || handle < 1 || handle >= tr.numSkins ) return qfalse;
	skin = tr.skins[handle];
	if ( !skin || !skin->registered ) return qfalse;
	/* Enemy Territory keeps no player scale in its skins. */
	if ( ri.gameId != COM_GAME_WOLFET && !Q_stricmp( type, "playerscale" ) ) {
		Com_sprintf( result, sizeof( result ), "%.2f %.2f %.2f", skin->scale[0], skin->scale[1], skin->scale[2] );
	} else {
		for ( i = 0; i < skin->numModels; ++i ) if ( !Q_stricmp( skin->models[i].type, type ) ) break;
		if ( i == skin->numModels ) return qfalse;
		Q_strncpyz( result, skin->models[i].model, sizeof( result ) );
	}
	/* No match leaves the output untouched. Capture aliased query strings first. */
	memcpy( name, result, strlen( result ) + 1 );
	return qtrue;
}

#endif
