/*
===========================================================================
Copyright (C) 2026 FnQL contributors

This file is part of FnQL.

FnQL is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.
===========================================================================
*/

#ifndef FNQL_NETCHAN_PROFILE_H
#define FNQL_NETCHAN_PROFILE_H

/*
 * Netchan framing is not implied by a protocol number's age.  In particular,
 * retail Quake Live protocol 91 kept the Quake III reliable XOR codec and did
 * not adopt ioquake3 protocol 71's extra sequence checksum.  Keep those wire
 * choices explicit so legacy Quake III, existing FnQ3/ioq3, and retail QL
 * connections can coexist without overloading the broader "compat" policy.
 */
#define NETCHAN_QL_RETAIL_PROTOCOL_VERSION 91
#define NETCHAN_LEGACY_Q3_PROTOCOL_VERSION 68
#define NETCHAN_RTCW_SP_PROTOCOL_VERSION 49
#define NETCHAN_RTCW_MP_PROTOCOL_VERSION 60
#define NETCHAN_WOLFET_PROTOCOL_VERSION 84

/* Parsers deliberately pass unknown numeric profiles to the rejecting lookup.
 * A fixed C++ underlying type makes that validation defined for every unsigned
 * input; an unfixed enum can invoke UB before its default switch arm is reached.
 * The C representation and the existing four-byte engine layout are unchanged. */
typedef enum
#ifdef __cplusplus
	: unsigned int
#endif
{
	NETCHAN_WIRE_LEGACY_Q3,
	NETCHAN_WIRE_IOQ3,
	NETCHAN_WIRE_QL_RETAIL,
	NETCHAN_WIRE_RTCW_SP,
	NETCHAN_WIRE_RTCW_MP,
	NETCHAN_WIRE_WOLFET
} netchanWireProfile_t;

typedef enum {
	NETCHAN_FEATURE_CLIENT_QPORT = 1u << 0,
	NETCHAN_FEATURE_SEQUENCE_CHECKSUM = 1u << 1,
	NETCHAN_FEATURE_RELIABLE_XOR = 1u << 2
} netchanWireFeature_t;

#ifdef __cplusplus
#define NETCHAN_PROFILE_INLINE inline constexpr
#else
#define NETCHAN_PROFILE_INLINE static inline
#endif

NETCHAN_PROFILE_INLINE netchanWireProfile_t Netchan_SelectWireProfile(
	int protocol, int legacyCompat ) {
	if ( protocol == NETCHAN_RTCW_SP_PROTOCOL_VERSION ) return NETCHAN_WIRE_RTCW_SP;
	if ( protocol == NETCHAN_RTCW_MP_PROTOCOL_VERSION ) return NETCHAN_WIRE_RTCW_MP;
	if ( protocol == NETCHAN_WOLFET_PROTOCOL_VERSION ) return NETCHAN_WIRE_WOLFET;
	if ( protocol == NETCHAN_QL_RETAIL_PROTOCOL_VERSION ) {
		return NETCHAN_WIRE_QL_RETAIL;
	}
	// Protocol 68 is always the Q3 framing contract. A loopback connection
	// can skip the challenge exchange that normally sets legacyCompat.
	if ( protocol == NETCHAN_LEGACY_Q3_PROTOCOL_VERSION ) {
		return NETCHAN_WIRE_LEGACY_Q3;
	}

	return legacyCompat ? NETCHAN_WIRE_LEGACY_Q3 : NETCHAN_WIRE_IOQ3;
}

NETCHAN_PROFILE_INLINE unsigned int Netchan_WireFeatures(
	netchanWireProfile_t profile ) {
	switch ( profile ) {
	case NETCHAN_WIRE_RTCW_SP:
		// Original RTCW SP qcommon.h sets DO_NET_ENCODE to zero.
		return NETCHAN_FEATURE_CLIENT_QPORT;
	case NETCHAN_WIRE_IOQ3:
		return NETCHAN_FEATURE_CLIENT_QPORT |
			NETCHAN_FEATURE_SEQUENCE_CHECKSUM;
	case NETCHAN_WIRE_QL_RETAIL:
	case NETCHAN_WIRE_RTCW_MP:
	case NETCHAN_WIRE_WOLFET:
	case NETCHAN_WIRE_LEGACY_Q3:
	default:
		return NETCHAN_FEATURE_CLIENT_QPORT |
			NETCHAN_FEATURE_RELIABLE_XOR;
	}
}

NETCHAN_PROFILE_INLINE int Netchan_WireHasFeature(
	netchanWireProfile_t profile, netchanWireFeature_t feature ) {
	return ( Netchan_WireFeatures( profile ) & (unsigned int)feature ) != 0u;
}

/* Protocol limits, independent of the largest engine buffers. */
NETCHAN_PROFILE_INLINE int Netchan_ReliableCommandLimit( netchanWireProfile_t profile ) {
	switch ( profile ) {
	case NETCHAN_WIRE_LEGACY_Q3:
	case NETCHAN_WIRE_IOQ3:
	case NETCHAN_WIRE_QL_RETAIL: return 64;
	case NETCHAN_WIRE_RTCW_SP:
	case NETCHAN_WIRE_RTCW_MP:
	case NETCHAN_WIRE_WOLFET: return 256;
	default: return 0;
	}
}

NETCHAN_PROFILE_INLINE int Netchan_MessageLimit( netchanWireProfile_t profile ) {
	switch ( profile ) {
	case NETCHAN_WIRE_LEGACY_Q3:
	case NETCHAN_WIRE_IOQ3:
	case NETCHAN_WIRE_QL_RETAIL: return 16384;
	case NETCHAN_WIRE_RTCW_SP:
	case NETCHAN_WIRE_RTCW_MP:
	case NETCHAN_WIRE_WOLFET: return 32768;
	default: return 0;
	}
}

#undef NETCHAN_PROFILE_INLINE

#endif
