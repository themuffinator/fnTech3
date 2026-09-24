/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_SAVEGAME_HPP
#define FNTECH3_COMPAT_ALICE_SAVEGAME_HPP

/*
 * American McGee's Alice's savegames, as retail alice.exe writes and reads
 * them. Everything here is measured from the retail engine; see
 * docs/fntech3/2026-09-23-alice-savegames.md.
 *
 * A save is four files under `save/`, each named by the archive name builder
 * (0x0043a350): `save/<config>_<name>.<ext>`, where <config> is the `config`
 * cvar cut at its first dot. The game module writes the level (`.sav`) and
 * the persistent data (`.spv`); the engine writes a 200-byte header with the
 * area portals behind it (`.ssv`) and the client a 256-pixel grey picture
 * (`.tga`).
 *
 * This header holds the byte-level rules only -- names, the header record,
 * the portal block, the picture and the module's CRC -- so that the server,
 * the client and the tests share one statement of them.
 */

#include "alice_saveshot.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace fntech3 {
namespace alice {
namespace savegame {

constexpr const char *Directory = "save";
/* The name builder formats into a 128-byte static buffer (0x006898a4,
 * Com_sprintf bound 0x80 at 0x0043a3cf). */
constexpr std::size_t PathBytes = 128;
/* The archive name the engine keeps (0x00ac0d48) and each header string. */
constexpr std::size_t NameBytes = 64;
constexpr std::size_t FieldBytes = 64;

/* The `.ssv` header (SV_ArchiveServerFile 0x00451860): no magic, no version. */
constexpr std::size_t HeaderBytes = 200;
constexpr std::size_t HeaderSaveTime = 0x00;   /* 32-bit time_t, time() 0x004e7d57 */
constexpr std::size_t HeaderLevelTime = 0x04;  /* the level clock in ms */
constexpr std::size_t HeaderComment = 0x08;    /* "%s" or "Starting %s" of configstring 2 */
constexpr std::size_t HeaderMap = 0x48;        /* the BSP name, no `$` target */
constexpr std::size_t HeaderName = 0x88;       /* the archive name itself */

constexpr const char *CurrentName = "current";
/* The load/save list leaves this name out (strcmp 0x0041a68f). */
constexpr const char *QuickName = "quicksave";

constexpr const char *LevelExtension = "sav";
constexpr const char *ServerExtension = "ssv";
constexpr const char *PersistentExtension = "spv";
constexpr const char *ShotExtension = "tga";
/* Com_WipeSaveGame (0x0043a400) deletes them in this order. */
constexpr const char *WipeOrder[4] = { "sav", "ssv", "spv", "tga" };

constexpr const char *AutosaveComment = "Starting %s";      /* 0x0051a1a4 */
constexpr const char *PlainComment = "%s";                  /* 0x0050db54 */
constexpr const char *AutosaveCommand = "wait 1500; pause ; autosavegame ; pause\n"; /* 0x00519e00 */
constexpr const char *SaveshotCommand = "saveshot %s 256 256\n";                     /* 0x0051a18c */
/* SV_SaveGame refuses below this many kilobytes free (0x00451e33). */
constexpr long long MinimumFreeKilobytes = 0x1000;
/* SV_FindSaveName (0x00451ce0) tries four digits after the map's name. */
constexpr int AutosaveNames = 10000;

/* The module's own archive facts, for tests and diagnostics only: the engine
 * never parses a `.sav` or a `.spv`. */
constexpr const char *ArchiveSignature = "Alice Archive Version 1"; /* fgamex86 0x101bb558 */
constexpr int LevelArchiveMajor = 4;   /* fgamex86 0x100a3c30 */
constexpr int LevelArchiveMinor = 8;
constexpr int PersistentVersion = 1;   /* fgamex86 0x100a3a50 */

/* The area portal block after the header: a count and count * count
 * reference counts (CM_WritePortalState 0x004362a0). A snapshot carries 32
 * area bytes, so no map has more than 256 areas. */
constexpr int MaxAreas = 0x100;

/* What the UI's CheckErrorState reads (0x00b59bc0): SV_SaveGame's failure. */
enum class SaveError : int {
	None = 0,
	DiskFull = 1,     /* "Not enough free space to save" */
	Deathmatch = 2,   /* "Can't savegame in a deathmatch" */
	NotInGame = 3,    /* "You must be in a game to save." */
	Dead = 4,         /* "Can't savegame when dead" */
	BadName = 5,      /* "Bad savedir." */
};

inline bool HasTraversal( const char *text ) {
	return !text || std::strstr( text, ".." ) || std::strchr( text, '/' ) || std::strchr( text, '\\' );
}

/* A save name the engine may build paths from. Retail refuses `..`, `/` and
 * `\` only on save (0x00451ef0..0x00451f38) and copies without a bound;
 * fnTech3 refuses them everywhere and refuses what would not fit. */
inline bool NameAllowed( const char *name ) {
	return name && name[0] && std::strlen( name ) < NameBytes && !HasTraversal( name );
}

/* `config` up to its first dot (strchr 0x0043a389). */
inline bool ConfigPrefix( const char *config, char ( &out )[PathBytes] ) {
	out[0] = '\0';
	if ( !config || HasTraversal( config ) ) {
		return false;
	}
	std::size_t length = 0;
	while ( config[length] && config[length] != '.' ) {
		if ( length + 1 >= PathBytes ) {
			return false;
		}
		out[length] = config[length];
		++length;
	}
	out[length] = '\0';
	return true;
}

/* "save/<config>_<name>.<ext>" (0x0043a3cf..0x0043a3e3). `name` may be the
 * list pattern "*". Retail truncates at 128 bytes; this refuses instead. */
inline bool ArchiveFileName( const char *config, const char *name, const char *ext,
	char ( &out )[PathBytes] ) {
	char prefix[PathBytes];
	out[0] = '\0';
	if ( !ConfigPrefix( config, prefix ) || !name || !name[0] || HasTraversal( name )
		|| !ext || !ext[0] || HasTraversal( ext ) ) {
		return false;
	}
	const int written = std::snprintf( out, PathBytes, "%s/%s_%s.%s", Directory, prefix, name, ext );
	if ( written < 0 || static_cast<std::size_t>( written ) >= PathBytes ) {
		out[0] = '\0';
		return false;
	}
	return true;
}

/* The map name with every `/` made `_` (0x00451ea1..0x00451eda). */
inline void AutosavePrefix( const char *mapName, char ( &out )[NameBytes] ) {
	std::size_t i = 0;
	for ( ; mapName && mapName[i] && i + 1 < NameBytes; ++i ) {
		out[i] = mapName[i] == '/' ? '_' : mapName[i];
	}
	out[i] = '\0';
}

/* "%s%i%i%i%i" (SV_FindSaveName 0x00451ce0): garden1 and 42 give garden10042. */
inline bool AutosaveName( const char *prefix, int index, char ( &out )[NameBytes] ) {
	out[0] = '\0';
	if ( !prefix || index < 0 || index >= AutosaveNames ) {
		return false;
	}
	const int written = std::snprintf( out, NameBytes, "%s%i%i%i%i", prefix, index / 1000,
		index / 100 % 10, index / 10 % 10, index % 10 );
	if ( written < 0 || static_cast<std::size_t>( written ) >= NameBytes ) {
		out[0] = '\0';
		return false;
	}
	return true;
}

/* The header, with each string one byte longer than its field so that a
 * field written full -- strncpy leaves no terminator at 64 -- still reads as
 * a terminated string. */
struct ServerHeader {
	int32_t saveTime = 0;
	int32_t levelTime = 0;
	char comment[FieldBytes + 1] = {};
	char mapName[FieldBytes + 1] = {};
	char saveName[FieldBytes + 1] = {};
};

static_assert( HeaderComment == HeaderLevelTime + 4, "the comment follows the two times" );
static_assert( HeaderMap == HeaderComment + FieldBytes, "the map follows the comment" );
static_assert( HeaderName == HeaderMap + FieldBytes, "the name follows the map" );
static_assert( HeaderBytes == HeaderName + FieldBytes, "the name ends the header" );

inline void WriteInt32( uint8_t *to, int32_t value ) {
	const uint32_t v = static_cast<uint32_t>( value );
	to[0] = static_cast<uint8_t>( v );
	to[1] = static_cast<uint8_t>( v >> 8 );
	to[2] = static_cast<uint8_t>( v >> 16 );
	to[3] = static_cast<uint8_t>( v >> 24 );
}

inline int32_t ReadInt32( const uint8_t *from ) {
	return static_cast<int32_t>( static_cast<uint32_t>( from[0] )
		| static_cast<uint32_t>( from[1] ) << 8 | static_cast<uint32_t>( from[2] ) << 16
		| static_cast<uint32_t>( from[3] ) << 24 );
}

/* strncpy: up to 64 bytes and zeros after, no terminator when full. */
inline void WriteField( uint8_t *to, const char *text ) {
	std::size_t i = 0;
	for ( ; text && text[i] && i < FieldBytes; ++i ) {
		to[i] = static_cast<uint8_t>( text[i] );
	}
	for ( ; i < FieldBytes; ++i ) {
		to[i] = 0;
	}
}

inline void ReadField( const uint8_t *from, char ( &out )[FieldBytes + 1] ) {
	std::size_t i = 0;
	for ( ; i < FieldBytes && from[i]; ++i ) {
		out[i] = static_cast<char>( from[i] );
	}
	out[i] = '\0';
}

/* The comment goes through Com_sprintf with a 64-byte bound, so it always
 * ends in a terminator; the map and the name go through strncpy. */
inline void Encode( const ServerHeader &header, uint8_t ( &out )[HeaderBytes] ) {
	std::memset( out, 0, sizeof( out ) );
	WriteInt32( out + HeaderSaveTime, header.saveTime );
	WriteInt32( out + HeaderLevelTime, header.levelTime );
	std::size_t i = 0;
	for ( ; header.comment[i] && i + 1 < FieldBytes; ++i ) {
		out[HeaderComment + i] = static_cast<uint8_t>( header.comment[i] );
	}
	WriteField( out + HeaderMap, header.mapName );
	WriteField( out + HeaderName, header.saveName );
}

inline void Decode( const uint8_t ( &in )[HeaderBytes], ServerHeader &header ) {
	header.saveTime = ReadInt32( in + HeaderSaveTime );
	header.levelTime = ReadInt32( in + HeaderLevelTime );
	ReadField( in + HeaderComment, header.comment );
	ReadField( in + HeaderMap, header.mapName );
	ReadField( in + HeaderName, header.saveName );
}

/* The portal block's length for an area count, or false for a count no map
 * can have. */
inline bool PortalBytes( int numAreas, std::size_t &bytes ) {
	bytes = 0;
	if ( numAreas < 1 || numAreas > MaxAreas ) {
		return false;
	}
	const std::size_t n = static_cast<std::size_t>( numAreas );
	bytes = sizeof( int32_t ) + sizeof( int32_t ) * n * n;
	return true;
}

/* The saveshot and its TGA header; see alice_saveshot.h. */
constexpr int ShotSide = FNTECH3_ALICE_SAVESHOT_SIDE;
constexpr int TgaHeaderBytes = FNTECH3_ALICE_TGA_HEADER_BYTES;
constexpr std::size_t ShotBytes = TgaHeaderBytes + 3u * ShotSide * ShotSide; /* 196,626 */

inline void Saveshot( const uint8_t *rgb, int width, int height, int stride, uint8_t *out,
	int outWidth = ShotSide, int outHeight = ShotSide ) {
	FNTECH3_AliceSaveshot( rgb, width, height, stride, out, outWidth, outHeight );
}

inline void TgaHeader( uint8_t ( &header )[TgaHeaderBytes], int width, int height ) {
	FNTECH3_AliceTgaHeader( header, width, height );
}

/* The game module's import 99 (engine 0x0043a4c0): CRC-16, polynomial
 * 0x1021 through the table at 0x00511bb8, starting at 0xffff with no final
 * xor -- the CRC-16/CCITT-FALSE parameters -- and 0xffff for nothing. The
 * module keeps the low sixteen bits of each script's CRC in its savegames
 * and refuses a load whose script has changed (fgamex86 0x100b30c3). */
inline uint16_t CalcCRC( const uint8_t *data, int length ) {
	uint16_t crc = 0xffff;
	for ( int i = 0; data && i < length; ++i ) {
		crc = static_cast<uint16_t>( crc ^ ( static_cast<uint16_t>( data[i] ) << 8 ) );
		for ( int bit = 0; bit < 8; ++bit ) {
			crc = static_cast<uint16_t>( ( crc & 0x8000 ) ? ( crc << 1 ) ^ 0x1021 : crc << 1 );
		}
	}
	return crc;
}

} // namespace savegame
} // namespace alice
} // namespace fntech3

#endif
