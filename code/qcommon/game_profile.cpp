/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#include "game_manager.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

namespace {

/* Steam AppIDs and conventional directories were observed in the local retail
 * manifests; see docs/fntech3/2026-09-07-steam-discovery.md. Discovery identity
 * is independent of each profile's optional service integration contract. */

const comGameTitle_t titles[] = {
	{ COM_GAME_TITLE_QUAKE3, "quake3", "Quake III Arena" },
	{ COM_GAME_TITLE_QUAKELIVE, "quakelive", "Quake Live" },
	{ COM_GAME_TITLE_RTCW, "rtcw", "Return to Castle Wolfenstein" },
	{ COM_GAME_TITLE_WOLFET, "wolfet", "Wolfenstein: Enemy Territory" }
};

/* One catalog owns title identity, aliases and startup contracts. RTCW facts
 * are pinned in docs/fntech3/2026-09-14-rtcw-contracts.md. Catalog registration
 * does not enable an unfinished engine boundary or claim runtime compatibility.
 */
const comGameProfile_t profiles[] = { {
	COM_GAME_QUAKE3,
	"quake3",
	"Quake III Arena",
	"baseq3",
	"pak0.pk3",
	68,
	68,
	COM_PROTOCOL_FAMILY_LEGACY_QUAKE3 | COM_PROTOCOL_FAMILY_IOQUAKE3,
	COM_MODULE_API_QVM | COM_MODULE_API_LEGACY_NATIVE,
	COM_MODULE_API_QVM,
	0,
	0,
	"2200",
	"Quake 3 Arena",
	COM_GAME_TITLE_QUAKE3,
	COM_GAME_MARKER_QUAKE3,
	{ "q3", "quake3arena", nullptr, nullptr },
	"q3config.cfg",
	"q3config_server.cfg",
	nullptr,
	nullptr,
	nullptr,
	64, 1024, 544, 256, COM_ENTITY_HULL_TRACE_SHAPE, "servercache.dat",
	{ "0", "20", "8", "8", nullptr, nullptr, 1, "1", 1 },
	{ "1", "64", "1024", 1024, COM_BOT_AI_QUAKE, 1, 0, 0, 1 },
	COM_PURE_PROOF_QVM,
	{ "sarge", "headmodel", "sarge", "sarge", "color1", "5", 1, 0 },
	{ "cinematic idlogo.RoQ", "cinematic intro.RoQ", 0 }
}, {
	COM_GAME_QUAKELIVE,
	"quakelive",
	"Quake Live",
	"baseq3",
	"pak00.pk3",
	91,
	91,
	COM_PROTOCOL_FAMILY_LEGACY_QUAKE3 | COM_PROTOCOL_FAMILY_IOQUAKE3
		| COM_PROTOCOL_FAMILY_QUAKELIVE,
	COM_MODULE_API_QVM | COM_MODULE_API_LEGACY_NATIVE
		| COM_MODULE_API_STRUCTURED_NATIVE,
	COM_MODULE_API_STRUCTURED_NATIVE,
	1,
	1,
	"282440",
	"Quake Live",
	COM_GAME_TITLE_QUAKELIVE,
	COM_GAME_MARKER_QUAKELIVE,
	{ "ql", nullptr, nullptr, nullptr },
	"qzconfig.cfg",
	"q3config_server.cfg",
	nullptr,
	nullptr,
	nullptr,
	64, 1024, 544, 256, COM_ENTITY_HULL_TRACE_SHAPE, "servercache.dat",
	{ "0", "50", "8", "8", nullptr, nullptr, 1, "1", 1 },
	{ "1", "64", "1024", 1024, COM_BOT_AI_QUAKE, 1, 0, 0, 1 },
	COM_PURE_PROOF_QL_NATIVE,
	{ "sarge", "headmodel", "sarge", "sarge", "color1", "5", 1, 0 },
	{ "cinematic idlogo.RoQ", "cinematic intro.RoQ", 0 }
}, {
	COM_GAME_RTCW_SP,
	"rtcw-sp",
	"Return to Castle Wolfenstein (Single Player)",
	"main",
	"pak0.pk3",
	49,
	49,
	COM_PROTOCOL_FAMILY_RTCW_SP | COM_PROTOCOL_FAMILY_IORTCW_SP,
	COM_MODULE_API_LEGACY_NATIVE,
	COM_MODULE_API_LEGACY_NATIVE,
	0,
	0,
	"9010",
	"Return to Castle Wolfenstein",
	COM_GAME_TITLE_RTCW,
	COM_GAME_MARKER_RTCW_SP,
	{ "rtcwsp", "wolfsp", nullptr, nullptr },
	"wolfconfig.cfg",
	"wolfconfig_server.cfg",
	"sp_",
	"mp_",
	"RTCW SP module, wire, savegame and renderer adapters are not yet complete",
	128, 2048, 544, 512, COM_ENTITY_HULL_ENTITY_FLAG, nullptr,
	{ "0", "20", "8", "32", "1", "64", 1, "0", 0 },
	{ "1", "128", "2048", 2048, COM_BOT_AI_MOVEMENT, 2, 1, 16384, 0 },
	COM_PURE_PROOF_UNAVAILABLE,
	{ "bj2", "head", "default", nullptr, "color", nullptr, 0, 1 },
	{ nullptr, "cinematic wolfintro.RoQ 3", 1 }
}, {
	COM_GAME_RTCW_MP,
	"rtcw-mp",
	"Return to Castle Wolfenstein (Multiplayer)",
	"main",
	"mp_pak0.pk3",
	60,
	60,
	COM_PROTOCOL_FAMILY_RTCW_MP | COM_PROTOCOL_FAMILY_IORTCW_MP,
	COM_MODULE_API_LEGACY_NATIVE,
	COM_MODULE_API_LEGACY_NATIVE,
	0,
	0,
	"9010",
	"Return to Castle Wolfenstein",
	COM_GAME_TITLE_RTCW,
	COM_GAME_MARKER_RTCW_MP,
	{ "rtcwmp", "wolfmp", nullptr, nullptr },
	"wolfconfig_mp.cfg",
	"wolfconfig_server.cfg",
	"mp_",
	"sp_",
	"RTCW MP module, wire and renderer adapters are not yet complete",
	64, 2048, 576, 512, COM_ENTITY_HULL_ENTITY_FLAG, "fntech3-rtcw-mp-servercache.dat",
	{ "5", "0", "20", "32", "3", nullptr, 0, "1", 0 },
	{ "0", "128", "1024", 1024, 0, 2, 1, 16384, 0 },
	COM_PURE_PROOF_RTCW_NATIVE,
	{ "multi", "head", "default", nullptr, "color", nullptr, 0, 1 },
	{ "cinematic gmlogo.RoQ", "cinematic wolfintro.RoQ", 0 }
}, {
	COM_GAME_WOLFET,
	"wolfet",
	"Wolfenstein: Enemy Territory",
	"etmain",
	"pak0.pk3",
	84,
	84,
	COM_PROTOCOL_FAMILY_WOLFET,
	COM_MODULE_API_LEGACY_NATIVE,
	COM_MODULE_API_LEGACY_NATIVE,
	0,
	0,
	"1873030",
	"Wolfenstein Enemy Territory",
	COM_GAME_TITLE_WOLFET,
	COM_GAME_MARKER_WOLFET,
	{ "et", "enemy-territory", "enemyterritory", nullptr },
	"etconfig.cfg",
	"etconfig_server.cfg",
	nullptr,
	nullptr,
	"Wolfenstein: Enemy Territory module, wire and renderer adapters are not yet complete",
	64, 1024, 689, 512, COM_ENTITY_HULL_ENTITY_FLAG, "fntech3-wolfet-servercache.dat",
	{ "4", "0", "20", "20", nullptr, nullptr, 0, "1", 0 },
	{ "0", "64", "1024", 1024, 0, 1, 1, 16384, 0 },
	COM_PURE_PROOF_WOLFET_NATIVE,
	{ "multi", "head", "default", nullptr, "color", nullptr, 0, 1 },
	{ "cinematic etintro.roq", nullptr, 0 }
} };

static_assert( sizeof( profiles ) / sizeof( profiles[0] ) == COM_GAME_COUNT - 1,
	"Each concrete game ID needs one catalog entry" );

const comGameProfile_t *activeProfile;

bool EqualsIgnoreCase( const char *left, const char *right )
{
	unsigned char leftChar;
	unsigned char rightChar;

	if ( !left || !right ) {
		return false;
	}

	for ( ;; left++, right++ ) {
		leftChar = static_cast<unsigned char>( *left );
		rightChar = static_cast<unsigned char>( *right );
		if ( std::tolower( leftChar ) != std::tolower( rightChar ) ) {
			return false;
		}
		if ( leftChar == '\0' ) {
			return true;
		}
	}
}

bool RuntimeBlocked( const comGameProfile_t *profile )
{
#if defined(FNTECH3_WOLFET_DEVELOPMENT) && defined(DEDICATED)
	// Server probes only until the distinct ET client/renderer boundary exists.
	if (profile && profile->id == COM_GAME_WOLFET) return false;
#endif
#if defined( FNTECH3_RTCW_DEVELOPMENT )
	/* Explicit developer builds exercise incomplete adapters with real modules.
	 * This does not alter any profile contract or the release gate ledger. */
	if ( profile && profile->titleId == COM_GAME_TITLE_RTCW ) return false;
#endif
	return profile && profile->runtimeBlocker;
}

comGameDetectionResult_t Result( comGameDetectionStatus_t status,
	comGameDetectionEvidence_t evidence, const comGameProfile_t *profile )
{
	comGameDetectionResult_t result;
#if defined( FNTECH3_Q3_ONLY )
	/* Preserve evidence priority: a detected QL installation must fail here,
	 * before activation/filesystem startup, rather than falling through to a
	 * different installation or exposing a retail x86 module to an x64 host. */
	if ( profile && profile->id != COM_GAME_QUAKE3 ) {
		status = COM_GAME_DETECTION_UNSUPPORTED_BUILD;
		profile = nullptr;
	}
#endif
	if ( RuntimeBlocked( profile ) ) {
		status = COM_GAME_DETECTION_INCOMPLETE_ENGINE;
		profile = nullptr;
	}
	result.status = status;
	result.evidence = evidence;
	result.profile = profile;
	return result;
}

comGameDetectionResult_t DetectCandidate( unsigned markers,
	comGameDetectionEvidence_t evidence )
{
	if ( markers & COM_GAME_MARKER_UNRECOGNIZED_PAK ) {
		return Result( COM_GAME_DETECTION_UNRECOGNIZED_PAK, evidence, nullptr );
	}
	const comGameProfile_t *candidate = nullptr;
	bool multiple = false, mixedTitles = false;
	for ( const auto &profile : profiles ) {
		if ( !( markers & profile.installMarker ) ) continue;
		if ( candidate ) {
			multiple = true;
			mixedTitles |= candidate->titleId != profile.titleId;
		}
		candidate = &profile;
	}
	if ( multiple ) {
		return Result( mixedTitles ? COM_GAME_DETECTION_AMBIGUOUS
			: COM_GAME_DETECTION_MODE_REQUIRED, evidence, nullptr );
	}
	if ( candidate ) {
		return Result( COM_GAME_DETECTION_OK, evidence, candidate );
	}

	return Result( COM_GAME_DETECTION_NOT_FOUND, COM_GAME_EVIDENCE_NONE, nullptr );
}

bool IsRegularFile( const std::filesystem::path &path )
{
	std::error_code error;
	const bool regular = std::filesystem::is_regular_file( path, error );
	return regular && !error;
}

bool HasPakPrefix( const char *name, const char *prefix )
{
	if ( !name || !prefix ) return false;
	for ( ; *prefix; ++prefix, ++name ) {
		if ( std::tolower( static_cast<unsigned char>( *name ) )
			!= std::tolower( static_cast<unsigned char>( *prefix ) ) ) return false;
	}
	return true;
}

unsigned LE16( const unsigned char *p )
{
	return p[0] | (unsigned( p[1] ) << 8);
}

struct PakIdentity {
	unsigned marker;
	std::uint32_t configSize, configCrc;
	const char *map;
	std::uint32_t mapSize, mapCrc;
};

const PakIdentity pakIdentities[] = {
	{ COM_GAME_MARKER_QUAKE3, 1737, 0x9a18b2fdu,
		"maps/q3dm6.bsp", 4408212, 0x9fef9a4bu },
	{ COM_GAME_MARKER_QUAKELIVE, 1580, 0xc6c9f233u,
		"maps/campgrounds.bsp", 2924532, 0x0ddc0f1du },
	{ COM_GAME_MARKER_RTCW_SP, 2237, 0x844222c3u,
		"maps/escape1.bsp", 12366352, 0x5f08b87cu },
	{ COM_GAME_MARKER_RTCW_MP, 1372, 0x91dfaca0u,
		"maps/mp_beach.bsp", 7343512, 0x77533d56u },
	// Steam build 7997832: metadata only, no retail payload is incorporated.
	{ COM_GAME_MARKER_WOLFET, 1496, 0x061b5285u,
		"maps/goldrush.bsp", 14757996, 0x7ebac2f4u }
};

std::uint32_t LE32( const unsigned char *p )
{
	return LE16( p ) | (std::uint32_t( LE16( p + 2 ) ) << 16);
}

// This is an early identity hint, not authentication or a ZIP payload validator.
// Layout: PKWARE APPNOTE 6.3.10, sections 4.3.12 and 4.3.16.
// Retail metadata observations: docs/fntech3/2026-09-08-pak-identity.md.
bool RecognizedPak( const std::filesystem::path &path, unsigned expected )
{
	const PakIdentity *identity = nullptr;
	for ( const auto &candidate : pakIdentities ) {
		if ( candidate.marker == expected ) identity = &candidate;
	}
	if ( !identity ) return false;
	std::ifstream file( path, std::ios::binary | std::ios::ate );
	const auto end = file.tellg();
	if ( !file || end < 22 || end > std::streamoff( UINT32_MAX ) ) {
		return false;
	}
	const auto size = static_cast<std::uint32_t>( end );
	const auto tailSize = size < 65557u ? size : 65557u;
	std::vector<unsigned char> tail( tailSize );
	file.seekg( size - tailSize );
	if ( !file.read( reinterpret_cast<char *>( tail.data() ), tailSize ) ) {
		return false;
	}
	std::size_t eocd = tailSize;
	for ( std::size_t pos = tailSize - 22; ; --pos ) {
		if ( LE32( &tail[pos] ) == 0x06054b50u
			&& pos + 22u + LE16( &tail[pos + 20] ) == tailSize ) {
			eocd = pos;
			break;
		}
		if ( pos == 0 ) break;
	}
	if ( eocd == tailSize ) return false;
	const auto *e = &tail[eocd];
	const unsigned count = LE16( e + 10 );
	const std::uint32_t bytes = LE32( e + 12 ), offset = LE32( e + 16 );
	// Bound startup work. Split archives, ZIP64 and unknown layouts require an
	// explicit profile; the full filesystem remains responsible for loading.
	if ( LE16( e + 4 ) || LE16( e + 6 ) || LE16( e + 8 ) != count
		|| count == 0 || count == 65535 || bytes > 8u * 1024u * 1024u
		|| std::uint64_t( offset ) + bytes != size - tailSize + eocd ) {
		return false;
	}
	std::vector<unsigned char> directory( bytes );
	file.seekg( offset );
	if ( !file.read( reinterpret_cast<char *>( directory.data() ), bytes ) ) return false;
	unsigned defaultSeen = 0, mapSeen = 0;
	bool defaultMatches = false, mapMatches = false, foreignMapSeen = false;
	std::size_t pos = 0;
	for ( unsigned i = 0; i < count; ++i ) {
		if ( bytes - pos < 46 ) return false;
		const auto *h = directory.data() + pos;
		const unsigned nameLength = LE16( h + 28 );
		const std::size_t recordSize = 46u + nameLength + LE16( h + 30 ) + LE16( h + 32 );
		if ( LE32( h ) != 0x02014b50u || recordSize > bytes - pos
			|| !nameLength || LE16( h + 34 ) || ( LE16( h + 8 ) & 1u )
			|| ( LE16( h + 10 ) != 0 && LE16( h + 10 ) != 8 )
			|| std::uint64_t( LE32( h + 42 ) ) + 30u + LE32( h + 20 ) > offset ) {
			return false;
		}
		const std::string name( reinterpret_cast<const char *>( h + 46 ), nameLength );
		const auto length = LE32( h + 24 ), crc = LE32( h + 16 );
		if ( name == "default.cfg" ) {
			++defaultSeen;
			defaultMatches = length == identity->configSize && crc == identity->configCrc;
		} else if ( name == identity->map ) {
			++mapSeen;
			mapMatches = length == identity->mapSize && crc == identity->mapCrc;
		} else {
			for ( const auto &other : pakIdentities ) {
				if ( name == other.map ) foreignMapSeen = true;
			}
		}
		pos += recordSize;
	}
	return pos == bytes && defaultSeen == 1 && defaultMatches
		&& mapSeen == 1 && mapMatches && !foreignMapSeen;
}

} // namespace

extern "C" {

unsigned Com_GameManagerTitleCount( void )
{
	return sizeof( titles ) / sizeof( titles[0] );
}

const comGameTitle_t *Com_GameManagerTitleAt( unsigned index )
{
	return index < Com_GameManagerTitleCount() ? &titles[index] : nullptr;
}

const comGameTitle_t *Com_GameManagerTitleById( comGameTitleId_t id )
{
	for ( const auto &title : titles ) {
		if ( title.id == id ) return &title;
	}
	return nullptr;
}

unsigned Com_GameManagerProfileCount( void )
{
	return sizeof( profiles ) / sizeof( profiles[0] );
}

const comGameProfile_t *Com_GameManagerProfileAt( unsigned index )
{
	return index < Com_GameManagerProfileCount() ? &profiles[index] : nullptr;
}

const comGameProfile_t *Com_GameManagerFindProfile( const char *key )
{
	if ( !key || !key[0] ) return nullptr;
	for ( const auto &profile : profiles ) {
		if ( EqualsIgnoreCase( key, profile.key ) ) return &profile;
		for ( const char *alias : profile.aliases ) {
			if ( EqualsIgnoreCase( key, alias ) ) return &profile;
		}
	}
	return nullptr;
}

unsigned Com_GameManagerTitleProfileCount( comGameTitleId_t title )
{
	unsigned count = 0;
	for ( const auto &profile : profiles ) {
		if ( profile.titleId == title ) ++count;
	}
	return count;
}

const comGameProfile_t *Com_GameManagerTitleProfileAt(
	comGameTitleId_t title, unsigned index )
{
	for ( const auto &profile : profiles ) {
		if ( profile.titleId == title && index-- == 0 ) return &profile;
	}
	return nullptr;
}

int Com_GameManagerCanActivate( const comGameProfile_t *profile )
{
	if ( !profile || Com_GameProfileById( profile->id ) != profile
		|| RuntimeBlocked( profile ) ) return 0;
#if defined( FNTECH3_Q3_ONLY )
	return profile->id == COM_GAME_QUAKE3;
#else
	return 1;
#endif
}

comGameBspFormat_t Com_GameManagerBspFormat( const comGameProfile_t *profile,
	int version )
{
	if ( !profile || Com_GameProfileById( profile->id ) != profile )
		return COM_GAME_BSP_UNSUPPORTED;
	if ( profile->titleId == COM_GAME_TITLE_RTCW )
		return version == 47 ? COM_GAME_BSP_RTCW : COM_GAME_BSP_UNSUPPORTED;
	if ( profile->titleId == COM_GAME_TITLE_WOLFET )
		return version == 47 ? COM_GAME_BSP_WOLFET : COM_GAME_BSP_UNSUPPORTED;
	if ( profile->titleId != COM_GAME_TITLE_QUAKE3 && profile->titleId != COM_GAME_TITLE_QUAKELIVE )
		return COM_GAME_BSP_UNSUPPORTED;
	/* Preserve the existing Q3/QL asset set, including IHV translation. RTCW's
	 * v47 header is pinned to the source/retail observations in the RTCW ledger. */
	switch ( version ) {
	case 43: return COM_GAME_BSP_QUAKE3_IHV;
	case 46: return COM_GAME_BSP_QUAKE3;
	case 47: return COM_GAME_BSP_QUAKELIVE;
	default: return COM_GAME_BSP_UNSUPPORTED;
	}
}

comGameBspFormat_t Com_GameBspFormat( int version )
{
	return Com_GameManagerBspFormat( activeProfile, version );
}

comGameModeLaunchStatus_t Com_GameManagerFormatModeLaunch(
	const comGameProfile_t *current, const comGameProfile_t *target,
	const char *basePath, const char *homePath, int fullscreen,
	char *destination, unsigned capacity )
{
	if ( !current || !target || Com_GameProfileById( current->id ) != current ||
		Com_GameProfileById( target->id ) != target ) return COM_GAME_MODE_LAUNCH_INVALID_PROFILE;
	if ( current == target ) return COM_GAME_MODE_LAUNCH_ALREADY_SELECTED;
	if ( current->titleId != target->titleId ) return COM_GAME_MODE_LAUNCH_DIFFERENT_TITLE;
	if ( !destination || !basePath || !basePath[0] || !homePath || !homePath[0] ||
		( fullscreen != 0 && fullscreen != 1 ) ) return COM_GAME_MODE_LAUNCH_INVALID_VALUE;
	for ( const char *value : { basePath, homePath } ) {
		unsigned length = 0;
		for ( ; length < COM_GAME_MODE_LAUNCH_BYTES && value[length]; ++length ) {
			const unsigned char ch = static_cast<unsigned char>( value[length] );
			if ( ch < 32 || ch == 127 || ch == '"' ) return COM_GAME_MODE_LAUNCH_INVALID_VALUE;
		}
		if ( length == COM_GAME_MODE_LAUNCH_BYTES ) return COM_GAME_MODE_LAUNCH_TOO_LONG;
	}
	std::string command = "+set com_gameProfile \"";
	command += target->key;
	command += "\" +set fs_basepath \"";
	command += basePath;
	command += "\" +set fs_homepath \"";
	command += homePath;
	command += "\" +set fs_game \"\" +set r_fullscreen \"";
	command += fullscreen ? '1' : '0';
	command += '"';
	if ( command.size() >= COM_GAME_MODE_LAUNCH_BYTES || command.size() >= capacity )
		return COM_GAME_MODE_LAUNCH_TOO_LONG;
	std::memcpy( destination, command.c_str(), command.size() + 1 );
	return COM_GAME_MODE_LAUNCH_READY;
}

const char *Com_GameManagerModeLaunchStatusMessage( comGameModeLaunchStatus_t status )
{
	switch ( status ) {
	case COM_GAME_MODE_LAUNCH_READY: return "mode relaunch command is ready";
	case COM_GAME_MODE_LAUNCH_INVALID_PROFILE: return "choose a registered game mode";
	case COM_GAME_MODE_LAUNCH_ALREADY_SELECTED: return "this game mode is already selected";
	case COM_GAME_MODE_LAUNCH_DIFFERENT_TITLE: return "mode switching requires the same game title";
	case COM_GAME_MODE_LAUNCH_INVALID_VALUE: return "mode switching requires non-empty paths without quotes or control characters and a valid window mode";
	case COM_GAME_MODE_LAUNCH_TOO_LONG: return "mode relaunch paths exceed the startup command capacity";
	default: return "invalid mode relaunch request";
	}
}

const char *Com_GameManagerConfigFile( const comGameProfile_t *profile,
	int dedicated )
{
	if ( !profile ) return nullptr;
	return dedicated ? profile->serverConfig : profile->clientConfig;
}

const char *Com_GameManagerServerCacheFile( const comGameProfile_t *profile )
{
	if ( !profile || profile != Com_GameProfileById( profile->id ) ) return nullptr;
	return profile->serverCacheFile;
}

const char *Com_GameConfigFile( void )
{
#ifdef DEDICATED
	return Com_GameManagerConfigFile( activeProfile, 1 );
#else
	return Com_GameManagerConfigFile( activeProfile, 0 );
#endif
}

int Com_GameManagerIsConfigFile( const char *filename )
{
	if ( EqualsIgnoreCase( filename, "autoexec.cfg" )
		|| EqualsIgnoreCase( filename, "repconfig.cfg" ) ) return 1;
	for ( const auto &profile : profiles ) {
		if ( EqualsIgnoreCase( filename, profile.clientConfig )
			|| EqualsIgnoreCase( filename, profile.serverConfig ) ) return 1;
	}
	return 0;
}

int Com_GameManagerNativeModuleFilename( const comGameProfile_t *profile,
	const char *module, const char *architecture, const char *extension,
	unsigned candidate, char *destination, unsigned capacity )
{
	if ( !profile || Com_GameProfileById( profile->id ) != profile
		|| !module || !architecture || !extension || !destination || !capacity
		|| ( std::strcmp( module, "cgame" ) && std::strcmp( module, "qagame" )
			&& std::strcmp( module, "ui" ) ) ) return 0;
	const auto architectureLength = std::strlen( architecture );
	if ( !architectureLength || architectureLength > 32 ) return 0;
	for ( const char *p = architecture; *p; ++p ) {
		if ( !( ( *p >= 'a' && *p <= 'z' ) || ( *p >= 'A' && *p <= 'Z' )
			|| ( *p >= '0' && *p <= '9' ) || *p == '_' ) ) return 0;
	}
	const bool windows = !std::strcmp( extension, ".dll" );
	if ( !windows && std::strcmp( extension, ".so" )
		&& std::strcmp( extension, ".dylib" ) ) return 0;
	const bool rtcw = profile->titleId == COM_GAME_TITLE_RTCW || profile->id == COM_GAME_WOLFET;
	const char *arch = architecture;
	if ( rtcw ) {
		if ( windows && !std::strcmp( arch, "x86_64" ) ) arch = "x64";
		else if ( windows && !std::strcmp( arch, "i386" ) ) arch = "x86";
		else if ( !windows && !std::strcmp( arch, "x86" ) ) arch = "i386";
	}
	std::string name = module;
	if ( rtcw ) {
		if ( candidate == 0 ) {
			name += profile->id == COM_GAME_RTCW_SP
				? ( windows ? "_sp_" : ".sp." ) : ( windows ? "_mp_" : ".mp." );
		} else if ( candidate != 1 || profile->id != COM_GAME_RTCW_SP ) {
			return 0;
		}
	} else if ( candidate ) {
		// Preserve the existing Linux game-only x64 alias after the normal name.
		if ( candidate != 1 || std::strcmp( extension, ".so" )
			|| std::strcmp( arch, "x86_64" ) || std::strcmp( module, "qagame" ) ) return 0;
		arch = "x64";
	}
	name += arch;
	name += extension;
	if ( name.size() >= capacity ) return 0;
	std::memcpy( destination, name.c_str(), name.size() + 1 );
	return 1;
}

int Com_GameManagerPureModuleFilename( const comGameProfile_t *profile,
	unsigned slot, const char *architecture, const char *extension,
	char *destination, unsigned capacity )
{
	if ( !profile || Com_GameProfileById( profile->id ) != profile || !destination || !capacity ) return 0;
	const char *name;
	switch ( profile->pureProof ) {
	case COM_PURE_PROOF_QVM:
		if ( slot > 1 ) return 0;
		name = slot ? "vm/ui.qvm" : "vm/cgame.qvm";
		break;
	case COM_PURE_PROOF_QL_NATIVE:
		if ( slot ) return 0;
		name = "cgamex86.dll"; // Retail QL's native contract is fixed to x86.
		break;
	case COM_PURE_PROOF_RTCW_NATIVE:
	case COM_PURE_PROOF_WOLFET_NATIVE:
		if ( slot > 1 ) return 0;
		return Com_GameManagerNativeModuleFilename( profile, slot ? "ui" : "cgame",
			architecture, extension, 0, destination, capacity );
	default: return 0;
	}
	const auto length = std::strlen( name );
	if ( length >= capacity ) return 0;
	std::memcpy( destination, name, length + 1 );
	return 1;
}

int Com_GameManagerNativeModuleCandidate( const comGameProfile_t *profile,
	const char *module, const char *architecture, const char *extension,
	unsigned candidate, char *destination, unsigned capacity, int *installationRoot )
{
	if ( !installationRoot ) return 0;
	const bool root = profile == Com_GameProfileById( COM_GAME_RTCW_SP ) && candidate >= 2;
	if ( !Com_GameManagerNativeModuleFilename( profile, module, architecture,
		extension, root ? candidate - 2 : candidate, destination, capacity ) ) return 0;
	*installationRoot = root ? 1 : 0;
	return 1;
}

int Com_GameManagerResolveRootModule( const comGameProfile_t *profile,
	const char *root, const char *filename, const char *architecture,
	const char *extension, char *destination, unsigned capacity )
{
	if ( !profile || profile != Com_GameProfileById( COM_GAME_RTCW_SP )
		|| !root || !root[0] || !filename || !destination || !capacity ) return 0;
	bool known = false;
	for ( const char *module : { "cgame", "qagame", "ui" } ) {
		char candidate[64];
		for ( unsigned i = 0; Com_GameManagerNativeModuleFilename( profile,
			module, architecture, extension, i, candidate, sizeof( candidate ) ); ++i ) {
			if ( !std::strcmp( filename, candidate ) ) known = true;
		}
	}
	if ( !known ) return 0;
	try {
		std::error_code error;
		const auto base = std::filesystem::canonical( std::filesystem::u8path( root ), error );
		if ( error ) return 0;
		const auto file = std::filesystem::canonical( base / filename, error );
		if ( error || file.parent_path() != base || !IsRegularFile( file ) ) return 0;
		const auto path = file.u8string();
		if ( path.size() >= capacity ) return 0;
		std::memcpy( destination, path.c_str(), path.size() + 1 );
		return 1;
	} catch ( const std::exception & ) {
		return 0;
	}
}

int Com_GameManagerInstallationMatches( const comGameProfile_t *profile,
	unsigned markers )
{
	if ( !profile || !( markers & profile->installMarker ) ) return 0;
	unsigned titleMarkers = 0;
	for ( const auto &candidate : profiles ) {
		if ( candidate.titleId == profile->titleId ) titleMarkers |= candidate.installMarker;
	}
	return !( markers & ~titleMarkers );
}

int Com_GameManagerAllowsPak( const comGameProfile_t *profile,
	const char *filename )
{
	if ( !profile || !filename || !filename[0]
		|| std::strpbrk( filename, "/\\:" ) ) return 0;
	return !HasPakPrefix( filename, profile->pakExcludePrefix );
}

int Com_GameManagerComparePaks( const comGameProfile_t *profile,
	const char *left, const char *right )
{
	if ( !left || !right ) return left ? 1 : right ? -1 : 0;
	const bool leftOverride = profile && HasPakPrefix( left, profile->pakOverridePrefix );
	const bool rightOverride = profile && HasPakPrefix( right, profile->pakOverridePrefix );
	for ( unsigned i = 0; ; ++i ) {
		const auto l = std::tolower( static_cast<unsigned char>( leftOverride && i < 2 ? 'z' : left[i] ) );
		const auto r = std::tolower( static_cast<unsigned char>( rightOverride && i < 2 ? 'z' : right[i] ) );
		if ( l != r ) return l < r ? -1 : 1;
		if ( !l ) {
			// A literal zz_ archive and a mode archive may have equal virtual
			// keys. Break ties by actual name for deterministic mount order.
			if ( leftOverride != rightOverride ) return leftOverride ? -1 : 1;
			return 0;
		}
	}
}

const comGameProfile_t *Com_GameProfileById( comGameId_t id )
{
	for ( const auto &profile : profiles ) {
		if ( profile.id == id ) return &profile;
	}
	return nullptr;
}

comGameDetectionResult_t Com_DetectGameProfileMarkers(
	const comGameMarkerInputs_t *inputs )
{
	comGameDetectionResult_t result;

	if ( !inputs ) {
		return Result( COM_GAME_DETECTION_NOT_FOUND, COM_GAME_EVIDENCE_NONE, nullptr );
	}

	const char *key = inputs->requestedProfile;
	const comGameProfile_t *requested = Com_GameManagerFindProfile( key );
	if ( requested ) {
		return Result( COM_GAME_DETECTION_OK, COM_GAME_EVIDENCE_EXPLICIT,
			requested );
	}
	for ( const auto &title : titles ) {
		if ( EqualsIgnoreCase( key, title.key ) ) {
			return Result( COM_GAME_DETECTION_MODE_REQUIRED,
				COM_GAME_EVIDENCE_EXPLICIT, nullptr );
		}
	}
	if ( key && key[0] && !EqualsIgnoreCase( key, "auto" ) ) {
		return Result( COM_GAME_DETECTION_UNKNOWN_REQUEST,
			COM_GAME_EVIDENCE_EXPLICIT, nullptr );
	}

	result = DetectCandidate( inputs->basePathMarkers, COM_GAME_EVIDENCE_BASE_PATH );
	if ( result.status != COM_GAME_DETECTION_NOT_FOUND ) {
		return result;
	}

	result = DetectCandidate( inputs->appPathMarkers, COM_GAME_EVIDENCE_APP_PATH );
	if ( result.status != COM_GAME_DETECTION_NOT_FOUND ) {
		return result;
	}

	result = DetectCandidate( inputs->steamPathMarkers, COM_GAME_EVIDENCE_STEAM_PATH );
	if ( result.status != COM_GAME_DETECTION_NOT_FOUND ) {
		return result;
	}

	return Result( COM_GAME_DETECTION_NOT_FOUND, COM_GAME_EVIDENCE_NONE, nullptr );
}

unsigned Com_ProbeGameProfileMarkers( const char *rootPath )
{
	unsigned markers = COM_GAME_MARKER_NONE;

	if ( !rootPath || !rootPath[0] ) {
		return markers;
	}

	try {
		const auto root = std::filesystem::u8path( rootPath );
		for ( const auto &profile : profiles ) {
			const auto pak = root / profile.baseGame / profile.basePakName;
			if ( IsRegularFile( pak ) ) {
				markers |= RecognizedPak( pak, profile.installMarker )
					? profile.installMarker : COM_GAME_MARKER_UNRECOGNIZED_PAK;
			}
		}
	} catch ( const std::system_error & ) {
		// Windows path conversion can fail before the error-code filesystem
		// queries below. An unrepresentable candidate supplies no evidence;
		// do not let a conversion exception escape this C-facing probe.
		return markers;
	}

	return markers;
}

comGameDetectionResult_t Com_DetectGameProfile(
	const comGameDetectionInputs_t *inputs )
{
	comGameMarkerInputs_t markerInputs = {};

	if ( !inputs ) {
		return Result( COM_GAME_DETECTION_NOT_FOUND, COM_GAME_EVIDENCE_NONE, nullptr );
	}

	markerInputs.requestedProfile = inputs->requestedProfile;
	if ( inputs->requestedProfile && inputs->requestedProfile[0]
		&& !EqualsIgnoreCase( inputs->requestedProfile, "auto" ) ) {
		return Com_DetectGameProfileMarkers( &markerInputs );
	}
	markerInputs.basePathMarkers = Com_ProbeGameProfileMarkers( inputs->basePath );
	if ( markerInputs.basePathMarkers ) return Com_DetectGameProfileMarkers( &markerInputs );
	markerInputs.appPathMarkers = Com_ProbeGameProfileMarkers( inputs->appPath );
	if ( markerInputs.appPathMarkers ) return Com_DetectGameProfileMarkers( &markerInputs );
	for ( const auto &profile : profiles ) {
		const char *path = inputs->steamPaths[profile.id];
		if ( !path || !path[0] ) continue;
		bool duplicate = false;
		for ( unsigned i = 1; i < profile.id; ++i ) {
			const char *earlier = inputs->steamPaths[i];
			if ( earlier && !std::strcmp( earlier, path ) ) duplicate = true;
		}
		if ( !duplicate ) markerInputs.steamPathMarkers |= Com_ProbeGameProfileMarkers( path );
	}
	return Com_DetectGameProfileMarkers( &markerInputs );
}

int Com_ActivateGameProfile( comGameId_t id )
{
#if defined( FNTECH3_Q3_ONLY )
	if ( id != COM_GAME_QUAKE3 ) {
		return 0;
	}
#endif
	const comGameProfile_t *requested = Com_GameProfileById( id );

	if ( !Com_GameManagerCanActivate( requested ) ) {
		return 0;
	}
	if ( activeProfile && activeProfile != requested ) {
		return 0;
	}

	activeProfile = requested;
	return 1;
}

const comGameProfile_t *Com_GameProfile( void )
{
	return activeProfile;
}

int Com_GameClientLimit( void )
{
	return activeProfile ? activeProfile->maxClients : 0;
}

int Com_GameConfigstringLimit( void )
{
	return activeProfile ? activeProfile->maxConfigstrings : 0;
}

int Com_GamePlayerConfigstringBase( void )
{
	return activeProfile ? activeProfile->playerConfigstringBase : 0;
}

int Com_GameIsQuake3( void )
{
	return activeProfile && activeProfile->id == COM_GAME_QUAKE3;
}

int Com_GameIsQuakeLive( void )
{
	return activeProfile && activeProfile->id == COM_GAME_QUAKELIVE;
}

int Com_GameIsRTCW( void )
{
	return activeProfile && activeProfile->titleId == COM_GAME_TITLE_RTCW;
}

int Com_GameIsRTCWSP( void )
{
	return activeProfile && activeProfile->id == COM_GAME_RTCW_SP;
}

int Com_GameIsRTCWMP( void )
{
	return activeProfile && activeProfile->id == COM_GAME_RTCW_MP;
}

int Com_GameIsWolfET( void )
{
	return activeProfile && activeProfile->id == COM_GAME_WOLFET;
}

int Com_PureProofIsNativeModulePair( const comGameProfile_t *profile )
{
	return profile && ( profile->pureProof == COM_PURE_PROOF_RTCW_NATIVE
		|| profile->pureProof == COM_PURE_PROOF_WOLFET_NATIVE );
}


unsigned Com_ProtocolFamilyForVersion( int protocol )
{
	switch ( protocol ) {
		case 49:
			return COM_PROTOCOL_FAMILY_RTCW_SP;
		case 50:
			return COM_PROTOCOL_FAMILY_IORTCW_SP;
		case 60:
			return COM_PROTOCOL_FAMILY_RTCW_MP;
		case 61:
			return COM_PROTOCOL_FAMILY_IORTCW_MP;
		case 68:
			return COM_PROTOCOL_FAMILY_LEGACY_QUAKE3;
		case 71:
			return COM_PROTOCOL_FAMILY_IOQUAKE3;
		case 84:
			return COM_PROTOCOL_FAMILY_WOLFET;
		case 91:
			return COM_PROTOCOL_FAMILY_QUAKELIVE;
		default:
			return 0;
	}
}

int Com_GameProfileAllowsProtocol( const comGameProfile_t *profile,
	int protocol )
{
	const unsigned family = Com_ProtocolFamilyForVersion( protocol );
	return profile && family != 0
		&& ( profile->allowedProtocolFamilies & family ) != 0;
}

int Com_GameProfileAllowsModuleApi( const comGameProfile_t *profile,
	unsigned moduleApi )
{
	const unsigned knownApis = COM_MODULE_API_QVM
		| COM_MODULE_API_LEGACY_NATIVE
		| COM_MODULE_API_STRUCTURED_NATIVE;
	return profile && moduleApi != 0
		&& ( moduleApi & ( moduleApi - 1 ) ) == 0
		&& ( moduleApi & knownApis ) != 0
		&& ( profile->allowedModuleApis & moduleApi ) != 0;
}

const char *Com_GameDetectionStatusName( comGameDetectionStatus_t status )
{
	switch ( status ) {
		case COM_GAME_DETECTION_OK:
			return "ok";
		case COM_GAME_DETECTION_NOT_FOUND:
			return "not found";
		case COM_GAME_DETECTION_AMBIGUOUS:
			return "ambiguous";
		case COM_GAME_DETECTION_UNKNOWN_REQUEST:
			return "unknown profile";
		case COM_GAME_DETECTION_UNSUPPORTED_BUILD:
			return "this game is unavailable in the Q3-only build; use the shared Win32 fnTech3 binary";
		case COM_GAME_DETECTION_UNRECOGNIZED_PAK:
			return "unrecognized base pak identity; verify the installation or select its profile explicitly";
		case COM_GAME_DETECTION_MODE_REQUIRED:
			return "choose the RTCW mode explicitly with +set com_gameProfile rtcw-sp or rtcw-mp";
		case COM_GAME_DETECTION_INCOMPLETE_ENGINE:
			return "the game is recognized, but its engine adapters are not yet complete; see docs/fntech3/COMPATIBILITY.md";
		default:
			return "invalid status";
	}
}

const char *Com_GameDetectionEvidenceName( comGameDetectionEvidence_t evidence )
{
	switch ( evidence ) {
		case COM_GAME_EVIDENCE_EXPLICIT:
			return "command line";
		case COM_GAME_EVIDENCE_BASE_PATH:
			return "fs_basepath";
		case COM_GAME_EVIDENCE_APP_PATH:
			return "application directory";
		case COM_GAME_EVIDENCE_STEAM_PATH:
			return "Steam installation";
		default:
			return "none";
	}
}

} // extern "C"
