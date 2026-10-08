/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_SP_SAVEGAME_HPP
#define FNTECH3_STVEF_SP_SAVEGAME_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

/*
 * Elite Force SP's savegame files, saves/<name>.sav (stvoy.exe 0x426760 to
 * 0x427e70). A save is a run of chunks: the chunk's id, its data's length, a
 * block checksum of the data, the data, then the magic 0x1234ABCD, each word
 * little-endian. The engine writes its chunks around the game's: a comment,
 * a 256x256 screenshot, the map, the userinfo cvars and the player data a
 * transition carries, and for a full save the level's times, its area
 * portals and its configstrings; the game's WriteLevel follows.
 */
namespace fntech3::stvef::savegame {

/* A chunk id holds its name's four characters, the first most significant:
 * retail prints the name that way, and it reads backwards in the file. */
constexpr std::uint32_t Id( const char ( &name )[5] ) {
	return std::uint32_t( std::uint8_t( name[0] ) ) << 24 | std::uint32_t( std::uint8_t( name[1] ) ) << 16 |
		std::uint32_t( std::uint8_t( name[2] ) ) << 8 | std::uint8_t( name[3] );
}

inline constexpr std::uint32_t Comment = Id( "COMM" );
inline constexpr std::uint32_t Screenshot = Id( "SHOT" );
inline constexpr std::uint32_t MapName = Id( "MPCM" );
inline constexpr std::uint32_t CvarCount = Id( "CVCN" );
inline constexpr std::uint32_t CvarName = Id( "CVAR" );
inline constexpr std::uint32_t CvarValue = Id( "VALU" );
inline constexpr std::uint32_t Autosave = Id( "GAME" );
inline constexpr std::uint32_t PlayerSave = Id( "CVSV" );
inline constexpr std::uint32_t PlayerAmmo = Id( "AMMO" );
inline constexpr std::uint32_t BorgAdapt = Id( "ADPT" );
inline constexpr std::uint32_t Time = Id( "TIME" );
inline constexpr std::uint32_t TimeResidual = Id( "TIMR" );
inline constexpr std::uint32_t Portals = Id( "PRTS" );
inline constexpr std::uint32_t ConfigstringCount = Id( "CSCN" );
inline constexpr std::uint32_t ConfigstringIndex = Id( "CSIN" );
inline constexpr std::uint32_t ConfigstringData = Id( "CSDA" );

inline constexpr std::uint32_t Magic = 0x1234ABCDu;
// Id, length and checksum before the data, the magic after it.
inline constexpr std::uint32_t FrameBytes = 16;
inline constexpr std::uint32_t CommentBytes = 0x80;
inline constexpr int ScreenshotSide = 256;
inline constexpr std::uint32_t ScreenshotBytes = ScreenshotSide * ScreenshotSide * 4;
inline constexpr std::uint32_t MapNameBytes = 0x400;
// Each of the player's carried values is kept as a 1 KB string.
inline constexpr std::uint32_t CarriedTextBytes = 0x400;
inline constexpr int PlayerAmmoCount = 4;
inline constexpr int BorgAdaptCount = 32;
// Configstring 1, the system info, is not kept (0x426db5).
inline constexpr int SkippedConfigstring = 1;

inline void Name( std::uint32_t id, char ( &name )[5] ) {
	name[0] = static_cast<char>( id >> 24 );
	name[1] = static_cast<char>( id >> 16 );
	name[2] = static_cast<char>( id >> 8 );
	name[3] = static_cast<char>( id );
	name[4] = '\0';
}

/* The save and load commands refuse a name with "..", "/" or "\" in it
 * (0x426c31, 0x42699d); the engine's own names, such as hub/<map>, do not
 * pass through them. */
inline bool NameAllowed( const char *name ) {
	return name && !std::strstr( name, ".." ) && !std::strchr( name, '/' ) && !std::strchr( name, '\\' );
}

/* An autosave's comment names the level it starts (0x42706e). */
inline void AutosaveComment( const char *map, char ( &comment )[CommentBytes] ) {
	std::snprintf( comment, sizeof( comment ), "-------> %s", map );
}

/* Any other save without a comment of its own is labelled with the time, the
 * date and the level: _strtime, two spaces, _strdate with two spaces, then
 * "  %s" with the map, within the 127 characters (0x426fd9). */
inline void DatedComment( int hour, int minute, int second, int month, int day, int year, const char *map,
	char ( &comment )[CommentBytes] ) {
	char text[CommentBytes * 2]{};
	std::snprintf( text, sizeof( text ), "%02d:%02d:%02d  %02d/%02d/%02d    %s", hour % 100, minute % 100,
		second % 100, month % 100, day % 100, year % 100, map );
	std::memcpy( comment, text, CommentBytes - 1 );
	comment[CommentBytes - 1] = '\0';
}

/* From byte 64 a save's comment chunk also holds the line the load and save
 * menus list beside the comment: _strtime, _strdate followed by two spaces
 * and the level, as "%s %s %s" within 63 characters, copied without its
 * terminator (0x427086..0x427115). The chunk is cleared before either is
 * written (0x426f72..0x426f88), so the rest of it is zero, and a comment
 * longer than 63 characters runs into the line. */
inline constexpr std::size_t CommentInfoOffset = 0x40;

inline void CommentInfo( int hour, int minute, int second, int month, int day, int year, const char *map,
	char ( &comment )[CommentBytes] ) {
	char text[CommentBytes - CommentInfoOffset]{};
	std::snprintf( text, sizeof( text ), "%02d:%02d:%02d %02d/%02d/%02d   %s", hour % 100, minute % 100,
		second % 100, month % 100, day % 100, year % 100, map ? map : "" );
	std::memcpy( comment + CommentInfoOffset, text, std::strlen( text ) );
}

/* Little-endian words for the chunk frame. */
inline void PutWord( unsigned char *destination, std::uint32_t value ) {
	destination[0] = static_cast<unsigned char>( value );
	destination[1] = static_cast<unsigned char>( value >> 8 );
	destination[2] = static_cast<unsigned char>( value >> 16 );
	destination[3] = static_cast<unsigned char>( value >> 24 );
}

inline std::uint32_t GetWord( const unsigned char *source ) {
	return std::uint32_t( source[0] ) | std::uint32_t( source[1] ) << 8 | std::uint32_t( source[2] ) << 16 |
		std::uint32_t( source[3] ) << 24;
}

/* A chunk's frame: its id, its data's length and the data's checksum, then
 * the data and the magic. */
inline void PutFrameHeader( std::uint32_t id, std::uint32_t length, std::uint32_t checksum,
	unsigned char ( &header )[12] ) {
	PutWord( header, id );
	PutWord( header + 4, length );
	PutWord( header + 8, checksum );
}

inline void PutFrameTrailer( unsigned char ( &trailer )[4] ) { PutWord( trailer, Magic ); }

/* What a chunk read found, in the order retail tests it (0x427b60). */
enum class ReadResult {
	Ok,
	WrongId,      // another chunk is here: refused, or the end of an optional read
	WrongLength,  // the chunk is here with a length the caller did not ask for
	Truncated,    // the frame runs past the end of the save
	BadChecksum,
	BadMagic
};

struct Chunk {
	std::uint32_t id;            // the chunk found here, however the read ended
	std::uint32_t length;        // its data's length
	const unsigned char *data;   // its data, once the frame is whole
	std::size_t next;            // where the next chunk begins
};

/* One chunk of a save held whole in memory. A read asks for a chunk by id,
 * and by length unless it takes any (retail's zero length, 0x427c8b). The
 * checksum is the engine's block checksum over the data. */
template <typename Checksum>
ReadResult ReadChunk( const unsigned char *bytes, std::size_t size, std::size_t offset,
	std::uint32_t wanted, std::uint32_t wantedLength, Chunk &chunk, Checksum checksum ) {
	chunk = Chunk{};
	chunk.next = offset;
	if ( !bytes || offset > size ) {
		return ReadResult::WrongId;
	}
	const std::size_t left = size - offset;
	// Retail reads the id and the length whatever is left, so the end of a
	// save answers an optional read with another chunk's id rather than an
	// error; a short id reads as zero.
	unsigned char head[8] = {};
	std::memcpy( head, bytes + offset, left < sizeof( head ) ? left : sizeof( head ) );
	chunk.id = GetWord( head );
	chunk.length = GetWord( head + 4 );
	if ( chunk.id != wanted ) {
		return ReadResult::WrongId;
	}
	if ( wantedLength != 0 && wantedLength != chunk.length ) {
		return ReadResult::WrongLength;
	}
	if ( left < FrameBytes || chunk.length > left - FrameBytes ) {
		return ReadResult::Truncated;
	}
	const unsigned char *data = bytes + offset + 12;
	if ( GetWord( bytes + offset + 8 ) != checksum( data, chunk.length ) ) {
		return ReadResult::BadChecksum;
	}
	if ( GetWord( data + chunk.length ) != Magic ) {
		return ReadResult::BadMagic;
	}
	chunk.data = data;
	chunk.next = offset + FrameBytes + chunk.length;
	return ReadResult::Ok;
}

/* A picture fitted to a savegame's, as retail's renderer fits a levelshot
 * (stvoy.exe 0x44ff50): each pixel of the result averages the source pixels
 * from its own place across to the next one's, over as many rows, and is
 * opaque. Both pictures are RGBA with their top row first. */
inline void Resample( const unsigned char *source, int sourceWidth, int sourceHeight,
	unsigned char *destination, int width, int height ) {
	if ( !source || !destination || sourceWidth <= 0 || sourceHeight <= 0 || width <= 0 || height <= 0 ) {
		return;
	}
	const float xStep = static_cast<float>( static_cast<double>( sourceWidth ) / width );
	const float yStep = static_cast<float>( static_cast<double>( sourceHeight ) / height );
	const int columns = static_cast<int>( std::ceil( xStep ) );
	const int rows = static_cast<int>( std::ceil( yStep ) );
	const int divisor = ( columns > 0 ? columns : 1 ) * ( rows > 0 ? rows : 1 );
	for ( int y = 0; y < height; ++y ) {
		const double top = y * static_cast<double>( yStep );
		const double bottom = ( y + 1 ) * static_cast<double>( yStep );
		for ( int x = 0; x < width; ++x ) {
			const double left = x * static_cast<double>( xStep );
			const double right = ( x + 1 ) * static_cast<double>( xStep );
			long sum[3] = { 0, 0, 0 };
			for ( double sy = top; sy < bottom; sy += 1.0 ) {
				int row = static_cast<int>( sy );
				if ( row >= sourceHeight ) row = sourceHeight - 1;
				for ( double sx = left; sx < right; sx += 1.0 ) {
					int column = static_cast<int>( sx );
					if ( column >= sourceWidth ) column = sourceWidth - 1;
					const unsigned char *pixel = source + ( static_cast<std::size_t>( row ) * sourceWidth + column ) * 4;
					sum[0] += pixel[0];
					sum[1] += pixel[1];
					sum[2] += pixel[2];
				}
			}
			unsigned char *pixel = destination + ( static_cast<std::size_t>( y ) * width + x ) * 4;
			pixel[0] = static_cast<unsigned char>( sum[0] / divisor );
			pixel[1] = static_cast<unsigned char>( sum[1] / divisor );
			pixel[2] = static_cast<unsigned char>( sum[2] / divisor );
			pixel[3] = 255;
		}
	}
}

/* A savegame keeps its picture the way the screen is read, the bottom row
 * first, so a picture read from a file is turned over (the vertical flip of
 * retail's 0x45017e). */
inline void FlipRows( unsigned char *rgba, int width, int height ) {
	if ( !rgba || width <= 0 || height <= 0 ) {
		return;
	}
	const std::size_t stride = static_cast<std::size_t>( width ) * 4;
	for ( int y = 0; y < height / 2; ++y ) {
		unsigned char *top = rgba + static_cast<std::size_t>( y ) * stride;
		unsigned char *bottom = rgba + static_cast<std::size_t>( height - 1 - y ) * stride;
		for ( std::size_t i = 0; i < stride; ++i ) {
			const unsigned char keep = top[i];
			top[i] = bottom[i];
			bottom[i] = keep;
		}
	}
}

} // namespace fntech3::stvef::savegame

#endif
