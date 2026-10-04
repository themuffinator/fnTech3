/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JK2_SP_GAME_HPP
#define FNTECH3_SERVER_JK2_SP_GAME_HPP
#include "../compat/jk2_sp_game_api.h"
#include "../compat/jk2_sp_memory.hpp"
#include <algorithm>
#include <array>
#include <cstdarg>
#include <utility>

namespace {
fntech3_jk2_sp_game_import_t jk2SpImports{};
fntech3_jk2_sp_game_export_t *jk2SpGame = nullptr;
fntech3::jk2::sp::AllocationPool jk2SpMemory;
bool jk2SpInitComplete = false;
std::int32_t jk2SpVoiceVolume[1024]{};

void SV_JK2SPReset() {
	jk2SpGame = nullptr;
	jk2SpInitComplete = false;
	jk2SpMemory.Reset();
	FS_VM_CloseFiles( H_QAGAME );
}

/* No opaque entity, save or Ghoul2 pointer is dereferenced by a refusal.
 * These are cdecl callbacks; the caller removes its own arguments. */
template<std::size_t Slot> void SV_JK2SPRefuseImport() {
	SV_JK2SPReset();
	Com_Error( ERR_DROP, "Jedi Outcast SP game import %i (+0x%x) is not implemented",
		static_cast<int>( Slot ), static_cast<unsigned>( Slot * 4 ) );
}
template<std::size_t... I> void SV_JK2SPFillOpaque( std::index_sequence<I...> ) {
	const fntech3_jk2_sp_opaque_import_t callbacks[] = { &SV_JK2SPRefuseImport<I + 42>... };
	std::copy( std::begin( callbacks ), std::end( callbacks ), std::begin( jk2SpImports.unserved ) );
}

void SV_JK2SPPrintf( const char *fmt, ... ) {
	char text[MAXPRINTMSG]; va_list args; va_start( args, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, args ); va_end( args );
	Com_Printf( "%s", text );
}
void SV_JK2SPError( std::int32_t level, const char *fmt, ... ) {
	char text[MAXPRINTMSG]; va_list args; va_start( args, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, args ); va_end( args );
	SV_JK2SPReset();
	Com_Error( level == 1 ? ERR_DROP : level == 2 ? ERR_DISCONNECT :
		level == 3 ? ERR_NEED_CD : ERR_FATAL, "%s", text );
}
void SV_JK2SPWriteCam( const char * ) { SV_JK2SPRefuseImport<1>(); }
std::int32_t SV_JK2SPMilliseconds() { return Sys_Milliseconds(); }
fntech3_jk2_sp_cvar_t *SV_JK2SPCvar( const char *name, const char *value, std::int32_t flags ) {
	/* Retail reads string at +4, value at +28 and integer at +32. The
	 * common prefix is frozen; none of the engine's extended tail escapes. */
	static_assert( offsetof( cvar_t, string ) == offsetof( fntech3_jk2_sp_cvar_t, string ) );
	static_assert( offsetof( cvar_t, value ) == offsetof( fntech3_jk2_sp_cvar_t, value ) );
	static_assert( offsetof( cvar_t, integer ) == offsetof( fntech3_jk2_sp_cvar_t, integer ) );
	return reinterpret_cast<fntech3_jk2_sp_cvar_t *>( Cvar_Get( name, value, flags ) );
}
void SV_JK2SPCvarSet( const char *name, const char *value ) { Cvar_Set( name, value ); }
std::int32_t SV_JK2SPCvarInteger( const char *name ) { return Cvar_VariableIntegerValue( name ); }
void SV_JK2SPCvarString( const char *name, char *buffer, std::int32_t size ) { Cvar_VariableStringBuffer( name, buffer, size ); }
std::int32_t SV_JK2SPArgc() { return Cmd_Argc(); }
char *SV_JK2SPArgv( std::int32_t n ) { return const_cast<char *>( Cmd_Argv( n ) ); }
std::int32_t SV_JK2SPOpenFile( const char *path, std::int32_t *file, std::int32_t mode ) {
	fileHandle_t handle = 0;
	const int length = FS_VM_OpenFile( path, file ? &handle : nullptr, static_cast<fsMode_t>( mode ), H_QAGAME );
	if ( file ) *file = handle;
	return length;
}
std::int32_t SV_JK2SPRead( void *buffer, std::int32_t length, std::int32_t file ) { return FS_VM_ReadFile( buffer, length, file, H_QAGAME ); }
std::int32_t SV_JK2SPWrite( const void *buffer, std::int32_t length, std::int32_t file ) { return FS_VM_WriteFile( const_cast<void *>( buffer ), length, file, H_QAGAME ); }
void SV_JK2SPCloseFile( std::int32_t file ) { FS_VM_CloseFile( file, H_QAGAME ); }
std::int32_t SV_JK2SPReadFile( const char *name, void **buffer ) { return FS_ReadFile( name, buffer ); }
void SV_JK2SPFreeFile( void *buffer ) { if ( buffer ) FS_FreeFile( buffer ); }
std::int32_t SV_JK2SPFileList( const char *path, const char *extension, char *list, std::int32_t size ) { return FS_GetFileList( path, extension, list, size ); }
void SV_JK2SPConsoleCommand( const char *text ) { Cbuf_AddText( text ); }
void SV_JK2SPDropClient( std::int32_t client, const char *reason ) { SV_GameDropClient( client, reason ); }
void SV_JK2SPServerCommand( std::int32_t client, const char *fmt, ... ) {
	char text[MAX_STRING_CHARS]; va_list args; va_start( args, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, args ); va_end( args );
	SV_GameSendServerCommand( client, text );
}
void SV_JK2SPSetConfigstring( std::int32_t num, const char *string ) { SV_SetConfigstring( num, string ); }
void SV_JK2SPGetConfigstring( std::int32_t num, char *buffer, std::int32_t size ) { SV_GetConfigstring( num, buffer, size ); }
void SV_JK2SPGetUserinfo( std::int32_t num, char *buffer, std::int32_t size ) { SV_GetUserinfo( num, buffer, size ); }
void SV_JK2SPSetUserinfo( std::int32_t num, const char *buffer ) { SV_SetUserinfo( num, buffer ); }
void SV_JK2SPGetServerinfo( char *buffer, std::int32_t size ) { SV_GetServerinfo( buffer, size ); }
std::int32_t SV_JK2SPInPVS( const float *a, const float *b ) { return SV_inPVS( a, b ); }
std::int32_t SV_JK2SPInPVSIgnorePortals( const float *a, const float *b ) { return SV_inPVSIgnorePortals( a, b ); }
std::int32_t SV_JK2SPAreasConnected( std::int32_t a, std::int32_t b ) { return CM_AreasConnected( a, b ); }
void *SV_JK2SPMalloc( std::int32_t bytes, std::int32_t tag, std::int32_t zero ) {
	void *data = jk2SpMemory.Allocate( bytes, tag, zero != 0 );
	if ( !data ) { SV_JK2SPReset(); Com_Error( ERR_DROP, "Jedi Outcast SP invalid or failed allocation of %i bytes", bytes ); }
	return data;
}
void SV_JK2SPFree( void *data ) {
	if ( !jk2SpMemory.Free( data ) ) { SV_JK2SPReset(); Com_Error( ERR_DROP, "Jedi Outcast SP freed unowned memory" ); }
}
void SV_JK2SPFillImports() {
	jk2SpImports = {};
	jk2SpImports.Printf = SV_JK2SPPrintf;
	jk2SpImports.WriteCam = SV_JK2SPWriteCam;
	jk2SpImports.Error = SV_JK2SPError;
	jk2SpImports.Milliseconds = SV_JK2SPMilliseconds;
	jk2SpImports.cvar = SV_JK2SPCvar;
	jk2SpImports.cvar_set = SV_JK2SPCvarSet;
	jk2SpImports.Cvar_VariableIntegerValue = SV_JK2SPCvarInteger;
	jk2SpImports.Cvar_VariableStringBuffer = SV_JK2SPCvarString;
	jk2SpImports.argc = SV_JK2SPArgc;
	jk2SpImports.argv = SV_JK2SPArgv;
	jk2SpImports.FS_FOpenFile = SV_JK2SPOpenFile;
	jk2SpImports.FS_Read = SV_JK2SPRead;
	jk2SpImports.FS_Write = SV_JK2SPWrite;
	jk2SpImports.FS_FCloseFile = SV_JK2SPCloseFile;
	jk2SpImports.FS_ReadFile = SV_JK2SPReadFile;
	jk2SpImports.FS_FreeFile = SV_JK2SPFreeFile;
	jk2SpImports.FS_GetFileList = SV_JK2SPFileList;
	jk2SpImports.AppendToSaveGame = SV_JK2SPRefuseImport<17>;
	jk2SpImports.ReadFromSaveGame = SV_JK2SPRefuseImport<18>;
	jk2SpImports.ReadFromSaveGameOptional = SV_JK2SPRefuseImport<19>;
	jk2SpImports.SendConsoleCommand = SV_JK2SPConsoleCommand;
	jk2SpImports.DropClient = SV_JK2SPDropClient;
	jk2SpImports.SendServerCommand = SV_JK2SPServerCommand;
	jk2SpImports.SetConfigstring = SV_JK2SPSetConfigstring;
	jk2SpImports.GetConfigstring = SV_JK2SPGetConfigstring;
	jk2SpImports.GetUserinfo = SV_JK2SPGetUserinfo;
	jk2SpImports.SetUserinfo = SV_JK2SPSetUserinfo;
	jk2SpImports.GetServerinfo = SV_JK2SPGetServerinfo;
	jk2SpImports.SetBrushModel = SV_JK2SPRefuseImport<28>;
	jk2SpImports.trace = SV_JK2SPRefuseImport<29>;
	jk2SpImports.pointcontents = SV_JK2SPRefuseImport<30>;
	jk2SpImports.inPVS = SV_JK2SPInPVS;
	jk2SpImports.inPVSIgnorePortals = SV_JK2SPInPVSIgnorePortals;
	jk2SpImports.AdjustAreaPortalState = SV_JK2SPRefuseImport<33>;
	jk2SpImports.AreasConnected = SV_JK2SPAreasConnected;
	jk2SpImports.linkentity = SV_JK2SPRefuseImport<35>;
	jk2SpImports.unlinkentity = SV_JK2SPRefuseImport<36>;
	jk2SpImports.EntitiesInBox = SV_JK2SPRefuseImport<37>;
	jk2SpImports.EntityContact = SV_JK2SPRefuseImport<38>;
	jk2SpImports.VoiceVolume = jk2SpVoiceVolume;
	jk2SpImports.Malloc = SV_JK2SPMalloc;
	jk2SpImports.Free = SV_JK2SPFree;
	SV_JK2SPFillOpaque( std::make_index_sequence<61>{} );
}

intptr_t QDECL SV_JK2SPCgameSyscall( intptr_t call, ... ) {
	SV_JK2SPReset();
	Com_Error( ERR_DROP, "Jedi Outcast SP client import %i is not implemented", static_cast<int>( call ) );
	return 0;
}
intptr_t QDECL SV_JK2SPGameEntry( int command, ... ) {
	if ( command == GAME_SHUTDOWN ) {
		/* An incomplete Init may have left private objects unconstructed. Do
		 * not execute retail Shutdown on them or recursively refuse its G2
		 * callbacks while processing an error. All owned memory is reclaimed. */
		if ( jk2SpGame && jk2SpInitComplete && !com_errorEntered ) jk2SpGame->Shutdown();
		SV_JK2SPReset();
		return 0;
	}
	if ( !jk2SpGame ) { Com_Error( ERR_DROP, "Jedi Outcast SP game called after shutdown" ); return 0; }
	if ( command == GAME_INIT ) {
		va_list args; va_start( args, command );
		const std::int32_t time = va_arg( args, std::int32_t );
		const std::int32_t seed = va_arg( args, std::int32_t ); va_end( args );
		char map[MAX_QPATH], target[MAX_QPATH];
		Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
		Cvar_VariableStringBuffer( "spawntarget", target, sizeof( target ) );
		jk2SpInitComplete = false;
		jk2SpGame->Init( map, target, Cvar_VariableIntegerValue( "sv_mapChecksum" ),
			sv.entityParsePoint ? sv.entityParsePoint : "", time, seed, Com_Milliseconds(), 0, 0 );
		jk2SpInitComplete = true;
		/* The bootstrap never grants the game-data view permission to read
		 * this title's still-unadapted SP entity or player records. */
		SV_JK2SPReset();
		Com_Error( ERR_DROP, "Jedi Outcast SP initialized; entity and frame adapters are not implemented" );
		return 0;
	}
	SV_JK2SPReset();
	Com_Error( ERR_DROP, "Jedi Outcast SP export %i is not implemented", command );
	return 0;
}

vm_t *SV_JK2SPLoadGame() {
#if !defined(_WIN32) || UINTPTR_MAX != UINT32_MAX
	Com_Error( ERR_DROP, "Jedi Outcast SP retail modules require Windows x86" );
	return nullptr;
#else
	SV_JK2SPReset();
	SV_JK2SPFillImports();
	std::memset( jk2SpVoiceVolume, 0, sizeof( jk2SpVoiceVolume ) );
	void *symbol = nullptr;
	vm_t *vm = VM_CreateTable( VM_GAME, "GetGameAPI", SV_JK2SPGameEntry, &symbol );
	if ( !vm ) return nullptr;
	/* Publish ownership before GetGameAPI: its game-info initialization can
	 * call imports and drop, and server shutdown must then unload this VM. */
	gvm = vm;
	auto getGameAPI = reinterpret_cast<fntech3_jk2_sp_GetGameAPI_t>( symbol );
	auto *game = getGameAPI( &jk2SpImports );
	const auto error = fntech3::jk2::sp::ValidateExports( game );
	if ( error != fntech3::jk2::sp::ExportError::None ) {
		SV_JK2SPReset(); gvm = nullptr; VM_Free( vm );
		Com_Error( ERR_DROP, "Invalid Jedi Outcast SP game exports (%i)", static_cast<int>( error ) );
		return nullptr;
	}
	void *entry = Sys_LoadFunction( vm->dllHandle, "dllEntry" );
	void *main = Sys_LoadFunction( vm->dllHandle, "vmMain" );
	if ( !entry || !main ) {
		SV_JK2SPReset(); gvm = nullptr; VM_Free( vm );
		Com_Error( ERR_DROP, "Jedi Outcast SP game has no client entry points" );
		return nullptr;
	}
	reinterpret_cast<dllEntry_t>( entry )( SV_JK2SPCgameSyscall );
	jk2SpGame = game;
	Com_Printf( "Jedi Outcast SP GetGameAPI: version %i, imports %i bytes, entity stride %i\n",
		game->apiversion, FNTECH3_JK2_SP_GAME_IMPORT_BYTES, game->gentitySize );
	return vm;
#endif
}
}
#endif
