/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_ALICE_GAME_HPP
#define FNTECH3_SERVER_ALICE_GAME_HPP

/* Alice's game module boundary, as far as it reaches.
 *
 * `base/fgamex86.dll` is a function table, not a vmMain: the engine calls
 * `GetGameAPI` with an import table and is handed an export table back. That
 * much is served here, through the same `VM_CreateTable` path Elite Force's
 * single player and the IHV build use. The published table is checked against
 * the shape frozen in code/compat/alice_module_abi.h rather than trusted.
 *
 * Every one of the 111 import slots is filled, because a null slot is the one
 * thing this boundary must never hand the module: calling one fails inside the
 * module with nothing to say which. Slots fnTech3 serves get an engine
 * service; the rest get a thunk that drops by name and slot number. That is
 * the whole diagnostic value of this file -- the module runs until it asks for
 * something that is not there, and then says what it asked for.
 *
 * Under cdecl the caller cleans the stack, so a thunk declaring no parameters
 * is safe to call with any number of arguments. That is what lets one thunk
 * shape stand in for all 111 slots whatever their measured arity.
 *
 * The argument counts the served slots use are measured from the module's own
 * call sites (code/compat/alice_game_import_calls.def) and the export arities
 * from the export bodies; see docs/fntech3/2026-09-22-alice-module-abi.md.
 * No slot is served on resemblance to a reference declaration alone. */

#include "../compat/alice_module_abi.h"
#include "../compat/alice_savegame.hpp"
#include "../compat/alice_state_adapter.hpp"
#include "../compat/fakk2_convert.hpp"
#include "../compat/ritual_layout.hpp"
#include "../compat/ritual_solid.hpp"
#include "server_alice_snapshot.hpp"
#include "../qcommon/alice_runtime.h"
#include "../qcommon/alice_tiki.h"

#include <cstdarg>
#include <cstddef>
#include <utility>

namespace {

using fntech3::alice::module::GameImportTable;
using fntech3::alice::module::ExportEvidence;
using fntech3::alice::module::SlotKind;
using fntech3::alice::module::kGameExports;
using fntech3::alice::module::kGameImports;

/* Heavy Metal: F.A.K.K.2's game module is the parent of Alice's, and this file
 * serves both. The table below is kept in Alice's slot order, the order both
 * titles' reference members share; F.A.K.K.2's module is handed that table
 * without the null Alice's engine leaves at slot 100 and without the five slots
 * Alice appended (code/compat/fakk2_module_abi.h). Every record offset and
 * configstring block the engine uses comes from the running title's layout
 * (code/compat/ritual_layout.hpp). */
const fntech3::ritual::ServerLayout *ritualLayout = &fntech3::ritual::AliceServerLayout();

const fntech3::ritual::ServerLayout &SV_RitualLayout() {
	return *ritualLayout;
}

/* "Alice's" or "F.A.K.K.2's", and the title alone, for the messages below. */
const char *SV_RitualTitle() {
	return ritualLayout->title;
}

const char *SV_RitualName() {
	return ritualLayout->profile == COM_GAME_FAKK2 ? "F.A.K.K.2" : "Alice";
}

bool SV_RitualIsFAKK2() {
	return ritualLayout->profile == COM_GAME_FAKK2;
}

GameImportTable aliceImport;
/* What F.A.K.K.2's module is handed: the same services, in its own 105 slots. */
std::intptr_t fakk2Import[FNTECH3_FAKK2_GAME_IMPORT_SLOTS];
const std::intptr_t *aliceGameExports;

/*
==================
SV_AliceUnservedImport

Every slot fnTech3 does not serve. Dropping by name is the point: the module
gets to run until it needs something real, and the log then names exactly what
it needed rather than faulting inside the module on a null.
==================
*/
template<int Slot>
intptr_t QDECL SV_AliceUnservedImport() {
	Com_Error( ERR_DROP,
		"%s game module called import %i (%s), which fnTech3 does not "
		"serve. The module ran to this point; this call is the next thing it "
		"needs. See docs/fntech3/COMPATIBILITY.md.", SV_RitualTitle(),
		Slot, kGameImports[Slot].name );
	return 0;
}

/* A copy of the unserved table, kept so that what was replaced can be counted
 * rather than remembered. Each slot's thunk is a distinct function, so a slot
 * differing from this is a slot something real was put into. */
GameImportTable aliceUnserved;

template<std::size_t... Slots>
void SV_AliceFillUnserved( std::index_sequence<Slots...> ) {
	( ( aliceUnserved.slot[Slots] = reinterpret_cast<std::intptr_t>(
		&SV_AliceUnservedImport<static_cast<int>( Slots )> ) ), ... );
	aliceImport = aliceUnserved;
}

int SV_AliceServedImports() {
	int served = 0;

	for ( int i = 0; i < FNTECH3_ALICE_GAME_IMPORT_SLOTS; i++ ) {
		served += aliceImport.slot[i] != aliceUnserved.slot[i];
	}
	return served;
}

/* ---- the slots fnTech3 serves ------------------------------------------- */

/* Slots 0, 1 and 2. All three read variadic at every call site that states its
 * own cleanup, which is what a format-string member has to look like. */
void QDECL SV_AlicePrintf( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_Printf( "%s", text );
}

void QDECL SV_AliceDPrintf( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_DPrintf( "%s", text );
}

/* Slot 106, one of Alice's appends: WidgetPrintf (0x004389e0), which formats
 * its text and hands it to the engine UI's widget of the given name -- the
 * subtitle widget, `aliceDialog`, is the one the module names -- and which a
 * dedicated server skips. */
void QDECL SV_AliceImport106( const char *widget, const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list argptr;

	if ( !widget || !fmt ) {
		return;
	}
	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_AliceWidgetPrint( widget, text );
}

/* Slot 3. Variadic, and its first argument is a severity this tree has not
 * measured the values of, so it is reported rather than interpreted. Every
 * level drops: there is no level at which continuing would be safe here. */
void QDECL SV_AliceError( int level, const char *fmt, ... ) {
	char text[MAX_STRING_CHARS];
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_Error( ERR_DROP, "%s game error (level %i): %s", SV_RitualName(), level, text );
}

/* Slot 4, measured at no arguments across twelve call sites. */
int QDECL SV_AliceMilliseconds() {
	return Sys_Milliseconds();
}

/* Slots 5 and 6. Paired: what one hands out the other has to take back, so
 * both go through the same allocator. */
void *QDECL SV_AliceMalloc( int size ) {
	if ( size < 0 ) {
		Com_Error( ERR_DROP, "%s game module asked for %i bytes", SV_RitualTitle(), size );
		return nullptr;
	}
	return Z_TagMalloc( size ? size : 1, TAG_GENERAL );
}

void QDECL SV_AliceFree( void *block ) {
	if ( block ) {
		Z_Free( block );
	}
}

/* Slots 9, 10 and 11: the console command the module is answering. */
int QDECL SV_AliceArgc() {
	return Cmd_Argc();
}

const char *QDECL SV_AliceArgv( int arg ) {
	return Cmd_Argv( arg );
}

const char *QDECL SV_AliceArgs() {
	/* fnTech3 spells this one as "everything from argument 1", which is what
	 * a bare args accessor returns. */
	return Cmd_ArgsFrom( 1 );
}

/* Slot 7, measured at three arguments across 131 of its 132 call sites, and
 * the one import that returns a record rather than a value. What the module
 * does with the pointer names the record's layout, and every displacement it
 * dereferences has the same meaning in fnTech3's own `cvar_t`:
 *
 *     +0x00  compared and moved            name
 *     +0x04  read 25 times                 string
 *     +0x08  read                          resetString
 *     +0x1c  read, five of them with `fld` value, a float at 28
 *     +0x20  read as an integer            integer, at 32
 *
 * The `fld` at +0x1c is what fixes the float, and nothing the module reads
 * lies beyond +0x20, so fnTech3's record -- which carries further fields of
 * its own past that point -- can be handed over whole. This is not an argument
 * from resemblance to Quake III: the offsets were measured in Alice's module
 * first and found to agree afterwards.
 *
 * The flag bits are passed through unexamined. The module never reads the
 * flags field back, so nothing here depends on them matching, but whether
 * Alice's bit values are Quake III's is not established. */
cvar_t *QDECL SV_AliceCvar( const char *name, const char *value, int flags ) {
	if ( !name || !value ) {
		Com_Error( ERR_DROP, "%s game module asked for a cvar with no name", SV_RitualTitle() );
		return nullptr;
	}
	return Cvar_Get( name, value, flags );
}

/* Slot 8, measured at two arguments across eighty-five call sites. */
void QDECL SV_AliceCvarSet( const char *name, const char *value ) {
	if ( name && value ) {
		Cvar_Set( name, value );
	}
}

/* Slot 12, measured at one argument across 33 of its 35 call sites. One
 * argument means it registers a name and no handler, so the module expects the
 * engine to route the command back through an export rather than to a function
 * pointer it supplied -- which is what its `ConsoleCommand` export, measured at
 * no arguments, is for: it reads the command through argc and argv.
 *
 * fnTech3's own command table wants a handler, so every Alice command gets the
 * same one, and that handler asks the module.
 *
 * These registrations are deliberately not undone on shutdown. Some of the
 * names the module asks for are already fnTech3's own -- `say` and `script`
 * are both refused as already defined -- and there is no way from here to tell
 * a registration that succeeded from one that did not, so removing them all
 * would take the engine's own commands away with them. Leaving them is safe
 * because the handler lives in the engine rather than in the module and does
 * nothing once the export table is gone, which is what shutdown clears. */
void SV_AliceConsoleCommandHandler();

/* Whether a command of this name exists, asked through the one query the
 * command table offers: its completion walk. */
const char *aliceCommandProbe;
bool aliceCommandFound;

void SV_AliceCommandSeen( const char *name ) {
	if ( name && aliceCommandProbe && !Q_stricmp( name, aliceCommandProbe ) ) {
		aliceCommandFound = true;
	}
}

bool SV_AliceCommandExists( const char *name ) {
	aliceCommandProbe = name;
	aliceCommandFound = false;
	Cmd_CommandCompletion( SV_AliceCommandSeen );
	aliceCommandProbe = nullptr;
	return aliceCommandFound;
}

/* Retail's slot 12 registers the name with no handler, and its
 * Cmd_AddCommand (0x00438370) reports a duplicate only when it is given one,
 * so a name that is already a command -- the engine's own `say`, or one the
 * module registered when an earlier level loaded it -- is skipped without a
 * word. This engine's table would print "already defined" for each, once per
 * level, so the name is looked up first. */
void QDECL SV_AliceAddCommand( const char *name ) {
	if ( name && name[0] && !SV_AliceCommandExists( name ) ) {
		Cmd_AddCommand( name, SV_AliceConsoleCommandHandler );
	}
}

/* Slots 13, 14 and 15. The read takes three arguments where fnTech3's takes
 * two; the third is not established, and is ignored rather than guessed at. */
int QDECL SV_AliceReadFile( const char *path, void **buffer, int unestablished ) {
	const int length = FS_ReadFile( path, buffer );

	Com_DPrintf( "%s ReadFile: \"%s\" third=%i -> %i bytes at %p\n", SV_RitualName(),
		path ? path : "(null)", unestablished, length,
		buffer ? *buffer : nullptr );
	return length;
}

void QDECL SV_AliceFreeFile( void *buffer ) {
	if ( buffer ) {
		FS_FreeFile( buffer );
	}
}

/* Slots 16 to 25, the rest of the file family, whose argument counts the call
 * sites settle: one to open, three to write, one to close, one to tell, three
 * to seek, one to flush. The handles are the engine's own, passed through as
 * the integers both sides treat them as.
 *
 * The seek origins are passed through untranslated because they already
 * agree: retail's FS_Seek (0x0043c680) maps 0, 1 and 2 to the C library's
 * SEEK_CUR, SEEK_END and SEEK_SET, which is the order of this engine's own
 * FS_SEEK_CUR, FS_SEEK_END and FS_SEEK_SET (q_shared.h). The module's archive
 * writer seeks with 2, back to where its count goes (fgamex86 0x10035ff0). */
fileHandle_t QDECL SV_AliceFOpenFileWrite( const char *path ) {
	return path ? FS_FOpenFileWrite( path ) : 0;
}

fileHandle_t QDECL SV_AliceFOpenFileAppend( const char *path ) {
	return path ? FS_FOpenFileAppend( path ) : 0;
}

int QDECL SV_AliceFSWrite( const void *buffer, int length, fileHandle_t file ) {
	if ( !buffer || length <= 0 || !file ) {
		return 0;
	}
	return FS_Write( buffer, length, file );
}

int QDECL SV_AliceFSRead( void *buffer, int length, fileHandle_t file ) {
	if ( !buffer || length <= 0 || !file ) {
		return 0;
	}
	return FS_Read( buffer, length, file );
}

void QDECL SV_AliceFCloseFile( fileHandle_t file ) {
	if ( file ) {
		FS_FCloseFile( file );
	}
}

int QDECL SV_AliceFTell( fileHandle_t file ) {
	return file ? FS_FTell( file ) : 0;
}

int QDECL SV_AliceFSeek( fileHandle_t file, int offset, int origin ) {
	if ( !file ) {
		return -1;
	}
	return FS_Seek( file, offset, static_cast<fsOrigin_t>( origin ) );
}

void QDECL SV_AliceFSFlush( fileHandle_t file ) {
	if ( file ) {
		FS_Flush( file );
	}
}

void QDECL SV_AliceWriteFile( const char *path, const void *buffer, int size ) {
	if ( path && buffer && size >= 0 ) {
		FS_WriteFile( path, buffer, size );
	}
}

/* The archive name every savegame file goes by, retail's name builder
 * (0x0043a350): "save/<config>_<name>.<ext>", <config> being the `config`
 * cvar up to its first dot. See code/compat/alice_savegame.hpp for what is
 * refused. */
bool SV_AliceArchivePath( const char *name, const char *extension,
	char ( &path )[fntech3::alice::savegame::PathBytes] ) {
	return fntech3::alice::savegame::ArchiveFileName( Cvar_VariableString( "config" ),
		name, extension, path );
}

/* Slot 25, that same builder, written into the table at 0x00454511. Like
 * retail's it answers from one static buffer. The module's only caller is its
 * play log (fgamex86 0x10117001), which opens what it is given; a name the
 * rules refuse answers an empty path, which opens nothing. */
const char *QDECL SV_AliceGetArchiveFileName( const char *name, const char *extension ) {
	static char path[fntech3::alice::savegame::PathBytes];

	if ( !SV_AliceArchivePath( name, extension, path ) ) {
		Com_DPrintf( "%s game module asked for archive \"%s\" \"%s\", which is refused\n", SV_RitualTitle(),
			name ? name : "(null)", extension ? extension : "(null)" );
		path[0] = '\0';
	}
	return path;
}

/* Slot 99, the engine's CalcCRC (0x0043a4c0): CRC-16 over a buffer. The
 * module calls it when it loads a script it has not cached (fgamex86
 * 0x100b201a) and, reading a level archive, compares the result with the one
 * the save recorded (0x100b30aa). It keeps the low sixteen bits, and so does
 * this answer. */
int QDECL SV_AliceCalcCRC( const byte *data, int length ) {
	// Silent, as retail's is.
	return fntech3::alice::savegame::CalcCRC( data, length );
}

/* Slot 107, one of Alice's own additions and the first thing its game module
 * asks for that the reference table has no member for. In the retail engine
 * this slot holds a function whose entire body is `mov eax, 0xc300a0; ret`:
 * it hands back the address of a fixed 60-byte block. The size is not a
 * guess -- the routine that builds the import table opens by clearing exactly
 * fifteen dwords there, immediately before it fills the table:
 *
 *     sub  esp, 0x1bc        ; 444 bytes, the 111-slot table
 *     mov  ecx, 0xf          ; 15 dwords
 *     mov  edi, 0xc300a0
 *     rep  stosd
 *
 * so a zeroed block of that size is exactly what the retail module is handed
 * at this point in startup. The client game and the renderer are handed the
 * same block (0x0042c490 fills all three), so it lives in the engine's shared
 * Alice state, where the camera the client game writes reaches the game and
 * the HUD state the game writes reaches the client. */
void *QDECL SV_AliceSharedBlock() {
	return Alice_SharedBlock();
}

/* Slots 101 and 102 are the two slots in this table that are not functions at
 * all, and the module proves it: it writes straight through them.
 *
 *     mov eax, [0x101e8ea4]     ; its copy of slot 101
 *     mov ecx, [0x101e40d4]     ; its own debug line array
 *     mov [eax], ecx            ; store the array through the slot
 *     mov edx, [0x101e8ea8]     ; its copy of slot 102
 *     mov [edx], 0              ; and zero the count through that one
 *
 * So each slot holds the address of a variable the engine owns and the module
 * assigns to. Filling them with a function thunk like every other slot has the
 * module write into code, which is an access violation at the first store --
 * and which is exactly how these two were confirmed to be the data pair the
 * engine's own table build says they are. What the module stores here is its
 * own debug line array and its length, which the client game's R_DebugLine
 * appends to and the renderer draws; both live in the engine's shared Alice
 * state (Alice_DebugLineArray, Alice_DebugLineCount). */

/* Slot 103, the call whose body identified it: it reads exactly five stack
 * arguments and stores them into five consecutive engine globals. That is the
 * module publishing where its entity array lives and how big a record is, so
 * the engine can walk entities it did not allocate.
 *
 * fnTech3 records them and does not yet use them: no Alice entity is marshalled
 * anywhere. Recording is still the right thing to do rather than ignoring the
 * call, because the record size it reports is checkable -- it has to be the
 * 732 bytes the module publishes in export slot 22, and a module that says two
 * different things about its own entity is not one to keep running. */
struct AliceGameData {
	void *entities;
	int entityCount;
	int entityBytes;
	void *clients;
	int clientBytes;
} aliceGameData;

/*
==================
SV_AliceBindEntities

Give the server's shared entity view the arrays the module just published.

The count the module reports is not the array's bound -- it is what is in use
at the moment of the call, and the module goes on to spawn a level's worth
past it -- so the arena bound comes from the capacity the module publishes in
export slot 24 and the count is checked against it.

Re-binding invalidates every outstanding entity reference, so this only binds
when something has actually moved.
==================
*/
void SV_AliceBindEntities() {
	if ( !aliceGameExports || !aliceGameData.entities
		|| aliceGameData.entityBytes <= 0 ) {
		return;
	}
	const std::intptr_t capacity =
		aliceGameExports[FNTECH3_ALICE_EXPORT_max_entities];
	if ( capacity <= 0 || capacity > MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "%s game module published a %i-entity arena, "
			"which is outside the %i this engine carries", SV_RitualTitle(),
			(int)capacity, MAX_GENTITIES );
		return;
	}
	/* The module keeps its live count in export slot 23 and updates it as it
	 * spawns, where LocateGameData's argument is only what was in use when
	 * that call happened. Prefer the slot, and fall back to the argument when
	 * the slot is not yet sane. */
	std::intptr_t live = aliceGameExports[FNTECH3_ALICE_EXPORT_num_entities];
	if ( live <= 0 || live > capacity ) {
		live = aliceGameData.entityCount;
	}
	const int count = live > 0 && live <= capacity
		? (int)live : (int)capacity;
	const int clients = sv.maxclients > 0
		? sv.maxclients : FNTECH3_ALICE_MAX_CLIENTS;

	if ( serverGameData.IsBound() && sv.gentities == aliceGameData.entities
		&& sv.gentitySize == aliceGameData.entityBytes
		&& serverGameData.EntityCount() == count
		&& sv.gameClients == aliceGameData.clients
		&& serverGameData.ClientCount() == clients ) {
		return;
	}

	fntech3::server::NativeGameDataBinding binding;
	binding.entityBase = aliceGameData.entities;
	binding.entityCount = count;
	binding.entityCapacity = (std::int32_t)capacity;
	binding.entityStride = (std::size_t)aliceGameData.entityBytes;
	binding.entitySpanBytes = binding.entityStride * (std::size_t)capacity;
	binding.clientBase = aliceGameData.clients;
	binding.clientCount = clients;
	binding.clientStride = (std::size_t)aliceGameData.clientBytes;
	binding.clientSpanBytes = binding.clientStride * (std::size_t)clients;

	const fntech3::server::GameDataBindError error =
		serverGameData.BindNative( SV_RitualLayout().profile, binding );
	if ( error != fntech3::server::GameDataBindError::None ) {
		Com_Error( ERR_DROP, "%s game data cannot be bound: %s", SV_RitualTitle(),
			fntech3::server::GameDataBindErrorName( error ) );
		return;
	}

	const bool moved = sv.gentities != aliceGameData.entities
		|| sv.gameClients != aliceGameData.clients;

	sv.gentities = aliceGameData.entities;
	sv.gentitySize = aliceGameData.entityBytes;
	sv.num_entities = count;
	sv.gameClients = aliceGameData.clients;
	sv.gameClientSize = aliceGameData.clientBytes;

	/* Only when the arrays themselves move. The module publishes again on
	 * every spawn, so reporting the count here would bury a level's startup
	 * under several hundred lines that all say the same thing. */
	if ( moved ) {
		Com_DPrintf( "%s bound its entity arena at %p: %i slots of %i bytes, "
			"and %i clients of %i bytes at %p\n", SV_RitualName(), aliceGameData.entities,
			(int)capacity, aliceGameData.entityBytes, clients,
			aliceGameData.clientBytes, aliceGameData.clients );
	}
}

void QDECL SV_AliceLocateGameData( void *entities, int entityCount,
	int entityBytes, void *clients, int clientBytes ) {
	if ( entityBytes != SV_RitualLayout().gentitySize ) {
		Com_Error( ERR_DROP, "%s game module located its entities at %i "
			"bytes each, but publishes a %i-byte entity record", SV_RitualTitle(),
			entityBytes, SV_RitualLayout().gentitySize );
		return;
	}
	if ( clientBytes != SV_RitualLayout().gclientSize ) {
		Com_Error( ERR_DROP, "%s game module located its clients at %i "
			"bytes each, where every retail map reports %i", SV_RitualTitle(),
			clientBytes, SV_RitualLayout().gclientSize );
		return;
	}
	/* The module calls this again whenever its entity count grows; only a
	 * moved arena is worth a line. */
	const bool moved = entities != aliceGameData.entities || clients != aliceGameData.clients;
	aliceGameData.entities = entities;
	aliceGameData.entityCount = entityCount;
	aliceGameData.entityBytes = entityBytes;
	aliceGameData.clients = clients;
	aliceGameData.clientBytes = clientBytes;
	if ( moved ) {
		Com_DPrintf( "%s located game data: %i entities of %i bytes at %p, "
			"clients of %i bytes at %p\n", SV_RitualName(), entityCount, entityBytes, entities,
			clientBytes, clients );
	}
	SV_AliceBindEntities();
}

/* Slots 29 and 30, at two and one argument. Alice's retail accessors bound
 * the index by 0x530 (set 0x00454a5b, get 0x00454b09); F.A.K.K.2 1.02's by
 * 0x4b0 (set 0x004458cb, get 0x00445979). The shared engine's larger storage
 * is not a module contract: use the selected title's limit for both calls. */
void QDECL SV_AliceSetConfigstring( int index, const char *value ) {
	const int limit = SV_RitualLayout().maxConfigstrings;
	if ( index < 0 || index >= limit ) {
		Com_Error( ERR_DROP, "%s game module set configstring %i, outside "
			"the %i this title carries", SV_RitualTitle(), index, limit );
		return;
	}
	SV_SetConfigstring( index, value ? value : "" );
}

const char *QDECL SV_AliceGetConfigstring( int index ) {
	const int limit = SV_RitualLayout().maxConfigstrings;
	if ( index < 0 || index >= limit ) {
		Com_Error( ERR_DROP, "%s game module read configstring %i, outside "
			"the %i this title carries", SV_RitualTitle(), index, limit );
		return "";
	}
	/* The module takes a pointer rather than filling a buffer, so it gets the
	 * engine's own string. Nothing in the module writes through it: every one
	 * of its call sites reads. */
	return sv.configstrings[index] ? sv.configstrings[index] : "";
}

/* Slots 104 and 105. Both bodies store one value in the server state, which
 * every spawn clears: SetFarPlane (0x004537d0) stores zero or the distance
 * plus 32, squared, and SetSkyPortal (0x00453800) its argument. The snapshot
 * walk culls by the one and opens the sky portal's view on the other
 * (server_alice_snapshot.hpp). */
void QDECL SV_AliceSetFarPlane( int farPlane ) {
	sv.aliceFarPlane = fntech3::alice::snapshot::SquaredFarPlane( farPlane );
}

void QDECL SV_AliceSetSkyPortal( int enabled ) {
	sv.aliceSkyPortal = enabled ? qtrue : qfalse;
}

/* Slots 45, 46 and 47, the index family. Each is a two-instruction wrapper in
 * the retail engine around one shared routine, and each passes a different
 * configstring base and length, which is what tells them apart:
 *
 *     slot 47  base 0x220, 0x100 entries   the largest block
 *     slot 45  base 0x320, 0x40 entries
 *     slot 46  base 0x400, 0x40 entries
 *
 * The shared routine reports `SV_FindIndex: overflow` when a block fills, so
 * it is a find-or-add over a fixed configstring range. That is what this does:
 * an empty slot is free, a matching one is a hit, and running out is an error
 * rather than a silent zero, because index zero means "none" to the module. */
int SV_AliceFindIndex( const char *name, int base, int count, const char *what ) {
	if ( !name || !name[0] ) {
		return 0;
	}
	for ( int i = 1; i < count; i++ ) {
		const char *have = sv.configstrings[base + i];

		if ( !have || !have[0] ) {
			SV_SetConfigstring( base + i, name );
			return i;
		}
		if ( !strcmp( have, name ) ) {
			return i;
		}
	}
	Com_Error( ERR_DROP, "%s game module ran out of %s slots: %i is all "
		"this block holds", SV_RitualTitle(), what, count );
	return 0;
}

/* The four blocks, in the reference's own order of the four calls: images at
 * 0x320, items at 0x400, sounds at 0x220 and models at 0x20. An earlier reading
 * of this table named these one slot early; the block each body indexes is
 * what settles which is which. */
int QDECL SV_AliceImageIndex( const char *name ) {
	return SV_AliceFindIndex( name, SV_RitualLayout().csImages, SV_RitualLayout().maxImages, "image" );
}

int QDECL SV_AliceItemIndex( const char *name ) {
	return SV_AliceFindIndex( name, SV_RitualLayout().csItems, SV_RitualLayout().maxItems, "item" );
}

int QDECL SV_AliceSoundIndex( const char *name ) {
	return SV_AliceFindIndex( name, SV_RitualLayout().csSounds, SV_RitualLayout().maxSounds, "sound" );
}

/* The model definition each model index holds, as its handle plus one so that
 * zero means none -- retail's own table at 0x006a0070 holds it that way. The
 * array holds Alice's 512; F.A.K.K.2's block is 384, and its layout bounds it. */
enum { FNTECH3_ALICE_MAX_MODELS = 0x200 };
static_assert( FNTECH3_FAKK2_MAX_MODELS <= FNTECH3_ALICE_MAX_MODELS,
	"one table serves both titles' model blocks" );
int aliceModelTiki[FNTECH3_ALICE_MAX_MODELS];

/* Slot 48. Retail's body at 0x00454110 finds or adds the name in the model
 * block, and when the name ends in `tik` and the index holds no definition yet
 * it loads one and records it. */
int QDECL SV_AliceModelIndex( const char *name ) {
	const int index = SV_AliceFindIndex( name, SV_RitualLayout().csModels,
		SV_RitualLayout().maxModels, "model" );
	if ( index <= 0 || !name ) {
		return index;
	}
	const std::size_t length = strlen( name );
	if ( length >= 3 && !Q_stricmp( name + length - 3, "tik" ) && !aliceModelTiki[index] ) {
		aliceModelTiki[index] = AliceTiki_Register( name ) + 1;
	}
	return index;
}

/* Retail's SV_ClearWorld (alice.exe 0x00457960) ends by naming every inline
 * model but the world's -- `*1` up to the last (0x004579a9..0x004579d2) --
 * through the model block's find-or-add (0x00454c00), in order. Each brush
 * model's index is then its own number, and the module's model definitions
 * are indexed after them. Without it a definition takes the index of a brush
 * model: the module finds a model definition behind a brush entity's index
 * (IsModel) and runs that definition's init commands on it -- F.A.K.K.2's
 * fakkhouse lost its sky origin to fx_plantseeds.tik's that way. A level
 * whose configstrings a load kept already holds the names. */
void SV_AliceNameInlineModels() {
	const int count = CM_NumInlineModels();
	for ( int i = 1; i < count; i++ ) {
		SV_AliceFindIndex( va( "*%i", i ), SV_RitualLayout().csModels, SV_RitualLayout().maxModels, "model" );
	}
}

/* Set when a load kept the running level's configstrings (server_alice_save.hpp,
 * SV_AliceRestoreConfigstrings). Retail's spawn skips its model table's clear
 * (0x004551de) on the same path that skips the configstrings', so the table
 * still holds a definition for every kept model; this engine loads the game
 * module afresh, which starts the table empty, and registers each kept `tik`
 * name again as the module's table is filled. Retail's own registration
 * (0x00454153..0x00454176) leaves an entry empty when the definition fails to
 * load, and so does this. */
bool aliceModelTableKept;

void SV_AliceRegisterKeptModels() {
	if ( !aliceModelTableKept ) {
		return;
	}
	aliceModelTableKept = false;
	for ( int index = 1; index < SV_RitualLayout().maxModels; ++index ) {
		const char *name = sv.configstrings[SV_RitualLayout().csModels + index];
		const std::size_t length = name ? strlen( name ) : 0;
		if ( length >= 3 && !Q_stricmp( name + length - 3, "tik" ) ) {
			aliceModelTiki[index] = AliceTiki_Register( name ) + 1;
		}
	}
}

/* A model index's definition, or -1. Retail's routine at 0x00453870 answers
 * -1 for an index that never had one and loads the definition again when the
 * index's configstring has changed name since (0x0045c960). */
int SV_AliceTikiForIndex( int index ) {
	if ( index <= 0 || index >= SV_RitualLayout().maxModels || !aliceModelTiki[index] ) {
		return -1;
	}
	const int handle = aliceModelTiki[index] - 1;
	const char *name = sv.configstrings[SV_RitualLayout().csModels + index];
	const char *loaded = AliceTiki_Get( handle ) ? AliceTiki_Get( handle )->name : nullptr;
	if ( name && loaded && Q_stricmp( name, loaded ) ) {
		aliceModelTiki[index] = AliceTiki_Register( name ) + 1;
		return aliceModelTiki[index] - 1;
	}
	return handle;
}

/* Slot 35, at two arguments. The point arrives as a pointer rather than by
 * value, and the second argument is the entity number to ignore, which is what
 * this engine's own call takes too. The contents bits are passed straight back
 * up: the module compares them against 0x100000 and 0x4000000, and whether
 * those mean what Quake III's bits of the same value mean is not established,
 * so nothing here translates them. */
int QDECL SV_AlicePointContents( const float *point, int passEntityNum ) {
	if ( !point ) {
		return 0;
	}
	return SV_PointContents( point, passEntityNum );
}

/* Slots 89 to 92, the engine's global alias list, the one retail names
 * `Global` in its own code. The client game's slots 82 to 85 are these same
 * four engine functions, so both modules share one list. */
qboolean QDECL SV_AliceGlobalAliasAdd( const char *name, const char *value, const char *parameters ) {
	return AliceAlias_Add( -1, name, value, parameters );
}

const char *QDECL SV_AliceGlobalAliasFindRandom( const char *name ) {
	return AliceAlias_FindRandom( -1, name );
}

void QDECL SV_AliceGlobalAliasDump() {
	AliceAlias_Dump( -1 );
}

void QDECL SV_AliceGlobalAliasClear() {
	AliceAlias_Clear( -1 );
}

/* Slots 51 to 88, the model queries, each taking a model index that retail's
 * `PF_` wrappers resolve to a definition before calling the engine's TIKI
 * routine -- the same routines the client game reaches directly, so both
 * modules read one cache. See code/qcommon/alice_tiki.cpp. */
/* Slot 50, the game directory: retail returns its own directory buffer, the
 * one its client-game loader also reads. */
const char *QDECL SV_AliceGameDir() {
	return FS_GetCurrentGameDir();
}

int QDECL SV_AliceIsModel( int index ) {
	return SV_AliceTikiForIndex( index ) >= 0;
}

/* Retail's NumSkins wrapper answers zero whatever it is asked. */
int QDECL SV_AliceNumSkins( int index ) {
	(void)index;
	return 0;
}

/* Every wrapper from here to slot 88 resolves its index first and, for an
 * index with no definition -- index zero, or one never registered in this
 * level -- answers its own default without reaching the TIKI routine, whose
 * handle check would report it: each of retail's `PF_` wrappers
 * (0x00453960..0x004540f0) branches past its call on `jl`. The defaults are
 * each wrapper's own: zero or null, -1 from the three name lookups, a cleared
 * vector from the two animation deltas (0x00453ba7, 0x00453be7), and nothing
 * at all from the bounds, the frame delta and the alias updates. */
template<typename Result, typename Call>
Result SV_AliceTikiCall( int index, Result fallback, Call call ) {
	const int tiki = SV_AliceTikiForIndex( index );
	return tiki < 0 ? fallback : call( tiki );
}

template<typename Call>
void SV_AliceTikiDo( int index, Call call ) {
	const int tiki = SV_AliceTikiForIndex( index );
	if ( tiki >= 0 ) {
		call( tiki );
	}
}

int QDECL SV_AliceNumAnims( int index ) {
	return SV_AliceTikiCall( index, 0, []( int tiki ) { return AliceTiki_NumAnims( tiki ); } );
}

int QDECL SV_AliceNumSurfaces( int index ) {
	return SV_AliceTikiCall( index, 0, []( int tiki ) { return AliceTiki_NumSurfaces( tiki ); } );
}

int QDECL SV_AliceNumTags( int index ) {
	return SV_AliceTikiCall( index, 0, []( int tiki ) { return AliceTiki_NumTags( tiki ); } );
}

qboolean QDECL SV_AliceInitCommands( int index, fntech3_alice_tiki_cmd_t *commands ) {
	return SV_AliceTikiCall( index, qfalse, [&]( int tiki ) {
		return AliceTiki_InitCommands( tiki, qfalse, commands ); } );
}

void QDECL SV_AliceCalculateBounds( int index, float scale, float *mins, float *maxs ) {
	if ( mins && maxs ) {
		SV_AliceTikiDo( index, [&]( int tiki ) { AliceTiki_CalculateBounds( tiki, scale, mins, maxs ); } );
	}
}

const char *QDECL SV_AliceAnimNameForNum( int index, int anim ) {
	return SV_AliceTikiCall<const char *>( index, nullptr, [&]( int tiki ) {
		return AliceTiki_AnimName( tiki, anim ); } );
}

int QDECL SV_AliceAnimNumForName( int index, const char *name ) {
	return SV_AliceTikiCall( index, -1, [&]( int tiki ) { return AliceTiki_AnimNumForName( tiki, name ); } );
}

int QDECL SV_AliceAnimRandom( int index, const char *name ) {
	return SV_AliceTikiCall( index, -1, [&]( int tiki ) { return AliceTiki_AnimRandom( tiki, name ); } );
}

int QDECL SV_AliceAnimNumFrames( int index, int anim ) {
	return SV_AliceTikiCall( index, 0, [&]( int tiki ) { return AliceTiki_AnimNumFrames( tiki, anim ); } );
}

float QDECL SV_AliceAnimTime( int index, int anim ) {
	return SV_AliceTikiCall( index, 0.0f, [&]( int tiki ) { return AliceTiki_AnimTime( tiki, anim ); } );
}

void QDECL SV_AliceAnimDelta( int index, int anim, float *delta ) {
	if ( !delta ) {
		return;
	}
	VectorClear( delta );
	SV_AliceTikiDo( index, [&]( int tiki ) { AliceTiki_AnimDelta( tiki, anim, delta ); } );
}

void QDECL SV_AliceAnimAbsoluteDelta( int index, int anim, float *delta ) {
	if ( !delta ) {
		return;
	}
	VectorClear( delta );
	SV_AliceTikiDo( index, [&]( int tiki ) { AliceTiki_AnimAbsoluteDelta( tiki, anim, delta ); } );
}

int QDECL SV_AliceAnimFlags( int index, int anim ) {
	return SV_AliceTikiCall( index, 0, [&]( int tiki ) { return AliceTiki_AnimFlags( tiki, anim ); } );
}

qboolean QDECL SV_AliceAnimHasCommands( int index, int anim ) {
	return SV_AliceTikiCall( index, qfalse, [&]( int tiki ) {
		return AliceTiki_AnimHasCommands( tiki, anim, qfalse ); } );
}

qboolean QDECL SV_AliceFrameCommands( int index, int anim, int frame, fntech3_alice_tiki_cmd_t *commands ) {
	return SV_AliceTikiCall( index, qfalse, [&]( int tiki ) {
		return AliceTiki_FrameCommands( tiki, anim, frame, qfalse, commands ); } );
}

void QDECL SV_AliceFrameDelta( int index, int anim, int frame, float *delta ) {
	if ( delta ) {
		SV_AliceTikiDo( index, [&]( int tiki ) { AliceTiki_FrameDelta( tiki, anim, frame, delta ); } );
	}
}

float QDECL SV_AliceFrameTime( int index, int anim, int frame ) {
	return SV_AliceTikiCall( index, 0.0f, [&]( int tiki ) { return AliceTiki_FrameTime( tiki, anim, frame ); } );
}

void QDECL SV_AliceFrameBounds( int index, int anim, int frame, float scale, float *mins, float *maxs ) {
	if ( mins && maxs ) {
		SV_AliceTikiDo( index, [&]( int tiki ) {
			AliceTiki_FrameBounds( tiki, anim, frame, scale, mins, maxs ); } );
	}
}

int QDECL SV_AliceSurfaceNameToNum( int index, const char *name ) {
	return SV_AliceTikiCall( index, -1, [&]( int tiki ) { return AliceTiki_SurfaceNameToNum( tiki, name ); } );
}

const char *QDECL SV_AliceSurfaceNumToName( int index, int surface ) {
	return SV_AliceTikiCall<const char *>( index, nullptr, [&]( int tiki ) {
		return AliceTiki_SurfaceNumToName( tiki, surface ); } );
}

int QDECL SV_AliceSurfaceFlags( int index, int surface ) {
	return SV_AliceTikiCall( index, 0, [&]( int tiki ) { return AliceTiki_SurfaceFlags( tiki, surface ); } );
}

int QDECL SV_AliceSurfaceNumSkins( int index, int surface ) {
	return SV_AliceTikiCall( index, 0, [&]( int tiki ) { return AliceTiki_SurfaceNumSkins( tiki, surface ); } );
}

int QDECL SV_AliceTagNumForName( int index, const char *name ) {
	return SV_AliceTikiCall( index, -1, [&]( int tiki ) { return AliceTiki_TagNumForName( tiki, name ); } );
}

const char *QDECL SV_AliceTagNameForNum( int index, int tag ) {
	return SV_AliceTikiCall<const char *>( index, nullptr, [&]( int tiki ) {
		return AliceTiki_TagNameForNum( tiki, tag ); } );
}

/* Slots 78 and 79 return a 48-byte orientation by value: the caller passes
 * where it goes as a hidden first argument and copies twelve dwords out of the
 * pointer it gets back, so the answer is written there and that pointer is
 * returned. Retail's wrappers at 0x00453e20 and 0x00453ec0 hand the engine's
 * routine the definition in place of the index, then the module's six and
 * fifteen further arguments -- one pointer to the entity's 36-byte bone
 * controllers where the reference declared two arrays. Without a definition
 * retail reports "not a tiki model" as a developer print and answers the
 * origin with the axis AnglesToAxis makes of a zero vector -- the identity
 * (0x00453e72..0x00453e93, 0x00453f47..0x00453f68). */
struct AliceOrientation {
	float origin[3];
	float axis[3][3];
};

AliceOrientation *SV_AliceNoTikiOrientation( AliceOrientation *out, const char *wrapper ) {
	Com_DPrintf( "%s: not a tiki model\n", wrapper );
	VectorClear( out->origin );
	AnglesToAxis( vec3_origin, out->axis );
	return out;
}

AliceOrientation *QDECL SV_AliceTagOrientation( AliceOrientation *out, int index, int anim,
	int frame, int tag, float scale, const fntech3_alice_bonectrl_t *controllers ) {
	const int tiki = SV_AliceTikiForIndex( index );
	if ( tiki < 0 ) {
		return SV_AliceNoTikiOrientation( out, "PF_Tag_Orientation" );
	}
	AliceTiki_TagOrientation( tiki, anim, frame, tag, scale, controllers, out->origin, out->axis );
	return out;
}

AliceOrientation *QDECL SV_AliceTagOrientationEx( AliceOrientation *out, int index, int anim,
	int frame, int tag, float scale, const fntech3_alice_bonectrl_t *controllers,
	int crossblendAnim, int crossblendFrame, float crossblendLerp, int useLegs, int useTorso,
	int torsoAnim, int torsoFrame, int torsoCrossblendAnim, int torsoCrossblendFrame,
	float torsoCrossblendLerp ) {
	const int tiki = SV_AliceTikiForIndex( index );
	if ( tiki < 0 ) {
		return SV_AliceNoTikiOrientation( out, "PF_Tag_OrientationEx" );
	}
	AliceTiki_TagOrientationEx( tiki, anim, frame, tag, scale, controllers,
		crossblendAnim, crossblendFrame, crossblendLerp, useLegs ? qtrue : qfalse,
		useTorso ? qtrue : qfalse, torsoAnim, torsoFrame, torsoCrossblendAnim,
		torsoCrossblendFrame, torsoCrossblendLerp, out->origin, out->axis );
	return out;
}

/* F.A.K.K.2's slots 78 and 79: the same queries, with the entity's bone tags
 * and bone quaternions handed over as the two arrays its entity state keeps
 * them in -- eight pushes at every site of 78 (0x100cee55 pushes the module's
 * own `s + 0xd8` and `s + 0x128`), where Alice's module pushes seven. They
 * cross into Alice's controller records with a scale of one, which is what an
 * engine without a controller scale applies. */
AliceOrientation *QDECL SV_FAKK2TagOrientation( AliceOrientation *out, int index, int anim,
	int frame, int tag, float scale, const int32_t *boneTag, const float ( *boneQuat )[4] ) {
	fntech3_alice_bonectrl_t controllers[FNTECH3_ALICE_BONE_CONTROLLERS];
	fntech3::fakk2::convert::ControllersToAlice( boneTag, boneQuat, controllers );
	return SV_AliceTagOrientation( out, index, anim, frame, tag, scale, controllers );
}

AliceOrientation *QDECL SV_FAKK2TagOrientationEx( AliceOrientation *out, int index, int anim,
	int frame, int tag, float scale, const int32_t *boneTag, const float ( *boneQuat )[4],
	int crossblendAnim, int crossblendFrame, float crossblendLerp, int useLegs, int useTorso,
	int torsoAnim, int torsoFrame, int torsoCrossblendAnim, int torsoCrossblendFrame,
	float torsoCrossblendLerp ) {
	fntech3_alice_bonectrl_t controllers[FNTECH3_ALICE_BONE_CONTROLLERS];
	fntech3::fakk2::convert::ControllersToAlice( boneTag, boneQuat, controllers );
	return SV_AliceTagOrientationEx( out, index, anim, frame, tag, scale, controllers,
		crossblendAnim, crossblendFrame, crossblendLerp, useLegs, useTorso, torsoAnim,
		torsoFrame, torsoCrossblendAnim, torsoCrossblendFrame, torsoCrossblendLerp );
}

/* F.A.K.K.2's slots 57 and 68 fill the smaller command list its module keeps
 * (code/compat/fakk2_game_abi.h). The definition answers into Alice's record
 * first, which is large enough to hold any list the runtime builds. */
fntech3_alice_tiki_cmd_t fakk2CommandScratch;

qboolean SV_FAKK2Commands( qboolean found, fntech3_fakk2_tiki_cmd_t *commands, const char *call ) {
	if ( !commands ) {
		return qfalse;
	}
	if ( !found ) {
		commands->num_cmds = 0;
		return qfalse;
	}
	if ( !fntech3::fakk2::convert::CommandsFromAlice( fakk2CommandScratch, *commands ) ) {
		Com_DPrintf( "%s: a command list longer than F.A.K.K.2's 128 commands of ten "
			"arguments was cut short\n", call );
	}
	return qtrue;
}

qboolean QDECL SV_FAKK2InitCommands( int index, fntech3_fakk2_tiki_cmd_t *commands ) {
	std::memset( &fakk2CommandScratch, 0, sizeof( fakk2CommandScratch ) );
	return SV_FAKK2Commands( SV_AliceInitCommands( index, &fakk2CommandScratch ),
		commands, "InitCommands" );
}

qboolean QDECL SV_FAKK2FrameCommands( int index, int anim, int frame,
	fntech3_fakk2_tiki_cmd_t *commands ) {
	std::memset( &fakk2CommandScratch, 0, sizeof( fakk2CommandScratch ) );
	return SV_FAKK2Commands( SV_AliceFrameCommands( index, anim, frame, &fakk2CommandScratch ),
		commands, "Frame_Commands" );
}

/* Slots 80 to 88, a definition's own alias list. Retail's clear wrapper calls
 * the dump routine (0x0045cc60) rather than clearing, which is kept. */
/* Retail's wrappers (0x00453f9f, 0x00453fcf) answer nothing for an index with
 * no model definition; here -1 would otherwise mean the global list. */
qboolean QDECL SV_AliceAliasAdd( int index, const char *alias, const char *name, const char *parameters ) {
	const int tiki = SV_AliceTikiForIndex( index );
	return tiki >= 0 ? AliceAlias_Add( tiki, alias, name, parameters ) : qfalse;
}

const char *QDECL SV_AliceAliasFindRandom( int index, const char *alias ) {
	const int tiki = SV_AliceTikiForIndex( index );
	return tiki >= 0 ? AliceAlias_FindRandom( tiki, alias ) : nullptr;
}

void QDECL SV_AliceAliasDump( int index ) {
	const int tiki = SV_AliceTikiForIndex( index );
	if ( tiki >= 0 ) AliceAlias_Dump( tiki );
}

const char *QDECL SV_AliceAliasFindDialog( int index, const char *alias, int random, int entityNumber ) {
	return SV_AliceTikiCall<const char *>( index, nullptr, [&]( int tiki ) {
		return AliceAlias_FindDialog( tiki, alias, random, entityNumber ); } );
}

/* The list itself, which the module keeps a pointer to and walks. Its layout
 * is retail's own and is not established, so no list is handed over: the
 * module's null checks keep it on the path it takes for a model without
 * aliases. */
/* Slot 85 hands the module the model's own alias list (0x0045cdb0): its save
 * code walks the records directly, at retail's layout, and restores them
 * through 86 and 87. */
void *QDECL SV_AliceAliasGetList( int index ) {
	return SV_AliceTikiCall<void *>( index, nullptr, []( int tiki ) { return AliceAlias_GetList( tiki ); } );
}

/* Dialog bookkeeping the module restores from a save: how often a line was
 * played and when. It updates what FindDialog consults. */
void QDECL SV_AliceAliasUpdateDialog( int index, const char *alias, int timesPlayed,
	int played, int lastPlayed ) {
	SV_AliceTikiDo( index, [&]( int tiki ) {
		AliceAlias_UpdateDialog( tiki, alias, -1, timesPlayed, played, lastPlayed ); } );
}

void QDECL SV_AliceAliasAddActorDialog( int index, const char *alias, int actor, int timesPlayed,
	int played, int lastPlayed ) {
	SV_AliceTikiDo( index, [&]( int tiki ) {
		AliceAlias_UpdateDialog( tiki, alias, actor, timesPlayed, played, lastPlayed ); } );
}

const char *QDECL SV_AliceNameForNum( int index ) {
	return SV_AliceTikiCall<const char *>( index, nullptr, []( int tiki ) { return AliceTiki_Name( tiki ); } );
}

/* Slot 49, which is not an index call at all: its body writes a configstring
 * at base 0x360 directly, which is where the block after the image one starts.
 * F.A.K.K.2's client game reads its light styles from 736, the same block after
 * its own image block. Both retail wrappers pass the sum to their bounded
 * configstring setter (alice.exe 0x00454c88..0x00454c90, fakk2.exe 1.02
 * 0x00445af8..0x00445b00). Check the title's remaining range before adding
 * the offset, so a malformed module argument cannot overflow a signed int.
 * The existing refusal of negative style numbers is fnTech3's policy. */
void QDECL SV_AliceSetLightStyle( int style, const char *value ) {
	const int base = SV_RitualLayout().csLightStyles;
	const int limit = SV_RitualLayout().maxConfigstrings;
	if ( style < 0 || style >= limit - base ) {
		Com_Error( ERR_DROP, "%s game module set light style %i", SV_RitualTitle(), style );
		return;
	}
	SV_SetConfigstring( base + style, value ? value : "" );
}

/* Slots 108, 109 and 110, three of the five Alice appends past the reference
 * table's end. Their engine bodies are short enough to read whole.
 *
 * 108 takes one value and stores it in a global: a recorder, like the far
 * plane, and nothing in fnTech3 reads it.
 *
 * 109 takes nothing and, below a threshold, calls on into the engine. There is
 * no fnTech3 equivalent of what it reaches and it returns on the other branch
 * anyway, so it answers and does nothing.
 *
 * 110 names itself: its body reports `Cbuf_ExecuteText: bad exec_when` for any
 * first argument outside nought, one and two, which is this engine's own
 * command buffer call with this engine's own three values. That is a literal
 * naming a slot the map had only as a position, so the map now carries the
 * name. */
int aliceRecorded108;

/* 108 sets the engine UI's loading stage (0x0040b590) and 109 is the same
 * loading-screen refresh the client game's slot 19 reaches; the engine UI
 * owns both, and a dedicated server has none. */
void QDECL SV_AliceRecord108( int value ) {
	aliceRecorded108 = value;
	Com_AliceLoadingStage( value );
}

void QDECL SV_AliceImport109() {
	Com_AliceUpdateLoadingScreen();
}

void QDECL SV_AliceExecuteText( int when, const char *text ) {
	if ( !text ) {
		return;
	}
	if ( when != EXEC_NOW && when != EXEC_INSERT && when != EXEC_APPEND ) {
		Com_Error( ERR_DROP, "%s game module asked to execute text with "
			"exec_when %i, which is not one this engine has", SV_RitualTitle(), when );
		return;
	}
	Cbuf_ExecuteText( static_cast<cbufExec_t>( when ), text );
}

/* Slot 28, the reliable server command, at retail's shape: a client number
 * -- negative for every client -- a format and its arguments. */
void QDECL SV_AliceSendServerCommand( int clientNum, const char *fmt, ... ) {
	char text[MAX_STRING_CHARS];
	va_list argptr;

	if ( !fmt ) {
		return;
	}
	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	if ( clientNum < 0 ) {
		SV_SendServerCommand( nullptr, "%s", text );
		return;
	}
	if ( clientNum >= sv_maxclients->integer ) {
		Com_DPrintf( "%s game sent a command to client %i of %i\n", SV_RitualTitle(),
			clientNum, sv_maxclients->integer );
		return;
	}
	SV_SendServerCommand( &svs.clients[clientNum], "%s", text );
}

/* Slots 93 and 94, the two prints a client draws itself. Retail's centre
 * print at 0x00454250 takes the client number out of the entity's first word,
 * formats into a 1,024-byte buffer, warns "Centerprint text exceeds buffer
 * size" past 256, and copies 256 bytes into the client, which sends it after
 * its next snapshot as `svc_centerprint`; the located print keeps an x and a y
 * beside it and goes as `svc_locprint`. */
client_t *SV_AlicePrintClient( const void *entity ) {
	if ( !entity ) {
		return nullptr;
	}
	const int number = *static_cast<const int32_t *>( entity );
	if ( number < 0 || number >= sv_maxclients->integer ) {
		return nullptr;
	}
	return &svs.clients[number];
}

void QDECL SV_AliceCenterPrintf( const void *entity, const char *fmt, ... ) {
	char text[1024];
	va_list argptr;
	client_t *client = SV_AlicePrintClient( entity );

	if ( !client || !fmt ) {
		return;
	}
	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	if ( strlen( text ) > sizeof( client->aliceCenterPrint ) ) {
		Com_DPrintf( "Centerprint text exceeds buffer size\n" );
	}
	/* Retail leaves the located flag alone here; the snapshot that sends the
	 * print clears it. */
	Q_strncpyz( client->aliceCenterPrint, text, sizeof( client->aliceCenterPrint ) );
}

void QDECL SV_AliceLocationPrintf( const void *entity, int x, int y, const char *fmt, ... ) {
	char text[1024];
	va_list argptr;
	client_t *client = SV_AlicePrintClient( entity );

	if ( !client || !fmt ) {
		return;
	}
	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Q_strncpyz( client->aliceCenterPrint, text, sizeof( client->aliceCenterPrint ) );
	client->aliceLocationPrint = qtrue;
	client->aliceLocationX = x;
	client->aliceLocationY = y;
}

/* Slots 95 and 96. Retail's sound at 0x00457670 appends to a thirty-two entry
 * list on every active client, resolving the name to a sound index -- which
 * registers it as a configstring if it is new -- and copying the origin only
 * when one is given. The stop at 0x00457740 appends a record that carries just
 * the entity and the channel. Both drop silently once a client's list is full,
 * as retail does. */
void QDECL SV_AliceSound( const float *origin, int entityNumber, int channel,
	const char *name, float volume, float minDist ) {
	if ( !name || !name[0] ) {
		return;
	}
	const int index = SV_AliceSoundIndex( name );
	for ( int i = 0; i < sv_maxclients->integer; i++ ) {
		client_t *client = &svs.clients[i];
		if ( client->state != CS_ACTIVE || client->aliceNumSounds >= ALICE_MAX_SERVER_SOUNDS ) {
			continue;
		}
		aliceServerSound_t &sound = client->aliceSounds[client->aliceNumSounds++];
		sound = {};
		if ( origin ) {
			VectorCopy( origin, sound.origin );
			sound.hasOrigin = qtrue;
		}
		sound.entityNumber = entityNumber;
		sound.channel = channel;
		sound.soundIndex = index;
		sound.volume = volume;
		sound.minDist = minDist;
	}
}

void QDECL SV_AliceStopSound( int entityNumber, int channel ) {
	for ( int i = 0; i < sv_maxclients->integer; i++ ) {
		client_t *client = &svs.clients[i];
		if ( client->state != CS_ACTIVE || client->aliceNumSounds >= ALICE_MAX_SERVER_SOUNDS ) {
			continue;
		}
		aliceServerSound_t &sound = client->aliceSounds[client->aliceNumSounds++];
		sound = {};
		sound.stop = qtrue;
		sound.entityNumber = entityNumber;
		sound.channel = channel;
	}
}

/* Slots 97 and 98, the lip-sync pair. Retail puts the same two engine
 * functions here that it hands the client game for its lip calls, so both
 * modules read one cache of `.lip` files. */
float QDECL SV_AliceSoundLength( const char *name ) {
	return Alice_LipLength( name );
}

const byte *QDECL SV_AliceSoundAmplitudes( const char *name, int *count ) {
	return Alice_LipAmplitudes( name, count );
}

/* Slots 26, 31, 32, 37, 38 and 40, each of which maps onto a service this
 * engine already has, at the argument counts the call sites give.
 *
 * The two visibility calls and the area test take the engine's own arguments
 * unchanged. The userinfo pair take a client index, which this engine bounds
 * itself. The console command is text for the command buffer, appended rather
 * than executed immediately: the module is mid-frame when it calls, and
 * running a command inside its own frame would re-enter it. */
void QDECL SV_AliceSendConsoleCommand( const char *text ) {
	if ( text && text[0] ) {
		Cbuf_AddText( text );
	}
}

void QDECL SV_AliceSetUserinfo( int index, const char *value ) {
	if ( index >= 0 && index < sv_maxclients->integer && value ) {
		SV_SetUserinfo( index, value );
	}
}

void QDECL SV_AliceGetUserinfo( int index, char *buffer, int size ) {
	if ( !buffer || size <= 0 ) {
		return;
	}
	buffer[0] = '\0';
	if ( index >= 0 && index < sv_maxclients->integer ) {
		SV_GetUserinfo( index, buffer, size );
	}
}

int QDECL SV_AliceInPVS( const float *p1, const float *p2 ) {
	return ( p1 && p2 ) ? SV_inPVS( p1, p2 ) : 0;
}

int QDECL SV_AliceInPVSIgnorePortals( const float *p1, const float *p2 ) {
	return ( p1 && p2 ) ? SV_inPVSIgnorePortals( p1, p2 ) : 0;
}

int QDECL SV_AliceAreasConnected( int area1, int area2 ) {
	return CM_AreasConnected( area1, area2 );
}

/* Slot 27, a debug graph this engine has no equivalent for, and slot 18, which
 * prepares a path for writing -- something this engine's own file open already
 * does for itself. Both answer and do nothing. */
void QDECL SV_AliceNothing() {
}

/* Slots 41 and 42, the first imports that take one of Alice's own entities
 * rather than a value the engine already has a word for. Serving them at all
 * means agreeing with the module about the inside of a 732-byte record, and
 * only part of that record is established -- the part the retail engine's own
 * linking code dereferences, at 0x00457a40 and 0x004579e0:
 *
 *     +0x1b4  the state's solid: a bounding box packed by 0x0044a000, the
 *             brush-model marker 0xffffff, or zero (ritual_solid.hpp)
 *     +0x204  compared against zero; zero means do not link
 *     +0x218  the low bound, passed as a vector
 *     +0x224  the high bound, passed as a vector
 *     +0x230  contents
 *     +0x27c  the solid type, compared against 1, 2 and 3
 *
 * The bounds are the firmest of those: the engine hands them straight to its
 * box-model routine as a mins/maxs pair, so their order and type come from the
 * call rather than from resemblance.
 *
 * What is served here is the opening of that function and no more: bound the
 * pointer, and for the one solid type whose branch is unambiguous, build the
 * box model and write the handle back. The entity is not put into any world,
 * so nothing can trace against it or find it by area yet -- this is the module
 * being allowed to continue, not a working link. */
/* Where the engine reads and writes the running title's entity record in place;
 * Alice's offsets are retail alice.exe's own, F.A.K.K.2's the same members 24
 * bytes earlier (code/compat/ritual_layout.hpp). */
#define FNTECH3_RITUAL_ENT_CLIPHANDLE ( SV_RitualLayout().sSolid )
	/* Two separate fields, and only one of them is `linked`.
	 *
	 * The retail linker returns immediately when `+0x204` is zero, and the
	 * snapshot walk skips an entity whose `+0x204` is zero, but neither ever
	 * writes it: the module owns it, and it reads like an "in use" flag.
	 *
	 * `+0x208` is the linked flag. The linker's last act is to set it to one
	 * (`0x00457ef7`) and the unlinker's first is to clear it (`0x004579eb`),
	 * and the snapshot walk requires it as well. */
#define FNTECH3_RITUAL_ENT_LINKGATE ( SV_RitualLayout().rInuse )
#define FNTECH3_RITUAL_ENT_LINKED ( SV_RitualLayout().rLinked )
#define FNTECH3_RITUAL_ENT_MODELINDEX ( SV_RitualLayout().sModelIndex )
#define FNTECH3_RITUAL_ENT_BMODEL ( SV_RitualLayout().rBmodel )
#define FNTECH3_RITUAL_ENT_MINS ( SV_RitualLayout().rMins )
#define FNTECH3_RITUAL_ENT_MAXS ( SV_RitualLayout().rMaxs )
#define FNTECH3_RITUAL_ENT_CONTENTS ( SV_RitualLayout().rContents )
#define FNTECH3_RITUAL_ENT_ABSMIN ( SV_RitualLayout().rAbsmin )
#define FNTECH3_RITUAL_ENT_ABSMAX ( SV_RitualLayout().rAbsmax )
	/* The world origin, and the angles immediately after it, which is the
	 * pairing every engine of this lineage keeps. The retail linker reads
	 * it at 0x00457c01 to build absmin -- `absmin[i] = mins[i] + origin[i]`
	 * -- and again at 0x00457c39 for absmax, then pulls the pair one unit
	 * apart the way Quake III does. It tests the three angles at 0x00457ae1
	 * against zero to decide whether the box may be offset at all.
	 *
	 * This was 0x1dc until the linker was read. That offset holds a
	 * rotation, not a translation: the rotated-brush branch hands it to a
	 * bounds transform that walks the box's eight corners, so it is what
	 * turns a box rather than where the box is. An entity linked with it
	 * was linked in the wrong place. */
#define FNTECH3_RITUAL_ENT_ORIGIN ( SV_RitualLayout().rCurrentOrigin )
#define FNTECH3_RITUAL_ENT_ANGLES ( SV_RitualLayout().rCurrentAngles )
#define FNTECH3_RITUAL_ENT_SOLID ( SV_RitualLayout().rSolidType )

/* An entity pointer is only usable if it lies on a record boundary inside the
 * module's own array. Where that array is and how long it is comes from the
 * module rather than from fnTech3: export slots 21, 23 and 24 are the three
 * the module writes at run time, and they are the array, the count in use and
 * the capacity.
 *
 * LocateGameData's own count is not the bound. It reports the count at the
 * moment it is called -- eight on a server with eight client slots -- and the
 * module goes on to spawn a level's worth of entities past that, so bounding
 * by it refuses the first world entity linked. */
int SV_AliceEntityNumber( void *entity ) {
	if ( !entity || !aliceGameExports || aliceGameData.entityBytes <= 0 ) {
		return -1;
	}
	const auto base = reinterpret_cast<byte *>(
		aliceGameExports[FNTECH3_ALICE_EXPORT_gentities] );
	const std::intptr_t capacity =
		aliceGameExports[FNTECH3_ALICE_EXPORT_max_entities];
	if ( !base || capacity <= 0 || capacity > MAX_GENTITIES ) {
		return -1;
	}
	const std::ptrdiff_t delta = static_cast<byte *>( entity ) - base;
	if ( delta < 0 || delta % aliceGameData.entityBytes ) {
		return -1;
	}
	const std::ptrdiff_t index = delta / aliceGameData.entityBytes;
	if ( index >= capacity ) {
		return -1;
	}
	return (int)index;
}

template<typename T>
T *SV_AliceEntityField( void *entity, int offset ) {
	return reinterpret_cast<T *>( static_cast<byte *>( entity ) + offset );
}

/* Slots 33 and 52, which between them name two more of the record's fields.
 *
 * `SetBrushModel` refuses a name that is null or does not begin with a star,
 * by its own two literals, then reads the number after the star and:
 *
 *     ent[+0xb8]  = that number                  the model index
 *     ent[+0x218] = the inline model's low bound
 *     ent[+0x224] = its high bound
 *     ent[+0x214] = 1                            this is a brush model
 *     ent[+0x230] = -1, when it was zero         contents default to all
 *
 * The bounds it writes are the same pair `linkentity` reads, which is the
 * second independent sighting of that layout, and the two agree.
 *
 * `setmodel` is the same call with the star test in front: a starred name goes
 * to the brush path, and anything else is indexed as a model and marked not a
 * brush model. Both are served exactly as the engine's own bodies do it,
 * because every field they touch is one those bodies name. */
void QDECL SV_AliceSetBrushModel( void *entity, const char *name ) {
	if ( SV_AliceEntityNumber( entity ) < 0 ) {
		Com_Error( ERR_DROP, "%s game module gave a brush model to an "
			"entity that is not in the array it published", SV_RitualTitle() );
		return;
	}
	if ( !name || !name[0] ) {
		Com_Error( ERR_DROP, "%s game module set a brush model with no name", SV_RitualTitle() );
		return;
	}
	if ( name[0] != '*' ) {
		Com_Error( ERR_DROP, "%s game module set brush model \"%s\", which "
			"is not a brush model name", SV_RitualTitle(), name );
		return;
	}

	const int number = atoi( name + 1 );
	*SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_MODELINDEX ) = number;

	const clipHandle_t handle = CM_InlineModel( number );
	float *mins = SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_MINS );
	float *maxs = SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_MAXS );
	CM_ModelBounds( handle, mins, maxs );

	*SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_BMODEL ) = 1;

	int *contents = SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_CONTENTS );
	if ( !*contents ) {
		*contents = -1;
	}
}

void QDECL SV_AliceSetModel( void *entity, const char *name ) {
	if ( SV_AliceEntityNumber( entity ) < 0 ) {
		Com_Error( ERR_DROP, "%s game module set a model on an entity that "
			"is not in the array it published", SV_RitualTitle() );
		return;
	}
	if ( !name ) {
		Com_Error( ERR_DROP, "%s game module set a model with no name", SV_RitualTitle() );
		return;
	}
	if ( name[0] == '*' ) {
		SV_AliceSetBrushModel( entity, name );
		return;
	}
	/* Retail's body at 0x00454190 is the model index call -- which loads the
	 * definition behind a `tik` name -- and a store into the entity. */
	*SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_MODELINDEX ) =
		SV_AliceModelIndex( name );
	*SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_BMODEL ) = 0;
}

/* Which entities are currently linked. Alice's own record carries a gate at
 * +0x204 and the absolute bounds the queries below test, so the world this
 * engine keeps for the title is no more than the set of entity numbers that
 * have been linked and not unlinked. A sector tree would be faster; with a
 * level's several hundred entities a walk is not the bottleneck, and it keeps
 * this boundary free of any structure the module cannot see. */
byte aliceLinked[MAX_GENTITIES];

void *SV_AliceEntityByNumber( int number ) {
	if ( !aliceGameExports || number < 0 || number >= MAX_GENTITIES ) {
		return nullptr;
	}
	const auto base = reinterpret_cast<byte *>(
		aliceGameExports[FNTECH3_ALICE_EXPORT_gentities] );
	if ( !base || aliceGameData.entityBytes <= 0 ) {
		return nullptr;
	}
	return base + (std::ptrdiff_t)number * aliceGameData.entityBytes;
}

bool SV_AliceBoundsOverlap( const float *aMin, const float *aMax,
	const float *bMin, const float *bMax ) {
	for ( int i = 0; i < 3; i++ ) {
		if ( aMin[i] > bMax[i] || aMax[i] < bMin[i] ) {
			return false;
		}
	}
	return true;
}

/* Slot 36, at two arguments, returning a brush number for a point. The engine
 * returns -1 when it has no world loaded, by `or eax, 0xffffffff` on that
 * path, and -1 is what this returns always: fnTech3's collision does not hand
 * out brush numbers, so there is no number to give that would mean anything.
 *
 * The module calls this at two sites and neither is reached in a frame, so
 * nothing observable depends on the answer today. It is served rather than
 * refused because -1 is a real answer here -- "no brush" -- rather than a
 * stand-in for one. */
int QDECL SV_AlicePointBrushNum( const float *point, int contentMask ) {
	(void)point;
	(void)contentMask;
	return -1;
}

/* Slot 43, at four arguments: a box, a list to fill and its length, returning
 * how many entity numbers were put in the list. Every linked entity whose
 * absolute bounds meet the box is one of them. */
int QDECL SV_AliceAreaEntities( const float *mins, const float *maxs,
	int *list, int maxcount ) {
	int count = 0;

	if ( !mins || !maxs || !list || maxcount <= 0 ) {
		return 0;
	}
	for ( int i = 0; i < MAX_GENTITIES && count < maxcount; i++ ) {
		if ( !aliceLinked[i] ) {
			continue;
		}
		void *entity = SV_AliceEntityByNumber( i );
		if ( !entity ) {
			continue;
		}
		const float *absmin =
			SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_ABSMIN );
		const float *absmax =
			SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_ABSMAX );

		if ( SV_AliceBoundsOverlap( mins, maxs, absmin, absmax ) ) {
			list[count++] = i;
		}
	}
	return count;
}

/* Slot 44, at seven arguments: a result, a start, a box, an end, one entity
 * number and a content mask. It traces against that entity alone, which is
 * what the clip handle written during linking is for -- a brush model traces
 * against its own brushes at its own origin and angles, and anything else
 * against the temporary box built from its bounds.
 *
 * The capsule flag is the trace's: retail's entity clip (0x00458130) hands the
 * clip record's copy of it to the transformed trace at 0x00458267. Slot 44
 * itself has no such argument, so it clips as a box. */
/* Whether an entity clips as its own brushes: a brush model that is not a
 * bounding box (the test at 0x004577b4..0x004577c5). */
bool SV_AliceClipsAsBrushes( void *entity ) {
	return *SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_BMODEL ) != 0
		&& *SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_SOLID ) != 2;
}

/* SV_ClipHandleForEntity (0x004577b0), which retail asks at every clip -- the
 * trace's entity pass at 0x0045821b and the single-entity clip at 0x00453709
 * -- rather than keeping an answer: a brush model traces against its own
 * inline model, and anything else against the temporary box, built again from
 * its own bounds and contents. Ritual's CM_TempBoxModel takes the contents and
 * writes them into the box brush (0x00430f50). The box is one model that every
 * build overwrites, so a handle kept from linking would name whichever entity
 * was built last; and linking writes none for a brush model. */
clipHandle_t SV_AliceClipHandleForEntity( void *entity ) {
	if ( SV_AliceClipsAsBrushes( entity ) ) {
		const int model = *SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_MODELINDEX );
		return model > 0 && model < CM_NumInlineModels() ? CM_InlineModel( model ) : 0;
	}
	const clipHandle_t box = CM_TempBoxModel(
		SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_MINS ),
		SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_MAXS ), qfalse );
	CM_SetTempBoxModelContents( *SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_CONTENTS ) );
	return box;
}

void SV_AliceClipEntity( trace_t *trace, const float *start, const float *end,
	const float *mins, const float *maxs, int number, int contentMask,
	qboolean capsule ) {
	static const vec3_t zero = { 0.0f, 0.0f, 0.0f };
	void *entity = SV_AliceEntityByNumber( number );

	trace->fraction = 1.0f;
	trace->entityNum = ENTITYNUM_NONE;
	if ( !entity || !aliceLinked[number] ) {
		return;
	}

	const clipHandle_t handle = SV_AliceClipHandleForEntity( entity );
	if ( !handle ) {
		return;
	}
	const float *origin =
		SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_ORIGIN );
	const float *angles =
		SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_ANGLES );

	/* Only brushes turn with the entity; a box never does (0x00458229..0x00458242). */
	CM_TransformedBoxTrace( trace, start, end, mins, maxs, handle, contentMask,
		origin, SV_AliceClipsAsBrushes( entity ) ? angles : zero, capsule );
	if ( trace->fraction < 1.0f || trace->startsolid || trace->allsolid ) {
		trace->entityNum = number;
	}
}

void QDECL SV_AliceClipToEntity( void *results, const float *start,
	const float *mins, const float *maxs, const float *end, int number,
	int contentMask ) {
	static const vec3_t zero = { 0.0f, 0.0f, 0.0f };
	trace_t trace;

	if ( !results ) {
		return;
	}
	Com_Memset( results, 0, FNTECH3_ALICE_TRACE_BYTES );
	Com_Memset( &trace, 0, sizeof( trace ) );
	if ( !start || !end ) {
		trace.fraction = 1.0f;
		trace.entityNum = ENTITYNUM_NONE;
	} else {
		SV_AliceClipEntity( &trace, start, end, mins ? mins : zero,
			maxs ? maxs : zero, number, contentMask, qfalse );
	}
	Com_Memcpy( results, &trace, sizeof( trace ) );
}

/* Slot 34, the trace, and the import that most of the retail maps stop at.
 *
 * Its shape is measured. The import takes eight arguments -- a result, a
 * start, a low and a high bound, an end and three trailing integers -- from
 * the argument slots its body reads and the eight pushes at its call sites.
 * The result it fills is **60 bytes**: the engine copies fifteen dwords into
 * the caller's buffer, in two different routines, and the thirty dwords it
 * clears first are scratch rather than the record.
 *
 * Two of those fifteen dwords are named by what the engine does to them, not
 * by where they sit:
 *
 *   +0x08  set to 0x3f800000, which is 1.0f, on the path where nothing is
 *          hit, and read back with `fld`. That is a fraction.
 *   +0x34  set to 1022, or 1023 on one branch. Those are the reserved entity
 *          numbers of a 1024-entity world: none, and the world itself.
 *
 * Both sit exactly where Quake III's own trace record keeps the same two
 * things, and a third sighting agrees: the module reads `+0x20` at its call
 * sites, which in that layout is the impact plane's vertical normal -- what a
 * ground test reads. So the first 56 bytes are filled here as that record,
 * with the remaining four left zero.
 *
 * That last step is a hypothesis, and it is worth naming as one. Three fields
 * agreeing does not prove the other eleven do, and 60 bytes is not 56. What
 * makes it worth acting on rather than guessing is that it is testable: a
 * wrong layout does not fail quietly here, it has entities fall through floors
 * or stick in the air, and the maps that stopped at this import either run
 * afterwards or they do not.
 *
 * The world is traced first, then every linked entity the sweep could reach,
 * as retail's entity pass (0x00458130) clips them: each one's clip model is
 * built at that moment (SV_AliceClipHandleForEntity), and the entity number
 * names the world, the entity hit, or nothing.
 *
 * The eighth argument is the capsule flag. Retail's body (0x004582f0) pushes
 * it straight through as CM_BoxTrace's last argument at 0x00458340, the one
 * Quake III 1.32 calls `capsule`, and keeps it in its clip record for the
 * entity pass, so a caller that asks for a cylinder traces one against the
 * world and against every entity alike. */
void QDECL SV_AliceTrace( void *results, const float *start, const float *mins,
	const float *maxs, const float *end, int passEntityNum, int contentMask,
	int cylinder ) {
	static const vec3_t zero = { 0.0f, 0.0f, 0.0f };
	const qboolean capsule = cylinder ? qtrue : qfalse;
	trace_t trace;

	if ( !results ) {
		return;
	}
	Com_Memset( results, 0, FNTECH3_ALICE_TRACE_BYTES );
	if ( !start || !end ) {
		/* Nothing to trace along. A fraction of one is "hit nothing", which
		 * is what the engine's own early-out writes. */
		*reinterpret_cast<float *>( static_cast<byte *>( results ) + 8 ) = 1.0f;
		*reinterpret_cast<int *>( static_cast<byte *>( results ) + 0x34 ) =
			ENTITYNUM_NONE;
		return;
	}

	/* The engine defaults a null bound to a zero vector rather than refusing,
	 * which is what makes a point trace expressible. */
	CM_BoxTrace( &trace, start, end, mins ? mins : zero, maxs ? maxs : zero,
		0, contentMask, capsule );
	trace.entityNum = trace.fraction != 1.0f ? ENTITYNUM_WORLD : ENTITYNUM_NONE;

	/* Then every linked entity the moving box could reach, keeping the nearest
	 * hit. The box that sweeps is the one the caller gave, grown by the
	 * entity's own extent, which is how a swept box and a static box are
	 * compared without moving either. */
	if ( trace.fraction > 0.0f ) {
		vec3_t sweepMin, sweepMax;
		int touched[MAX_GENTITIES];

		for ( int i = 0; i < 3; i++ ) {
			const float from = start[i] + ( mins ? mins[i] : 0.0f );
			const float to = trace.endpos[i] + ( maxs ? maxs[i] : 0.0f );
			const float lo = start[i] + ( mins ? mins[i] : 0.0f );
			const float hi = trace.endpos[i] + ( mins ? mins[i] : 0.0f );

			sweepMin[i] = ( lo < hi ? lo : hi ) - 1.0f;
			sweepMax[i] = ( from > to ? from : to ) + 1.0f;
			if ( start[i] + ( maxs ? maxs[i] : 0.0f ) > sweepMax[i] ) {
				sweepMax[i] = start[i] + ( maxs ? maxs[i] : 0.0f ) + 1.0f;
			}
		}

		const int count = SV_AliceAreaEntities( sweepMin, sweepMax, touched,
			MAX_GENTITIES );
		/* Retail's entity pass (0x00458130..0x004582e5), which is Quake III's:
		 * nothing that is not solid, and -- when something is passing -- not
		 * the passing entity, its owner, its own missiles or its owner's
		 * other missiles (0x004581d2..0x00458203, the owner at +0x278, just
		 * after the current angles); nothing whose contents the mask does not
		 * take (0x00458209). The nearest hit wins, and a solid start or an
		 * entirely solid trace is kept whatever its fraction, the latter
		 * ending the pass (0x0045826c..0x004582ca). */
		const int ownerOffset = SV_RitualLayout().rCurrentAngles + 12;
		void *passing = passEntityNum != ENTITYNUM_NONE ? SV_AliceEntityByNumber( passEntityNum ) : nullptr;
		int passOwner = passing ? *SV_AliceEntityField<int>( passing, ownerOffset ) : ENTITYNUM_NONE;
		if ( passOwner == ENTITYNUM_NONE ) {
			passOwner = -1;
		}
		for ( int i = 0; i < count && !trace.allsolid; i++ ) {
			void *touch = SV_AliceEntityByNumber( touched[i] );
			if ( !touch ) {
				continue;
			}
			const int solid = *SV_AliceEntityField<int>( touch, FNTECH3_RITUAL_ENT_SOLID );
			if ( solid == 0 || solid == 1 ) {
				continue;
			}
			if ( passEntityNum != ENTITYNUM_NONE ) {
				const int touchOwner = *SV_AliceEntityField<int>( touch, ownerOffset );
				if ( touched[i] == passEntityNum || touched[i] == passOwner
					|| touchOwner == passEntityNum || touchOwner == passOwner ) {
					continue;
				}
			}
			if ( !( *SV_AliceEntityField<int>( touch, FNTECH3_RITUAL_ENT_CONTENTS ) & contentMask ) ) {
				continue;
			}
			trace_t hit;

			Com_Memset( &hit, 0, sizeof( hit ) );
			SV_AliceClipEntity( &hit, start, end, mins ? mins : zero,
				maxs ? maxs : zero, touched[i], contentMask, capsule );
			if ( hit.allsolid ) {
				trace.allsolid = qtrue;
				trace.entityNum = touched[i];
			} else if ( hit.startsolid ) {
				trace.startsolid = qtrue;
				trace.entityNum = touched[i];
			}
			if ( hit.fraction < trace.fraction ) {
				const qboolean startedSolid = trace.startsolid;

				trace = hit;
				trace.startsolid = static_cast<qboolean>( trace.startsolid | startedSolid );
				trace.entityNum = touched[i];
			}
		}
	}

	Com_Memcpy( results, &trace, sizeof( trace ) );
}

/* Slot 39, at two arguments. The engine's body (0x004536d0) looks the entity
 * up in its own side table by its number and reads two area numbers out of
 * it -- the second at +0x254 of that record, the first at +0x250 -- returns
 * without doing anything when the second is -1, meaning the entity does not
 * straddle two areas, and otherwise hands both and the flag to
 * CM_AdjustAreaPortalState (0x004361a0). The side table is this engine's
 * svEntity_t, whose two areas linking fills below. */
void QDECL SV_AliceAdjustAreaPortalState( void *entity, int open ) {
	const int number = SV_AliceEntityNumber( entity );

	if ( number < 0 ) {
		Com_Error( ERR_DROP, "%s game module adjusted the portal of an entity "
			"that is not in the array it published", SV_RitualTitle() );
		return;
	}
	const svEntity_t &record = sv.svEntities[number];
	if ( record.areanum2 == -1 ) {
		return;
	}
	CM_AdjustAreaPortalState( record.areanum, record.areanum2, open ? qtrue : qfalse );
}

void QDECL SV_AliceLinkEntity( void *entity ) {
	const int number = SV_AliceEntityNumber( entity );

	if ( number < 0 ) {
		Com_Error( ERR_DROP, "%s game module linked an entity that is not "
			"in the array it published", SV_RitualTitle() );
		return;
	}
	if ( !*SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_LINKGATE ) ) {
		return;
	}

	const int solid = *SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_SOLID );
	const float *mins = SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_MINS );
	const float *maxs = SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_MAXS );

	/* The state's solid, which the client game unpacks to clip against the
	 * entity: a bounding box packed by BoundingBoxToInteger (0x0044a000), the
	 * brush-model marker, or zero (0x00457a78..0x00457bfc; ritual_solid.hpp).
	 * This link once wrote the temporary box's handle here, which clients read
	 * as a box 511 units wide and none high. Traces build their clip model
	 * when they clip (SV_AliceClipHandleForEntity), as retail's do. */
	*SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_CLIPHANDLE ) = fntech3::ritual::StateSolid( solid,
		*SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_CONTENTS ), mins, maxs );

	/* The absolute bounds, which the engine computes here and the module reads
	 * back. It copies the local bounds into the absolute pair and offsets them
	 * by the origin, and it only takes that simple path when the entity is
	 * unrotated: it tests all three angles against zero first, because a
	 * rotated box does not keep its own extents once it turns.
	 *
	 * The rotated case is handled the way every engine of this lineage handles
	 * it -- take the largest extent in any direction and make a cube of it,
	 * which is larger than the true bounds but never smaller, so nothing is
	 * missed by a query that uses it. */
	const float *origin = SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_ORIGIN );
	const float *angles = SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_ANGLES );
	float *absmin = SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_ABSMIN );
	float *absmax = SV_AliceEntityField<float>( entity, FNTECH3_RITUAL_ENT_ABSMAX );
	const bool rotated = ( solid == 3 ) &&
		( angles[0] || angles[1] || angles[2] );

	if ( rotated ) {
		float extent = 0.0f;

		for ( int i = 0; i < 3; i++ ) {
			const float low = mins[i] < 0.0f ? -mins[i] : mins[i];
			const float high = maxs[i] < 0.0f ? -maxs[i] : maxs[i];

			if ( low > extent ) extent = low;
			if ( high > extent ) extent = high;
		}
		for ( int i = 0; i < 3; i++ ) {
			absmin[i] = origin[i] - extent;
			absmax[i] = origin[i] + extent;
		}
	} else {
		for ( int i = 0; i < 3; i++ ) {
			absmin[i] = origin[i] + mins[i];
			absmax[i] = origin[i] + maxs[i];
		}
	}

	/* A small outward margin, as this lineage's engines all keep, so that an
	 * entity resting exactly on a boundary is still found by a query. */
	for ( int i = 0; i < 3; i++ ) {
		absmin[i] -= 1.0f;
		absmax[i] += 1.0f;
	}

	/* Where the snapshot walk will look for it: the areas and the PVS
	 * clusters the box touches, recorded exactly as every other profile's
	 * linker records them. Without them the walk culls every entity that is
	 * not broadcast, the player's own included. Retail keeps the two areas in
	 * its side table at +0x250 and +0x254 (written at 0x00457cec), which slot
	 * 39 reads back. */
	SV_LinkEntityClusters( &sv.svEntities[number], absmin, absmax, number );

	/* The module's own copy of the answer. The retail linker's last act is to
	 * write one here (`0x00457ef7`) and the unlinker's first is to clear it
	 * (`0x004579eb`), and the server's snapshot walk reads it back before it
	 * will send an entity, so the module may read it too. fnTech3 keeps its
	 * own array as well, because that is what its world is indexed by, but
	 * leaving the module's field clear would have told the module the
	 * opposite of what just happened. */
	*SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_LINKED ) = 1;
	aliceLinked[number] = 1;
}

void QDECL SV_AliceUnlinkEntity( void *entity ) {
	const int number = SV_AliceEntityNumber( entity );

	if ( number < 0 ) {
		return;
	}
	aliceLinked[number] = 0;
	*SV_AliceEntityField<int>( entity, FNTECH3_RITUAL_ENT_LINKED ) = 0;

	/* An unlinked entity touches no area and no cluster. */
	svEntity_t &record = sv.svEntities[number];
	record.numClusters = 0;
	record.lastCluster = 0;
	record.areanum = -1;
	record.areanum2 = -1;
}

/*
==================
SV_AliceFillImports

Fill every slot, then replace the ones fnTech3 serves. The order matters: the
unserved thunks go down first so that a slot nobody remembered still drops by
name rather than being left null.
==================
*/
void SV_AliceFillImports() {
	SV_AliceFillUnserved(
		std::make_index_sequence<FNTECH3_ALICE_GAME_IMPORT_SLOTS>{} );

	/* A new game module starts a new level's model indices, unless a load kept
	 * the level's own. */
	std::memset( aliceModelTiki, 0, sizeof( aliceModelTiki ) );
	SV_AliceRegisterKeptModels();
	/* Retail's table build clears the shared block first, and the module
	 * publishes its debug lines afresh. */
	Alice_SharedBlockClear();
	*Alice_DebugLineArray() = nullptr;
	*Alice_DebugLineCount() = 0;

/* The parameter may not be called `slot`: it would replace the member name in
 * `aliceImport.slot` as well as the enumerator suffix. */
#define SERVE( member, fn ) \
	aliceImport.slot[FNTECH3_ALICE_IMPORT_##member] = \
		reinterpret_cast<std::intptr_t>( &fn )

	SERVE( Printf, SV_AlicePrintf );
	SERVE( DPrintf, SV_AliceDPrintf );
	SERVE( DebugPrintf, SV_AliceDPrintf );
	SERVE( AliceImport106, SV_AliceImport106 );
	SERVE( Error, SV_AliceError );
	SERVE( Milliseconds, SV_AliceMilliseconds );
	SERVE( Malloc, SV_AliceMalloc );
	SERVE( Free, SV_AliceFree );
	SERVE( cvar, SV_AliceCvar );
	SERVE( AddCommand, SV_AliceAddCommand );
	SERVE( cvar_set, SV_AliceCvarSet );
	SERVE( argc, SV_AliceArgc );
	SERVE( argv, SV_AliceArgv );
	SERVE( args, SV_AliceArgs );
	SERVE( FS_ReadFile, SV_AliceReadFile );
	SERVE( FS_FreeFile, SV_AliceFreeFile );
	SERVE( FS_WriteFile, SV_AliceWriteFile );
	SERVE( FS_FOpenFileWrite, SV_AliceFOpenFileWrite );
	SERVE( FS_FOpenFileAppend, SV_AliceFOpenFileAppend );
	SERVE( FS_Write, SV_AliceFSWrite );
	SERVE( FS_Read, SV_AliceFSRead );
	SERVE( FS_FCloseFile, SV_AliceFCloseFile );
	SERVE( FS_FTell, SV_AliceFTell );
	SERVE( FS_FSeek, SV_AliceFSeek );
	SERVE( FS_Flush, SV_AliceFSFlush );
	SERVE( GetArchiveFileName, SV_AliceGetArchiveFileName );
	SERVE( CalcCRC, SV_AliceCalcCRC );
	SERVE( AliceImport107, SV_AliceSharedBlock );
	SERVE( LocateGameData, SV_AliceLocateGameData );
	SERVE( SetFarPlane, SV_AliceSetFarPlane );
	SERVE( AliceImport108, SV_AliceRecord108 );
	SERVE( AliceImport109, SV_AliceImport109 );
	SERVE( Cbuf_ExecuteText, SV_AliceExecuteText );
	SERVE( SetSkyPortal, SV_AliceSetSkyPortal );
	SERVE( IsModel, SV_AliceIsModel );
	SERVE( GameDir, SV_AliceGameDir );

	/* The model queries, through the shared TIKI runtime. */
	SERVE( NumAnims, SV_AliceNumAnims );
	SERVE( NumSkins, SV_AliceNumSkins );
	SERVE( NumSurfaces, SV_AliceNumSurfaces );
	SERVE( NumTags, SV_AliceNumTags );
	SERVE( InitCommands, SV_AliceInitCommands );
	SERVE( CalculateBounds, SV_AliceCalculateBounds );
	SERVE( Anim_NameForNum, SV_AliceAnimNameForNum );
	SERVE( Anim_NumForName, SV_AliceAnimNumForName );
	SERVE( Anim_Random, SV_AliceAnimRandom );
	SERVE( Anim_NumFrames, SV_AliceAnimNumFrames );
	SERVE( Anim_Time, SV_AliceAnimTime );
	SERVE( Anim_Delta, SV_AliceAnimDelta );
	SERVE( Anim_AbsoluteDelta, SV_AliceAnimAbsoluteDelta );
	SERVE( Anim_Flags, SV_AliceAnimFlags );
	SERVE( Anim_HasCommands, SV_AliceAnimHasCommands );
	SERVE( Frame_Commands, SV_AliceFrameCommands );
	SERVE( Frame_Delta, SV_AliceFrameDelta );
	SERVE( Frame_Time, SV_AliceFrameTime );
	SERVE( Frame_Bounds, SV_AliceFrameBounds );
	SERVE( Surface_NameToNum, SV_AliceSurfaceNameToNum );
	SERVE( Surface_NumToName, SV_AliceSurfaceNumToName );
	SERVE( Surface_Flags, SV_AliceSurfaceFlags );
	SERVE( Surface_NumSkins, SV_AliceSurfaceNumSkins );
	SERVE( Tag_NumForName, SV_AliceTagNumForName );
	SERVE( Tag_NameForNum, SV_AliceTagNameForNum );
	SERVE( Tag_Orientation, SV_AliceTagOrientation );
	SERVE( Tag_OrientationEx, SV_AliceTagOrientationEx );
	SERVE( Alias_Add, SV_AliceAliasAdd );
	SERVE( Alias_FindRandom, SV_AliceAliasFindRandom );
	SERVE( Alias_Dump, SV_AliceAliasDump );
	SERVE( Alias_Clear, SV_AliceAliasDump );
	SERVE( Alias_FindDialog, SV_AliceAliasFindDialog );
	SERVE( Alias_GetList, SV_AliceAliasGetList );
	SERVE( Alias_UpdateDialog, SV_AliceAliasUpdateDialog );
	SERVE( Alias_AddActorDialog, SV_AliceAliasAddActorDialog );
	SERVE( NameForNum, SV_AliceNameForNum );
	SERVE( pointcontents, SV_AlicePointContents );
	SERVE( GlobalAlias_Add, SV_AliceGlobalAliasAdd );
	SERVE( GlobalAlias_FindRandom, SV_AliceGlobalAliasFindRandom );
	SERVE( GlobalAlias_Dump, SV_AliceGlobalAliasDump );
	SERVE( GlobalAlias_Clear, SV_AliceGlobalAliasClear );
	SERVE( imageindex, SV_AliceImageIndex );
	SERVE( itemindex, SV_AliceItemIndex );
	SERVE( soundindex, SV_AliceSoundIndex );
	SERVE( modelindex, SV_AliceModelIndex );
	SERVE( SetLightStyle, SV_AliceSetLightStyle );
	SERVE( setConfigstring, SV_AliceSetConfigstring );
	SERVE( getConfigstring, SV_AliceGetConfigstring );
	SERVE( setmodel, SV_AliceSetModel );
	SERVE( SetBrushModel, SV_AliceSetBrushModel );
	SERVE( trace, SV_AliceTrace );
	SERVE( SendConsoleCommand, SV_AliceSendConsoleCommand );
	SERVE( setUserinfo, SV_AliceSetUserinfo );
	SERVE( getUserinfo, SV_AliceGetUserinfo );
	SERVE( inPVS, SV_AliceInPVS );
	SERVE( inPVSIgnorePortals, SV_AliceInPVSIgnorePortals );
	SERVE( AreasConnected, SV_AliceAreasConnected );
	SERVE( DebugGraph, SV_AliceNothing );
	SERVE( FS_PrepFileWrite, SV_AliceNothing );
	SERVE( SendServerCommand, SV_AliceSendServerCommand );
	SERVE( AdjustAreaPortalState, SV_AliceAdjustAreaPortalState );
	SERVE( pointbrushnum, SV_AlicePointBrushNum );
	SERVE( AreaEntities, SV_AliceAreaEntities );
	SERVE( ClipToEntity, SV_AliceClipToEntity );

	/* What the game sends a client besides snapshots. */
	SERVE( centerprintf, SV_AliceCenterPrintf );
	SERVE( locationprintf, SV_AliceLocationPrintf );
	SERVE( Sound, SV_AliceSound );
	SERVE( StopSound, SV_AliceStopSound );
	SERVE( SoundLength, SV_AliceSoundLength );
	SERVE( SoundAmplitudes, SV_AliceSoundAmplitudes );
	SERVE( linkentity, SV_AliceLinkEntity );
	SERVE( unlinkentity, SV_AliceUnlinkEntity );

	/* The two data slots. These carry the address of a variable rather than
	 * of a function, so they are assigned directly: SERVE takes the address of
	 * a function and would be the wrong shape here. */
	aliceImport.slot[FNTECH3_ALICE_IMPORT_DebugLines] =
		reinterpret_cast<std::intptr_t>( Alice_DebugLineArray() );
	aliceImport.slot[FNTECH3_ALICE_IMPORT_numDebugLines] =
		reinterpret_cast<std::intptr_t>( Alice_DebugLineCount() );

#undef SERVE
}

/* The engine asks for a Quake III export by Quake III's ordinal. Alice's
 * table is numbered differently -- its slot 0 is a version, not a call --
 * so an incoming ordinal must not be looked up in the Alice map. Naming
 * the engine's own call is what tells the reader what was wanted. */
const char *SV_AliceEngineCallName( int command ) {
	switch ( command ) {
	case GAME_INIT: return "GAME_INIT";
	case GAME_SHUTDOWN: return "GAME_SHUTDOWN";
	case GAME_CLIENT_CONNECT: return "GAME_CLIENT_CONNECT";
	case GAME_CLIENT_BEGIN: return "GAME_CLIENT_BEGIN";
	case GAME_CLIENT_USERINFO_CHANGED: return "GAME_CLIENT_USERINFO_CHANGED";
	case GAME_CLIENT_DISCONNECT: return "GAME_CLIENT_DISCONNECT";
	case GAME_CLIENT_COMMAND: return "GAME_CLIENT_COMMAND";
	case GAME_CLIENT_THINK: return "GAME_CLIENT_THINK";
	case GAME_RUN_FRAME: return "GAME_RUN_FRAME";
	case GAME_CONSOLE_COMMAND: return "GAME_CONSOLE_COMMAND";
	default: return "an export the engine does not name";
	}
}

/* Alice's exports, with the argument counts measured from their own bodies:
 * each reads its arguments off the stack, and the highest slot it touches
 * bounds the count. Init reads two, Shutdown and ConsoleCommand none. */
typedef void ( QDECL *AliceInit_t )( int levelTime, int randomSeed );
typedef void ( QDECL *AliceShutdown_t )();
typedef void ( QDECL *AliceRunFrame_t )( int levelTime, int frameTime );

/* Alice spawns a level separately from starting the game, and Quake III has no
 * ordinal for that step, so nothing asks for it. It has to happen anyway: a
 * frame run against an unspawned level faults inside the module on a null
 * world, at the first `mov ecx, [eax+0x20]` off a global the spawn fills.
 *
 * The argument order is the retail engine's, read at its own call site:
 *
 *     mov  eax, [0xac0e08]      ; the export table
 *     push edi                  ; third: zero, and the level time it clears
 *     lea  esi, [eax + 0x10]    ; slot 4
 *     call 0x430dc0             ; returns the engine's entity text copy
 *     mov  ecx, [0xb59b58]      ; its server state
 *     push eax                  ; second: the entity text
 *     mov  edx, [ecx + 4]
 *     push edx                  ; first: a short name
 *     call [esi]
 *     add  esp, 0xc             ; three arguments
 *
 * The first argument is a name, not the text: the module hands it straight to
 * its dialog loader, which builds `dialog/<name>.TLK` in a 128-byte stack
 * buffer. Passing the entity text there overruns that buffer and the module
 * returns into its own data -- which is what happened when the two were tried
 * the other way round, with `"spawnflags"` landing in the return slot.
 *
 * The text itself is the second argument, and the retail engine hands over a
 * copy rather than its own lump: the routine behind `0x430dc0` allocates a
 * buffer, records its length beside it and `rep movsd`s the lump in. This does
 * the same, because the module parses that text in place. */
typedef void ( QDECL *AliceSpawnEntities_t )( const char *name,
	const char *entities, int zero );

/* The entity text handed to SpawnEntities, as a copy this file owns. */
char *aliceEntityText;

const char *SV_AliceEntityText() {
	// The level's text: a .ent override's when the server loaded one.
	const char *source = SV_EntityText();
	std::size_t length;

	if ( aliceEntityText ) {
		Z_Free( aliceEntityText );
		aliceEntityText = nullptr;
	}
	if ( !source ) {
		return "";
	}
	length = strlen( source );
	aliceEntityText = static_cast<char *>(
		Z_TagMalloc( (int)length + 1, TAG_GENERAL ) );
	memcpy( aliceEntityText, source, length );
	aliceEntityText[length] = '\0';
	return aliceEntityText;
}

/* Quake III passes a frame one number, the level time. Alice's RunFrame reads
 * two, and what it does with them says which is which: it stores the first as
 * an integer and then converts both to float and scales each by the same
 * constant, 0.001, which is milliseconds to seconds. So the first is the level
 * time in milliseconds and the second is the frame's own length, also in
 * milliseconds -- a delta, not a second clock.
 *
 * The engine does not hand that delta to the game, so it is recovered here
 * from the level time it does hand over. Keeping the previous value in this
 * file rather than changing the call site keeps the arithmetic beside the
 * module that needs it. */
int aliceLastLevelTime;

/* Quake III hands a frame one number, the level time. Alice's RunFrame reads
 * two, and what it does with them says which is which: it stores the first as
 * an integer and then converts both to float and scales each by 0.001, which
 * is milliseconds to seconds. So the first is the level time in milliseconds
 * and the second is the frame's own length, also in milliseconds.
 *
 * The engine does not hand that length to the game, so it is recovered here
 * from the level time it does hand over. The settle frames and a level read
 * back from a save set the previous time themselves, so the first frame after
 * either is measured from where the level clock actually stood. */
int SV_AliceFrameTime( int levelTime ) {
	int frameTime = levelTime - aliceLastLevelTime;

	aliceLastLevelTime = levelTime;
	/* A level time that went backwards or jumped is a restart rather than a
	 * frame, and a negative or absurd delta would reach the module as a
	 * negative elapsed time. One frame at 50ms is the honest stand-in. */
	if ( frameTime < 0 || frameTime > 1000 ) {
		frameTime = 50;
	}
	return frameTime;
}
typedef int ( QDECL *AliceConsoleCommand_t )();

/* The client exports, at the shapes the retail engine's own call sites give
 * them. Only `ClientConnect` takes a client number; every other one takes the
 * client's entity, which is what retail keeps in its client record at
 * +0x20430 and pushes:
 *
 *   ClientConnect          ( clientNum, firstTime )   0x00452822 pushes 1;
 *                                                     0x004553f1 and 0x00455523
 *                                                     push a register and 0
 *   ClientBegin            ( entity, command )        0x00452c8b
 *   ClientUserinfoChanged  ( entity, userinfo )       0x00452e5d
 *   ClientDisconnect       ( entity )                 0x00452a46
 *   ClientCommand          ( entity )                 0x00452f1e
 *   ClientThink            ( entity, command )        0x00452f80
 *
 * What `ClientConnect`'s first argument is, is measured rather than assumed:
 * it multiplies it by 732 -- the entity record's own size -- to index a base
 * pointer, and hands it straight to `getUserinfo`. It returns a value the
 * engine tests, which in every engine of this lineage is a rejection message
 * or nothing. `ClientBegin`'s first argument is dereferenced at +0x204, +0x280
 * and +0 by its own body, which a number could never survive.
 *
 * `ClientUserinfoChanged` is handed the engine's own copy of the client's
 * userinfo (client +4 in retail), after the engine has stored it. */
typedef const char *( QDECL *AliceClientConnect_t )( int clientNum, int firstTime );
typedef void ( QDECL *AliceClientBegin_t )( void *entity,
	fntech3_alice_usercmd_t *command );
typedef void ( QDECL *AliceClientUserinfoChanged_t )( void *entity,
	const char *userinfo );
typedef void ( QDECL *AliceClientEntity_t )( void *entity );
typedef void ( QDECL *AliceClientThink_t )( void *entity,
	fntech3_alice_usercmd_t *command );

/* After each client export retail tests the module's `error_message`, export
 * slot 25, and if the module left one it clears the slot and drops with that
 * text (0x00452f83..0x00452f9d after ClientThink, and the same few lines after
 * ClientConnect, ClientBegin, ClientUserinfoChanged, ClientCommand and
 * ClientDisconnect). The slot is the module's own storage, which retail
 * writes, so it is cleared here the same way. The text is copied first,
 * because dropping unloads the module that owns it. */
void SV_AliceCheckErrorMessage() {
	if ( !aliceGameExports ) {
		return;
	}
	std::intptr_t &slot = const_cast<std::intptr_t *>( aliceGameExports )
		[FNTECH3_ALICE_EXPORT_error_message];
	const char *message = reinterpret_cast<const char *>( slot );
	if ( !message ) {
		return;
	}
	char text[MAX_STRING_CHARS];

	slot = 0;
	Q_strncpyz( text, message, sizeof( text ) );
	Com_Error( ERR_DROP, "%s", text );
}

/* The entity a client export is handed: the module's own record for the
 * client, as retail's SV_GentityNum gives it. */
void *SV_AliceClientEntity( int clientNum, const char *call ) {
	void *entity = nullptr;

	if ( clientNum >= 0 && clientNum < sv.maxclients ) {
		entity = SV_AliceEntityByNumber( clientNum );
	}
	if ( !entity ) {
		Com_Error( ERR_DROP, "%s %s was asked for client %i, which has no "
			"entity in the module's array", SV_RitualTitle(), call, clientNum );
	}
	return entity;
}

/* The command a client export is handed. Retail passes a pointer to the
 * command it just received, and the module rewrites that command's time in
 * place when it clamps it to the level (0x100fea1c..0x100fea3c), so the module
 * is given a copy of its own: one per client, filled from the command the
 * engine keeps as the client's last, so a rewrite never reaches that. */
fntech3_alice_usercmd_t aliceClientCommands[MAX_CLIENTS];

fntech3_alice_usercmd_t *SV_AliceClientCommand( int clientNum ) {
	fntech3_alice_usercmd_t &command = aliceClientCommands[clientNum];

	fntech3::alice::state::FromInternal(
		SV_ClientForIndex( clientNum ).lastUsercmd, command );
	return &command;
}

/* A console command the module registered. Its `ConsoleCommand` export takes
 * no arguments and reads the command through argc and argv, so there is
 * nothing to pass: the engine has already parsed it. */
void SV_AliceConsoleCommandHandler() {
	if ( !aliceGameExports ) {
		return;
	}
	const auto consoleCommand = reinterpret_cast<AliceConsoleCommand_t>(
		aliceGameExports[FNTECH3_ALICE_EXPORT_ConsoleCommand] );
	if ( consoleCommand && !consoleCommand() ) {
		Com_Printf( "%s game module did not handle \"%s\"\n", SV_RitualTitle(), Cmd_Argv( 0 ) );
	}
}

/* ---- the level's exports ------------------------------------------------- */

/* Exports 3 and 16 to 20, at the shapes the retail engine's call sites push:
 *
 *   Cleanup            ()                   0x004551cf, no arguments
 *   WritePersistant    ( file )             0x00451787, result unused
 *   ReadPersistant     ( file ) -> bool     0x0045177a
 *   WriteLevel         ( file, autosave )   0x00451843, pushes 0 always
 *   ReadLevel          ( file ) -> bool     0x004517d4
 *   LevelArchiveValid  ( file ) -> bool     0x00451c6c
 *
 * Each file is an archive path the engine built ("save/<config>_<name>.sav"
 * or ".spv"); the module opens it through the file imports it already has.
 * A qboolean is four bytes on both sides. */
typedef void ( QDECL *AliceCleanup_t )();
typedef void ( QDECL *AliceWritePersistant_t )( const char *file );
typedef qboolean ( QDECL *AliceReadPersistant_t )( const char *file );
typedef void ( QDECL *AliceWriteLevel_t )( const char *file, qboolean autosave );
typedef qboolean ( QDECL *AliceReadLevel_t )( const char *file );
typedef qboolean ( QDECL *AliceLevelArchiveValid_t )( const char *file );

template<typename T>
T SV_AliceExport( int slot, const char *name ) {
	const T call = aliceGameExports ? reinterpret_cast<T>( aliceGameExports[slot] ) : nullptr;
	if ( !call ) {
		Com_Error( ERR_DROP, "%s game module is not loaded to answer %s", SV_RitualTitle(), name );
	}
	return call;
}

void SV_AliceCallCleanup() {
	SV_AliceExport<AliceCleanup_t>( FNTECH3_ALICE_EXPORT_Cleanup, "Cleanup" )();
}

/* Export 4, at the retail engine's own push order (0x004552c8..0x004552e7):
 * the `mapname` cvar -- the whole `map` argument, `$` target and all -- then
 * a copy of the entity text, then zero. Retail drops with the module's
 * `error_message` straight afterwards (0x004552f2). */
void SV_AliceCallSpawnEntities() {
	const auto spawnEntities = SV_AliceExport<AliceSpawnEntities_t>(
		FNTECH3_ALICE_EXPORT_SpawnEntities, "SpawnEntities" );
	const char *name = sv_mapname ? sv_mapname->string : "";
	const char *entities = SV_AliceEntityText();
	Com_DPrintf( "%s SpawnEntities: \"%s\", %i bytes of entity text\n", SV_RitualName(),
		name, (int)strlen( entities ) );
	spawnEntities( name, entities, 0 );
	SV_AliceCheckErrorMessage();
}

/* Retail checks nothing after these two, so neither does this: an error the
 * module leaves surfaces at its next checked call. */
void SV_AliceCallWritePersistant( const char *file ) {
	SV_AliceExport<AliceWritePersistant_t>( FNTECH3_ALICE_EXPORT_WritePersistant,
		"WritePersistant" )( file );
}

void SV_AliceCallWriteLevel( const char *file ) {
	SV_AliceExport<AliceWriteLevel_t>( FNTECH3_ALICE_EXPORT_WriteLevel, "WriteLevel" )( file, qfalse );
}

qboolean SV_AliceCallReadPersistant( const char *file ) {
	return SV_AliceExport<AliceReadPersistant_t>( FNTECH3_ALICE_EXPORT_ReadPersistant,
		"ReadPersistant" )( file ) ? qtrue : qfalse;
}

qboolean SV_AliceCallReadLevel( const char *file ) {
	return SV_AliceExport<AliceReadLevel_t>( FNTECH3_ALICE_EXPORT_ReadLevel,
		"ReadLevel" )( file ) ? qtrue : qfalse;
}

qboolean SV_AliceCallLevelArchiveValid( const char *file ) {
	return SV_AliceExport<AliceLevelArchiveValid_t>( FNTECH3_ALICE_EXPORT_LevelArchiveValid,
		"LevelArchiveValid" )( file ) ? qtrue : qfalse;
}

/* One of the three frames a freshly spawned level settles with: RunFrame at
 * the given level time with a frame of 100 ms, then the module's
 * `error_message` (0x00455316..0x00455361). The next frame's length is
 * measured from the end of this one. */
void SV_AliceSettleFrame( int levelTime ) {
	const auto runFrame = SV_AliceExport<AliceRunFrame_t>( FNTECH3_ALICE_EXPORT_RunFrame, "RunFrame" );
	runFrame( levelTime, 100 );
	aliceLastLevelTime = levelTime + 100;
	SV_AliceCheckErrorMessage();
}

/*
==================
SV_AliceGameEntry

Dispatch the engine's Quake III ordinals onto Alice's own exports. Only the
ones whose argument counts are measured are dispatched; the rest refuse by
name, because calling an export with the wrong argument count corrupts the
module's stack rather than failing cleanly.
==================
*/
intptr_t QDECL SV_AliceGameEntry( int command, ... ) {
	va_list argptr;

	/* Shutting down has to be answerable at any time, including twice and
	 * including after the table is gone: the engine calls it while handling an
	 * error, and erroring here would fire again inside that handling. */
	if ( command == GAME_SHUTDOWN && !aliceGameExports ) {
		return 0;
	}
	if ( !aliceGameExports ) {
		Com_Error( ERR_DROP, "%s game module was called before it loaded", SV_RitualTitle() );
		return 0;
	}

	switch ( command ) {
	case GAME_INIT: {
		va_start( argptr, command );
		const int levelTime = va_arg( argptr, int );
		const int randomSeed = va_arg( argptr, int );
		va_end( argptr );

		const auto init = reinterpret_cast<AliceInit_t>(
			aliceGameExports[FNTECH3_ALICE_EXPORT_Init] );
		Com_DPrintf( "%s GAME_INIT: levelTime %i, randomSeed %i\n", SV_RitualName(),
			levelTime, randomSeed );
		aliceLastLevelTime = levelTime;
		Com_Memset( aliceLinked, 0, sizeof( aliceLinked ) );
		init( levelTime, randomSeed );

		/* Init alone, as retail's SV_InitGameProgs (0x004544c0) calls it. The
		 * level is not spawned here: SV_SpawnServer asks SV_AliceSpawnLevel
		 * next, which runs Cleanup and then either reads the level back from a
		 * save or spawns the map's entities (server_alice_save.hpp). */
		return 0;
	}

	case GAME_SHUTDOWN: {
		/* Shutting down takes no arguments and is safe to answer even if the
		 * module never started, because nothing was begun. */
		const auto shutdown = reinterpret_cast<AliceShutdown_t>(
			aliceGameExports[FNTECH3_ALICE_EXPORT_Shutdown] );
		if ( shutdown ) {
			shutdown();
		}
		/* The commands the module registered keep their engine-side handler,
		 * which does nothing once this is cleared. */
		aliceGameExports = nullptr;
		return 0;
	}

	case GAME_RUN_FRAME: {
		/* Two arguments, a level time and a frame length, both in
		 * milliseconds: the module scales each by 0.001. The engine supplies
		 * only the first, so the second is recovered from it. */
		va_start( argptr, command );
		const int levelTime = va_arg( argptr, int );
		va_end( argptr );

		const auto runFrame = reinterpret_cast<AliceRunFrame_t>(
			aliceGameExports[FNTECH3_ALICE_EXPORT_RunFrame] );
		runFrame( levelTime, SV_AliceFrameTime( levelTime ) );
		return 0;
	}

	case GAME_CLIENT_CONNECT: {
		va_start( argptr, command );
		const int clientNum = va_arg( argptr, int );
		const int firstTime = va_arg( argptr, int );
		va_end( argptr );

		const auto connect = reinterpret_cast<AliceClientConnect_t>(
			aliceGameExports[FNTECH3_ALICE_EXPORT_ClientConnect] );
		const char *refusal = connect( clientNum, firstTime );
		SV_AliceCheckErrorMessage();

		Com_DPrintf( "%s ClientConnect: client %i, firstTime %i -> %s\n", SV_RitualName(),
			clientNum, firstTime, refusal ? refusal : "accepted" );
		return reinterpret_cast<intptr_t>( refusal );
	}

	case GAME_CLIENT_BEGIN: {
		/* The engine has stored the client's first command as its last
		 * before asking, which is the command retail hands ClientBegin. */
		va_start( argptr, command );
		const int clientNum = va_arg( argptr, int );
		va_end( argptr );

		void *entity = SV_AliceClientEntity( clientNum, "ClientBegin" );
		const auto begin = reinterpret_cast<AliceClientBegin_t>(
			aliceGameExports[FNTECH3_ALICE_EXPORT_ClientBegin] );
		Com_DPrintf( "%s ClientBegin: client %i\n", SV_RitualName(), clientNum );
		begin( entity, SV_AliceClientCommand( clientNum ) );
		SV_AliceCheckErrorMessage();
		return 0;
	}

	case GAME_CLIENT_USERINFO_CHANGED: {
		va_start( argptr, command );
		const int clientNum = va_arg( argptr, int );
		va_end( argptr );

		void *entity = SV_AliceClientEntity( clientNum, "ClientUserinfoChanged" );
		const auto changed = reinterpret_cast<AliceClientUserinfoChanged_t>(
			aliceGameExports[FNTECH3_ALICE_EXPORT_ClientUserinfoChanged] );
		changed( entity, SV_ClientForIndex( clientNum ).userinfo );
		SV_AliceCheckErrorMessage();
		return 0;
	}

	case GAME_CLIENT_DISCONNECT: {
		/* Retail pushes the client's entity alone (0x00452a39..0x00452a46).
		 * A drop can come from inside error handling, so a client with no
		 * entity is skipped here rather than raising a second error. */
		va_start( argptr, command );
		const int clientNum = va_arg( argptr, int );
		va_end( argptr );

		void *entity = clientNum >= 0 && clientNum < sv.maxclients
			? SV_AliceEntityByNumber( clientNum ) : nullptr;
		if ( !entity ) {
			Com_DPrintf( "%s ClientDisconnect: client %i has no entity\n", SV_RitualName(),
				clientNum );
			return 0;
		}
		const auto disconnect = reinterpret_cast<AliceClientEntity_t>(
			aliceGameExports[FNTECH3_ALICE_EXPORT_ClientDisconnect] );
		disconnect( entity );
		SV_AliceCheckErrorMessage();
		return 0;
	}

	case GAME_CLIENT_COMMAND: {
		/* A command the engine did not claim; the module reads it through
		 * argc and argv, which the engine has already parsed. */
		va_start( argptr, command );
		const int clientNum = va_arg( argptr, int );
		va_end( argptr );

		void *entity = SV_AliceClientEntity( clientNum, "ClientCommand" );
		const auto clientCommand = reinterpret_cast<AliceClientEntity_t>(
			aliceGameExports[FNTECH3_ALICE_EXPORT_ClientCommand] );
		clientCommand( entity );
		SV_AliceCheckErrorMessage();
		return 0;
	}

	case GAME_CLIENT_THINK: {
		/* The engine calls this only for an active client, having stored the
		 * command as the client's last (SV_ClientThink, as retail does at
		 * 0x00452f5a before its own CS_ACTIVE test). */
		va_start( argptr, command );
		const int clientNum = va_arg( argptr, int );
		va_end( argptr );

		void *entity = SV_AliceClientEntity( clientNum, "ClientThink" );
		const auto think = reinterpret_cast<AliceClientThink_t>(
			aliceGameExports[FNTECH3_ALICE_EXPORT_ClientThink] );
		think( entity, SV_AliceClientCommand( clientNum ) );
		SV_AliceCheckErrorMessage();
		return 0;
	}

	case GAME_CONSOLE_COMMAND: {
		/* Measured at no arguments: the export reads the command through argc
		 * and argv, which are served, and returns whether it handled it. The
		 * engine asks this for a command nothing else claimed. */
		const auto consoleCommand = reinterpret_cast<AliceConsoleCommand_t>(
			aliceGameExports[FNTECH3_ALICE_EXPORT_ConsoleCommand] );
		return consoleCommand ? consoleCommand() : 0;
	}

	default:
		Com_Error( ERR_DROP,
			"%s game module is loaded, but fnTech3 dispatches only "
			"GAME_INIT, GAME_SHUTDOWN, GAME_RUN_FRAME, GAME_CONSOLE_COMMAND "
			"and the six client exports: the engine asked "
			"for %s (%i). "
			"Calling an export whose argument count is not measured would "
			"corrupt the module's stack rather than fail here.", SV_RitualTitle(),
			SV_AliceEngineCallName( command ), command );
		return 0;
	}
}

/*
==================
SV_AliceLoadGame

Load the retail game library and take the table it publishes, checking it
against the frozen shape rather than trusting it.
==================
*/
vm_t *SV_AliceLoadGame() {
	const fntech3::ritual::ServerLayout *layout =
		fntech3::ritual::ServerLayoutFor( Com_GameProfile() ? Com_GameProfile()->id : COM_GAME_AUTO );
	if ( !layout ) {
		Com_Error( ERR_DROP, "the Ritual game adapter was asked to load outside Alice and F.A.K.K.2" );
		return nullptr;
	}
	ritualLayout = layout;

	void *symbol = nullptr;
	vm_t *vm = VM_CreateTable( VM_GAME, "GetGameAPI", SV_AliceGameEntry, &symbol );
	if ( !vm ) {
		/* A copy of F.A.K.K.2's disc lays pak0.pk3 out loose but keeps both
		 * libraries inside its InstallShield cabinets, which data1.hdr
		 * describes; only an installation has fakk/gamex86.dll. */
		const char *base = Cvar_VariableString( "fs_basepath" );
		fileOffset_t size;
		fileTime_t modified, created;
		if ( SV_RitualIsFAKK2() && base[0] &&
			Sys_GetFileStats( va( "%s%cdata1.hdr", base, PATH_SEP ), &size, &modified, &created ) ) {
			Com_Error( ERR_DROP, "%s game module fakk/gamex86.dll is missing: %s is a copy of the "
				"retail disc, which keeps its libraries inside the data*.cab cabinets. Install the "
				"game with its own setup and point fs_basepath at the installed folder.",
				SV_RitualTitle(), base );
		}
		return nullptr;
	}

	typedef void *( QDECL *AliceGetGameAPI_t )( void *import );
	auto getGameAPI = reinterpret_cast<AliceGetGameAPI_t>( symbol );
	SV_AliceFillImports();

	/* F.A.K.K.2's module takes its 105 slots from the same services: every
	 * slot is the member Alice's table keeps at the mapped position, except the
	 * two tag queries, whose argument lists are F.A.K.K.2's own. */
	void *importTable = &aliceImport;
	if ( SV_RitualIsFAKK2() ) {
		for ( int slot = 0; slot < FNTECH3_FAKK2_GAME_IMPORT_SLOTS; ++slot ) {
			fakk2Import[slot] = aliceImport.slot[fntech3::fakk2::module::AliceGameSlot( slot )];
		}
		fakk2Import[FNTECH3_FAKK2_IMPORT_Tag_Orientation] =
			reinterpret_cast<std::intptr_t>( &SV_FAKK2TagOrientation );
		fakk2Import[FNTECH3_FAKK2_IMPORT_Tag_OrientationEx] =
			reinterpret_cast<std::intptr_t>( &SV_FAKK2TagOrientationEx );
		fakk2Import[FNTECH3_FAKK2_IMPORT_InitCommands] =
			reinterpret_cast<std::intptr_t>( &SV_FAKK2InitCommands );
		fakk2Import[FNTECH3_FAKK2_IMPORT_Frame_Commands] =
			reinterpret_cast<std::intptr_t>( &SV_FAKK2FrameCommands );
		importTable = fakk2Import;
	}

	const std::intptr_t *game =
		static_cast<const std::intptr_t *>( getGameAPI( importTable ) );
	if ( !game ) {
		VM_Free( vm );
		Com_Error( ERR_DROP, "%s game module published no export table", SV_RitualTitle() );
		return nullptr;
	}

	/* Both titles' exports share one shape, so the same slots are read; the
	 * version and the record size are each title's own. An Alice module under
	 * the F.A.K.K.2 profile, or the reverse, stops here by its record size. */
	const std::intptr_t version = game[FNTECH3_ALICE_EXPORT_apiversion];
	if ( version != SV_RitualLayout().gameApiVersion ) {
		VM_Free( vm );
		Com_Error( ERR_DROP, "%s game module is API version %i, not %i", SV_RitualTitle(),
			(int)version, SV_RitualLayout().gameApiVersion );
		return nullptr;
	}
	const std::intptr_t entitySize = game[FNTECH3_ALICE_EXPORT_gentitySize];
	if ( entitySize != SV_RitualLayout().gentitySize ) {
		/* The record size is what every entity marshal would be sized by, so a
		 * module that disagrees is a build this tree has not measured. */
		VM_Free( vm );
		Com_Error( ERR_DROP, "%s game module has a %i-byte entity record, not %i", SV_RitualTitle(),
			(int)entitySize, SV_RitualLayout().gentitySize );
		return nullptr;
	}

	/* The slots the frozen map says this build never writes must still be
	 * null, because an adapter that called one would jump to zero. */
	for ( const auto &row : kGameExports ) {
		if ( row.evidence == ExportEvidence::ABSENT && game[row.slot] ) {
			VM_Free( vm );
			Com_Error( ERR_DROP, "%s game module publishes %s, which this "
				"build is not measured to provide", SV_RitualTitle(), row.name );
			return nullptr;
		}
	}

	aliceGameExports = game;
	Com_Printf( "%s game module loaded: API version %i, %i-byte entity record, "
		"table at %p\n", SV_RitualName(), (int)version, (int)entitySize, (const void *)game );
	if ( SV_RitualIsFAKK2() ) {
		/* Alice's null and her five appends are not in this table, so every
		 * slot it has is one fnTech3 serves or one that drops by name. */
		int served = 0;
		for ( int slot = 0; slot < FNTECH3_FAKK2_GAME_IMPORT_SLOTS; ++slot ) {
			served += fakk2Import[slot]
				!= aliceUnserved.slot[fntech3::fakk2::module::AliceGameSlot( slot )];
		}
		Com_Printf( "  %i of %i imports are served; the rest drop by name when called.\n",
			served, FNTECH3_FAKK2_GAME_IMPORT_SLOTS );
	} else {
		Com_Printf( "  %i of %i imports are served; the rest drop by name when called.\n",
			SV_AliceServedImports(), FNTECH3_ALICE_GAME_IMPORT_SLOTS );
	}
	return vm;
}

} // namespace

void SV_AliceRegisterInlineModels( void ) {
	SV_AliceNameInlineModels();
}

/* Retail's SV_GentityNum (0x00453390) is the arena base plus the number times
 * the record size, unchecked; this one refuses a number past the arena the
 * module published. */
unsigned char *SV_AliceGameEntity( int number ) {
	fntech3::server::GameEntityRef reference;
	if ( !Com_GameIsRitual() || !serverGameData.EntityRefAt( number, reference ) ) {
		return nullptr;
	}
	return static_cast<unsigned char *>( serverGameData.EntityPointer( reference ) );
}

/* The savegames and the level flow built on the exports above. */
#include "server_alice_save.hpp"

#endif
