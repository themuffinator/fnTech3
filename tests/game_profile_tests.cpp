/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#include "../code/qcommon/game_manager.h"
#include "../code/compat/rtcw_module_abi.h"

#include <cassert>
#include <cstring>
#include <climits>
#include <initializer_list>
#include <algorithm>
#include <string>
#include <vector>

static comGameDetectionResult_t Detect( const char *request, unsigned base,
	unsigned app, unsigned steam )
{
	comGameMarkerInputs_t inputs = { request, base, app, steam };
	return Com_DetectGameProfileMarkers( &inputs );
}

static void TestPureModuleNames()
{
	const auto *q3 = Com_GameProfileById( COM_GAME_QUAKE3 );
	const auto *ql = Com_GameProfileById( COM_GAME_QUAKELIVE );
	const auto *sp = Com_GameProfileById( COM_GAME_RTCW_SP );
	const auto *mp = Com_GameProfileById( COM_GAME_RTCW_MP );
	assert( q3->pureProof == COM_PURE_PROOF_QVM && ql->pureProof == COM_PURE_PROOF_QL_NATIVE );
	assert( sp->pureProof == COM_PURE_PROOF_UNAVAILABLE && mp->pureProof == COM_PURE_PROOF_RTCW_NATIVE );
	assert( q3->entityHullPolicy == COM_ENTITY_HULL_TRACE_SHAPE && ql->entityHullPolicy == COM_ENTITY_HULL_TRACE_SHAPE );
	assert( sp->entityHullPolicy == COM_ENTITY_HULL_ENTITY_FLAG && mp->entityHullPolicy == COM_ENTITY_HULL_ENTITY_FLAG );
	for ( const auto *profile : { q3, ql, sp, mp } ) {
		for ( const char *extension : { ".dll", ".so", ".dylib" } ) {
			for ( unsigned slot = 0; slot < 4; ++slot ) {
				char name[64] = "unchanged";
				const bool supported = profile != sp && slot < ( profile == ql ? 1u : 2u );
				assert( !!Com_GameManagerPureModuleFilename( profile, slot, "x86", extension, name, sizeof( name ) ) == supported );
				if ( !supported ) { assert( !std::strcmp( name, "unchanged" ) ); continue; }
				std::string expected;
				if ( profile == q3 ) expected = slot ? "vm/ui.qvm" : "vm/cgame.qvm";
				else if ( profile == ql ) expected = "cgamex86.dll";
				else expected = std::string( slot ? "ui" : "cgame" ) +
					( !std::strcmp( extension, ".dll" ) ? "_mp_x86" : ".mp.i386" ) + extension;
				assert( name == expected );
				std::memcpy( name, "unchanged", sizeof( "unchanged" ) );
				assert( !Com_GameManagerPureModuleFilename( profile, slot, "x86", extension, name, (unsigned)expected.size() ) );
				assert( !std::strcmp( name, "unchanged" ) );
			}
		}
	}
	char name[64] = "unchanged";
	auto copy = *mp;
	assert( !Com_GameManagerPureModuleFilename( &copy, 0, "x86", ".dll", name, sizeof( name ) ) );
	assert( !Com_GameManagerPureModuleFilename( nullptr, 0, "x86", ".dll", name, sizeof( name ) ) );
	assert( !Com_GameManagerPureModuleFilename( mp, 0, "../bad", ".dll", name, sizeof( name ) ) );
	assert( !Com_GameManagerPureModuleFilename( mp, 0, "x86", ".bad", name, sizeof( name ) ) );
	assert( !Com_GameManagerPureModuleFilename( mp, 0, "x86", ".dll", nullptr, sizeof( name ) ) );
	assert( !std::strcmp( name, "unchanged" ) );
}

static void TestNativeModuleNames()
{
	struct Host { const char *arch, *ext; };
	for ( const auto host : { Host{ "x86", ".dll" }, Host{ "x86_64", ".dll" },
		Host{ "i386", ".so" }, Host{ "x86_64", ".so" }, Host{ "arm64", ".dylib" } } ) {
		for ( unsigned p = 0; p < Com_GameManagerProfileCount(); ++p ) {
			const auto *profile = Com_GameManagerProfileAt( p );
			for ( const char *module : { "cgame", "qagame", "ui" } ) {
				char name[64];
				const bool rtcw = profile->titleId == COM_GAME_TITLE_RTCW || profile->id == COM_GAME_WOLFET;
				const bool windows = !std::strcmp( host.ext, ".dll" );
				const std::string arch = rtcw && windows && !std::strcmp( host.arch, "x86_64" )
					? "x64" : host.arch;
				std::string expected = module;
				if ( rtcw ) {
					expected += profile->id == COM_GAME_RTCW_SP
						? ( windows ? "_sp_" : ".sp." ) : ( windows ? "_mp_" : ".mp." );
				}
				expected += arch + host.ext;
				assert( Com_GameManagerNativeModuleFilename( profile, module, host.arch,
					host.ext, 0, name, sizeof( name ) ) && name == expected );
				const bool alias = profile->id == COM_GAME_RTCW_SP
					|| ( !rtcw && !std::strcmp( host.ext, ".so" )
						&& !std::strcmp( host.arch, "x86_64" ) && !std::strcmp( module, "qagame" ) );
				assert( ( Com_GameManagerNativeModuleFilename( profile, module, host.arch,
					host.ext, 1, name, sizeof( name ) ) != 0 ) == alias );
				if ( alias ) assert( name == std::string( module ) + ( rtcw ? arch : "x64" ) + host.ext );
				for ( unsigned invalid : { 2u, UINT_MAX } ) {
					std::memset( name, 0x5a, sizeof( name ) );
					assert( !Com_GameManagerNativeModuleFilename( profile, module, host.arch,
						host.ext, invalid, name, sizeof( name ) ) );
					for ( char byte : name ) assert( byte == 0x5a );
				}
				for ( unsigned size = 0; size <= expected.size(); ++size ) {
					std::memset( name, 0x5a, sizeof( name ) );
					assert( !Com_GameManagerNativeModuleFilename( profile, module, host.arch,
						host.ext, 0, name, size ) );
					for ( char byte : name ) assert( byte == 0x5a );
				}
				assert( Com_GameManagerNativeModuleFilename( profile, module, host.arch,
					host.ext, 0, name, (unsigned)expected.size() + 1 ) && name == expected );
				assert( name[expected.size() + 1] == 0x5a );
			}
		}
	}
	const auto *sp = Com_GameProfileById( COM_GAME_RTCW_SP );
	char output[64];
	std::memset( output, 0x5a, sizeof( output ) );
	for ( const char *bad : std::initializer_list<const char *>{ nullptr, "", "../ui", "main/ui", "main\\ui", "C:ui", "UI", "renderer" } ) {
		assert( !Com_GameManagerNativeModuleFilename( sp, bad, "x86", ".dll", 0, output, sizeof( output ) ) );
	}
	for ( const char *bad : std::initializer_list<const char *>{ nullptr, "", "../x86", "x86/", "x86\\", "x86:evil", "x86.so", "x86-evil", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" } ) {
		assert( !Com_GameManagerNativeModuleFilename( sp, "ui", bad, ".dll", 0, output, sizeof( output ) ) );
	}
	for ( const char *bad : std::initializer_list<const char *>{ nullptr, "", "dll", ".DLL", ".dll/evil", ".dll:evil", ".fixture" } ) {
		assert( !Com_GameManagerNativeModuleFilename( sp, "ui", "x86", bad, 0, output, sizeof( output ) ) );
	}
	assert( !Com_GameManagerNativeModuleFilename( nullptr, "ui", "x86", ".dll", 0, output, sizeof( output ) ) );
	comGameProfile_t forged = *sp;
	assert( !Com_GameManagerNativeModuleFilename( &forged, "ui", "x86", ".dll", 0, output, sizeof( output ) ) );
	assert( !Com_GameManagerNativeModuleFilename( sp, "ui", "x86", ".dll", 0, nullptr, sizeof( output ) ) );
	for ( char byte : output ) assert( byte == 0x5a );
	assert( Com_GameManagerNativeModuleFilename( sp, "ui", "i386", ".dll", 0, output, sizeof( output ) ) );
	assert( !std::strcmp( output, "ui_sp_x86.dll" ) );
	assert( Com_GameManagerNativeModuleFilename( sp, "ui", "x86", ".so", 0, output, sizeof( output ) ) );
	assert( !std::strcmp( output, "ui.sp.i386.so" ) );
	for ( unsigned p = 0; p < Com_GameManagerProfileCount(); ++p ) {
		const auto *profile = Com_GameManagerProfileAt( p );
		for ( unsigned index = 0; index < 6; ++index ) {
			std::memset( output, 0x5a, sizeof( output ) );
			int root = -7;
			const int result = Com_GameManagerNativeModuleCandidate( profile, "cgame",
				"x86", ".dll", index, output, sizeof( output ), &root );
			const bool present = index < ( profile == sp ? 4u : 1u );
			assert( ( result != 0 ) == present );
			if ( present ) {
				assert( root == ( index >= 2 ? 1 : 0 ) );
				const char *expected = profile == sp
					? ( index % 2 ? "cgamex86.dll" : "cgame_sp_x86.dll" )
					: (profile->id == COM_GAME_RTCW_MP || profile->id == COM_GAME_WOLFET) ? "cgame_mp_x86.dll" : "cgamex86.dll";
				assert( !std::strcmp( output, expected ) );
			} else {
				assert( root == -7 );
				for ( char byte : output ) assert( byte == 0x5a );
			}
		}
	}
	int root = -7;
	assert( !Com_GameManagerNativeModuleCandidate( sp, "cgame", "x86", ".dll",
		UINT_MAX, output, sizeof( output ), &root ) && root == -7 );
	assert( !Com_GameManagerNativeModuleCandidate( sp, "cgame", "x86", ".dll",
		0, output, 1, &root ) && root == -7 );
	assert( !Com_GameManagerNativeModuleCandidate( sp, "cgame", "x86", ".dll",
		0, output, sizeof( output ), nullptr ) );
}

static void TestNativeModuleCalls()
{
	using namespace fntech3::rtcw::module;
	static_assert( sizeof( comGameId_t ) == 4 && sizeof( comGameTitleId_t ) == 4 );
	static_assert( FNTECH3_RTCW_UI_API_VERSION == 4 && FNTECH3_RTCW_CGAME_API_VERSION == 3
		&& FNTECH3_RTCW_GAME_API_VERSION == 8 );
	assert( Lookup( COM_GAME_RTCW_SP, Module::UI, Direction::IMPORT, 15 ) == Call::UI_FS_SEEK );
	assert( Lookup( COM_GAME_RTCW_MP, Module::UI, Direction::IMPORT, 15 ) == Call::UI_FS_WRITE );
	assert( Lookup( COM_GAME_RTCW_SP, Module::GAME, Direction::IMPORT, 2 ) == Call::G_ENDGAME );
	assert( Lookup( COM_GAME_RTCW_MP, Module::GAME, Direction::IMPORT, 2 ) == Call::G_MILLISECONDS );
	assert( Ordinal( COM_GAME_RTCW_SP, Call::CG_MEMSET ) == 110 );
	assert( Ordinal( COM_GAME_RTCW_MP, Call::CG_MEMSET ) == 100 );
	for ( const auto &descriptor : descriptors ) {
		assert( Describe( descriptor.call ) == &descriptor && descriptor.name[0] );
		for ( auto profile : { COM_GAME_RTCW_SP, COM_GAME_RTCW_MP } ) {
			const int number = Ordinal( profile, descriptor.call );
			if ( number >= 0 ) assert( Lookup( profile, descriptor.module, descriptor.direction,
				number ) == descriptor.call );
		}
		for ( auto invalid : { COM_GAME_AUTO, COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_COUNT } ) {
			assert( Ordinal( invalid, descriptor.call ) == -1 );
			assert( Lookup( invalid, descriptor.module, descriptor.direction, descriptor.sp ) == Call::Invalid );
		}
	}
	for ( auto profile : { COM_GAME_RTCW_SP, COM_GAME_RTCW_MP } ) {
		for ( auto module : { Module::UI, Module::CGAME, Module::GAME } ) {
			for ( auto direction : { Direction::IMPORT, Direction::EXPORT } ) {
				for ( intptr_t number = -1; number <= maxOrdinal + 1; ++number ) {
					const auto call = Lookup( profile, module, direction, number );
					const auto *descriptor = Describe( call );
					if ( descriptor ) {
						assert( descriptor->module == module && descriptor->direction == direction );
						assert( Ordinal( profile, call ) == number );
					}
				}
				for ( intptr_t bad : { INTPTR_MIN, INTPTR_MAX } ) {
					assert( Lookup( profile, module, direction, bad ) == Call::Invalid );
				}
			}
		}
		assert( Lookup( profile, (Module)UINT_MAX, Direction::IMPORT, 0 ) == Call::Invalid );
		assert( Lookup( profile, Module::UI, (Direction)UINT_MAX, 0 ) == Call::Invalid );
		assert( Ordinal( profile, Call::Invalid ) == -1 && Ordinal( profile, Call::Count ) == -1 );
		assert( Ordinal( profile, (Call)UINT16_MAX ) == -1 );
	}
}

static void TestManager()
{
	assert( Com_GameManagerTitleCount() == 4 );
	assert( Com_GameManagerProfileCount() == 5 );
	assert( !Com_GameManagerTitleAt( UINT_MAX ) );
	assert( !Com_GameManagerProfileAt( UINT_MAX ) );
	assert( !Com_GameManagerProfileAt( Com_GameManagerProfileCount() ) );
	assert( !Com_GameManagerTitleById( static_cast<comGameTitleId_t>( 0 ) ) );
	assert( !Com_GameManagerTitleProfileAt( COM_GAME_TITLE_RTCW, 2 ) );
	assert( !Com_GameProfileById( COM_GAME_AUTO ) );
	assert( !Com_GameProfileById( COM_GAME_COUNT ) );
	assert( !Com_GameProfileById( static_cast<comGameId_t>( -1 ) ) );
	assert( !Com_GameManagerFindProfile( nullptr ) );
	assert( !Com_GameManagerFindProfile( "" ) );
	assert( !Com_GameManagerFindProfile( "rtcw" ) );
	assert( !Com_GameManagerFindProfile( "rtcw-sp-extra" ) );
	assert( !Com_GameManagerFindProfile( " q3" ) );
	assert( !Com_GameManagerCanActivate( nullptr ) );
	assert( !Com_GameManagerConfigFile( nullptr, 0 ) );
	unsigned seen = 0;
	for ( unsigned t = 0; t < Com_GameManagerTitleCount(); ++t ) {
		const auto *title = Com_GameManagerTitleAt( t );
		assert( title == Com_GameManagerTitleById( title->id ) );
		const auto count = Com_GameManagerTitleProfileCount( title->id );
		assert( count == ( title->id == COM_GAME_TITLE_RTCW ? 2u : 1u ) );
		for ( unsigned i = 0; i < count; ++i ) {
			const auto *profile = Com_GameManagerTitleProfileAt( title->id, i );
			assert( profile && profile->titleId == title->id );
			assert( profile == Com_GameProfileById( profile->id ) );
			assert( !( seen & profile->installMarker ) );
			seen |= profile->installMarker;
			assert( profile == Com_GameManagerFindProfile( profile->key ) );
			for ( const char *alias : profile->aliases ) {
				if ( alias ) assert( profile == Com_GameManagerFindProfile( alias ) );
			}
			assert( Com_GameProfileAllowsProtocol( profile, profile->preferredProtocol ) );
			assert( Com_GameProfileAllowsProtocol( profile, profile->preferredDemoProtocol ) );
			assert( Com_GameProfileAllowsModuleApi( profile, profile->preferredModuleApi ) );
			assert( profile->clientConfig && profile->serverConfig );
			comGameProfile_t forged = *profile;
			forged.runtimeBlocker = nullptr;
			assert( !Com_GameManagerCanActivate( &forged ) );
		}
	}
	assert( seen == ( COM_GAME_MARKER_QUAKE3 | COM_GAME_MARKER_QUAKELIVE
		| COM_GAME_MARKER_RTCW_SP | COM_GAME_MARKER_RTCW_MP | COM_GAME_MARKER_WOLFET ) );
	const auto *sp = Com_GameProfileById( COM_GAME_RTCW_SP );
	const auto *mp = Com_GameProfileById( COM_GAME_RTCW_MP );
	assert( sp == Com_GameManagerFindProfile( "RTCW-SP" ) );
	assert( mp == Com_GameManagerFindProfile( "WoLfMp" ) );
	assert( !std::strcmp( sp->steamAppId, mp->steamAppId ) );
	assert( !sp->usesSteam && !mp->usesSteam );
	assert( !sp->supportsWorkshop && !mp->supportsWorkshop );
	assert( !std::strcmp( Com_GameManagerConfigFile( sp, 0 ), "wolfconfig.cfg" ) );
	assert( !std::strcmp( Com_GameManagerConfigFile( mp, 0 ), "wolfconfig_mp.cfg" ) );
	assert( !std::strcmp( Com_GameManagerConfigFile( mp, 1 ), "wolfconfig_server.cfg" ) );
	assert( Com_GameManagerInstallationMatches( sp, COM_GAME_MARKER_RTCW_SP | COM_GAME_MARKER_RTCW_MP ) );
	assert( Com_GameManagerInstallationMatches( mp, COM_GAME_MARKER_RTCW_SP | COM_GAME_MARKER_RTCW_MP ) );
	assert( !Com_GameManagerInstallationMatches( mp, COM_GAME_MARKER_RTCW_SP ) );
	assert( !Com_GameManagerInstallationMatches( sp, COM_GAME_MARKER_RTCW_SP | COM_GAME_MARKER_QUAKE3 ) );
	assert( !Com_GameManagerInstallationMatches( sp, COM_GAME_MARKER_RTCW_SP | COM_GAME_MARKER_UNRECOGNIZED_PAK ) );
	assert( !Com_GameManagerInstallationMatches( nullptr, UINT_MAX ) );
	assert( !Com_GameManagerInstallationMatches( sp, UINT_MAX ) );
	for ( const char *filename : { "autoexec.cfg", "q3config.cfg", "q3config_server.cfg",
		"qzconfig.cfg", "repconfig.cfg", "wolfconfig.cfg", "WOLFCONFIG_MP.CFG", "wolfconfig_server.cfg" } ) {
		assert( Com_GameManagerIsConfigFile( filename ) );
	}
	assert( !Com_GameManagerIsConfigFile( "default.cfg" ) );
	assert( !Com_GameManagerIsConfigFile( "scripts/wolfconfig.cfg" ) );
	assert( !Com_GameManagerIsConfigFile( nullptr ) );
	assert( !Com_GameManagerAllowsPak( sp, "MP_pak0.pk3" ) );
	assert( !Com_GameManagerAllowsPak( mp, "SP_pak1.pk3dir" ) );
	assert( !Com_GameManagerAllowsPak( sp, nullptr ) );
	assert( !Com_GameManagerAllowsPak( sp, "" ) );
	assert( !Com_GameManagerAllowsPak( sp, "../pak0.pk3" ) );
	assert( !Com_GameManagerAllowsPak( mp, "main\\pak0.pk3" ) );
	assert( !Com_GameManagerAllowsPak( nullptr, "pak0.pk3" ) );
	assert( Com_GameManagerAllowsPak( sp, "mpcustom.pk3" ) );
	const std::vector<std::string> archives = { "pak0.pk3", "mp_pak0.pk3", "sp_pak1.pk3",
		"z_custom.pk3", "zz_pak1.pk3", "zzz_custom.pk3", "mp_bin.pk3", "sp_pak2.pk3" };
	for ( const auto *profile : { sp, mp, Com_GameProfileById( COM_GAME_QUAKE3 ),
		Com_GameProfileById( COM_GAME_QUAKELIVE ) } ) {
		std::vector<std::string> mounted;
		for ( const auto &name : archives ) {
			if ( Com_GameManagerAllowsPak( profile, name.c_str() ) ) mounted.push_back( name );
		}
		std::sort( mounted.begin(), mounted.end(), [profile]( const auto &a, const auto &b ) {
			return Com_GameManagerComparePaks( profile, a.c_str(), b.c_str() ) < 0;
		} );
		if ( profile == sp ) {
			assert( mounted == ( std::vector<std::string>{ "pak0.pk3", "z_custom.pk3",
				"sp_pak1.pk3", "zz_pak1.pk3", "sp_pak2.pk3", "zzz_custom.pk3" } ) );
		} else if ( profile == mp ) {
			assert( mounted == ( std::vector<std::string>{ "pak0.pk3", "z_custom.pk3",
				"mp_bin.pk3", "mp_pak0.pk3", "zz_pak1.pk3", "zzz_custom.pk3" } ) );
		} else {
			auto expected = archives;
			std::sort( expected.begin(), expected.end() );
			assert( mounted == expected );
		}
	}
	for ( int protocol : { 49, 50, 60, 61, 68, 71, 91, 48, 59, 62, -1 } ) {
		assert( Com_GameProfileAllowsProtocol( sp, protocol ) == ( protocol == 49 || protocol == 50 ) );
		assert( Com_GameProfileAllowsProtocol( mp, protocol ) == ( protocol == 60 || protocol == 61 ) );
		if ( protocol < 68 ) {
			assert( !Com_GameProfileAllowsProtocol( Com_GameProfileById( COM_GAME_QUAKE3 ), protocol ) );
			assert( !Com_GameProfileAllowsProtocol( Com_GameProfileById( COM_GAME_QUAKELIVE ), protocol ) );
		}
	}
	// Registration must never activate Q3/QL codecs against RTCW modules.
	assert( !Com_GameManagerCanActivate( sp ) && !Com_GameManagerCanActivate( mp ) );
	assert( !Com_ActivateGameProfile( COM_GAME_RTCW_SP ) );
	assert( !Com_ActivateGameProfile( COM_GAME_RTCW_MP ) );
	assert( !Com_GameIsRTCW() && !Com_GameIsRTCWSP() && !Com_GameIsRTCWMP() );
	assert( !Com_GameProfile() );
	for ( const char *key : { "rtcw-sp", "rtcwsp", "wolfsp", "rtcw-mp", "rtcwmp", "wolfmp" } ) {
		const auto result = Detect( key, COM_GAME_MARKER_QUAKE3, 0, 0 );
#if defined( FNTECH3_Q3_ONLY )
		assert( result.status == COM_GAME_DETECTION_UNSUPPORTED_BUILD );
#else
		assert( result.status == COM_GAME_DETECTION_INCOMPLETE_ENGINE );
#endif
		assert( result.evidence == COM_GAME_EVIDENCE_EXPLICIT && !result.profile );
	}
	assert( Detect( "rtcw", 0, 0, 0 ).status == COM_GAME_DETECTION_MODE_REQUIRED );
	for ( unsigned markers = 0; markers < 32; ++markers ) {
		const auto result = Detect( "auto", markers, 0, 0 );
		if ( markers & COM_GAME_MARKER_UNRECOGNIZED_PAK ) {
			assert( result.status == COM_GAME_DETECTION_UNRECOGNIZED_PAK );
		} else if ( markers == ( COM_GAME_MARKER_RTCW_SP | COM_GAME_MARKER_RTCW_MP ) ) {
			assert( result.status == COM_GAME_DETECTION_MODE_REQUIRED );
		} else if ( markers && ( markers & ( markers - 1 ) ) ) {
			assert( result.status == COM_GAME_DETECTION_AMBIGUOUS );
		}
	}
}

static void TestBspFormats()
{
	static_assert( sizeof( comGameBspFormat_t ) == 4 );
	assert( !Com_GameProfile() );
	for ( int version : { INT_MIN, -1, 0, 42, 43, 44, 45, 46, 47, 48, INT_MAX } ) {
		assert( Com_GameBspFormat( version ) == COM_GAME_BSP_UNSUPPORTED );
		assert( Com_GameManagerBspFormat( nullptr, version ) == COM_GAME_BSP_UNSUPPORTED );
		for ( unsigned p = 0; p < Com_GameManagerProfileCount(); ++p ) {
			const auto *profile = Com_GameManagerProfileAt( p );
			const bool rtcw = profile->titleId == COM_GAME_TITLE_RTCW || profile->id == COM_GAME_WOLFET;
			assert( profile->collisionModelHandleCount == ( rtcw ? 512 : 256 ) );
			const auto expected = version == 47
				? ( profile->id == COM_GAME_WOLFET ? COM_GAME_BSP_WOLFET : rtcw ? COM_GAME_BSP_RTCW : COM_GAME_BSP_QUAKELIVE )
				: !rtcw && version == 46 ? COM_GAME_BSP_QUAKE3
				: !rtcw && version == 43 ? COM_GAME_BSP_QUAKE3_IHV
				: COM_GAME_BSP_UNSUPPORTED;
			assert( Com_GameManagerBspFormat( profile, version ) == expected );
			comGameProfile_t forged = *profile;
			assert( Com_GameManagerBspFormat( &forged, version ) == COM_GAME_BSP_UNSUPPORTED );
			assert( !Com_GameProfile() );
		}
	}
}

static void TestModeLaunch()
{
	const auto *sp = Com_GameProfileById( COM_GAME_RTCW_SP );
	const auto *mp = Com_GameProfileById( COM_GAME_RTCW_MP );
	const auto *q3 = Com_GameProfileById( COM_GAME_QUAKE3 );
	const auto *active = Com_GameProfile();
	char output[COM_GAME_MODE_LAUNCH_BYTES + 1];
	for ( const auto *target : { sp, mp } ) {
		const auto *current = target == sp ? mp : sp;
		for ( const int fullscreen : { 0, 1 } ) {
			for ( const char *path : { "E:\\Games\\Wolf\\", "/games/Wolf; +set x y/",
				u8"E:\\Games\\Wölf 東京\\", "/games/'$()`%&|/" } ) {
				const std::string expected = std::string( "+set com_gameProfile \"" ) + target->key
					+ "\" +set fs_basepath \"" + path + "\" +set fs_homepath \"" + path
					+ "\" +set fs_game \"\" +set r_fullscreen \"" + char( '0' + fullscreen ) + "\"";
				for ( unsigned capacity = 0; capacity <= expected.size(); ++capacity ) {
					std::memset( output, 0x5a, sizeof( output ) );
					assert( Com_GameManagerFormatModeLaunch( current, target, path, path, fullscreen,
						output, capacity ) == COM_GAME_MODE_LAUNCH_TOO_LONG );
					for ( char byte : output ) assert( byte == 0x5a );
				}
				assert( Com_GameManagerFormatModeLaunch( current, target, path, path, fullscreen,
					output, unsigned( expected.size() ) + 1 ) == COM_GAME_MODE_LAUNCH_READY );
				assert( output == expected && output[expected.size() + 1] == 0x5a );
			}
		}
	}
	const auto rejects = [&]( const comGameProfile_t *current, const comGameProfile_t *target,
		const char *base, const char *home, int fullscreen, comGameModeLaunchStatus_t status ) {
		std::memset( output, 0x5a, sizeof( output ) );
		assert( Com_GameManagerFormatModeLaunch( current, target, base, home, fullscreen,
			output, sizeof( output ) ) == status );
		for ( char byte : output ) assert( byte == 0x5a );
	};
	for ( const auto *profile : { sp, mp, q3 } ) {
		rejects( profile, profile, "base", "home", 0, COM_GAME_MODE_LAUNCH_ALREADY_SELECTED );
	}
	rejects( q3, sp, "base", "home", 0, COM_GAME_MODE_LAUNCH_DIFFERENT_TITLE );
	rejects( sp, q3, "base", "home", 0, COM_GAME_MODE_LAUNCH_DIFFERENT_TITLE );
	comGameProfile_t impostor = *sp;
	for ( const auto *bad : std::initializer_list<const comGameProfile_t *>{ nullptr, &impostor } ) {
		rejects( bad, mp, "base", "home", 0, COM_GAME_MODE_LAUNCH_INVALID_PROFILE );
		rejects( mp, bad, "base", "home", 0, COM_GAME_MODE_LAUNCH_INVALID_PROFILE );
	}
	for ( const char *bad : std::initializer_list<const char *>{ nullptr, "", "a\"b" } ) {
		rejects( sp, mp, bad, "home", 0, COM_GAME_MODE_LAUNCH_INVALID_VALUE );
		rejects( sp, mp, "base", bad, 0, COM_GAME_MODE_LAUNCH_INVALID_VALUE );
	}
	for ( unsigned ch = 1; ch <= 127; ++ch ) {
		if ( ch >= 32 && ch != 127 ) continue;
		const char bad[] = { 'a', char( ch ), 'b', 0 };
		rejects( sp, mp, bad, "home", 0, COM_GAME_MODE_LAUNCH_INVALID_VALUE );
		rejects( sp, mp, "base", bad, 0, COM_GAME_MODE_LAUNCH_INVALID_VALUE );
	}
	for ( int fullscreen : { -1, 2, INT_MIN, INT_MAX } ) {
		rejects( sp, mp, "base", "home", fullscreen, COM_GAME_MODE_LAUNCH_INVALID_VALUE );
	}
	assert( Com_GameManagerFormatModeLaunch( sp, mp, "base", "home", 0, nullptr,
		sizeof( output ) ) == COM_GAME_MODE_LAUNCH_INVALID_VALUE );
	// Exercise the total startup budget independently of the caller's buffer.
	assert( Com_GameManagerFormatModeLaunch( sp, mp, "b", "h", 0,
		output, sizeof( output ) ) == COM_GAME_MODE_LAUNCH_READY );
	const size_t fixedBytes = std::strlen( output ) - 1;
	const std::string longest( COM_GAME_MODE_LAUNCH_BYTES - fixedBytes - 1, 'x' );
	assert( Com_GameManagerFormatModeLaunch( sp, mp, longest.c_str(), "h", 0,
		output, sizeof( output ) ) == COM_GAME_MODE_LAUNCH_READY );
	assert( std::strlen( output ) == COM_GAME_MODE_LAUNCH_BYTES - 1 );
	rejects( sp, mp, ( longest + 'x' ).c_str(), "h", 0, COM_GAME_MODE_LAUNCH_TOO_LONG );
	const std::string oversized( COM_GAME_MODE_LAUNCH_BYTES, 'x' );
	rejects( sp, mp, oversized.c_str(), "h", 0, COM_GAME_MODE_LAUNCH_TOO_LONG );
	rejects( sp, mp, "b", oversized.c_str(), 0, COM_GAME_MODE_LAUNCH_TOO_LONG );
	for ( unsigned status = COM_GAME_MODE_LAUNCH_READY; status <= COM_GAME_MODE_LAUNCH_TOO_LONG; ++status ) {
		assert( Com_GameManagerModeLaunchStatusMessage( comGameModeLaunchStatus_t( status ) )[0] );
	}
	assert( Com_GameProfile() == active );
	assert( !Com_GameManagerCanActivate( sp ) && !Com_GameManagerCanActivate( mp ) );
}

int main()
{
	comGameDetectionResult_t result;
	TestManager();
	TestNativeModuleNames();
	TestPureModuleNames();
	TestNativeModuleCalls();
	TestModeLaunch();
	TestBspFormats();

	result = Detect( "q3", COM_GAME_MARKER_QUAKELIVE,
		COM_GAME_MARKER_NONE, COM_GAME_MARKER_NONE );
	assert( result.status == COM_GAME_DETECTION_OK );
	assert( result.profile->id == COM_GAME_QUAKE3 );
	assert( result.evidence == COM_GAME_EVIDENCE_EXPLICIT );

	result = Detect( "QuakeLive", COM_GAME_MARKER_QUAKE3,
		COM_GAME_MARKER_NONE, COM_GAME_MARKER_NONE );
#if defined( FNTECH3_Q3_ONLY )
	assert( result.status == COM_GAME_DETECTION_UNSUPPORTED_BUILD );
	assert( result.profile == nullptr );
#else
	assert( result.status == COM_GAME_DETECTION_OK );
	assert( result.profile->id == COM_GAME_QUAKELIVE );
#endif

	result = Detect( "auto", COM_GAME_MARKER_QUAKE3,
		COM_GAME_MARKER_QUAKELIVE, COM_GAME_MARKER_QUAKELIVE );
	assert( result.profile->id == COM_GAME_QUAKE3 );
	assert( result.evidence == COM_GAME_EVIDENCE_BASE_PATH );

	result = Detect( "auto", COM_GAME_MARKER_NONE,
		COM_GAME_MARKER_QUAKELIVE, COM_GAME_MARKER_NONE );
#if defined( FNTECH3_Q3_ONLY )
	assert( result.status == COM_GAME_DETECTION_UNSUPPORTED_BUILD );
	assert( result.profile == nullptr );
#else
	assert( result.profile->id == COM_GAME_QUAKELIVE );
#endif
	assert( result.evidence == COM_GAME_EVIDENCE_APP_PATH );

	result = Detect( "auto", COM_GAME_MARKER_NONE,
		COM_GAME_MARKER_NONE, COM_GAME_MARKER_QUAKELIVE );
#if defined( FNTECH3_Q3_ONLY )
	assert( result.status == COM_GAME_DETECTION_UNSUPPORTED_BUILD );
	assert( result.profile == nullptr );
#else
	assert( result.profile->id == COM_GAME_QUAKELIVE );
#endif
	assert( result.evidence == COM_GAME_EVIDENCE_STEAM_PATH );

	result = Detect( "auto",
		COM_GAME_MARKER_QUAKE3 | COM_GAME_MARKER_QUAKELIVE,
		COM_GAME_MARKER_NONE, COM_GAME_MARKER_NONE );
	assert( result.status == COM_GAME_DETECTION_AMBIGUOUS );
	assert( result.profile == nullptr );

	// An occupied but unrecognized candidate must not silently select a
	// different game from a lower-priority installation.
	result = Detect( "auto", COM_GAME_MARKER_UNRECOGNIZED_PAK,
		COM_GAME_MARKER_QUAKELIVE, COM_GAME_MARKER_QUAKE3 );
	assert( result.status == COM_GAME_DETECTION_UNRECOGNIZED_PAK && !result.profile );
	assert( result.evidence == COM_GAME_EVIDENCE_BASE_PATH );
	result = Detect( "auto", 0, COM_GAME_MARKER_UNRECOGNIZED_PAK, COM_GAME_MARKER_QUAKE3 );
	assert( result.status == COM_GAME_DETECTION_UNRECOGNIZED_PAK && !result.profile );
	assert( result.evidence == COM_GAME_EVIDENCE_APP_PATH );
	result = Detect( "auto", 0, 0, COM_GAME_MARKER_UNRECOGNIZED_PAK | COM_GAME_MARKER_QUAKE3 );
	assert( result.status == COM_GAME_DETECTION_UNRECOGNIZED_PAK && !result.profile );
	assert( result.evidence == COM_GAME_EVIDENCE_STEAM_PATH );

	result = Detect( "auto", COM_GAME_MARKER_NONE,
		COM_GAME_MARKER_NONE, COM_GAME_MARKER_NONE );
	assert( result.status == COM_GAME_DETECTION_NOT_FOUND );

	result = Detect( "doom3", COM_GAME_MARKER_QUAKE3,
		COM_GAME_MARKER_NONE, COM_GAME_MARKER_NONE );
	assert( result.status == COM_GAME_DETECTION_UNKNOWN_REQUEST );

	assert( Com_GameProfile() == nullptr );
#if defined( FNTECH3_Q3_ONLY )
	/* Activation itself guards the ABI even if a caller skips detection. */
	assert( !Com_ActivateGameProfile( COM_GAME_QUAKELIVE ) );
	assert( Com_GameProfile() == nullptr );
	result = Detect( "auto", COM_GAME_MARKER_QUAKELIVE,
		COM_GAME_MARKER_QUAKE3, COM_GAME_MARKER_NONE );
	assert( result.status == COM_GAME_DETECTION_UNSUPPORTED_BUILD );
	assert( result.profile == nullptr );
	assert( result.evidence == COM_GAME_EVIDENCE_BASE_PATH );
#endif
	assert( Com_ActivateGameProfile( COM_GAME_QUAKE3 ) );
	assert( Com_GameIsQuake3() );
	assert( Com_GameBspFormat( 47 ) == COM_GAME_BSP_QUAKELIVE );
	assert( Com_GameBspFormat( 46 ) == COM_GAME_BSP_QUAKE3 );
	assert( Com_GameBspFormat( 43 ) == COM_GAME_BSP_QUAKE3_IHV );
	assert( Com_GameBspFormat( 48 ) == COM_GAME_BSP_UNSUPPORTED );
	assert( Com_GameClientLimit() == 64 && Com_GameConfigstringLimit() == 1024 );
	assert( Com_GamePlayerConfigstringBase() == 544 );
	const auto *rtcwSpLimits = Com_GameProfileById( COM_GAME_RTCW_SP );
	const auto *rtcwMpLimits = Com_GameProfileById( COM_GAME_RTCW_MP );
	assert( rtcwSpLimits->maxClients == 128 && rtcwMpLimits->maxClients == 64 );
	assert( rtcwSpLimits->maxConfigstrings == 2048 && rtcwMpLimits->maxConfigstrings == 2048 );
	assert( rtcwSpLimits->playerConfigstringBase == 544 && rtcwMpLimits->playerConfigstringBase == 576 );
	assert( !Com_GameIsQuakeLive() );
	assert( Com_ActivateGameProfile( COM_GAME_QUAKE3 ) );
	assert( !Com_ActivateGameProfile( COM_GAME_QUAKELIVE ) );

	assert( Com_ProtocolFamilyForVersion( 68 ) ==
		COM_PROTOCOL_FAMILY_LEGACY_QUAKE3 );
	assert( Com_ProtocolFamilyForVersion( 71 ) ==
		COM_PROTOCOL_FAMILY_IOQUAKE3 );
	assert( Com_ProtocolFamilyForVersion( 91 ) ==
		COM_PROTOCOL_FAMILY_QUAKELIVE );
	assert( Com_ProtocolFamilyForVersion( 69 ) == 0 );
	assert( Com_GameProfileAllowsProtocol(
		Com_GameProfileById( COM_GAME_QUAKE3 ), 68 ) );
	assert( Com_GameProfileAllowsProtocol(
		Com_GameProfileById( COM_GAME_QUAKE3 ), 71 ) );
	assert( !Com_GameProfileAllowsProtocol(
		Com_GameProfileById( COM_GAME_QUAKE3 ), 91 ) );
	assert( Com_GameProfileAllowsProtocol(
		Com_GameProfileById( COM_GAME_QUAKELIVE ), 68 ) );
	assert( Com_GameProfileAllowsProtocol(
		Com_GameProfileById( COM_GAME_QUAKELIVE ), 71 ) );
	assert( Com_GameProfileAllowsProtocol(
		Com_GameProfileById( COM_GAME_QUAKELIVE ), 91 ) );
	assert( !Com_GameProfileAllowsProtocol( nullptr, 68 ) );

	const comGameProfile_t *quake3 =
		Com_GameProfileById( COM_GAME_QUAKE3 );
	const comGameProfile_t *quakeLive =
		Com_GameProfileById( COM_GAME_QUAKELIVE );
	assert( Com_GameProfileAllowsModuleApi( quake3, COM_MODULE_API_QVM ) );
	assert( Com_GameProfileAllowsModuleApi(
		quake3, COM_MODULE_API_LEGACY_NATIVE ) );
	assert( !Com_GameProfileAllowsModuleApi(
		quake3, COM_MODULE_API_STRUCTURED_NATIVE ) );
	assert( Com_GameProfileAllowsModuleApi( quakeLive, COM_MODULE_API_QVM ) );
	assert( Com_GameProfileAllowsModuleApi(
		quakeLive, COM_MODULE_API_LEGACY_NATIVE ) );
	assert( Com_GameProfileAllowsModuleApi(
		quakeLive, COM_MODULE_API_STRUCTURED_NATIVE ) );
	assert( !Com_GameProfileAllowsModuleApi( quake3, 0 ) );
	assert( !Com_GameProfileAllowsModuleApi( quakeLive,
		COM_MODULE_API_QVM | COM_MODULE_API_STRUCTURED_NATIVE ) );
	assert( !Com_GameProfileAllowsModuleApi( quakeLive, 1u << 12 ) );
	assert( !Com_GameProfileAllowsModuleApi(
		nullptr, COM_MODULE_API_QVM ) );

	return 0;
}
