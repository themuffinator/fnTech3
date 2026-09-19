/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_STVEF_SP_GAME_HPP
#define FNTECH3_SERVER_STVEF_SP_GAME_HPP
#include "../compat/stvef_sp_game_api.h"
#include "../compat/stvef_state_adapter.hpp"

#include <algorithm>
#include <array>
#include <cstdarg>

/*
 * Elite Force SP's game module is a function table (stvef_sp_game_api.h).
 * The server drives it through gvm like any module: SV_STVEFSPGameEntry turns
 * each game export call into a call on the table, and the imports below are
 * the engine's side, in retail stvoy.exe's order. The game's gentity array is
 * bound whole, and its count is refreshed after every call, since the game
 * spawns entities as it runs.
 */

namespace {

fntech3_stvef_sp_game_export_t *stvefSpGame = nullptr;
fntech3_stvef_sp_game_import_t stvefSpImport{};
// Per-entity voice volume for lip-sync (the import's S_Override).
std::int32_t stvefSpVoiceVolume[FNTECH3_STVEF_SP_LIPSYNC_ENTITIES]{};
// The camera file WriteCam appends to, opened on first use.
fileHandle_t stvefSpCameraFile = 0;
// How the level being entered began: SavedGameJustLoaded_e and whether it is
// a hub transition. Set by the savegame and transition commands.
std::int32_t stvefSpSavedGameJustLoaded = FNTECH3_STVEF_SP_SAVED_NO;
std::int32_t stvefSpLoadTransition = 0;
// The cgame half of efgamex86.dll: its vmMain, and the client's handler for
// its syscalls once a client registers one.
vmMainFunc_t stvefSpCgameMain = nullptr;
std::intptr_t ( *stvefSpClientSyscalls )( std::intptr_t *args ) = nullptr;

/* The cgame's one syscall. The game half calls cgame functions as it runs
 * (it starts world sounds through them), so retail attaches the cgame before
 * the game's Init (SV_InitGameProgs 0x425680). With no client, as on a
 * dedicated server, those calls do nothing. */
intptr_t QDECL SV_STVEFSPCgameSyscall( intptr_t call, ... ) {
	std::intptr_t args[16] = { call };
	va_list ap;
	va_start( ap, call );
	for ( int i = 1; i < 16; ++i ) args[i] = va_arg( ap, std::intptr_t );
	va_end( ap );
	return stvefSpClientSyscalls ? stvefSpClientSyscalls( args ) : 0;
}

void SV_STVEFSPBindEntities() {
	fntech3_stvef_sp_game_export_t *game = stvefSpGame;
	if ( !game || !game->gentities ) {
		return;
	}
	const int count = game->num_entities;
	if ( serverGameData.IsBound() && serverGameData.EntityCount() == count
		&& sv.gentities == game->gentities && sv.gentitySize == game->gentitySize ) {
		return;
	}
	if ( game->gentitySize < static_cast<int>( sizeof( fntech3_stvef_sp_gentity_t ) ) ) {
		Com_Error( ERR_DROP, "Elite Force SP gentities are %i bytes", game->gentitySize );
		return;
	}
	fntech3::server::NativeGameDataBinding binding;
	binding.entityBase = game->gentities;
	binding.entityCount = count;
	// The game's g_entities is a fixed array of every gentity.
	binding.entityCapacity = FNTECH3_STVEF_MAX_GENTITIES;
	binding.entityStride = static_cast<std::size_t>( game->gentitySize );
	binding.entitySpanBytes = binding.entityStride * FNTECH3_STVEF_MAX_GENTITIES;
	binding.clientCount = FNTECH3_STVEF_SP_MAX_CLIENTS;
	const fntech3::server::GameDataBindError error =
		serverGameData.BindNative( COM_GAME_STVEF_SP, binding );
	if ( error != fntech3::server::GameDataBindError::None ) {
		Com_Error( ERR_DROP, "Invalid Elite Force SP game data: %s",
			fntech3::server::GameDataBindErrorName( error ) );
		return;
	}
	sv.gentities = game->gentities;
	sv.gentitySize = game->gentitySize;
	sv.num_entities = count;
}

sharedEntity_t *SV_STVEFSPEntity( const fntech3_stvef_sp_gentity_t *entity ) {
	SV_STVEFSPBindEntities();
	return reinterpret_cast<sharedEntity_t *>( const_cast<fntech3_stvef_sp_gentity_t *>( entity ) );
}

// Raven's levels are fatal, drop, disconnect and missing CD (Com_Error 0x411170).
errorParm_t SV_STVEFSPErrorLevel( int level ) {
	switch ( level ) {
	case FNTECH3_STVEF_SP_ERR_DROP: return ERR_DROP;
	case FNTECH3_STVEF_SP_ERR_DISCONNECT: return ERR_DISCONNECT;
	case FNTECH3_STVEF_SP_ERR_NEED_CD: return ERR_NEED_CD;
	default: return ERR_FATAL;
	}
}

void SV_STVEFSPPrintf( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list ap;
	va_start( ap, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	Com_Printf( "%s", text );
}

void SV_STVEFSPWriteCam( const char *text ) {
	char mapname[MAX_QPATH], path[MAX_QPATH];
	Cvar_VariableStringBuffer( "mapname", mapname, sizeof( mapname ) );
	Com_sprintf( path, sizeof( path ), "maps/%s_cam.map", mapname );
	if ( !stvefSpCameraFile ) {
		stvefSpCameraFile = FS_FOpenFileWrite( path );
	}
	if ( stvefSpCameraFile && text ) {
		FS_Write( text, static_cast<int>( std::strlen( text ) ), stvefSpCameraFile );
	}
	Com_Printf( "%s\n", path );
}

void SV_STVEFSPError( int level, const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list ap;
	va_start( ap, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	Com_Error( SV_STVEFSPErrorLevel( level ), "%s", text );
}

std::int32_t SV_STVEFSPMilliseconds() { return Sys_Milliseconds(); }

/* The game reads a cvar's string, value and integer, which the engine's cvar_t
 * keeps at retail's offsets on the only host. */
fntech3_stvef_sp_cvar_t *SV_STVEFSPCvar( const char *name, const char *value, std::int32_t flags ) {
	return reinterpret_cast<fntech3_stvef_sp_cvar_t *>( Cvar_Get( name, value, flags ) );
}

void SV_STVEFSPCvarSet( const char *name, const char *value ) { Cvar_Set( name, value ); }
std::int32_t SV_STVEFSPCvarInteger( const char *name ) { return Cvar_VariableIntegerValue( name ); }
void SV_STVEFSPCvarString( const char *name, char *buffer, std::int32_t size ) {
	Cvar_VariableStringBuffer( name, buffer, size );
}
std::int32_t SV_STVEFSPArgc() { return Cmd_Argc(); }
char *SV_STVEFSPArgv( std::int32_t n ) { return const_cast<char *>( Cmd_Argv( n ) ); }

std::int32_t SV_STVEFSPOpenFile( const char *qpath, std::int32_t *file, std::int32_t mode ) {
	fileHandle_t handle = 0;
	const int length = FS_VM_OpenFile( qpath, file ? &handle : nullptr, static_cast<fsMode_t>( mode ), H_QAGAME );
	if ( file ) *file = handle;
	return length;
}
std::int32_t SV_STVEFSPRead( void *buffer, std::int32_t length, std::int32_t file ) {
	return FS_VM_ReadFile( buffer, length, file, H_QAGAME );
}
std::int32_t SV_STVEFSPWrite( const void *buffer, std::int32_t length, std::int32_t file ) {
	return FS_VM_WriteFile( const_cast<void *>( buffer ), length, file, H_QAGAME );
}
void SV_STVEFSPCloseFile( std::int32_t file ) { FS_VM_CloseFile( file, H_QAGAME ); }
std::int32_t SV_STVEFSPReadFile( const char *name, void **buffer ) { return FS_ReadFile( name, buffer ); }
void SV_STVEFSPFreeFile( void *buffer ) { FS_FreeFile( buffer ); }
std::int32_t SV_STVEFSPFileList( const char *path, const char *extension, char *list, std::int32_t size ) {
	return FS_GetFileList( path, extension, list, size );
}

// Savegames arrive with the savegame slice; until then none is written or read.
std::int32_t SV_STVEFSPAppendToSave( std::uint32_t, void *, std::int32_t ) { return qfalse; }
std::int32_t SV_STVEFSPReadFromSave( std::uint32_t chunk, void *, std::int32_t, void ** ) {
	Com_Error( ERR_DROP, "Elite Force SP savegame chunk %08x requested with no savegame", chunk );
	return 0;
}
std::int32_t SV_STVEFSPReadFromSaveOptional( std::uint32_t, void *, std::int32_t, void ** ) { return 0; }

void SV_STVEFSPConsoleCommand( const char *text ) { Cbuf_AddText( text ); }
void SV_STVEFSPDropClient( std::int32_t clientNum, const char *reason ) { SV_GameDropClient( clientNum, reason ); }
void SV_STVEFSPServerCommand( std::int32_t clientNum, const char *fmt, ... ) {
	char text[MAX_STRING_CHARS];
	va_list ap;
	va_start( ap, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	SV_GameSendServerCommand( clientNum, text );
}
void SV_STVEFSPSetConfigstring( std::int32_t num, const char *string ) { SV_SetConfigstring( num, string ); }
void SV_STVEFSPGetConfigstring( std::int32_t num, char *buffer, std::int32_t size ) {
	SV_GetConfigstring( num, buffer, size );
}
void SV_STVEFSPGetUserinfo( std::int32_t num, char *buffer, std::int32_t size ) { SV_GetUserinfo( num, buffer, size ); }
void SV_STVEFSPSetUserinfo( std::int32_t num, const char *buffer ) { SV_SetUserinfo( num, buffer ); }
void SV_STVEFSPGetServerinfo( char *buffer, std::int32_t size ) { SV_GetServerinfo( buffer, size ); }

void SV_STVEFSPSetBrushModel( fntech3_stvef_sp_gentity_t *entity, const char *name ) {
	SV_SetBrushModel( SV_STVEFSPEntity( entity ), name );
}

/* The game passes -1 for "no entity" where Quake III passes ENTITYNUM_NONE.
 * Retail tests only ENTITYNUM_NONE and looks any other number up unchecked,
 * so -1 names a record before g_entities that no entity matches: none. */
int SV_STVEFSPPassEntity( std::int32_t passEntityNum ) {
	return passEntityNum < 0 || passEntityNum >= FNTECH3_STVEF_MAX_GENTITIES ? ENTITYNUM_NONE : passEntityNum;
}

void SV_STVEFSPTrace( fntech3_stvef_trace_t *results, const float *start, const float *mins,
	const float *maxs, const float *end, std::int32_t passEntityNum, std::int32_t contentmask ) {
	SV_STVEFSPBindEntities();
	trace_t trace;
	SV_Trace( &trace, start, mins, maxs, end, SV_STVEFSPPassEntity( passEntityNum ), contentmask, qfalse );
	fntech3_stvef_trace_t external{};
	external.allsolid = trace.allsolid;
	external.startsolid = trace.startsolid;
	external.fraction = trace.fraction;
	VectorCopy( trace.endpos, external.endpos );
	VectorCopy( trace.plane.normal, external.plane.normal );
	external.plane.dist = trace.plane.dist;
	external.plane.type = trace.plane.type;
	external.plane.signbits = trace.plane.signbits;
	external.surfaceFlags = trace.surfaceFlags;
	external.contents = trace.contents;
	external.entityNum = trace.entityNum;
	*results = external;
}

std::int32_t SV_STVEFSPPointContents( const float *point, std::int32_t passEntityNum ) {
	SV_STVEFSPBindEntities();
	return SV_PointContents( point, SV_STVEFSPPassEntity( passEntityNum ) );
}
std::int32_t SV_STVEFSPInPVS( const float *p1, const float *p2 ) { return SV_inPVS( p1, p2 ); }
std::int32_t SV_STVEFSPInPVSIgnorePortals( const float *p1, const float *p2 ) {
	return SV_inPVSIgnorePortals( p1, p2 ) ? qtrue : qfalse;
}
void SV_STVEFSPAdjustAreaPortalState( fntech3_stvef_sp_gentity_t *entity, std::int32_t open ) {
	SV_AdjustAreaPortalState( SV_STVEFSPEntity( entity ), open != 0 );
}
std::int32_t SV_STVEFSPAreasConnected( std::int32_t area1, std::int32_t area2 ) {
	return CM_AreasConnected( area1, area2 );
}
void SV_STVEFSPLinkEntity( fntech3_stvef_sp_gentity_t *entity ) { SV_LinkEntity( SV_STVEFSPEntity( entity ) ); }
void SV_STVEFSPUnlinkEntity( fntech3_stvef_sp_gentity_t *entity ) { SV_UnlinkEntity( SV_STVEFSPEntity( entity ) ); }

std::int32_t SV_STVEFSPEntitiesInBox( const float *mins, const float *maxs,
	fntech3_stvef_sp_gentity_t **list, std::int32_t maxcount ) {
	SV_STVEFSPBindEntities();
	if ( !list || maxcount <= 0 || !stvefSpGame ) {
		return 0;
	}
	std::array<int, MAX_GENTITIES> numbers{};
	const int count = SV_AreaEntities( mins, maxs, numbers.data(),
		( std::min )( static_cast<int>( maxcount ), MAX_GENTITIES ) );
	auto *base = reinterpret_cast<unsigned char *>( stvefSpGame->gentities );
	for ( int i = 0; i < count; ++i ) {
		list[i] = reinterpret_cast<fntech3_stvef_sp_gentity_t *>( base
			+ static_cast<std::size_t>( numbers[i] ) * static_cast<std::size_t>( stvefSpGame->gentitySize ) );
	}
	return count;
}

std::int32_t SV_STVEFSPEntityContact( const float *mins, const float *maxs,
	const fntech3_stvef_sp_gentity_t *entity ) {
	return SV_EntityContact( mins, maxs, SV_STVEFSPEntity( entity ), false ) ? qtrue : qfalse;
}

void *SV_STVEFSPMalloc( std::int32_t bytes ) {
	if ( bytes < 0 ) {
		Com_Error( ERR_DROP, "Elite Force SP game allocated %i bytes", bytes );
	}
	return Z_Malloc( bytes );
}
void SV_STVEFSPFree( void *buffer ) { Z_Free( buffer ); }

void SV_STVEFSPFillImports( fntech3_stvef_sp_game_import_t &import ) {
	import = {};
	import.Printf = SV_STVEFSPPrintf;
	import.WriteCam = SV_STVEFSPWriteCam;
	import.Error = SV_STVEFSPError;
	import.Milliseconds = SV_STVEFSPMilliseconds;
	import.cvar = SV_STVEFSPCvar;
	import.cvar_set = SV_STVEFSPCvarSet;
	import.Cvar_VariableIntegerValue = SV_STVEFSPCvarInteger;
	import.Cvar_VariableStringBuffer = SV_STVEFSPCvarString;
	import.argc = SV_STVEFSPArgc;
	import.argv = SV_STVEFSPArgv;
	import.FS_FOpenFile = SV_STVEFSPOpenFile;
	import.FS_Read = SV_STVEFSPRead;
	import.FS_Write = SV_STVEFSPWrite;
	import.FS_FCloseFile = SV_STVEFSPCloseFile;
	import.FS_ReadFile = SV_STVEFSPReadFile;
	import.FS_FreeFile = SV_STVEFSPFreeFile;
	import.FS_GetFileList = SV_STVEFSPFileList;
	import.AppendToSaveGame = SV_STVEFSPAppendToSave;
	import.ReadFromSaveGame = SV_STVEFSPReadFromSave;
	import.ReadFromSaveGameOptional = SV_STVEFSPReadFromSaveOptional;
	import.SendConsoleCommand = SV_STVEFSPConsoleCommand;
	import.DropClient = SV_STVEFSPDropClient;
	import.SendServerCommand = SV_STVEFSPServerCommand;
	import.SetConfigstring = SV_STVEFSPSetConfigstring;
	import.GetConfigstring = SV_STVEFSPGetConfigstring;
	import.GetUserinfo = SV_STVEFSPGetUserinfo;
	import.SetUserinfo = SV_STVEFSPSetUserinfo;
	import.GetServerinfo = SV_STVEFSPGetServerinfo;
	import.SetBrushModel = SV_STVEFSPSetBrushModel;
	import.trace = SV_STVEFSPTrace;
	import.pointcontents = SV_STVEFSPPointContents;
	import.inPVS = SV_STVEFSPInPVS;
	import.inPVSIgnorePortals = SV_STVEFSPInPVSIgnorePortals;
	import.AdjustAreaPortalState = SV_STVEFSPAdjustAreaPortalState;
	import.AreasConnected = SV_STVEFSPAreasConnected;
	import.linkentity = SV_STVEFSPLinkEntity;
	import.unlinkentity = SV_STVEFSPUnlinkEntity;
	import.EntitiesInBox = SV_STVEFSPEntitiesInBox;
	import.EntityContact = SV_STVEFSPEntityContact;
	import.S_Override = stvefSpVoiceVolume;
	import.Malloc = SV_STVEFSPMalloc;
	import.Free = SV_STVEFSPFree;
}

// The command a client last sent, as the game reads it.
fntech3_stvef_sp_usercmd_t SV_STVEFSPCommand( int clientNum ) {
	fntech3_stvef_sp_usercmd_t command{};
	if ( !SV_IsClientIndex( clientNum ) ||
		!fntech3::stvef::state::FromInternal( SV_ClientForIndex( clientNum ).lastUsercmd, command ) ) {
		Com_Error( ERR_DROP, "Invalid Elite Force SP user command for client %i", clientNum );
	}
	return command;
}

/* gvm's entry point. Arguments are the server's game export calls; the SP
 * table takes more for Init and ClientBegin, which come from the server. */
intptr_t QDECL SV_STVEFSPGameEntry( int command, ... ) {
	std::int32_t arg[3] = {};
	va_list ap;
	va_start( ap, command );
	for ( std::int32_t &value : arg ) value = va_arg( ap, std::int32_t );
	va_end( ap );

	fntech3_stvef_sp_game_export_t *game = stvefSpGame;
	if ( !game ) {
		Com_Error( ERR_DROP, "Elite Force SP game called after shutdown" );
		return 0;
	}
	SV_STVEFSPBindEntities();
	intptr_t result = 0;
	switch ( command ) {
	case GAME_INIT: {
		// Retail passes the map, its spawn target and checksum, the entity
		// string, the level time, com_frameTime as the seed, the current
		// milliseconds and how the level began (SV_InitGameProgs 0x425490).
		char mapname[MAX_QPATH], spawntarget[MAX_QPATH];
		Cvar_VariableStringBuffer( "mapname", mapname, sizeof( mapname ) );
		Cvar_VariableStringBuffer( "spawntarget", spawntarget, sizeof( spawntarget ) );
		game->Init( mapname, spawntarget, Cvar_VariableIntegerValue( "sv_mapChecksum" ),
			sv.entityParsePoint ? sv.entityParsePoint : "", arg[0], arg[1], Com_Milliseconds(),
			stvefSpSavedGameJustLoaded, stvefSpLoadTransition );
		break;
	}
	case GAME_SHUTDOWN:
		game->Shutdown();
		stvefSpGame = nullptr;
		stvefSpCgameMain = nullptr;
		if ( stvefSpCameraFile ) {
			FS_FCloseFile( stvefSpCameraFile );
			stvefSpCameraFile = 0;
		}
		return 0;
	case GAME_CLIENT_CONNECT:
		// A pointer to the refusal text, or null.
		result = reinterpret_cast<intptr_t>( game->ClientConnect( arg[0], arg[1], stvefSpSavedGameJustLoaded ) );
		break;
	case GAME_CLIENT_BEGIN: {
		fntech3_stvef_sp_usercmd_t cmd = SV_STVEFSPCommand( arg[0] );
		game->ClientBegin( arg[0], &cmd, stvefSpSavedGameJustLoaded );
		break;
	}
	case GAME_CLIENT_USERINFO_CHANGED:
		game->ClientUserinfoChanged( arg[0] );
		break;
	case GAME_CLIENT_DISCONNECT:
		game->ClientDisconnect( arg[0] );
		break;
	case GAME_CLIENT_COMMAND:
		game->ClientCommand( arg[0] );
		break;
	case GAME_CLIENT_THINK: {
		fntech3_stvef_sp_usercmd_t cmd = SV_STVEFSPCommand( arg[0] );
		game->ClientThink( arg[0], &cmd );
		break;
	}
	case GAME_RUN_FRAME:
		game->RunFrame( arg[0] );
		break;
	case GAME_CONSOLE_COMMAND:
		result = game->ConsoleCommand();
		break;
	case BOTAI_START_FRAME:
		// SP has no bots.
		return 0;
	default:
		Com_Error( ERR_DROP, "Elite Force SP game has no export %i", command );
		return 0;
	}
	SV_STVEFSPBindEntities();
	return result;
}

vm_t *SV_STVEFSPLoadGame() {
	void *symbol = nullptr;
	vm_t *vm = VM_CreateTable( VM_GAME, "GetGameAPI", SV_STVEFSPGameEntry, &symbol );
	if ( !vm ) {
		return nullptr;
	}
	auto getGameAPI = reinterpret_cast<fntech3_stvef_sp_GetGameAPI_t>( symbol );
	SV_STVEFSPFillImports( stvefSpImport );
	std::memset( stvefSpVoiceVolume, 0, sizeof( stvefSpVoiceVolume ) );
	fntech3_stvef_sp_game_export_t *game = getGameAPI( &stvefSpImport );
	if ( !game ) {
		VM_Free( vm );
		Com_Error( ERR_DROP, "Elite Force SP game returned no exports" );
		return nullptr;
	}
	// Attach the cgame half, as retail does next (0x42e670).
	void *entry = Sys_LoadFunction( vm->dllHandle, "dllEntry" );
	void *main = Sys_LoadFunction( vm->dllHandle, "vmMain" );
	if ( !entry || !main ) {
		VM_Free( vm );
		Com_Error( ERR_DROP, "failed to attach to the client DLL" );
		return nullptr;
	}
	reinterpret_cast<dllEntry_t>( entry )( SV_STVEFSPCgameSyscall );
	stvefSpCgameMain = reinterpret_cast<vmMainFunc_t>( main );
	if ( game->apiversion != FNTECH3_STVEF_SP_GAME_API_VERSION ) {
		const int version = game->apiversion;
		VM_Free( vm );
		Com_Error( ERR_DROP, "game is version %i, not %i", version, FNTECH3_STVEF_SP_GAME_API_VERSION );
		return nullptr;
	}
	stvefSpGame = game;
	return vm;
}

} // namespace

#endif
