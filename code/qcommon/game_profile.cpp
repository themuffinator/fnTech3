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
#include <system_error>

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

	basePath = std::filesystem::u8path( rootPath ) / "baseq3";
	if ( IsRegularFile( basePath / quake3Profile.basePakName ) ) {
		markers |= COM_GAME_MARKER_QUAKE3;
	}
	if ( IsRegularFile( basePath / quakeLiveProfile.basePakName ) ) {
		markers |= COM_GAME_MARKER_QUAKELIVE;
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
