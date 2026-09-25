/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3TEST_INSTALL_HPP
#define FNTECH3_Q3TEST_INSTALL_HPP

#include "q3_ihv_install.hpp"
#include "q3test_release.h"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

/* Early installation evidence for the Q3Test releases, not executable
 * authentication or a module loader: no DLL code runs during detection. The
 * Win32 installers put quake3.exe and the modules in the installation root
 * and the paks in demoq3/. Each module's PE TimeDateStamp names the build it
 * belongs to; the stamps and the maps/q3test1.bsp identities below were read
 * from the six Win32 packages recorded in docs/fntech3/Q3TEST.md. */
namespace fntech3::q3test::install {

inline constexpr q3testRelease_t releases[] = {
	{ Q3TEST_RELEASE_1_02, "1.02", "Q3T 1.02", 35, 44,
		0x37228c03u, 0, 0, Q3TEST_CGAME_NONE, 0, 5818936u, 0x00610dfeu },
	{ Q3TEST_RELEASE_1_03, "1.03", "Q3T 1.03", 35, 44,
		0x3727df48u, 0, 0, Q3TEST_CGAME_NONE, 0, 5818936u, 0x00610dfeu },
	{ Q3TEST_RELEASE_1_05, "1.05", "Q3T 1.05", 36, 44,
		0x37369456u, 0x37369443u, 0, Q3TEST_CGAME_TABLE, 0, 5304664u, 0x5024a87au },
	{ Q3TEST_RELEASE_1_06, "1.06", "Q3T 1.06", 38, 45,
		0x378fba49u, 0x378fba41u, 0x378fba4du, Q3TEST_CGAME_TABLE, 1, 5480056u, 0xd02db1a3u },
	{ Q3TEST_RELEASE_1_07, "1.07", "Q3T 1.07", 39, 45,
		0x37940ae0u, 0x37940ad8u, 0x37940ae5u, Q3TEST_CGAME_TABLE, 1, 5480056u, 0xd02db1a3u },
	{ Q3TEST_RELEASE_1_08, "1.08", "Q3T 1.08", 40, 45,
		0x37a67322u, 0x37a671e0u, 0x37a67da4u, Q3TEST_CGAME_SYSCALLS, 1, 5480056u, 0xd02db1a3u },
};
static_assert( sizeof( releases ) / sizeof( releases[0] ) == Q3TEST_RELEASE_COUNT - 1,
	"Each Q3Test release needs one identity" );

/* Every release's demoq3/pak0.pk3 carries the same 1,634-byte default.cfg. */
inline constexpr std::uint32_t defaultConfigSize = 1634u;
inline constexpr std::uint32_t defaultConfigCrc = 0x022ce58au;

inline const q3testRelease_t *ById( q3testReleaseId_t id ) {
	for ( const auto &release : releases ) if ( release.id == id ) return &release;
	return nullptr;
}

inline bool ReadModule( const std::filesystem::path &path, std::vector<std::uint8_t> &data ) {
	std::ifstream file( path, std::ios::binary | std::ios::ate );
	const auto size = static_cast<std::streamoff>( file.tellg() );
	if ( !file || size < 128 || size > 16 * 1024 * 1024 ) return false;
	data.resize( static_cast<std::size_t>( size ) ); file.seekg( 0 );
	return bool( file.read( reinterpret_cast<char *>( data.data() ), size ) );
}

/* The COFF header's TimeDateStamp of an x86 PE image that HasX86Export has
 * already validated. */
inline std::uint32_t Stamp( const std::vector<std::uint8_t> &data ) {
	const std::size_t nt = q3ihv::install::Dword( data.data() + 60 );
	return nt <= data.size() - 12 ? q3ihv::install::Dword( data.data() + nt + 8 ) : 0u;
}

inline bool Exports( const std::vector<std::uint8_t> &data, const char *symbol ) {
	return q3ihv::install::HasX86Export( data.data(), data.size(), symbol );
}

inline bool EqualsFolded( const std::string &name, const char *wanted ) {
	if ( name.size() != std::strlen( wanted ) ) return false;
	for ( std::size_t i = 0; i < name.size(); ++i ) {
		if ( std::tolower( static_cast<unsigned char>( name[i] ) ) != wanted[i] ) return false;
	}
	return true;
}

/* The pak's central directory must hold default.cfg and maps/q3test1.bsp
 * exactly once each, at the identities the release shipped. The retail
 * installers wrote the map name in capitals up to 1.05, so it is compared
 * case-insensitively. PKWARE APPNOTE 6.3.10, sections 4.3.12 and 4.3.16. */
inline bool Pak( const std::filesystem::path &path, const q3testRelease_t &release ) {
	using q3ihv::install::Word; using q3ihv::install::Dword;
	std::ifstream file( path, std::ios::binary | std::ios::ate );
	const auto end = file.tellg();
	if ( !file || end < 22 || end > std::streamoff( UINT32_MAX ) ) return false;
	const auto size = static_cast<std::uint32_t>( end );
	const auto tailSize = size < 65557u ? size : 65557u;
	std::vector<std::uint8_t> tail( tailSize );
	file.seekg( size - tailSize );
	if ( !file.read( reinterpret_cast<char *>( tail.data() ), tailSize ) ) return false;
	std::size_t eocd = tailSize;
	for ( std::size_t pos = tailSize - 22; ; --pos ) {
		if ( Dword( &tail[pos] ) == 0x06054b50u && pos + 22u + Word( &tail[pos + 20] ) == tailSize ) { eocd = pos; break; }
		if ( pos == 0 ) break;
	}
	if ( eocd == tailSize ) return false;
	const auto *e = &tail[eocd];
	const unsigned count = Word( e + 10 );
	const std::uint32_t bytes = Dword( e + 12 ), offset = Dword( e + 16 );
	if ( Word( e + 4 ) || Word( e + 6 ) || Word( e + 8 ) != count || !count || count == 65535
		|| bytes > 8u * 1024u * 1024u || std::uint64_t( offset ) + bytes != size - tailSize + eocd ) return false;
	std::vector<std::uint8_t> directory( bytes );
	file.seekg( offset );
	if ( !file.read( reinterpret_cast<char *>( directory.data() ), bytes ) ) return false;
	unsigned configs = 0, maps = 0; bool configMatches = false, mapMatches = false;
	std::size_t pos = 0;
	for ( unsigned i = 0; i < count; ++i ) {
		if ( bytes - pos < 46 ) return false;
		const auto *h = directory.data() + pos;
		const unsigned nameLength = Word( h + 28 );
		const std::size_t record = 46u + nameLength + Word( h + 30 ) + Word( h + 32 );
		if ( Dword( h ) != 0x02014b50u || record > bytes - pos || !nameLength ) return false;
		const std::string name( reinterpret_cast<const char *>( h + 46 ), nameLength );
		const auto length = Dword( h + 24 ), crc = Dword( h + 16 );
		if ( EqualsFolded( name, "default.cfg" ) ) {
			++configs; configMatches = length == defaultConfigSize && crc == defaultConfigCrc;
		} else if ( EqualsFolded( name, "maps/q3test1.bsp" ) ) {
			++maps; mapMatches = length == release.mapSize && crc == release.mapCrc;
		}
		pos += record;
	}
	return pos == bytes && configs == 1 && configMatches && maps == 1 && mapMatches;
}

/* The release this installation holds, or NONE when any of its evidence is
 * missing, unknown or belongs to another release. A dedicated-server
 * installation may omit the client modules; one that has them must have the
 * release's own. */
inline q3testReleaseId_t Recognize( const std::filesystem::path &path ) {
	std::error_code error;
	const auto root = std::filesystem::canonical( path, error );
	if ( error ) return Q3TEST_RELEASE_NONE;
	std::vector<std::uint8_t> data;
	const auto game = q3ihv::install::ContainedFile( root, "qagamex86.dll" );
	if ( game.empty() || !ReadModule( game, data ) || !Exports( data, "GetGameAPI" ) ) return Q3TEST_RELEASE_NONE;
	const q3testRelease_t *release = nullptr;
	const auto stamp = Stamp( data );
	for ( const auto &candidate : releases ) if ( candidate.gameStamp == stamp ) release = &candidate;
	if ( !release || Exports( data, "GetBotAIAPI" ) != ( release->botAI != 0 ) ) return Q3TEST_RELEASE_NONE;
	const auto cgame = q3ihv::install::ContainedFile( root, "cgamex86.dll" );
	if ( !cgame.empty() ) {
		if ( !release->cgameStamp || !ReadModule( cgame, data ) || Stamp( data ) != release->cgameStamp ) return Q3TEST_RELEASE_NONE;
		const bool table = Exports( data, "GetCGameAPI" );
		const bool syscalls = Exports( data, "dllEntry" ) && Exports( data, "vmMain" );
		if ( release->cgameEntry == Q3TEST_CGAME_TABLE ? !table || syscalls : !syscalls || table ) return Q3TEST_RELEASE_NONE;
	}
	const auto ui = q3ihv::install::ContainedFile( root, "uix86.dll" );
	if ( !ui.empty() && ( !release->uiStamp || !ReadModule( ui, data ) || Stamp( data ) != release->uiStamp
		|| !Exports( data, "GetUIAPI" ) ) ) return Q3TEST_RELEASE_NONE;
	const auto pak = q3ihv::install::ContainedFile( root, "demoq3/pak0.pk3" );
	if ( pak.empty() || !Pak( pak, *release ) ) return Q3TEST_RELEASE_NONE;
	return release->id;
}

} // namespace fntech3::q3test::install
#endif
