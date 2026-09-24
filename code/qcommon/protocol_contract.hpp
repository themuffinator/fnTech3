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

#ifndef FNQL_PROTOCOL_CONTRACT_HPP
#define FNQL_PROTOCOL_CONTRACT_HPP

#include "netchan_profile.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fnql::protocol {

// Text challenge responses carry an engine identity marker. Retail Quake Live
// does not emit it, so FnQL clients can honor the project policy of hosting
// retail clients without accidentally joining retail-operated servers.
inline constexpr std::string_view FnqlHandshakeMarker = "FnQL";

enum class Family : std::uint8_t {
	LegacyQuake3,
	Ioquake3,
	QuakeLive,
	RtcwSinglePlayer,
	RtcwMultiplayer,
	WolfET,
	STVEFHolomatch,
	STVEFSinglePlayer,
	Alice
};

/*
 * Netchan framing and game-message bodies are independent compatibility
 * choices.  Protocols 66/67/68 and 71 differ at the transport layer, but all use
 * the original Quake III usercmd/entity/playerstate delta layouts.  Protocol
 * 91 uses the extended Quake Live layouts.
 */
enum class GameMessageLayout : std::uint8_t {
	Quake3,
	QuakeLive,
	RtcwSinglePlayer,
	RtcwMultiplayer,
	WolfET,
	STVEFHolomatch,
	STVEFSinglePlayer,
	/* Ritual's, not id's. Alice's entityState is 732 bytes of its own fields
	 * and its server opcodes stop at nine, with two Ritual additions and no
	 * end-of-message opcode, so no Quake III field table describes it. No
	 * reader or writer serves this layout yet; the ones that dispatch on it
	 * name it and refuse rather than fall back to Quake III's tables. */
	Alice
};

enum class Capability : std::uint32_t {
	ClientQport = 1u << 0,
	ReliableXor = 1u << 1,
	SequenceChecksum = 1u << 2,
	CompressedConnect = 1u << 3,
	PlatformAuthentication = 1u << 4,
	WorkshopContent = 1u << 5,
	RetailClientsMayJoin = 1u << 6,
	PacketScramble = 1u << 7,
	RawMessages = 1u << 8
};

[[nodiscard]] constexpr std::uint32_t Bit( Capability capability ) noexcept {
	return static_cast<std::uint32_t>( capability );
}

struct Commands {
	std::string_view getChallenge;
	std::string_view challengeResponse;
	std::string_view connect;
	std::string_view connectResponse;
	std::string_view getInfo;
	std::string_view infoResponse;
	std::string_view getStatus;
	std::string_view statusResponse;
	std::string_view disconnect;
	std::string_view print;
	std::string_view echo;
	std::string_view rcon;
	std::string_view download;
	std::string_view nextDownload;
	std::string_view stopDownload;
	std::string_view doneDownload;
	std::string_view pureChecksums;
	std::string_view pureReset;
	std::string_view userinfo;
};

struct InfoKeys {
	std::string_view protocol;
	std::string_view qport;
	std::string_view challenge;
	std::string_view serverId;
	std::string_view paks;
	std::string_view pakNames;
	std::string_view referencedPaks;
	std::string_view referencedPakNames;
	std::string_view fsGame;
	std::string_view systemPure;
};

struct Limits {
	std::size_t packetBytes;
	std::size_t messageBytes;
	std::size_t downloadWindow;
	std::size_t downloadBlockBytes;
	std::size_t reliableCommands;
	std::size_t packetBackup;
};

struct Contract {
	Family family;
	int protocol;
	int demoProtocol;
	netchanWireProfile_t wireProfile;
	GameMessageLayout gameMessageLayout;
	std::uint32_t capabilities;
	Commands commands;
	InfoKeys infoKeys;
	Limits limits;

	[[nodiscard]] constexpr bool Has( Capability capability ) const noexcept {
		return ( capabilities & Bit( capability ) ) != 0u;
	}
};

inline constexpr Commands QuakeCommands{
	"getchallenge", "challengeResponse", "connect", "connectResponse",
	"getinfo", "infoResponse", "getstatus", "statusResponse",
	"disconnect", "print", "echo", "rcon", "download", "nextdl",
	"stopdl", "donedl", "cp", "vdr", "userinfo"
};

inline constexpr InfoKeys QuakeInfoKeys{
	"protocol", "qport", "challenge", "sv_serverid", "sv_paks",
	"sv_pakNames", "sv_referencedPaks", "sv_referencedPakNames",
	"fs_game", "sv_pure"
};

/* Alice's out-of-band vocabulary, read out of retail alice.exe rather than
 * inherited. The connectionless dispatcher at 0x00456250 answers exactly
 * getstatus, getinfo, getchallenge, connect, ipAuthorize, rcon and disconnect;
 * the client's at 0x0042bc90 answers challengeResponse, connectResponse,
 * infoResponse, print, echo, keyAuthorize, motd and getserversResponse.
 * Six Quake III names have no counterpart at all and are left empty
 * rather than guessed:
 *
 *   download, nextdl, stopdl, donedl -- Alice has the svc_download opcode but
 *   no parse arm for it. Its client dispatcher's jump table at 0x0042e278
 *   sends opcode six to the same address as an unknown opcode, the one that
 *   reports an illegible server message, so nothing can arrive over it.
 *
 *   cp, vdr -- the server's client-command table at 0x0051a398 holds two
 *   entries, userinfo and disconnect, and then a null pair. There is no pure
 *   checksum exchange to name, which is the same finding the profile already
 *   records as no pure proof being available for this title. */
inline constexpr Commands AliceCommands{
	"getchallenge", "challengeResponse", "connect", "connectResponse",
	"getinfo", "infoResponse", "getstatus", "statusResponse",
	"disconnect", "print", "echo", "rcon", "", "",
	"", "", "", "", "userinfo"
};

/* The same three keys Quake III reads out of a connect string, and a
 * systeminfo that predates the pak-name triplet: sv_pakNames,
 * sv_referencedPaks and sv_referencedPakNames appear nowhere in the image. */
inline constexpr InfoKeys AliceInfoKeys{
	"protocol", "qport", "challenge", "sv_serverid", "sv_paks",
	"", "", "", "fs_game", "sv_pure"
};

inline constexpr Limits ModernizedQ3Limits{
	1'400u, Netchan_MessageLimit( NETCHAN_WIRE_LEGACY_Q3 ), 48u, 1'024u,
	Netchan_ReliableCommandLimit( NETCHAN_WIRE_LEGACY_Q3 ), 32u
};

// QLSRP and the retail-facing download path identify an eight-block window
// of 2048-byte chunks. Storage may be larger, but the QL sender contract must
// retain these runtime limits.
inline constexpr Limits QuakeLiveLimits{
	1'400u, Netchan_MessageLimit( NETCHAN_WIRE_QL_RETAIL ), 8u, 2'048u,
	Netchan_ReliableCommandLimit( NETCHAN_WIRE_QL_RETAIL ), 32u
};

// Original SP/MP qcommon.h and net_chan.c. These are wire limits;
// engine storage is independently sized to accommodate every active profile.
inline constexpr Limits RtcwLimits{
	1'400u, Netchan_MessageLimit( NETCHAN_WIRE_RTCW_SP ), 8u, 2'048u,
	Netchan_ReliableCommandLimit( NETCHAN_WIRE_RTCW_SP ), 32u
};

inline constexpr Contract LegacyQuake3{
	Family::LegacyQuake3, 68, 68, NETCHAN_WIRE_LEGACY_Q3,
	GameMessageLayout::Quake3,
	Bit( Capability::ClientQport ) | Bit( Capability::ReliableXor ) |
		Bit( Capability::CompressedConnect ),
	QuakeCommands, QuakeInfoKeys, ModernizedQ3Limits
};

inline constexpr Contract Ioquake3{
	Family::Ioquake3, 71, 71, NETCHAN_WIRE_IOQ3,
	GameMessageLayout::Quake3,
	Bit( Capability::ClientQport ) | Bit( Capability::SequenceChecksum ) |
		Bit( Capability::CompressedConnect ),
	QuakeCommands, QuakeInfoKeys, ModernizedQ3Limits
};

// 1.29/1.30 (66) and 1.31 (67) use the same Huffman/delta and reliable-XOR
// framing as 68. Keep identities separate for negotiation and demo recording.
// Original executables and the remaining pure/interop gates are identified in
// docs/fntech3/Q3_POINT_RELEASES.md.
inline constexpr Contract Quake3_66{
	Family::LegacyQuake3, 66, 66, NETCHAN_WIRE_Q3_66,
	GameMessageLayout::Quake3, LegacyQuake3.capabilities,
	QuakeCommands, QuakeInfoKeys, ModernizedQ3Limits
};
inline constexpr Contract Quake3_67{
	Family::LegacyQuake3, 67, 67, NETCHAN_WIRE_Q3_67,
	GameMessageLayout::Quake3, LegacyQuake3.capabilities,
	QuakeCommands, QuakeInfoKeys, ModernizedQ3Limits
};

// Original 1.30/1.31 client transcripts: cp cgame ui @ refs... mixed.
// Protocol 68 added the serverId. Do not disable any checksum validation.
[[nodiscard]] constexpr bool Q3PureChecksumsOmitServerId(
	netchanWireProfile_t profile ) noexcept {
	return profile == NETCHAN_WIRE_Q3_66 || profile == NETCHAN_WIRE_Q3_67;
}

inline constexpr Contract QuakeLive{
	Family::QuakeLive, NETCHAN_QL_RETAIL_PROTOCOL_VERSION,
	NETCHAN_QL_RETAIL_PROTOCOL_VERSION, NETCHAN_WIRE_QL_RETAIL,
	GameMessageLayout::QuakeLive,
	Bit( Capability::ClientQport ) | Bit( Capability::ReliableXor ) |
		Bit( Capability::CompressedConnect ) |
		Bit( Capability::PlatformAuthentication ) |
		Bit( Capability::WorkshopContent ) |
		Bit( Capability::RetailClientsMayJoin ),
	QuakeCommands, QuakeInfoKeys, QuakeLiveLimits
};

inline constexpr Contract RtcwSinglePlayer{
	Family::RtcwSinglePlayer, NETCHAN_RTCW_SP_PROTOCOL_VERSION,
	NETCHAN_RTCW_SP_PROTOCOL_VERSION, NETCHAN_WIRE_RTCW_SP,
	GameMessageLayout::RtcwSinglePlayer,
	Bit( Capability::ClientQport ),
	QuakeCommands, QuakeInfoKeys, RtcwLimits
};

inline constexpr Contract RtcwMultiplayer{
	Family::RtcwMultiplayer, NETCHAN_RTCW_MP_PROTOCOL_VERSION,
	NETCHAN_RTCW_MP_PROTOCOL_VERSION, NETCHAN_WIRE_RTCW_MP,
	GameMessageLayout::RtcwMultiplayer,
	Bit( Capability::ClientQport ) | Bit( Capability::ReliableXor ) |
		Bit( Capability::CompressedConnect ),
	QuakeCommands, QuakeInfoKeys, RtcwLimits
};

inline constexpr Contract WolfET{
	Family::WolfET, NETCHAN_WOLFET_PROTOCOL_VERSION, NETCHAN_WOLFET_PROTOCOL_VERSION,
	NETCHAN_WIRE_WOLFET, GameMessageLayout::WolfET,
	Bit(Capability::ClientQport) | Bit(Capability::ReliableXor) | Bit(Capability::CompressedConnect),
	QuakeCommands, QuakeInfoKeys,
	{1400u, 32768u, 8u, 2048u, 256u, 32u}
};

// Retail stvoyHM.exe 1.20: 16 KB messages, 64 reliable commands, 32 packet
// backups, and 2 KB download blocks in a window of eight
// (SV_WriteDownloadToClient 0x430060), as in the Quake III engine of its era.
inline constexpr Contract STVEFHolomatch{
	Family::STVEFHolomatch, NETCHAN_STVEF_HM_PROTOCOL_VERSION,
	NETCHAN_STVEF_HM_PROTOCOL_VERSION, NETCHAN_WIRE_STVEF_HM,
	GameMessageLayout::STVEFHolomatch,
	Bit( Capability::ClientQport ) | Bit( Capability::PacketScramble ) |
		Bit( Capability::RawMessages ) | Bit( Capability::RetailClientsMayJoin ),
	QuakeCommands, QuakeInfoKeys,
	{ 1'400u, Netchan_MessageLimit( NETCHAN_WIRE_STVEF_HM ), 8u, 2'048u,
		Netchan_ReliableCommandLimit( NETCHAN_WIRE_STVEF_HM ), 32u }
};

/* Retail alice.exe: 48 KB messages, 128 reliable commands of 1,024 bytes and
 * 32 packet backups -- the client's outgoing ring is indexed with `and 0x1f`
 * at 0x00428bba. The download pair is zero because this engine has no
 * download path to size; see AliceCommands above. Alice ships no multiplayer,
 * so no retail client joins this and none is invited to. */
inline constexpr Contract Alice{
	Family::Alice, NETCHAN_ALICE_PROTOCOL_VERSION,
	NETCHAN_ALICE_PROTOCOL_VERSION, NETCHAN_WIRE_ALICE,
	GameMessageLayout::Alice,
	Bit( Capability::ClientQport ) | Bit( Capability::PacketScramble ) |
		Bit( Capability::RawMessages ),
	AliceCommands, AliceInfoKeys,
	{ 1'400u, Netchan_MessageLimit( NETCHAN_WIRE_ALICE ), 0u, 0u,
		Netchan_ReliableCommandLimit( NETCHAN_WIRE_ALICE ), 32u }
};

// Single player never leaves the loopback channel.
inline constexpr Contract STVEFSinglePlayer{
	Family::STVEFSinglePlayer, NETCHAN_STVEF_SP_PROTOCOL_VERSION,
	NETCHAN_STVEF_SP_PROTOCOL_VERSION, NETCHAN_WIRE_STVEF_SP,
	GameMessageLayout::STVEFSinglePlayer,
	Bit( Capability::ClientQport ),
	QuakeCommands, QuakeInfoKeys,
	{ 1'400u, Netchan_MessageLimit( NETCHAN_WIRE_STVEF_SP ), 8u, 1'024u,
		Netchan_ReliableCommandLimit( NETCHAN_WIRE_STVEF_SP ), 32u }
};

[[nodiscard]] constexpr const Contract *Find( int protocol,
	bool legacyCompatibility = false ) noexcept {
	if ( protocol == 66 ) return &Quake3_66;
	if ( protocol == 67 ) return &Quake3_67;
	if ( protocol == STVEFHolomatch.protocol ) return &STVEFHolomatch;
	if ( protocol == STVEFSinglePlayer.protocol ) return &STVEFSinglePlayer;
	if ( protocol == Alice.protocol ) return &Alice;
	if ( protocol == RtcwSinglePlayer.protocol ) return &RtcwSinglePlayer;
	if ( protocol == RtcwMultiplayer.protocol ) return &RtcwMultiplayer;
	if ( protocol == WolfET.protocol ) return &WolfET;
	if ( protocol == QuakeLive.protocol ) {
		return &QuakeLive;
	}
	if ( protocol == LegacyQuake3.protocol ) {
		return &LegacyQuake3;
	}
	if ( protocol == Ioquake3.protocol ) {
		return &Ioquake3;
	}
	if ( legacyCompatibility ) {
		return &LegacyQuake3;
	}
	return nullptr;
}

[[nodiscard]] constexpr const Contract *TryForWireProfile(
	netchanWireProfile_t profile ) noexcept {
	switch ( profile ) {
	case NETCHAN_WIRE_Q3_66:
		return &Quake3_66;
	case NETCHAN_WIRE_Q3_67:
		return &Quake3_67;
	case NETCHAN_WIRE_STVEF_HM:
		return &STVEFHolomatch;
	case NETCHAN_WIRE_STVEF_SP:
		return &STVEFSinglePlayer;
	case NETCHAN_WIRE_ALICE:
		return &Alice;
	case NETCHAN_WIRE_WOLFET:
		return &WolfET;
	case NETCHAN_WIRE_RTCW_SP:
		return &RtcwSinglePlayer;
	case NETCHAN_WIRE_RTCW_MP:
		return &RtcwMultiplayer;
	case NETCHAN_WIRE_QL_RETAIL:
		return &QuakeLive;
	case NETCHAN_WIRE_IOQ3:
		return &Ioquake3;
	case NETCHAN_WIRE_LEGACY_Q3:
		return &LegacyQuake3;
	default:
		return nullptr;
	}
}

[[nodiscard]] constexpr const Contract &ForWireProfile(
	netchanWireProfile_t profile ) noexcept {
	const Contract *contract = TryForWireProfile( profile );
	/* Existing trusted call sites use this reference helper. New parsers must
	 * call TryForWireProfile and reject a null result before consuming bytes. */
	return contract ? *contract : LegacyQuake3;
}

} // namespace fnql::protocol

#endif
