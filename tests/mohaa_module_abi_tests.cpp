/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 *
 * Medal of Honor: Allied Assault 1.00's module tables and boundary records.
 * The tables must be dense, in slot order and of the measured lengths, and
 * the records must keep the offsets the retail modules read. */

#include "../code/compat/mohaa_game_abi.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

static int failures = 0;
#define CHECK( condition ) do { if ( !( condition ) ) { \
	std::fprintf( stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition ); \
	++failures; } } while ( 0 )

namespace {

struct Row {
	int slot;
	const char *name;
	const char *evidence;
};

enum Kind { FUNCTION, NULL_FUNCTION, DATA, NULL_DATA };
enum Evidence { LITERAL, ARITY, ORDER, VALUE, BEHAVIOUR, UNREFERENCED };

const Row gameImports[] = {
#define FNTECH3_MOHAA_GAME_IMPORT( slot, name, evidence, sites ) { slot, #name, #evidence },
#include "../code/compat/mohaa_game_imports.def"
#undef FNTECH3_MOHAA_GAME_IMPORT
};

struct ExportRow {
	int slot;
	const char *name;
	Kind kind;
};

const ExportRow gameExports[] = {
#define FNTECH3_MOHAA_GAME_EXPORT( slot, name, kind, evidence ) { slot, #name, kind },
#include "../code/compat/mohaa_game_exports.def"
#undef FNTECH3_MOHAA_GAME_EXPORT
};

const Row cgameImports[] = {
#define VARARGS -2
#define FNTECH3_MOHAA_CGAME_IMPORT( slot, name, args, evidence, sites ) { slot, #name, #evidence },
#include "../code/compat/mohaa_cgame_imports.def"
#undef FNTECH3_MOHAA_CGAME_IMPORT
#undef VARARGS
};

const ExportRow cgameExports[] = {
#define FNTECH3_MOHAA_CGAME_EXPORT( slot, name, kind, evidence ) { slot, #name, kind },
#include "../code/compat/mohaa_cgame_exports.def"
#undef FNTECH3_MOHAA_CGAME_EXPORT
};

template <typename T, std::size_t N>
void CheckDense( const T ( &rows )[N], std::size_t expected, const char *table )
{
	CHECK( N == expected );
	std::set<std::string> names;
	for ( std::size_t i = 0; i < N; ++i ) {
		if ( rows[i].slot != static_cast<int>( i ) ) {
			std::fprintf( stderr, "%s: row %zu carries slot %d\n", table, i, rows[i].slot );
			++failures;
		}
		CHECK( names.insert( rows[i].name ).second );
	}
}

int Slot( const Row *rows, std::size_t count, const char *name )
{
	for ( std::size_t i = 0; i < count; ++i ) {
		if ( !std::strcmp( rows[i].name, name ) ) return rows[i].slot;
	}
	return -1;
}

}

int main()
{
	using namespace fntech3::mohaa;

	CheckDense( gameImports, FNTECH3_MOHAA_GAME_IMPORT_SLOTS, "game imports" );
	CheckDense( gameExports, FNTECH3_MOHAA_GAME_EXPORT_BYTES / 4, "game exports" );
	CheckDense( cgameImports, FNTECH3_MOHAA_CGAME_IMPORT_SLOTS, "cgame imports" );
	CheckDense( cgameExports, FNTECH3_MOHAA_CGAME_EXPORT_SLOTS, "cgame exports" );

	constexpr std::size_t gameImportCount = sizeof( gameImports ) / sizeof( gameImports[0] );
	constexpr std::size_t cgameImportCount = sizeof( cgameImports ) / sizeof( cgameImports[0] );

	/* Slots the retail call sites pin by a literal, and the places the five
	 * members 1.00 lacks would otherwise have pushed them. */
	CHECK( Slot( gameImports, gameImportCount, "Argc" ) == 13 );
	CHECK( Slot( gameImports, gameImportCount, "FS_Write" ) == 23 );
	CHECK( Slot( gameImports, gameImportCount, "setConfigstring" ) == 56 );
	CHECK( Slot( gameImports, gameImportCount, "getConfigstring" ) == 57 );
	CHECK( Slot( gameImports, gameImportCount, "GetUserinfo" ) == 59 );
	CHECK( Slot( gameImports, gameImportCount, "trace" ) == 64 );
	CHECK( Slot( gameImports, gameImportCount, "NumHeadModels" ) == 104 );
	CHECK( Slot( gameImports, gameImportCount, "Frame_Commands" ) == 108 );
	CHECK( Slot( gameImports, gameImportCount, "Tag_NumForName" ) == 111 );
	CHECK( Slot( gameImports, gameImportCount, "LocateGameData" ) == 141 );
	CHECK( Slot( gameImports, gameImportCount, "HudDrawFont" ) == 165 );
	CHECK( Slot( gameImports, gameImportCount, "fsDebug" ) == 166 );
	for ( const char *absent : { "Cvar_CheckRange", "FS_PrepFileWrite", "Anim_HasCommands_Client",
		"Frame_Commands_Client", "SanitizeName" } ) {
		CHECK( Slot( gameImports, gameImportCount, absent ) == -1 );
	}

	CHECK( !std::strcmp( gameExports[0].name, "apiversion" ) && gameExports[0].kind == DATA );
	CHECK( gameExports[15].kind == NULL_FUNCTION && gameExports[16].kind == NULL_FUNCTION
		&& gameExports[17].kind == NULL_FUNCTION );
	CHECK( !std::strcmp( gameExports[37].name, "gentitySize" ) );

	CHECK( Slot( cgameImports, cgameImportCount, "Cvar_Set" ) == 10 );
	CHECK( Slot( cgameImports, cgameImportCount, "MSG_ReadBits" ) == 23 );
	CHECK( Slot( cgameImports, cgameImportCount, "R_UnregisterServerModel" ) == 84 );
	CHECK( Slot( cgameImports, cgameImportCount, "R_RegisterShader" ) == 86 );
	CHECK( Slot( cgameImports, cgameImportCount, "R_AddRefEntityToScene" ) == 88 );
	CHECK( Slot( cgameImports, cgameImportCount, "MUSIC_UpdateMood" ) == 72 );
	CHECK( Slot( cgameImports, cgameImportCount, "UI_HideMenu" ) == 129 );
	CHECK( Slot( cgameImports, cgameImportCount, "R_Model_GetHandle" ) == 130 );
	CHECK( Slot( cgameImports, cgameImportCount, "UI_SetScoreBoardItem" ) == 157 );
	CHECK( Slot( cgameImports, cgameImportCount, "HudDrawElements" ) == 164 );
	CHECK( Slot( cgameImports, cgameImportCount, "forceFeedback" ) == 167 );
	CHECK( cgameExports[28].kind == NULL_DATA );
	CHECK( !std::strcmp( cgameExports[29].name, "Command_ProcessFile" ) );

	/* The records, beyond the header's own static_asserts. */
	static_assert( sizeof( EntityState ) * FNTECH3_MOHAA_MAX_SNAPSHOT_ENTITIES
		+ sizeof( ServerSound ) * FNTECH3_MOHAA_MAX_SERVER_SOUNDS + 0x2c + 660 + 16 == sizeof( Snapshot ) );
	static_assert( FNTECH3_MOHAA_CS_OBJECTIVES == FNTECH3_MOHAA_CS_MODELS + FNTECH3_MOHAA_MAX_MODELS );
	static_assert( FNTECH3_MOHAA_CS_SOUNDS == FNTECH3_MOHAA_CS_OBJECTIVES + FNTECH3_MOHAA_MAX_OBJECTIVES );
	static_assert( FNTECH3_MOHAA_CS_IMAGES == FNTECH3_MOHAA_CS_SOUNDS + FNTECH3_MOHAA_MAX_SOUNDS );
	static_assert( FNTECH3_MOHAA_CS_LIGHTSTYLES == FNTECH3_MOHAA_CS_IMAGES + FNTECH3_MOHAA_MAX_IMAGES );
	static_assert( FNTECH3_MOHAA_CS_PLAYERS == FNTECH3_MOHAA_CS_LIGHTSTYLES + FNTECH3_MOHAA_MAX_LIGHTSTYLES );
	static_assert( FNTECH3_MOHAA_CS_PLAYERS + FNTECH3_MOHAA_MAX_CLIENTS <= FNTECH3_MOHAA_MAX_CONFIGSTRINGS );

	if ( failures ) {
		std::fprintf( stderr, "%d failure(s)\n", failures );
		return 1;
	}
	std::puts( "mohaa module ABI tests passed" );
	return 0;
}
