/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_Q3TEST_GAME_HPP
#define FNTECH3_SERVER_Q3TEST_GAME_HPP
#include "../compat/q3test_game_abi.h"
#include "../compat/q3test_state_adapter.hpp"
#include "server_world_bounds.hpp"
#include <cstdarg>
#include <memory>
#include <new>
#include <vector>

/* The Q3Test releases' qagamex86.dll, served through its own GetGameAPI
 * tables. Each release's engine builds one of four import tables (families
 * A to D in q3test_game_abi.h) and reads one of three export layouts; the
 * facts and addresses are in docs/fntech3/Q3TEST.md. Every retail engine
 * unloads and reloads the library for each spawn and restart, so its static
 * data starts afresh; this host does the same through VM_Free and a new
 * VM_CreateTable. No engine record is handed to the module: cvars, traces
 * and usercmds cross as the release's own records. */
namespace {

/* sv_game.cpp's flag table names these through Quake III's own bits. */
static_assert( FNTECH3_Q3TEST_SVF_NOCLIENT == SVF_NOCLIENT && FNTECH3_Q3TEST_SVF_BOT == SVF_BOT
	&& FNTECH3_Q3TEST_SVF_BROADCAST == SVF_BROADCAST && FNTECH3_Q3TEST_SVF_PORTAL == SVF_PORTAL,
	"Q3Test's server flags are Quake III's" );

enum class Q3TestFamily { None, A, B, C, D };

struct Q3TestCvar {
	char name[256]{}, string[1024]{}, reset[1024]{}, latched[1024]{};
	fntech3_q3test_cvar_t record{};
	fntech3_q3test_cvar102_t record102{};
	cvar_t *source = nullptr;
	int nativeFlags = 0;
	int serial = -1;
};

struct Q3TestGameState {
	Q3TestFamily family = Q3TestFamily::None;
	void *exports = nullptr;
	fntech3_q3test_game_import_ab_t importsAB{};
	fntech3_q3test_game_import_c_t importsC{};
	fntech3_q3test_game_import_d_t importsD{};
	bool initialized = false;
	int frameTime = 0;
	bool frameTimeKnown = false;
	/* The native usercmd the next ClientThink/ClientBegin receives, per
	 * client: 14, 20 or 24 bytes by release. */
	std::uint8_t commands[MAX_GAME_CLIENTS][24]{};
	std::uint8_t lastCommands[MAX_GAME_CLIENTS][24]{};
};
Q3TestGameState q3test;
/* Cvar records outlive the module, as the retail engine's do: its Cvar_Get
 * returns a record that stays valid for the process, and a reloaded module
 * registering the same name receives the same record. */
std::vector<std::unique_ptr<Q3TestCvar>> q3testCvars;

/* Retail FS handles: A/B hand out C FILE pointers, C/D small integers (1..7,
 * 1.06 FS_FileForHandle 0x413950). The module only ever passes them back, so
 * each is an opaque token for one of this engine's handles. */
constexpr int kQ3TestFileSlots = 8;
fileHandle_t q3testFiles[kQ3TestFileSlots]{};
unsigned char q3testFileTokens[kQ3TestFileSlots]{};

Q3TestFamily SV_Q3TestFamilyForRelease() {
	const q3testRelease_t *release = Com_Q3TestRelease();
	if ( !release ) return Q3TestFamily::None;
	switch ( release->id ) {
	case Q3TEST_RELEASE_1_02: case Q3TEST_RELEASE_1_03: return Q3TestFamily::A;
	case Q3TEST_RELEASE_1_05: return Q3TestFamily::B;
	case Q3TEST_RELEASE_1_06: case Q3TEST_RELEASE_1_07: return Q3TestFamily::C;
	case Q3TEST_RELEASE_1_08: return Q3TestFamily::D;
	default: return Q3TestFamily::None;
	}
}
bool SV_Q3TestNumberedClients() { return q3test.family == Q3TestFamily::C || q3test.family == Q3TestFamily::D; }
/* A/B's client n is entity n + 1 and the world is entity 0; C/D's client n
 * is entity n (ABI record 6.1). */
int SV_Q3TestClientEntity( int client ) { return SV_Q3TestNumberedClients() ? client : client + 1; }
std::size_t SV_Q3TestUsercmdBytes() {
	switch ( q3test.family ) {
	case Q3TestFamily::A: return 14;
	case Q3TestFamily::D: return 24;
	default: return 20;
	}
}

fntech3_q3test_game_export_a_t *SV_Q3TestA() { return static_cast<fntech3_q3test_game_export_a_t *>( q3test.exports ); }
fntech3_q3test_game_export_b_t *SV_Q3TestB() { return static_cast<fntech3_q3test_game_export_b_t *>( q3test.exports ); }
fntech3_q3test_game_export_cd_t *SV_Q3TestCD() { return static_cast<fntech3_q3test_game_export_cd_t *>( q3test.exports ); }

/* The entity array the module publishes. Every retail engine re-reads the
 * base, stride and count at each use (SV_GentityNum 1.02 0x41b520). */
void SV_Q3TestEntityArray( void *&base, int &stride, int &count, int &capacity ) {
	base = nullptr; stride = count = capacity = 0;
	if ( !q3test.exports ) return;
	switch ( q3test.family ) {
	case Q3TestFamily::A: {
		const auto *e = SV_Q3TestA();
		base = e->entities; stride = e->entitySize; count = e->numEntities;
		// Init publishes its allocation, the maxentities cvar (1.02 0x20012d80).
		capacity = e->maxEntities > 0 && e->maxEntities <= FNTECH3_Q3TEST_MAX_GENTITIES ? e->maxEntities : count;
		break;
	}
	case Q3TestFamily::B: {
		const auto *e = SV_Q3TestB();
		// 1.05's Init allocates 0x99000 bytes, 1024 entities (0x20012fdf).
		base = e->entities; stride = e->entitySize; count = e->numEntities; capacity = FNTECH3_Q3TEST_MAX_GENTITIES;
		break;
	}
	default: {
		const auto *e = SV_Q3TestCD();
		// 1.06-1.08 allocate a fixed 1024-entity array (0x91000 bytes, 0x2000f7c7).
		base = e->entities; stride = e->entitySize; count = e->numEntities; capacity = FNTECH3_Q3TEST_MAX_GENTITIES;
		break;
	}
	}
}

bool SV_Q3TestValidateClient( void *, const void *record, int, size_t ) { return record != nullptr; }

void SV_Q3TestBindEntities() {
	void *base; int stride, count, capacity;
	SV_Q3TestEntityArray( base, stride, count, capacity );
	if ( !base || count < 1 )
		Com_Error( ERR_DROP, "Q3Test game has no entity array" );
	if ( serverGameData.IsBound() && sv.gentities == base && sv.gentitySize == stride
		&& serverGameData.EntityCount() == count ) return;
	fntech3::server::NativeGameDataBinding binding;
	binding.entityBase = base;
	binding.entitySpanBytes = static_cast<std::size_t>( capacity ) * static_cast<std::size_t>( stride );
	binding.entityCount = count; binding.entityCapacity = capacity;
	binding.entityStride = static_cast<std::size_t>( stride );
	binding.clientCount = sv.maxclients > 0 ? sv.maxclients : 1;
	binding.validateClientRecord = SV_Q3TestValidateClient;
	const auto error = serverGameData.BindNative( COM_GAME_Q3TEST, binding );
	if ( error != fntech3::server::GameDataBindError::None )
		Com_Error( ERR_DROP, "Invalid Q3Test game data: %s", fntech3::server::GameDataBindErrorName( error ) );
	sv.gentities = static_cast<sharedEntity_t *>( base ); sv.gentitySize = stride; sv.num_entities = count;
}

int SV_Q3TestEntityIndex( const void *entity ) {
	SV_Q3TestBindEntities();
	fntech3::server::GameEntityRef reference;
	if ( !entity || !serverGameData.EntityRefForPointer( entity, reference ) )
		Com_Error( ERR_DROP, "Q3Test import received an invalid entity" );
	return reference.index;
}
sharedEntity_t *SV_Q3TestEntity( const void *entity ) {
	SV_Q3TestEntityIndex( entity );
	return static_cast<sharedEntity_t *>( const_cast<void *>( entity ) );
}
/* A/B name a client by its entity: n = index - 1, ignored unless 1 <= index
 * <= sv_maxclients (DropClient 1.02 0x41b650). -1 when it is not a client. */
int SV_Q3TestClientForEntity( const void *entity ) {
	const int index = SV_Q3TestEntityIndex( entity );
	return index >= 1 && index <= sv.maxclients ? index - 1 : -1;
}
void *SV_Q3TestEntityForClient( int client ) {
	SV_Q3TestBindEntities();
	return SV_GentityNum( SV_Q3TestClientEntity( client ) );
}

/* ---- cvars ---------------------------------------------------------------- */

struct Q3TestCvarFlag { int native, engine; };
/* A/B's scheme (1.02 Cvar_WriteVariables, Cvar_InfoString(4) and (8), the
 * write-protection prints); C/D's is Quake III 1.32's, with no CHEAT bit in C
 * and 0x200 in D (ABI record 5.7). */
constexpr Q3TestCvarFlag kQ3TestCvarFlagsAB[] = {
	{ 0x001, CVAR_ARCHIVE }, { 0x004, CVAR_USERINFO }, { 0x008, CVAR_SERVERINFO },
	{ 0x010, CVAR_INIT }, { 0x020, CVAR_LATCH }, { 0x040, CVAR_CHEAT },
	{ 0x080, CVAR_ROM }, { 0x100, CVAR_USER_CREATED }, { 0x200, CVAR_SYSTEMINFO }
};
constexpr Q3TestCvarFlag kQ3TestCvarFlagsCD[] = {
	{ 0x001, CVAR_ARCHIVE }, { 0x002, CVAR_USERINFO }, { 0x004, CVAR_SERVERINFO },
	{ 0x008, CVAR_SYSTEMINFO }, { 0x010, CVAR_INIT }, { 0x020, CVAR_LATCH },
	{ 0x040, CVAR_ROM }, { 0x080, CVAR_USER_CREATED }, { 0x100, CVAR_TEMP }
};
int SV_Q3TestCvarFlagsToEngine( int flags ) {
	int result = 0;
	if ( q3test.family == Q3TestFamily::A || q3test.family == Q3TestFamily::B ) {
		for ( const auto &bit : kQ3TestCvarFlagsAB ) if ( flags & bit.native ) result |= bit.engine;
	} else {
		for ( const auto &bit : kQ3TestCvarFlagsCD ) if ( flags & bit.native ) result |= bit.engine;
		if ( q3test.family == Q3TestFamily::D && ( flags & FNTECH3_Q3TEST_CVAR_D_CHEAT ) ) result |= CVAR_CHEAT;
	}
	return result;
}
int SV_Q3TestCvarFlagsToNative( int engine, int previous ) {
	int result = previous;
	const bool ab = q3test.family == Q3TestFamily::A || q3test.family == Q3TestFamily::B;
	const auto apply = [&]( const auto &table ) {
		for ( const auto &bit : table ) {
			result &= ~bit.native;
			if ( engine & bit.engine ) result |= bit.native;
		}
	};
	if ( ab ) apply( kQ3TestCvarFlagsAB ); else apply( kQ3TestCvarFlagsCD );
	if ( q3test.family == Q3TestFamily::D ) {
		result &= ~FNTECH3_Q3TEST_CVAR_D_CHEAT;
		if ( engine & CVAR_CHEAT ) result |= FNTECH3_Q3TEST_CVAR_D_CHEAT;
	}
	return result;
}
std::uint32_t SV_Q3TestAddress( const void *pointer ) {
	return static_cast<std::uint32_t>( reinterpret_cast<std::uintptr_t>( pointer ) );
}
void SV_Q3TestSyncCvar( Q3TestCvar &node, bool created ) {
	if ( Cvar_Flags( node.name ) & CVAR_PRIVATE )
		Com_Error( ERR_DROP, "Q3Test game requested private cvar %s", node.name );
	if ( !node.source || !node.source->name || Q_stricmp( node.name, node.source->name ) )
		node.source = Cvar_Get( node.name, node.reset, SV_Q3TestCvarFlagsToEngine( node.nativeFlags ) );
	const cvar_t *v = node.source;
	const bool changed = node.serial != v->modificationCount;
	Q_strncpyz( node.string, v->string ? v->string : "", sizeof( node.string ) );
	Q_strncpyz( node.reset, v->resetString ? v->resetString : node.reset, sizeof( node.reset ) );
	Q_strncpyz( node.latched, v->latchedString ? v->latchedString : "", sizeof( node.latched ) );
	const int flags = SV_Q3TestCvarFlagsToNative( v->flags, node.nativeFlags );
	node.nativeFlags = flags;
	auto fill = [&]( auto &record ) {
		record.name = SV_Q3TestAddress( node.name );
		record.string = SV_Q3TestAddress( node.string );
		record.resetString = SV_Q3TestAddress( node.reset );
		record.latchedString = v->latchedString ? SV_Q3TestAddress( node.latched ) : 0;
		record.flags = flags;
		if ( created || changed ) record.modified = 1;
		record.value = v->value;
		record.integer = v->integer;
	};
	fill( node.record ); fill( node.record102 );
	if ( created || changed ) node.record.modificationCount++;
	node.serial = v->modificationCount;
}
void SV_Q3TestSyncCvars() {
	for ( auto &node : q3testCvars ) SV_Q3TestSyncCvar( *node, false );
}
void *SV_Q3TestCvarRecord( Q3TestCvar &node ) {
	const q3testRelease_t *release = Com_Q3TestRelease();
	return release && release->id == Q3TEST_RELEASE_1_02
		? static_cast<void *>( &node.record102 ) : static_cast<void *>( &node.record );
}
void *SV_Q3TestCvarGet( const char *name, const char *value, int flags ) {
	if ( !name || !value || !*name || strlen( name ) >= 256 || strlen( value ) >= 1024 )
		Com_Error( ERR_DROP, "Invalid Q3Test cvar registration" );
	if ( Cvar_Flags( name ) & CVAR_PRIVATE )
		Com_Error( ERR_DROP, "Q3Test game requested private cvar %s", name );
	Q3TestCvar *node = nullptr;
	for ( auto &candidate : q3testCvars ) if ( !Q_stricmp( candidate->name, name ) ) node = candidate.get();
	const bool created = !node;
	if ( created ) {
		q3testCvars.emplace_back( new( std::nothrow ) Q3TestCvar );
		node = q3testCvars.back().get();
		if ( !node ) Com_Error( ERR_DROP, "Q3Test cvar allocation failed" );
		Q_strncpyz( node->name, name, sizeof( node->name ) );
	}
	Q_strncpyz( node->reset, value, sizeof( node->reset ) );
	node->nativeFlags |= flags;
	node->source = Cvar_Get( name, value, SV_Q3TestCvarFlagsToEngine( flags ) );
	/* A/B force the value when the caller registers it ROM (1.02 Cvar_Get
	 * 0x40fef0: `if (flags & 0x80) Cvar_Set2(name, value, 1)`). */
	if ( ( q3test.family == Q3TestFamily::A || q3test.family == Q3TestFamily::B )
		&& ( flags & FNTECH3_Q3TEST_CVAR_AB_ROM ) ) Cvar_Set2( name, value, qtrue );
	SV_Q3TestSyncCvar( *node, created );
	// Link the records the way the engine's cvar list would.
	for ( std::size_t i = 0; i < q3testCvars.size(); ++i ) {
		const auto next = i + 1 < q3testCvars.size() ? SV_Q3TestAddress( SV_Q3TestCvarRecord( *q3testCvars[i + 1] ) ) : 0u;
		q3testCvars[i]->record.next = next; q3testCvars[i]->record102.next = next;
	}
	return SV_Q3TestCvarRecord( *node );
}
/* Cvar_Set2(name, value, force = 1) in every release (1.02 0x410310). */
void SV_Q3TestCvarSet( const char *name, const char *value ) {
	if ( !name || !*name || strlen( name ) >= 256 || ( value && strlen( value ) >= 1024 )
		|| ( Cvar_Flags( name ) & CVAR_PRIVATE ) ) Com_Error( ERR_DROP, "Invalid Q3Test cvar assignment" );
	Cvar_Set2( name, value ? value : "", qtrue );
	SV_Q3TestSyncCvars();
}
int SV_Q3TestCvarVariableIntegerValue( const char *name ) {
	if ( !name || ( Cvar_Flags( name ) & CVAR_PRIVATE ) ) return 0;
	return Cvar_VariableIntegerValue( name );
}
void SV_Q3TestCvarVariableStringBuffer( const char *name, char *buffer, int size ) {
	if ( !buffer || size < 1 ) Com_Error( ERR_DROP, "Invalid Q3Test cvar buffer" );
	if ( !name || ( Cvar_Flags( name ) & CVAR_PRIVATE ) ) { buffer[0] = '\0'; return; }
	Cvar_VariableStringBuffer( name, buffer, size );
}

/* ---- printing, errors, commands ------------------------------------------ */

void SV_Q3TestPrintf( const char *fmt, ... ) {
	char text[MAXPRINTMSG]; va_list args; va_start( args, fmt ); Q_vsnprintf( text, sizeof text, fmt, args ); va_end( args );
	Com_Printf( "%s", text );
}
/* A/B pass Com_Error's level through: 0 fatal, 1 drop, 2 disconnect (1.02
 * Com_Error 0x40eac0). A module cannot end the process here, so fatal and
 * drop both unwind the server. */
void SV_Q3TestErrorAB( int level, const char *fmt, ... ) {
	char text[MAXPRINTMSG]; va_list args; va_start( args, fmt ); Q_vsnprintf( text, sizeof text, fmt, args ); va_end( args );
	Com_Error( level == 2 ? ERR_DISCONNECT : ERR_DROP, "%s", text );
}
void SV_Q3TestErrorCD( const char *text ) { Com_Error( ERR_DROP, "%s", text ? text : "" ); }
int SV_Q3TestMilliseconds() { return Sys_Milliseconds(); }
int SV_Q3TestArgc() { return Cmd_Argc(); }
char *SV_Q3TestArgv( int n ) { return const_cast<char *>( Cmd_Argv( n ) ); }
void SV_Q3TestAddCommandText( const char *text ) { if ( text ) Cbuf_AddText( text ); }
void SV_Q3TestDebugGraph( float, int ) { /* No graph on a server. */ }

/* ---- files ---------------------------------------------------------------- */

int SV_Q3TestFileSlot( fileHandle_t handle ) {
	for ( int i = 1; i < kQ3TestFileSlots; ++i ) if ( !q3testFiles[i] ) { q3testFiles[i] = handle; return i; }
	FS_FCloseFile( handle );
	return 0;
}
fileHandle_t SV_Q3TestFileFromToken( const void *token ) {
	const auto *byte = static_cast<const unsigned char *>( token );
	if ( byte < q3testFileTokens + 1 || byte >= q3testFileTokens + kQ3TestFileSlots ) return 0;
	return q3testFiles[byte - q3testFileTokens];
}
fileHandle_t SV_Q3TestFileFromHandle( int handle ) {
	if ( handle < 1 || handle >= kQ3TestFileSlots || !q3testFiles[handle] )
		Com_Error( ERR_DROP, "Q3Test FS_FileForHandle: bad handle %i", handle );
	return q3testFiles[handle];
}
void SV_Q3TestCloseFiles() {
	for ( int i = 1; i < kQ3TestFileSlots; ++i ) if ( q3testFiles[i] ) { FS_FCloseFile( q3testFiles[i] ); q3testFiles[i] = 0; }
}
/* A/B: FS_FOpenFileRead(qpath, FILE **, unique) returns the length or -1. */
int SV_Q3TestFOpenFileRead( const char *qpath, void **file, int unique ) {
	if ( !qpath || !*qpath ) Com_Error( ERR_DROP, "Invalid Q3Test file read" );
	fileHandle_t handle = 0;
	const int length = FS_FOpenFileRead( qpath, file ? &handle : nullptr, unique ? qtrue : qfalse );
	if ( !file ) return length;
	*file = nullptr;
	if ( !handle ) return -1;
	const int slot = SV_Q3TestFileSlot( handle );
	if ( !slot ) return -1;
	*file = q3testFileTokens + slot;
	return length;
}
void *SV_Q3TestFOpenFileWrite( const char *qpath ) {
	if ( !FS_WriteQpathIsValid( qpath ) ) Com_Error( ERR_DROP, "Invalid Q3Test file write" );
	const fileHandle_t handle = FS_FOpenFileWrite( qpath );
	if ( !handle ) return nullptr;
	const int slot = SV_Q3TestFileSlot( handle );
	return slot ? static_cast<void *>( q3testFileTokens + slot ) : nullptr;
}
void SV_Q3TestReadAB( void *buffer, int length, void *file ) {
	const fileHandle_t handle = SV_Q3TestFileFromToken( file );
	if ( !handle || !buffer || length < 0 ) Com_Error( ERR_DROP, "Invalid Q3Test file read" );
	FS_Read( buffer, length, handle );
}
void SV_Q3TestWriteAB( const void *buffer, int length, void *file ) {
	const fileHandle_t handle = SV_Q3TestFileFromToken( file );
	if ( !handle || !buffer || length < 0 ) Com_Error( ERR_DROP, "Invalid Q3Test file write" );
	FS_Write( buffer, length, handle );
}
void SV_Q3TestFCloseFileAB( void *file ) {
	const auto *byte = static_cast<const unsigned char *>( file );
	if ( byte < q3testFileTokens + 1 || byte >= q3testFileTokens + kQ3TestFileSlots ) return;
	const int slot = static_cast<int>( byte - q3testFileTokens );
	if ( q3testFiles[slot] ) { FS_FCloseFile( q3testFiles[slot] ); q3testFiles[slot] = 0; }
}
/* C/D: mode 0 read returns the length or -1, 1 write and 2 append return 0,
 * and the handle is 0 when nothing opened (1.06 0x4139d0). */
int SV_Q3TestFOpenFileByMode( const char *qpath, int *handle, int mode ) {
	if ( !qpath || !*qpath || !handle ) Com_Error( ERR_DROP, "Invalid Q3Test file open" );
	*handle = 0;
	if ( mode < 0 || mode > 2 ) Com_Error( ERR_DROP, "FSH_FOpenFile: bad mode" );
	if ( mode != 0 && !FS_WriteQpathIsValid( qpath ) ) Com_Error( ERR_DROP, "Invalid Q3Test file write" );
	fileHandle_t file = 0;
	const int length = FS_FOpenFileByMode( qpath, &file, mode == 0 ? FS_READ : mode == 1 ? FS_WRITE : FS_APPEND );
	if ( !file ) return mode == 0 ? -1 : 0;
	*handle = SV_Q3TestFileSlot( file );
	if ( !*handle ) return mode == 0 ? -1 : 0;
	return mode == 0 ? length : 0;
}
void SV_Q3TestReadCD( void *buffer, int length, int handle ) {
	if ( !buffer || length < 0 ) Com_Error( ERR_DROP, "Invalid Q3Test file read" );
	FS_Read( buffer, length, SV_Q3TestFileFromHandle( handle ) );
}
void SV_Q3TestWriteCD( const void *buffer, int length, int handle ) {
	if ( !buffer || length < 0 ) Com_Error( ERR_DROP, "Invalid Q3Test file write" );
	FS_Write( buffer, length, SV_Q3TestFileFromHandle( handle ) );
}
void SV_Q3TestFCloseFileCD( int handle ) {
	FS_FCloseFile( SV_Q3TestFileFromHandle( handle ) );
	q3testFiles[handle] = 0;
}

/* ---- clients, commands, configstrings ------------------------------------ */

void SV_Q3TestDropClientAB( q3tEnt *entity, const char *reason ) {
	const int client = SV_Q3TestClientForEntity( entity );
	if ( client >= 0 ) SV_GameDropClient( client, reason ? reason : "" );
}
void SV_Q3TestDropClientCD( int client, const char *reason ) { SV_GameDropClient( client, reason ? reason : "" ); }
/* A/B format into 1024 bytes and C/D into 8192 (1.02 0x41b5b4, 1.06
 * 0x41c3e0); retail's vsprintf had no bound, this one truncates. */
void SV_Q3TestSendServerCommandAB( q3tEnt *entity, const char *fmt, ... ) {
	char text[1024]; va_list args; va_start( args, fmt ); Q_vsnprintf( text, sizeof text, fmt, args ); va_end( args );
	if ( !entity ) { SV_GameSendServerCommand( -1, text ); return; }
	const int client = SV_Q3TestClientForEntity( entity );
	if ( client >= 0 ) SV_GameSendServerCommand( client, text );
}
void SV_Q3TestSendServerCommandCD( int client, const char *fmt, ... ) {
	char text[8192]; va_list args; va_start( args, fmt ); Q_vsnprintf( text, sizeof text, fmt, args ); va_end( args );
	if ( client == -1 || ( client >= 0 && client < sv.maxclients ) ) SV_GameSendServerCommand( client, text );
}
void SV_Q3TestSetConfigstring( int index, const char *value ) {
	if ( index < 0 || index >= FNTECH3_Q3TEST_MAX_CONFIGSTRINGS )
		Com_Error( ERR_DROP, "SV_SetConfigstring: bad index %i", index );
	SV_SetConfigstring( index, value ? value : "" );
}
/* A/B return the engine's own string, "" for a bad index (1.02 0x41bb00). */
const char *SV_Q3TestGetConfigstringAB( int index ) {
	if ( index < 0 || index >= FNTECH3_Q3TEST_MAX_CONFIGSTRINGS || !sv.configstrings[index] ) return "";
	return sv.configstrings[index];
}
void SV_Q3TestGetConfigstringCD( int index, char *buffer, int size ) {
	if ( size < 1 || !buffer ) Com_Error( ERR_DROP, "SV_GetConfigstring: bufferSize == %i", size );
	if ( index < 0 || index >= FNTECH3_Q3TEST_MAX_CONFIGSTRINGS ) Com_Error( ERR_DROP, "SV_GetConfigstring: bad index %i", index );
	SV_GetConfigstring( index, buffer, size );
}
void SV_Q3TestGetUserinfo( int client, char *buffer, int size ) {
	if ( client < 0 || client >= sv.maxclients ) Com_Error( ERR_DROP, "SV_GetUserinfo: bad index %i", client );
	if ( !buffer || size < 1 ) Com_Error( ERR_DROP, "SV_GetUserinfo: bufferSize == %i", size );
	Q_strncpyz( buffer, svs.clients[client].userinfo, size );
}
/* C/D copy the string and do not reparse name, rate or snaps (1.06 0x41ca00). */
void SV_Q3TestSetUserinfo( int client, const char *userinfo ) {
	if ( client < 0 || client >= sv.maxclients ) Com_Error( ERR_DROP, "SV_SetUserinfo: bad index %i", client );
	Q_strncpyz( svs.clients[client].userinfo, userinfo ? userinfo : "", sizeof( svs.clients[client].userinfo ) );
}
void SV_Q3TestGetServerinfo( char *buffer, int size ) {
	if ( size < 1 || !buffer ) Com_Error( ERR_DROP, "SV_GetServerinfo: bufferSize == %i", size );
	/* Retail's is Cvar_InfoString( CVAR_SERVERINFO ) over the release's own
	 * cvars: the release's serverinfo, not every key this engine marks. */
	Q_strncpyz( buffer, SV_Q3TestServerinfo(), size );
}

/* ---- world ---------------------------------------------------------------- */

void SV_Q3TestSetBrushModel( q3tEnt *entity, const char *name ) { SV_SetBrushModel( SV_Q3TestEntity( entity ), name ); }
/* 56 bytes into the caller's record (1.02 SV_Trace 0x41e690). A/B report the
 * world, and no hit, as entity 0: their CM_BoxTrace never writes entityNum
 * (1.02 0x40d950). C/D report 1022 and 1023, as this engine does. */
void SV_Q3TestTrace( fntech3_q3test_trace_t *out, const float *start, const float *mins, const float *maxs,
	const float *end, int pass, int mask ) {
	if ( !out || !start || !end ) Com_Error( ERR_DROP, "Invalid Q3Test trace" );
	SV_Q3TestBindEntities();
	trace_t trace{};
	SV_Trace( &trace, start, mins ? mins : vec3_origin, maxs ? maxs : vec3_origin, end, pass, mask, qfalse );
	fntech3_q3test_trace_t result{};
	result.allsolid = trace.allsolid; result.startsolid = trace.startsolid; result.fraction = trace.fraction;
	VectorCopy( trace.endpos, result.endpos ); VectorCopy( trace.plane.normal, result.planeNormal );
	result.planeDist = trace.plane.dist; result.planeType = trace.plane.type; result.planeSignbits = trace.plane.signbits;
	result.surfaceFlags = trace.surfaceFlags; result.contents = trace.contents;
	result.entityNum = trace.entityNum;
	if ( !SV_Q3TestNumberedClients() && ( trace.entityNum == ENTITYNUM_WORLD || trace.entityNum == ENTITYNUM_NONE ) )
		result.entityNum = 0;
	*out = result;
}
int SV_Q3TestPointContents( const float *point, int pass ) {
	if ( !point ) Com_Error( ERR_DROP, "Invalid Q3Test point" );
	SV_Q3TestBindEntities();
	return SV_PointContents( point, pass );
}
int SV_Q3TestInPVS( const float *a, const float *b ) {
	if ( !SV_FiniteWorldVector( a ) || !SV_FiniteWorldVector( b ) ) Com_Error( ERR_DROP, "Invalid Q3Test PVS points" );
	return SV_inPVS( a, b );
}
int SV_Q3TestInPVSIgnorePortals( const float *a, const float *b ) {
	if ( !SV_FiniteWorldVector( a ) || !SV_FiniteWorldVector( b ) ) Com_Error( ERR_DROP, "Invalid Q3Test PVS points" );
	return SV_inPVSIgnorePortals( a, b ) ? 1 : 0;
}
void SV_Q3TestAdjustAreaPortalState( q3tEnt *entity, int open ) { SV_AdjustAreaPortalState( SV_Q3TestEntity( entity ), open != 0 ); }
int SV_Q3TestAreasConnected( int a, int b ) { return CM_AreasConnected( a, b ); }
/* A/B never link the world, entity 0 (1.02 SV_LinkEntity 0x41de5a). */
void SV_Q3TestLinkEntity( q3tEnt *entity ) {
	if ( !SV_Q3TestNumberedClients() && SV_Q3TestEntityIndex( entity ) == 0 ) return;
	SV_LinkEntity( SV_Q3TestEntity( entity ) );
}
void SV_Q3TestUnlinkEntity( q3tEnt *entity ) { SV_UnlinkEntity( SV_Q3TestEntity( entity ) ); }
/* A/B ContactEntities (1.02 0x41e3d0): the area query, then brush models must
 * hold the box inside them at their current transform; C/D EntitiesInBox is
 * the area query alone. Both write gentity pointers. */
int SV_Q3TestEntityList( const float *mins, const float *maxs, q3tEnt **list, int capacity, bool contact ) {
	SV_Q3TestBindEntities();
	if ( capacity < 0 || ( capacity && !list ) ) Com_Error( ERR_DROP, "Q3Test entity list output is invalid" );
	if ( !capacity ) return 0;
	if ( !SV_ValidWorldBounds( mins, maxs ) ) Com_Error( ERR_DROP, "Invalid Q3Test entity list bounds" );
	int numbers[MAX_GENTITIES];
	const int found = SV_AreaEntities( mins, maxs, numbers, MAX_GENTITIES );
	int count = 0;
	for ( int i = 0; i < found && count < capacity; ++i ) {
		sharedEntity_t *entity = SV_GentityNum( numbers[i] );
		if ( contact ) {
			sharedEntity_t view;
			if ( !SV_ReadGameEntity( entity, &view ) ) Com_Error( ERR_DROP, "Q3Test contact entity unavailable" );
			if ( view.r.bmodel && !SV_EntityContact( mins, maxs, entity, false ) ) continue;
		}
		list[count++] = reinterpret_cast<q3tEnt *>( entity );
	}
	return count;
}
int SV_Q3TestContactEntities( const float *mins, const float *maxs, q3tEnt **list, int capacity ) {
	return SV_Q3TestEntityList( mins, maxs, list, capacity, true );
}
int SV_Q3TestEntitiesInBox( const float *mins, const float *maxs, q3tEnt **list, int capacity ) {
	return SV_Q3TestEntityList( mins, maxs, list, capacity, false );
}
/* C/D EntityContact (1.06 0x41c6a0): the transformed box trace's startsolid
 * for any entity type. */
int SV_Q3TestEntityContact( const float *mins, const float *maxs, const q3tEnt *entity ) {
	if ( !SV_ValidWorldBounds( mins, maxs ) ) Com_Error( ERR_DROP, "Invalid Q3Test contact bounds" );
	return SV_EntityContact( mins, maxs, SV_Q3TestEntity( entity ), false ) ? 1 : 0;
}

/* ---- snapshots and bots (A/B, D) ----------------------------------------- */

/* A/B: the client's frame for its current outgoing sequence
 * (SnapshotEntityCount 1.02 0x41d850, Number 0x41d890). */
const clientSnapshot_t *SV_Q3TestClientFrame( q3tEnt *entity ) {
	const int client = SV_Q3TestClientForEntity( entity );
	if ( client < 0 ) Com_Error( ERR_DROP, "Q3Test snapshot import received a non-client entity" );
	const client_t &cl = svs.clients[client];
	return &cl.frames[cl.netchan.outgoingSequence & PACKET_MASK];
}
int SV_Q3TestSnapshotEntityCount( q3tEnt *entity ) { return SV_Q3TestClientFrame( entity )->num_entities; }
int SV_Q3TestSnapshotEntityNumber( q3tEnt *entity, int index ) {
	const clientSnapshot_t *frame = SV_Q3TestClientFrame( entity );
	if ( index < 0 || index >= frame->num_entities || !frame->ents[index] )
		Com_Error( ERR_DROP, "Q3Test snapshot entity %i is outside the client's %i entities", index, frame->num_entities );
	return frame->ents[index]->number;
}
/* The first free slot becomes a connected bot client (A/B 1.02 0x419ad0, D
 * 1.08 0x41a7a0): its entity's number is written and it is cached. */
q3tEnt *SV_Q3TestBotAllocateClient() {
	SV_Q3TestBindEntities();
	for ( int slot = 0; slot < sv.maxclients; ++slot ) {
		client_t &cl = svs.clients[slot];
		if ( cl.state != CS_FREE ) continue;
		sharedEntity_t *entity = SV_GentityNum( SV_Q3TestClientEntity( slot ) );
		fntech3::server::GameEntityRef reference;
		if ( !serverGameData.EntityRefForPointer( entity, reference )
			|| !serverGameData.SetNumber( reference, SV_Q3TestClientEntity( slot ) ) )
			Com_Error( ERR_DROP, "Q3Test bot client %i cannot be published", slot );
		cl.gentity = entity;
		cl.state = CS_CONNECTED;
		cl.lastPacketTime = svs.time;
		cl.netchan.remoteAddress.type = NA_BOT;
		cl.rate = 16384;
		return reinterpret_cast<q3tEnt *>( entity );
	}
	return nullptr;
}
/* The slot becomes a zombie and loses its name; no game callback (1.02
 * 0x419b50, 1.08 0x41a820). */
void SV_Q3TestBotFreeClient( q3tEnt *entity ) {
	const int index = SV_Q3TestEntityIndex( entity );
	const int client = SV_Q3TestNumberedClients() ? index : index - 1;
	if ( client < 0 || client >= sv.maxclients ) Com_Error( ERR_DROP, "Q3Test bot release outside the client range" );
	svs.clients[client].state = CS_ZOMBIE;
	svs.clients[client].name[0] = '\0';
}
/* A/B: 1023-byte copy, ClientUserinfoChanged, then the engine reparse (1.02 0x419b90). */
void SV_Q3TestBotSetUserinfo( q3tEnt *entity, const char *userinfo ) {
	const int client = SV_Q3TestClientForEntity( entity );
	if ( client < 0 ) Com_Error( ERR_DROP, "Q3Test bot userinfo for a non-client entity" );
	Q_strncpyz( svs.clients[client].userinfo, userinfo ? userinfo : "", 1024 );
	VM_Call( gvm, 1, GAME_CLIENT_USERINFO_CHANGED, client );
	SV_UserinfoChanged( &svs.clients[client], qtrue, qfalse );
}
/* A/B SV_BotFrame (1.02 0x419c50): every slot whose cached entity carries
 * the bot flag gets BotBegin on its first visit, then BotThink(msec). */
void SV_Q3TestBotFrame( int msec ) {
	if ( q3test.family != Q3TestFamily::A && q3test.family != Q3TestFamily::B ) return;
	for ( int slot = 0; slot < sv.maxclients; ++slot ) {
		client_t &cl = svs.clients[slot];
		if ( cl.state < CS_CONNECTED || !cl.gentity ) continue;
		sharedEntity_t view;
		if ( !SV_ReadGameEntity( cl.gentity, &view ) || !( view.r.svFlags & SVF_BOT ) ) continue;
		cl.lastPacketTime = svs.time;
		auto *entity = static_cast<q3tEnt *>( cl.gentity );
		if ( cl.state != CS_ACTIVE ) {
			cl.state = CS_ACTIVE;
			if ( q3test.family == Q3TestFamily::A ) SV_Q3TestA()->BotBegin( entity );
			else SV_Q3TestB()->BotBegin( entity );
		} else if ( q3test.family == Q3TestFamily::A ) {
			SV_Q3TestA()->BotThink( entity, msec );
		} else {
			SV_Q3TestB()->BotThink( entity, msec );
		}
	}
}

/* ---- exports -------------------------------------------------------------- */

void SV_Q3TestClearGame() {
	q3test.exports = nullptr; q3test.initialized = false; q3test.frameTimeKnown = false;
	SV_Q3TestCloseFiles();
	InvalidateServerGameData();
}

/* The native command the next ClientThink or ClientBegin receives. The
 * Q3Test client path stores it before calling into the game. */
void *SV_Q3TestCommand( int client ) {
	if ( client < 0 || client >= MAX_GAME_CLIENTS ) Com_Error( ERR_DROP, "Q3Test command for a bad client %i", client );
	return q3test.commands[client];
}

intptr_t QDECL SV_Q3TestGameEntry( int command, ... ) {
	if ( command == GAME_SHUTDOWN ) {
		if ( q3test.initialized ) {
			q3test.initialized = false; // Error cleanup must not re-enter Shutdown.
			switch ( q3test.family ) {
			case Q3TestFamily::A: SV_Q3TestA()->Shutdown(); break;
			case Q3TestFamily::B: SV_Q3TestB()->Shutdown(); break;
			default: SV_Q3TestCD()->Shutdown(); break;
			}
		}
		SV_Q3TestClearGame();
		return 0;
	}
	if ( !q3test.exports ) Com_Error( ERR_DROP, "Q3Test game called without exports" );
	SV_Q3TestSyncCvars();
	va_list args; va_start( args, command );
	intptr_t result = 0;
	const bool ab = !SV_Q3TestNumberedClients();
	switch ( command ) {
	case GAME_INIT: {
		const int levelTime = va_arg( args, int );
		const char *mapname = Cvar_VariableString( "mapname" );
		// The level's text: a .ent override's when the server loaded one.
		const char *entities = SV_EntityText();
		q3test.frameTime = levelTime; q3test.frameTimeKnown = true;
		switch ( q3test.family ) {
		case Q3TestFamily::A:
			/* Init() at load, then SpawnEntities(map, entities, time)
			 * (1.02 SV_InitGameProgs 0x41b9ea, SV_SpawnServer 0x41bdaf). */
			SV_Q3TestA()->Init(); q3test.initialized = true;
			SV_Q3TestA()->SpawnEntities( mapname, entities, levelTime );
			break;
		case Q3TestFamily::B:
			SV_Q3TestB()->Init( mapname, entities, levelTime, com_frameTime ); q3test.initialized = true;
			break;
		default:
			SV_Q3TestCD()->Init( mapname, entities, levelTime, com_frameTime, Com_Milliseconds() ); q3test.initialized = true;
			break;
		}
		break;
	}
	case GAME_CLIENT_CONNECT: {
		const int client = va_arg( args, int );
		const int firstTime = va_arg( args, int );
		if ( client < 0 || client >= sv.maxclients ) Com_Error( ERR_DROP, "Q3Test ClientConnect: bad client %i", client );
		if ( ab ) {
			auto *entity = static_cast<q3tEnt *>( SV_Q3TestEntityForClient( client ) );
			char *userinfo = svs.clients[client].userinfo;
			result = reinterpret_cast<intptr_t>( q3test.family == Q3TestFamily::A
				? SV_Q3TestA()->ClientConnect( entity, userinfo ) : SV_Q3TestB()->ClientConnect( entity, userinfo ) );
		} else {
			result = reinterpret_cast<intptr_t>( SV_Q3TestCD()->ClientConnect( client, firstTime ? 1 : 0 ) );
		}
		break;
	}
	case GAME_CLIENT_BEGIN: {
		const int client = va_arg( args, int );
		void *cmd = SV_Q3TestCommand( client );
		if ( q3test.family == Q3TestFamily::A ) SV_Q3TestA()->ClientBegin( static_cast<q3tEnt *>( SV_Q3TestEntityForClient( client ) ), cmd );
		else if ( q3test.family == Q3TestFamily::B ) SV_Q3TestB()->ClientBegin( static_cast<q3tEnt *>( SV_Q3TestEntityForClient( client ) ), cmd );
		else SV_Q3TestCD()->ClientBegin( client, cmd );
		break;
	}
	case GAME_CLIENT_USERINFO_CHANGED: {
		const int client = va_arg( args, int );
		if ( client < 0 || client >= sv.maxclients ) Com_Error( ERR_DROP, "Q3Test userinfo: bad client %i", client );
		if ( q3test.family == Q3TestFamily::A ) SV_Q3TestA()->ClientUserinfoChanged( static_cast<q3tEnt *>( SV_Q3TestEntityForClient( client ) ), svs.clients[client].userinfo );
		else if ( q3test.family == Q3TestFamily::B ) SV_Q3TestB()->ClientUserinfoChanged( static_cast<q3tEnt *>( SV_Q3TestEntityForClient( client ) ), svs.clients[client].userinfo );
		else SV_Q3TestCD()->ClientUserinfoChanged( client );
		break;
	}
	case GAME_CLIENT_DISCONNECT: {
		const int client = va_arg( args, int );
		if ( q3test.family == Q3TestFamily::A ) SV_Q3TestA()->ClientDisconnect( static_cast<q3tEnt *>( SV_Q3TestEntityForClient( client ) ) );
		else if ( q3test.family == Q3TestFamily::B ) SV_Q3TestB()->ClientDisconnect( static_cast<q3tEnt *>( SV_Q3TestEntityForClient( client ) ) );
		else SV_Q3TestCD()->ClientDisconnect( client );
		break;
	}
	case GAME_CLIENT_COMMAND: {
		const int client = va_arg( args, int );
		if ( q3test.family == Q3TestFamily::A ) SV_Q3TestA()->ClientCommand( static_cast<q3tEnt *>( SV_Q3TestEntityForClient( client ) ) );
		else if ( q3test.family == Q3TestFamily::B ) SV_Q3TestB()->ClientCommand( static_cast<q3tEnt *>( SV_Q3TestEntityForClient( client ) ) );
		else SV_Q3TestCD()->ClientCommand( client );
		break;
	}
	case GAME_CLIENT_THINK: {
		const int client = va_arg( args, int );
		void *cmd = SV_Q3TestCommand( client );
		if ( q3test.family == Q3TestFamily::A ) SV_Q3TestA()->ClientThink( static_cast<q3tEnt *>( SV_Q3TestEntityForClient( client ) ), cmd );
		else if ( q3test.family == Q3TestFamily::B ) SV_Q3TestB()->ClientThink( static_cast<q3tEnt *>( SV_Q3TestEntityForClient( client ) ), cmd );
		else SV_Q3TestCD()->ClientThink( client, cmd );
		break;
	}
	case GAME_RUN_FRAME: {
		const int time = va_arg( args, int );
		/* A/B run the engine-driven bots inside the step, before RunFrame
		 * (1.02 SV_Frame 0x41cff0); the step is the distance from the last. */
		const int msec = q3test.frameTimeKnown && time > q3test.frameTime ? time - q3test.frameTime : 0;
		q3test.frameTime = time; q3test.frameTimeKnown = true;
		SV_Q3TestBotFrame( msec );
		switch ( q3test.family ) {
		case Q3TestFamily::A: SV_Q3TestA()->RunFrame( time ); break;
		case Q3TestFamily::B: SV_Q3TestB()->RunFrame( time ); break;
		default: SV_Q3TestCD()->RunFrame( time ); break;
		}
		break;
	}
	case GAME_CONSOLE_COMMAND:
		switch ( q3test.family ) {
		case Q3TestFamily::A: result = SV_Q3TestA()->ConsoleCommand(); break;
		case Q3TestFamily::B: result = SV_Q3TestB()->ConsoleCommand(); break;
		default: result = SV_Q3TestCD()->ConsoleCommand(); break;
		}
		break;
	case BOTAI_START_FRAME:
		break; // Q3Test's bot AI is not driven through Quake III's botlib.
	default:
		va_end( args );
		Com_Error( ERR_DROP, "Q3Test game export %i is not served", command );
		return 0;
	}
	va_end( args );
	SV_Q3TestSyncCvars();
	if ( q3test.initialized ) SV_Q3TestBindEntities();
	return result;
}

/* A/B's partial-command hook, called once per accepted move packet after the
 * ClientThink calls unless sv_nopredict is set (1.02 0x41b45b). */
void SV_Q3TestClientAfterCommands( int client, const void *cmd ) {
	if ( q3test.family != Q3TestFamily::A && q3test.family != Q3TestFamily::B ) return;
	if ( client < 0 || client >= sv.maxclients || !cmd ) return;
	std::memcpy( q3test.commands[client], cmd, SV_Q3TestUsercmdBytes() );
	auto *entity = static_cast<q3tEnt *>( SV_Q3TestEntityForClient( client ) );
	if ( q3test.family == Q3TestFamily::A ) SV_Q3TestA()->ClientAfterCommands( entity, q3test.commands[client] );
	else SV_Q3TestB()->ClientAfterCommands( entity, q3test.commands[client] );
}

void SV_Q3TestFillImports() {
	auto &ab = q3test.importsAB;
	ab = {};
	ab.Printf = SV_Q3TestPrintf; ab.Error = SV_Q3TestErrorAB; ab.Milliseconds = SV_Q3TestMilliseconds;
	ab.CvarGet = SV_Q3TestCvarGet; ab.CvarSet = SV_Q3TestCvarSet; ab.Argc = SV_Q3TestArgc; ab.Argv = SV_Q3TestArgv;
	ab.FOpenFileRead = SV_Q3TestFOpenFileRead; ab.Read = SV_Q3TestReadAB; ab.FOpenFileWrite = SV_Q3TestFOpenFileWrite;
	ab.Write = SV_Q3TestWriteAB; ab.FCloseFile = SV_Q3TestFCloseFileAB; ab.AddCommandText = SV_Q3TestAddCommandText;
	ab.DebugGraph = SV_Q3TestDebugGraph; ab.DropClient = SV_Q3TestDropClientAB;
	ab.SendServerCommand = SV_Q3TestSendServerCommandAB; ab.SetConfigstring = SV_Q3TestSetConfigstring;
	ab.GetConfigstring = SV_Q3TestGetConfigstringAB; ab.SetBrushModel = SV_Q3TestSetBrushModel;
	ab.Trace = SV_Q3TestTrace; ab.PointContents = SV_Q3TestPointContents; ab.InPVS = SV_Q3TestInPVS;
	ab.InPVSIgnorePortals = SV_Q3TestInPVSIgnorePortals; ab.AdjustAreaPortalState = SV_Q3TestAdjustAreaPortalState;
	ab.AreasConnected = SV_Q3TestAreasConnected; ab.LinkEntity = SV_Q3TestLinkEntity;
	ab.UnlinkEntity = SV_Q3TestUnlinkEntity; ab.ContactEntities = SV_Q3TestContactEntities;
	ab.SnapshotEntityCount = SV_Q3TestSnapshotEntityCount; ab.SnapshotEntityNumber = SV_Q3TestSnapshotEntityNumber;
	ab.BotAllocateClient = SV_Q3TestBotAllocateClient; ab.BotFreeClient = SV_Q3TestBotFreeClient;
	ab.BotSetUserinfo = SV_Q3TestBotSetUserinfo;

	fntech3_q3test_game_import_cd_head_t head{};
	head.Printf = SV_Q3TestPrintf; head.Error = SV_Q3TestErrorCD; head.Milliseconds = SV_Q3TestMilliseconds;
	head.CvarGet = SV_Q3TestCvarGet; head.CvarSet = SV_Q3TestCvarSet;
	head.CvarVariableIntegerValue = SV_Q3TestCvarVariableIntegerValue;
	head.CvarVariableStringBuffer = SV_Q3TestCvarVariableStringBuffer; head.Argc = SV_Q3TestArgc; head.Argv = SV_Q3TestArgv;
	head.FOpenFileByMode = SV_Q3TestFOpenFileByMode; head.Read = SV_Q3TestReadCD; head.Write = SV_Q3TestWriteCD;
	head.FCloseFile = SV_Q3TestFCloseFileCD; head.AddCommandText = SV_Q3TestAddCommandText;
	head.DropClient = SV_Q3TestDropClientCD; head.SendServerCommand = SV_Q3TestSendServerCommandCD;
	head.SetConfigstring = SV_Q3TestSetConfigstring; head.GetConfigstring = SV_Q3TestGetConfigstringCD;
	head.GetUserinfo = SV_Q3TestGetUserinfo; head.SetUserinfo = SV_Q3TestSetUserinfo;
	fntech3_q3test_game_import_cd_tail_t tail{};
	tail.SetBrushModel = SV_Q3TestSetBrushModel; tail.Trace = SV_Q3TestTrace; tail.PointContents = SV_Q3TestPointContents;
	tail.InPVS = SV_Q3TestInPVS; tail.InPVSIgnorePortals = SV_Q3TestInPVSIgnorePortals;
	tail.AdjustAreaPortalState = SV_Q3TestAdjustAreaPortalState; tail.AreasConnected = SV_Q3TestAreasConnected;
	tail.LinkEntity = SV_Q3TestLinkEntity; tail.UnlinkEntity = SV_Q3TestUnlinkEntity;
	tail.EntitiesInBox = SV_Q3TestEntitiesInBox; tail.EntityContact = SV_Q3TestEntityContact;
	q3test.importsC = {}; q3test.importsC.head = head; q3test.importsC.tail = tail;
	q3test.importsD = {}; q3test.importsD.head = head; q3test.importsD.GetServerinfo = SV_Q3TestGetServerinfo;
	q3test.importsD.tail = tail; q3test.importsD.BotAllocateClient = SV_Q3TestBotAllocateClient;
	q3test.importsD.BotFreeClient = SV_Q3TestBotFreeClient;
}

vm_t *SV_Q3TestLoadGame() {
#if !defined(_M_IX86) && !defined(__i386__)
	Com_Error( ERR_DROP, "Q3Test's game modules are Win32 x86 libraries; this build cannot load them" );
	return nullptr;
#else
	q3test.family = SV_Q3TestFamilyForRelease();
	if ( q3test.family == Q3TestFamily::None || !Com_Q3TestRelease()
		|| !fntech3::server::ServerGameDataView::SelectQ3TestProtocol( Com_Q3TestRelease()->protocol ) )
		Com_Error( ERR_DROP, "Q3Test has no pinned release" );
	void *symbol = nullptr;
	vm_t *vm = VM_CreateTable( VM_GAME, "GetGameAPI", SV_Q3TestGameEntry, &symbol );
	if ( !vm ) return nullptr;
	// GetGameAPI copies the table before returning, and may call an import.
	gvm = vm;
	SV_Q3TestFillImports();
	void *imports = q3test.family == Q3TestFamily::C ? static_cast<void *>( &q3test.importsC )
		: q3test.family == Q3TestFamily::D ? static_cast<void *>( &q3test.importsD ) : static_cast<void *>( &q3test.importsAB );
	auto getter = reinterpret_cast<fntech3_q3test_GetGameAPI_t>( symbol );
	void *exports = getter( imports );
	const int expected = q3test.family == Q3TestFamily::A || q3test.family == Q3TestFamily::B
		? FNTECH3_Q3TEST_GAME_API_AB : FNTECH3_Q3TEST_GAME_API_CD;
	const int stride = q3test.family == Q3TestFamily::A || q3test.family == Q3TestFamily::B
		? FNTECH3_Q3TEST_ENTITY_BYTES_AB : FNTECH3_Q3TEST_ENTITY_BYTES_CD;
	bool valid = exports && *static_cast<const int32_t *>( exports ) == expected;
	if ( valid ) {
		q3test.exports = exports;
		switch ( q3test.family ) {
		case Q3TestFamily::A: {
			const auto *e = SV_Q3TestA();
			valid = e->Init && e->Shutdown && e->SpawnEntities && e->ClientConnect && e->ClientBegin
				&& e->ClientUserinfoChanged && e->ClientDisconnect && e->ClientCommand && e->ClientThink
				&& e->ClientAfterCommands && e->BotBegin && e->BotThink && e->RunFrame && e->ConsoleCommand
				&& e->entitySize == stride;
			break;
		}
		case Q3TestFamily::B: {
			const auto *e = SV_Q3TestB();
			valid = e->Init && e->Shutdown && e->ClientConnect && e->ClientBegin && e->ClientUserinfoChanged
				&& e->ClientDisconnect && e->ClientCommand && e->ClientThink && e->ClientAfterCommands
				&& e->BotBegin && e->BotThink && e->RunFrame && e->ConsoleCommand && e->entitySize == stride;
			break;
		}
		default: {
			const auto *e = SV_Q3TestCD();
			valid = e->Init && e->Shutdown && e->ClientConnect && e->ClientBegin && e->ClientUserinfoChanged
				&& e->ClientDisconnect && e->ClientCommand && e->ClientThink && e->RunFrame && e->ConsoleCommand
				&& e->entitySize == stride;
			break;
		}
		}
	}
	if ( !valid ) {
		const int version = exports ? *static_cast<const int32_t *>( exports ) : 0;
		SV_Q3TestClearGame(); VM_Free( vm ); gvm = nullptr;
		Com_Error( ERR_DROP, "game is version %i, not %i", version, expected );
		return nullptr;
	}
	Com_Printf( "Q3Test %s game library: API %i, %i-byte entities\n", Com_Q3TestRelease()->version, expected, stride );
	return vm;
#endif
}

} // namespace
#endif
