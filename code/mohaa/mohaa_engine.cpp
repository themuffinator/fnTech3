/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* The engine services the vendored OpenMoHAA sources under code/mohaa call,
 * served by fnTech3 (see code/mohaa/qcommon/qcommon.h). */

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "mohaa_tiki.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {

/* Each block carries its size ahead of the caller's bytes, so the whole of
 * what the model cache holds can be reported. The header keeps the caller's
 * bytes at 16-byte alignment, as the zone's are at least 8. */
struct alignas( 16 ) BlockHeader {
	size_t size;
	int tag;
};

size_t liveBytes;
size_t liveBlocks;

void *Allocate( size_t size, int tag ) {
	void *raw = std::calloc( 1, sizeof( BlockHeader ) + size );
	if ( !raw ) {
		Com_Error( ERR_FATAL, "Medal of Honor model data: out of memory allocating %zu bytes", size );
	}
	BlockHeader *header = static_cast<BlockHeader *>( raw );
	header->size = size;
	header->tag = tag;
	liveBytes += size;
	liveBlocks++;
	return header + 1;
}

} // namespace

extern "C" {

void *MOHAA_TagMalloc( size_t size, int tag ) {
	return Allocate( size, tag );
}

void *MOHAA_Malloc( size_t size ) {
	return Allocate( size, 0 );
}

void MOHAA_Free( void *ptr ) {
	if ( !ptr ) {
		return;
	}
	BlockHeader *header = static_cast<BlockHeader *>( ptr ) - 1;
	liveBytes -= header->size;
	liveBlocks--;
	std::free( header );
}

int MOHAA_ReadFileEx( const char *qpath, void **buffer, qboolean quiet ) {
	const int length = FS_ReadFile( qpath, buffer );
	if ( length < 0 && !quiet ) {
		Com_DPrintf( "Medal of Honor model data: %s not found\n", qpath );
	}
	return length;
}

namespace {
void ( *loadResourceHook )( const char *name );
}

extern "C" void MOHAA_SetLoadResourceHook( void ( *hook )( const char *name ) ) {
	loadResourceHook = hook;
}

extern "C" void MOHAA_LoadResource( const char *name ) {
	if ( loadResourceHook && name && name[0] ) {
		loadResourceHook( name );
	}
}

void UI_LoadResource( const char *name ) {
	MOHAA_LoadResource( name );
}

const char *TIKI_MapName( void ) {
	const char *name = Cvar_VariableString( "mapname" );
	return name[0] ? name : "utils";
}

/* OpenMoHAA's FS_CanonicalFilename (code/qcommon/files.cpp,
 * GPL-2.0-or-later), which the retail game module is also handed. */
void FS_CanonicalFilename( char *filename ) {
	char *dest = filename;

	for ( const char *source = filename; *source; source++ ) {
		switch ( *source ) {
		case '\t':
		case ' ':
			break;
		case '\n':
		case '\r':
			*filename = 0;
			break;
		case '/':
		case '\\':
			if ( dest == filename ) {
				*filename = 0;
				return;
			}
			if ( *( dest - 1 ) != '/' ) {
				*dest++ = '/';
			}
			break;
		default:
			*dest++ = static_cast<char>( std::tolower( static_cast<unsigned char>( *source ) ) );
			break;
		}
	}
	*dest = 0;
}

/* What the model cache holds, for its status command. */
void MOHAA_ModelMemory( size_t *bytes, size_t *blocks ) {
	*bytes = liveBytes;
	*blocks = liveBlocks;
}

} // extern "C"
