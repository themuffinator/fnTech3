/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#include "../code/qcommon/game_manager.h"
#include "../code/qcommon/protocol_contract_profile.hpp"
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
	// Only WolfMP.exe and et.exe sweep a box entity as its own contents.
	for ( int id = COM_GAME_AUTO + 1; id < COM_GAME_COUNT; ++id ) {
		const auto *profile = Com_GameProfileById( static_cast<comGameId_t>( id ) );
		const bool entityContents = id == COM_GAME_RTCW_MP || id == COM_GAME_WOLFET;
		assert( profile && profile->entityBoxContentsPolicy == ( entityContents
			? COM_ENTITY_BOX_CONTENTS_ENTITY : COM_ENTITY_BOX_CONTENTS_BODY ) );
	}
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
				if ( profile->id == COM_GAME_Q3TEST ) {
					// Every Q3Test installer put its Win32 x86 libraries in the
					// installation root, uix86.dll included from 1.06.
					const bool available = !std::strcmp( host.arch, "x86" ) && !std::strcmp( host.ext, ".dll" );
					std::memset( name, 0x5a, sizeof( name ) );
					assert( !!Com_GameManagerNativeModuleFilename( profile, module, host.arch, host.ext, 0, name, sizeof( name ) ) == available );
					if ( available ) assert( name == std::string( module ) + "x86.dll" );
					else for ( char byte : name ) assert( byte == 0x5a );
					assert( !Com_GameManagerNativeModuleFilename( profile, module, host.arch, host.ext, 1, name, sizeof( name ) ) );
					continue;
				}
				if ( profile->id == COM_GAME_QUAKE3_IHV ) {
					const bool available = !std::strcmp( host.arch, "x86" ) && !std::strcmp( host.ext, ".dll" ) && std::strcmp( module, "ui" );
					std::memset( name, 0x5a, sizeof( name ) );
					assert( !!Com_GameManagerNativeModuleFilename( profile, module, host.arch, host.ext, 0, name, sizeof( name ) ) == available );
					if ( available ) assert( name == std::string( module ) + "x86.dll" );
					else for ( char byte : name ) assert( byte == 0x5a );
					assert( !Com_GameManagerNativeModuleFilename( profile, module, host.arch, host.ext, 1, name, sizeof( name ) ) );
					continue;
				}
				if ( profile->titleId == COM_GAME_TITLE_ALICE || profile->titleId == COM_GAME_TITLE_FAKK2
					|| profile->titleId == COM_GAME_TITLE_MOHAA ) {
					// Ritual shipped Win32 x86 libraries only: fgamex86.dll (Alice)
					// or gamex86.dll (F.A.K.K.2) for the game and cgamex86.dll for
					// the client game, both beside the paks. The menus are the
					// engine's, so there is no UI module. Medal of Honor's are named
					// as F.A.K.K.2's, in main/.
					const bool fakk2 = profile->titleId == COM_GAME_TITLE_FAKK2
						|| profile->titleId == COM_GAME_TITLE_MOHAA;
					const char *leaf = !std::strcmp( module, "ui" ) ? nullptr
						: !std::strcmp( module, "cgame" ) ? "cgamex86.dll"
						: fakk2 ? "gamex86.dll" : "fgamex86.dll";
					const bool retail = leaf && !std::strcmp( host.arch, "x86" )
						&& !std::strcmp( host.ext, ".dll" );
					std::memset( name, 0x5a, sizeof( name ) );
					assert( !!Com_GameManagerNativeModuleFilename( profile, module, host.arch,
						host.ext, 0, name, sizeof( name ) ) == retail );
					if ( retail ) assert( !std::strcmp( name, leaf ) );
					else for ( char byte : name ) assert( byte == 0x5a );
					assert( !Com_GameManagerNativeModuleFilename( profile, module, host.arch,
						host.ext, 1, name, sizeof( name ) ) );
					if ( leaf ) {
						const unsigned length = static_cast<unsigned>( std::strlen( leaf ) );
						assert( !Com_GameManagerNativeModuleFilename( profile, module, "x86",
							".dll", 0, name, length ) );
						assert( Com_GameManagerNativeModuleFilename( profile, module, "x86",
							".dll", 0, name, length + 1 ) && std::strlen( name ) == length );
					}
					continue;
				}
				if ( profile->id == COM_GAME_STVEF_SP ) {
					// Raven shipped one Win32 x86 game/cgame library and one UI library.
					const bool retail = !std::strcmp( host.arch, "x86" ) && !std::strcmp( host.ext, ".dll" );
					std::memset( name, 0x5a, sizeof( name ) );
					assert( !!Com_GameManagerNativeModuleFilename( profile, module, host.arch,
						host.ext, 0, name, sizeof( name ) ) == retail );
					if ( retail ) assert( !std::strcmp( name, std::strcmp( module, "ui" )
						? "efgamex86.dll" : "efuix86.dll" ) );
					else for ( char byte : name ) assert( byte == 0x5a );
					assert( !Com_GameManagerNativeModuleFilename( profile, module, host.arch,
						host.ext, 1, name, sizeof( name ) ) );
					const unsigned leaf = std::strcmp( module, "ui" ) ? 13u : 11u;
					assert( !Com_GameManagerNativeModuleFilename( profile, module, "x86",
						".dll", 0, name, leaf ) );
					assert( Com_GameManagerNativeModuleFilename( profile, module, "x86",
						".dll", 0, name, leaf + 1 ) && std::strlen( name ) == leaf );
					continue;
				}
				if ( profile->id == COM_GAME_JK2_SP ) {
					// One jk2gamex86.dll in the installation root serves the
					// game and the client game, and there is no menu module.
					// Multiplayer is absent here: Raven shipped it as QVMs.
					const char *leaf = std::strcmp( module, "ui" ) ? "jk2gamex86.dll" : nullptr;
					const bool retail = leaf && !std::strcmp( host.arch, "x86" )
						&& !std::strcmp( host.ext, ".dll" );
					std::memset( name, 0x5a, sizeof( name ) );
					assert( !!Com_GameManagerNativeModuleFilename( profile, module, host.arch,
						host.ext, 0, name, sizeof( name ) ) == retail );
					if ( retail ) assert( !std::strcmp( name, leaf ) );
					else for ( char byte : name ) assert( byte == 0x5a );
					assert( !Com_GameManagerNativeModuleFilename( profile, module, host.arch,
						host.ext, 1, name, sizeof( name ) ) );
					if ( leaf ) {
						const unsigned length = static_cast<unsigned>( std::strlen( leaf ) );
						assert( !Com_GameManagerNativeModuleFilename( profile, module, "x86",
							".dll", 0, name, length ) );
						assert( Com_GameManagerNativeModuleFilename( profile, module, "x86",
							".dll", 0, name, length + 1 ) && std::strlen( name ) == length );
					}
					continue;
				}
				if ( profile->titleId == COM_GAME_TITLE_JKA ) {
					// Raven shipped Win32 x86 modules only: one jagamex86.dll
					// for single player's game and client game with no menu
					// module of its own, and three for multiplayer.
					const bool single = profile->id == COM_GAME_JKA_SP;
					const char *leaf = !std::strcmp( module, "ui" )
						? ( single ? nullptr : "uix86.dll" )
						: !std::strcmp( module, "cgame" )
						? ( single ? "jagamex86.dll" : "cgamex86.dll" )
						: ( single ? "jagamex86.dll" : "jampgamex86.dll" );
					const bool retail = leaf && !std::strcmp( host.arch, "x86" )
						&& !std::strcmp( host.ext, ".dll" );
					std::memset( name, 0x5a, sizeof( name ) );
					assert( !!Com_GameManagerNativeModuleFilename( profile, module, host.arch,
						host.ext, 0, name, sizeof( name ) ) == retail );
					if ( retail ) assert( !std::strcmp( name, leaf ) );
					else for ( char byte : name ) assert( byte == 0x5a );
					assert( !Com_GameManagerNativeModuleFilename( profile, module, host.arch,
						host.ext, 1, name, sizeof( name ) ) );
					if ( leaf ) {
						const unsigned length = static_cast<unsigned>( std::strlen( leaf ) );
						assert( !Com_GameManagerNativeModuleFilename( profile, module, "x86",
							".dll", 0, name, length ) );
						assert( Com_GameManagerNativeModuleFilename( profile, module, "x86",
							".dll", 0, name, length + 1 ) && std::strlen( name ) == length );
					}
					continue;
				}
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
			if ( present && profile->id == COM_GAME_STVEF_SP ) {
				assert( root == 1 && !std::strcmp( output, "efgamex86.dll" ) );
			} else if ( present && ( profile->id == COM_GAME_QUAKE3_IHV || profile->id == COM_GAME_Q3TEST ) ) {
				assert( root == 1 && !std::strcmp( output, "cgamex86.dll" ) );
			} else if ( present && profile->titleId == COM_GAME_TITLE_JKA ) {
				// Jedi Academy's single player takes its client game from the
				// one library beside the executable; multiplayer's modules
				// ship inside the paks, so they come through the filesystem.
				const bool single = profile->id == COM_GAME_JKA_SP;
				assert( root == ( single ? 1 : 0 ) );
				assert( !std::strcmp( output, single ? "jagamex86.dll" : "cgamex86.dll" ) );
			} else if ( present && profile->id == COM_GAME_JK2_SP ) {
				// Jedi Outcast's single player does the same, from the one
				// library beside jk2sp.exe. Its multiplayer ships QVMs, so it
				// takes the ordinary filesystem path.
				assert( root == 1 && !std::strcmp( output, "jk2gamex86.dll" ) );
			} else if ( present ) {
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
	assert( Com_GameManagerTitleCount() == 10 );
	assert( Com_GameManagerProfileCount() == 16 );
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
	assert( !Com_GameManagerFindProfile( "jka" ) );
	assert( !Com_GameManagerFindProfile( "rtcw-sp-extra" ) );
	assert( !Com_GameManagerFindProfile( " q3" ) );
	assert( !Com_GameManagerCanActivate( nullptr ) );
	assert( !Com_GameManagerConfigFile( nullptr, 0 ) );
	unsigned seen = 0;
	for ( unsigned t = 0; t < Com_GameManagerTitleCount(); ++t ) {
		const auto *title = Com_GameManagerTitleAt( t );
		assert( title == Com_GameManagerTitleById( title->id ) );
		const auto count = Com_GameManagerTitleProfileCount( title->id );
		// Quake III holds the retail line, the IHV test and the Q3Test releases.
		assert( count == ( title->id == COM_GAME_TITLE_QUAKE3 ? 3u : title->id == COM_GAME_TITLE_RTCW
			|| title->id == COM_GAME_TITLE_STVEF || title->id == COM_GAME_TITLE_JKA
			|| title->id == COM_GAME_TITLE_JK2 ? 2u : 1u ) );
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
		| COM_GAME_MARKER_RTCW_SP | COM_GAME_MARKER_RTCW_MP | COM_GAME_MARKER_WOLFET
		| COM_GAME_MARKER_STVEF_SP | COM_GAME_MARKER_STVEF_MP | COM_GAME_MARKER_QUAKE3_IHV
		| COM_GAME_MARKER_JKA_SP | COM_GAME_MARKER_JKA_MP
		| COM_GAME_MARKER_JK2_SP | COM_GAME_MARKER_JK2_MP
		| COM_GAME_MARKER_ALICE | COM_GAME_MARKER_FAKK2 | COM_GAME_MARKER_MOHAA
		| COM_GAME_MARKER_Q3TEST ) );
	// Jedi Academy's two modes share one installation, as RTCW's and Elite
	// Force's do, so each mode matches an installation carrying both.
	const auto *jkaSp = Com_GameProfileById( COM_GAME_JKA_SP );
	const auto *jkaMp = Com_GameProfileById( COM_GAME_JKA_MP );
	const unsigned jkaMarkers = COM_GAME_MARKER_JKA_SP | COM_GAME_MARKER_JKA_MP;
	assert( Com_GameManagerInstallationMatches( jkaSp, jkaMarkers ) );
	assert( Com_GameManagerInstallationMatches( jkaMp, jkaMarkers ) );
	assert( !Com_GameManagerInstallationMatches( jkaSp, jkaMarkers | COM_GAME_MARKER_QUAKE3 ) );
	assert( !Com_GameManagerInstallationMatches( jkaSp, COM_GAME_MARKER_JKA_MP ) );
	assert( !std::strcmp( Com_GameManagerConfigFile( jkaSp, 0 ), "jaconfig.cfg" ) );
	assert( !std::strcmp( Com_GameManagerConfigFile( jkaSp, 1 ), "jaconfig_server.cfg" ) );
	assert( !std::strcmp( Com_GameManagerConfigFile( jkaMp, 0 ), "jampconfig.cfg" ) );
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
		"qzconfig.cfg", "repconfig.cfg", "wolfconfig.cfg", "WOLFCONFIG_MP.CFG", "wolfconfig_server.cfg",
		"efconfig.cfg", "efconfig_server.cfg", "HMCONFIG.CFG", "hmconfig_server.cfg" } ) {
		assert( Com_GameManagerIsConfigFile( filename ) );
	}
	/* Alice's retail configuration names are generic words rather than a title
	 * stem. With no profile selected, and under every profile but Alice, they
	 * are ordinary filenames that a pak may carry. */
	assert( !Com_GameManagerIsConfigFile( "config.cfg" ) );
	assert( !Com_GameManagerIsConfigFile( "config_server.cfg" ) );
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
	assert( Detect( "stvef", 0, 0, 0 ).status == COM_GAME_DETECTION_MODE_REQUIRED );
	assert( Detect( "auto", COM_GAME_MARKER_STVEF_SP | COM_GAME_MARKER_STVEF_MP, 0, 0 ).status
		== COM_GAME_DETECTION_MODE_REQUIRED );
	assert( Detect( "auto", COM_GAME_MARKER_STVEF_SP | COM_GAME_MARKER_QUAKE3, 0, 0 ).status
		== COM_GAME_DETECTION_AMBIGUOUS );
	{
		const auto *efsp = Com_GameProfileById( COM_GAME_STVEF_SP );
		const auto *efmp = Com_GameProfileById( COM_GAME_STVEF_MP );
		assert( efsp == Com_GameManagerFindProfile( "STVEF-SP" ) && efsp == Com_GameManagerFindProfile( "stvoy" ) );
		assert( efmp == Com_GameManagerFindProfile( "holomatch" ) && efmp == Com_GameManagerFindProfile( "stvef-hm" ) );
		assert( !std::strcmp( efsp->baseGame, "baseEF" ) && !std::strcmp( efmp->baseGame, "baseEF" ) );
		assert( !efsp->steamAppId && !efsp->steamInstallName && !efmp->steamAppId && !efmp->steamInstallName );
		assert( !std::strcmp( Com_GameManagerConfigFile( efsp, 0 ), "efconfig.cfg" ) );
		assert( !std::strcmp( Com_GameManagerConfigFile( efmp, 0 ), "hmconfig.cfg" ) );
		assert( !std::strcmp( Com_GameManagerConfigFile( efmp, 1 ), "hmconfig_server.cfg" ) );
		assert( Com_GameManagerInstallationMatches( efsp, COM_GAME_MARKER_STVEF_SP | COM_GAME_MARKER_STVEF_MP ) );
		assert( !Com_GameManagerInstallationMatches( efmp, COM_GAME_MARKER_STVEF_MP | COM_GAME_MARKER_RTCW_MP ) );
		for ( int protocol = -1; protocol < 100; ++protocol ) {
			assert( !!Com_GameProfileAllowsProtocol( efsp, protocol ) == ( protocol == 40 ) );
			assert( !!Com_GameProfileAllowsProtocol( efmp, protocol ) == ( protocol == 24 || protocol == 26 ) );
		}
		assert( Com_GameProfileAllowsModuleApi( efsp, COM_MODULE_API_STVEF_SP_NATIVE ) );
		assert( !Com_GameProfileAllowsModuleApi( efsp, COM_MODULE_API_QVM ) );
		assert( !Com_GameProfileAllowsModuleApi( efsp, COM_MODULE_API_LEGACY_NATIVE ) );
		assert( Com_GameProfileAllowsModuleApi( efmp, COM_MODULE_API_QVM ) );
		assert( !Com_GameProfileAllowsModuleApi( efmp, COM_MODULE_API_STVEF_SP_NATIVE ) );
		assert( efsp->pureProof == COM_PURE_PROOF_UNAVAILABLE && efmp->pureProof == COM_PURE_PROOF_QVM );
		assert( efsp->maxClients == 1 && efmp->maxClients == 64 );
		for ( const auto *other : { Com_GameProfileById( COM_GAME_QUAKE3 ), Com_GameProfileById( COM_GAME_QUAKELIVE ),
			Com_GameProfileById( COM_GAME_RTCW_SP ), Com_GameProfileById( COM_GAME_RTCW_MP ),
			Com_GameProfileById( COM_GAME_WOLFET ) } ) {
			for ( int protocol : { 24, 26, 40 } ) assert( !Com_GameProfileAllowsProtocol( other, protocol ) );
			assert( !Com_GameProfileAllowsModuleApi( other, COM_MODULE_API_STVEF_SP_NATIVE ) );
		}
	}
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
	for ( int version : { INT_MIN, -1, 0, 1, 12, 17, 18, 19, 20, 21, 42, 43, 44, 45, 46, 47, 48, INT_MAX } ) {
		assert( Com_GameBspFormat( version ) == COM_GAME_BSP_UNSUPPORTED );
		assert( Com_GameManagerBspFormat( nullptr, version ) == COM_GAME_BSP_UNSUPPORTED );
		for ( unsigned p = 0; p < Com_GameManagerProfileCount(); ++p ) {
			const auto *profile = Com_GameManagerProfileAt( p );
			const bool rtcw = profile->titleId == COM_GAME_TITLE_RTCW || profile->id == COM_GAME_WOLFET;
			const bool jka = profile->titleId == COM_GAME_TITLE_JKA;
			// Jedi Outcast's two engines disagree with each other: its single
			// player admits 512 submodels and its multiplayer 256, each read
			// from that binary's own bounds check.
			const bool rbsp = jka || profile->titleId == COM_GAME_TITLE_JK2;
			// Alice is a Ritual F.A.K.K.2 engine: "FAKK" 42 and nothing else,
			// with the same 512 submodels its own bounds check admits.
			// F.A.K.K.2 itself admits its own 12 and nothing else, and 384
			// submodels, the bound fakk2.exe 1.02 checks (0x0042575f).
			const bool fakk = profile->titleId == COM_GAME_TITLE_ALICE;
			const bool fakk2 = profile->titleId == COM_GAME_TITLE_FAKK2;
			// Medal of Honor 1.00 admits its own "2015" 19 and nothing else, and
			// 1,024 submodels, the maps' own ceiling.
			const bool mohaa = profile->titleId == COM_GAME_TITLE_MOHAA;
			const bool wide = rtcw || jka || fakk || profile->id == COM_GAME_JK2_SP;
			assert( profile->collisionModelHandleCount == ( mohaa ? 1024 : fakk2 ? 384 : wide ? 512 : 256 ) );
			const bool stvef = profile->titleId == COM_GAME_TITLE_STVEF;
			const auto expected = profile->id == COM_GAME_QUAKE3_IHV ? ( version == 43 ? COM_GAME_BSP_QUAKE3_IHV : COM_GAME_BSP_UNSUPPORTED )
				// Q3Test's releases read IBSP 44 (1.02 to 1.05) or 45 (1.06 to 1.08).
				: profile->id == COM_GAME_Q3TEST ? ( version == 44 || version == 45 ? COM_GAME_BSP_Q3TEST : COM_GAME_BSP_UNSUPPORTED )
				: mohaa ? ( version == 19 ? COM_GAME_BSP_MOHAA : COM_GAME_BSP_UNSUPPORTED )
				: fakk ? ( version == 42 ? COM_GAME_BSP_FAKK : COM_GAME_BSP_UNSUPPORTED )
				: fakk2 ? ( version == 12 ? COM_GAME_BSP_FAKK : COM_GAME_BSP_UNSUPPORTED )
				: rbsp ? ( version == 1 ? COM_GAME_BSP_RBSP : COM_GAME_BSP_UNSUPPORTED )
				: stvef ? ( version == 46 ? COM_GAME_BSP_QUAKE3 : COM_GAME_BSP_UNSUPPORTED )
				: version == 47
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
	// Protocol 1 is spoken by Alice's engine and by F.A.K.K.2's, and no
	// other profile admits it. Each of the two admits only its own family.
	assert( Com_ProtocolFamilyForVersion( 1 )
		== ( COM_PROTOCOL_FAMILY_ALICE | COM_PROTOCOL_FAMILY_FAKK2 ) );
	{
		const auto *alice = Com_GameProfileById( COM_GAME_ALICE );
		const auto *fakk2 = Com_GameProfileById( COM_GAME_FAKK2 );
		assert( alice && Com_GameProfileAllowsProtocol( alice, 1 ) );
		assert( fakk2 && Com_GameProfileAllowsProtocol( fakk2, 1 ) );
		assert( alice->allowedProtocolFamilies == COM_PROTOCOL_FAMILY_ALICE );
		assert( fakk2->allowedProtocolFamilies == COM_PROTOCOL_FAMILY_FAKK2 );
		for ( unsigned p = 0; p < Com_GameManagerProfileCount(); ++p ) {
			const auto *other = Com_GameManagerProfileAt( p );
			if ( other != alice && other != fakk2 ) assert( !Com_GameProfileAllowsProtocol( other, 1 ) );
			else assert( !Com_GameProfileAllowsProtocol( other, 68 ) );
		}
		// The two Ritual titles share an engine lineage and nothing that
		// would let one profile's modules load under the other.
		assert( !Com_GameProfileAllowsModuleApi( alice, COM_MODULE_API_FAKK2_NATIVE ) );
		assert( !Com_GameProfileAllowsModuleApi( fakk2, COM_MODULE_API_ALICE_NATIVE ) );
		assert( Com_GameProfileAllowsModuleApi( fakk2, COM_MODULE_API_FAKK2_NATIVE ) );
		assert( !std::strcmp( fakk2->baseGame, "fakk" ) && !std::strcmp( fakk2->basePakName, "pak0.pk3" ) );
		assert( fakk2 == Com_GameManagerFindProfile( "FAKK2" ) );
		assert( fakk2 == Com_GameManagerFindProfile( "hmfakk2" ) );
		assert( fakk2->maxConfigstrings == 1200 && fakk2->playerConfigstringBase == 768 );
		assert( fakk2->maxClients == 128 && fakk2->bots.reliableCommands == 64 );
		assert( alice->bots.reliableCommands == 128 );
		assert( !std::strcmp( Com_GameManagerConfigFile( fakk2, 0 ), "config.cfg" ) );
		assert( Com_GameManagerInstallationMatches( fakk2, COM_GAME_MARKER_FAKK2 ) );
		assert( !Com_GameManagerInstallationMatches( fakk2, COM_GAME_MARKER_FAKK2 | COM_GAME_MARKER_ALICE ) );
		assert( !Com_GameManagerInstallationMatches( alice, COM_GAME_MARKER_FAKK2 ) );
	}
	// Medal of Honor: Allied Assault 1.00 speaks protocol 6, which no other
	// profile admits, and loads its own function-table modules from main/.
	assert( Com_ProtocolFamilyForVersion( 6 ) == COM_PROTOCOL_FAMILY_MOHAA );
	{
		const auto *mohaa = Com_GameProfileById( COM_GAME_MOHAA );
		assert( mohaa && Com_GameProfileAllowsProtocol( mohaa, 6 ) );
		assert( mohaa->allowedProtocolFamilies == COM_PROTOCOL_FAMILY_MOHAA );
		assert( !Com_GameProfileAllowsProtocol( mohaa, 8 ) && !Com_GameProfileAllowsProtocol( mohaa, 1 ) );
		for ( unsigned p = 0; p < Com_GameManagerProfileCount(); ++p ) {
			const auto *other = Com_GameManagerProfileAt( p );
			if ( other != mohaa ) {
				assert( !Com_GameProfileAllowsProtocol( other, 6 ) );
				assert( !Com_GameProfileAllowsModuleApi( other, COM_MODULE_API_MOHAA_NATIVE ) );
			}
		}
		assert( Com_GameProfileAllowsModuleApi( mohaa, COM_MODULE_API_MOHAA_NATIVE ) );
		assert( !Com_GameProfileAllowsModuleApi( mohaa, COM_MODULE_API_FAKK2_NATIVE ) );
		assert( !Com_GameProfileAllowsModuleApi( mohaa, COM_MODULE_API_ALICE_NATIVE ) );
		assert( !std::strcmp( mohaa->baseGame, "main" ) && !std::strcmp( mohaa->basePakName, "Pak0.pk3" ) );
		assert( mohaa == Com_GameManagerFindProfile( "MOHAA" ) );
		assert( mohaa == Com_GameManagerFindProfile( "medalofhonor" ) );
		assert( mohaa->maxClients == 64 && mohaa->maxConfigstrings == 2736 );
		assert( mohaa->playerConfigstringBase == 1684 && mohaa->bots.reliableCommands == 0 );
		// pure proves the general references alone; sv_pure is off by default
		assert( mohaa->pureProof == COM_PURE_PROOF_MOHAA_REFERENCES && !std::strcmp( mohaa->server.pure, "0" ) );
		for ( int id = COM_GAME_AUTO + 1; id < COM_GAME_COUNT; ++id ) {
			const auto *other = Com_GameProfileById( static_cast<comGameId_t>( id ) );
			assert( !other || other == mohaa || other->pureProof != COM_PURE_PROOF_MOHAA_REFERENCES );
		}
		assert( !std::strcmp( Com_GameManagerConfigFile( mohaa, 0 ), "configs/unnamedsoldier.cfg" ) );
		assert( Com_GameManagerIsConfigFile( "configs/unnamedsoldier.cfg" ) );
		assert( Com_GameManagerInstallationMatches( mohaa, COM_GAME_MARKER_MOHAA ) );
		assert( !Com_GameManagerInstallationMatches( mohaa, COM_GAME_MARKER_MOHAA | COM_GAME_MARKER_RTCW_SP ) );
		assert( !Com_GameManagerInstallationMatches( Com_GameProfileById( COM_GAME_RTCW_SP ), COM_GAME_MARKER_MOHAA ) );
	}
	for ( int protocol : { 66, 67 } ) {
		assert( Com_GameProfileAllowsProtocol( Com_GameProfileById( COM_GAME_QUAKE3 ), protocol ) );
		for ( auto other : { COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP,
			COM_GAME_WOLFET, COM_GAME_STVEF_SP, COM_GAME_STVEF_MP } )
			assert( !Com_GameProfileAllowsProtocol( Com_GameProfileById( other ), protocol ) );
	}
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
	// Admitting a number is not enough: a connection also needs the contract
	// Find() picks for it to serve the profile, as SV_DirectConnect and the
	// client's connectResponse require. The Jedi titles have no wire of their
	// own yet, and IHV and Q3Test answer through their own packet paths, so
	// none of those is served by any contract.
	for ( unsigned p = 0; p < Com_GameManagerProfileCount(); ++p ) {
		const auto *profile = Com_GameManagerProfileAt( p );
		const bool ownWire = !( profile->allowedProtocolFamilies
			& ( COM_PROTOCOL_FAMILY_JKA_SP | COM_PROTOCOL_FAMILY_JKA_MP
				| COM_PROTOCOL_FAMILY_JK2_SP | COM_PROTOCOL_FAMILY_JK2_MP
				| COM_PROTOCOL_FAMILY_QUAKE3_IHV | COM_PROTOCOL_FAMILY_Q3TEST ) );
		bool served = false;
		for ( int protocol = -1; protocol < 100; ++protocol ) {
			if ( !Com_GameProfileAllowsProtocol( profile, protocol ) ) continue;
			for ( bool compat : { false, true } ) {
				const auto *contract = fnql::protocol::Find( protocol, compat );
				served = served || ( contract && fnql::protocol::ServesProfile( *contract, profile ) );
			}
		}
		assert( served == ownWire );
		if ( ownWire ) {
			const auto *preferred = fnql::protocol::Find( profile->preferredProtocol );
			assert( preferred && fnql::protocol::ServesProfile( *preferred, profile ) );
		}
	}
	assert( !fnql::protocol::ServesProfile( fnql::protocol::LegacyQuake3, nullptr ) );

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
