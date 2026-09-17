/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_RTCW_MARKS_HPP
#define FNTECH3_CLIENT_RTCW_MARKS_HPP

#include "../renderercommon/tr_rtcw_marks.h"
#include "../compat/rtcw_game_abi.h"
#include <climits>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include "client_rtcw_render_memory.hpp"

inline int CL_RTCWMarkFragments( vm_t *vm, const intptr_t *args ) {
	for ( int index : { 1, 4, 6 } )
		if ( args[index] < INT_MIN || args[index] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid RTCW mark integer" );
	const int maxPoints = static_cast<int>( args[4] );
	const int signedFragments = static_cast<int>( args[6] );
	if ( maxPoints < 0 || signedFragments == INT_MIN )
		Com_Error( ERR_DROP, "Invalid RTCW mark capacity" );
	const int maxFragments = signedFragments < 0 ? -signedFragments : signedFragments;
	if ( !maxPoints || !maxFragments ) return 0;
	const std::size_t pointBytes = static_cast<std::size_t>( maxPoints ) * sizeof( fnRTCWMarkPoint_t );
	const std::size_t fragmentBytes = static_cast<std::size_t>( maxFragments ) * sizeof( markFragment_t );
	if ( maxPoints > INT_MAX / static_cast<int>( sizeof( fnRTCWMarkPoint_t ) ) ||
		maxFragments > INT_MAX / static_cast<int>( sizeof( markFragment_t ) ) ||
		pointBytes > INT_MAX - fragmentBytes )
		Com_Error( ERR_DROP, "RTCW mark buffers are too large" );
	vec3_t corners[4], projection;
	std::memcpy( corners, CL_RTCWRenderSpan( vm, args[2], sizeof( corners ) ), sizeof( corners ) );
	std::memcpy( projection, CL_RTCWRenderSpan( vm, args[3], sizeof( projection ) ), sizeof( projection ) );
	for ( const auto &corner : corners ) for ( float value : corner )
		if ( !FnRTCWMarkFinite( value ) ) Com_Error( ERR_DROP, "Non-finite RTCW mark corner" );
	for ( float value : projection )
		if ( !FnRTCWMarkFinite( value ) ) Com_Error( ERR_DROP, "Non-finite RTCW mark projection" );
	auto *pointOutput = static_cast<unsigned char *>( CL_RTCWRenderSpan( vm, args[5],
		static_cast<std::size_t>( maxPoints ) * sizeof( fntech3_rtcw_markPoint_t ) ) );
	auto *fragmentOutput = static_cast<unsigned char *>( CL_RTCWRenderSpan( vm, args[7],
		static_cast<std::size_t>( maxFragments ) * sizeof( fntech3_rtcw_markFragment_t ) ) );
	const auto pointAddress = reinterpret_cast<std::uintptr_t>( pointOutput );
	const auto fragmentAddress = reinterpret_cast<std::uintptr_t>( fragmentOutput );
	if ( pointAddress <= fragmentAddress ? fragmentAddress - pointAddress < pointBytes :
		pointAddress - fragmentAddress < fragmentBytes )
		Com_Error( ERR_DROP, "Overlapping RTCW mark outputs" );
	std::unique_ptr<void, decltype( &Hunk_FreeTempMemory )> memory(
		Hunk_AllocateTempMemory( static_cast<int>( pointBytes + fragmentBytes ) ), Hunk_FreeTempMemory );
	auto *points = static_cast<fnRTCWMarkPoint_t *>( memory.get() );
	auto *fragments = reinterpret_cast<markFragment_t *>( static_cast<unsigned char *>( memory.get() ) + pointBytes );
	std::memset( memory.get(), 0, pointBytes + fragmentBytes );
	const int count = re.MarkRTCWFragments( static_cast<int>( args[1] ), corners, projection,
		maxPoints, points, signedFragments, fragments );
	if ( count < 0 || count > maxFragments ) Com_Error( ERR_DROP, "Invalid RTCW mark result count" );
	// Validate every result before publishing any module-visible bytes.
	int usedPoints = 0;
	for ( int i = 0; i < count; ++i ) {
		const auto &fragment = fragments[i];
		if ( fragment.numPoints == INT_MIN ) Com_Error( ERR_DROP, "Invalid RTCW mark fragment" );
		const int length = fragment.numPoints < 0 ? -fragment.numPoints : fragment.numPoints;
		if ( length < 3 || fragment.firstPoint != usedPoints || length > maxPoints - usedPoints )
			Com_Error( ERR_DROP, "Invalid RTCW mark point range" );
		for ( int p = usedPoints; p < usedPoints + length; ++p ) {
			for ( float value : points[p].xyz )
				if ( !FnRTCWMarkFinite( value ) ) Com_Error( ERR_DROP, "Non-finite RTCW mark result" );
			if ( fragment.numPoints < 0 ) for ( float value : points[p].st )
				if ( !FnRTCWMarkFinite( value ) ) Com_Error( ERR_DROP, "Non-finite RTCW mark texture coordinate" );
		}
		usedPoints += length;
	}
	for ( int i = 0; i < count; ++i ) {
		const auto &fragment = fragments[i];
		const int length = fragment.numPoints < 0 ? -fragment.numPoints : fragment.numPoints;
		for ( int p = fragment.firstPoint; p < fragment.firstPoint + length; ++p ) {
			fntech3_rtcw_markPoint_t point;
			std::memcpy( point.xyz, points[p].xyz, sizeof( point.xyz ) );
			std::memcpy( point.st, points[p].st, sizeof( point.st ) );
			std::memcpy( pointOutput + p * sizeof( point ), &point,
				fragment.numPoints < 0 ? sizeof( point ) : sizeof( point.xyz ) );
		}
		const fntech3_rtcw_markFragment_t record{ fragment.firstPoint, fragment.numPoints };
		std::memcpy( fragmentOutput + i * sizeof( record ), &record, sizeof( record ) );
	}
	return count;
}

#endif
