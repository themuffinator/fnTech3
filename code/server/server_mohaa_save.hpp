/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_MOHAA_SAVE_HPP
#define FNTECH3_SERVER_MOHAA_SAVE_HPP

/*
 * Medal of Honor: Allied Assault's single-player level flow and savegames.
 * Included by server_mohaa_game.hpp, whose export calls it builds on.
 *
 * MOHAA.exe cannot be read (SafeDisc encrypts it); this follows OpenMoHAA's
 * reconstruction (server/sv_ccmds.c and sv_init.c, reference only) wherever
 * 1.00's own game module does not settle it:
 *
 *   gamemap <map>   a level's exit (the game sends `gamemap "%s"`, gamex86.dll
 *                   0x311d5640): the player's persistent data -- health,
 *                   weapons, ammunition -- goes to current.spv through the
 *                   game's ArchivePersistant (export 23) before the change
 *                   and comes back after the next level has spawned.
 *   restart         the same map again (the game's own `restart\n`,
 *                   0x311cf5a0, after a failed mission).
 *   loadlastgame    the save named by g_lastsave, which the game sets as it
 *                   writes a level and sends for after a death when it is
 *                   set, `restart` otherwise (0x311143ec..0x3111440d).
 *   savegame [name] autosavegame  loadgame <name>
 *                   A save is three files under save/<folder>/: the game
 *                   writes the level (.sav, WriteLevel, export 24), the
 *                   engine a header with the area portals behind it (.ssv)
 *                   and the client a picture of the view (.tga), which the
 *                   load list shows. While the game writes or reads the
 *                   level it calls the engine back (import 156) to put the
 *                   local client's HUD elements, view-model animation and
 *                   stopwatch into the same archive through its Archive*
 *                   exports (27..30).
 *
 * 1.00's exports take no client-game state, unlike the later games': it is
 * WriteLevel( file, autosave ), ReadLevel( file ) and LevelArchiveValid( file )
 * (gamex86.dll 0x310d7720, 0x310d7750, 0x310d7770), each reading its
 * arguments straight off the stack.
 *
 * The .ssv layout is fnTech3's own (code/compat/mohaa_savegame.hpp): 1.00's
 * cannot be checked. The .sav is the game's own format end to end. fnTech3
 * writes into fs_homepath and reads through the search path, so the
 * installation is never written.
 */

#include "../compat/mohaa_savegame.hpp"

#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#ifndef DEDICATED
#include "../compat/mohaa_cgame_abi.h"
/* The local client's state a save carries (cl_mohaa_cgame.cpp). */
fntech3::mohaa::HudDrawElement *CL_MOHAAHudElements( int *count );
fntech3::mohaa::ClientAnim *CL_MOHAAClientAnim( void );
fntech3::mohaa::StopWatch *CL_MOHAAStopWatch( void );
void CL_MOHAARefreshHudElements( void );
/* Its triggered music (cl_mohaa_sound.cpp). */
bool CL_MOHAAMusicTriggeredState( char *file, int fileSize, bool *loops, int *offset );
void CL_MOHAAMusicRestoreTriggered( const char *file, bool loops, int offset );
#endif

namespace mohaa_game {
namespace save {

namespace ssv = fntech3::mohaa::savegame;

/* svs.gameName: the archive the engine works on, "current" between saves. */
char gameName[ssv::NameBytes] = "current";
/* The map the level was spawned from, and the level time a save recorded. */
char mapName[ssv::NameBytes];
int mapTime;
/* What the next spawn does: read a save's level back, or bring the player's
 * persistent data along from the level before. */
bool loading;
bool transition;
/* Whether the next spawn is the running level over again -- a restart, or a
 * load of a save of its own map -- which 2015's engine spawns without loading
 * the map again (OpenMoHAA sv_init.c SV_SpawnServer, `differentmap`). */
bool sameLevel;
/* Whether the level just spawned was read back from a save. */
bool restored;
/* The level saves itself once its player is in (svs.autosave). */
bool autosave;
/* A loaded save's triggered music, started again once its player is in
 * (svs.tm_filename, tm_loopcount, tm_offset). */
ssv::Header loadedHeader;

typedef void ( QDECL *ArchivePersistant_t )( const char *name, qboolean loading );
typedef void ( QDECL *WriteLevel_t )( const char *filename, qboolean autosave );
typedef qboolean ( QDECL *ReadLevel_t )( const char *filename );
typedef qboolean ( QDECL *LevelArchiveValid_t )( const char *filename );
typedef void ( QDECL *ArchiveInteger_t )( int *value );
typedef void ( QDECL *ArchiveFloat_t )( float *value );
typedef void ( QDECL *ArchiveString_t )( char *value );

const char *Archive( const char *name, const char *extension ) {
	return GetArchiveFileName( name, extension );
}

/* ---- the engine's part of the level archive (import 156) ------------------ */

#ifndef DEDICATED
/* A string through the game's archiver. Reading copies the whole stored
 * string into the buffer it is given, so it lands in scratch room first and
 * is cut to the field. */
void ArchiveText( char *field, std::size_t size, bool reading ) {
	static char scratch[16384];
	const auto archive = Export<ArchiveString_t>( GE_ArchiveString );
	if ( !archive ) {
		return;
	}
	if ( reading ) {
		scratch[0] = '\0';
		archive( scratch );
		Q_strncpyz( field, scratch, static_cast<int>( size ) );
	} else {
		Q_strncpyz( scratch, field, sizeof( scratch ) );
		archive( scratch );
	}
}

/* SV_ArchiveLevel: the HUD elements, the view model's animation and the
 * stopwatch, in OpenMoHAA's order. */
void ArchiveClientState( bool reading ) {
	const auto archiveInt = Export<ArchiveInteger_t>( GE_ArchiveInteger );
	const auto archiveFloat = Export<ArchiveFloat_t>( GE_ArchiveFloat );
	const auto archiveTime = Export<ArchiveInteger_t>( GE_ArchiveSvsTime );
	if ( !archiveInt || !archiveFloat || !archiveTime ) {
		return;
	}
	int count = 0;
	fntech3::mohaa::HudDrawElement *elements = CL_MOHAAHudElements( &count );
	for ( int i = 0; elements && i < count; i++ ) {
		fntech3::mohaa::HudDrawElement &e = elements[i];
		ArchiveText( e.shaderName, sizeof( e.shaderName ), reading );
		archiveInt( &e.iX );
		archiveInt( &e.iY );
		archiveInt( &e.iWidth );
		archiveInt( &e.iHeight );
		for ( float &c : e.vColor ) archiveFloat( &c );
		archiveInt( &e.iHorizontalAlign );
		archiveInt( &e.iVerticalAlign );
		archiveInt( &e.bVirtualScreen );
		ArchiveText( e.fontName, sizeof( e.fontName ), reading );
		ArchiveText( e.string, sizeof( e.string ), reading );
	}
	if ( reading ) {
		CL_MOHAARefreshHudElements();
	}

	fntech3::mohaa::ClientAnim *anim = CL_MOHAAClientAnim();
	if ( anim ) {
		for ( fntech3::mohaa::FrameInfo &frame : anim->g_VMFrameInfo ) {
			archiveInt( &frame.index );
			archiveFloat( &frame.time );
			archiveFloat( &frame.weight );
		}
		archiveInt( &anim->g_iLastVMAnim );
		archiveInt( &anim->g_iLastVMAnimChanged );
		archiveInt( &anim->g_iCurrentVMAnimSlot );
		archiveInt( &anim->g_iCurrentVMDuration );
		archiveInt( &anim->g_bCrossblending );
		archiveInt( &anim->g_iLastEquippedWeaponStat );
		ArchiveText( anim->g_szLastActiveItem, sizeof( anim->g_szLastActiveItem ), reading );
		archiveInt( &anim->g_iLastAnimPrefixIndex );
		for ( float &v : anim->g_vCurrentVMPosOffset ) archiveFloat( &v );
	}

	fntech3::mohaa::StopWatch *watch = CL_MOHAAStopWatch();
	if ( watch ) {
		archiveTime( &watch->iStartTime );
		archiveTime( &watch->iEndTime );
	}
}
#endif

/* ---- the server file (.ssv) ------------------------------------------------ */

/* The header, and CM_WritePortalState's block behind it: the area count and
 * a reference count for every pair of areas. */
bool WriteServerFile( const ssv::Header &header ) {
	std::vector<std::uint8_t> bytes( ssv::HeaderBytes );
	std::uint8_t encoded[ssv::HeaderBytes];
	ssv::Encode( header, encoded );
	std::memcpy( bytes.data(), encoded, sizeof( encoded ) );
	int count = 0;
	const int *counts = CM_AreaPortalCounts( &count );
	const auto put = [&bytes]( std::int32_t value ) {
		std::uint8_t word[4];
		ssv::WriteInt32( word, value );
		bytes.insert( bytes.end(), word, word + 4 );
	};
	put( CM_NumAreas() );
	for ( int i = 0; counts && i < count; i++ ) put( counts[i] );

	const char *path = Archive( gameName, ssv::ServerExtension );
	const fileHandle_t f = path[0] ? FS_FOpenFileWrite( path ) : 0;
	if ( !f ) {
		Com_Printf( "Couldn't write %s\n", path );
		return false;
	}
	FS_Write( bytes.data(), static_cast<int>( bytes.size() ), f );
	FS_FCloseFile( f );
	return true;
}

/* The file's header, and its portal counts when `portals` is given. */
bool ReadServerFile( const char *name, ssv::Header &header, std::vector<std::int32_t> *portals = nullptr,
	int *numAreas = nullptr ) {
	void *buffer = nullptr;
	const char *path = Archive( name, ssv::ServerExtension );
	const int length = path[0] ? FS_ReadFile( path, &buffer ) : -1;
	if ( length < 0 || !buffer ) {
		return false;
	}
	const auto *p = static_cast<const std::uint8_t *>( buffer );
	const std::size_t size = static_cast<std::size_t>( length );
	const bool ok = ssv::Decode( p, size, header );
	if ( ok && portals && numAreas ) {
		portals->assign( static_cast<std::size_t>( ssv::MaxAreas ) * ssv::MaxAreas, 0 );
		if ( ssv::ReadPortals( p, size, *numAreas, portals->data(), portals->size() ) ) {
			portals->resize( static_cast<std::size_t>( *numAreas ) * static_cast<std::size_t>( *numAreas ) );
		} else {
			portals->clear();
		}
	}
	FS_FreeFile( buffer );
	return ok;
}

/* CM_ReadPortalState: the areas must be the map's, and then they are flooded
 * again. */
void RestorePortals( int numAreas, const std::vector<std::int32_t> &portals ) {
	int count = 0;
	int *counts = CM_AreaPortalCounts( &count );
	if ( numAreas != CM_NumAreas() || !counts || portals.size() != static_cast<std::size_t>( count ) ) {
		Com_Printf( "The save game's area portals do not match the map; they are left as spawned\n" );
		return;
	}
	for ( int i = 0; i < count; i++ ) {
		counts[i] = portals[i] < 0 ? 0 : portals[i];
	}
	CM_FloodAreaConnections();
}

/* ---- saving ---------------------------------------------------------------- */

/* SV_AllowSaveGame: a local single-player level under way, its player alive. */
bool AllowSave( bool quiet ) {
	const char *refusal = nullptr;
	if ( !com_sv_running || !com_sv_running->integer || !exports ) {
		refusal = "You must be in a game with a server to save.";
	} else if ( !serverLoaded ) {
		refusal = "You must be in game to save.";
	} else if ( Cvar_VariableIntegerValue( "g_gametype" ) != 0 ) {
		refusal = "Can't savegame in a multiplayer game";
	} else {
		const GEntity *player = mohaa_world::Entity( 0 );
		const auto *state = player && player->client
			? reinterpret_cast<const fntech3::mohaa::PlayerState *>( static_cast<std::uintptr_t>( player->client ) )
			: nullptr;
		if ( !state || state->stats[0] <= 0 ) {
			refusal = "Can't savegame when dead";
		}
	}
	if ( refusal && !quiet ) {
		Com_DPrintf( "%s\n", refusal );
	}
	return refusal == nullptr;
}

/* SV_SavegameFilename: the map's name and the first four-digit number no
 * save has yet. */
void DefaultSaveName( char ( &out )[ssv::NameBytes] ) {
	char prefix[ssv::NameBytes];
	ssv::AutosavePrefix( mapName, prefix );
	for ( int number = 0; number < ssv::AutosaveNames; number++ ) {
		if ( ssv::AutosaveName( prefix, number, out ) && FS_ReadFile( Archive( out, ssv::ServerExtension ), nullptr ) <= 0 ) {
			return;
		}
	}
}

/* SV_SaveGame: the level through the game, the engine's header, the
 * client's picture of the view, and back to "current". */
void SaveGame( const char *requested, bool isAutosave ) {
	if ( !AllowSave( false ) ) {
		return;
	}
	char name[ssv::NameBytes];
	if ( requested && requested[0] ) {
		Q_strncpyz( name, requested, sizeof( name ) );
	} else {
		DefaultSaveName( name );
	}
	if ( !ssv::NameAllowed( name ) ) {
		Com_Printf( "Bad savedir.\n" );
		return;
	}
	Com_Printf( "Saving to %s%s\n", name, isAutosave ? " (autosave)..." : "..." );
	Q_strncpyz( gameName, name, sizeof( gameName ) );

	const auto writeLevel = Export<WriteLevel_t>( GE_WriteLevel );
	if ( writeLevel ) {
		writeLevel( Archive( gameName, ssv::LevelExtension ), isAutosave ? qtrue : qfalse );
		CheckErrorMessage();
	}

	ssv::Header header;
	header.saveTime = static_cast<std::int32_t>( std::time( nullptr ) );
	header.mapTime = sv.time - levelStartTime;
	char comment[ssv::FieldBytes];
	ssv::Comment( sv.configstrings[FNTECH3_MOHAA_CS_MESSAGE], sv.configstrings[FNTECH3_MOHAA_CS_SAVENAME],
		isAutosave, ssv::IsQuickSave( name ), comment );
	Q_strncpyz( header.comment, comment, sizeof( header.comment ) );
	SV_SetConfigstring( FNTECH3_MOHAA_CS_SAVENAME, "" );
	Q_strncpyz( header.mapName, mapName, sizeof( header.mapName ) );
	Q_strncpyz( header.saveName, gameName, sizeof( header.saveName ) );
#ifndef DEDICATED
	/* The triggered music: its loop count as OpenMoHAA keeps it, 0 for a
	 * loop and 1 for one pass, and its place in the file's own frames. */
	bool loops = false;
	int offset = 0;
	if ( CL_MOHAAMusicTriggeredState( header.musicFile, sizeof( header.musicFile ), &loops, &offset ) ) {
		header.musicLoops = loops ? 0 : 1;
		header.musicOffset = offset;
	}
#endif
	WriteServerFile( header );

#ifndef DEDICATED
	/* The picture, a frame of the view alone; an autosave's comes as the
	 * next frames start, a save's at once (OpenMoHAA SV_SaveGame). */
	if ( Cvar_VariableIntegerValue( "cl_running" ) ) {
		Cbuf_ExecuteText( isAutosave ? EXEC_INSERT : EXEC_NOW,
			va( "saveshot %s %i %i\n", Archive( gameName, ssv::ShotExtension ), ssv::ShotSide, ssv::ShotSide ) );
	}
#endif

	Com_Printf( "Done.\n" );
	Q_strncpyz( gameName, ssv::CurrentName, sizeof( gameName ) );
}

/* ---- the spawn's hooks ------------------------------------------------------- */

/* Called after the level's Precache: a save's level is read back in place of
 * its entities being spawned. True when it was. */
bool ReadSavedLevel() {
	restored = false;
	if ( !loading ) {
		return false;
	}
	loading = false;
	const auto readLevel = Export<ReadLevel_t>( GE_ReadLevel );
	if ( !readLevel || !readLevel( Archive( gameName, ssv::LevelExtension ) ) ) {
		Com_Printf( "The saved level could not be read back; the map starts over\n" );
		Export<Cleanup_t>( GE_Cleanup )( qfalse );
		return false;
	}
	CheckErrorMessage();
	std::vector<std::int32_t> portals;
	int numAreas = 0;
	loadedHeader = ssv::Header();
	if ( ReadServerFile( gameName, loadedHeader, &portals, &numAreas ) ) {
		RestorePortals( numAreas, portals );
	}
	restored = true;
	return true;
}

/* The start of a level's spawn: its map is remembered, and a fresh
 * single-player level will save itself once its player is in. */
void BeginSpawn( const char *mapname ) {
	Q_strncpyz( mapName, mapname ? mapname : "", sizeof( mapName ) );
	restored = false;
	autosave = !loading && Cvar_VariableIntegerValue( "g_gametype" ) == 0;
}

bool Loading() { return loading; }
bool Restored() { return restored; }
int MapTime() { return mapTime; }

/* Whether the spawn under way is the running level's again, asked once as it
 * starts; any other spawn loads its map afresh. */
bool TakeSameLevel() {
	const bool same = sameLevel;
	sameLevel = false;
	return same;
}

/* After the entities of a level reached by a level change spawned: the
 * player's persistent data comes back. */
void AfterSpawn() {
	if ( transition ) {
		transition = false;
		const auto archive = Export<ArchivePersistant_t>( GE_ArchivePersistant );
		if ( archive ) {
			archive( Archive( gameName, ssv::PersistentExtension ), qtrue );
			CheckErrorMessage();
		}
	}
}

/* Once the player is in (SV_ServerLoaded): a loaded save's triggered music
 * plays on from where it was, and a fresh single-player level saves itself,
 * but not the credits. */
void AfterLoaded() {
	Q_strncpyz( gameName, ssv::CurrentName, sizeof( gameName ) );
#ifndef DEDICATED
	if ( restored && Cvar_VariableIntegerValue( "cl_running" ) ) {
		CL_MOHAAMusicRestoreTriggered( loadedHeader.musicFile, loadedHeader.musicLoops == 0,
			loadedHeader.musicOffset );
	}
#endif
	if ( autosave ) {
		autosave = false;
		if ( Q_stricmp( mapName, "credits" ) ) {
			SaveGame( nullptr, true );
		}
	}
}

/* ---- the commands ------------------------------------------------------------- */

/* The level change: the persistent data of a level under way goes to
 * current.spv, and the next level reads it back. */
void GameMap_f() {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "USAGE: gamemap <map>\n" );
		return;
	}
	char map[ssv::NameBytes];
	Q_strncpyz( map, Cmd_Argv( 1 ), sizeof( map ) );
	if ( FS_ReadFile( va( "maps/%s.bsp", map ), nullptr ) <= 0 ) {
		Com_Printf( "Can't find map %s\n", map );
		return;
	}
	Q_strncpyz( gameName, ssv::CurrentName, sizeof( gameName ) );
	const bool underWay = com_sv_running && com_sv_running->integer && exports && serverLoaded;
	if ( underWay ) {
		const auto archive = Export<ArchivePersistant_t>( GE_ArchivePersistant );
		if ( archive ) {
			archive( Archive( gameName, ssv::PersistentExtension ), qfalse );
			CheckErrorMessage();
		}
	}
	transition = underWay;
	sameLevel = false;
	SV_SpawnServer( map, qfalse );
}

/* The same map again, from its start. */
void Restart_f() {
	if ( !com_sv_running || !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	char map[ssv::NameBytes];
	Q_strncpyz( map, mapName, sizeof( map ) );
	transition = false;
	sameLevel = true;
	SV_SpawnServer( map, qfalse );
}

void SaveGame_f() {
	SaveGame( Cmd_Argc() == 2 ? Cmd_Argv( 1 ) : nullptr, false );
}

void AutosaveGame_f() {
	SaveGame( nullptr, true );
}

/* SV_Loadgame_f: only outside a multiplayer game; the level is read back by
 * the spawn of the map the save names, its clock resumed where it stopped. */
void LoadGame( const char *name ) {
	if ( ( com_sv_running && com_sv_running->integer && Cvar_VariableIntegerValue( "g_gametype" ) != 0 )
		|| ( Cvar_VariableIntegerValue( "cl_running" ) && Cvar_VariableIntegerValue( "cg_gametype" ) != 0
			&& !( com_sv_running && com_sv_running->integer ) ) ) {
		Com_Printf( "Can't loadgame in a multiplayer game\n" );
		return;
	}
	Com_Printf( "Loading game...%s\n", name );
	if ( !ssv::NameAllowed( name ) ) {
		Com_Printf( "Bad savedir.\n" );
		return;
	}
	if ( FS_ReadFile( Archive( name, ssv::LevelExtension ), nullptr ) <= 0 ) {
		Com_Printf( "Savegame not found.\n" );
		return;
	}
	ssv::Header header;
	if ( !ReadServerFile( name, header ) ) {
		Com_Printf( "Invalid or Old Server SaveGame Version\n" );
		return;
	}
	if ( FS_ReadFile( va( "maps/%s.bsp", header.mapName ), nullptr ) <= 0 ) {
		Com_Printf( "Can't find map %s\n", header.mapName );
		return;
	}
	Cvar_Set( "g_gametype", "0" );
	Cvar_SetLatched( "sv_maxclients", "1" );
	Q_strncpyz( gameName, name, sizeof( gameName ) );
	mapTime = header.mapTime;
	loading = true;
	transition = false;
	sameLevel = com_sv_running && com_sv_running->integer && !Q_stricmp( mapName, header.mapName );
	char map[ssv::NameBytes];
	Q_strncpyz( map, header.mapName, sizeof( map ) );
	SV_SpawnServer( map, qfalse );
}

void LoadGame_f() {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "USAGE: loadgame <name>\n" );
		return;
	}
	char name[ssv::NameBytes + 1];
	Q_strncpyz( name, Cmd_Argv( 1 ), sizeof( name ) );
	LoadGame( name );
}

/* The game's own call after a death with a save behind it. */
void LoadLastGame_f() {
	char name[ssv::NameBytes + 1];
	Cvar_VariableStringBuffer( "g_lastsave", name, sizeof( name ) );
	if ( !name[0] ) {
		Com_Printf( "There is no last savegame to load.\n" );
		return;
	}
	LoadGame( name );
}

} // namespace save
} // namespace mohaa_game

/* The single-player level flow's commands, beside Quake III's map set. */
void SV_MOHAAAddCommands( void ) {
	Cmd_AddCommand( "gamemap", mohaa_game::save::GameMap_f );
	Cmd_AddCommand( "restart", mohaa_game::save::Restart_f );
	Cmd_AddCommand( "savegame", mohaa_game::save::SaveGame_f );
	Cmd_AddCommand( "autosavegame", mohaa_game::save::AutosaveGame_f );
	Cmd_AddCommand( "loadgame", mohaa_game::save::LoadGame_f );
	Cmd_AddCommand( "loadlastgame", mohaa_game::save::LoadLastGame_f );
}

#endif
