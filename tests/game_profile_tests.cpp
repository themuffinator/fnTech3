/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#include "../code/qcommon/game_profile.h"

#include <cassert>

static comGameDetectionResult_t Detect( const char *request, unsigned base,
	unsigned app, unsigned steam )
{
	comGameMarkerInputs_t inputs = { request, base, app, steam };
	return Com_DetectGameProfileMarkers( &inputs );
}

int main()
{
	comGameDetectionResult_t result;

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
