/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_ROFF_FORMAT_HPP
#define FNTECH3_COMPAT_JKA_ROFF_FORMAT_HPP

/* Bounded reader for Jedi Academy's ROFF motion paths: the recorded offsets
 * and rotations a scripted object follows, and the note track that fires
 * callbacks along the way. Cameras, lifts and cut-scene props move on these.
 *
 * Two versions are defined. Version 2 carries a frame interval and a note track;
 * version 1 carries neither, and its entries are six floats with no note
 * fields. A legitimate installation ships version 2 only, so version 1 here is
 * read from the format definition rather than observed.
 *
 * Layout: Raven's released sources as carried by OpenJK,
 * codemp/qcommon/RoffSystem.h:33-100. No implementation text is incorporated,
 * and nothing here moves an entity. See docs/fntech3/2026-09-21-jka-roff.md. */

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3 {
namespace jka {
namespace roff {

inline constexpr std::uint32_t Ident = ( 'F' << 24 ) | ( 'F' << 16 ) | ( 'O' << 8 ) | 'R';
inline constexpr int Version1 = 1;
inline constexpr int Version2 = 2;
inline constexpr int SampleRate = 10;               /* Raven's own 10Hz base. */

inline constexpr std::size_t Version1HeaderBytes = 12;
inline constexpr std::size_t Version1EntryBytes = 24;
inline constexpr std::size_t Version2HeaderBytes = 20;
inline constexpr std::size_t Version2EntryBytes = 32;

enum class Error { None, Size, Ident, Version, Counts, Extent, Note, Trailing, Value };

inline const char *ErrorName( Error error ) {
	switch ( error ) {
	case Error::None: return "none";
	case Error::Size: return "file too small";
	case Error::Ident: return "wrong identifier";
	case Error::Version: return "unsupported version";
	case Error::Counts: return "count out of range";
	case Error::Extent: return "record past end of file";
	case Error::Note: return "note index out of range";
	case Error::Trailing: return "note track does not fill the file";
	case Error::Value: return "non-finite motion offset";
	}
	return "unknown";
}

struct Path {
	int version = 0;
	int entries = 0;
	/* Historical source/member spelling; the value is milliseconds per frame,
	 * not frames per second. Retail jamp.exe VA 0x42633b reads header+12 as
	 * frame time and VA 0x426341 divides 1000 by it for the velocity scale. */
	int frameRate = 0;
	int notes = 0;
	int maxNoteIndex = -1;
	std::size_t entryOffset = 0;
	std::size_t noteOffset = 0;
};

inline int FrameMilliseconds( const Path &path )
{
	return path.version == Version2 ? path.frameRate : 1000 / SampleRate;
}

inline int VelocityScale( const Path &path )
{
	const int interval = FrameMilliseconds( path );
	return interval > 0 ? 1000 / interval : 0;
}

/* Version 1 stores its entry count as a float, which the exporter wrote; a
 * value that is not a whole, representable count is a malformed file. */
inline bool WholeCount( float value, int &count ) {
	std::uint32_t bits;
	std::memcpy( &bits, &value, sizeof( bits ) );
	volatile std::uint32_t encoded = bits;
	if ( ( encoded & 0x7f800000u ) == 0x7f800000u ) return false;
	if ( !( value >= 0.0f ) || value > 1024.0f * 1024.0f ) return false;
	const int whole = int( value );
	if ( float( whole ) != value ) return false;
	count = whole;
	return true;
}

/* One recorded step along the path: how far the object moves and how far it
 * turns during that step, and which notes fire when it is reached. Version 1
 * has no notes, so its note range is empty. */
struct Sample {
	float move[3] = { 0.0f, 0.0f, 0.0f };
	float turn[3] = { 0.0f, 0.0f, 0.0f };
	int firstNote = -1;
	int notes = 0;
};

inline std::size_t EntryBytes( const Path &path ) {
	return path.version == Version2 ? Version2EntryBytes : Version1EntryBytes;
}

/* Read one step. False when there is no such step, which is how the end of a
 * path is reached rather than by running off it. */
inline bool ReadSample( const void *data, std::size_t bytes, const Path &path,
	int index, Sample &out )
{
	if ( !data || index < 0 || index >= path.entries ) return false;
	const std::size_t stride = EntryBytes( path );
	const std::size_t at = path.entryOffset + std::size_t( index ) * stride;
	if ( at + stride > bytes ) return false;
	const auto *file = static_cast<const std::uint8_t *>( data );
	out = {};
	std::memcpy( out.move, file + at, sizeof( out.move ) );
	std::memcpy( out.turn, file + at + 12, sizeof( out.turn ) );
	if ( path.version == Version2 ) {
		std::memcpy( &out.firstNote, file + at + 24, sizeof( out.firstNote ) );
		std::memcpy( &out.notes, file + at + 28, sizeof( out.notes ) );
	}
	return true;
}

/* A note by its number, as a pointer into the file's own track, or null. The
 * track is terminated strings, and Parse has already proved every one of them
 * ends inside the file. */
inline const char *Note( const void *data, std::size_t bytes, const Path &path, int index )
{
	if ( !data || index < 0 || index >= path.notes ) return nullptr;
	const auto *file = static_cast<const char *>( data );
	std::size_t at = path.noteOffset;
	for ( int note = 0; note < index; ++note ) {
		if ( at >= bytes ) return nullptr;
		const void *end = std::memchr( file + at, 0, bytes - at );
		if ( !end ) return nullptr;
		at = std::size_t( static_cast<const char *>( end ) - file ) + 1;
	}
	return at < bytes ? file + at : nullptr;
}

inline Error Parse( const void *data, std::size_t bytes, Path &out ) {
	const auto *file = static_cast<const std::uint8_t *>( data );
	out = {};
	if ( !file || bytes < Version1HeaderBytes ) return Error::Size;

	const auto word = [file]( std::size_t offset ) {
		std::uint32_t value = 0;
		std::memcpy( &value, file + offset, sizeof( value ) );
		return value;
	};
	const auto integer = [file]( std::size_t offset ) {
		std::int32_t value = 0;
		std::memcpy( &value, file + offset, sizeof( value ) );
		return value;
	};

	if ( word( 0 ) != Ident ) return Error::Ident;
	out.version = integer( 4 );
	if ( out.version != Version1 && out.version != Version2 ) return Error::Version;

	std::size_t stride = Version1EntryBytes;
	if ( out.version == Version1 ) {
		float count = 0;
		std::memcpy( &count, file + 8, sizeof( count ) );
		if ( !WholeCount( count, out.entries ) ) return Error::Counts;
		out.entryOffset = Version1HeaderBytes;
	} else {
		if ( bytes < Version2HeaderBytes ) return Error::Size;
		out.entries = integer( 8 );
		out.frameRate = integer( 12 );
		out.notes = integer( 16 );
		stride = Version2EntryBytes;
		out.entryOffset = Version2HeaderBytes;
		if ( out.entries < 0 || out.frameRate <= 0 || out.notes < 0 ) return Error::Counts;
	}

	const std::size_t available = bytes - out.entryOffset;
	if ( std::size_t( out.entries ) > available / stride ) return Error::Extent;
	out.noteOffset = out.entryOffset + std::size_t( out.entries ) * stride;
	/* Motion must not put an entity's origin, angles or collision bounds at a
	 * NaN or infinity. Test float encodings so this remains true under fast math. */
	for ( int entry = 0; entry < out.entries; ++entry ) {
		const std::size_t at = out.entryOffset + std::size_t( entry ) * stride;
		for ( int scalar = 0; scalar < 6; ++scalar ) {
			if ( ( word( at + std::size_t( scalar ) * 4 ) & 0x7f800000u ) == 0x7f800000u ) {
				return Error::Value;
			}
		}
	}

	if ( out.version == Version1 ) {
		/* Version 1 has no note track, so the entries end the file. */
		return out.noteOffset == bytes ? Error::None : Error::Trailing;
	}

	/* Every entry's note range must land inside the track that follows. */
	for ( int entry = 0; entry < out.entries; ++entry ) {
		const std::size_t at = out.entryOffset + std::size_t( entry ) * stride;
		const std::int32_t start = integer( at + 24 );
		const std::int32_t count = integer( at + 28 );
		if ( start < -1 || count < 0 ) return Error::Note;
		if ( start >= 0 ) {
			if ( count > out.notes - start ) return Error::Note;
			if ( start + count - 1 > out.maxNoteIndex ) out.maxNoteIndex = start + count - 1;
		}
	}

	/* The track is that many terminated strings, and nothing else. */
	std::size_t at = out.noteOffset;
	for ( int note = 0; note < out.notes; ++note ) {
		if ( at >= bytes ) return Error::Extent;
		const void *end = std::memchr( file + at, 0, bytes - at );
		if ( !end ) return Error::Extent;
		at = std::size_t( static_cast<const std::uint8_t *>( end ) - file ) + 1;
	}
	return at == bytes ? Error::None : Error::Trailing;
}

} // namespace roff
} // namespace jka
} // namespace fntech3

#endif
