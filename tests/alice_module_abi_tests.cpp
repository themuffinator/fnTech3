/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Alice's module-table shapes, frozen against the measurements in
 * docs/fntech3/2026-09-22-alice-module-abi.md. No retail module is loaded or
 * called here; only the shape the engine would have to present is asserted. */

#include "../code/compat/alice_module_abi.h"
#include "../code/qcommon/game_manager.h"
#include "../code/qcommon/game_profile.h"

#include <cstring>
#include <iostream>
#include <set>
#include <string>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::module;

/* The measured lengths. A change to any of these is a change to what the
 * retail libraries were observed to do, not a refactor. */
static_assert( FNTECH3_ALICE_GAME_IMPORT_SLOTS == 111, "GetGameAPI copies 111 dwords" );
static_assert( FNTECH3_ALICE_GAME_EXPORT_SLOTS == 26, "the game writes 26 export slots" );
static_assert( FNTECH3_ALICE_CGAME_IMPORT_SLOTS == 125, "the cgame's init copies 125 dwords" );
static_assert( FNTECH3_ALICE_CGAME_EXPORT_SLOTS == 7, "the cgame writes 7 export slots" );
static_assert( FNTECH3_ALICE_GAME_API_VERSION == 4, "game export slot 0" );
static_assert( FNTECH3_ALICE_GENTITY_SIZE == 732, "game export slot 22" );
/* The client record size, reported by the module while it runs rather than
 * written anywhere in its bytes. It is recorded here because it is measured,
 * and it has to stay a plain size: no member of that record is established. */
static_assert( FNTECH3_ALICE_GCLIENT_SIZE == 2112, "reported by LocateGameData" );
static_assert( FNTECH3_ALICE_GCLIENT_SIZE != FNTECH3_ALICE_GENTITY_SIZE,
	"the two records are different sizes and must not be conflated" );

/* Retail is Win32 x86, so each slot is four bytes there whatever this host is. */
static_assert( GameImportBytes() == 444, "111 four-byte slots" );
static_assert( GameExportBytes() == 104, "26 four-byte slots" );
static_assert( CGameImportBytes() == 500, "125 four-byte slots" );
static_assert( CGameExportBytes() == 28, "7 four-byte slots" );

/* The export table's length is corroborated by where the import copy lands:
 * the game's import buffer begins one export table above the export table. */
static_assert( GameExportBytes() == 0x101e8d10u - 0x101e8ca8u,
	"the game's export table ends where its import copy begins" );

/* The tables are flat slot arrays with no padding, because the engine has to
 * hand the module exactly the bytes it copies. */
static_assert( sizeof( GameImportTable )
	== FNTECH3_ALICE_GAME_IMPORT_SLOTS * sizeof( std::intptr_t ), "flat game imports" );
static_assert( sizeof( GameExportTable )
	== FNTECH3_ALICE_GAME_EXPORT_SLOTS * sizeof( std::intptr_t ), "flat game exports" );
static_assert( sizeof( CGameImportTable )
	== FNTECH3_ALICE_CGAME_IMPORT_SLOTS * sizeof( std::intptr_t ), "flat cgame imports" );
static_assert( sizeof( CGameExportTable )
	== FNTECH3_ALICE_CGAME_EXPORT_SLOTS * sizeof( std::intptr_t ), "flat cgame exports" );

/* The named slots lie inside their tables. */
static_assert( FNTECH3_ALICE_GAME_EXPORT_API_VERSION < FNTECH3_ALICE_GAME_EXPORT_SLOTS, "slot 0" );
static_assert( FNTECH3_ALICE_GAME_EXPORT_GENTITY_SIZE < FNTECH3_ALICE_GAME_EXPORT_SLOTS, "slot 22" );
static_assert( FNTECH3_ALICE_CGAME_EXPORT_INIT < FNTECH3_ALICE_CGAME_EXPORT_SLOTS, "slot 0" );

/* The client game's import table, as the engine's own builder at 0x00424400
 * fills it. The three slots it never writes lie inside the table, and the
 * first one the module calls is the last slot, so both have to stay in range
 * of a table this size. */
static_assert( FNTECH3_ALICE_CGAME_IMPORT_FIRST_CALLED
	< FNTECH3_ALICE_CGAME_IMPORT_SLOTS, "the first import called is a real slot" );
static_assert( FNTECH3_ALICE_CGAME_IMPORT_FIRST_CALLED
	== FNTECH3_ALICE_CGAME_IMPORT_SLOTS - 1,
	"the module calls the last slot first" );
static_assert( FNTECH3_ALICE_CGAME_IMPORT_UNFILLED_A == 0,
	"the table begins with a hole" );
static_assert( FNTECH3_ALICE_CGAME_IMPORT_UNFILLED_B
	< FNTECH3_ALICE_CGAME_IMPORT_SLOTS, "inside the table" );
static_assert( FNTECH3_ALICE_CGAME_IMPORT_UNFILLED_C
	< FNTECH3_ALICE_CGAME_IMPORT_SLOTS, "inside the table" );
/* A slot the engine never fills is one the module must never call, so the
 * one it calls first cannot be one of them. */
static_assert( FNTECH3_ALICE_CGAME_IMPORT_FIRST_CALLED
	!= FNTECH3_ALICE_CGAME_IMPORT_UNFILLED_A
	&& FNTECH3_ALICE_CGAME_IMPORT_FIRST_CALLED
	!= FNTECH3_ALICE_CGAME_IMPORT_UNFILLED_B
	&& FNTECH3_ALICE_CGAME_IMPORT_FIRST_CALLED
	!= FNTECH3_ALICE_CGAME_IMPORT_UNFILLED_C,
	"the first import called is one the engine fills" );

static void CheckEntryPoints()
{
	/* Alice's client game keeps Quake III's filename and does not keep its
	 * ABI, which is exactly the confusion these names exist to prevent. */
	CHECK( !std::strcmp( LibraryName( Entry::Game ), "fgamex86.dll" ) );
	CHECK( !std::strcmp( LibraryName( Entry::ClientGame ), "cgamex86.dll" ) );
	CHECK( !std::strcmp( EntryPointName( Entry::Game ), "GetGameAPI" ) );
	CHECK( !std::strcmp( EntryPointName( Entry::ClientGame ), "GetCGameAPI" ) );
	for ( Entry entry : { Entry::Game, Entry::ClientGame } ) {
		CHECK( std::strcmp( EntryPointName( entry ), "vmMain" ) );
		CHECK( std::strcmp( EntryPointName( entry ), "dllEntry" ) );
	}
}

static void CheckProfileAgreement()
{
	/* The catalogue has to name the same two libraries the ABI does, or the
	 * loader would open a file this header does not describe. */
	const auto *alice = Com_GameProfileById( COM_GAME_ALICE );
	CHECK( alice && alice->titleId == COM_GAME_TITLE_ALICE );
	CHECK( Com_GameProfileAllowsModuleApi( alice, COM_MODULE_API_ALICE_NATIVE ) );
	CHECK( !Com_GameProfileAllowsModuleApi( alice, COM_MODULE_API_QVM ) );
	CHECK( !Com_GameProfileAllowsModuleApi( alice, COM_MODULE_API_LEGACY_NATIVE ) );

	char name[64];
	CHECK( Com_GameManagerNativeModuleFilename( alice, "qagame", "x86", ".dll", 0,
		name, sizeof( name ) ) );
	CHECK( !std::strcmp( name, LibraryName( Entry::Game ) ) );
	CHECK( Com_GameManagerNativeModuleFilename( alice, "cgame", "x86", ".dll", 0,
		name, sizeof( name ) ) );
	CHECK( !std::strcmp( name, LibraryName( Entry::ClientGame ) ) );
	/* There is no menu module and no second candidate for either library. */
	CHECK( !Com_GameManagerNativeModuleFilename( alice, "ui", "x86", ".dll", 0,
		name, sizeof( name ) ) );
	CHECK( !Com_GameManagerNativeModuleFilename( alice, "qagame", "x86", ".dll", 1,
		name, sizeof( name ) ) );

	/* Retail shipped Win32 x86 only, and the slot widths above assume it. */
	for ( const char *architecture : { "x86_64", "i386", "arm64" } ) {
		CHECK( !Com_GameManagerNativeModuleFilename( alice, "qagame", architecture,
			".dll", 0, name, sizeof( name ) ) );
	}
	for ( const char *extension : { ".so", ".dylib" } ) {
		CHECK( !Com_GameManagerNativeModuleFilename( alice, "qagame", "x86",
			extension, 0, name, sizeof( name ) ) );
	}

	/* No other profile answers to Alice's entry points, and Alice claims no
	 * pure proof, because nothing has admitted one of its modules yet. */
	CHECK( alice->pureProof == COM_PURE_PROOF_UNAVAILABLE );
	CHECK( !Com_PureProofIsNativeModulePair( alice ) );
	CHECK( !Com_GameManagerPureModuleFilename( alice, 0, "x86", ".dll",
		name, sizeof( name ) ) );
}

/* The slots this file's own measurements pin independently. These are
 * compile-time constants, so a drift in the map is a build failure. */
static_assert( FNTECH3_ALICE_IMPORT_setConfigstring == 29, "0x530 bound" );
static_assert( FNTECH3_ALICE_IMPORT_getConfigstring == 30, "0x530 bound" );
static_assert( FNTECH3_ALICE_IMPORT_getUserinfo == 32, "SV_GetUserinfo literal" );
static_assert( FNTECH3_ALICE_IMPORT_SetBrushModel == 33, "SV_SetBrushModel literal" );
/* Slot 103's body reads five stack arguments straight into five adjacent
 * globals, which is this member and no other. */
static_assert( FNTECH3_ALICE_IMPORT_LocateGameData == 103, "five arguments" );
/* The two data members sit together, and nothing else may claim to. */
static_assert( FNTECH3_ALICE_IMPORT_DebugLines == 101, "measured data pointer" );
static_assert( FNTECH3_ALICE_IMPORT_numDebugLines
	== FNTECH3_ALICE_IMPORT_DebugLines + 1, "the pair is adjacent" );
static_assert( FNTECH3_ALICE_IMPORT_SENTINEL == FNTECH3_ALICE_GAME_IMPORT_SLOTS,
	"the map ends where the measured table does" );

/* The export slots the module writes as fixed values. */
static_assert( FNTECH3_ALICE_EXPORT_apiversion
	== FNTECH3_ALICE_GAME_EXPORT_API_VERSION, "slot 0 is the version" );
static_assert( FNTECH3_ALICE_EXPORT_gentitySize
	== FNTECH3_ALICE_GAME_EXPORT_GENTITY_SIZE, "slot 22 is the record size" );
/* The entity block the reference table declares together stays together. */
static_assert( FNTECH3_ALICE_EXPORT_gentities == 21, "written at run time" );
static_assert( FNTECH3_ALICE_EXPORT_num_entities == 23, "written at run time" );
static_assert( FNTECH3_ALICE_EXPORT_max_entities == 24, "written at run time" );
/* The level's exports the engine calls, at the table offsets its own call
 * sites read (docs/fntech3/2026-09-23-alice-savegames.md): Cleanup with no
 * arguments, then WritePersistant, ReadPersistant, ReadLevel and
 * LevelArchiveValid with a path and WriteLevel with a path and a zero. */
static_assert( FNTECH3_ALICE_EXPORT_Cleanup * 4 == 0x0c, "ge+0x0c, 0x004551cf" );
static_assert( FNTECH3_ALICE_EXPORT_WritePersistant * 4 == 0x40, "ge+0x40, 0x00451787" );
static_assert( FNTECH3_ALICE_EXPORT_ReadPersistant * 4 == 0x44, "ge+0x44, 0x0045177a" );
static_assert( FNTECH3_ALICE_EXPORT_WriteLevel * 4 == 0x48, "ge+0x48, 0x00451843" );
static_assert( FNTECH3_ALICE_EXPORT_ReadLevel * 4 == 0x4c, "ge+0x4c, 0x004517d4" );
static_assert( FNTECH3_ALICE_EXPORT_LevelArchiveValid * 4 == 0x50, "ge+0x50, 0x00451c6c" );
static_assert( FNTECH3_ALICE_EXPORT_RunFrame * 4 == 0x38, "ge+0x38, the settle frames at 0x00455331" );
static_assert( FNTECH3_ALICE_EXPORT_SpawnEntities * 4 == 0x10, "ge+0x10, 0x004552d4" );
static_assert( FNTECH3_ALICE_EXPORT_error_message * 4 == 0x64, "ge+0x64, checked at 0x004552f2" );
/* The two archive imports: the name builder the play log asks for with a
 * name and "log" (fgamex86 0x10117001, eight bytes cleaned), and the CRC its
 * script loader asks for with a buffer and a length (0x100b201a). */
static_assert( FNTECH3_ALICE_IMPORT_GetArchiveFileName == 25, "table +0x64, 0x101e8d74" );
static_assert( FNTECH3_ALICE_IMPORT_CalcCRC == 99, "table +0x18c, 0x101e8e9c" );
/* The slot the engine reads after every call and raises as an error when it
 * is not null. It lies inside the table and is one of the run-time-written
 * slots, not one of the two the module fills with an immediate. */
static_assert( FNTECH3_ALICE_GAME_EXPORT_DEFERRED_ERROR
	< FNTECH3_ALICE_GAME_EXPORT_SLOTS, "inside the export table" );
static_assert( FNTECH3_ALICE_GAME_EXPORT_DEFERRED_ERROR
	!= FNTECH3_ALICE_GAME_EXPORT_API_VERSION
	&& FNTECH3_ALICE_GAME_EXPORT_DEFERRED_ERROR
	!= FNTECH3_ALICE_GAME_EXPORT_GENTITY_SIZE,
	"not one of the immediates" );
static_assert( FNTECH3_ALICE_EXPORT_SENTINEL == FNTECH3_ALICE_GAME_EXPORT_SLOTS,
	"the export map ends where the measured table does" );

static void CheckImportSlots()
{
	/* One row per measured slot, in slot order, with no gaps. A gap would
	 * mean the map and the measurement had drifted apart. */
	int expected = 0;
	std::set<std::string> names;
	int literal = 0, behaviour = 0, arity = 0, order = 0;
	int added = 0, unnamed = 0, absent = 0;
	for ( const auto &row : kGameImports ) {
		CHECK( row.slot == expected++ );
		CHECK( row.name && row.name[0] );
		/* No two slots may carry the same name: the reference table has no
		 * duplicates, so a duplicate here is a transcription error. */
		CHECK( names.insert( row.name ).second );
		switch ( row.evidence ) {
		case Evidence::LITERAL: ++literal; break;
		case Evidence::BEHAVIOUR: ++behaviour; break;
		case Evidence::ARITY: ++arity; break;
		case Evidence::ORDER: ++order; break;
		case Evidence::ADDED: ++added; break;
		case Evidence::UNNAMED: ++unnamed; break;
		case Evidence::ABSENT: ++absent; break;
		}
	}
	CHECK( expected == FNTECH3_ALICE_GAME_IMPORT_SLOTS );

	/* One slot, and only one, is left null by the engine's own table build.
	 * Naming another as absent would hand the module a null it does call:
	 * the module calls 92 of these slots, including three that an earlier
	 * measurement of this table wrongly read as never written. */
	CHECK( absent == 1 );
	CHECK( kGameImports[100].evidence == Evidence::ABSENT );

	/* Alice's own additions: the null inserted at 100, and five appended past
	 * the reference table's end, four of which carry no name. The fifth
	 * append, slot 110, names itself in the engine's own error string and so
	 * is a literal rather than an unnamed addition. */
	CHECK( added == 4 );
	for ( int slot : { 106, 107, 108, 109 } ) {
		CHECK( kGameImports[slot].evidence == Evidence::ADDED );
	}

	/* The table decomposes as 105 reference members and six Alice slots --
	 * the inserted null and the five appended; naming one of the six does not
	 * move it out of the six. */
	CHECK( FNTECH3_ALICE_GAME_IMPORT_SLOTS - added - 1 - absent == 105 );
	static_assert( FNTECH3_ALICE_IMPORT_Cbuf_ExecuteText == 110, "the fifth append" );

	/* The four index calls sit in the reference's order, each over its own
	 * configstring block, and the model one is the one that loads a TIKI. */
	static_assert( FNTECH3_ALICE_IMPORT_imageindex == 45, "imageindex" );
	static_assert( FNTECH3_ALICE_IMPORT_itemindex == 46, "itemindex" );
	static_assert( FNTECH3_ALICE_IMPORT_soundindex == 47, "soundindex" );
	static_assert( FNTECH3_ALICE_IMPORT_modelindex == 48, "modelindex" );
	static_assert( FNTECH3_ALICE_IMPORT_GameDir == 50, "GameDir" );

	/* The slots the engine writes and this map names. */
	CHECK( literal + behaviour + arity + order
		== FNTECH3_ALICE_GAME_IMPORT_SLOTS - added - unnamed - absent );
	/* A name resting on position alone is the weakest kind here, and the
	 * map is only useful if most slots do better than that. */
	CHECK( order < literal + behaviour + arity );

	CHECK( kGameImports[101].evidence == Evidence::BEHAVIOUR );
	CHECK( kGameImports[102].evidence == Evidence::BEHAVIOUR );
}

/* What the module does with each slot, measured from its own call sites. */
static void CheckImportCallSites()
{
	int expected = 0, called = 0, settled = 0, uncalled = 0;
	for ( const auto &row : kGameImportCalls ) {
		CHECK( row.slot == expected++ );
		CHECK( row.sites >= 0 );
		/* A count is recorded only where the sites establish one. */
		if ( row.basis == CallBasis::AGREED || row.basis == CallBasis::PUSHES ) {
			CHECK( row.args >= 0 );
			++settled;
		} else {
			CHECK( row.args == -1 );
		}
		/* Never called and called are exactly the UNCALLED rows. */
		if ( row.basis == CallBasis::UNCALLED ) {
			CHECK( row.sites == 0 );
			++uncalled;
		} else {
			CHECK( row.sites > 0 );
			++called;
		}
	}
	CHECK( expected == FNTECH3_ALICE_GAME_IMPORT_SLOTS );
	CHECK( called == 92 );
	CHECK( uncalled == 19 );
	CHECK( settled == 59 );

	/* The slot the engine leaves null must be one the module never calls,
	 * or this build of Alice would call zero. That is the whole reason the
	 * two measurements have to agree. */
	for ( const auto &row : kGameImports ) {
		if ( row.evidence == Evidence::ABSENT ) {
			CHECK( kGameImportCalls[row.slot].basis == CallBasis::UNCALLED );
		}
	}

	/* The two data members are written by the engine and never called by the
	 * module, which is what tells a data pointer from a function pointer
	 * from the call side alone. */
	CHECK( kGameImportCalls[FNTECH3_ALICE_IMPORT_DebugLines].basis
		== CallBasis::UNCALLED );
	CHECK( kGameImportCalls[FNTECH3_ALICE_IMPORT_numDebugLines].basis
		== CallBasis::UNCALLED );

	/* The count that identifies slot 103 in the first place. */
	CHECK( kGameImportCalls[FNTECH3_ALICE_IMPORT_LocateGameData].args == 5 );

	/* The members whose argument count the reference table also fixes. Each
	 * of these was named before any count was measured, so the count is a
	 * second reading that has to agree with the first. */
	struct { int slot; int args; } known[] = {
		{ FNTECH3_ALICE_IMPORT_Free, 1 },
		{ FNTECH3_ALICE_IMPORT_FS_ReadFile, 3 },
		{ FNTECH3_ALICE_IMPORT_FS_FSeek, 3 },
		{ FNTECH3_ALICE_IMPORT_inPVS, 2 },
		{ FNTECH3_ALICE_IMPORT_AreasConnected, 2 },
		{ FNTECH3_ALICE_IMPORT_AdjustAreaPortalState, 2 },
		{ FNTECH3_ALICE_IMPORT_AreaEntities, 4 },
		{ FNTECH3_ALICE_IMPORT_CalculateBounds, 4 },
		{ FNTECH3_ALICE_IMPORT_Frame_Commands, 4 },
		{ FNTECH3_ALICE_IMPORT_Alias_Add, 4 },
	};
	for ( const auto &row : known ) {
		CHECK( kGameImportCalls[row.slot].args == row.args );
	}

	/* A variadic member cannot have one count, and the members that read as
	 * variadic are the printf family and the one server command that takes
	 * a format string. */
	for ( int slot : { FNTECH3_ALICE_IMPORT_Printf, FNTECH3_ALICE_IMPORT_DPrintf,
		FNTECH3_ALICE_IMPORT_DebugPrintf, FNTECH3_ALICE_IMPORT_Error,
		FNTECH3_ALICE_IMPORT_SendServerCommand } ) {
		CHECK( kGameImportCalls[slot].basis == CallBasis::VARIADIC );
	}
}

static void CheckExportSlots()
{
	int expected = 0, absent = 0, values = 0;
	std::set<std::string> names;
	for ( const auto &row : kGameExports ) {
		CHECK( row.slot == expected++ );
		CHECK( row.name && row.name[0] );
		CHECK( names.insert( row.name ).second );
		if ( row.evidence == ExportEvidence::ABSENT ) ++absent;
		if ( row.kind == SlotKind::VALUE ) ++values;
	}
	CHECK( expected == FNTECH3_ALICE_GAME_EXPORT_SLOTS );

	/* The three slots the module never writes are the two bot entry points
	 * and the frame preparation call. That Alice leaves both bot slots null
	 * agrees with its profile, which carries no bot library at all. */
	CHECK( absent == 3 );
	for ( int slot : { FNTECH3_ALICE_EXPORT_BotBegin,
		FNTECH3_ALICE_EXPORT_BotThink, FNTECH3_ALICE_EXPORT_PrepFrame } ) {
		CHECK( kGameExports[slot].evidence == ExportEvidence::ABSENT );
		/* An absent slot is still declared a function, so an adapter that
		 * ignored the evidence would jump to null rather than read a value. */
		CHECK( kGameExports[slot].kind == SlotKind::FUNCTION );
	}
	const auto *alice = Com_GameProfileById( COM_GAME_ALICE );
	CHECK( alice && alice->bots.aiSystems == 0 && alice->bots.navigationWorlds == 0 );

	/* Six slots carry data rather than code: the version, the entity array,
	 * its size, its count, its capacity, and the error message. */
	CHECK( values == 6 );
	for ( int slot : { 0, 21, 22, 23, 24, 25 } ) {
		CHECK( kGameExports[slot].kind == SlotKind::VALUE );
	}
	/* The entity size is the one value the module writes as a constant. */
	CHECK( kGameExports[FNTECH3_ALICE_EXPORT_gentitySize].evidence
		== ExportEvidence::IMMEDIATE );
}

/* The two delta field tables. What is asserted is the shape the retail
 * arrays have -- every row named, every offset four-aligned and inside the
 * record it bounds, every bit count one the Quake III convention allows, and
 * no offset claimed twice -- plus the handful of anchors that identify the
 * tables as Alice's rather than a neighbouring engine's. */
static void CheckWireFields()
{
	using fntech3::alice::module::WireField;
	using fntech3::alice::module::kEntityStateFields;
	using fntech3::alice::module::kPlayerStateFields;

	struct Table {
		const WireField *fields;
		std::size_t count;
		int minBytes;
	};
	const Table tables[] = {
		{ kEntityStateFields, FNTECH3_ALICE_ENTITY_STATE_FIELDS,
			FNTECH3_ALICE_ENTITY_STATE_DELTA_BYTES },
		{ kPlayerStateFields, FNTECH3_ALICE_PLAYER_STATE_FIELDS,
			FNTECH3_ALICE_PLAYER_STATE_DELTA_BYTES },
	};

	for ( const Table &table : tables ) {
		std::set<int> offsets;
		std::set<std::string> names;
		int highest = -1;

		for ( std::size_t i = 0; i < table.count; ++i ) {
			const WireField &field = table.fields[i];
			CHECK( field.name && field.name[0] );
			CHECK( names.insert( field.name ).second );
			CHECK( field.offset >= 0 && ( field.offset & 3 ) == 0 );
			CHECK( field.offset + 4 <= table.minBytes );
			CHECK( offsets.insert( field.offset ).second );
			/* Zero is a float; anything else is a bit width the writer can
			 * shift by, signed when negative. */
			const int width = field.bits < 0 ? -field.bits : field.bits;
			CHECK( width >= 0 && width <= 32 );
			if ( field.offset > highest ) {
				highest = field.offset;
			}
		}
		/* The record is bounded by its last field, not by a declared size. */
		CHECK( highest + 4 == table.minBytes );
	}

	/* Anchors. The first entity field is Quake III's, which is why the
	 * tables look familiar; the ones after it are Ritual's and are why
	 * no Quake III field table describes this record. */
	CHECK( !std::strcmp( kEntityStateFields[0].name, "eType" ) );
	CHECK( kEntityStateFields[0].offset == 4 );
	CHECK( !std::strcmp( kEntityStateFields[78].name, "renderfx2" ) );

	std::set<std::string> entityNames;
	for ( const WireField &field : kEntityStateFields ) {
		entityNames.insert( field.name );
	}
	/* Five bone modifiers of four fields each, an attachment triple, a
	 * separate torso animation and a second render-effects word: 21 of the
	 * 79 entity fields have no counterpart in Quake III's 51. */
	for ( const char *name : { "bonemod[0].tag", "bonemod[4].scale",
		"parent", "tag_num", "attach_use_angles", "attach_offset[0]",
		"torso_anim", "torso_frame", "torso_crossblend_time",
		"crossblend_time", "alpha", "scale", "renderfx2",
		"loopSoundVolume", "loopSoundMinDist" } ) {
		CHECK( entityNames.count( name ) == 1 );
	}
	/* And Quake III fields this record does not have at all. */
	for ( const char *name : { "weapon", "legsAnim", "powerups", "event",
		"eventParm", "otherEntityNum", "generic1" } ) {
		CHECK( entityNames.count( name ) == 0 );
	}

	std::set<std::string> playerNames;
	for ( const WireField &field : kPlayerStateFields ) {
		playerNames.insert( field.name );
	}
	/* Alice's player state carries the music, reverb and camera state
	 * Quake III's client game would own, which is why its record is more
	 * than three times the size. */
	for ( const char *name : { "current_music_mood", "fallback_music_mood",
		"music_volume", "reverb_type", "reverb_level", "camera_origin[0]",
		"camera_angles[0]", "camera_offset[0]", "camera_time",
		"camera_flags", "damage_angles[0]", "feetfalling", "pm_runtime" } ) {
		CHECK( playerNames.count( name ) == 1 );
	}
	for ( const char *name : { "weapon", "weaponstate", "ammo", "stats",
		"persistant", "powerups", "externalEvent", "generic1" } ) {
		CHECK( playerNames.count( name ) == 0 );
	}
	/* Each delta table reaches part of its record, and each record fits
	 * inside what the module publishes around it. A table that reached its
	 * record's end would be the suspicious case, not this one. */
	static_assert( FNTECH3_ALICE_ENTITY_STATE_DELTA_BYTES
		< FNTECH3_ALICE_ENTITY_STATE_BYTES, "entity delta table" );
	static_assert( FNTECH3_ALICE_PLAYER_STATE_DELTA_BYTES
		< FNTECH3_ALICE_PLAYER_STATE_BYTES, "player delta table" );
	static_assert( FNTECH3_ALICE_ENTITY_STATE_BYTES
		== FNTECH3_ALICE_ENTITY_SHARED_OFFSET, "shared view offset" );
	static_assert( FNTECH3_ALICE_ENTITY_STATE_BYTES < FNTECH3_ALICE_GENTITY_SIZE,
		"entity state inside the entity" );
	static_assert( FNTECH3_ALICE_PLAYER_STATE_BYTES < FNTECH3_ALICE_GCLIENT_SIZE,
		"player state inside the client" );
	/* Every offset the engine reads out of its own half of an entity has to
	 * land past the entity state and inside the record the module publishes. */
	for ( int offset : { FNTECH3_ALICE_R_INUSE, FNTECH3_ALICE_R_LINKED,
		FNTECH3_ALICE_R_SVFLAGS, FNTECH3_ALICE_R_BMODEL, FNTECH3_ALICE_R_MINS,
		FNTECH3_ALICE_R_MAXS, FNTECH3_ALICE_R_CONTENTS, FNTECH3_ALICE_R_ABSMIN,
		FNTECH3_ALICE_R_ABSMAX, FNTECH3_ALICE_R_CURRENTORIGIN,
		FNTECH3_ALICE_R_CURRENTANGLES, FNTECH3_ALICE_R_SOLIDTYPE } ) {
		CHECK( offset >= FNTECH3_ALICE_ENTITY_SHARED_OFFSET );
		CHECK( offset + 4 <= FNTECH3_ALICE_GENTITY_SIZE );
	}
	/* And the two the engine reads out of the state's own tail land past the
	 * delta table but inside the state. */
	for ( int offset : { FNTECH3_ALICE_S_OWNERNUM, FNTECH3_ALICE_S_AXIS } ) {
		CHECK( offset >= FNTECH3_ALICE_ENTITY_STATE_DELTA_BYTES );
		CHECK( offset < FNTECH3_ALICE_ENTITY_STATE_BYTES );
	}
	static_assert( FNTECH3_ALICE_S_AXIS + 9 * 4 == FNTECH3_ALICE_ENTITY_STATE_BYTES,
		"the axis ends the entity state" );
	static_assert( FNTECH3_ALICE_PS_PING + 4 == FNTECH3_ALICE_PLAYER_STATE_BYTES,
		"ping ends the player state" );
}

/* The client game's map, held to the engine measurements the header already
 * carried before any slot had a name. */
static_assert( FNTECH3_ALICE_CG_SharedBlock == FNTECH3_ALICE_CGAME_IMPORT_FIRST_CALLED,
	"the slot the module calls first is the one Alice appended" );
static_assert( FNTECH3_ALICE_CG_apiversion == FNTECH3_ALICE_CGAME_IMPORT_UNFILLED_A
	&& FNTECH3_ALICE_CG_NumSkins == FNTECH3_ALICE_CGAME_IMPORT_UNFILLED_B
	&& FNTECH3_ALICE_CG_Tag_OrientationEx == FNTECH3_ALICE_CGAME_IMPORT_UNFILLED_C,
	"the three slots the engine never writes" );
static_assert( FNTECH3_ALICE_CG_GetGameState == 74 && FNTECH3_ALICE_CG_GetGlconfig == 77,
	"the two records the client game is handed" );
static_assert( FNTECH3_ALICE_CGE_Init == FNTECH3_ALICE_CGAME_EXPORT_INIT,
	"the export that receives the imports" );

static void CheckClientGameImports()
{
	std::set<std::string> names;
	int absent = 0, renderer = 0, shared = 0;

	for ( int i = 0; i < FNTECH3_ALICE_CGAME_IMPORT_SLOTS; i++ ) {
		const CGameImportSlot &row = kCGameImports[i];
		CHECK( row.slot == i );
		CHECK( names.insert( row.name ).second );
		CHECK( row.sites >= 0 );
		/* A count is only ever claimed where the sites establish one. */
		if ( row.basis == CallBasis::UNCALLED ) {
			CHECK( row.sites == 0 && row.args == -1 );
		} else {
			CHECK( row.sites > 0 );
		}
		if ( row.basis == CallBasis::AGREED || row.basis == CallBasis::PUSHES ) {
			CHECK( row.args >= 0 );
		} else {
			CHECK( row.args == -1 );
		}
		if ( row.evidence == Evidence::ABSENT ) {
			/* The engine leaves it null, so the module must never call it. */
			CHECK( row.basis == CallBasis::UNCALLED );
			absent++;
		}
		/* The renderer's own entry points are copied in as one block and
		 * two strays; nothing else is read out of it. */
		if ( row.evidence == Evidence::RENDERER ) {
			CHECK( ( i >= FNTECH3_ALICE_CG_EndRegistration && i <= FNTECH3_ALICE_CG_R_DrawBox )
				|| i == FNTECH3_ALICE_CG_TIKI_GetHandle || i == FNTECH3_ALICE_CG_FlushAll );
			renderer++;
		}
		if ( row.evidence == Evidence::SHARED ) {
			shared++;
		}
		CHECK( row.evidence != Evidence::UNNAMED );
	}
	CHECK( absent == 3 );
	CHECK( renderer == 27 );
	CHECK( shared == 17 );

	/* A slot named from a shared engine address has to carry the name the
	 * game table gives that address -- the printf family, the cvar pair and
	 * the error call at least, which both tables name the same way. */
	CHECK( !std::strcmp( kCGameImports[1].name, kGameImports[0].name ) );
	CHECK( !std::strcmp( kCGameImports[2].name, kGameImports[1].name ) );
	CHECK( !std::strcmp( kCGameImports[3].name, kGameImports[2].name ) );
	CHECK( !std::strcmp( kCGameImports[6].name, kGameImports[3].name ) );

	/* Measured shapes an adapter depends on. The reference declares seven
	 * arguments for Tag_Orientation; Alice's sites push six and the hidden
	 * return pointer, and so does the engine's own body. */
	CHECK( kCGameImports[FNTECH3_ALICE_CG_Tag_Orientation].args == 7 );
	CHECK( kCGameImports[FNTECH3_ALICE_CG_Tag_LerpedOrientation].args == 4 );
	CHECK( kCGameImports[FNTECH3_ALICE_CG_S_StartSound].args == 6 );
	CHECK( kCGameImports[FNTECH3_ALICE_CG_CM_TransformedBoxTrace].args == 10 );
	CHECK( kCGameImports[FNTECH3_ALICE_CG_R_DrawStretchPic].args == 9 );
	CHECK( kCGameImports[FNTECH3_ALICE_CG_R_AddPolyToScene].args == 4 );
	CHECK( kCGameImports[FNTECH3_ALICE_CG_SharedBlock].args == 0 );
	/* Named from its body, not from its error string, which names the
	 * damage query the engine keeps beside it. */
	CHECK( !std::strcmp( kCGameImports[111].name, "Surface_Flags" ) );
}

static void CheckClientGameExports()
{
	for ( int i = 0; i < FNTECH3_ALICE_CGAME_EXPORT_SLOTS; i++ ) {
		CHECK( kCGameExports[i].slot == i );
		CHECK( kCGameExports[i].args >= 0 );
		/* No slot is data: this table has no version member. */
		CHECK( kCGameExports[i].evidence != ExportEvidence::RUNTIME
			&& kCGameExports[i].evidence != ExportEvidence::IMMEDIATE );
	}
	CHECK( kCGameExports[FNTECH3_ALICE_CGE_Init].args == 3 );
	CHECK( kCGameExports[FNTECH3_ALICE_CGE_DrawActiveFrame].args == 3 );
	CHECK( kCGameExports[FNTECH3_ALICE_CGE_EntityOrientation].evidence
		== ExportEvidence::ADDED );
}

static void CheckNothingElseClaimsThisApi()
{
	for ( unsigned i = 0; i < Com_GameManagerProfileCount(); ++i ) {
		const auto *profile = Com_GameManagerProfileAt( i );
		const bool alice = profile->titleId == COM_GAME_TITLE_ALICE;
		CHECK( !!Com_GameProfileAllowsModuleApi( profile,
			COM_MODULE_API_ALICE_NATIVE ) == alice );
	}
}

int main()
{
	CheckEntryPoints();
	CheckProfileAgreement();
	CheckImportSlots();
	CheckImportCallSites();
	CheckExportSlots();
	CheckClientGameImports();
	CheckClientGameExports();
	CheckWireFields();
	CheckNothingElseClaimsThisApi();
	std::cout << "Alice module table and import slot tests passed\n";
	return 0;
}
