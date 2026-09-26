/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Jedi Academy installation identity and startup contracts. The archives below
 * are synthetic directory metadata written by this test: they declare the names,
 * sizes and checksums a retail base/assets2.pk3 declares, and carry no retail
 * payload. Passing a legitimate installation directory as the first argument
 * probes that installation as well. */

#include "../code/qcommon/game_manager.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::fprintf( stderr, "%d: %s\n", __LINE__, #x ); std::exit( 1 ); } } while ( 0 )

namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;

/* Retail Jedi Academy 1.01, observed in a legitimate installation: both
 * configurations live in base/assets2.pk3, and the maps live elsewhere. */
struct Member {
	const char *name;
	std::uint32_t size;
	std::uint32_t crc;
};
static constexpr Member RetailConfig{ "default.cfg", 2178, 0xfb11a25bu };
static constexpr Member RetailMpConfig{ "mpdefault.cfg", 2845, 0x4999454au };

static void Put( Bytes &data, std::uint32_t value, unsigned bytes )
{
	for ( unsigned i = 0; i < bytes; ++i ) {
		data.push_back( std::uint8_t( value >> ( 8 * i ) ) );
	}
}

static void PutName( Bytes &data, const char *name )
{
	for ( const char *p = name; *p; ++p ) data.push_back( std::uint8_t( *p ) );
}

/* One stored byte per member, with the directory declaring the retail name,
 * size and checksum. The identity check reads the directory alone. */
static Bytes Archive( const std::vector<Member> &members )
{
	Bytes local, directory;
	for ( const auto &member : members ) {
		const std::uint32_t offset = std::uint32_t( local.size() );
		const std::uint32_t nameLength = std::uint32_t( std::strlen( member.name ) );
		Put( local, 0x04034b50u, 4 );
		Put( local, 20, 2 );                 // version needed
		Put( local, 0, 2 );                  // flags
		Put( local, 0, 2 );                  // method: stored
		Put( local, 0, 2 );                  // time
		Put( local, 0, 2 );                  // date
		Put( local, member.crc, 4 );
		Put( local, 1, 4 );                  // compressed size
		Put( local, member.size, 4 );        // uncompressed size
		Put( local, nameLength, 2 );
		Put( local, 0, 2 );                  // extra length
		PutName( local, member.name );
		local.push_back( 'x' );

		Put( directory, 0x02014b50u, 4 );
		Put( directory, 20, 2 );             // version made by
		Put( directory, 20, 2 );             // version needed
		Put( directory, 0, 2 );              // flags
		Put( directory, 0, 2 );              // method
		Put( directory, 0, 2 );              // time
		Put( directory, 0, 2 );              // date
		Put( directory, member.crc, 4 );
		Put( directory, 1, 4 );              // compressed size
		Put( directory, member.size, 4 );    // uncompressed size
		Put( directory, nameLength, 2 );
		Put( directory, 0, 2 );              // extra length
		Put( directory, 0, 2 );              // comment length
		Put( directory, 0, 2 );              // disk number start
		Put( directory, 0, 2 );              // internal attributes
		Put( directory, 0, 4 );              // external attributes
		Put( directory, offset, 4 );
		PutName( directory, member.name );
	}
	Bytes file = local;
	const std::uint32_t directoryOffset = std::uint32_t( file.size() );
	file.insert( file.end(), directory.begin(), directory.end() );
	Put( file, 0x06054b50u, 4 );
	Put( file, 0, 2 );                       // this disk
	Put( file, 0, 2 );                       // directory disk
	Put( file, std::uint32_t( members.size() ), 2 );
	Put( file, std::uint32_t( members.size() ), 2 );
	Put( file, std::uint32_t( directory.size() ), 4 );
	Put( file, directoryOffset, 4 );
	Put( file, 0, 2 );                       // comment length
	return file;
}

static void Write( const fs::path &path, const Bytes &data )
{
	std::ofstream file( path, std::ios::binary | std::ios::trunc );
	file.write( reinterpret_cast<const char *>( data.data() ),
		static_cast<std::streamsize>( data.size() ) );
	CHECK( file.good() );
}

static void TestInstallationIdentity()
{
	const fs::path root = fs::temp_directory_path() / ( "fntech3-jka-test-"
		+ std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	CHECK( fs::create_directory( root ) );
	// This unique, newly created directory holds only test-authored fixtures.
	struct Cleanup {
		fs::path path;
		~Cleanup() { std::error_code error; fs::remove_all( path, error ); }
	} cleanup{ root };
	const fs::path base = root / "base";
	CHECK( fs::create_directory( base ) );
	const auto probe = [&root] { return Com_ProbeGameProfileMarkers( root.u8string().c_str() ); };

	CHECK( probe() == COM_GAME_MARKER_NONE );

	// One installation carries both modes, so the pak sets both marks.
	Write( base / "assets2.pk3", Archive( { RetailConfig, RetailMpConfig } ) );
	CHECK( probe() == ( COM_GAME_MARKER_JKA_SP | COM_GAME_MARKER_JKA_MP ) );
	comGameMarkerInputs_t both{ "auto", COM_GAME_MARKER_JKA_SP | COM_GAME_MARKER_JKA_MP, 0, 0 };
	const auto result = Com_DetectGameProfileMarkers( &both );
	CHECK( result.status == COM_GAME_DETECTION_MODE_REQUIRED && !result.profile );
	CHECK( result.evidence == COM_GAME_EVIDENCE_BASE_PATH );

	// Another title's installation beside this one stays ambiguous.
	comGameMarkerInputs_t mixed{ "auto",
		COM_GAME_MARKER_JKA_MP | COM_GAME_MARKER_QUAKE3, 0, 0 };
	CHECK( Com_DetectGameProfileMarkers( &mixed ).status == COM_GAME_DETECTION_AMBIGUOUS );

	// A pak that declares another size, another checksum, or only one of the
	// two configurations, is an unrecognized pak rather than this title.
	const Member wrongSize{ "default.cfg", 2179, RetailConfig.crc };
	const Member wrongCrc{ "default.cfg", RetailConfig.size, 0x00000000u };
	const Member wrongCompanionSize{ "mpdefault.cfg", 2846, RetailMpConfig.crc };
	const Member foreignMap{ "maps/q3dm6.bsp", 4408212, 0x9fef9a4bu };
	for ( const auto &members : std::vector<std::vector<Member>>{
		{ wrongSize, RetailMpConfig },
		{ wrongCrc, RetailMpConfig },
		{ RetailConfig, wrongCompanionSize },
		{ RetailConfig },
		{ RetailMpConfig },
		{ RetailConfig, RetailMpConfig, RetailConfig },
		{ RetailConfig, RetailMpConfig, foreignMap } } ) {
		Write( base / "assets2.pk3", Archive( members ) );
		CHECK( probe() == COM_GAME_MARKER_UNRECOGNIZED_PAK );
	}

	// A file that is not an archive at all supplies no evidence either way.
	Write( base / "assets2.pk3", Bytes( 64, 0x5a ) );
	CHECK( probe() == COM_GAME_MARKER_UNRECOGNIZED_PAK );
	Write( base / "assets2.pk3", Bytes() );
	CHECK( probe() == COM_GAME_MARKER_UNRECOGNIZED_PAK );

	// The pak names the installation; the modules beside it do not. An
	// assets0.pk3 that cannot be read is now itself unrecognized rather than
	// nothing, because that is the pak Jedi Outcast keeps its identity in.
	fs::remove( base / "assets2.pk3" );
	Write( root / "jagamex86.dll", Bytes( 512, 0x4d ) );
	Write( base / "assets0.pk3", Bytes( 512, 0x50 ) );
	CHECK( probe() == COM_GAME_MARKER_UNRECOGNIZED_PAK );
	fs::remove( base / "assets0.pk3" );
	CHECK( probe() == COM_GAME_MARKER_NONE );

	CHECK( Com_ProbeGameProfileMarkers( "" ) == COM_GAME_MARKER_NONE );
	CHECK( Com_ProbeGameProfileMarkers( nullptr ) == COM_GAME_MARKER_NONE );
}

static void TestProfiles()
{
	const auto *sp = Com_GameProfileById( COM_GAME_JKA_SP );
	const auto *mp = Com_GameProfileById( COM_GAME_JKA_MP );
	CHECK( sp && mp && sp->titleId == COM_GAME_TITLE_JKA && mp->titleId == COM_GAME_TITLE_JKA );
	const auto *title = Com_GameManagerTitleById( COM_GAME_TITLE_JKA );
	CHECK( title && !std::strcmp( title->key, "jka" ) );
	CHECK( Com_GameManagerTitleProfileCount( COM_GAME_TITLE_JKA ) == 2 );
	CHECK( Com_GameManagerTitleProfileAt( COM_GAME_TITLE_JKA, 0 ) == sp );
	CHECK( Com_GameManagerTitleProfileAt( COM_GAME_TITLE_JKA, 1 ) == mp );

	for ( const char *key : { "jka-sp", "jasp", "jediacademy-sp", "jedi-academy-sp" } ) {
		CHECK( Com_GameManagerFindProfile( key ) == sp );
	}
	for ( const char *key : { "jka-mp", "jamp", "jediacademy-mp", "jedi-academy-mp" } ) {
		CHECK( Com_GameManagerFindProfile( key ) == mp );
	}
	// The bare title key asks for a mode rather than guessing one.
	comGameMarkerInputs_t bare{ "jka", 0, 0, 0 };
	CHECK( Com_DetectGameProfileMarkers( &bare ).status == COM_GAME_DETECTION_MODE_REQUIRED );

	// Both modes read one base/ directory and one identifying pak.
	CHECK( !std::strcmp( sp->baseGame, "base" ) && !std::strcmp( mp->baseGame, "base" ) );
	CHECK( !std::strcmp( sp->basePakName, "assets2.pk3" ) );
	CHECK( !std::strcmp( mp->basePakName, "assets2.pk3" ) );
	CHECK( !std::strcmp( sp->clientConfig, "jaconfig.cfg" ) );
	CHECK( !std::strcmp( mp->clientConfig, "jampconfig.cfg" ) );
	CHECK( !sp->serverCacheFile && mp->serverCacheFile );
	CHECK( !std::strcmp( sp->officialPakStem, "assets" ) );
	CHECK( !std::strcmp( mp->officialPakStem, "assets" ) );

	// Retail limits: Raven's own configstring layout, not Quake III's.
	CHECK( sp->maxClients == 1 && sp->maxConfigstrings == 1300 && sp->playerConfigstringBase == 743 );
	CHECK( mp->maxClients == 32 && mp->maxConfigstrings == 1700 && mp->playerConfigstringBase == 1131 );
	CHECK( sp->collisionModelHandleCount == 512 && mp->collisionModelHandleCount == 512 );
	CHECK( sp->entityHullPolicy == COM_ENTITY_HULL_TRACE_SHAPE );
	CHECK( mp->entityHullPolicy == COM_ENTITY_HULL_TRACE_SHAPE );

	// No Steam identity has been observed, and neither mode claims a service.
	CHECK( !sp->usesSteam && !mp->usesSteam && !sp->supportsWorkshop && !mp->supportsWorkshop );
	CHECK( !sp->steamAppId && !mp->steamAppId && !sp->steamInstallName && !mp->steamInstallName );

	// Both stay blocked in ordinary builds; registration is not a claim.
	CHECK( sp->runtimeBlocker && mp->runtimeBlocker );
#if !defined( FNTECH3_JKA_DEVELOPMENT )
	CHECK( !Com_GameManagerCanActivate( sp ) && !Com_GameManagerCanActivate( mp ) );
	CHECK( !Com_ActivateGameProfile( COM_GAME_JKA_SP ) );
	CHECK( !Com_ActivateGameProfile( COM_GAME_JKA_MP ) );
	comGameMarkerInputs_t explicitRequest{ "jka-mp", 0, 0, 0 };
	CHECK( Com_DetectGameProfileMarkers( &explicitRequest ).status
		== COM_GAME_DETECTION_INCOMPLETE_ENGINE );
#endif
}

static void TestProtocolsAndModules()
{
	const auto *sp = Com_GameProfileById( COM_GAME_JKA_SP );
	const auto *mp = Com_GameProfileById( COM_GAME_JKA_MP );

	// Jedi Academy shares both of its version numbers with Elite Force's
	// families, so a version names every family and each profile admits one.
	// Three single players speak 40: Elite Force's, Jedi Academy's and Jedi
	// Outcast's. So does Q3Test 1.08.
	CHECK( Com_ProtocolFamilyForVersion( 40 )
		== ( COM_PROTOCOL_FAMILY_STVEF_SP | COM_PROTOCOL_FAMILY_JKA_SP
			| COM_PROTOCOL_FAMILY_JK2_SP | COM_PROTOCOL_FAMILY_Q3TEST ) );
	CHECK( Com_ProtocolFamilyForVersion( 26 )
		== ( COM_PROTOCOL_FAMILY_IOEF | COM_PROTOCOL_FAMILY_JKA_MP ) );
	CHECK( Com_GameProfileAllowsProtocol( sp, 40 ) );
	CHECK( Com_GameProfileAllowsProtocol( mp, 26 ) );
	CHECK( Com_GameProfileAllowsProtocol( Com_GameProfileById( COM_GAME_STVEF_SP ), 40 ) );
	CHECK( Com_GameProfileAllowsProtocol( Com_GameProfileById( COM_GAME_STVEF_MP ), 26 ) );
	CHECK( !Com_GameProfileAllowsProtocol( sp, 26 ) );
	CHECK( !Com_GameProfileAllowsProtocol( mp, 40 ) );
	for ( int protocol : { 24, 33, 49, 60, 66, 68, 71, 84, 91 } ) {
		CHECK( !Com_GameProfileAllowsProtocol( sp, protocol ) );
		CHECK( !Com_GameProfileAllowsProtocol( mp, protocol ) );
	}
	for ( auto other : { COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP,
		COM_GAME_RTCW_MP, COM_GAME_WOLFET, COM_GAME_QUAKE3_IHV } ) {
		CHECK( !Com_GameProfileAllowsProtocol( Com_GameProfileById( other ), 26 ) );
		CHECK( !Com_GameProfileAllowsProtocol( Com_GameProfileById( other ), 40 ) );
	}

	// Single player runs one function-table library; multiplayer runs Raven's
	// three native modules, and neither mode accepts a QVM.
	CHECK( Com_GameProfileAllowsModuleApi( sp, COM_MODULE_API_JKA_SP_NATIVE ) );
	CHECK( Com_GameProfileAllowsModuleApi( mp, COM_MODULE_API_LEGACY_NATIVE ) );
	for ( unsigned api : { COM_MODULE_API_QVM, COM_MODULE_API_LEGACY_NATIVE,
		COM_MODULE_API_STRUCTURED_NATIVE, COM_MODULE_API_STVEF_SP_NATIVE,
		COM_MODULE_API_QUAKE3_IHV_NATIVE } ) {
		CHECK( !Com_GameProfileAllowsModuleApi( sp, api ) );
	}
	for ( unsigned api : { COM_MODULE_API_QVM, COM_MODULE_API_STRUCTURED_NATIVE,
		COM_MODULE_API_STVEF_SP_NATIVE, COM_MODULE_API_QUAKE3_IHV_NATIVE,
		COM_MODULE_API_JKA_SP_NATIVE } ) {
		CHECK( !Com_GameProfileAllowsModuleApi( mp, api ) );
	}
	CHECK( !Com_GameProfileAllowsModuleApi( Com_GameProfileById( COM_GAME_STVEF_SP ),
		COM_MODULE_API_JKA_SP_NATIVE ) );

	char name[64];
	int installationRoot = -1;
	struct Expected { comGameId_t id; const char *module, *leaf; int root; };
	/* Retail keeps jagamex86.dll beside jasp.exe in the installation root,
	 * and ships multiplayer's three modules inside base/assets2.pk3 and
	 * assets3.pk3, which the filesystem serves like any other pak member. */
	for ( auto expected : { Expected{ COM_GAME_JKA_SP, "qagame", "jagamex86.dll", 1 },
		{ COM_GAME_JKA_SP, "cgame", "jagamex86.dll", 1 },
		{ COM_GAME_JKA_MP, "qagame", "jampgamex86.dll", 0 },
		{ COM_GAME_JKA_MP, "cgame", "cgamex86.dll", 0 },
		{ COM_GAME_JKA_MP, "ui", "uix86.dll", 0 } } ) {
		const auto *profile = Com_GameProfileById( expected.id );
		CHECK( Com_GameManagerNativeModuleFilename( profile, expected.module,
			"x86", ".dll", 0, name, sizeof( name ) ) );
		CHECK( !std::strcmp( name, expected.leaf ) );
		CHECK( Com_GameManagerNativeModuleCandidate( profile, expected.module,
			"x86", ".dll", 0, name, sizeof( name ), &installationRoot ) );
		CHECK( installationRoot == expected.root && !std::strcmp( name, expected.leaf ) );
		// Raven shipped Win32 x86 only, and there is no second candidate.
		CHECK( !Com_GameManagerNativeModuleFilename( profile, expected.module,
			"x86_64", ".dll", 0, name, sizeof( name ) ) );
		CHECK( !Com_GameManagerNativeModuleFilename( profile, expected.module,
			"x86", ".so", 0, name, sizeof( name ) ) );
		CHECK( !Com_GameManagerNativeModuleFilename( profile, expected.module,
			"x86", ".dll", 1, name, sizeof( name ) ) );
	}
	// Single player has no menu module of its own.
	CHECK( !Com_GameManagerNativeModuleFilename( Com_GameProfileById( COM_GAME_JKA_SP ),
		"ui", "x86", ".dll", 0, name, sizeof( name ) ) );
	// Only single player reads a module from the installation root.
	CHECK( !Com_GameManagerResolveRootModule( Com_GameProfileById( COM_GAME_JKA_MP ), ".",
		"jampgamex86.dll", "x86", ".dll", name, sizeof( name ) ) );

	// Multiplayer proves pure admission with its client and menu libraries.
	CHECK( sp->pureProof == COM_PURE_PROOF_UNAVAILABLE );
	CHECK( mp->pureProof == COM_PURE_PROOF_JKA_NATIVE );
	CHECK( Com_PureProofIsNativeModulePair( mp ) && !Com_PureProofIsNativeModulePair( sp ) );
	CHECK( Com_GameManagerPureModuleFilename( mp, 0, "x86", ".dll", name, sizeof( name ) ) );
	CHECK( !std::strcmp( name, "cgamex86.dll" ) );
	CHECK( Com_GameManagerPureModuleFilename( mp, 1, "x86", ".dll", name, sizeof( name ) ) );
	CHECK( !std::strcmp( name, "uix86.dll" ) );
	CHECK( !Com_GameManagerPureModuleFilename( mp, 2, "x86", ".dll", name, sizeof( name ) ) );
	CHECK( !Com_GameManagerPureModuleFilename( sp, 0, "x86", ".dll", name, sizeof( name ) ) );

	// Only Jedi Academy's own maps, and no other profile reads them.
	CHECK( Com_GameManagerBspFormat( sp, 1 ) == COM_GAME_BSP_RBSP );
	CHECK( Com_GameManagerBspFormat( mp, 1 ) == COM_GAME_BSP_RBSP );
	for ( int version : { 0, 2, 43, 46, 47 } ) {
		CHECK( Com_GameManagerBspFormat( sp, version ) == COM_GAME_BSP_UNSUPPORTED );
		CHECK( Com_GameManagerBspFormat( mp, version ) == COM_GAME_BSP_UNSUPPORTED );
	}
	for ( auto other : { COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP,
		COM_GAME_WOLFET, COM_GAME_STVEF_SP, COM_GAME_QUAKE3_IHV } ) {
		CHECK( Com_GameManagerBspFormat( Com_GameProfileById( other ), 1 )
			== COM_GAME_BSP_UNSUPPORTED );
	}
}

/* An installation directory supplied on the command line is read, never
 * written, and never has a module loaded from it. */
static void TestInstallation( const char *installation )
{
	CHECK( Com_ProbeGameProfileMarkers( installation )
		== ( COM_GAME_MARKER_JKA_SP | COM_GAME_MARKER_JKA_MP ) );
	comGameMarkerInputs_t inputs{ "auto",
		Com_ProbeGameProfileMarkers( installation ), 0, 0 };
	CHECK( Com_DetectGameProfileMarkers( &inputs ).status == COM_GAME_DETECTION_MODE_REQUIRED );

	char resolved[1024];
	const auto *sp = Com_GameProfileById( COM_GAME_JKA_SP );
	CHECK( Com_GameManagerResolveRootModule( sp, installation, "jagamex86.dll",
		"x86", ".dll", resolved, sizeof( resolved ) ) );
	CHECK( std::strstr( resolved, "jagamex86.dll" ) );
	// Multiplayer's modules are inside the paks, so none resolves in the root.
	for ( const char *leaf : { "jampgamex86.dll", "cgamex86.dll", "uix86.dll" } ) {
		CHECK( !Com_GameManagerResolveRootModule( Com_GameProfileById( COM_GAME_JKA_MP ),
			installation, leaf, "x86", ".dll", resolved, sizeof( resolved ) ) );
	}
	std::puts( "Supplied Jedi Academy installation recognized from its pak identity" );
}

int main( int argc, char **argv )
{
	TestInstallationIdentity();
	TestProfiles();
	TestProtocolsAndModules();
	if ( argc > 1 ) TestInstallation( argv[1] );
	std::puts( "Jedi Academy installation identity and startup contracts passed" );
	return 0;
}
