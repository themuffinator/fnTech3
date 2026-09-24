/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_ICARUS_FORMAT_HPP
#define FNTECH3_COMPAT_JKA_ICARUS_FORMAT_HPP

/* Bounded reader for Jedi Academy's compiled ICARUS scripts, the `.ibi` files
 * Raven's BehavEd produces. A Jedi Academy game module asks the engine to
 * initialize ICARUS before it has spawned a single entity, so this title's
 * server cannot start a level without reading them.
 *
 * The container was established by reading, not by assumption: every one of
 * the 1,018 scripts a legitimate retail 1.01 installation ships parses this way
 * exactly, with no byte left over and no count reaching past its file. See
 * docs/fntech3/2026-09-21-jka-icarus.md.
 *
 * This reads the container and nothing else. It does not interpret a block, so
 * no block or member number is given a meaning here; it hands the caller each
 * one's number and bytes and lets a later slice decide what they mean. */

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3 {
namespace jka {
namespace icarus {

/* "IBI\0" as it sits in the file, little end first. */
inline constexpr std::uint32_t Ident = ( '\0' << 24 ) | ( 'I' << 16 ) | ( 'B' << 8 ) | 'I';

/* Raven wrote the version as a 32-bit float. Every installed script carries
 * the same one, so it is compared by its exact bits rather than by a tolerance:
 * a script written by a different compiler is a different contract. */
inline constexpr std::uint32_t VersionBits = 0x3FC8F5C3u;   /* 1.57f */

inline constexpr std::size_t HeaderBytes = 8;
inline constexpr std::size_t BlockHeaderBytes = 9;          /* id, count, flags */
inline constexpr std::size_t MemberHeaderBytes = 8;         /* id, size */

enum class Error { None, Size, Ident, Version, Counts, Extent };

inline const char *ErrorName( Error error ) {
	switch ( error ) {
	case Error::None: return "none";
	case Error::Size: return "file too small";
	case Error::Ident: return "wrong identifier";
	case Error::Version: return "unsupported version";
	case Error::Counts: return "count out of range";
	case Error::Extent: return "record past end of file";
	}
	return "unknown";
}

struct Script {
	std::uint32_t versionBits = 0;
	int blocks = 0;
	int members = 0;
	std::size_t memberBytes = 0;
};

/* One member's number and where its bytes are. The bytes are not copied and
 * not interpreted: `offset` and `bytes` always name a range inside the file. */
struct Member {
	std::int32_t id = 0;
	std::size_t offset = 0;
	std::size_t bytes = 0;
};

struct Block {
	std::int32_t id = 0;
	std::uint8_t flags = 0;
	int members = 0;
};

/* Read the eight-byte header, leaving `at` on the first block. */
inline Error ReadHeader( const void *data, std::size_t bytes, std::uint32_t &versionBits,
	std::size_t &at ) {
	const auto *file = static_cast<const std::uint8_t *>( data );
	if ( !file || bytes < HeaderBytes ) return Error::Size;
	std::uint32_t value = 0;
	std::memcpy( &value, file, sizeof( value ) );
	if ( value != Ident ) return Error::Ident;
	std::memcpy( &versionBits, file + 4, sizeof( versionBits ) );
	if ( versionBits != VersionBits ) return Error::Version;
	at = HeaderBytes;
	return Error::None;
}

/* Read one block header at `at`, leaving `at` on its first member. */
inline Error ReadBlock( const void *data, std::size_t bytes, std::size_t &at, Block &block ) {
	const auto *file = static_cast<const std::uint8_t *>( data );
	if ( bytes - at < BlockHeaderBytes ) return Error::Extent;
	std::memcpy( &block.id, file + at, sizeof( block.id ) );
	std::memcpy( &block.members, file + at + 4, sizeof( block.members ) );
	block.flags = file[at + 8];
	at += BlockHeaderBytes;
	if ( block.members < 0 ) return Error::Counts;
	/* Each member needs a header of its own, so a count that cannot fit one
	 * apiece in what is left is a lie the file tells about itself. */
	if ( std::size_t( block.members ) > ( bytes - at ) / MemberHeaderBytes ) {
		return Error::Counts;
	}
	return Error::None;
}

/* Read one member header at `at`, leaving `at` on the next member. */
inline Error ReadMember( const void *data, std::size_t bytes, std::size_t &at, Member &member ) {
	const auto *file = static_cast<const std::uint8_t *>( data );
	if ( bytes - at < MemberHeaderBytes ) return Error::Extent;
	std::int32_t size = 0;
	std::memcpy( &member.id, file + at, sizeof( member.id ) );
	std::memcpy( &size, file + at + 4, sizeof( size ) );
	at += MemberHeaderBytes;
	if ( size < 0 ) return Error::Counts;
	if ( std::size_t( size ) > bytes - at ) return Error::Extent;
	member.offset = at;
	member.bytes = std::size_t( size );
	at += member.bytes;
	return Error::None;
}

/* Walk a script, calling `visit( block, member )` for every member of every
 * block in file order, with `member` null once for a block that has none, so
 * an empty block is offered rather than passed over. Nothing is read outside
 * [data, data + bytes), and the file must end exactly where its last member
 * does. A visitor that does nothing validates. */
template<class Visitor>
inline Error Walk( const void *data, std::size_t bytes, Script &out, Visitor &&visit ) {
	out = {};
	std::size_t at = 0;
	Error error = ReadHeader( data, bytes, out.versionBits, at );
	if ( error != Error::None ) return error;

	while ( at < bytes ) {
		Block block;
		error = ReadBlock( data, bytes, at, block );
		if ( error != Error::None ) return error;
		if ( out.blocks == INT32_MAX ) return Error::Counts;
		++out.blocks;

		if ( block.members == 0 ) {
			visit( block, static_cast<const Member *>( nullptr ) );
			continue;
		}
		for ( int index = 0; index < block.members; ++index ) {
			Member member;
			error = ReadMember( data, bytes, at, member );
			if ( error != Error::None ) return error;
			out.memberBytes += member.bytes;
			++out.members;
			visit( block, &member );
		}
	}
	return at == bytes ? Error::None : Error::Extent;
}

/* Validate a script without looking at any of it. */
inline Error Parse( const void *data, std::size_t bytes, Script &out ) {
	struct Ignore {
		void operator()( const Block &, const Member * ) const {}
	};
	return Walk( data, bytes, out, Ignore{} );
}

/* A member the caller expects to be text: it must end in exactly one
 * terminator and carry none before it, or it is not a string. */
inline bool TerminatedText( const void *data, const Member &member, const char *&text ) {
	const auto *file = static_cast<const std::uint8_t *>( data );
	if ( member.bytes == 0 || file[member.offset + member.bytes - 1] != 0 ) return false;
	if ( std::memchr( file + member.offset, 0, member.bytes - 1 ) != nullptr ) return false;
	text = reinterpret_cast<const char *>( file + member.offset );
	return true;
}

} // namespace icarus
} // namespace jka
} // namespace fntech3

#endif
