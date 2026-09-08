/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#include "game_profile.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <system_error>
#include <vector>

namespace {

/* Steam AppIDs and conventional directories were observed in the local retail
 * manifests; see docs/fntech3/2026-09-07-steam-discovery.md. Discovery identity
 * is independent of each profile's optional service integration contract. */

const comGameProfile_t quake3Profile = {
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
	"Quake 3 Arena"
};

const comGameProfile_t quakeLiveProfile = {
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
	"Quake Live"
};

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

comGameId_t ParseRequestedProfile( const char *requested )
{
	if ( !requested || !requested[0] || EqualsIgnoreCase( requested, "auto" ) ) {
		return COM_GAME_AUTO;
	}

	if ( EqualsIgnoreCase( requested, "q3" )
		|| EqualsIgnoreCase( requested, "quake3" )
		|| EqualsIgnoreCase( requested, "quake3arena" ) ) {
		return COM_GAME_QUAKE3;
	}

	if ( EqualsIgnoreCase( requested, "ql" )
		|| EqualsIgnoreCase( requested, "quakelive" ) ) {
		return COM_GAME_QUAKELIVE;
	}

	return static_cast<comGameId_t>( -1 );
}

comGameDetectionResult_t Result( comGameDetectionStatus_t status,
	comGameDetectionEvidence_t evidence, const comGameProfile_t *profile )
{
	comGameDetectionResult_t result;
#if defined( FNTECH3_Q3_ONLY )
	/* Preserve evidence priority: a detected QL installation must fail here,
	 * before activation/filesystem startup, rather than falling through to a
	 * different installation or exposing a retail x86 module to an x64 host. */
	if ( profile && profile->id == COM_GAME_QUAKELIVE ) {
		status = COM_GAME_DETECTION_UNSUPPORTED_BUILD;
		profile = nullptr;
	}
#endif
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
	const unsigned knownMarkers = markers
		& ( COM_GAME_MARKER_QUAKE3 | COM_GAME_MARKER_QUAKELIVE );

	if ( knownMarkers == ( COM_GAME_MARKER_QUAKE3 | COM_GAME_MARKER_QUAKELIVE ) ) {
		return Result( COM_GAME_DETECTION_AMBIGUOUS, evidence, nullptr );
	}
	if ( knownMarkers == COM_GAME_MARKER_QUAKE3 ) {
		return Result( COM_GAME_DETECTION_OK, evidence, &quake3Profile );
	}
	if ( knownMarkers == COM_GAME_MARKER_QUAKELIVE ) {
		return Result( COM_GAME_DETECTION_OK, evidence, &quakeLiveProfile );
	}

	return Result( COM_GAME_DETECTION_NOT_FOUND, COM_GAME_EVIDENCE_NONE, nullptr );
}

bool IsRegularFile( const std::filesystem::path &path )
{
	std::error_code error;
	const bool regular = std::filesystem::is_regular_file( path, error );
	return regular && !error;
}

unsigned LE16( const unsigned char *p )
{
	return p[0] | (unsigned( p[1] ) << 8);
}

std::uint32_t LE32( const unsigned char *p )
{
	return LE16( p ) | (std::uint32_t( LE16( p + 2 ) ) << 16);
}

// This is an early identity hint, not authentication or a ZIP payload validator.
// Layout: PKWARE APPNOTE 6.3.10, sections 4.3.12 and 4.3.16.
// Retail metadata observations: docs/fntech3/2026-09-08-pak-identity.md.
bool RecognizedPak( const std::filesystem::path &path, unsigned expected )
{
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
	unsigned defaultSeen = 0, q3MapSeen = 0, qlMapSeen = 0;
	bool defaultMatches = false, mapMatches = false;
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
			defaultMatches = expected == COM_GAME_MARKER_QUAKE3
				? length == 1737 && crc == 0x9a18b2fdu
				: length == 1580 && crc == 0xc6c9f233u;
		} else if ( name == "maps/q3dm6.bsp" ) {
			++q3MapSeen;
			if ( expected == COM_GAME_MARKER_QUAKE3 ) mapMatches = length == 4408212 && crc == 0x9fef9a4bu;
		} else if ( name == "maps/campgrounds.bsp" ) {
			++qlMapSeen;
			if ( expected == COM_GAME_MARKER_QUAKELIVE ) mapMatches = length == 2924532 && crc == 0x0ddc0f1du;
		}
		pos += recordSize;
	}
	return pos == bytes && defaultSeen == 1 && defaultMatches && mapMatches
		&& ( expected == COM_GAME_MARKER_QUAKE3 ? q3MapSeen == 1 && qlMapSeen == 0
			: qlMapSeen == 1 && q3MapSeen == 0 );
}

} // namespace

extern "C" {

const comGameProfile_t *Com_GameProfileById( comGameId_t id )
{
	switch ( id ) {
		case COM_GAME_QUAKE3:
			return &quake3Profile;
		case COM_GAME_QUAKELIVE:
			return &quakeLiveProfile;
		default:
			return nullptr;
	}
}

comGameDetectionResult_t Com_DetectGameProfileMarkers(
	const comGameMarkerInputs_t *inputs )
{
	comGameDetectionResult_t result;
	comGameId_t requested;

	if ( !inputs ) {
		return Result( COM_GAME_DETECTION_NOT_FOUND, COM_GAME_EVIDENCE_NONE, nullptr );
	}

	requested = ParseRequestedProfile( inputs->requestedProfile );
	if ( requested == COM_GAME_QUAKE3 || requested == COM_GAME_QUAKELIVE ) {
		return Result( COM_GAME_DETECTION_OK, COM_GAME_EVIDENCE_EXPLICIT,
			Com_GameProfileById( requested ) );
	}
	if ( requested != COM_GAME_AUTO ) {
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
	std::filesystem::path basePath;
	unsigned markers = COM_GAME_MARKER_NONE;

	if ( !rootPath || !rootPath[0] ) {
		return markers;
	}

	try {
		basePath = std::filesystem::u8path( rootPath ) / "baseq3";
	} catch ( const std::system_error & ) {
		// Windows path conversion can fail before the error-code filesystem
		// queries below. An unrepresentable candidate supplies no evidence;
		// do not let a conversion exception escape this C-facing probe.
		return markers;
	}
	if ( IsRegularFile( basePath / quake3Profile.basePakName ) ) {
		markers |= RecognizedPak( basePath / quake3Profile.basePakName, COM_GAME_MARKER_QUAKE3 )
			? COM_GAME_MARKER_QUAKE3 : COM_GAME_MARKER_UNRECOGNIZED_PAK;
	}
	if ( IsRegularFile( basePath / quakeLiveProfile.basePakName ) ) {
		markers |= RecognizedPak( basePath / quakeLiveProfile.basePakName, COM_GAME_MARKER_QUAKELIVE )
			? COM_GAME_MARKER_QUAKELIVE : COM_GAME_MARKER_UNRECOGNIZED_PAK;
	}

	return markers;
}

comGameDetectionResult_t Com_DetectGameProfile(
	const comGameDetectionInputs_t *inputs )
{
	comGameMarkerInputs_t markerInputs;

	if ( !inputs ) {
		return Result( COM_GAME_DETECTION_NOT_FOUND, COM_GAME_EVIDENCE_NONE, nullptr );
	}

	markerInputs.requestedProfile = inputs->requestedProfile;
	markerInputs.basePathMarkers = Com_ProbeGameProfileMarkers( inputs->basePath );
	markerInputs.appPathMarkers = Com_ProbeGameProfileMarkers( inputs->appPath );
	markerInputs.steamPathMarkers = Com_ProbeGameProfileMarkers( inputs->q3SteamPath )
		| Com_ProbeGameProfileMarkers( inputs->qlSteamPath );
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

	if ( !requested ) {
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

int Com_GameIsQuake3( void )
{
	return activeProfile == &quake3Profile;
}

int Com_GameIsQuakeLive( void )
{
	return activeProfile == &quakeLiveProfile;
}

unsigned Com_ProtocolFamilyForVersion( int protocol )
{
	switch ( protocol ) {
		case 68:
			return COM_PROTOCOL_FAMILY_LEGACY_QUAKE3;
		case 71:
			return COM_PROTOCOL_FAMILY_IOQUAKE3;
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
			return "Quake Live is unavailable in this Q3-only build; use the shared Win32 fnTech3 binary";
		case COM_GAME_DETECTION_UNRECOGNIZED_PAK:
			return "unrecognized base pak identity; verify the installation or select its profile explicitly";
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
