/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* Medal of Honor's module memory, from the installation's system86.dll; see
 * mohaa_system86.h. MemoryMalloc and MemoryFree are plain cdecl functions of
 * one argument each (system86.dll 0x10001190, 0x10001500). */

#include "mohaa_system86.h"
#include "../qcommon/qcommon.h"

#include <cstdlib>
#include <cstring>

#if defined( _WIN32 )
#include <windows.h>
#endif

namespace {

typedef void *( *MemoryMalloc_t )( int size );
typedef void ( *MemoryFree_t )( void *block );

MemoryMalloc_t memoryMalloc;
MemoryFree_t memoryFree;
bool tried;

} // namespace

extern "C" {

qboolean MOHAA_LoadSystem86( void ) {
	if ( memoryMalloc && memoryFree ) {
		return qtrue;
	}
	if ( tried ) {
		return qfalse;
	}
	tried = true;
#if defined( _WIN32 )
	HMODULE library = GetModuleHandleA( "system86.dll" );
	if ( !library ) {
		char path[MAX_OSPATH];
		Com_sprintf( path, sizeof( path ), "%s%csystem86.dll", Cvar_VariableString( "fs_basepath" ), PATH_SEP );
		library = LoadLibraryA( path );
		if ( !library ) {
			Com_Printf( S_COLOR_YELLOW "Medal of Honor: %s could not be loaded (error %lu); the retail "
				"modules import their allocator from it\n", path, static_cast<unsigned long>( GetLastError() ) );
			return qfalse;
		}
		Com_DPrintf( "Medal of Honor: loaded the installation's %s\n", path );
	}
	memoryMalloc = reinterpret_cast<MemoryMalloc_t>( GetProcAddress( library, "MemoryMalloc" ) );
	memoryFree = reinterpret_cast<MemoryFree_t>( GetProcAddress( library, "MemoryFree" ) );
	if ( !memoryMalloc || !memoryFree ) {
		memoryMalloc = nullptr;
		memoryFree = nullptr;
		Com_Printf( S_COLOR_YELLOW "Medal of Honor: system86.dll has no MemoryMalloc or MemoryFree\n" );
		return qfalse;
	}
	return qtrue;
#else
	return qfalse;
#endif
}

void *MOHAA_ModuleMalloc( int size ) {
	if ( size < 0 ) {
		return nullptr;
	}
	void *block;
	if ( memoryMalloc || MOHAA_LoadSystem86() ) {
		block = memoryMalloc( size ? size : 1 );
		if ( block ) {
			std::memset( block, 0, static_cast<size_t>( size ) );
		}
	} else {
		block = std::calloc( 1, size ? static_cast<size_t>( size ) : 1 );
	}
	if ( !block ) {
		Com_Error( ERR_DROP, "Medal of Honor: out of module memory for %i bytes", size );
	}
	return block;
}

void MOHAA_ModuleFree( void *block ) {
	if ( !block ) {
		return;
	}
	if ( memoryFree ) {
		memoryFree( block );
	} else {
		std::free( block );
	}
}

int MOHAA_ModuleReadFile( const char *path, void **buffer ) {
	if ( buffer ) {
		*buffer = nullptr;
	}
	if ( !path ) {
		return -1;
	}
	if ( !buffer ) {
		return FS_ReadFile( path, nullptr );
	}
	void *file = nullptr;
	const int length = FS_ReadFile( path, &file );
	if ( length < 0 || !file ) {
		return -1;
	}
	char *copy = static_cast<char *>( MOHAA_ModuleMalloc( length + 1 ) );
	std::memcpy( copy, file, static_cast<size_t>( length ) );
	copy[length] = '\0';
	FS_FreeFile( file );
	*buffer = copy;
	return length;
}

} // extern "C"
