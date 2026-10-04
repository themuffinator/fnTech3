/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_PROTOCOL_CONTRACT_PROFILE_HPP
#define FNTECH3_PROTOCOL_CONTRACT_PROFILE_HPP

#include "game_profile.h"
#include "protocol_contract.hpp"

namespace fnql::protocol {

/* The game-profile protocol families whose connections a contract serves.
 *
 * A version number can belong to more than one game: 26 is ioEF's and Jedi
 * Academy multiplayer's, 40 Elite Force single player's, both Jedi titles'
 * and Q3Test 1.08's, and Find() takes any other legacy number as Quake III's.
 * Com_GameProfileAllowsProtocol() admits a number for every family that
 * speaks it, so a connection also needs the contract Find() chose to belong
	* to the active profile. Profiles with no wire of their own are served by
	* none of them. Kept apart from protocol_contract.hpp,
 * which test fixtures include beside game declarations of their own. */
[[nodiscard]] constexpr unsigned ProfileFamilies( const Contract &contract ) noexcept {
	switch ( contract.family ) {
	case Family::LegacyQuake3:
		return COM_PROTOCOL_FAMILY_LEGACY_QUAKE3 | COM_PROTOCOL_FAMILY_Q3_POINT_RELEASES;
	case Family::Ioquake3:
		return COM_PROTOCOL_FAMILY_IOQUAKE3;
	case Family::QuakeLive:
		return COM_PROTOCOL_FAMILY_QUAKELIVE;
	case Family::RtcwSinglePlayer:
		return COM_PROTOCOL_FAMILY_RTCW_SP;
	case Family::RtcwMultiplayer:
		return COM_PROTOCOL_FAMILY_RTCW_MP;
	case Family::WolfET:
		return COM_PROTOCOL_FAMILY_WOLFET;
	case Family::STVEFHolomatch:
		return COM_PROTOCOL_FAMILY_STVEF_HM;
	case Family::STVEFSinglePlayer:
		return COM_PROTOCOL_FAMILY_STVEF_SP;
	case Family::IoEF:
		return COM_PROTOCOL_FAMILY_IOEF;
	case Family::Alice:
		/* F.A.K.K.2 crosses Alice's wire, with its own records. */
		return COM_PROTOCOL_FAMILY_ALICE | COM_PROTOCOL_FAMILY_FAKK2;
	case Family::JediAcademyMultiplayer:
		return COM_PROTOCOL_FAMILY_JKA_MP;
	case Family::MOHAA:
		return COM_PROTOCOL_FAMILY_MOHAA;
	case Family::JK2Multiplayer:
		return COM_PROTOCOL_FAMILY_JK2_MP;
	}
	return 0;
}

[[nodiscard]] constexpr bool ServesProfileFamilies( const Contract &contract,
	unsigned allowedFamilies ) noexcept {
	return ( ProfileFamilies( contract ) & allowedFamilies ) != 0;
}

[[nodiscard]] inline bool ServesProfile( const Contract &contract,
	const comGameProfile_t *profile ) noexcept {
	return profile && ServesProfileFamilies( contract, profile->allowedProtocolFamilies );
}

/* Exact profile selection precedes any fallback. Two games sharing a
 * protocol number do not share their transport or their message bodies. */
[[nodiscard]] constexpr const Contract *FindForProfile( int protocol,
	unsigned allowedFamilies, bool legacyCompatibility = false ) noexcept {
	if ( ( allowedFamilies & COM_PROTOCOL_FAMILY_JKA_MP ) != 0 ) {
		return protocol == JediAcademyMultiplayer.protocol
			? &JediAcademyMultiplayer : nullptr;
	}
	const Contract *contract = Find( protocol, legacyCompatibility );
	return contract && ServesProfileFamilies( *contract, allowedFamilies )
		? contract : nullptr;
}

[[nodiscard]] inline const Contract *FindForProfile( int protocol,
	const comGameProfile_t *profile, bool legacyCompatibility = false ) noexcept {
	return profile ? FindForProfile( protocol, profile->allowedProtocolFamilies,
		legacyCompatibility ) : nullptr;
}

} // namespace fnql::protocol

#endif
