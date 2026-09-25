/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_MOHAA_GAME_HPP
#define FNTECH3_SERVER_MOHAA_GAME_HPP

/* Medal of Honor: Allied Assault's game module boundary.
 *
 * `main/gamex86.dll` exports GetGameAPI, which takes the engine's 167-slot
 * import table and returns its 41-member export record
 * (code/compat/mohaa_game_abi.h). Both libraries import their allocator
 * from `system86.dll`, which lies beside the retail executable in the
 * installation's root; the user's own copy is loaded from there before the
 * module, so the Windows loader finds it by name.
 *
 * Every import slot is filled. A slot the engine does not serve drops by
 * name and number when called, as Alice's boundary does, so the log names
 * the next thing the module needs rather than the module faulting on a null.
 *
 * Where a slot's service is a rule of 2015's engine -- configstring blocks,
 * model user counts, the order the level is spawned in -- it follows
 * OpenMoHAA's reading of that engine (code/server/sv_game.c and sv_init.c,
 * GPL-2.0-or-later), checked against the retail modules' own call sites. The
 * slot numbers, argument counts and record sizes are measured in the retail
 * libraries; see docs/fntech3/2026-09-24-mohaa-contracts.md.
 *
 * Included by sv_game.cpp, whose game data view it binds. */

#include "../compat/mohaa_game_abi.h"
#include "../compat/mohaa_savegame.hpp"
#include "../mohaa/mohaa_tiki.h"
#include "../mohaa/mohaa_system86.h"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined( _WIN32 )
#include <windows.h>
#endif
#if defined( _MSC_VER )
#include <intrin.h>
#endif

#ifndef DEDICATED
/* The local client's HUD elements, which a listen server's game draws on
 * (cl_mohaa_cgame.cpp). */
void CL_MOHAAHudDrawShader( int info, const char *name );
void CL_MOHAAHudDrawAlign( int info, int horizontal, int vertical );
void CL_MOHAAHudDrawRect( int info, int x, int y, int width, int height );
void CL_MOHAAHudDrawVirtualSize( int info, bool virtualScreen );
void CL_MOHAAHudDrawColor( int info, const float *color );
void CL_MOHAAHudDrawAlpha( int info, float alpha );
void CL_MOHAAHudDrawString( int info, const char *string );
void CL_MOHAAHudDrawFont( int info, const char *name );
/* Whether a sound the local client started for the game still plays on the
 * channel the game was told (cl_mohaa_sound.cpp). */
bool CL_MOHAASoundPlaying( int id, const char *name );
/* The local player's key bindings (cl_mohaa_cgame.cpp). */
int CL_MOHAAKeyStringToKeynum( const char *name );
const char *CL_MOHAAKeyBindString( int keynum );
void CL_MOHAAKeysForCommand( const char *command, int *key1, int *key2 );
#endif

namespace mohaa_game {

using fntech3::mohaa::GEntity;

enum ImportSlot : int {
#define FNTECH3_MOHAA_GAME_IMPORT( slot, name, evidence, sites ) GI_##name = slot,
#include "../compat/mohaa_game_imports.def"
#undef FNTECH3_MOHAA_GAME_IMPORT
};

const char *const kImportNames[FNTECH3_MOHAA_GAME_IMPORT_SLOTS] = {
#define FNTECH3_MOHAA_GAME_IMPORT( slot, name, evidence, sites ) #name,
#include "../compat/mohaa_game_imports.def"
#undef FNTECH3_MOHAA_GAME_IMPORT
};

enum ExportSlot : int {
#define FNTECH3_MOHAA_GAME_EXPORT( slot, name, kind, evidence ) GE_##name = slot,
#include "../compat/mohaa_game_exports.def"
#undef FNTECH3_MOHAA_GAME_EXPORT
	GE_COUNT
};
static_assert( GE_COUNT * 4 == FNTECH3_MOHAA_GAME_EXPORT_BYTES, "41 four-byte members" );

std::intptr_t imports[FNTECH3_MOHAA_GAME_IMPORT_SLOTS];
std::intptr_t unserved[FNTECH3_MOHAA_GAME_IMPORT_SLOTS];
std::intptr_t *exports; // the module's record, which it and the engine both write

template<int Slot>
intptr_t QDECL UnservedImport() {
	Com_Error( ERR_DROP,
		"Medal of Honor game module called import %i (%s), which fnTech3 does not "
		"serve. The module ran to this point; this call is the next thing it needs. "
		"See docs/fntech3/COMPATIBILITY.md.", Slot, kImportNames[Slot] );
	return 0;
}

template<std::size_t... Slots>
void FillUnserved( std::index_sequence<Slots...> ) {
	( ( unserved[Slots] = reinterpret_cast<std::intptr_t>( &UnservedImport<static_cast<int>( Slots )> ) ), ... );
	std::memcpy( imports, unserved, sizeof( imports ) );
}

int ServedImports() {
	int served = 0;
	for ( int i = 0; i < FNTECH3_MOHAA_GAME_IMPORT_SLOTS; i++ ) {
		served += imports[i] != unserved[i];
	}
	return served;
}

template<typename T>
T Export( int slot ) {
	return exports ? reinterpret_cast<T>( exports[slot] ) : nullptr;
}

/* ---- the module's error message ---------------------------------------- */

/* The module leaves a message in its export record instead of raising some
 * errors itself; 2015's engine checks it after the spawn and each settle
 * frame and drops with it (OpenMoHAA sv_init.c). The text is copied first,
 * because dropping unloads the module that owns it. */
void CheckErrorMessage() {
	if ( !exports ) {
		return;
	}
	const char *message = reinterpret_cast<const char *>( exports[GE_errorMessage] );
	if ( !message ) {
		return;
	}
	char text[MAX_STRING_CHARS];
	exports[GE_errorMessage] = 0;
	Q_strncpyz( text, message, sizeof( text ) );
	Com_Error( ERR_DROP, "%s", text );
}

/* ---- entities ----------------------------------------------------------- */

struct GameData {
	void *entities = nullptr;
	int entityCount = 0;
	int entityBytes = 0;
	void *clients = nullptr;
	int clientBytes = 0;
} gameData;

byte *EntityBase() {
	return exports ? reinterpret_cast<byte *>( exports[GE_gentities] ) : nullptr;
}

int EntityCapacity() {
	const std::intptr_t capacity = exports ? exports[GE_max_entities] : 0;
	return capacity > 0 && capacity <= MAX_GENTITIES ? (int)capacity : 0;
}

/* Slots 142 and 143: the fog's far plane and whether the level has a sky
 * portal, which the snapshot walk reads. */
int farPlane;
bool skyPortal;

/* The entity states the snapshot deltas are built on: each linked entity's
 * state when the level finished spawning (OpenMoHAA SV_CreateBaseline). */
fntech3::mohaa::EntityState baselines[FNTECH3_MOHAA_MAX_GENTITIES];
bool baselineUsed[FNTECH3_MOHAA_MAX_GENTITIES];

} // namespace mohaa_game

namespace mohaa_world {

using fntech3::mohaa::GEntity;

GEntity *Entity( int number ) {
	byte *base = mohaa_game::EntityBase();
	if ( !base || number < 0 || number >= mohaa_game::EntityCapacity() ) {
		return nullptr;
	}
	return reinterpret_cast<GEntity *>( base + (std::ptrdiff_t)number * FNTECH3_MOHAA_GENTITY_BYTES );
}

int EntityNumber( const void *entity ) {
	const byte *base = mohaa_game::EntityBase();
	if ( !entity || !base ) {
		return -1;
	}
	const std::ptrdiff_t delta = static_cast<const byte *>( entity ) - base;
	if ( delta < 0 || delta % FNTECH3_MOHAA_GENTITY_BYTES ) {
		return -1;
	}
	const std::ptrdiff_t index = delta / FNTECH3_MOHAA_GENTITY_BYTES;
	return index < mohaa_game::EntityCapacity() ? (int)index : -1;
}

/* Export 31, TIKI_Orientation: a bone's orientation on the entity's current
 * pose, relative to the entity (gamex86 0x310d9fe0), returned by value. */
typedef orientation_t ( QDECL *TIKIOrientation_t )( GEntity *edict, int num );

orientation_t TagOrientation( GEntity *ent, int bone ) {
	const auto orientation = mohaa_game::Export<TIKIOrientation_t>( mohaa_game::GE_TIKI_Orientation );
	if ( !orientation ) {
		orientation_t none;
		Com_Memset( &none, 0, sizeof( none ) );
		AxisClear( none.axis );
		return none;
	}
	return orientation( ent, bone );
}

} // namespace mohaa_world

#include "server_mohaa_world.hpp"
#include "server_mohaa_net.hpp"

namespace mohaa_game {

GEntity *EntityOrDrop( void *entity, const char *call ) {
	const int number = mohaa_world::EntityNumber( entity );
	if ( number < 0 ) {
		Com_Error( ERR_DROP, "Medal of Honor game module passed %s an entity outside the array it published", call );
		return nullptr;
	}
	return static_cast<GEntity *>( entity );
}

dtiki_t *EntityTiki( const GEntity *ent ) {
	return reinterpret_cast<dtiki_t *>( static_cast<std::uintptr_t>( ent->tiki ) );
}

/* ---- printing, errors, time, memory (slots 0..8) ------------------------ */

void QDECL Printf( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_Printf( "%s", text );
}

void QDECL DPrintf( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_DPrintf( "%s", text );
}

/* Slot 2, the second developer channel, printed at developer 2 as 2015's
 * engine keeps it (OpenMoHAA Com_DPrintf2). */
void QDECL DPrintf2( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list argptr;

	if ( Cvar_VariableIntegerValue( "developer" ) < 2 ) {
		return;
	}
	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_Printf( "%s", text );
}

void QDECL Error( int level, const char *fmt, ... ) {
	char text[MAX_STRING_CHARS];
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	/* 2015's levels are Quake III's: fatal ends the process, anything else
	 * drops the level. A module's fatal error is not the engine's, so it
	 * drops too, with its level named. */
	if ( level == ERR_FATAL ) {
		Com_Error( ERR_DROP, "Medal of Honor game fatal error: %s", text );
	}
	Com_Error( ERR_DROP, "%s", text );
}

int QDECL Milliseconds() {
	return Sys_Milliseconds();
}

/* Slot 6. Retail 1.00 converts game strings through the engine's
 * localisation table (global/localization.txt) before they reach a player;
 * the table is not loaded yet, so the English text passes unchanged. */
const char *QDECL LV_ConvertString( const char *string ) {
	return string;
}

/* Slots 7 and 8, and the file buffers of slot 17: the installation's own
 * allocator, from which 2015's engine serves all three, so that the game may
 * free through Free a block it took from system86.dll directly, or a file it
 * read (gamex86 0x3103d449: FS_ReadFile, Malloc, copy, then Free of the
 * file's buffer). See code/mohaa/mohaa_system86.h. */
void *QDECL Malloc( int size ) {
	if ( size < 0 ) {
		Com_Error( ERR_DROP, "Medal of Honor game module asked for %i bytes", size );
		return nullptr;
	}
	return MOHAA_ModuleMalloc( size );
}

void QDECL Free( void *block ) {
	MOHAA_ModuleFree( block );
}

/* ---- console variables and commands (slots 9..16) ------------------------ */

/* 2015's cvar flags are Quake III's up to 0x400; above that its engine keeps
 * its own (0x1000 sound latch, 0x2000 terrain latch, 0x4000 saved with the
 * game), which are not fnTech3's bits of the same value. Those are dropped
 * here, and the saved-game ones remembered for the savegame. */
constexpr int MOHAA_CVAR_SAVEGAME = 0x4000;
constexpr int kSharedCvarFlags = CVAR_ARCHIVE | CVAR_USERINFO | CVAR_SERVERINFO | CVAR_SYSTEMINFO
	| CVAR_INIT | CVAR_LATCH | CVAR_ROM | CVAR_TEMP | CVAR_CHEAT | CVAR_NORESTART;

std::vector<std::string> savegameCvars;

cvar_t *QDECL CvarGet( const char *name, const char *value, int flags ) {
	if ( !name || !value ) {
		Com_Error( ERR_DROP, "Medal of Honor game module asked for a cvar with no name" );
		return nullptr;
	}
	if ( flags & MOHAA_CVAR_SAVEGAME ) {
		bool known = false;
		for ( const std::string &saved : savegameCvars ) {
			known = known || !Q_stricmp( saved.c_str(), name );
		}
		if ( !known ) {
			savegameCvars.emplace_back( name );
		}
	}
	return Cvar_Get( name, value, flags & kSharedCvarFlags );
}

void QDECL CvarSet( const char *name, const char *value ) {
	if ( name && value ) {
		Cvar_Set( name, value );
	}
}

cvar_t *QDECL CvarSet2( const char *name, const char *value, qboolean force ) {
	if ( !name ) {
		return nullptr;
	}
	return Cvar_Set2( name, value, force ? qtrue : qfalse );
}

cvar_t *QDECL NextCvar( cvar_t *var ) {
	return var ? var->next : nullptr;
}

int QDECL Argc() {
	return Cmd_Argc();
}

const char *QDECL Argv( int arg ) {
	return Cmd_Argv( arg );
}

const char *QDECL Args() {
	return Cmd_ArgsFrom( 1 );
}

void ConsoleCommandHandler();

/* Slot 16 takes a name alone (gamex86 0x310e4330: one push per call), so the
 * engine routes the command back through the ConsoleCommand export. A name
 * the engine already knows keeps its engine handler. */
const char *commandProbe;
bool commandFound;

void CommandSeen( const char *name ) {
	if ( name && commandProbe && !Q_stricmp( name, commandProbe ) ) {
		commandFound = true;
	}
}

bool CommandExists( const char *name ) {
	commandProbe = name;
	commandFound = false;
	Cmd_CommandCompletion( CommandSeen );
	commandProbe = nullptr;
	return commandFound;
}

void QDECL AddCommand( const char *name ) {
	if ( name && name[0] && !CommandExists( name ) ) {
		Cmd_AddCommand( name, ConsoleCommandHandler );
	}
}

/* ---- files (slots 17..33) -------------------------------------------------- */

int QDECL ReadFile( const char *path, void **buffer, qboolean quiet ) {
	const int length = MOHAA_ModuleReadFile( path, buffer );
	if ( length < 0 && path && !quiet ) {
		Com_DPrintf( "Medal of Honor game: %s not found\n", path );
	}
	return length;
}

void QDECL FreeFile( void *buffer ) {
	MOHAA_ModuleFree( buffer );
}

int QDECL WriteFile( const char *path, const void *buffer, int size ) {
	if ( path && buffer && size >= 0 ) {
		FS_WriteFile( path, buffer, size );
		return size;
	}
	return 0;
}

fileHandle_t QDECL FOpenFileWrite( const char *path ) {
	return path ? FS_FOpenFileWrite( path ) : 0;
}

fileHandle_t QDECL FOpenFileAppend( const char *path ) {
	return path ? FS_FOpenFileAppend( path ) : 0;
}

int QDECL FOpenFile( const char *path, fileHandle_t *file, qboolean uniqueFILE ) {
	if ( !path || !file ) {
		return -1;
	}
	return FS_FOpenFileRead( path, file, uniqueFILE );
}

int QDECL FSWrite( const void *buffer, int length, fileHandle_t file ) {
	if ( !buffer || length <= 0 || !file ) {
		return 0;
	}
	return FS_Write( buffer, length, file );
}

int QDECL FSRead( void *buffer, int length, fileHandle_t file ) {
	if ( !buffer || length <= 0 || !file ) {
		return 0;
	}
	return FS_Read( buffer, length, file );
}

void QDECL FCloseFile( fileHandle_t file ) {
	if ( file ) {
		FS_FCloseFile( file );
	}
}

int QDECL FTell( fileHandle_t file ) {
	return file ? FS_FTell( file ) : 0;
}

/* 2015's origins are Quake III's: 0 current, 1 end, 2 start. */
int QDECL FSeek( fileHandle_t file, int offset, int origin ) {
	return file ? FS_Seek( file, offset, static_cast<fsOrigin_t>( origin ) ) : -1;
}

void QDECL FSFlush( fileHandle_t file ) {
	if ( file ) {
		FS_Flush( file );
	}
}

/* Slot 29: whether `source` is newer than `destination`, which the game asks
 * of a script and its compiled form. Neither exists outside a pak in a
 * retail installation, so the answer is no. */
int QDECL FileNewer( const char *source, const char *destination ) {
	(void)source;
	(void)destination;
	return 0;
}

char **QDECL ListFiles( const char *path, const char *extension, int *numFiles ) {
	int count = 0;
	char **list = FS_ListFiles( path ? path : "", extension ? extension : "", &count );
	if ( numFiles ) {
		*numFiles = count;
	}
	return list;
}

void QDECL FreeFileList( char **list ) {
	if ( list ) {
		FS_FreeFileList( list );
	}
}

/* Slot 33: where a save's files go: save/<folder>/<name>.<ext>, the folder
 * being the player's configuration's name (OpenMoHAA Com_GetArchiveFileName;
 * code/compat/mohaa_savegame.hpp). A name that could leave the folder gives
 * an empty path, which opens nothing. */
const char *QDECL GetArchiveFileName( const char *name, const char *extension ) {
	static char path[fntech3::mohaa::savegame::PathBytes];
	if ( !fntech3::mohaa::savegame::ArchivePath( Cvar_VariableString( "config" ), name, extension, path ) ) {
		Com_DPrintf( "Refused an archive name: %s.%s\n", name ? name : "", extension ? extension : "" );
	}
	return path;
}

void QDECL SendConsoleCommand( const char *text ) {
	if ( text ) {
		Cbuf_AddText( text );
	}
}

/* ---- clients (slots 36, 37, 59) ------------------------------------------ */

void QDECL SendServerCommand( int clientNum, const char *fmt, ... ) {
	char text[MAX_STRING_CHARS * 2];
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	if ( clientNum == -1 ) {
		SV_SendServerCommand( nullptr, "%s", text );
		return;
	}
	if ( clientNum < 0 || clientNum >= sv.maxclients ) {
		return;
	}
	SV_SendServerCommand( &SV_ClientForIndex( clientNum ), "%s", text );
}

void QDECL DropClient( int clientNum, const char *reason ) {
	if ( clientNum < 0 || clientNum >= sv.maxclients ) {
		return;
	}
	SV_DropClient( &SV_ClientForIndex( clientNum ), reason ? reason : "" );
}

void QDECL GetUserinfo( int index, char *buffer, int size ) {
	if ( !buffer || size < 1 ) {
		return;
	}
	if ( index < 0 || index >= sv.maxclients ) {
		buffer[0] = '\0';
		return;
	}
	Q_strncpyz( buffer, SV_ClientForIndex( index ).userinfo, size );
}

void QDECL SetUserinfo( int index, const char *value ) {
	if ( index < 0 || index >= sv.maxclients || !value ) {
		return;
	}
	Q_strncpyz( SV_ClientForIndex( index ).userinfo, value, sizeof( SV_ClientForIndex( index ).userinfo ) );
}

/* ---- client game messages (slots 38..55) ---------------------------------- */

/* The game writes client-game messages into per-client buffers selected by
 * SetClient and the broadcast calls, and the engine sends them after the next
 * snapshot (server_mohaa_net.hpp). Each writer puts its value in at the
 * width 2015's does (OpenMoHAA sv_game.c PF_MSG_*). */
void QDECL MsgWriteBits( int value, int bits ) { mohaa_net::CGMWrite( value, bits ); }
void QDECL MsgWriteByte( int value ) { mohaa_net::CGMWrite( value, 8 ); }
void QDECL MsgWriteShort( int value ) { mohaa_net::CGMWrite( value, 16 ); }
void QDECL MsgWriteLong( int value ) { mohaa_net::CGMWrite( value, 32 ); }

void QDECL MsgWriteFloat( float value ) {
	int bits;
	std::memcpy( &bits, &value, sizeof( bits ) );
	mohaa_net::CGMWrite( bits, 32 );
}

void QDECL MsgWriteString( const char *value ) {
	if ( !value ) {
		value = "";
	}
	const std::size_t length = strlen( value ) + 1; // the terminator too
	for ( std::size_t i = 0; i < length; i++ ) {
		mohaa_net::CGMWrite( value[i], 8 );
	}
}

void QDECL MsgWriteAngle8( float value ) {
	mohaa_net::CGMWrite( (int)( value * 256.0f / 360.0f ), 8 );
}

void QDECL MsgWriteAngle16( float value ) {
	mohaa_net::CGMWrite( ANGLE2SHORT( value ), 16 );
}

/* A coordinate in sixteenths, eighteen bits and a sign in the nineteenth. */
void QDECL MsgWriteCoord( float value ) {
	int packed = std::abs( (int)( value * 16.0f ) );
	if ( value < 0 ) {
		packed |= 262144;
	} else {
		packed &= ~262144;
	}
	mohaa_net::CGMWrite( packed, 19 );
}

void QDECL MsgWriteDir( const float *dir ) {
	vec3_t v;
	VectorCopy( dir, v );
	mohaa_net::CGMWrite( DirToByte( v ), 8 );
}

void QDECL MsgStartCGM( int type ) { mohaa_net::CGMStart( type ); }
void QDECL MsgEndCGM() { mohaa_net::CGMEnd(); }
void QDECL MsgSetClient( int client ) { mohaa_net::CGMSetClient( client ); }
void QDECL SetBroadcastVisible( const float *pos, const float *posB ) { mohaa_net::CGMBroadcastVisible( pos, posB ); }
/* OpenMoHAA notes that 2015's hearable broadcast reaches every client. */
void QDECL SetBroadcastHearable( const float *pos, const float *posB ) {
	(void)pos;
	(void)posB;
	mohaa_net::CGMBroadcastAll();
}
void QDECL SetBroadcastAll() { mohaa_net::CGMBroadcastAll(); }

/* ---- configstrings and indices (slots 56, 57, 76..78, 82) ---------------- */

void QDECL SetConfigstring( int index, const char *value ) {
	if ( index < 0 || index >= FNTECH3_MOHAA_MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "Medal of Honor game module set configstring %i, outside its %i", index,
			FNTECH3_MOHAA_MAX_CONFIGSTRINGS );
		return;
	}
	SV_SetConfigstring( index, value ? value : "" );
}

const char *QDECL GetConfigstring( int index ) {
	if ( index < 0 || index >= FNTECH3_MOHAA_MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "Medal of Honor game module read configstring %i, outside its %i", index,
			FNTECH3_MOHAA_MAX_CONFIGSTRINGS );
		return "";
	}
	return sv.configstrings[index] ? sv.configstrings[index] : "";
}

/* OpenMoHAA sv_init.c SV_FindIndex: the first free or matching slot of a
 * block, never slot 0, compared without case. */
int FindIndex( const char *name, int start, int max, bool create ) {
	if ( !name || !name[0] ) {
		return 0;
	}
	int i;
	for ( i = 1; i < max; i++ ) {
		const char *s = sv.configstrings[start + i];
		if ( !s || !s[0] ) {
			break;
		}
		if ( !Q_stricmp( s, name ) ) {
			return i;
		}
	}
	const int result = i;
	for ( ; i < max; i++ ) {
		const char *s = sv.configstrings[start + i];
		if ( s && !Q_stricmp( s, name ) ) {
			return i;
		}
	}
	if ( !create ) {
		return 0;
	}
	if ( result == max ) {
		Com_Error( ERR_DROP, "SV_FindIndex: overflow  max%d create%d  name %s", max, create ? 1 : 0, name );
		return 0;
	}
	SV_SetConfigstring( start + result, name );
	return result;
}

/* The item block follows the players' (cgamex86 reads CS_PLAYERS + 64 +
 * index, 0x3001eb89). */
constexpr int kCsWeapons = FNTECH3_MOHAA_CS_PLAYERS + FNTECH3_MOHAA_MAX_CLIENTS;
constexpr int kMaxWeapons = 64;

int ModelIndex( const char *name ) {
	return FindIndex( name, FNTECH3_MOHAA_CS_MODELS, FNTECH3_MOHAA_MAX_MODELS, true );
}

void ClearModel( int index ) {
	SV_SetConfigstring( FNTECH3_MOHAA_CS_MODELS + index, "" );
}

int QDECL ImageIndex( const char *name ) {
	return FindIndex( name, FNTECH3_MOHAA_CS_IMAGES, FNTECH3_MOHAA_MAX_IMAGES, true );
}

int QDECL ItemIndex( const char *name ) {
	return FindIndex( name, kCsWeapons, kMaxWeapons, true );
}

/* A sound's configstring carries whether it streams, as its last character
 * (OpenMoHAA SV_SoundIndex). */
int QDECL SoundIndex( const char *name, int streamed ) {
	char buffer[1024];
	if ( !name || !name[0] ) {
		return 0;
	}
	Com_sprintf( buffer, sizeof( buffer ), "%s%d", name, streamed );
	return FindIndex( buffer, FNTECH3_MOHAA_CS_SOUNDS, FNTECH3_MOHAA_MAX_SOUNDS, true );
}

void QDECL SetLightStyle( int index, const char *data ) {
	if ( index < 0 || index >= FNTECH3_MOHAA_MAX_LIGHTSTYLES ) {
		Com_Error( ERR_DROP, "Medal of Honor game module set light style %i", index );
		return;
	}
	SV_SetConfigstring( FNTECH3_MOHAA_CS_LIGHTSTYLES + index, data ? data : "" );
}

/* ---- models (slots 60, 79..81, 84, 85) ----------------------------------- */

/* How many things use each model index; an index nobody uses is cleared
 * (OpenMoHAA sv_game.c modelUserCount). */
int modelUserCount[FNTECH3_MOHAA_MAX_MODELS];
int usageIndex;

void QDECL SetBrushModel( void *entity, const char *name ) {
	GEntity *ent = EntityOrDrop( entity, "SetBrushModel" );
	if ( !ent ) {
		return;
	}
	if ( !name ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: NULL" );
		return;
	}
	if ( name[0] != '*' ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name );
		return;
	}

	ent->s.modelindex = atoi( name + 1 );
	vec3_t mins, maxs;
	CM_ModelBounds( CM_InlineModel( ent->s.modelindex ), mins, maxs );
	VectorCopy( mins, ent->r.mins );
	VectorCopy( maxs, ent->r.maxs );
	ent->r.bmodel = qtrue;
	if ( !ent->r.contents ) {
		ent->r.contents = -1; // we don't know exactly what is in the brushes
	}
	mohaa_world::LinkEntity( ent );
}

dtiki_t *QDECL RegisterTiki( const char *path ) {
	if ( !path ) {
		return nullptr;
	}
	const int index = ModelIndex( path );
	if ( index > 0 && index < FNTECH3_MOHAA_MAX_MODELS ) {
		modelUserCount[index]++;
	}
	return TIKI_RegisterTiki( path );
}

dtiki_t *QDECL ModelTiki( const char *name ) {
	return name ? TIKI_RegisterTikiFlags( name, qtrue ) : nullptr;
}

dtikianim_t *QDECL ModelTikiAnim( const char *name ) {
	return name ? TIKI_RegisterTikiAnimFlags( name, qtrue ) : nullptr;
}

qboolean QDECL SetModel( void *entity, const char *name ) {
	GEntity *ent = EntityOrDrop( entity, "setmodel" );
	if ( !ent || !name ) {
		return qfalse;
	}

	if ( ++usageIndex >= 65535 ) {
		usageIndex = 1;
	}
	ent->s.usageIndex = usageIndex;

	dtiki_t *tiki = nullptr;
	int newModelIndex = 0;
	if ( name[0] ) {
		tiki = ModelTiki( name );
		if ( !tiki ) {
			return qfalse;
		}
		newModelIndex = ModelIndex( name );
		if ( !newModelIndex ) {
			return qfalse;
		}
		// don't reference the model again
		if ( ent->s.modelindex == newModelIndex ) {
			return qtrue;
		}
	}

	if ( ent->tiki && ent->s.modelindex > 0 && ent->s.modelindex < FNTECH3_MOHAA_MAX_MODELS ) {
		if ( modelUserCount[ent->s.modelindex] > 0 && !--modelUserCount[ent->s.modelindex] ) {
			ClearModel( ent->s.modelindex );
		}
		ent->s.modelindex = 0;
	}

	ent->tiki = static_cast<std::uint32_t>( reinterpret_cast<std::uintptr_t>( tiki ) );
	ent->r.bmodel = qfalse;

	if ( tiki ) {
		ent->s.modelindex = newModelIndex;
		modelUserCount[newModelIndex]++;
	}
	return qtrue;
}

void QDECL ClearModelImport( void *entity ) {
	SetModel( entity, "" );
}

/* ---- world (slots 62..75) ------------------------------------------------- */

qboolean QDECL SightTraceEntity( void *entity, const float *start, const float *mins, const float *maxs,
	const float *end, int contentMask, qboolean cylinder ) {
	GEntity *ent = EntityOrDrop( entity, "SightTraceEntity" );
	if ( !ent ) {
		return qfalse;
	}
	return mohaa_world::SightTraceEntity( ent, start, mins, maxs, end, contentMask, cylinder );
}

qboolean QDECL SightTrace( const float *start, const float *mins, const float *maxs, const float *end,
	int passEntityNum, int passEntityNum2, int contentMask, qboolean cylinder ) {
	return mohaa_world::SightTrace( start, mins, maxs, end, passEntityNum, passEntityNum2, contentMask, cylinder );
}

void QDECL TraceImport( fntech3::mohaa::Trace *results, const float *start, const float *mins, const float *maxs,
	const float *end, int passEntityNum, int contentMask, qboolean cylinder, qboolean traceDeep ) {
	static const vec3_t zero = { 0.0f, 0.0f, 0.0f };
	if ( !results ) {
		return;
	}
	mohaa_world::DeepTrace trace;
	mohaa_world::Trace( &trace, start, mins ? mins : zero, maxs ? maxs : zero, end, passEntityNum, contentMask,
		cylinder, traceDeep );
	mohaa_world::ToModuleTrace( trace, results );
}

int QDECL PointContents( const float *point, int passEntityNum ) {
	return point ? mohaa_world::PointContents( point, passEntityNum ) : 0;
}

/* Slot 61: an inline model's bounds by its `*n` name; anything else has
 * none. The retail game never calls it. */
void QDECL ModelBoundsFromName( const char *name, float *mins, float *maxs ) {
	if ( !mins || !maxs ) {
		return;
	}
	VectorClear( mins );
	VectorClear( maxs );
	if ( name && name[0] == '*' ) {
		CM_ModelBounds( CM_InlineModel( atoi( name + 1 ) ), mins, maxs );
	}
}

/* Slot 65: a map shader's record by number. The retail game never calls it,
 * and fnTech3's collision keeps no record in 2015's shape to hand out. */
void *QDECL GetShader( int shaderNum ) {
	(void)shaderNum;
	return nullptr;
}

/* Slot 35: a value for the engine's debug graph, which has no display on a
 * dedicated server. */
void QDECL DebugGraph( float value ) {
	(void)value;
}

/* Slot 67: the brush a point is in. fnTech3's collision does not number
 * brushes for a caller; the retail game never calls it. */
int QDECL PointBrushnum( const float *point, clipHandle_t model ) {
	(void)point;
	(void)model;
	return -1;
}

void QDECL AdjustAreaPortalState( void *entity, qboolean open ) {
	GEntity *ent = EntityOrDrop( entity, "AdjustAreaPortalState" );
	if ( !ent ) {
		return;
	}
	svEntity_t *svEnt = mohaa_world::SvEntity( ent );
	if ( !svEnt || svEnt->areanum2 == -1 ) {
		return;
	}
	CM_AdjustAreaPortalState( svEnt->areanum, svEnt->areanum2, open ? qtrue : qfalse );
}

int QDECL AreaForPoint( const float *point ) {
	return point ? CM_LeafArea( CM_PointLeafnum( point ) ) : -1;
}

qboolean QDECL AreasConnected( int area1, int area2 ) {
	return CM_AreasConnected( area1, area2 );
}

qboolean QDECL InPVS( const float *p1, const float *p2 ) {
	int leafnum = CM_PointLeafnum( p1 );
	int cluster = CM_LeafCluster( leafnum );
	const int area1 = CM_LeafArea( leafnum );
	const byte *mask = CM_ClusterPVS( cluster );

	leafnum = CM_PointLeafnum( p2 );
	cluster = CM_LeafCluster( leafnum );
	const int area2 = CM_LeafArea( leafnum );
	if ( mask && !( mask[cluster >> 3] & ( 1 << ( cluster & 7 ) ) ) ) {
		return qfalse;
	}
	return CM_AreasConnected( area1, area2 ) ? qtrue : qfalse;
}

void QDECL LinkEntity( void *entity ) {
	GEntity *ent = EntityOrDrop( entity, "linkentity" );
	if ( ent ) {
		mohaa_world::LinkEntity( ent );
	}
}

void QDECL UnlinkEntity( void *entity ) {
	GEntity *ent = EntityOrDrop( entity, "unlinkentity" );
	if ( ent ) {
		mohaa_world::UnlinkEntity( ent );
	}
}

int QDECL AreaEntities( const float *mins, const float *maxs, int *list, int maxcount ) {
	if ( !mins || !maxs || !list || maxcount <= 0 ) {
		return 0;
	}
	return mohaa_world::AreaEntities( mins, maxs, list, maxcount );
}

void QDECL ClipToEntity( fntech3::mohaa::Trace *results, const float *start, const float *mins,
	const float *maxs, const float *end, int entityNum, int contentMask ) {
	static const vec3_t zero = { 0.0f, 0.0f, 0.0f };
	if ( !results ) {
		return;
	}
	mohaa_world::DeepTrace trace;
	mohaa_world::ClipToEntity( &trace.trace, start, mins ? mins : zero, maxs ? maxs : zero, end, entityNum,
		contentMask );
	mohaa_world::ToModuleTrace( trace, results );
}

/* ---- TIKI (slots 86..118, 124) --------------------------------------------- */

int QDECL NumAnims( dtiki_t *tiki ) { return tiki ? TIKI_NumAnims( tiki ) : 0; }
int QDECL NumSurfaces( dtiki_t *tiki ) { return tiki ? TIKI_NumSurfaces( tiki ) : 0; }
int QDECL NumTags( dtiki_t *tiki ) { return tiki ? TIKI_NumTags( tiki ) : 0; }

void QDECL CalculateBounds( dtiki_t *tiki, float scale, float *mins, float *maxs ) {
	if ( tiki && mins && maxs ) {
		TIKI_CalculateBounds( tiki, scale, mins, maxs );
	}
}

void *QDECL GetSkeletor( dtiki_t *tiki, int entNum ) {
	return tiki ? TIKI_GetSkeletor( tiki, entNum ) : nullptr;
}

const char *QDECL AnimNameForNum( dtiki_t *tiki, int anim ) { return tiki ? TIKI_Anim_NameForNum( tiki, anim ) : nullptr; }
int QDECL AnimNumForName( dtiki_t *tiki, const char *name ) { return tiki && name ? TIKI_Anim_NumForName( tiki, name ) : -1; }
int QDECL AnimRandom( dtiki_t *tiki, const char *name ) { return tiki && name ? TIKI_Anim_Random( tiki, name ) : -1; }
int QDECL AnimNumFrames( dtiki_t *tiki, int anim ) { return tiki ? TIKI_Anim_NumFrames( tiki, anim ) : 0; }
float QDECL AnimTime( dtiki_t *tiki, int anim ) { return tiki ? TIKI_Anim_Time( tiki, anim ) : 0.0f; }
float QDECL AnimFrametime( dtiki_t *tiki, int anim ) { return tiki ? TIKI_Anim_Frametime( tiki, anim ) : 0.0f; }
float QDECL AnimCrossTime( dtiki_t *tiki, int anim ) { return tiki ? TIKI_Anim_CrossblendTime( tiki, anim ) : 0.0f; }

void QDECL AnimDelta( dtiki_t *tiki, int anim, float *delta ) {
	if ( tiki && delta ) {
		TIKI_Anim_Delta( tiki, anim, delta );
	}
}

qboolean QDECL AnimHasDelta( dtiki_t *tiki, int anim ) { return tiki ? TIKI_Anim_HasDelta( tiki, anim ) : qfalse; }

void QDECL AnimDeltaOverTime( dtiki_t *tiki, int anim, float time1, float time2, float *delta ) {
	if ( tiki && delta ) {
		TIKI_Anim_DeltaOverTime( tiki, anim, time1, time2, delta );
	}
}

int QDECL AnimFlags( dtiki_t *tiki, int anim ) { return tiki ? TIKI_Anim_Flags( tiki, anim ) : 0; }
int QDECL AnimFlagsSkel( dtiki_t *tiki, int anim ) { return tiki ? TIKI_Anim_FlagsSkel( tiki, anim ) : 0; }
qboolean QDECL AnimHasCommands( dtiki_t *tiki, int anim ) { return tiki ? TIKI_Anim_HasServerCommands( tiki, anim ) : qfalse; }

int QDECL NumHeadModels( const char *model ) {
	dtikianim_t *tiki = ModelTikiAnim( model );
	return tiki ? TIKI_NumHeadModels( tiki ) : 0;
}

void QDECL GetHeadModel( const char *model, int num, char *name ) {
	dtikianim_t *tiki = ModelTikiAnim( model );
	if ( tiki && name ) {
		TIKI_GetHeadModel( tiki, num, name );
	}
}

int QDECL NumHeadSkins( const char *model ) {
	dtikianim_t *tiki = ModelTikiAnim( model );
	return tiki ? TIKI_NumHeadSkins( tiki ) : 0;
}

void QDECL GetHeadSkin( const char *model, int num, char *name ) {
	dtikianim_t *tiki = ModelTikiAnim( model );
	if ( tiki && name ) {
		TIKI_GetHeadSkin( tiki, num, name );
	}
}

qboolean QDECL FrameCommands( dtiki_t *tiki, int anim, int frame, tiki_cmd_t *commands ) {
	return tiki && commands ? TIKI_Frame_Commands_Server( tiki, anim, frame, commands ) : qfalse;
}

int QDECL SurfaceNameToNum( dtiki_t *tiki, const char *name ) { return tiki && name ? TIKI_Surface_NameToNum( tiki, name ) : -1; }
const char *QDECL SurfaceNumToName( dtiki_t *tiki, int num ) { return tiki ? TIKI_Surface_NumToName( tiki, num ) : nullptr; }
int QDECL TagNumForName( dtiki_t *tiki, const char *name ) { return tiki && name ? TIKI_Tag_NameToNum( tiki, name ) : -1; }
const char *QDECL TagNameForNum( dtiki_t *tiki, int num ) { return tiki ? TIKI_Tag_NumToName( tiki, num ) : nullptr; }

orientation_t QDECL OrientationInternal( dtiki_t *tiki, int entNum, int tagNum, float scale ) {
	if ( !tiki ) {
		orientation_t none;
		Com_Memset( &none, 0, sizeof( none ) );
		AxisClear( none.axis );
		return none;
	}
	return TIKI_OrientationInternal( tiki, entNum, tagNum, scale );
}

void *QDECL TransformInternal( dtiki_t *tiki, int entNum, int tagNum ) {
	return tiki ? MOHAA_TIKI_TransformInternal( tiki, entNum, tagNum ) : nullptr;
}

qboolean QDECL IsOnGroundInternal( dtiki_t *tiki, int entNum, int num, float threshold ) {
	return tiki ? TIKI_IsOnGroundInternal( tiki, entNum, num, threshold ) : qfalse;
}

void QDECL SetPoseInternal( dtiki_t *tiki, int entNum, const frameInfo_t *frameInfo, int *boneTag,
	vec4_t *boneQuat, float actionWeight ) {
	if ( !tiki ) {
		return;
	}
	TIKI_SetPoseInternal( TIKI_GetSkeletor( tiki, entNum ), frameInfo, boneTag, boneQuat, actionWeight );
}

const char *QDECL HitLocationInfo( int location, float *radius, float *offset ) {
	return radius && offset ? mohaa_world::GetHitLocationInfo( location, radius, offset ) : nullptr;
}

const char *QDECL HitLocationInfoSecondary( int location, float *radius, float *offset ) {
	return radius && offset ? mohaa_world::GetHitLocationInfoSecondary( location, radius, offset ) : nullptr;
}

const char *QDECL NameForNum( dtiki_t *tiki ) {
	return tiki ? TIKI_Name( tiki ) : nullptr;
}

/* ---- aliases (slots 119..128) --------------------------------------------- */

qboolean QDECL AliasAdd( dtiki_t *tiki, const char *alias, const char *name, const char *parameters ) {
	return MOHAA_TIKI_AliasAdd( tiki, alias, name, parameters );
}

const char *QDECL AliasFindRandom( dtiki_t *tiki, const char *alias, AliasListNode_t **ret ) {
	return MOHAA_TIKI_AliasFindRandom( tiki, alias, ret );
}

/* Also slot 122, a model's Alias_Clear: 2015's engine serves it with the
 * dump and clears nothing (OpenMoHAA sv_game.c PF_Alias_Clear), and no 1.00
 * module calls it. */
void QDECL AliasDump( dtiki_t *tiki ) {
	MOHAA_TIKI_AliasDump( tiki );
}

void QDECL AliasUpdateDialog( dtikianim_t *tiki, const char *alias ) {
	MOHAA_TIKI_AliasUpdateDialog( tiki, alias );
}

qboolean QDECL GlobalAliasAdd( const char *alias, const char *name, const char *parameters ) {
	return MOHAA_GlobalAliasAdd( alias, name, parameters );
}

const char *QDECL GlobalAliasFindRandom( const char *alias, AliasListNode_t **ret ) {
	return MOHAA_GlobalAliasFindRandom( alias, ret );
}

void QDECL GlobalAliasDump() {
	MOHAA_GlobalAliasDump();
}

void QDECL GlobalAliasClear() {
	MOHAA_GlobalAliasClear();
}

/* ---- prints to a player (slots 129, 130) --------------------------------- */

/* What a player is to be shown with the next snapshot, which 2015's engine
 * keeps in its client record (OpenMoHAA client_t stringToPrint). */
void QDECL CenterPrintf( void *entity, const char *fmt, ... ) {
	const int number = mohaa_world::EntityNumber( entity );
	if ( number < 0 || number >= sv.maxclients || number >= FNTECH3_MOHAA_MAX_CLIENTS || !fmt ) {
		return;
	}
	char text[2048];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	if ( strlen( text ) > 256 ) {
		Com_DPrintf( "Centerprint text exceeds buffer size\n" );
	}
	mohaa_net::QueuePrint( number, text, false, 0, 0 );
}

void QDECL LocationPrintf( void *entity, int x, int y, const char *fmt, ... ) {
	const int number = mohaa_world::EntityNumber( entity );
	if ( number < 0 || number >= sv.maxclients || number >= FNTECH3_MOHAA_MAX_CLIENTS || !fmt ) {
		return;
	}
	char text[2048];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	if ( strlen( text ) > 256 ) {
		Com_DPrintf( "Locationprint text exceeds buffer size\n" );
	}
	mohaa_net::QueuePrint( number, text, true, x, y );
}

/* ---- sounds (slots 131..135) ----------------------------------------------- */

/* A sound the server starts is carried to every active client in its next
 * snapshot, by its configstring index (OpenMoHAA sv_snd.c). */
void QDECL Sound( float ( *origin )[3], int entNum, int channel, const char *name, float volume,
	float minDist, float pitch, float maxDist, int streamed ) {
	if ( !name || !name[0] ) {
		return;
	}
	mohaa_net::QueueSound( origin ? *origin : nullptr, entNum, channel, SoundIndex( name, streamed ), volume,
		minDist, pitch, maxDist, streamed );
}

void QDECL StopSound( int entNum, int channel ) {
	mohaa_net::QueueStopSound( entNum, channel );
}

float QDECL SoundLength( int channel, const char *name ) {
	(void)channel;
	(void)name;
	return -1.0f;
}

unsigned char *QDECL SoundAmplitudes( int channel, const char *name ) {
	(void)channel;
	(void)name;
	return nullptr;
}

/* Slot 135: whether the local client still plays a sound the game waits on,
 * on the channel the client told it of (SV_MOHAASoundCallback, below): the
 * game asks each frame until it has ended, then wakes whatever waits on
 * `sounddone` (OpenMoHAA Entity::EventSoundDone, reference only). A
 * dedicated server has no client, and the game waits on sounds only in
 * single player. */
int QDECL IsSoundPlaying( int channel, const char *name ) {
#ifndef DEDICATED
	return CL_MOHAASoundPlaying( channel, name ) ? 1 : 0;
#else
	(void)channel;
	(void)name;
	return 0;
#endif
}

/* Slot 136: Quake III's CRC_Block, CRC-16/CCITT-FALSE, which Alice's engine
 * serves in the same slot of its own table. */
unsigned short QDECL CalcCRC( const unsigned char *start, int count ) {
	return fntech3::alice::savegame::CalcCRC( start, count );
}

/* ---- data slots (137..140, 166) ------------------------------------------- */

/* The game stores its own debug line and string arrays through these. */
void *debugLines;
int numDebugLines;
void *debugStrings;
int numDebugStrings;

/* ---- the level (slots 141..157) ---------------------------------------- */

void BindEntities();

void QDECL LocateGameData( void *entities, int entityCount, int entityBytes, void *clients, int clientBytes ) {
	if ( entityBytes != FNTECH3_MOHAA_GENTITY_BYTES ) {
		Com_Error( ERR_DROP, "Medal of Honor game module located %i-byte entities, not %i", entityBytes,
			FNTECH3_MOHAA_GENTITY_BYTES );
		return;
	}
	if ( clientBytes != FNTECH3_MOHAA_GCLIENT_BYTES ) {
		Com_Error( ERR_DROP, "Medal of Honor game module located %i-byte clients, not %i", clientBytes,
			FNTECH3_MOHAA_GCLIENT_BYTES );
		return;
	}
	const bool moved = entities != gameData.entities || clients != gameData.clients;
	gameData.entities = entities;
	gameData.entityCount = entityCount;
	gameData.entityBytes = entityBytes;
	gameData.clients = clients;
	gameData.clientBytes = clientBytes;
	if ( moved ) {
		Com_DPrintf( "Medal of Honor located game data: %i entities of %i bytes at %p, clients of %i bytes at %p\n",
			entityCount, entityBytes, entities, clientBytes, clients );
	}
	BindEntities();
}

void QDECL SetFarPlane( int value ) {
	farPlane = value ? ( value + 32 ) * ( value + 32 ) : 0;
}

void QDECL SetSkyPortal( qboolean value ) {
	skyPortal = value != qfalse;
}

/* Slots 144..149: menus on a player's screen, which 2015's engine sends as
 * console text for the client to run (OpenMoHAA PF_UI_*). */
void QDECL PopMenu( int client, int i ) {
	SendServerCommand( client, "stufftext \"popmenu %i\"\n", i );
}

void QDECL ShowMenu( int client, const char *name, qboolean force ) {
	SendServerCommand( client, "stufftext \"showmenu %s %i\"\n", name ? name : "", force ? 1 : 0 );
}

void QDECL HideMenu( int client, const char *name, qboolean force ) {
	SendServerCommand( client, "stufftext \"hidemenu %s %i\"\n", name ? name : "", force ? 1 : 0 );
}

void QDECL PushMenu( int client, const char *name ) {
	SendServerCommand( client, "stufftext \"pushmenu %s\"\n", name ? name : "" );
}

void QDECL HideMouseCursor( int client ) {
	SendServerCommand( client, "stufftext \"ui_hidemouse\"\n" );
}

void QDECL ShowMouseCursor( int client ) {
	SendServerCommand( client, "stufftext \"ui_showmouse\"\n" );
}

/* Slot 150: when the map's BSP was built, which the game prints with its
 * version. fnTech3's loader does not keep it. */
const char *QDECL MapTime() {
	return "";
}

/* Slots 151 and 152: the loading screen's progress. The game reports each
 * entity it starts and each step of its level's load; a listen server's
 * client counts them against the map's load record, and a dedicated server,
 * with no screen, sets no hook. */
void QDECL LoadResource( const char *name ) {
	MOHAA_LoadResource( name );
}

void QDECL ClearResource() {}

/* Slots 153..155: the local player's key bindings, from the client a listen
 * server runs beside; a dedicated server has none and answers nothing
 * (OpenMoHAA PF_Key_*, reference only). */
bool LocalClient() {
#ifndef DEDICATED
	return com_cl_running && com_cl_running->integer;
#else
	return false;
#endif
}

int QDECL KeyStringToKeynum( const char *str ) {
#ifndef DEDICATED
	if ( LocalClient() ) {
		return CL_MOHAAKeyStringToKeynum( str );
	}
#endif
	(void)str;
	return 0;
}

const char *QDECL KeyKeynumToBindString( int keynum ) {
#ifndef DEDICATED
	if ( LocalClient() ) {
		return CL_MOHAAKeyBindString( keynum );
	}
#endif
	(void)keynum;
	return nullptr;
}

void QDECL KeyGetKeysForCommand( const char *command, int *key1, int *key2 ) {
#ifndef DEDICATED
	if ( LocalClient() ) {
		CL_MOHAAKeysForCommand( command, key1, key2 );
		return;
	}
#endif
	(void)command;
	if ( key1 ) *key1 = 0;
	if ( key2 ) *key2 = 0;
}

namespace save {
void ArchiveClientState( bool reading );
}

/* Slot 156: while the game writes or reads a level archive, the engine puts
 * the local client's part into it (server_mohaa_save.hpp). A dedicated
 * server has no client state to keep. */
void QDECL ArchiveLevel( qboolean loading ) {
#ifndef DEDICATED
	save::ArchiveClientState( loading != qfalse );
#else
	(void)loading;
#endif
}

/* Slot 157: times the engine re-bases when a level is read back. */
std::vector<int *> svsTimeFixups;

void QDECL AddSvsTimeFixup( int *piTime ) {
	if ( piTime ) {
		svsTimeFixups.push_back( piTime );
	}
}

/* Slots 158..165: the game's HUD elements (`huddraw_*` in its scripts). In
 * 1.00 they are the listen server's: the engine writes them into the local
 * client's own array (OpenMoHAA SV_HudDraw*), so a dedicated server has
 * nowhere to put them. */
void QDECL HudDrawShader( int info, const char *name ) {
#ifndef DEDICATED
	CL_MOHAAHudDrawShader( info, name );
#else
	(void)info; (void)name;
#endif
}

void QDECL HudDrawAlign( int info, int h, int v ) {
#ifndef DEDICATED
	CL_MOHAAHudDrawAlign( info, h, v );
#else
	(void)info; (void)h; (void)v;
#endif
}

void QDECL HudDrawRect( int info, int x, int y, int w, int h ) {
#ifndef DEDICATED
	CL_MOHAAHudDrawRect( info, x, y, w, h );
#else
	(void)info; (void)x; (void)y; (void)w; (void)h;
#endif
}

void QDECL HudDrawVirtualSize( int info, qboolean virtualScreen ) {
#ifndef DEDICATED
	CL_MOHAAHudDrawVirtualSize( info, virtualScreen != qfalse );
#else
	(void)info; (void)virtualScreen;
#endif
}

void QDECL HudDrawColor( int info, float *color ) {
#ifndef DEDICATED
	CL_MOHAAHudDrawColor( info, color );
#else
	(void)info; (void)color;
#endif
}

void QDECL HudDrawAlpha( int info, float alpha ) {
#ifndef DEDICATED
	CL_MOHAAHudDrawAlpha( info, alpha );
#else
	(void)info; (void)alpha;
#endif
}

void QDECL HudDrawString( int info, const char *string ) {
#ifndef DEDICATED
	CL_MOHAAHudDrawString( info, string );
#else
	(void)info; (void)string;
#endif
}

void QDECL HudDrawFont( int info, const char *fontName ) {
#ifndef DEDICATED
	CL_MOHAAHudDrawFont( info, fontName );
#else
	(void)info; (void)fontName;
#endif
}

const char *QDECL GameDir() {
	return FS_GetCurrentGameDir();
}

/* ---- the game data view --------------------------------------------------- */

void BindEntities() {
	const int capacity = EntityCapacity();
	if ( !exports || !gameData.entities || capacity <= 0 ) {
		return;
	}
	const int clients = sv.maxclients > 0 ? sv.maxclients : FNTECH3_MOHAA_MAX_CLIENTS;
	std::intptr_t live = exports[GE_num_entities];
	if ( live <= 0 || live > capacity ) {
		live = gameData.entityCount;
	}
	const int count = live > 0 && live <= capacity ? (int)live : capacity;

	fntech3::server::NativeGameDataBinding binding;
	binding.entityBase = gameData.entities;
	binding.entityCount = count;
	binding.entityCapacity = capacity;
	binding.entityStride = (std::size_t)gameData.entityBytes;
	binding.entitySpanBytes = binding.entityStride * (std::size_t)capacity;
	binding.clientBase = gameData.clients;
	binding.clientCount = clients;
	binding.clientStride = (std::size_t)gameData.clientBytes;
	binding.clientSpanBytes = binding.clientStride * (std::size_t)clients;

	const fntech3::server::GameDataBindError error = serverGameData.BindNative( COM_GAME_MOHAA, binding );
	if ( error != fntech3::server::GameDataBindError::None ) {
		Com_Error( ERR_DROP, "Medal of Honor game data cannot be bound: %s",
			fntech3::server::GameDataBindErrorName( error ) );
		return;
	}

	sv.gentities = gameData.entities;
	sv.gentitySize = gameData.entityBytes;
	sv.num_entities = count;
	sv.gameClients = gameData.clients;
	sv.gameClientSize = gameData.clientBytes;
}

/* ---- the import table --------------------------------------------------------- */

void FillImports() {
	FillUnserved( std::make_index_sequence<FNTECH3_MOHAA_GAME_IMPORT_SLOTS>{} );

#define SERVE( member, fn ) imports[GI_##member] = reinterpret_cast<std::intptr_t>( &fn )
#define SERVE_DATA( member, address ) imports[GI_##member] = reinterpret_cast<std::intptr_t>( address )

	SERVE( Printf, Printf );
	SERVE( DPrintf, DPrintf );
	SERVE( DPrintf2, DPrintf2 );
	SERVE( DebugPrintf, DPrintf );
	SERVE( Error, Error );
	SERVE( Milliseconds, Milliseconds );
	SERVE( LV_ConvertString, LV_ConvertString );
	SERVE( Malloc, Malloc );
	SERVE( Free, Free );
	SERVE( Cvar_Get, CvarGet );
	SERVE( cvar_set, CvarSet );
	SERVE( cvar_set2, CvarSet2 );
	SERVE( NextCvar, NextCvar );
	SERVE( Argc, Argc );
	SERVE( Argv, Argv );
	SERVE( Args, Args );
	SERVE( AddCommand, AddCommand );
	SERVE( FS_ReadFile, ReadFile );
	SERVE( FS_FreeFile, FreeFile );
	SERVE( FS_WriteFile, WriteFile );
	SERVE( FS_FOpenFileWrite, FOpenFileWrite );
	SERVE( FS_FOpenFileAppend, FOpenFileAppend );
	SERVE( FS_FOpenFile, FOpenFile );
	SERVE( FS_Write, FSWrite );
	SERVE( FS_Read, FSRead );
	SERVE( FS_FCloseFile, FCloseFile );
	SERVE( FS_Tell, FTell );
	SERVE( FS_Seek, FSeek );
	SERVE( FS_Flush, FSFlush );
	SERVE( FS_FileNewer, FileNewer );
	SERVE( FS_CanonicalFilename, FS_CanonicalFilename );
	SERVE( FS_ListFiles, ListFiles );
	SERVE( FS_FreeFileList, FreeFileList );
	SERVE( GetArchiveFileName, GetArchiveFileName );
	SERVE( SendConsoleCommand, SendConsoleCommand );
	SERVE( DebugGraph, DebugGraph );
	SERVE( SendServerCommand, SendServerCommand );
	SERVE( DropClient, DropClient );
	SERVE( MSG_WriteBits, MsgWriteBits );
	SERVE( MSG_WriteChar, MsgWriteByte );
	SERVE( MSG_WriteByte, MsgWriteByte );
	SERVE( MSG_WriteSVC, MsgWriteByte );
	SERVE( MSG_WriteShort, MsgWriteShort );
	SERVE( MSG_WriteLong, MsgWriteLong );
	SERVE( MSG_WriteFloat, MsgWriteFloat );
	SERVE( MSG_WriteString, MsgWriteString );
	SERVE( MSG_WriteAngle8, MsgWriteAngle8 );
	SERVE( MSG_WriteAngle16, MsgWriteAngle16 );
	SERVE( MSG_WriteCoord, MsgWriteCoord );
	SERVE( MSG_WriteDir, MsgWriteDir );
	SERVE( MSG_StartCGM, MsgStartCGM );
	SERVE( MSG_EndCGM, MsgEndCGM );
	SERVE( MSG_SetClient, MsgSetClient );
	SERVE( SetBroadcastVisible, SetBroadcastVisible );
	SERVE( SetBroadcastHearable, SetBroadcastHearable );
	SERVE( SetBroadcastAll, SetBroadcastAll );
	SERVE( setConfigstring, SetConfigstring );
	SERVE( getConfigstring, GetConfigstring );
	SERVE( SetUserinfo, SetUserinfo );
	SERVE( GetUserinfo, GetUserinfo );
	SERVE( SetBrushModel, SetBrushModel );
	SERVE( ModelBoundsFromName, ModelBoundsFromName );
	SERVE( SightTraceEntity, SightTraceEntity );
	SERVE( SightTrace, SightTrace );
	SERVE( trace, TraceImport );
	SERVE( GetShader, GetShader );
	SERVE( pointcontents, PointContents );
	SERVE( PointBrushnum, PointBrushnum );
	SERVE( AdjustAreaPortalState, AdjustAreaPortalState );
	SERVE( AreaForPoint, AreaForPoint );
	SERVE( AreasConnected, AreasConnected );
	SERVE( InPVS, InPVS );
	SERVE( linkentity, LinkEntity );
	SERVE( unlinkentity, UnlinkEntity );
	SERVE( AreaEntities, AreaEntities );
	SERVE( ClipToEntity, ClipToEntity );
	SERVE( imageindex, ImageIndex );
	SERVE( itemindex, ItemIndex );
	SERVE( soundindex, SoundIndex );
	SERVE( TIKI_RegisterModel, RegisterTiki );
	SERVE( modeltiki, ModelTiki );
	SERVE( modeltikianim, ModelTikiAnim );
	SERVE( SetLightStyle, SetLightStyle );
	SERVE( GameDir, GameDir );
	SERVE( setmodel, SetModel );
	SERVE( clearmodel, ClearModelImport );
	SERVE( TIKI_NumAnims, NumAnims );
	SERVE( TIKI_NumSurfaces, NumSurfaces );
	SERVE( TIKI_NumTags, NumTags );
	SERVE( TIKI_CalculateBounds, CalculateBounds );
	SERVE( TIKI_GetSkeletor, GetSkeletor );
	SERVE( Anim_NameForNum, AnimNameForNum );
	SERVE( Anim_NumForName, AnimNumForName );
	SERVE( Anim_Random, AnimRandom );
	SERVE( Anim_NumFrames, AnimNumFrames );
	SERVE( Anim_Time, AnimTime );
	SERVE( Anim_Frametime, AnimFrametime );
	SERVE( Anim_CrossTime, AnimCrossTime );
	SERVE( Anim_Delta, AnimDelta );
	SERVE( Anim_HasDelta, AnimHasDelta );
	SERVE( Anim_DeltaOverTime, AnimDeltaOverTime );
	SERVE( Anim_Flags, AnimFlags );
	SERVE( Anim_FlagsSkel, AnimFlagsSkel );
	SERVE( Anim_HasCommands, AnimHasCommands );
	SERVE( NumHeadModels, NumHeadModels );
	SERVE( GetHeadModel, GetHeadModel );
	SERVE( NumHeadSkins, NumHeadSkins );
	SERVE( GetHeadSkin, GetHeadSkin );
	SERVE( Frame_Commands, FrameCommands );
	SERVE( Surface_NameToNum, SurfaceNameToNum );
	SERVE( Surface_NumToName, SurfaceNumToName );
	SERVE( Tag_NumForName, TagNumForName );
	SERVE( Tag_NameForNum, TagNameForNum );
	SERVE( TIKI_OrientationInternal, OrientationInternal );
	SERVE( TIKI_TransformInternal, TransformInternal );
	SERVE( TIKI_IsOnGroundInternal, IsOnGroundInternal );
	SERVE( TIKI_SetPoseInternal, SetPoseInternal );
	SERVE( CM_GetHitLocationInfo, HitLocationInfo );
	SERVE( CM_GetHitLocationInfoSecondary, HitLocationInfoSecondary );
	SERVE( Alias_Add, AliasAdd );
	SERVE( Alias_FindRandom, AliasFindRandom );
	SERVE( Alias_Dump, AliasDump );
	SERVE( Alias_Clear, AliasDump );
	SERVE( Alias_UpdateDialog, AliasUpdateDialog );
	SERVE( TIKI_NameForNum, NameForNum );
	SERVE( GlobalAlias_Add, GlobalAliasAdd );
	SERVE( GlobalAlias_FindRandom, GlobalAliasFindRandom );
	SERVE( GlobalAlias_Dump, GlobalAliasDump );
	SERVE( GlobalAlias_Clear, GlobalAliasClear );
	SERVE( centerprintf, CenterPrintf );
	SERVE( locationprintf, LocationPrintf );
	SERVE( Sound, Sound );
	SERVE( StopSound, StopSound );
	SERVE( SoundLength, SoundLength );
	SERVE( SoundAmplitudes, SoundAmplitudes );
	SERVE( S_IsSoundPlaying, IsSoundPlaying );
	SERVE( CalcCRC, CalcCRC );
	SERVE_DATA( DebugLines, &debugLines );
	SERVE_DATA( numDebugLines, &numDebugLines );
	SERVE_DATA( DebugStrings, &debugStrings );
	SERVE_DATA( numDebugStrings, &numDebugStrings );
	SERVE( LocateGameData, LocateGameData );
	SERVE( SetFarPlane, SetFarPlane );
	SERVE( SetSkyPortal, SetSkyPortal );
	SERVE( Popmenu, PopMenu );
	SERVE( Showmenu, ShowMenu );
	SERVE( Hidemenu, HideMenu );
	SERVE( Pushmenu, PushMenu );
	SERVE( HideMouseCursor, HideMouseCursor );
	SERVE( ShowMouseCursor, ShowMouseCursor );
	SERVE( MapTime, MapTime );
	SERVE( LoadResource, LoadResource );
	SERVE( ClearResource, ClearResource );
	SERVE( Key_StringToKeynum, KeyStringToKeynum );
	SERVE( Key_KeynumToBindString, KeyKeynumToBindString );
	SERVE( Key_GetKeysForCommand, KeyGetKeysForCommand );
	SERVE( ArchiveLevel, ArchiveLevel );
	SERVE( AddSvsTimeFixup, AddSvsTimeFixup );
	SERVE( HudDrawShader, HudDrawShader );
	SERVE( HudDrawAlign, HudDrawAlign );
	SERVE( HudDrawRect, HudDrawRect );
	SERVE( HudDrawVirtualSize, HudDrawVirtualSize );
	SERVE( HudDrawColor, HudDrawColor );
	SERVE( HudDrawAlpha, HudDrawAlpha );
	SERVE( HudDrawString, HudDrawString );
	SERVE( HudDrawFont, HudDrawFont );
	SERVE_DATA( fsDebug, Cvar_Get( "fs_debug", "0", 0 ) );

#undef SERVE
#undef SERVE_DATA
}

/* ---- the exports ---------------------------------------------------------- */

typedef void ( QDECL *Init_t )( int startTime, int randomSeed );
typedef void ( QDECL *Void_t )();
typedef void ( QDECL *Cleanup_t )( qboolean samemap );
typedef void ( QDECL *SetMap_t )( const char *mapName );
typedef void ( QDECL *SetTime_t )( int svsStartTime, int svsTime );
typedef void ( QDECL *SpawnEntities_t )( char *entstring, int levelTime );
typedef const char *( QDECL *ClientConnect_t )( int clientNum, qboolean firstTime );
typedef void ( QDECL *ClientBegin_t )( void *ent, void *cmd );
typedef void ( QDECL *ClientUserinfoChanged_t )( void *ent, const char *userinfo );
typedef void ( QDECL *ClientEntity_t )( void *ent );
typedef void ( QDECL *ClientThink_t )( void *ent, void *cmd, void *eyeinfo );
typedef void ( QDECL *RunFrame_t )( int levelTime, int frameTime );
typedef qboolean ( QDECL *ConsoleCommand_t )();
typedef void ( QDECL *SetFrameNumber_t )( int frameNumber );

void ConsoleCommandHandler() {
	const auto command = Export<ConsoleCommand_t>( GE_ConsoleCommand );
	if ( command && !command() ) {
		Com_Printf( "Medal of Honor game module did not handle \"%s\"\n", Cmd_Argv( 0 ) );
	}
}

/* The level time the last frame ran at, from which each frame's length is
 * measured: 2015's RunFrame takes both (OpenMoHAA sv_main.c). */
int lastLevelTime;
int frameNumber;

/* A single-player level waits for its player: from its spawn until the local
 * client has its first snapshot the game runs no frames, and only then is it
 * told the level spawned -- the event every level script waits for with
 * `level waittill spawn` -- its clock started from that moment (OpenMoHAA
 * sv_init.c SV_ServerLoaded, from the client's CL_FirstSnapshot). A
 * multiplayer level starts as soon as it has spawned. */
bool serverLoaded = true;
void ServerLoaded();

int FrameTime( int levelTime ) {
	int frameTime = levelTime - lastLevelTime;
	lastLevelTime = levelTime;
	if ( frameTime < 0 || frameTime > 1000 ) {
		frameTime = 50;
	}
	return frameTime;
}

void RunFrame( int levelTime, int frameTime ) {
	const auto setFrameNumber = Export<SetFrameNumber_t>( GE_SetFrameNumber );
	if ( setFrameNumber ) {
		setFrameNumber( ++frameNumber );
	}
	Export<RunFrame_t>( GE_RunFrame )( levelTime, frameTime );
	CheckErrorMessage();
}

/* The entity text SpawnEntities is handed, as a copy the engine owns. */
std::string entityText;

intptr_t QDECL GameEntry( int command, ... ) {
	va_list argptr;

	if ( command == GAME_SHUTDOWN && !exports ) {
		return 0;
	}
	if ( !exports ) {
		Com_Error( ERR_DROP, "Medal of Honor game module was called before it loaded" );
		return 0;
	}

	switch ( command ) {
	case GAME_INIT: {
		va_start( argptr, command );
		const int levelTime = va_arg( argptr, int );
		const int randomSeed = va_arg( argptr, int );
		va_end( argptr );
		(void)randomSeed;
		/* OpenMoHAA's SV_InitGameProgs: Init with the server's start time and
		 * the millisecond clock as the seed. The level is spawned afterwards,
		 * in SV_MOHAASpawnLevel. */
		lastLevelTime = levelTime;
		Export<Init_t>( GE_Init )( levelTime, Com_Milliseconds() );
		CheckErrorMessage();
		return 0;
	}

	case GAME_SHUTDOWN: {
		const auto shutdown = Export<Void_t>( GE_Shutdown );
		if ( shutdown ) {
			shutdown();
		}
		exports = nullptr;
		return 0;
	}

	case GAME_RUN_FRAME: {
		va_start( argptr, command );
		const int levelTime = va_arg( argptr, int );
		va_end( argptr );
		if ( !serverLoaded ) {
			lastLevelTime = levelTime;
			return 0;
		}
		RunFrame( levelTime, FrameTime( levelTime ) );
		return 0;
	}

	case GAME_CONSOLE_COMMAND: {
		const auto consoleCommand = Export<ConsoleCommand_t>( GE_ConsoleCommand );
		return consoleCommand ? consoleCommand() : 0;
	}

	case GAME_CLIENT_CONNECT: {
		va_start( argptr, command );
		const int clientNum = va_arg( argptr, int );
		const int firstTime = va_arg( argptr, int );
		va_end( argptr );
		mohaa_net::ResetClient( clientNum );
		const char *refusal = Export<ClientConnect_t>( GE_ClientConnect )( clientNum, firstTime ? qtrue : qfalse );
		CheckErrorMessage();
		return reinterpret_cast<intptr_t>( refusal );
	}

	case GAME_CLIENT_BEGIN: {
		/* The client's first command, which the move that brought it in
		 * stored (UserMove below). */
		va_start( argptr, command );
		const int clientNum = va_arg( argptr, int );
		va_end( argptr );
		GEntity *ent = mohaa_world::Entity( clientNum );
		mohaa_net::ClientState *state = mohaa_net::State( clientNum );
		if ( !ent || !state ) {
			Com_Error( ERR_DROP, "Medal of Honor ClientBegin: client %i has no entity", clientNum );
			return 0;
		}
		Export<ClientBegin_t>( GE_ClientBegin )( ent, &state->lastCmd );
		CheckErrorMessage();
		/* A dedicated server has no local client to wait for: its first
		 * player starts the level (OpenMoHAA sv_client.c). */
		if ( !serverLoaded && com_dedicated && com_dedicated->integer ) {
			ServerLoaded();
		}
		return 0;
	}

	case GAME_CLIENT_USERINFO_CHANGED: {
		va_start( argptr, command );
		const int clientNum = va_arg( argptr, int );
		va_end( argptr );
		GEntity *ent = mohaa_world::Entity( clientNum );
		if ( !ent || clientNum < 0 || clientNum >= sv.maxclients ) {
			return 0;
		}
		Export<ClientUserinfoChanged_t>( GE_ClientUserinfoChanged )( ent, SV_ClientForIndex( clientNum ).userinfo );
		CheckErrorMessage();
		return 0;
	}

	case GAME_CLIENT_COMMAND: {
		va_start( argptr, command );
		const int clientNum = va_arg( argptr, int );
		va_end( argptr );
		GEntity *ent = mohaa_world::Entity( clientNum );
		if ( !ent ) {
			return 0;
		}
		Export<ClientEntity_t>( GE_ClientCommand )( ent );
		CheckErrorMessage();
		return 0;
	}

	case GAME_CLIENT_THINK: {
		va_start( argptr, command );
		const int clientNum = va_arg( argptr, int );
		va_end( argptr );
		GEntity *ent = mohaa_world::Entity( clientNum );
		mohaa_net::ClientState *state = mohaa_net::State( clientNum );
		if ( !ent || !state ) {
			return 0;
		}
		Export<ClientThink_t>( GE_ClientThink )( ent, &state->lastCmd, &state->lastEyes );
		CheckErrorMessage();
		return 0;
	}

	case GAME_CLIENT_DISCONNECT: {
		va_start( argptr, command );
		const int clientNum = va_arg( argptr, int );
		va_end( argptr );
		GEntity *ent = mohaa_world::Entity( clientNum );
		if ( ent ) {
			Export<ClientEntity_t>( GE_ClientDisconnect )( ent );
			CheckErrorMessage();
		}
		return 0;
	}

	default:
		Com_Error( ERR_DROP, "Medal of Honor game module is loaded, but fnTech3 does not dispatch engine "
			"call %i to it.", command );
		return 0;
	}
}

/* ---- a client's moves ------------------------------------------------------ */

/*
==================
EnterWorld

2015's CS_PRIMED to CS_ACTIVE (OpenMoHAA SV_ClientEnterWorld): the
configstrings that changed while the client was primed go to it again, its
entity learns its number, the next snapshot is sent whole, and the game's
ClientBegin runs with the command that brought the client in. The engine's
own enter-world reads the entity through Quake III's record, which 2015's
is not.
==================
*/
void EnterWorld( client_t *cl, mohaa_net::ClientState &state ) {
	SV_PrintClientStateChange( cl, CS_ACTIVE );
	cl->state = CS_ACTIVE;
	cl->gamestateAck = GSA_ACKED;
	cl->oldServerTime = 0;
	SV_UpdateConfigstrings( cl );

	const int clientNum = SV_ClientIndex( cl );
	GEntity *ent = mohaa_world::Entity( clientNum );
	if ( !ent ) {
		Com_Error( ERR_DROP, "Medal of Honor: client %i has no entity to enter the world with", clientNum );
		return;
	}
	ent->s.number = clientNum;
	cl->gentity = reinterpret_cast<sharedEntity_t *>( ent );
	cl->deltaMessage = fnql::net::RetreatSequence( cl->netchan.outgoingSequence, PACKET_BACKUP + 1u );
	cl->lastSnapshotTime = svs.time - 9999;

	Export<ClientBegin_t>( GE_ClientBegin )( ent, &state.lastCmd );
	CheckErrorMessage();
}

/* OpenMoHAA sv_client.c SV_UserMove: the eyes, then up to 32 commands keyed
 * as Quake III 1.32 keys them, each run through ClientThink with the eyes
 * unless it is older than the last one run. The engine's own record of the
 * client's last command is kept in step for its timeouts and pings. */
void UserMove( client_t *cl, msg_t *msg, qboolean delta ) {
	mohaa_net::ClientState *state = mohaa_net::State( cl );
	if ( !state ) {
		return;
	}

	cl->deltaMessage = delta ? cl->messageAcknowledge
		: fnql::net::RetreatSequence( cl->netchan.outgoingSequence, PACKET_BACKUP + 1u );

	const int cmdCount = MSG_ReadByte( msg );
	if ( cmdCount < 1 ) {
		Com_Printf( "cmdCount < 1\n" );
		return;
	}
	if ( cmdCount > fntech3::mohaa::wire::kMaxPacketUsercmds ) {
		Com_Printf( "cmdCount > MAX_PACKET_USERCMDS\n" );
		return;
	}

	int key = sv.checksumFeed;
	key ^= cl->messageAcknowledge;
	key ^= MSG_HashKeyForWireProfile( cl->netchan.wireProfile,
		cl->reliableCommands[cl->reliableAcknowledge & ( SV_ReliableCommandLimit( cl ) - 1 )], 32 );

	fntech3::mohaa::wire::ReadDeltaEyeInfo( msg, state->lastEyes, state->lastEyes );

	fntech3::mohaa::UserCmd cmds[fntech3::mohaa::wire::kMaxPacketUsercmds];
	const fntech3::mohaa::UserCmd nullcmd{};
	const fntech3::mohaa::UserCmd *oldcmd = &nullcmd;
	for ( int i = 0; i < cmdCount; i++ ) {
		cmds[i] = fntech3::mohaa::UserCmd{};
		fntech3::mohaa::wire::ReadDeltaUsercmdKey( msg, key, *oldcmd, cmds[i] );
		oldcmd = &cmds[i];
	}
	if ( msg->readcount > msg->cursize ) {
		SV_DropClient( cl, "Invalid or truncated user command" );
		return;
	}

	// save time for ping calculation
	if ( cl->frames[cl->messageAcknowledge & PACKET_MASK].messageAcked == 0 ) {
		cl->frames[cl->messageAcknowledge & PACKET_MASK].messageAcked = Sys_Milliseconds();
	}

	auto keepEngineRecord = [cl]( const fntech3::mohaa::UserCmd &cmd ) {
		cl->lastUsercmd.serverTime = cmd.serverTime;
		cl->lastUsercmd.angles[0] = cmd.angles[0];
		cl->lastUsercmd.angles[1] = cmd.angles[1];
		cl->lastUsercmd.angles[2] = cmd.angles[2];
		cl->lastUsercmd.forwardmove = cmd.forwardmove;
		cl->lastUsercmd.rightmove = cmd.rightmove;
		cl->lastUsercmd.upmove = cmd.upmove;
		cl->lastUsercmd.buttons = cmd.buttons;
	};

	// the first command of this gamestate puts the client into the world
	if ( cl->state == CS_PRIMED ) {
		state->lastCmd = cmds[0];
		keepEngineRecord( cmds[0] );
		EnterWorld( cl, *state );
	}

	if ( cl->state != CS_ACTIVE ) {
		cl->deltaMessage = -1;
		return;
	}

	GEntity *ent = mohaa_world::Entity( SV_ClientIndex( cl ) );
	const auto think = Export<ClientThink_t>( GE_ClientThink );
	for ( int i = 0; i < cmdCount; i++ ) {
		// a command from before a map_restart
		if ( cmds[i].serverTime > cmds[cmdCount - 1].serverTime ) {
			continue;
		}
		// one already run, which a duplicate packet can carry again
		if ( cmds[i].serverTime <= state->lastCmd.serverTime ) {
			continue;
		}
		state->lastCmd = cmds[i];
		keepEngineRecord( cmds[i] );
		if ( ent && think ) {
			think( ent, &state->lastCmd, &state->lastEyes );
			CheckErrorMessage();
		}
	}
}

/* ---- loading -------------------------------------------------------------- */

typedef std::intptr_t *( QDECL *GetGameAPI_t )( void *import );

vm_t *LoadGame() {
	/* The allocator both retail libraries import lies in the installation's
	 * root, beside MOHAA.exe; the user's copy is loaded from there, so the
	 * loader resolves the module's import by name. */
	MOHAA_LoadSystem86();

	void *symbol = nullptr;
	vm_t *vm = VM_CreateTable( VM_GAME, "GetGameAPI", GameEntry, &symbol );
	if ( !vm ) {
		return nullptr;
	}

	FillImports();
	Com_Memset( modelUserCount, 0, sizeof( modelUserCount ) );
	mohaa_net::Reset();
	gameData = GameData{};
	savegameCvars.clear();
	svsTimeFixups.clear();
	debugLines = nullptr;
	numDebugLines = 0;
	debugStrings = nullptr;
	numDebugStrings = 0;
	frameNumber = 0;

	std::intptr_t *game = reinterpret_cast<GetGameAPI_t>( symbol )( imports );
	if ( !game ) {
		VM_Free( vm );
		Com_Error( ERR_DROP, "Medal of Honor game module published no export table" );
		return nullptr;
	}
	if ( game[GE_apiversion] != FNTECH3_MOHAA_GAME_API_VERSION ) {
		VM_Free( vm );
		Com_Error( ERR_DROP, "Medal of Honor game module is API version %i, not %i", (int)game[GE_apiversion],
			FNTECH3_MOHAA_GAME_API_VERSION );
		return nullptr;
	}
	if ( game[GE_gentitySize] != FNTECH3_MOHAA_GENTITY_BYTES ) {
		VM_Free( vm );
		Com_Error( ERR_DROP, "Medal of Honor game module has a %i-byte entity record, not %i",
			(int)game[GE_gentitySize], FNTECH3_MOHAA_GENTITY_BYTES );
		return nullptr;
	}
	/* 1.00 has no bots: the three members after ClientThink stay null. */
	if ( game[GE_BotBegin] || game[GE_BotThink] || game[GE_PrepFrame] ) {
		VM_Free( vm );
		Com_Error( ERR_DROP, "Medal of Honor game module publishes bot exports 1.00 does not have" );
		return nullptr;
	}

	exports = game;
	Com_Printf( "Medal of Honor game module loaded: API version %i, %i-byte entities; %i of %i imports are "
		"served\n", (int)game[GE_apiversion], (int)game[GE_gentitySize], ServedImports(),
		FNTECH3_MOHAA_GAME_IMPORT_SLOTS );
	Com_DPrintf( "Medal of Honor game module base %p\n", vm->dllHandle );
	return vm;
}

/* ---- the level --------------------------------------------------------- */

void CreateBaselines() {
	Com_Memset( baselines, 0, sizeof( baselines ) );
	Com_Memset( baselineUsed, 0, sizeof( baselineUsed ) );
	const int count = exports ? (int)exports[GE_num_entities] : 0;
	for ( int entnum = 0; entnum < count && entnum < FNTECH3_MOHAA_MAX_GENTITIES; entnum++ ) {
		GEntity *ent = mohaa_world::Entity( entnum );
		if ( !ent || !ent->r.linked ) {
			continue;
		}
		ent->s.number = entnum;
		baselines[entnum] = ent->s;
		baselineUsed[entnum] = true;
	}
}

/* The single-player level flow's steps (server_mohaa_save.hpp). */
namespace save {
bool TakeSameLevel();
void BeginSpawn( const char *mapname );
bool Loading();
bool Restored();
int MapTime();
bool ReadSavedLevel();
void AfterSpawn();
void AfterLoaded();
}

/* The server time the level's clock started at (svs.startTime). */
int levelStartTime;

/* OpenMoHAA sv_init.c SV_SpawnServer, from the game's Cleanup to its entity
 * spawn: the model cache starts over for the new map, the level is named,
 * cached and spawned -- or read back from a save -- and the module's error
 * message is checked after. */
void SpawnLevel( const char *mapname ) {
	const bool sameLevel = save::TakeSameLevel();
	if ( !exports ) {
		return;
	}
	serverLoaded = false;
	save::BeginSpawn( mapname );
	Export<Cleanup_t>( GE_Cleanup )( qfalse );
	Export<SetTime_t>( GE_SetTime )( sv.time, sv.time );

	Com_Memset( modelUserCount, 0, sizeof( modelUserCount ) );
	TIKI_End();
	TIKI_Begin();
	TIKI_FreeAll();

	/* Loading a map empties the global alias list (OpenMoHAA cm_load.c
	 * CM_LoadMap), so a level has only the aliases registered for it: the
	 * client game's parse in single player, the game's RegisterSounds after
	 * the spawn in multiplayer. Until then the spawn's own lookups find
	 * nothing, as retail's do. A restart, or a load of a save of the running
	 * level, loads no map there and keeps what the level had. */
	if ( !sameLevel ) {
		MOHAA_GlobalAliasClear();
	}

	if ( !save::Loading() ) {
		SV_SetConfigstring( FNTECH3_MOHAA_CS_MUSIC, "none" );
	}

	Export<SetMap_t>( GE_SetMap )( mapname );
	Export<Void_t>( GE_Precache )();
	CheckErrorMessage();

	if ( !save::ReadSavedLevel() ) {
		entityText = CM_EntityString() ? CM_EntityString() : "";
		Export<SpawnEntities_t>( GE_SpawnEntities )( &entityText[0], sv.time );
		CheckErrorMessage();
		save::AfterSpawn();
	}

	if ( com_dedicated && com_dedicated->integer ) {
		TIKI_FinishLoad();
	}
	lastLevelTime = sv.time;
}

/* SV_ServerLoaded: the game's clock starts now, the times it asked to have
 * re-based take the present, and the level is told it spawned. */
void ServerLoaded() {
	if ( serverLoaded || !exports ) {
		return;
	}
	serverLoaded = true;
	/* A level read back from a save goes on from the time it was saved. */
	levelStartTime = save::Restored() ? sv.time - save::MapTime() : sv.time;
	Export<SetTime_t>( GE_SetTime )( levelStartTime, sv.time );
	for ( int *time : svsTimeFixups ) {
		*time = sv.time;
	}
	svsTimeFixups.clear();
	Export<Void_t>( GE_ServerSpawned )();
	CheckErrorMessage();
	lastLevelTime = sv.time;
	save::AfterLoaded();
}

/* Three frames of 100ms to let the level settle, each after advancing the
 * clock, as 2015's engine runs them (OpenMoHAA sv_init.c). */
void Settle() {
	/* None follow a level read back from a save. */
	if ( save::Restored() ) {
		lastLevelTime = sv.time;
		return;
	}
	for ( int i = 0; i < 3; i++ ) {
		sv.time += 100;
		RunFrame( sv.time, 100 );
	}
	lastLevelTime = sv.time;
}

} // namespace mohaa_game

#include "server_mohaa_save.hpp"

vm_t *SV_MOHAALoadGame( void ) {
	return mohaa_game::LoadGame();
}

void SV_MOHAAClearWorld( void ) {
	mohaa_world::ClearWorld();
	/* The inline models take the first model indexes, as the engine's world
	 * clear names them (OpenMoHAA sv_world.c SV_ClearWorld): a brush
	 * entity's index is its inline model's number, and the client game draws
	 * a general entity -- a door, a func_static -- by the model at its index
	 * (cgamex86.dll 0x300200f6, reached for entity type 5 through the table at
	 * 0x30020dbf). */
	const int count = CM_NumInlineModels();
	for ( int i = 1; i < count; i++ ) {
		mohaa_game::ModelIndex( va( "*%i", i ) );
	}
}

void SV_MOHAASpawnLevel( const char *mapname ) {
	mohaa_game::SpawnLevel( mapname );
}

void SV_MOHAASettle( void ) {
	mohaa_game::Settle();
}

void SV_MOHAACreateBaselines( void ) {
	mohaa_game::CreateBaselines();
}

void SV_MOHAAWriteBaselines( msg_t *msg ) {
	mohaa_net::WriteBaselines( msg );
}

void SV_MOHAASendClientSnapshot( client_t *client ) {
	mohaa_net::SendClientSnapshot( client );
}

void SV_MOHAAUserMove( client_t *cl, msg_t *msg, qboolean delta ) {
	mohaa_game::UserMove( cl, msg, delta );
}

/* The end of a level's spawn. A multiplayer game registers its sounds here:
 * the game parses the ubersound scripts into the engine's global alias list,
 * which in single player the client game fills instead (OpenMoHAA sv_init.c,
 * after the heartbeat). Nothing registers them before the spawn, so a sound
 * the spawn plays finds no alias; retail 1.11's logs show the same, a
 * dedicated server parsing after "Spawning Entities Done" and a single-player
 * spawn missing `snd_pickup` (docs/fntech3/2026-09-24-mohaa-contracts.md). */
void SV_MOHAASpawnDone( void ) {
	using namespace mohaa_game;
	if ( exports && Cvar_VariableIntegerValue( "g_gametype" ) != 0 ) {
		Export<Void_t>( GE_RegisterSounds )();
		CheckErrorMessage();
		ServerLoaded();
	}
}

/* The local client's first snapshot of a level. */
void SV_MOHAAServerLoaded( void ) {
	mohaa_game::ServerLoaded();
}

/* The local client started a sound the game waits on, on `channel` (OpenMoHAA
 * SV_SoundCallback): the game will ask after it each frame (import 135). Its
 * SoundCallback drops out of range entity numbers through the engine and
 * throws a script error out of the module for an entity whose object it has
 * freed (gamex86.dll 0x310d78c4, 0x310d78fb), so neither is passed on: the
 * sound's entity went while the snapshot was on its way. */
extern "C" void SV_MOHAASoundCallback( int entity, int channel, const char *name ) {
	using namespace mohaa_game;
	typedef void ( QDECL *SoundCallback_t )( int entNum, int channel, const char *name );
	const auto callback = Export<SoundCallback_t>( GE_SoundCallback );
	const byte *entities = EntityBase();
	const std::intptr_t size = exports ? exports[GE_gentitySize] : 0;
	if ( !callback || !entities || entity < 0 || entity >= EntityCapacity()
		|| size < fntech3::mohaa::kGEntityObjectOffset + 4 ) {
		return;
	}
	std::uint32_t object;
	std::memcpy( &object, entities + static_cast<std::size_t>( entity ) * static_cast<std::size_t>( size )
		+ fntech3::mohaa::kGEntityObjectOffset, sizeof( object ) );
	if ( !object ) {
		Com_DPrintf( "Medal of Honor: sound '%s' waited on by entity %i, which is gone\n", name ? name : "", entity );
		return;
	}
	Com_DPrintf( "Medal of Honor: entity %i waits on sound '%s' (channel %i)\n", entity, name ? name : "", channel );
	callback( entity, channel, name ? name : "" );
	CheckErrorMessage();
}

#endif
