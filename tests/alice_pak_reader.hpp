/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TESTS_ALICE_PAK_READER_HPP
#define FNTECH3_TESTS_ALICE_PAK_READER_HPP

/* Reads single entries out of a legitimate Alice installation's pak0.pk3 for
 * the tests that check this engine's readers against every retail file of a
 * kind. The installation is only read. When it is absent the tests say so
 * and skip that part; nothing retail is ever copied into the repository.
 *
 * The pak is a zip: the central directory is found from its end record, and an
 * entry stored or deflated is returned whole (deflate through the engine's own
 * puff.c). Set FNTECH3_ALICE_PATH to an installation root to override the
 * default of E:/Games/Alice. A pak can also be named by its whole path, as
 * the Heavy Metal: F.A.K.K.2 tests name fakk/pak0.pk3. */

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

extern "C" int32_t puff( uint8_t *dest, uint32_t *destlen, uint8_t *source, uint32_t *sourcelen );

namespace fntech3_tests {

struct PakEntry {
	std::string name;
	std::uint32_t method = 0, compressedSize = 0, size = 0, localOffset = 0;
};

class AlicePak {
public:
	explicit AlicePak( const char *pakName = "pak0.pk3" ) {
#ifdef _MSC_VER
#pragma warning( suppress : 4996 )
#endif
		const char *root = std::getenv( "FNTECH3_ALICE_PATH" );
		Open( std::string( root && root[0] ? root : "E:/Games/Alice" ) + "/base/" + pakName );
	}

	/* A pak by its whole path, for the other Ritual title's installation. */
	struct Path_ { std::string path; };
	explicit AlicePak( const Path_ &whole ) { Open( whole.path ); }

	bool Available() const { return !entries_.empty(); }
	const std::string &Path() const { return path_; }
	const std::vector<PakEntry> &Entries() const { return entries_; }

	bool Read( const PakEntry &entry, std::string &out ) {
		std::uint8_t header[30];
		file_.clear();
		file_.seekg( entry.localOffset );
		if ( !file_.read( reinterpret_cast<char *>( header ), sizeof( header ) ) || U32( header ) != 0x04034b50u ) {
			return false;
		}
		file_.seekg( static_cast<std::streamoff>( entry.localOffset ) + 30 + U16( header + 26 ) + U16( header + 28 ) );
		std::vector<std::uint8_t> packed( entry.compressedSize );
		if ( entry.compressedSize && !file_.read( reinterpret_cast<char *>( packed.data() ), entry.compressedSize ) ) {
			return false;
		}
		if ( entry.method == 0 ) {
			out.assign( packed.begin(), packed.end() );
			return true;
		}
		if ( entry.method != 8 ) return false;
		std::vector<std::uint8_t> unpacked( entry.size ? entry.size : 1 );
		std::uint32_t destLength = entry.size, sourceLength = entry.compressedSize;
		if ( puff( unpacked.data(), &destLength, packed.data(), &sourceLength ) != 0 || destLength != entry.size ) {
			return false;
		}
		out.assign( unpacked.begin(), unpacked.begin() + entry.size );
		return true;
	}

private:
	void Open( const std::string &path ) {
		path_ = path;
		file_.open( path_, std::ios::binary );
		if ( !file_ ) return;
		file_.seekg( 0, std::ios::end );
		const std::streamoff length = file_.tellg();
		const std::streamoff tail = length < 66000 ? length : 66000;
		std::vector<std::uint8_t> end( static_cast<std::size_t>( tail ) );
		file_.seekg( length - tail );
		file_.read( reinterpret_cast<char *>( end.data() ), tail );
		for ( std::streamoff i = tail - 22; i >= 0; --i ) {
			if ( U32( &end[static_cast<std::size_t>( i )] ) == 0x06054b50u ) {
				const std::uint8_t *record = &end[static_cast<std::size_t>( i )];
				ReadDirectory( U16( record + 10 ), U32( record + 12 ), U32( record + 16 ) );
				break;
			}
		}
	}

	static std::uint16_t U16( const std::uint8_t *p ) { return static_cast<std::uint16_t>( p[0] | p[1] << 8 ); }
	static std::uint32_t U32( const std::uint8_t *p ) {
		return static_cast<std::uint32_t>( p[0] ) | static_cast<std::uint32_t>( p[1] ) << 8
			| static_cast<std::uint32_t>( p[2] ) << 16 | static_cast<std::uint32_t>( p[3] ) << 24;
	}

	void ReadDirectory( unsigned count, std::uint32_t size, std::uint32_t offset ) {
		std::vector<std::uint8_t> directory( size );
		file_.clear();
		file_.seekg( offset );
		if ( !file_.read( reinterpret_cast<char *>( directory.data() ), size ) ) return;
		std::size_t at = 0;
		for ( unsigned i = 0; i < count && at + 46 <= directory.size(); ++i ) {
			const std::uint8_t *record = &directory[at];
			if ( U32( record ) != 0x02014b50u ) break;
			PakEntry entry;
			entry.method = U16( record + 10 );
			entry.compressedSize = U32( record + 20 );
			entry.size = U32( record + 24 );
			entry.localOffset = U32( record + 42 );
			const unsigned nameLength = U16( record + 28 );
			if ( at + 46 + nameLength > directory.size() ) break;
			entry.name.assign( reinterpret_cast<const char *>( record + 46 ), nameLength );
			entries_.push_back( entry );
			at += 46 + nameLength + U16( record + 30 ) + U16( record + 32 );
		}
	}

	std::string path_;
	std::ifstream file_;
	std::vector<PakEntry> entries_;
};

inline std::string Lower( std::string text ) {
	for ( char &c : text ) {
		if ( c >= 'A' && c <= 'Z' ) c = static_cast<char>( c - 'A' + 'a' );
	}
	return text;
}

inline bool EndsWith( const std::string &text, const char *suffix ) {
	const std::size_t length = std::strlen( suffix );
	return text.size() >= length && text.compare( text.size() - length, length, suffix ) == 0;
}

} // namespace fntech3_tests

#endif
