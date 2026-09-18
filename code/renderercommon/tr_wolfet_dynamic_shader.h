/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_DYNAMIC_SHADER_H
#define FNTECH3_TR_WOLFET_DYNAMIC_SHADER_H

/* Enemy Territory lets its cgame define a shader at runtime instead of shipping
 * it in a .shader file, and looks those definitions up before the loaded shader
 * text. The list is engine-owned: the module's text is copied in, so nothing
 * here retains module memory, and the list is emptied when the renderer stops.
 *
 * Included by each renderer's shader file, after its own parser helpers. */
#define FNTECH3_WOLFET_MAX_DYNAMIC_SHADERS 1024
#define FNTECH3_WOLFET_MAX_DYNAMIC_SHADER_BYTES 16384

typedef struct wolfetDynamicShader_s {
	struct wolfetDynamicShader_s *next;
	char *text;
} wolfetDynamicShader_t;

static wolfetDynamicShader_t *wolfetDynamicShaders;
static int wolfetDynamicShaderCount;

/* The name a definition declares is its first token, exactly as ET reads it. */
static qboolean R_WolfETDynamicShaderIsNamed( const char *text, const char *name ) {
	const char *cursor = text;
	const char *token;
	if ( !text || !name ) {
		return qfalse;
	}
	token = COM_ParseExt( &cursor, qtrue );
	return ( token[0] != 0 && !Q_stricmp( token, name ) ) ? qtrue : qfalse;
}

static void R_WolfETClearDynamicShaders( void ) {
	wolfetDynamicShader_t *entry = wolfetDynamicShaders;
	while ( entry ) {
		wolfetDynamicShader_t *next = entry->next;
		ri.Free( entry->text );
		ri.Free( entry );
		entry = next;
	}
	wolfetDynamicShaders = NULL;
	wolfetDynamicShaderCount = 0;
}

/* Returns the body of a runtime definition, positioned past its name, or NULL.
 * Called before the loaded shader text is searched, as ET searches it. */
static const char *R_WolfETFindDynamicShader( const char *name ) {
	const wolfetDynamicShader_t *entry;
	if ( ri.gameId != COM_GAME_WOLFET || !name ) {
		return NULL;
	}
	for ( entry = wolfetDynamicShaders; entry; entry = entry->next ) {
		const char *cursor = entry->text;
		const char *token;
		if ( !entry->text || !entry->text[0] ) {
			continue;
		}
		token = COM_ParseExt( &cursor, qtrue );
		if ( token[0] != 0 && !Q_stricmp( token, name ) ) {
			return cursor;
		}
	}
	return NULL;
}

qboolean RE_LoadWolfETDynamicShader( const char *name, const char *text ) {
	wolfetDynamicShader_t *entry, *previous, *added;
	size_t bytes;

	if ( ri.gameId != COM_GAME_WOLFET ) {
		ri.Error( ERR_DROP, "ET dynamic shader submitted outside its game profile" );
		return qfalse;
	}
	/* Text with no name has nothing to be found by. */
	if ( !name && text ) {
		ri.Printf( PRINT_WARNING, "RE_LoadDynamicShader: shader text with no name\n" );
		return qfalse;
	}
	/* Neither argument empties the whole list, as ET's does. */
	if ( !name && !text ) {
		R_WolfETClearDynamicShaders();
		return qtrue;
	}
	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_WARNING, "RE_LoadDynamicShader: name exceeds MAX_QPATH\n" );
		return qfalse;
	}

	previous = NULL;
	for ( entry = wolfetDynamicShaders; entry; entry = entry->next ) {
		if ( R_WolfETDynamicShaderIsNamed( entry->text, name ) ) {
			/* A name with no text removes that definition. */
			if ( !text ) {
				if ( previous ) {
					previous->next = entry->next;
				} else {
					wolfetDynamicShaders = entry->next;
				}
				ri.Free( entry->text );
				ri.Free( entry );
				wolfetDynamicShaderCount--;
				return qtrue;
			}
			/* ET refuses to redefine a name that is already present. */
			ri.Printf( PRINT_WARNING, "RE_LoadDynamicShader: shader %s already exists\n", name );
			return qfalse;
		}
		previous = entry;
	}

	if ( !text || !text[0] ) {
		ri.Printf( PRINT_WARNING, "RE_LoadDynamicShader: new shader %s has no text\n", name );
		return qfalse;
	}
	bytes = strlen( text ) + 1;
	if ( bytes > FNTECH3_WOLFET_MAX_DYNAMIC_SHADER_BYTES ||
		wolfetDynamicShaderCount >= FNTECH3_WOLFET_MAX_DYNAMIC_SHADERS ) {
		ri.Printf( PRINT_WARNING, "RE_LoadDynamicShader: %s exceeds the runtime shader limits\n", name );
		return qfalse;
	}
	/* The definition must declare the name it was submitted under, or nothing
	 * would ever find it. */
	if ( !R_WolfETDynamicShaderIsNamed( text, name ) ) {
		ri.Printf( PRINT_WARNING, "RE_LoadDynamicShader: %s does not declare that name\n", name );
		return qfalse;
	}

	added = (wolfetDynamicShader_t *)ri.Malloc( sizeof( *added ) );
	if ( !added ) {
		return qfalse;
	}
	added->next = NULL;
	added->text = (char *)ri.Malloc( (int)bytes );
	if ( !added->text ) {
		ri.Free( added );
		return qfalse;
	}
	Com_Memcpy( added->text, text, bytes );
	if ( previous ) {
		previous->next = added;
	} else {
		wolfetDynamicShaders = added;
	}
	wolfetDynamicShaderCount++;
	return qtrue;
}

#endif
