/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_MOHAA_SAVEGAME_HPP
#define FNTECH3_COMPAT_MOHAA_SAVEGAME_HPP

/*
 * Medal of Honor: Allied Assault's savegames: the byte-level rules the
 * server, the load/save menu and the tests share.
 *
 * A save is three files under save/<folder>/: the game module's level
 * (.sav, its WriteLevel), the engine's header with the area portals behind
 * it (.ssv) and a picture of the view (.tga, 256 by 256). Between two levels
 * the player's persistent data waits in current.spv. The folder is the
 * `config` cvar without its extension (OpenMoHAA Com_GetArchiveFolder,
 * reference only); fnTech3 keeps no `config` for this profile, so the folder
 * is the profile's default configuration's name, "unnamedsoldier".
 *
 * MOHAA.exe cannot be read (SafeDisc encrypts it), so 1.00's own .ssv
 * layout is unknown. This one is fnTech3's: the fields of OpenMoHAA's
 * savegamestruct_t, little-endian, behind a version number of its own
 * ("FNMH" and 1), so neither engine takes the other's file for its own.
 *
 *   0x000  int32  version       0x464e4d01
 *   0x004  int32  saveTime      time() when saved, 32 bits
 *   0x008  int32  mapTime       the level's clock, in milliseconds
 *   0x00c  char   comment[64]   what the load list shows
 *   0x04c  char   mapName[64]
 *   0x08c  char   saveName[64]
 *   0x0cc  char   musicFile[64] the triggered music playing, or empty
 *   0x10c  int32  musicLoops    0 for a loop, 1 for one pass (OpenMoHAA's count)
 *   0x110  int32  musicOffset   its place, in the file's own frames
 *   0x114  int32  numAreas, then numAreas * numAreas int32 portal counts
 *                 (CM_WritePortalState's block)
 *
 * Each string is written as strncpy writes it: up to 64 bytes, zeros after,
 * no terminator when full. The game module sets `g_lastsave` to the name
 * itself as it writes the level (gamex86.dll 0x310d7102..0x310d7107).
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace fntech3 {
namespace mohaa {
namespace savegame {

constexpr const char *Directory = "save";
constexpr const char *DefaultFolder = "unnamedsoldier";
constexpr std::size_t NameBytes = 64;
constexpr std::size_t FieldBytes = 64;
constexpr std::size_t PathBytes = 256;

constexpr std::int32_t Version = 0x464e4d01;
constexpr std::size_t HeaderVersion = 0x000;
constexpr std::size_t HeaderSaveTime = 0x004;
constexpr std::size_t HeaderMapTime = 0x008;
constexpr std::size_t HeaderComment = 0x00c;
constexpr std::size_t HeaderMap = 0x04c;
constexpr std::size_t HeaderName = 0x08c;
constexpr std::size_t HeaderMusic = 0x0cc;
constexpr std::size_t HeaderMusicLoops = 0x10c;
constexpr std::size_t HeaderMusicOffset = 0x110;
constexpr std::size_t HeaderBytes = 0x114;

static_assert( HeaderComment == HeaderMapTime + 4, "the comment follows the three numbers" );
static_assert( HeaderMap == HeaderComment + FieldBytes, "the map follows the comment" );
static_assert( HeaderName == HeaderMap + FieldBytes, "the name follows the map" );
static_assert( HeaderMusic == HeaderName + FieldBytes, "the music follows the name" );
static_assert( HeaderMusicLoops == HeaderMusic + FieldBytes, "the loop count follows the music" );
static_assert( HeaderBytes == HeaderMusicOffset + 4, "the offset ends the header" );

/* A snapshot carries 32 area bytes, so no map has more than 256 areas. */
constexpr int MaxAreas = 0x100;

constexpr const char *CurrentName = "current";
constexpr const char *LevelExtension = "sav";
constexpr const char *ServerExtension = "ssv";
constexpr const char *PersistentExtension = "spv";
constexpr const char *ShotExtension = "tga";
/* What deleting a save removes (OpenMoHAA Com_WipeSavegame); the
 * persistent data belongs to the level change, not to a save. */
constexpr const char *WipeOrder[3] = { "sav", "ssv", "tga" };

/* The picture: "saveshot <path> 256 256" (OpenMoHAA SV_SaveGame). */
constexpr int ShotSide = 256;
/* The load list's picture when there is none (a shader in Pak0's
 * scripts/menu shader file, textures/mohmenu/no_saved_games.tga). */
constexpr const char *NoShot = "textures/menu/no_saved_games";

/* SV_FindSaveName: four digits after the map's name. */
constexpr int AutosaveNames = 10000;

inline bool HasTraversal( const char *text ) {
	return !text || std::strstr( text, ".." ) || std::strchr( text, '/' ) || std::strchr( text, '\\' )
		|| std::strchr( text, ':' );
}

/* A save name the engine may build paths from, and a command: the load list
 * sends `loadgame "<name>"`, so no quote or command break either. */
inline bool NameAllowed( const char *name ) {
	return name && name[0] && std::strlen( name ) < NameBytes && !HasTraversal( name ) && !std::strpbrk( name, "\";" );
}

/* The folder: `config` without any directory and without its extension,
 * or the default when that leaves nothing. */
inline void Folder( const char *config, char ( &out )[NameBytes] ) {
	const char *base = config ? config : "";
	for ( const char *c = base; *c; ++c ) {
		if ( *c == '/' || *c == '\\' || *c == ':' ) base = c + 1;
	}
	const char *dot = std::strrchr( base, '.' );
	std::size_t length = dot ? static_cast<std::size_t>( dot - base ) : std::strlen( base );
	if ( length >= NameBytes ) {
		length = 0;
	}
	std::memcpy( out, base, length );
	out[length] = '\0';
	if ( !out[0] || !std::strcmp( out, "." ) || std::strstr( out, ".." ) ) {
		std::snprintf( out, NameBytes, "%s", DefaultFolder );
	}
}

/* "save/<folder>" (the list's search folder). */
inline void FolderPath( const char *config, char ( &out )[PathBytes] ) {
	char folder[NameBytes];
	Folder( config, folder );
	std::snprintf( out, PathBytes, "%s/%s", Directory, folder );
}

/* "save/<folder>/<name>.<ext>"; false, and empty, for a name or an
 * extension that could leave the folder. */
inline bool ArchivePath( const char *config, const char *name, const char *ext, char ( &out )[PathBytes] ) {
	out[0] = '\0';
	if ( !NameAllowed( name ) || !ext || !ext[0] || HasTraversal( ext ) ) {
		return false;
	}
	char folder[NameBytes];
	Folder( config, folder );
	const int written = std::snprintf( out, PathBytes, "%s/%s/%s.%s", Directory, folder, name, ext );
	if ( written < 0 || static_cast<std::size_t>( written ) >= PathBytes ) {
		out[0] = '\0';
		return false;
	}
	return true;
}

/* The map's name with the characters a file name cannot hold made `_`. */
inline void AutosavePrefix( const char *mapName, char ( &out )[NameBytes] ) {
	std::size_t i = 0;
	for ( ; mapName && mapName[i] && i + 5 < NameBytes; ++i ) {
		const char c = mapName[i];
		out[i] = c == '/' || c == '\\' || c == '$' || c == ':' ? '_' : c;
	}
	out[i] = '\0';
}

/* "%s%04i": m1l1 and 3 give m1l10003. */
inline bool AutosaveName( const char *prefix, int index, char ( &out )[NameBytes] ) {
	out[0] = '\0';
	if ( !prefix || index < 0 || index >= AutosaveNames ) {
		return false;
	}
	const int written = std::snprintf( out, NameBytes, "%s%04i", prefix, index );
	if ( written < 0 || static_cast<std::size_t>( written ) >= NameBytes ) {
		out[0] = '\0';
		return false;
	}
	return true;
}

/* A quick save's comment says so: its .ssv path ends in "quick.ssv"
 * (OpenMoHAA SV_SaveGame), which is to say its name ends in "quick". */
inline bool IsQuickSave( const char *name ) {
	const std::size_t length = name ? std::strlen( name ) : 0;
	return length >= 5 && !std::strcmp( name + length - 5, "quick" );
}

/* The comment: the level's message, then "Starting" for an autosave or the
 * name the level gave this point (CS_SAVENAME), and "QuickSave - " before
 * it all for a quick save. */
inline void Comment( const char *message, const char *pointName, bool autosave, bool quick,
	char ( &out )[FieldBytes] ) {
	char body[FieldBytes];
	const char *level = message ? message : "";
	if ( autosave ) {
		std::snprintf( body, sizeof( body ), "%s - Starting", level );
	} else if ( pointName && pointName[0] ) {
		std::snprintf( body, sizeof( body ), "%s - %s", level, pointName );
	} else {
		std::snprintf( body, sizeof( body ), "%s", level );
	}
	if ( quick ) {
		/* The field holds 63 characters, twelve of them the prefix. */
		std::snprintf( out, FieldBytes, "QuickSave - %.51s", body );
	} else {
		std::snprintf( out, FieldBytes, "%s", body );
	}
}

/* The header, each string one byte longer than its field so that a field
 * written full still reads as a terminated string. */
struct Header {
	std::int32_t version = Version;
	std::int32_t saveTime = 0;
	std::int32_t mapTime = 0;
	char comment[FieldBytes + 1] = {};
	char mapName[FieldBytes + 1] = {};
	char saveName[FieldBytes + 1] = {};
	char musicFile[FieldBytes + 1] = {};
	std::int32_t musicLoops = 0;
	std::int32_t musicOffset = 0;
};

inline void WriteInt32( std::uint8_t *to, std::int32_t value ) {
	const std::uint32_t v = static_cast<std::uint32_t>( value );
	to[0] = static_cast<std::uint8_t>( v );
	to[1] = static_cast<std::uint8_t>( v >> 8 );
	to[2] = static_cast<std::uint8_t>( v >> 16 );
	to[3] = static_cast<std::uint8_t>( v >> 24 );
}

inline std::int32_t ReadInt32( const std::uint8_t *from ) {
	return static_cast<std::int32_t>( static_cast<std::uint32_t>( from[0] )
		| static_cast<std::uint32_t>( from[1] ) << 8 | static_cast<std::uint32_t>( from[2] ) << 16
		| static_cast<std::uint32_t>( from[3] ) << 24 );
}

inline void WriteField( std::uint8_t *to, const char *text ) {
	std::size_t i = 0;
	for ( ; text && text[i] && i < FieldBytes; ++i ) {
		to[i] = static_cast<std::uint8_t>( text[i] );
	}
	for ( ; i < FieldBytes; ++i ) {
		to[i] = 0;
	}
}

inline void ReadField( const std::uint8_t *from, char ( &out )[FieldBytes + 1] ) {
	std::size_t i = 0;
	for ( ; i < FieldBytes && from[i]; ++i ) {
		out[i] = static_cast<char>( from[i] );
	}
	out[i] = '\0';
}

inline void Encode( const Header &header, std::uint8_t ( &out )[HeaderBytes] ) {
	WriteInt32( out + HeaderVersion, header.version );
	WriteInt32( out + HeaderSaveTime, header.saveTime );
	WriteInt32( out + HeaderMapTime, header.mapTime );
	WriteField( out + HeaderComment, header.comment );
	WriteField( out + HeaderMap, header.mapName );
	WriteField( out + HeaderName, header.saveName );
	WriteField( out + HeaderMusic, header.musicFile );
	WriteInt32( out + HeaderMusicLoops, header.musicLoops );
	WriteInt32( out + HeaderMusicOffset, header.musicOffset );
}

/* False for a file too short to hold a header or of another version. */
inline bool Decode( const std::uint8_t *in, std::size_t size, Header &header ) {
	if ( !in || size < HeaderBytes ) {
		return false;
	}
	header.version = ReadInt32( in + HeaderVersion );
	if ( header.version != Version ) {
		return false;
	}
	header.saveTime = ReadInt32( in + HeaderSaveTime );
	header.mapTime = ReadInt32( in + HeaderMapTime );
	ReadField( in + HeaderComment, header.comment );
	ReadField( in + HeaderMap, header.mapName );
	ReadField( in + HeaderName, header.saveName );
	ReadField( in + HeaderMusic, header.musicFile );
	header.musicLoops = ReadInt32( in + HeaderMusicLoops );
	header.musicOffset = ReadInt32( in + HeaderMusicOffset );
	return true;
}

/* The portal block's length for an area count, or false for a count no map
 * can have. */
inline bool PortalBytes( int numAreas, std::size_t &bytes ) {
	bytes = 0;
	if ( numAreas < 1 || numAreas > MaxAreas ) {
		return false;
	}
	const std::size_t n = static_cast<std::size_t>( numAreas );
	bytes = sizeof( std::int32_t ) + sizeof( std::int32_t ) * n * n;
	return true;
}

/* The block after the header: its area count and the counts themselves,
 * `counts` holding numAreas squared of them. False, with nothing read, for a
 * missing, short or impossible block. */
inline bool ReadPortals( const std::uint8_t *in, std::size_t size, int &numAreas, std::int32_t *counts,
	std::size_t capacity ) {
	numAreas = 0;
	if ( !in || size < HeaderBytes + 4 ) {
		return false;
	}
	const int areas = ReadInt32( in + HeaderBytes );
	std::size_t bytes = 0;
	if ( !PortalBytes( areas, bytes ) || size < HeaderBytes + bytes ) {
		return false;
	}
	const std::size_t pairs = static_cast<std::size_t>( areas ) * static_cast<std::size_t>( areas );
	if ( !counts || capacity < pairs ) {
		return false;
	}
	for ( std::size_t i = 0; i < pairs; ++i ) {
		counts[i] = ReadInt32( in + HeaderBytes + 4 + i * 4 );
	}
	numAreas = areas;
	return true;
}

/* The load list's elapsed time, "H:MM:SS" (OpenMoHAA FAKKLoadGameItem). */
inline void ElapsedText( std::int32_t mapTime, char ( &out )[32] ) {
	const long seconds = mapTime > 0 ? static_cast<long>( mapTime / 1000 ) : 0;
	std::snprintf( out, sizeof( out ), "%ld:%02ld:%02ld", seconds / 3600, seconds % 3600 / 60, seconds % 60 );
}

/* Its date, as strftime's "%a %b %d %Y %H:%M:%S". */
constexpr const char *DateFormat = "%a %b %d %Y %H:%M:%S";

} // namespace savegame
} // namespace mohaa
} // namespace fntech3

#endif
