// Runs the engine's own bot character and weight loaders outside the engine,
// over files in a directory, under Quake III's or Enemy Territory's rules.
// Every path the loaders ask the filesystem for is printed, so a test can see
// where an #include was looked for. Used by wolfet_bot_character_tests.py.
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/botlib/botlib.h"
#include "code/botlib/be_interface.h"
#include "code/botlib/l_script.h"
#include "code/botlib/l_precomp.h"
#include "code/botlib/l_libvar.h"
#include "code/botlib/be_ai_char.h"
#include "code/botlib/be_ai_weight.h"
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

botlib_import_t botimport;
botlib_globals_t botlibglobals;
int bot_developer;

static std::string root;
static std::vector<std::FILE *> files(1, nullptr);
static int errors, wolfet;

// The one profile question the character loader asks.
int Com_GameIsWolfET( void ) { return wolfet; }

static void QDECL HostPrint( int type, const char *fmt, ... ) {
	char text[4096];
	va_list ap;
	va_start( ap, fmt );
	std::vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	if ( type == PRT_ERROR || type == PRT_FATAL || std::strstr( text, "rror:" ) ) ++errors;
	std::fprintf( stderr, "%s", text );
}
static void *HostGetMemory( int size ) { return std::calloc( 1, (size_t)size ); }
static void HostFreeMemory( void *ptr ) { std::free( ptr ); }
static int HostAvailableMemory( void ) { return 1 << 30; }
static void *HostHunkAlloc( int size ) { return std::calloc( 1, (size_t)size ); }
// A qpath's separators are the filesystem's to fold, as the engine's own
// lookup does, so each request is printed with forward slashes.
static int HostOpen( const char *qpath, fileHandle_t *handle, fsMode_t mode ) {
	std::string name = qpath;
	for ( char &c : name ) if ( c == '\\' ) c = '/';
	std::FILE *file = mode == FS_READ ? std::fopen( ( root + "/" + name ).c_str(), "rb" ) : nullptr;
	std::printf( "open %s %s\n", name.c_str(), file ? "found" : "missing" );
	if ( !file ) { if ( handle ) *handle = 0; return -1; }
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

// Everything else the loaders link against, answered minimally.
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
void QDECL Log_Write( const char *, ... ) {}

int main( int argc, char **argv ) {
	// usage: host <root> <wolfet 0|1> <character file> <skill> [weight file]
	if ( argc < 5 ) return 2;
	root = argv[1];
	wolfet = std::atoi( argv[2] );
	botimport.Print = HostPrint;
	botimport.GetMemory = HostGetMemory;
	botimport.FreeMemory = HostFreeMemory;
	botimport.AvailableMemory = HostAvailableMemory;
	botimport.HunkAlloc = HostHunkAlloc;
	botimport.FS_FOpenFile = HostOpen;
	botimport.FS_Read = HostRead;
	botimport.FS_FCloseFile = HostClose;
	botimport.Sys_Milliseconds = HostMilliseconds;
	const int character = BotLoadCharacter( argv[3], (float)std::atof( argv[4] ) );
	std::printf( "character %d\n", character );
	if ( character ) {
		char text[MAX_QPATH];
		for ( int index : { 0, 40 } ) {
			text[0] = 0;
			Characteristic_String( character, index, text, sizeof( text ) );
			std::printf( "string %d %s\n", index, text );
		}
		std::printf( "float 2 %g\n", Characteristic_Float( character, 2 ) );
		std::printf( "integer 3 %d\n", Characteristic_Integer( character, 3 ) );
	}
	if ( argc > 5 ) {
		// A weight file after the character: its loader sets its own folder.
		weightconfig_t *weights = ReadWeightConfig( argv[5] );
		std::printf( "weights %d\n", weights ? weights->numweights : -1 );
		for ( int i = 0; weights && i < weights->numweights; ++i ) std::printf( "weight %s\n", weights->weights[i].name );
		if ( weights ) FreeWeightConfig( weights );
	}
	BotShutdownCharacters();
	std::fprintf( stderr, "errors=%d\n", errors );
	return 0;
}
