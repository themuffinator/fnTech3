// Runs the engine's own script precompiler outside the engine, reading files
// from a directory, so a menu set can be preprocessed and its token stream
// inspected. Used by the ET menu regression and by the retail menu probe.
#include "code/qcommon/q_shared.h"
#include "code/botlib/botlib.h"
#include "code/botlib/be_interface.h"
#include "code/botlib/l_script.h"
#include "code/botlib/l_precomp.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

botlib_import_t botimport;
botlib_globals_t botlibglobals;
int bot_developer;

static std::string root;
static std::vector<std::FILE *> files(1, nullptr);
static int errors, warnings;

static void QDECL HostPrint( int type, const char *fmt, ... ) {
	char text[4096];
	va_list ap;
	va_start( ap, fmt );
	std::vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	if ( type == PRT_ERROR || type == PRT_FATAL || std::strstr( text, "rror:" ) ) ++errors;
	if ( type == PRT_WARNING || std::strstr( text, "arning:" ) ) ++warnings;
	std::fprintf( stderr, "%s", text );
}
static void *HostGetMemory( int size ) { return std::calloc( 1, (size_t)size ); }
static void HostFreeMemory( void *ptr ) { std::free( ptr ); }
static int HostAvailableMemory( void ) { return 1 << 30; }
static void *HostHunkAlloc( int size ) { return std::calloc( 1, (size_t)size ); }
static int HostOpen( const char *qpath, fileHandle_t *handle, fsMode_t mode ) {
	std::string path = root + "/" + qpath;
	std::FILE *file = std::fopen( path.c_str(), "rb" );
	if ( mode != FS_READ || !file ) { if ( handle ) *handle = 0; if ( file ) std::fclose( file ); return -1; }
	std::fseek( file, 0, SEEK_END );
	const long length = std::ftell( file );
	std::fseek( file, 0, SEEK_SET );
	files.push_back( file );
	*handle = (fileHandle_t)( files.size() - 1 );
	return (int)length;
}
static int HostRead( void *buffer, int len, fileHandle_t handle ) {
	return (int)std::fread( buffer, 1, (size_t)len, files[handle] );
}
static void HostClose( fileHandle_t handle ) {
	if ( handle > 0 && (size_t)handle < files.size() && files[handle] ) {
		std::fclose( files[handle] ); files[handle] = nullptr;
	}
}
static int HostMilliseconds( void ) { return 0; }

// Everything else the precompiler links against, answered minimally.
void Q_strncpyz( char *dest, const char *src, int destsize ) {
	std::strncpy( dest, src, (size_t)destsize - 1 ); dest[destsize - 1] = 0;
}
int QDECL Com_sprintf( char *dest, int size, const char *fmt, ... ) {
	va_list ap; va_start( ap, fmt ); const int n = std::vsnprintf( dest, (size_t)size, fmt, ap ); va_end( ap );
	return n;
}
void QDECL Com_Error( errorParm_t, const char *fmt, ... ) {
	va_list ap; va_start( ap, fmt ); std::vfprintf( stderr, fmt, ap ); va_end( ap ); std::exit( 3 );
}
void QDECL Com_Printf( const char *fmt, ... ) {
	va_list ap; va_start( ap, fmt ); std::vfprintf( stderr, fmt, ap ); va_end( ap );
}

#ifndef Q_vsnprintf
int Q_vsnprintf( char *str, size_t size, const char *format, va_list ap ) {
	return std::vsnprintf( str, size, format, ap );
}
#endif
void Q_strcat( char *dest, int size, const char *src ) {
	const size_t used = std::strlen( dest );
	if ( used + 1 < (size_t)size ) Q_strncpyz( dest + used, src, size - (int)used );
}
int Q_stricmp( const char *s1, const char *s2 ) {
	for ( ;; ++s1, ++s2 ) {
		const int a = std::tolower( (unsigned char)*s1 ), b = std::tolower( (unsigned char)*s2 );
		if ( a != b ) return a < b ? -1 : 1;
		if ( !a ) return 0;
	}
}

int main( int argc, char **argv ) {
	// usage: host <root> <wolfet 0|1> <dump 0|1> <file> [define ...]
	if ( argc < 5 ) return 2;
	root = argv[1];
	const int wolfet = std::atoi( argv[2] ), dump = std::atoi( argv[3] );
	botimport.Print = HostPrint;
	botimport.GetMemory = HostGetMemory;
	botimport.FreeMemory = HostFreeMemory;
	botimport.AvailableMemory = HostAvailableMemory;
	botimport.HunkAlloc = HostHunkAlloc;
	botimport.FS_FOpenFile = HostOpen;
	botimport.FS_Read = HostRead;
	botimport.FS_FCloseFile = HostClose;
	botimport.Sys_Milliseconds = HostMilliseconds;
	for ( int i = 5; i < argc; ++i ) PC_AddGlobalDefine( argv[i] );
	const int handle = wolfet ? PC_LoadWolfETSourceHandle( argv[4] ) : PC_LoadSourceHandle( argv[4] );
	if ( !handle ) { std::fprintf( stderr, "cannot load %s\n", argv[4] ); return 1; }
	pc_token_t token;
	long count = 0;
	while ( PC_ReadTokenHandle( handle, &token ) ) {
		++count;
		if ( dump ) std::printf( "%s\n", token.string );
	}
	PC_FreeSourceHandle( handle );
	std::fprintf( stderr, "tokens=%ld errors=%d warnings=%d\n", count, errors, warnings );
	return errors ? 1 : 0;
}
