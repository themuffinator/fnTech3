/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_ALICE_SAVE_HPP
#define FNTECH3_SERVER_ALICE_SAVE_HPP

/*
 * American McGee's Alice's savegames and the server's side of its level flow,
 * as retail alice.exe runs them (SV_AddOperatorCommands 0x00450dd0 and the
 * routines it registers). Included at the end of server_alice_game.hpp, whose
 * export calls it builds on.
 *
 * A save is four files named by the archive builder, save/<config>_<name>:
 * the game module writes the level (.sav) and the persistent data (.spv), the
 * engine a 200-byte header with the area portals behind it (.ssv), and the
 * client a grey picture of the frame (.tga, through the `saveshot` command).
 * The archive the engine is working on is `current` between saves; the
 * persistent file of `current` is what carries Alice's inventory from one
 * level to the next -- `gamemap` writes it before a level change and every
 * fresh spawn reads it back.
 *
 * Retail writes all of it into its installation (fs_basepath); fnTech3
 * writes into fs_homepath and reads through the search path, so a retail
 * save copied into the home path loads and the installation is never
 * written. See docs/fntech3/2026-09-23-alice-savegames.md.
 */

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#ifndef DEDICATED
/* The engine UI's state that retail's save and load commands gate on: the
 * intro's stage (0x005315f8, -1 once a map has loaded) and the loading
 * screen's flag (0x006a006c). */
extern "C" int CL_AliceUIGetIntroState( void );
bool CL_AliceLoadingActive( void );
/* The client's CIN_Stop (0x00425770), in cl_cin.cpp. */
void CL_AliceStopCinematic( void );
#endif

namespace {

namespace alicesave = fntech3::alice::savegame;

/* The engine's archive state. Retail keeps it inside svs (0x00ac0d48,
 * 0x00ac0d88, 0x00ac0dc8): the archive name, "current" between saves; the
 * level's BSP name; and the whole `map` argument, `$` target and all. */
char aliceGameName[alicesave::NameBytes] = "current";
char aliceMapName[alicesave::NameBytes];
char aliceRawServerName[alicesave::NameBytes];

/* SV_SaveGame's result, which the UI's CheckErrorState reads (0x00b59bc0). */
int aliceSaveError;

/* A loadgame between reading the header and spawning: the level clock the
 * save recorded, which the spawn restores before the module's Init. */
bool aliceLoadPending;
int aliceLoadLevelTime;

/* The BSP the server last spawned (retail's 0x00b59b80, written at
 * 0x00455130), and the configstrings a load of that same BSP keeps across
 * its spawn (SV_AlicePrepareSpawn). */
char aliceSpawnedBsp[alicesave::NameBytes];
std::vector<std::string> aliceKeptConfigstrings;

bool aliceFreeSpaceUnknownReported;

/* What SV_SaveGame reads to decide the player is alive (0x00451dfc..
 * 0x00451e2d): the client's entity's game client, the word at +0x200 -- the
 * first past its 512-byte entity state -- and in that record the first stat
 * of the player state it begins with, at +0xb0. */
constexpr int kAliceEntityClient = 0x200;
constexpr int kAlicePlayerHealth = static_cast<int>( offsetof( fntech3_alice_playerState_t, block ) );
static_assert( kAliceEntityClient == FNTECH3_ALICE_ENTITY_SHARED_OFFSET,
	"the game client pointer is the first word past the entity state" );
static_assert( kAlicePlayerHealth == 0xb0, "the first stat array begins at +0xb0" );

/* An archive path, from one of four rotating buffers; null for a name the
 * rules refuse. */
const char *SV_AliceArchive( const char *name, const char *extension ) {
	static char paths[4][alicesave::PathBytes];
	static int next;
	char ( &path )[alicesave::PathBytes] = paths[next++ & 3];

	return SV_AliceArchivePath( name, extension, path ) ? path : nullptr;
}

/* Com_WipeSaveGame (0x0043a400): the four files of one archive, in retail's
 * order. Only fs_homepath is ever written. */
void SV_AliceWipeSaveGame( const char *name ) {
	for ( const char *extension : alicesave::WipeOrder ) {
		const char *path = SV_AliceArchive( name, extension );
		if ( path ) {
			FS_HomeRemove( path );
		}
	}
}

/* SV_CopyPersistant (0x00451690): current.spv to the named archive when
 * saving, and back when loading. A missing source is skipped, as retail's
 * FS_CopyFile (0x0043b7e0) skips it. */
void SV_AliceCopyPersistant( bool toCurrent ) {
	char current[alicesave::PathBytes];
	char named[alicesave::PathBytes];

	if ( !SV_AliceArchivePath( alicesave::CurrentName, alicesave::PersistentExtension, current )
		|| !SV_AliceArchivePath( aliceGameName, alicesave::PersistentExtension, named ) ) {
		return;
	}
	const char *from = toCurrent ? named : current;
	const char *to = toCurrent ? current : named;

	Com_Printf( "copy %s to %s\n", from, to );
	void *buffer = nullptr;
	const int length = FS_ReadFile( from, &buffer );
	if ( length < 0 || !buffer ) {
		return;
	}
	if ( Q_stricmp( from, to ) ) {
		FS_WriteFile( to, buffer, length );
	}
	FS_FreeFile( buffer );
}

/* SV_ArchivePersistantFile (0x00451750). Loading, the module's answer is
 * returned but the spawn ignores it, as retail's does. */
qboolean SV_AliceArchivePersistantFile( bool loading ) {
	Com_DPrintf( "SV_ArchivePersistantFile()\n" );
	const char *path = SV_AliceArchive( aliceGameName, alicesave::PersistentExtension );
	if ( !path ) {
		return qfalse;
	}
	if ( loading ) {
		return SV_AliceCallReadPersistant( path );
	}
	SV_AliceCallWritePersistant( path );
	return qtrue;
}

/* The area portal block (CM_WritePortalState 0x004362a0): the map's area
 * count and a reference count for every pair of areas. */
void SV_AliceWritePortalState( fileHandle_t file ) {
	int count = 0;
	const int *counts = CM_AreaPortalCounts( &count );
	std::vector<uint8_t> bytes( sizeof( int32_t ) * ( 1 + static_cast<std::size_t>( count ) ) );

	alicesave::WriteInt32( bytes.data(), CM_NumAreas() );
	for ( int i = 0; i < count; ++i ) {
		alicesave::WriteInt32( bytes.data() + sizeof( int32_t ) * ( 1 + i ), counts[i] );
	}
	FS_Write( bytes.data(), static_cast<int>( bytes.size() ), file );
}

/* A `.ssv` read whole: its header and whatever portal block follows it. It
 * is read into memory and released before anything is checked, so an error
 * raised over its contents leaves no file open. */
struct AliceServerFile {
	alicesave::ServerHeader header;
	int numAreas = -1;
	std::vector<int32_t> counts;
};

bool SV_AliceReadServerFile( const char *path, AliceServerFile &file ) {
	void *buffer = nullptr;
	const int length = path ? FS_ReadFile( path, &buffer ) : -1;

	if ( length < 0 || !buffer ) {
		return false;
	}
	const auto *bytes = static_cast<const uint8_t *>( buffer );
	const std::size_t size = static_cast<std::size_t>( length );
	const bool whole = size >= alicesave::HeaderBytes;

	if ( whole ) {
		uint8_t header[alicesave::HeaderBytes];
		std::memcpy( header, bytes, sizeof( header ) );
		alicesave::Decode( header, file.header );
		std::size_t portalBytes = 0;
		if ( size >= alicesave::HeaderBytes + sizeof( int32_t ) ) {
			file.numAreas = alicesave::ReadInt32( bytes + alicesave::HeaderBytes );
			if ( alicesave::PortalBytes( file.numAreas, portalBytes )
				&& size >= alicesave::HeaderBytes + portalBytes ) {
				const std::size_t pairs = static_cast<std::size_t>( file.numAreas ) * file.numAreas;
				file.counts.resize( pairs );
				for ( std::size_t i = 0; i < pairs; ++i ) {
					file.counts[i] = alicesave::ReadInt32( bytes + alicesave::HeaderBytes
						+ sizeof( int32_t ) * ( 1 + i ) );
				}
			}
		}
	}
	FS_FreeFile( buffer );
	return whole;
}

/* CM_ReadPortalState (0x004362e0): a different area count is retail's own
 * drop (0x00436302). A block cut short or a negative count is refused too,
 * where retail would read on regardless. Then the areas are flooded again. */
void SV_AliceRestorePortalState( const AliceServerFile &file ) {
	int count = 0;
	int *counts = CM_AreaPortalCounts( &count );

	if ( file.numAreas != CM_NumAreas() ) {
		Com_Error( ERR_DROP, "numareaportals differs from save game" );
		return;
	}
	if ( !counts || file.counts.size() != static_cast<std::size_t>( count ) ) {
		Com_Error( ERR_DROP, "The save game's area portals are cut short" );
		return;
	}
	for ( int i = 0; i < count; ++i ) {
		if ( file.counts[i] < 0 ) {
			Com_Error( ERR_DROP, "The save game holds a negative area portal count" );
			return;
		}
	}
	for ( int i = 0; i < count; ++i ) {
		counts[i] = file.counts[i];
	}
	CM_FloodAreaConnections();
}

/* SV_ArchiveLevelFile (0x004517a0). Writing hands the module the `.sav`
 * path and a constant zero for its autosave flag, even for an autosave
 * (0x00451840). Reading asks the module first, and only when it succeeds puts
 * the area portals back from the `.ssv`; a `.ssv` that is missing there is
 * skipped, as retail skips it. */
qboolean SV_AliceArchiveLevelFile( bool loading ) {
	Com_DPrintf( "SV_ArchiveLevelFile()\n" );
	const char *path = SV_AliceArchive( aliceGameName, alicesave::LevelExtension );
	if ( !path ) {
		return qfalse;
	}
	if ( !loading ) {
		SV_AliceCallWriteLevel( path );
		return qtrue;
	}
	if ( !SV_AliceCallReadLevel( path ) ) {
		return qfalse;
	}
	AliceServerFile file;
	if ( SV_AliceReadServerFile( SV_AliceArchive( aliceGameName, alicesave::ServerExtension ), file ) ) {
		SV_AliceRestorePortalState( file );
	}
	return qtrue;
}

/* SV_ArchiveServerFile (0x00451860). Saving writes the header and the portal
 * block, then asks the client for the picture with the command retail
 * issues; a dedicated server has no such command, as retail's has none.
 * Loading takes the map and the level clock from the header, which carries
 * no magic and no version: retail checks nothing in it, and this checks only
 * that it is whole and names a map that exists. */
qboolean SV_AliceArchiveServerFile( bool loading, bool autosave ) {
	Com_DPrintf( "SV_ArchiveServerFile(%s)\n", autosave ? "true" : "false" );
	const char *path = SV_AliceArchive( aliceGameName, alicesave::ServerExtension );
	if ( !path ) {
		return qfalse;
	}

	if ( !loading ) {
		const fileHandle_t file = FS_FOpenFileWrite( path );
		if ( !file ) {
			Com_Printf( "Couldn't write %s\n", path );
			return qfalse;
		}
		alicesave::ServerHeader header;
		header.saveTime = static_cast<int32_t>( std::time( nullptr ) );
		header.levelTime = sv.time;
		const char *title = sv.configstrings[2] ? sv.configstrings[2] : "";
		if ( autosave ) {
			Com_sprintf( header.comment, static_cast<int>( alicesave::FieldBytes ), "Starting %s", title );
		} else {
			Com_sprintf( header.comment, static_cast<int>( alicesave::FieldBytes ), "%s", title );
		}
		Q_strncpyz( header.mapName, aliceMapName, sizeof( header.mapName ) );
		Q_strncpyz( header.saveName, aliceGameName, sizeof( header.saveName ) );

		uint8_t bytes[alicesave::HeaderBytes];
		alicesave::Encode( header, bytes );
		FS_Write( bytes, sizeof( bytes ), file );
		SV_AliceWritePortalState( file );
		FS_FCloseFile( file );

		const char *shot = SV_AliceArchive( aliceGameName, alicesave::ShotExtension );
		if ( shot ) {
			Cbuf_ExecuteText( EXEC_NOW, va( "saveshot %s 256 256\n", shot ) );
		}
		return qtrue;
	}

	AliceServerFile file;
	if ( !SV_AliceReadServerFile( path, file ) ) {
		Com_Printf( "Couldn't read %s\n", path );
		return qfalse;
	}
	const char *map = file.header.mapName;
	char bsp[MAX_QPATH];
	if ( !map[0] || strstr( map, ".." ) || strchr( map, '\\' ) || strchr( map, ':' )
		|| strchr( map, '$' ) ) {
		Com_Printf( "%s names no map this engine can load\n", path );
		return qfalse;
	}
	Com_sprintf( bsp, sizeof( bsp ), "maps/%s.bsp", map );
	if ( FS_ReadFile( bsp, nullptr ) == -1 ) {
		Com_Printf( "Can't find map %s\n", bsp );
		return qfalse;
	}
	Q_strncpyz( aliceMapName, map, sizeof( aliceMapName ) );
	aliceLoadLevelTime = file.header.levelTime;
	return qtrue;
}

/* SV_FindSaveName (0x00451ce0): the map's name and four digits, the first
 * whose `.ssv` is missing or empty; when all ten thousand are taken, the last
 * is used again. */
void SV_AliceFindSaveName( char ( &name )[alicesave::NameBytes] ) {
	char prefix[alicesave::NameBytes];

	alicesave::AutosavePrefix( aliceMapName, prefix );
	for ( int i = 0; i < alicesave::AutosaveNames; ++i ) {
		if ( !alicesave::AutosaveName( prefix, i, name ) ) {
			return;
		}
		const char *path = SV_AliceArchive( name, alicesave::ServerExtension );
		if ( !path || FS_ReadFile( path, nullptr ) <= 0 ) {
			return;
		}
	}
}

/* The free-space test (0x0043edb0, compared at 0x00451e33): at least 4 MB.
 * Retail asks the drive of fs_basepath, which it writes into; this asks
 * fs_homepath's for the same reason. Retail refuses when the question fails
 * (its answer is then 2 MB) and, through a signed comparison, on a disk with
 * more than 2 TiB free; neither is reproduced -- an unanswerable question
 * lets the save go ahead. */
bool SV_AliceEnoughFreeSpace() {
	std::error_code error;
	const std::filesystem::space_info space =
		std::filesystem::space( std::filesystem::u8path( Cvar_VariableString( "fs_homepath" ) ), error );

	if ( error ) {
		if ( !aliceFreeSpaceUnknownReported ) {
			aliceFreeSpaceUnknownReported = true;
			Com_DPrintf( "Alice savegames: the home path's free space is unknown (%s); saving anyway\n",
				error.message().c_str() );
		}
		return true;
	}
	return static_cast<long long>( space.available >> 10 ) >= alicesave::MinimumFreeKilobytes;
}

/* Client zero has an entity in the world, a game client behind it, and a
 * first stat that is not zero. The pointer the module keeps is only followed
 * when it lands on one of the client records the module published. */
bool SV_AlicePlayerAlive() {
	if ( !svs.clients || sv.maxclients < 1 || !svs.clients[0].gentity ) {
		return false;
	}
	void *entity = SV_AliceEntityByNumber( 0 );
	if ( !entity || !aliceGameData.clients || aliceGameData.clientBytes <= 0 ) {
		return false;
	}
	/* F.A.K.K.2's game client pointer is the first word past its own, shorter
	 * entity state: the entity record's shared half starts there. */
	const byte *client = *SV_AliceEntityField<const byte *>( entity,
		SV_RitualIsFAKK2() ? FNTECH3_FAKK2_R_CLIENT : kAliceEntityClient );
	const byte *first = static_cast<const byte *>( aliceGameData.clients );
	const std::ptrdiff_t offset = client - first;
	if ( !client || offset < 0 || offset % aliceGameData.clientBytes
		|| offset / aliceGameData.clientBytes >= sv.maxclients ) {
		return false;
	}
	int32_t health;
	std::memcpy( &health, client + kAlicePlayerHealth, sizeof( health ) );
	return health != 0;
}

/* SV_SaveGame (0x00451d90). `requirePlayer` is fnTech3's: only the developer
 * archive probe below turns it off, to exercise a dedicated server's module
 * with nobody connected. */
void SV_AliceSaveGame( const char *requested, bool autosave, bool requirePlayer = true ) {
	using alicesave::SaveError;
	char name[alicesave::NameBytes];

	if ( sv.state != SS_GAME ) {
		Com_Printf( "You must be in a game to save.\n" );
		aliceSaveError = static_cast<int>( SaveError::NotInGame );
		return;
	}
	if ( Cvar_VariableValue( "deathmatch" ) != 0.0f ) {
		Com_Printf( "Can't savegame in a deathmatch\n" );
		aliceSaveError = static_cast<int>( SaveError::Deathmatch );
		return;
	}
	if ( requirePlayer && !SV_AlicePlayerAlive() ) {
		Com_Printf( "Can't savegame when dead\n" );
		aliceSaveError = static_cast<int>( SaveError::Dead );
		return;
	}
	if ( !SV_AliceEnoughFreeSpace() ) {
		Com_Printf( "Not enough free space to save\n" );
		aliceSaveError = static_cast<int>( SaveError::DiskFull );
		return;
	}
	if ( requested ) {
		if ( !alicesave::NameAllowed( requested ) ) {
			Com_DPrintf( "Bad savedir.\n" );
			aliceSaveError = static_cast<int>( SaveError::BadName );
			return;
		}
		Q_strncpyz( name, requested, sizeof( name ) );
	} else {
		SV_AliceFindSaveName( name );
	}
	if ( !alicesave::NameAllowed( name ) ) {
		Com_DPrintf( "Bad savedir.\n" );
		aliceSaveError = static_cast<int>( SaveError::BadName );
		return;
	}

	Com_DPrintf( "Saving game...%s\n", name );
	Q_strncpyz( aliceGameName, name, sizeof( aliceGameName ) );
	SV_AliceCopyPersistant( false );
	SV_AliceArchiveLevelFile( false );
	SV_AliceArchiveServerFile( false, autosave );
	Com_Printf( "Done.\n" );
	Q_strncpyz( aliceGameName, alicesave::CurrentName, sizeof( aliceGameName ) );
	aliceSaveError = static_cast<int>( SaveError::None );
}

int SV_AliceIntroStage() {
#ifndef DEDICATED
	if ( !com_dedicated->integer ) {
		return CL_AliceUIGetIntroState();
	}
#endif
	/* The stage starts at 0 in retail's data and only its client moves it. */
	return 0;
}

bool SV_AliceLoadingScreenUp() {
#ifndef DEDICATED
	if ( !com_dedicated->integer ) {
		return CL_AliceLoadingActive();
	}
#endif
	return false;
}

/* CIN_Stop (0x00425770), which `map`, `loadgame` and `loadbrokengame` call
 * once `nextmap` is cleared and before they spawn or load a module
 * (0x00451118, 0x00451c53, 0x00451b09). It stops the client's cinematic only
 * while one plays (cls.state 9); a dedicated server has no client, and its
 * build has no hook to call. */
void SV_AliceStopCinematic() {
#ifndef DEDICATED
	if ( !com_dedicated->integer ) {
		CL_AliceStopCinematic();
	}
#endif
}

/* savegame and loadgame do nothing, silently, before the intro has given way
 * to a map unless developer is set (0x0045201c, 0x00451b34). */
bool SV_AliceSaveLoadAllowed() {
	return Cvar_VariableIntegerValue( "developer" ) || SV_AliceIntroStage() == -1;
}

/* savegame [name] (0x00452010). A failed save puts up the load/save menu,
 * through the engine UI's push (0x0040cca0 with "loadsave"). */
void SV_AliceSaveGame_f() {
	if ( !SV_AliceSaveLoadAllowed() || SV_AliceLoadingScreenUp() ) {
		return;
	}
	SV_AliceSaveGame( Cmd_Argc() == 2 ? Cmd_Argv( 1 ) : nullptr, false );
#ifndef DEDICATED
	if ( aliceSaveError && !com_dedicated->integer ) {
		Cbuf_ExecuteText( EXEC_NOW, "pushmenu loadsave\n" );
	}
#endif
}

/* autosavegame (0x00452070): the same save under a found name with the
 * "Starting" comment, and no gates at all. */
void SV_AliceAutosaveGame_f() {
	SV_AliceSaveGame( nullptr, true );
}

/* fnTech3's developer probe: the save above without its player check, so a
 * dedicated server with nobody connected can have its module archive a
 * level. It is not a retail command and does nothing without developer. */
void SV_AliceArchiveProbe_f() {
	if ( !Cvar_VariableIntegerValue( "developer" ) ) {
		Com_Printf( "alicearchive is a developer probe; set developer 1\n" );
		return;
	}
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: alicearchive <name>\n" );
		return;
	}
	char name[alicesave::NameBytes];
	Q_strncpyz( name, Cmd_Argv( 1 ), sizeof( name ) );
	SV_AliceSaveGame( name, false, false );
	Com_Printf( "alicearchive %s: level time %i, save error %i\n", name, sv.time, aliceSaveError );
}

/* The load both commands finish with (0x00451ca3..0x00451cc3): the header,
 * then current.spv from the save, then a spawn of the header's map that reads
 * the level back. */
void SV_AliceLoadArchive() {
	if ( !SV_AliceArchiveServerFile( true, false ) ) {
		return;
	}
	SV_AliceCopyPersistant( true );
	aliceLoadPending = true;
	char map[alicesave::NameBytes];
	Q_strncpyz( map, aliceMapName, sizeof( map ) );
	SV_SpawnServer( map, qfalse );
}

/* The name a load command was given, refused where retail only prints "Bad
 * savedir." and goes on to use it (0x00451bdb). */
bool SV_AliceLoadName( const char *usage, const char *announce ) {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "%s", usage );
		return false;
	}
	const char *name = Cmd_Argv( 1 );
	Com_Printf( "%s%s\n", announce, name );
	if ( !alicesave::NameAllowed( name ) ) {
		Com_Printf( "Bad savedir.\n" );
		return false;
	}
	Q_strncpyz( aliceGameName, name, sizeof( aliceGameName ) );
	return true;
}

/* loadgame <name> (0x00451b20). */
void SV_AliceLoadGame_f() {
	if ( !SV_AliceSaveLoadAllowed() || sv_maxclients->integer > 1
		|| ( svs.clients && svs.clients[0].state == CS_PRIMED ) || SV_AliceLoadingScreenUp() ) {
		return;
	}
	if ( !SV_AliceLoadName( "USAGE: loadgame <name>\n", "Loading game..." ) ) {
		return;
	}
	char name[alicesave::NameBytes];
	char level[alicesave::PathBytes];
	Q_strncpyz( name, aliceGameName, sizeof( name ) );
	if ( !SV_AliceArchivePath( name, alicesave::LevelExtension, level )
		|| FS_ReadFile( level, nullptr ) == -1 ) {
		Com_Printf( "Savegame not found.\n" );
		return;
	}
	/* Cleared so that the cinematic stopped next cannot chain a map. */
	Cvar_Set( "nextmap", "" );
	SV_AliceStopCinematic();
	if ( !gvm ) {
		SV_InitGameProgs();
	}
	if ( !SV_AliceCallLevelArchiveValid( level ) ) {
		Cbuf_ExecuteText( EXEC_NOW, va( "dialog \"\" \"\" \"loadbrokengame %s\" 260 150 "
			"textures\\menu\\invalid textures\\menu\\okbutton textures\\menu\\cancelbutton\n", name ) );
		return;
	}
	SV_AliceLoadArchive();
}

/* loadbrokengame <name> (0x00451a50): the load without its gates, its
 * existence test or the module's validity check. */
void SV_AliceLoadBrokenGame_f() {
	if ( !SV_AliceLoadName( "USAGE: loadbrokengame <name>\n", "Loading broken game..." ) ) {
		return;
	}
	if ( !SV_AliceArchiveServerFile( true, false ) ) {
		return;
	}
	SV_AliceCopyPersistant( true );
	Cvar_Set( "nextmap", "" );
	SV_AliceStopCinematic();
	aliceLoadPending = true;
	char map[alicesave::NameBytes];
	Q_strncpyz( map, aliceMapName, sizeof( map ) );
	SV_SpawnServer( map, qfalse );
}

/* The BSP a map argument names: the part before any `$` (0x00450f96). */
void SV_AliceBspName( const char *server, char *bsp, int size ) {
	Q_strncpyz( bsp, server, size );
	char *target = strchr( bsp, '$' );
	if ( target ) {
		*target = '\0';
	}
}

bool SV_AliceMapExists( const char *server, char ( &path )[MAX_QPATH] ) {
	char bsp[MAX_QPATH];
	SV_AliceBspName( server, bsp, sizeof( bsp ) );
	Com_sprintf( path, sizeof( path ), "maps/%s.bsp", bsp );
	return bsp[0] && FS_ReadFile( path, nullptr ) != -1;
}

/* map and devmap (both 0x00450f00). */
void SV_AliceMap_f() {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "USAGE: map <map>\n" );
		return;
	}
	char server[MAX_QPATH];
	const char *mapdir = Cvar_VariableString( "mapdir" );
	if ( Cvar_VariableIntegerValue( "developer" ) && mapdir[0] ) {
		Com_sprintf( server, sizeof( server ), "%s/%s", mapdir, Cmd_Argv( 1 ) );
	} else {
		Q_strncpyz( server, Cmd_Argv( 1 ), sizeof( server ) );
	}
	/* Backslashes become slashes (0x0044bef0). */
	for ( char *c = server; *c; ++c ) {
		if ( *c == '\\' ) {
			*c = '/';
		}
	}
	char path[MAX_QPATH];
	if ( !SV_AliceMapExists( server, path ) ) {
		Com_Printf( "Can't find map %s\n", path );
		return;
	}
	/* The game type is registered here and a single-player type put back to
	 * zero; retail's other branch, for commands beginning `sp`, has no
	 * command to reach it. */
	if ( Cvar_Get( "g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH )->integer == 2 ) {
		Cvar_Set( "g_gametype", "0" );
	}
	/* cheats follows the command only with developer off; developer leaves it. */
	if ( !Cvar_VariableIntegerValue( "developer" ) ) {
		Cvar_Set( "cheats", !Q_stricmp( Cmd_Argv( 0 ), "devmap" ) ? "1" : "0" );
	}
	/* This engine's own cheat protection reads sv_cheats; it follows Alice's. */
	Cvar_Set( "sv_cheats", Cvar_VariableIntegerValue( "cheats" ) ? "1" : "0" );
	SV_AliceWipeSaveGame( alicesave::CurrentName );
	Cvar_Set( "nextmap", "" );
	SV_AliceStopCinematic();
	SV_SpawnServer( server, qfalse );
}

/* gamemap <map> (0x00450ce0), the level change the game module asks for
 * (G_ExitLevel, fgamex86 0x100a4da0): the persistent data goes to
 * current.spv, unless the map begins with `*`, which wipes the `current`
 * archive instead. Retail passes that `*` on to the spawn; so does this. After
 * the spawn a listen server queues its autosave. */
void SV_AliceGameMap_f() {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "USAGE: gamemap <map>\n" );
		return;
	}
	char server[MAX_QPATH];
	Q_strncpyz( server, Cmd_Argv( 1 ), sizeof( server ) );
	Com_DPrintf( "SV_GameMap(%s)\n", server );
	/* Not retail's: it has no such test and would drop inside the spawn,
	 * after the level it left was gone. */
	char path[MAX_QPATH];
	if ( !SV_AliceMapExists( server, path ) ) {
		Com_Printf( "Can't find map %s\n", path );
		return;
	}
	Q_strncpyz( aliceGameName, alicesave::CurrentName, sizeof( aliceGameName ) );
	if ( server[0] == '*' ) {
		SV_AliceWipeSaveGame( alicesave::CurrentName );
	} else if ( sv.state == SS_GAME ) {
		SV_AliceArchivePersistantFile( false );
	}
	SV_SpawnServer( server, qfalse );
	if ( !com_dedicated->integer ) {
		Cbuf_AddText( alicesave::AutosaveCommand );
	}
}

/* restart (0x00450d80): the current level again, from its whole `map`
 * argument, and never twice in one frame. Retail keeps the module and its
 * clients when the map is the same (its reuse path); this engine always
 * reloads, which is retail's full path. */
void SV_AliceRestart_f() {
	if ( com_frameTime == sv.serverId ) {
		return;
	}
	if ( !com_sv_running || !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	char server[alicesave::NameBytes];
	Q_strncpyz( server, aliceRawServerName, sizeof( server ) );
	SV_SpawnServer( server, qfalse );
}

/* pause (0x00439800): unpause if paused, otherwise pause a local game that is
 * not a deathmatch. This engine pauses its local server through cl_paused,
 * so that follows `paused`, as the engine UI's own toggle does. */
void SV_AlicePause_f() {
	if ( Cvar_VariableIntegerValue( "paused" ) ) {
		Cvar_Set( "paused", "0" );
		Cvar_Set( "cl_paused", "0" );
	} else if ( !Cvar_VariableIntegerValue( "deathmatch" ) && com_sv_running && com_sv_running->integer ) {
		Cvar_Set( "paused", "1" );
		Cvar_Set( "cl_paused", "1" );
	}
}

} // namespace

/*
====================
SV_AliceAddCommands

Alice's own map, level-change and savegame commands, which take the place of
the Quake III map set (SV_AddOperatorCommands 0x00450dd0), and the cvars they
read. It has no spmap, spdevmap or map_restart; `restart` is its reload.
====================
*/
void SV_AliceAddCommands( void ) {
	/* `config` names the savegames and the configuration file. Retail keeps
	 * it in the registry and defaults it to "config.cfg" (0x004398ff..
	 * 0x004399c8); fnTech3 has no registry, and Com_SelectRitualConfig has
	 * already set it at startup. This covers a cvar emptied since. */
	cvar_t *config = Cvar_Get( "config", "", 0 );
	if ( !config->string[0] ) {
		const char *profileConfig = Com_GameProfile()->clientConfig;
		Cvar_Set( "config", profileConfig && profileConfig[0] ? profileConfig : "config.cfg" );
	}
	Cvar_Get( "mapdir", "", 0 );
	Cvar_Get( "paused", "0", CVAR_ROM );
	Cvar_Get( "deathmatch", "0", 0 );
	Cvar_Get( "cheats", "1", CVAR_SYSTEMINFO | CVAR_ROM );

	/* The Quake III reload has no Alice equivalent and cannot restart a
	 * function-table module; `restart` below is Alice's. */
	Cmd_RemoveCommand( "map_restart" );

	Cmd_AddCommand( "map", SV_AliceMap_f );
	Cmd_AddCommand( "devmap", SV_AliceMap_f );
	Cmd_AddCommand( "gamemap", SV_AliceGameMap_f );
	Cmd_AddCommand( "restart", SV_AliceRestart_f );
	Cmd_AddCommand( "savegame", SV_AliceSaveGame_f );
	Cmd_AddCommand( "autosavegame", SV_AliceAutosaveGame_f );
	Cmd_AddCommand( "loadgame", SV_AliceLoadGame_f );
	Cmd_AddCommand( "loadbrokengame", SV_AliceLoadBrokenGame_f );
	Cmd_AddCommand( "pause", SV_AlicePause_f );
	Cmd_AddCommand( "alicearchive", SV_AliceArchiveProbe_f );

	Q_strncpyz( aliceGameName, alicesave::CurrentName, sizeof( aliceGameName ) );
}

/*
====================
SV_AlicePrepareSpawn

The start of retail's SV_SpawnServer (0x00454ff0..0x00455097): unpause, keep
the whole argument, point `nextmap` at it, and name the BSP by the part before
any `$`. Returns the BSP name in `bsp`.
====================
*/
const char *SV_AlicePrepareSpawn( const char *server, char *bsp, int size ) {
	if ( Cvar_VariableIntegerValue( "paused" ) ) {
		Cvar_Set( "paused", "0" );
	}
	Q_strncpyz( aliceRawServerName, server, sizeof( aliceRawServerName ) );
	Cvar_Set( "nextmap", va( "map %s", server ) );
	SV_AliceBspName( server, bsp, size );
	Q_strncpyz( aliceMapName, bsp, sizeof( aliceMapName ) );

	/* A load of the BSP the server is running, with the game module loaded,
	 * is retail's reuse path (0x004550a1..0x004550db): it skips the full
	 * spawn's wipe of the server state, so the configstrings stay as they
	 * were (0x00455171..0x004551bb are passed over) and the model table with
	 * them (0x004551de). The level archive keeps an entity's model index as a
	 * number and names it again only for an entity with a model name
	 * (fgamex86 G_ArchiveEdict 0x100ad47b), so a model index read back means
	 * the model it meant only while the model configstrings keep their
	 * places. This engine reloads the module on every spawn; what it keeps is
	 * the configstrings, which SV_AliceRestoreConfigstrings puts back after
	 * the spawn clears them, and the model table built from them. */
	aliceKeptConfigstrings.clear();
	if ( aliceLoadPending && gvm && aliceSpawnedBsp[0] && !Q_stricmp( bsp, aliceSpawnedBsp ) ) {
		const int count = Com_GameConfigstringLimit();
		aliceKeptConfigstrings.reserve( static_cast<std::size_t>( count ) );
		for ( int i = 0; i < count; ++i ) {
			aliceKeptConfigstrings.emplace_back( sv.configstrings[i] ? sv.configstrings[i] : "" );
		}
	}
	Q_strncpyz( aliceSpawnedBsp, bsp, sizeof( aliceSpawnedBsp ) );
	return bsp;
}

/*
====================
SV_AliceRestoreConfigstrings

After the spawn has cleared the server state: the configstrings a load of the
running BSP kept (SV_AlicePrepareSpawn), and a model table registered from
them when the module loads. Nothing is kept for any other spawn.
====================
*/
void SV_AliceRestoreConfigstrings( void ) {
	aliceModelTableKept = !aliceKeptConfigstrings.empty();
	const int count = std::min( static_cast<int>( aliceKeptConfigstrings.size() ),
		Com_GameConfigstringLimit() );
	for ( int i = 0; i < count; ++i ) {
		SV_ZFree( sv.configstrings[i] );
		sv.configstrings[i] = CopyString( aliceKeptConfigstrings[static_cast<std::size_t>( i )].c_str() );
	}
	if ( count ) {
		Com_DPrintf( "Alice load of the running level keeps its %i configstrings\n", count );
	}
	aliceKeptConfigstrings.clear();
}

/*
====================
SV_AliceFlushModels

Empty the process-wide model definition cache (code/qcommon/alice_tiki.cpp).
SV_SpawnServer calls it once the old game, the client game and the renderer
are all shut down -- everything that holds a handle into the cache -- and
before the new game registers a model, so a long session cannot fill the
cache's 384 slots with levels it has left.
====================
*/
void SV_AliceFlushModels( void ) {
	const int count = AliceTiki_Count();

	AliceTiki_Clear();
	std::memset( aliceModelTiki, 0, sizeof( aliceModelTiki ) );
	Com_DPrintf( "%s model cache flushed: %i definitions\n", SV_RitualName(), count );
}

/*
====================
SV_AliceRestoreLevelTime

A load restores the level clock the save recorded before the module's Init
sees it, as retail's loadgame sets it before its spawn (0x00451a2a).
====================
*/
void SV_AliceRestoreLevelTime( void ) {
	if ( aliceLoadPending ) {
		sv.time = aliceLoadLevelTime;
	}
}

/*
====================
SV_AliceSpawnLevel

The level's own part of retail's SV_SpawnServer, after the module's Init:
Cleanup (0x004551cf); the soundtrack configstring cleared unless loading
(0x00455276); then either the level read back from a save, or -- fresh, or a
load the module refused -- the level clock reset, the map's entities spawned
and the persistent data read back (0x004552a4..0x0045530e). True when the
level came from a save, which runs no settle frames.
====================
*/
qboolean SV_AliceSpawnLevel( void ) {
	const bool loading = aliceLoadPending;

	SV_AliceCallCleanup();
	if ( !loading ) {
		SV_SetConfigstring( 8, "none" );
	}
	if ( loading && SV_AliceArchiveLevelFile( true ) ) {
		aliceLastLevelTime = sv.time;
		Com_Printf( "Alice level read back from %s at level time %i\n", aliceGameName, sv.time );
		return qtrue;
	}
	sv.time = 0;
	aliceLastLevelTime = 0;
	SV_AliceCallSpawnEntities();
	SV_AliceArchivePersistantFile( true );
	return qfalse;
}

/*
====================
SV_AliceSettle

The three frames a fresh level settles with, at 0, 100 and 200 with 100 ms
each, leaving the clock at 300 (0x00455316..0x00455361); none for a level
read back from a save.
====================
*/
void SV_AliceSettle( qboolean restored ) {
	if ( restored ) {
		return;
	}
	for ( int i = 0; i < 3; ++i ) {
		SV_AliceSettleFrame( sv.time );
		sv.time += 100;
	}
}

/*
====================
SV_AliceSpawnDone

The end of every spawn (0x0045557e): the archive name back to `current`.
====================
*/
void SV_AliceSpawnDone( void ) {
	Q_strncpyz( aliceGameName, alicesave::CurrentName, sizeof( aliceGameName ) );
	aliceLoadPending = false;
}

/*
====================
SV_AliceSaveAbort

An error unwinding a load or a save leaves no load pending and the archive
name at `current`.
====================
*/
void SV_AliceSaveAbort( void ) {
	aliceLoadPending = false;
	aliceKeptConfigstrings.clear();
	aliceModelTableKept = false;
	Q_strncpyz( aliceGameName, alicesave::CurrentName, sizeof( aliceGameName ) );
}

/*
====================
SV_AliceSaveError

SV_SaveGame's last result, for the load/save menu's CheckErrorState
(0x00419ff0), which reads it and, having shown it, clears it: 0 none, 1 disk
full, 2 deathmatch, 3 not in a game, 4 dead, 5 a bad name.
====================
*/
int SV_AliceSaveError( qboolean clear ) {
	const int error = aliceSaveError;
	if ( clear ) {
		aliceSaveError = 0;
	}
	return error;
}

/*
====================
SV_AliceArchiveFileName

An archive's path, "save/<config>_<name>.<ext>", for the load/save menu; the
name may be "*" to list them. False, with an empty path, for a name or a
config the rules refuse or a path that would not fit.
====================
*/
qboolean SV_AliceArchiveFileName( const char *name, const char *extension, char *path, int size ) {
	char built[alicesave::PathBytes];

	if ( !path || size <= 0 ) {
		return qfalse;
	}
	path[0] = '\0';
	if ( !SV_AliceArchivePath( name, extension, built ) || strlen( built ) >= static_cast<std::size_t>( size ) ) {
		return qfalse;
	}
	Q_strncpyz( path, built, size );
	return qtrue;
}

#endif
