/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_STVEF_SP_SAVE_HPP
#define FNTECH3_SERVER_STVEF_SP_SAVE_HPP
#include "../compat/stvef_sp_savegame.hpp"

#include <vector>

/*
 * Elite Force SP's savegames (stvoy.exe 0x426760 to 0x427e70). A save is a
 * run of chunks in saves/<name>.sav: the engine writes a comment, a picture
 * of 256 pixels square, the map, the userinfo cvars and what the player
 * carries, and for a full save the level's time, its area portals and its
 * configstrings; the game's own WriteLevel follows. Reading one spawns the
 * map between the engine's chunks and the game's, as retail does, so the
 * level is standing when the game reads it back.
 *
 * Every save is written to "current" and renamed once it is whole, so a
 * failure leaves the save that was there.
 */

namespace {

namespace savegame = fntech3::stvef::savegame;

// The save being written, and whether it is a real save: sv_testsave runs the
// game's saving code without a file, which a real save overrides (0x427373).
fileHandle_t stvefSpSaveFile = 0;
bool stvefSpSaveWriting = false;
bool stvefSpSaveFailed = false;
cvar_t *stvefSpTestSave = nullptr;

/* A save being read, held whole. Retail reads it from an open file, but the
 * map spawns in the middle of the read, which clears the hunk and may take
 * the file's handle with it. */
struct STVEFSPSaveReader {
	std::vector<unsigned char> bytes;
	std::size_t offset = 0;
	// The menus look at saves that may be another version's, so theirs are
	// checked rather than dropped (retail's flag at 0xa94570).
	bool checking = false;
};

STVEFSPSaveReader stvefSpSaveReader;
// The comment the next save is written with, and the last save read, which
// "load *respawn" falls back to.
char stvefSpSaveComment[savegame::CommentBytes];
char stvefSpSaveLastRead[MAX_QPATH];
// What a menu last asked a save for.
char stvefSpSaveReadComment[savegame::CommentBytes];
char stvefSpSaveReadMap[savegame::MapNameBytes];

const char *SV_STVEFSPSavePath( const char *name ) {
	static char paths[8][MAX_QPATH * 2];
	static int index;
	index = ( index + 1 ) & 7;
	Com_sprintf( paths[index], sizeof( paths[index] ), "saves/%s.sav", name ? name : "" );
	return paths[index];
}

void SV_STVEFSPWipeSave( const char *name ) { FS_HomeRemove( SV_STVEFSPSavePath( name ) ); }

qboolean SV_STVEFSPCloseWrite() {
	qboolean success = stvefSpSaveWriting && stvefSpSaveFile && !stvefSpSaveFailed ? qtrue : qfalse;
	if ( stvefSpSaveFile ) {
		if ( !FS_FCloseFileWrite( stvefSpSaveFile ) ) success = qfalse;
		stvefSpSaveFile = 0;
	}
	stvefSpSaveWriting = false;
	stvefSpSaveFailed = false;
	return success;
}

qboolean SV_STVEFSPCreateSave( const char *name ) {
	stvefSpSaveFailed = false;
	SV_STVEFSPWipeSave( name );
	const char *path = SV_STVEFSPSavePath( name );
	stvefSpSaveFile = FS_FOpenFileWrite( path );
	if ( !stvefSpSaveFile ) {
		Com_Printf( "^1Failed to create new savegame file \"%s\"\n", path );
		return qfalse;
	}
	return qtrue;
}

qboolean SV_STVEFSPOpenSave( const char *name, STVEFSPSaveReader &reader ) {
	const char *path = SV_STVEFSPSavePath( name );
	fileHandle_t file = 0;
	reader.bytes.clear();
	reader.offset = 0;
	const int length = FS_FOpenFileRead( path, &file, qtrue );
	if ( !file || length < 0 ) {
		if ( file ) {
			FS_FCloseFile( file );
		}
		Com_DPrintf( "Failed to open savegame file %s\n", path );
		return qfalse;
	}
	reader.bytes.resize( static_cast<std::size_t>( length ) );
	const int read = length > 0 ? FS_Read( reader.bytes.data(), length, file ) : 0;
	FS_FCloseFile( file );
	if ( read != length ) {
		reader.bytes.clear();
		Com_DPrintf( "Failed to open savegame file %s\n", path );
		return qfalse;
	}
	return qtrue;
}

void SV_STVEFSPCloseRead( STVEFSPSaveReader &reader ) {
	reader.bytes.clear();
	reader.bytes.shrink_to_fit();
	reader.offset = 0;
	reader.checking = false;
}

/* One chunk written: its id, its data's length and the data's checksum, the
 * data itself and the magic (SG_Append 0x427a40). */
std::int32_t SV_STVEFSPAppendChunk( std::uint32_t id, const void *data, std::int32_t length ) {
	char name[5];
	savegame::Name( id, name );
	Com_DPrintf( "Attempting write of chunk %s length %d\n", name, length );
	if ( !stvefSpSaveWriting && stvefSpTestSave && stvefSpTestSave->value != 0.0f ) {
		// The game's saving code is being run for its own sake.
		return qtrue;
	}
	// Engine writers and the retail game's WriteLevel may ignore an import's
	// return value. Once any chunk fails, never publish this transaction.
	if ( stvefSpSaveFailed ) return qfalse;
	if ( !stvefSpSaveFile || length < 0 ||
		length > INT_MAX - static_cast<int>( savegame::FrameBytes ) || ( length > 0 && !data ) ) {
		stvefSpSaveFailed = true;
		Com_Printf( "^1Failed to write %s chunk\n", name );
		return qfalse;
	}
	static const unsigned char nothing = 0;
	const void *bytes = length > 0 ? data : &nothing;
	unsigned char header[12], trailer[4];
	savegame::PutFrameHeader( id, static_cast<std::uint32_t>( length ),
		static_cast<std::uint32_t>( Com_BlockChecksum( bytes, length ) ), header );
	savegame::PutFrameTrailer( trailer );
	if ( FS_Write( header, sizeof( header ), stvefSpSaveFile ) != sizeof( header ) ||
		( length > 0 && FS_Write( data, length, stvefSpSaveFile ) != length ) ||
		FS_Write( trailer, sizeof( trailer ), stvefSpSaveFile ) != sizeof( trailer ) ) {
		stvefSpSaveFailed = true;
		Com_Printf( "^1Failed to write %s chunk\n", name );
		return qfalse;
	}
	return qtrue;
}

/* One chunk read, by id and, unless any length will do, by length. Without an
 * address the chunk is handed over in memory of its own, which the caller
 * frees the way the game does. A chunk that is not the one asked for is an
 * error unless the read was optional or the save is only being checked
 * (SG_Read 0x427b60). */
std::int32_t SV_STVEFSPReadChunk( STVEFSPSaveReader &reader, std::uint32_t id, void *address,
	std::int32_t length, void **addressPointer, bool optional ) {
	char name[5], found[5];
	savegame::Name( id, name );
	Com_DPrintf( "Attempting read of chunk %s length %d\n", name, length );
	savegame::Chunk chunk{};
	const savegame::ReadResult result = savegame::ReadChunk( reader.bytes.data(), reader.bytes.size(),
		reader.offset, id, length < 0 ? 0 : static_cast<std::uint32_t>( length ), chunk,
		[]( const unsigned char *bytes, std::uint32_t size ) {
			return static_cast<std::uint32_t>( Com_BlockChecksum( bytes, static_cast<int>( size ) ) );
		} );
	savegame::Name( chunk.id, found );
	if ( result != savegame::ReadResult::Ok ) {
		if ( reader.checking || ( optional && result == savegame::ReadResult::WrongId ) ) {
			return 0;
		}
		switch ( result ) {
		case savegame::ReadResult::WrongId:
			Com_Error( ERR_DROP, "Loaded chunk ID (%s) does not match requested chunk ID (%s)", found, name );
			break;
		case savegame::ReadResult::WrongLength:
			Com_Error( ERR_DROP, "Loaded chunk (%s) has different length than requested", name );
			break;
		case savegame::ReadResult::BadChecksum:
			Com_Error( ERR_DROP, "Failed checksum check for chunk" );
			break;
		case savegame::ReadResult::BadMagic:
			Com_Error( ERR_DROP, "Bad savegame magic for chunk %s", name );
			break;
		default:
			Com_Error( ERR_DROP, "Error during loading chunk %s", name );
			break;
		}
		return 0;
	}
	reader.offset = chunk.next;
	if ( address ) {
		std::memcpy( address, chunk.data, chunk.length );
	} else if ( addressPointer ) {
		// One byte more than retail, always zero, so that a chunk of text
		// from a damaged save still ends.
		void *buffer = Z_Malloc( static_cast<int>( chunk.length ) + 1 );
		std::memcpy( buffer, chunk.data, chunk.length );
		*addressPointer = buffer;
	}
	return static_cast<std::int32_t>( chunk.length );
}

// The game's three savegame imports.
std::int32_t SV_STVEFSPAppendToSave( std::uint32_t chunk, void *data, std::int32_t length ) {
	return SV_STVEFSPAppendChunk( chunk, data, length );
}
std::int32_t SV_STVEFSPReadFromSave( std::uint32_t chunk, void *address, std::int32_t length,
	void **addressPointer ) {
	return SV_STVEFSPReadChunk( stvefSpSaveReader, chunk, address, length, addressPointer, false );
}
std::int32_t SV_STVEFSPReadFromSaveOptional( std::uint32_t chunk, void *address, std::int32_t length,
	void **addressPointer ) {
	return SV_STVEFSPReadChunk( stvefSpSaveReader, chunk, address, length, addressPointer, true );
}

/* The level a save belongs to, as its serverinfo names it. */
const char *SV_STVEFSPMapName() {
	const char *serverinfo = sv.configstrings[CS_SERVERINFO] ? sv.configstrings[CS_SERVERINFO] : "";
	const char *mapname = Info_ValueForKey( serverinfo, "mapname" );
	return mapname[0] ? mapname : "<unknown>";
}

/* An autosave is labelled with the level it starts; any other save without a
 * comment of its own with the time, the date and the level (0x426f40). */
void SV_STVEFSPWriteComment( qboolean autosave, const char *map ) {
	// Retail clears the whole chunk before writing into it (0x426f72).
	char comment[savegame::CommentBytes]{};
	qtime_t now;
	Com_RealTime( &now );
	const int year = ( now.tm_year + 1900 ) % 100;
	if ( autosave ) {
		savegame::AutosaveComment( map, comment );
	} else {
		Q_strncpyz( comment, stvefSpSaveComment, sizeof( comment ) );
		if ( !comment[0] ) {
			savegame::DatedComment( now.tm_hour, now.tm_min, now.tm_sec, now.tm_mon + 1, now.tm_mday,
				year, map, comment );
		}
	}
	// The menus list a save by the time, the date and the level from byte 64.
	savegame::CommentInfo( now.tm_hour, now.tm_min, now.tm_sec, now.tm_mon + 1, now.tm_mday, year, map, comment );
	SV_STVEFSPAppendChunk( savegame::Comment, comment, sizeof( comment ) );
	Com_DPrintf( "Saving: current (%s)\n", comment );
}

/* A save keeps the screen as it was, except that an autosave keeps the
 * level's own picture where it has one (0x427690). */
void SV_STVEFSPWriteScreenshot( qboolean autosave, const char *map ) {
	static unsigned char pixels[savegame::ScreenshotBytes];
#ifndef DEDICATED
	if ( autosave && CL_STVEFSPLevelshot( map, pixels ) ) {
		SV_STVEFSPAppendChunk( savegame::Screenshot, pixels, sizeof( pixels ) );
		return;
	}
	SV_STVEFSPAppendChunk( savegame::Screenshot, CL_STVEFSPScreenshotPixels(), sizeof( pixels ) );
#else
	(void)autosave;
	(void)map;
	SV_STVEFSPAppendChunk( savegame::Screenshot, pixels, sizeof( pixels ) );
#endif
}

void SV_STVEFSPCountCvar( const char *, const char *, void *context ) {
	++*static_cast<std::int32_t *>( context );
}

void SV_STVEFSPWriteCvar( const char *name, const char *string, void * ) {
	SV_STVEFSPAppendChunk( savegame::CvarName, name, static_cast<std::int32_t>( std::strlen( name ) ) + 1 );
	SV_STVEFSPAppendChunk( savegame::CvarValue, string, static_cast<std::int32_t>( std::strlen( string ) ) + 1 );
}

/* The userinfo cvars, counted and then written as name and value
 * (SG_WriteCvars 0x426c90). */
void SV_STVEFSPWriteCvars() {
	std::int32_t count = 0;
	Cvar_ForEachWithFlags( CVAR_USERINFO, SV_STVEFSPCountCvar, &count );
	SV_STVEFSPAppendChunk( savegame::CvarCount, &count, sizeof( count ) );
	Cvar_ForEachWithFlags( CVAR_USERINFO, SV_STVEFSPWriteCvar, nullptr );
}

void SV_STVEFSPReadCvars( STVEFSPSaveReader &reader ) {
	std::int32_t count = 0;
	SV_STVEFSPReadChunk( reader, savegame::CvarCount, &count, sizeof( count ), nullptr, false );
	for ( std::int32_t i = 0; i < count; ++i ) {
		char *name = nullptr;
		char *string = nullptr;
		SV_STVEFSPReadChunk( reader, savegame::CvarName, nullptr, 0, reinterpret_cast<void **>( &name ), false );
		SV_STVEFSPReadChunk( reader, savegame::CvarValue, nullptr, 0, reinterpret_cast<void **>( &string ), false );
		if ( name && string ) {
			Cvar_Set( name, string );
		}
		if ( name ) Z_Free( name );
		if ( string ) Z_Free( string );
	}
}

/* Every configstring that holds anything, except the system info, which
 * belongs to the server that is running (0x426da0, 0x426e70). */
void SV_STVEFSPWriteConfigstrings() {
	const int limit = Com_GameConfigstringLimit();
	std::int32_t count = 0;
	for ( int i : SV_Indices( limit ) ) {
		if ( i != savegame::SkippedConfigstring && sv.configstrings[i] && sv.configstrings[i][0] ) {
			++count;
		}
	}
	SV_STVEFSPAppendChunk( savegame::ConfigstringCount, &count, sizeof( count ) );
	for ( int i : SV_Indices( limit ) ) {
		if ( i == savegame::SkippedConfigstring || !sv.configstrings[i] || !sv.configstrings[i][0] ) {
			continue;
		}
		const std::int32_t index = i;
		SV_STVEFSPAppendChunk( savegame::ConfigstringIndex, &index, sizeof( index ) );
		SV_STVEFSPAppendChunk( savegame::ConfigstringData, sv.configstrings[i],
			static_cast<std::int32_t>( std::strlen( sv.configstrings[i] ) ) + 1 );
	}
}

void SV_STVEFSPReadConfigstrings( STVEFSPSaveReader &reader ) {
	const int limit = Com_GameConfigstringLimit();
	for ( int i : SV_Indices( limit ) ) {
		if ( i != savegame::SkippedConfigstring ) {
			SV_SetConfigstring( i, "" );
		}
	}
	std::int32_t count = 0;
	SV_STVEFSPReadChunk( reader, savegame::ConfigstringCount, &count, sizeof( count ), nullptr, false );
	Com_DPrintf( "Reading %d configstrings...\n", count );
	for ( std::int32_t i = 0; i < count; ++i ) {
		std::int32_t index = 0;
		char *text = nullptr;
		SV_STVEFSPReadChunk( reader, savegame::ConfigstringIndex, &index, sizeof( index ), nullptr, false );
		SV_STVEFSPReadChunk( reader, savegame::ConfigstringData, nullptr, 0,
			reinterpret_cast<void **>( &text ), false );
		if ( index < 0 || index >= limit ) {
			if ( text ) Z_Free( text );
			Com_Error( ERR_DROP, "Elite Force SP savegame holds configstring %i", index );
			return;
		}
		Com_DPrintf( "Cfg str %d = %s\n", index, text ? text : "" );
		SV_SetConfigstring( index, text ? text : "" );
		if ( text ) Z_Free( text );
	}
}

/* The area portals, and the flood they feed once they are back (0x40b8b0). */
void SV_STVEFSPWritePortals() {
	int count = 0;
	const int *portals = CM_AreaPortalCounts( &count );
	if ( count <= 0 || !portals ) {
		return;
	}
	SV_STVEFSPAppendChunk( savegame::Portals, portals, count * static_cast<std::int32_t>( sizeof( int ) ) );
}

void SV_STVEFSPReadPortals( STVEFSPSaveReader &reader ) {
	int count = 0;
	int *portals = CM_AreaPortalCounts( &count );
	if ( count <= 0 || !portals ) {
		return;
	}
	SV_STVEFSPReadChunk( reader, savegame::Portals, portals,
		count * static_cast<std::int32_t>( sizeof( int ) ), nullptr, false );
	CM_FloodAreaConnections();
}

/* Retail SV_Player_EndOfLevelSave (0x423ea0): what the player carries into
 * the next level, kept in cvars for the game to read back. */
void SV_STVEFSPPlayerEndOfLevelSave() {
	if ( !SV_IsClientIndex( 0 ) ) {
		return;
	}
	const auto *entity = reinterpret_cast<const fntech3_stvef_sp_gentity_t *>( SV_ClientForIndex( 0 ).gentity );
	if ( !entity || !entity->client ) {
		return;
	}
	const fntech3_stvef_sp_playerState_t &player = *entity->client;
	char text[MAX_STRING_CHARS], name[MAX_QPATH];
	Cvar_Set( "playersave", "" );
	Com_sprintf( text, sizeof( text ), "%i %i %i %i %i %f %f %f",
		player.stats[FNTECH3_STVEF_SP_STAT_HEALTH], player.stats[FNTECH3_STVEF_SP_STAT_ARMOR],
		player.stats[FNTECH3_STVEF_SP_STAT_WEAPONS], player.weapon, player.weaponstate,
		player.viewangles[0], player.viewangles[1], player.viewangles[2] );
	Cvar_Set( "playersave", text );
	for ( int i : SV_Indices( FNTECH3_STVEF_SP_MAX_AMMO ) ) {
		Com_sprintf( name, sizeof( name ), "playerammo%d", i );
		Com_sprintf( text, sizeof( text ), "%i", player.ammo[i] );
		Cvar_Set( name, text );
	}
	for ( int i : SV_Indices( FNTECH3_STVEF_SP_MAX_WEAPONS ) ) {
		Com_sprintf( name, sizeof( name ), "borgadapt%d", i );
		Com_sprintf( text, sizeof( text ), "%i", player.borgAdaptHits[i] );
		Cvar_Set( name, text );
	}
}

/* Whether the save was made as a level began, and for one that was, what the
 * player carried into it (WriteGame 0x427580, ReadGame 0x4278c0). */
void SV_STVEFSPWriteGame( qboolean autosave ) {
	const std::int32_t flag = autosave ? 1 : 0;
	SV_STVEFSPAppendChunk( savegame::Autosave, &flag, sizeof( flag ) );
	if ( !flag ) {
		return;
	}
	SV_STVEFSPPlayerEndOfLevelSave();
	char carried[savegame::CarriedTextBytes], name[MAX_QPATH];
	std::memset( carried, 0, sizeof( carried ) );
	Cvar_VariableStringBuffer( "playersave", carried, sizeof( carried ) );
	SV_STVEFSPAppendChunk( savegame::PlayerSave, carried, sizeof( carried ) );
	for ( int i : SV_Indices( savegame::PlayerAmmoCount ) ) {
		Com_sprintf( name, sizeof( name ), "playerammo%d", i );
		std::memset( carried, 0, sizeof( carried ) );
		Cvar_VariableStringBuffer( name, carried, sizeof( carried ) );
		SV_STVEFSPAppendChunk( savegame::PlayerAmmo, carried, sizeof( carried ) );
	}
	for ( int i : SV_Indices( savegame::BorgAdaptCount ) ) {
		Com_sprintf( name, sizeof( name ), "borgadapt%d", i );
		std::memset( carried, 0, sizeof( carried ) );
		Cvar_VariableStringBuffer( name, carried, sizeof( carried ) );
		SV_STVEFSPAppendChunk( savegame::BorgAdapt, carried, sizeof( carried ) );
	}
}

qboolean SV_STVEFSPReadGame( STVEFSPSaveReader &reader ) {
	std::int32_t flag = 0;
	SV_STVEFSPReadChunk( reader, savegame::Autosave, &flag, sizeof( flag ), nullptr, false );
	if ( !flag ) {
		return qfalse;
	}
	char carried[savegame::CarriedTextBytes], name[MAX_QPATH];
	SV_STVEFSPReadChunk( reader, savegame::PlayerSave, carried, sizeof( carried ), nullptr, false );
	carried[sizeof( carried ) - 1] = '\0';
	Cvar_Set( "playersave", carried );
	for ( int i : SV_Indices( savegame::PlayerAmmoCount ) ) {
		SV_STVEFSPReadChunk( reader, savegame::PlayerAmmo, carried, sizeof( carried ), nullptr, false );
		carried[sizeof( carried ) - 1] = '\0';
		Com_sprintf( name, sizeof( name ), "playerammo%d", i );
		Cvar_Set( name, carried );
	}
	for ( int i : SV_Indices( savegame::BorgAdaptCount ) ) {
		SV_STVEFSPReadChunk( reader, savegame::BorgAdapt, carried, sizeof( carried ), nullptr, false );
		carried[sizeof( carried ) - 1] = '\0';
		Com_sprintf( name, sizeof( name ), "borgadapt%d", i );
		Cvar_Set( name, carried );
	}
	return qtrue;
}

/* The player's health, as the save commands ask for it. */
int SV_STVEFSPPlayerHealth() {
	playerState_t player{};
	if ( !SV_ReadGameClient( 0, &player ) ) {
		return 0;
	}
	return player.stats[FNTECH3_STVEF_SP_STAT_HEALTH];
}

qboolean SV_STVEFSPMoveSave( const char *from, const char *to ) {
	if ( FS_HomeReplaceFile( SV_STVEFSPSavePath( from ), SV_STVEFSPSavePath( to ) ) ) {
		return qtrue;
	}
	Com_Printf( "^1Error during savegame-rename. Check \"%s\" for write-protect!\n", SV_STVEFSPSavePath( to ) );
	return qfalse;
}

/* A whole save written, then put in place of the one it replaces
 * (SG_WriteSavegame 0x427350). writeFailed distinguishes a failed transaction
 * from a deliberate game refusal, which hub transitions may continue past. */
qboolean SV_STVEFSPWriteSavegame( const char *name, qboolean autosave, qboolean *writeFailed = nullptr ) {
	if ( writeFailed ) *writeFailed = qfalse;
	if ( !stvefSpGame || stvefSpSaveWriting ) {
		Com_Printf( "^1Cannot start savegame while its game writer is unavailable\n" );
		return qfalse;
	}
	if ( !autosave && !SV_STVEFSPGameAllowedToSaveHere( qfalse ) ) {
		return qfalse;
	}
	if ( writeFailed ) *writeFailed = qtrue;
	if ( !SV_STVEFSPCreateSave( "current" ) ) {
		Com_Printf( "^1Failed to create savegame\n" );
		SV_STVEFSPWipeSave( "current" );
		return qfalse;
	}
	stvefSpSaveWriting = true;
	char map[savegame::MapNameBytes];
	std::memset( map, 0, sizeof( map ) );
	Q_strncpyz( map, SV_STVEFSPMapName(), sizeof( map ) );
	SV_STVEFSPWriteComment( autosave, map );
	SV_STVEFSPWriteScreenshot( autosave, map );
	SV_STVEFSPAppendChunk( savegame::MapName, map, sizeof( map ) );
	SV_STVEFSPWriteCvars();
	SV_STVEFSPWriteGame( autosave );
	if ( !autosave ) {
		SV_STVEFSPAppendChunk( savegame::Time, &sv.time, sizeof( sv.time ) );
		SV_STVEFSPAppendChunk( savegame::TimeResidual, &sv.timeResidual, sizeof( sv.timeResidual ) );
		SV_STVEFSPWritePortals();
		SV_STVEFSPWriteConfigstrings();
	}
	if ( stvefSpGame ) {
		stvefSpGame->WriteLevel( autosave );
	}
	if ( !SV_STVEFSPCloseWrite() ) {
		Com_Printf( "^1Failed to write savegame \"%s\"\n", name );
		SV_STVEFSPWipeSave( "current" );
		return qfalse;
	}
	if ( SV_STVEFSPMoveSave( "current", name ) ) {
		SV_STVEFSPWipeSave( "current" );
		if ( writeFailed ) *writeFailed = qfalse;
		return qtrue;
	}
	return qfalse;
}

/* A whole save read: the engine's chunks, the map spawned between them, and
 * the game's own level last of all (SG_ReadSavegame 0x427750). */
qboolean SV_STVEFSPReadSavegame( const char *name ) {
	STVEFSPSaveReader &reader = stvefSpSaveReader;
	if ( !SV_STVEFSPOpenSave( name, reader ) ) {
		Com_Printf( "^1Failed to open savegame \"%s\"\n", name );
		return qfalse;
	}
	if ( name != stvefSpSaveLastRead ) {
		Q_strncpyz( stvefSpSaveLastRead, name, sizeof( stvefSpSaveLastRead ) );
	}
	char comment[savegame::CommentBytes];
	SV_STVEFSPReadChunk( reader, savegame::Comment, comment, sizeof( comment ), nullptr, false );
	comment[sizeof( comment ) - 1] = '\0';
	Com_DPrintf( "Reading: %s\n", comment );
	{
		// The save's picture is what the level loads behind.
		static unsigned char pixels[savegame::ScreenshotBytes];
		SV_STVEFSPReadChunk( reader, savegame::Screenshot, pixels, sizeof( pixels ), nullptr, false );
#ifndef DEDICATED
		CL_STVEFSPSetScreenshot( pixels, savegame::ScreenshotSide, savegame::ScreenshotSide );
#endif
	}
	static char map[savegame::MapNameBytes];
	SV_STVEFSPReadChunk( reader, savegame::MapName, map, sizeof( map ), nullptr, false );
	map[sizeof( map ) - 1] = '\0';
	if ( !map[0] || std::strstr( map, ".." ) || std::strchr( map, '\\' ) ) {
		SV_STVEFSPCloseRead( reader );
		Com_Printf( "^1Failed to open savegame \"%s\"\n", name );
		return qfalse;
	}
	SV_STVEFSPReadCvars( reader );
	const qboolean autosave = SV_STVEFSPReadGame( reader );
	stvefSpSavedGameJustLoaded = autosave ? FNTECH3_STVEF_SP_SAVED_AUTO : FNTECH3_STVEF_SP_SAVED_FULL;
	SV_SpawnServer( map, qfalse );
	if ( !autosave ) {
		SV_STVEFSPReadChunk( reader, savegame::Time, &sv.time, sizeof( sv.time ), nullptr, false );
		SV_STVEFSPReadChunk( reader, savegame::TimeResidual, &sv.timeResidual, sizeof( sv.timeResidual ),
			nullptr, false );
		SV_STVEFSPReadPortals( reader );
		SV_STVEFSPReadConfigstrings( reader );
	}
	if ( stvefSpGame ) {
		stvefSpGame->ReadLevel( autosave, stvefSpLoadTransition );
	}
	SV_STVEFSPCloseRead( reader );
	return qtrue;
}

/* The save a level keeps while the player is away in the hub (0x426910). */
qboolean SV_STVEFSPReadHubSave( const char *map ) {
	char name[MAX_QPATH];
	Com_sprintf( name, sizeof( name ), "hub/%s", map );
	Com_Printf( "^5Restoring game \"%s\"...\n", name );
	if ( !SV_STVEFSPReadSavegame( name ) ) {
		return qfalse;
	}
	Com_Printf( "^5Done.\n" );
	return qtrue;
}

/* The map a save was made on, and whether a save is one this engine can read,
 * for the menus' lists (0x427200, 0x427190). */
const char *SV_STVEFSPSaveMapName( const char *name ) {
	STVEFSPSaveReader reader;
	reader.checking = true;
	if ( !SV_STVEFSPOpenSave( name, reader ) ) {
		return nullptr;
	}
	const char *map = nullptr;
	if ( SV_STVEFSPReadChunk( reader, savegame::Comment, nullptr, savegame::CommentBytes, nullptr, false )
		&& SV_STVEFSPReadChunk( reader, savegame::Screenshot, nullptr, savegame::ScreenshotBytes, nullptr, false )
		&& SV_STVEFSPReadChunk( reader, savegame::MapName, stvefSpSaveReadMap, sizeof( stvefSpSaveReadMap ),
			nullptr, false ) ) {
		stvefSpSaveReadMap[sizeof( stvefSpSaveReadMap ) - 1] = '\0';
		map = stvefSpSaveReadMap;
	}
	SV_STVEFSPCloseRead( reader );
	return map;
}

/* The player's models follow the "sex" cvar (0x423e20). */
void SV_STVEFSPPlayerModels() {
	const cvar_t *sex = Cvar_Get( "sex", "male", CVAR_ARCHIVE | CVAR_USERINFO );
	const bool male = sex && sex->string && sex->string[0] == 'm';
	Cvar_Set( "legsmodel", male ? "hazard/default" : "hazardfemale/default" );
	Cvar_Set( "torsomodel", male ? "hazard/default" : "hazardfemale/default" );
	Cvar_Set( "headmodel", male ? "munro/default" : "alexandria/default" );
}

/* A level started from the beginning (0x424040). The autosave of the level
 * before is dropped, unless a holodeck or the brig is being entered, whose
 * names begin with an underscore and which the player returns from. */
qboolean SV_STVEFSPStartMap( const char *map ) {
	if ( !map || !map[0] ) {
		return qfalse;
	}
	if ( std::strchr( map, '\\' ) ) {
		Com_Printf( "Can't have mapnames with a \\\n" );
		return qfalse;
	}
	char expanded[MAX_QPATH];
	Com_sprintf( expanded, sizeof( expanded ), "maps/%s.bsp", map );
	if ( FS_FOpenFileRead( expanded, nullptr, qfalse ) == -1 ) {
		Com_Printf( "Can't find map %s\n", expanded );
		return qfalse;
	}
	if ( map[0] != '_' ) {
		SV_STVEFSPWipeSave( "auto" );
	}
	SV_STVEFSPPlayerModels();
	SV_SpawnServer( map, qfalse );
	return qtrue;
}

void SV_STVEFSPMap_f() {
	char map[MAX_QPATH];
	Q_strncpyz( map, Cmd_Argv( 1 ), sizeof( map ) );
	const bool cheats = !Q_stricmp( Cmd_Argv( 0 ), "devmap" );
	stvefSpLoadTransition = 0;
	Cvar_Set( "playersave", "" );
	Cvar_Set( "spawntarget", "" );
#ifndef DEDICATED
	CL_STVEFSPUnprecacheScreenshot();
#endif
	SV_STVEFSPStartMap( map );
	Cvar_Set( "sv_cheats", cheats ? "1" : "0" );
}

/* The player walks into the next level, carrying what they have (0x4241c0). */
void SV_STVEFSPMapTransition_f() {
	char map[MAX_QPATH], target[MAX_QPATH];
	Q_strncpyz( map, Cmd_Argv( 1 ), sizeof( map ) );
	Q_strncpyz( target, Cmd_Argv( 2 ), sizeof( target ) );
	stvefSpLoadTransition = 0;
#ifndef DEDICATED
	CL_STVEFSPPrecacheScreenshot();
#endif
	SV_STVEFSPPlayerEndOfLevelSave();
	Cvar_Set( "spawntarget", target );
	SV_STVEFSPStartMap( map );
}

/* The player leaves a level for one they may have been in before: this level
 * is kept whole, and the other restored where it was left (0x423fc0). */
void SV_STVEFSPLoadTransition_f() {
	char map[MAX_QPATH], target[MAX_QPATH], name[MAX_QPATH];
	Q_strncpyz( map, Cmd_Argv( 1 ), sizeof( map ) );
	Q_strncpyz( target, Cmd_Argv( 2 ), sizeof( target ) );
	if ( !map[0] ) {
		return;
	}
	stvefSpLoadTransition = 1;
#ifndef DEDICATED
	CL_STVEFSPPrecacheScreenshot();
#endif
	SV_STVEFSPPlayerEndOfLevelSave();
	Com_sprintf( name, sizeof( name ), "hub/%s", Cvar_VariableString( "mapname" ) );
	// The game can deliberately disallow a save during a transition. Keep
	// that flow, but do not leave after an attempted write or rename failed.
	qboolean writeFailed;
	SV_STVEFSPWriteSavegame( name, qfalse, &writeFailed );
	if ( writeFailed ) {
		stvefSpLoadTransition = 0;
#ifndef DEDICATED
		CL_STVEFSPUnprecacheScreenshot();
#endif
		Com_Printf( "^1Cannot leave the level: its hub save failed\n" );
		return;
	}
	Cvar_Set( "spawntarget", target );
	if ( !SV_STVEFSPReadHubSave( map ) ) {
		SV_STVEFSPStartMap( map );
	}
}

/* The quick, automatic and holodeck saves take a fresh picture of the screen
 * and a comment of their own, and "quik" is the ship's holodeck save while
 * the player is aboard the virtual Voyager (0x426ad0). */
void SV_STVEFSPSave_f() {
	if ( !com_sv_running || !com_sv_running->integer ) {
		Com_Printf( "^1Server is not running\n" );
		return;
	}
	if ( sv.state != SS_GAME ) {
		Com_Printf( "^1You must be in a game to save.\n" );
		return;
	}
	if ( Cvar_VariableString( "mapname" )[0] == '_' ) {
		Com_Printf( "^1Cannot save on holodeck or brig.\n" );
		return;
	}
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "USAGE: \"save <filename>\"\n" );
		return;
	}
	if ( SV_STVEFSPPlayerHealth() <= 0 ) {
		Com_Printf( "^1\nCan't savegame while dead!\n" );
		return;
	}
	char name[MAX_QPATH];
	Q_strncpyz( name, Cmd_Argv( 1 ), sizeof( name ) );
	if ( !Q_stricmp( name, "current" ) ) {
		Com_Printf( "^1Can't save to 'current'\n" );
		return;
	}
	if ( !Q_stricmp( name, "quik*" ) && Cvar_VariableIntegerValue( "cg_virtualvoyager" ) ) {
		Q_strncpyz( name, "virtual", sizeof( name ) );
	} else if ( !Q_stricmp( name, "quik*" ) || !Q_stricmp( name, "auto*" )
		|| !Q_stricmp( name, "exitholodeck" ) ) {
#ifndef DEDICATED
		CL_STVEFSPPrecacheScreenshot();
#endif
		if ( name[4] == '*' ) {
			name[4] = '\0';
		}
		Q_strncpyz( stvefSpSaveComment, "", sizeof( stvefSpSaveComment ) );
	}
	if ( !savegame::NameAllowed( name ) ) {
		Com_Printf( "^1Bad savegame name.\n" );
		return;
	}
	Com_Printf( "^5Saving game \"%s\"...\n", name );
	if ( SV_STVEFSPWriteSavegame( name, qfalse ) ) {
		Com_Printf( "^5Done.\n" );
	}
}

/* A save read back. "*respawn" takes the level's own autosave, or the save
 * the player last loaded where that autosave belongs to another level
 * (0x426960). */
void SV_STVEFSPLoad_f() {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "USAGE: load <filename>\n" );
		return;
	}
	stvefSpLoadTransition = 0;
	char name[MAX_QPATH];
	Q_strncpyz( name, Cmd_Argv( 1 ), sizeof( name ) );
	if ( !savegame::NameAllowed( name ) ) {
		Com_Printf( "^1Bad loadgame name.\n" );
		return;
	}
	if ( !Q_stricmp( name, "current" ) ) {
		Com_Printf( "^1Can't load from \"current\"\n" );
		return;
	}
	if ( !Q_stricmp( name, "quik*" ) && Cvar_VariableIntegerValue( "cg_virtualvoyager" ) ) {
		Q_strncpyz( name, "virtual", sizeof( name ) );
	}
	if ( !Q_stricmp( name, "*respawn" ) ) {
		Q_strncpyz( name, "auto", sizeof( name ) );
		if ( stvefSpSaveLastRead[0] ) {
			char mapname[MAX_QPATH];
			Q_strncpyz( mapname, SV_STVEFSPMapName(), sizeof( mapname ) );
			const char *autosave = SV_STVEFSPSaveMapName( "auto" );
			const bool brig = !Q_stricmp( mapname, "_brig" );
			if ( !autosave || ( !brig && Q_stricmp( mapname, autosave ) ) ) {
				Q_strncpyz( name, stvefSpSaveLastRead, sizeof( name ) );
			}
		}
	}
	Com_Printf( "^5Loading game \"%s\"...\n", name );
	SV_STVEFSPReadSavegame( name );
	Com_Printf( "^5Done.\n" );
}

void SV_STVEFSPWipe_f() {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "^1USAGE: wipe <name>\n" );
		return;
	}
	const char *name = Cmd_Argv( 1 );
	if ( !Q_stricmp( name, "auto" ) || !Q_stricmp( name, "exitholodeck" ) ) {
		Com_Printf( "^1Can't wipe 'auto' or 'exitholodeck'\n" );
		return;
	}
	SV_STVEFSPWipeSave( name );
	Com_Printf( "Ok\n" );
}

} // namespace

/* The menus keep the comment a save is to be made with, and read a save's own
 * comment and picture for their lists (0x4268f0, 0x427150, 0x427290). */
void SV_STVEFSPStoreSaveComment( const char *comment ) {
	Q_strncpyz( stvefSpSaveComment, comment ? comment : "", sizeof( stvefSpSaveComment ) );
}

const char *SV_STVEFSPSaveComment( const char *name ) {
	STVEFSPSaveReader reader;
	if ( !SV_STVEFSPOpenSave( name, reader ) ) {
		return nullptr;
	}
	SV_STVEFSPReadChunk( reader, savegame::Comment, stvefSpSaveReadComment,
		sizeof( stvefSpSaveReadComment ), nullptr, false );
	stvefSpSaveReadComment[sizeof( stvefSpSaveReadComment ) - 1] = '\0';
	SV_STVEFSPCloseRead( reader );
	return stvefSpSaveReadComment;
}

qboolean SV_STVEFSPValidateSave( const char *name ) {
	STVEFSPSaveReader reader;
	reader.checking = true;
	if ( !SV_STVEFSPOpenSave( name, reader ) ) {
		return qfalse;
	}
	const qboolean valid =
		SV_STVEFSPReadChunk( reader, savegame::Comment, nullptr, savegame::CommentBytes, nullptr, false )
		&& SV_STVEFSPReadChunk( reader, savegame::Screenshot, nullptr, savegame::ScreenshotBytes, nullptr, false )
		? qtrue : qfalse;
	SV_STVEFSPCloseRead( reader );
	return valid;
}

qboolean SV_STVEFSPSaveImage( const char *name, void *pixels ) {
	if ( !pixels ) {
		return qfalse;
	}
	STVEFSPSaveReader reader;
	if ( !SV_STVEFSPOpenSave( name, reader ) ) {
		return qfalse;
	}
	SV_STVEFSPReadChunk( reader, savegame::Comment, nullptr, savegame::CommentBytes, nullptr, false );
	SV_STVEFSPReadChunk( reader, savegame::Screenshot, pixels, savegame::ScreenshotBytes, nullptr, false );
	SV_STVEFSPCloseRead( reader );
	return qtrue;
}

/* Retail drops a savegame that was being written when an error unwinds the
 * game, and the temporary file with it (Com_Error 0x411204). */
void SV_STVEFSPSaveAbort( void ) {
	SV_STVEFSPCloseWrite();
	SV_STVEFSPCloseRead( stvefSpSaveReader );
	stvefSpSavedGameJustLoaded = FNTECH3_STVEF_SP_SAVED_NO;
	if ( FS_Initialized() ) {
		SV_STVEFSPWipeSave( "current" );
	}
}

/* The autosave a level is entered with (SV_UserMove 0x424ead). A level whose
 * name begins with an underscore, a holodeck or the brig, keeps the save of
 * the level the player came from. */
void SV_STVEFSPEnteredWorld( void ) {
	const char *mapname = Cvar_VariableString( "mapname" );
	if ( mapname[0] != '_' ) {
		if ( stvefSpLoadTransition ) {
			char name[MAX_QPATH];
			Com_sprintf( name, sizeof( name ), "hub/%s", mapname );
			SV_STVEFSPWriteSavegame( name, qfalse );
			SV_STVEFSPWriteSavegame( "auto", qfalse );
			if ( Cvar_VariableIntegerValue( "cg_virtualvoyager" ) ) {
				SV_STVEFSPWriteSavegame( "virtual", qfalse );
			}
		} else if ( !stvefSpSavedGameJustLoaded ) {
			SV_STVEFSPWriteSavegame( "auto", qtrue );
		}
	}
	stvefSpSavedGameJustLoaded = FNTECH3_STVEF_SP_SAVED_NO;
	stvefSpLoadTransition = 0;
}

/* With sv_testsave set, the game's saving code runs every frame and nothing
 * is written (SG_TestSave 0x427e70). */
void SV_STVEFSPTestSave( void ) {
	if ( !stvefSpTestSave || stvefSpTestSave->value == 0.0f || sv.state != SS_GAME || !stvefSpGame ) {
		return;
	}
	SV_STVEFSPWriteGame( qfalse );
	stvefSpGame->WriteLevel( qfalse );
}

/* Retail's server commands for single player (SV_AddOperatorCommands
 * 0x4240e0): its own map and devmap, the two transitions, and the savegames.
 * It has no spmap, spdevmap or map_restart. */
void SV_STVEFSPAddCommands( void ) {
	stvefSpTestSave = Cvar_Get( "sv_testsave", "0", 0 );
	Cmd_AddCommand( "map", SV_STVEFSPMap_f );
	Cmd_AddCommand( "devmap", SV_STVEFSPMap_f );
	Cmd_AddCommand( "maptransition", SV_STVEFSPMapTransition_f );
	Cmd_AddCommand( "loadtransition", SV_STVEFSPLoadTransition_f );
	Cmd_AddCommand( "save", SV_STVEFSPSave_f );
	Cmd_AddCommand( "load", SV_STVEFSPLoad_f );
	Cmd_AddCommand( "wipe", SV_STVEFSPWipe_f );
}

#endif
