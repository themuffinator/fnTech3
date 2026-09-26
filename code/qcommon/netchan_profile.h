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
#define NETCHAN_STVEF_HM_PROTOCOL_VERSION 24
#define NETCHAN_IOEF_PROTOCOL_VERSION 26
#define NETCHAN_STVEF_SP_PROTOCOL_VERSION 40
#define NETCHAN_Q3_IHV_PROTOCOL_VERSION 33
/* American McGee's Alice announces protocol 1. Retail alice.exe reads it
 * from the connect string's "protocol" key like every engine of this
 * lineage; the number is small because Ritual started its own count. */
#define NETCHAN_ALICE_PROTOCOL_VERSION 1
/* Medal of Honor: Allied Assault 1.00 speaks protocol 6 (OpenMoHAA: "MoHAA
 * v 1.00 uses protocol version 6"); 1.11 is 8 and the expansions 15 and up. */
#define NETCHAN_MOHAA_PROTOCOL_VERSION 6

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
	NETCHAN_WIRE_WOLFET,
	NETCHAN_WIRE_STVEF_HM,
	NETCHAN_WIRE_STVEF_SP,
	NETCHAN_WIRE_IOEF,
	NETCHAN_WIRE_Q3_66,
	NETCHAN_WIRE_Q3_67,
	/* February 1999: Quake II's two sequence words and single-slot reliable
	 * retransmission, Quake III's fragmenting, and a scrambled payload.
	 * q3_ihv_netchan.hpp implements it; no other profile shares it. */
	NETCHAN_WIRE_Q3_IHV,
	/* November 2000: Ritual's F.A.K.K.2 engine as American McGee's Alice
	 * ships it. Quake III's netchan_t and fragmenting -- retail 0x00447080
	 * writes the sequence, the client's qport, the payload and nothing
	 * else, but a fragment's offset as a long -- over plain LSB-first bits
	 * and the same per-packet scramble Holomatch uses. It carries its own
	 * message and reliable limits, which are larger than Quake III's. */
	NETCHAN_WIRE_ALICE,
	/* December 2001: 2015's descendant of Quake III's netchan as Medal of
	 * Honor: Allied Assault 1.00 ships it. Quake III 1.32's framing -- the
	 * client's qport, the reliable-command XOR, Huffman-coded messages -- with
	 * a fragment's offset written as a long, and its own opcodes past
	 * svc_snapshot: centre print, location print and client-game messages
	 * before the end-of-message byte, which is therefore 11, not 8. Its
	 * out-of-band packets carry a direction byte after the four 0xff bytes.
	 * The framing is OpenMoHAA's reading of the engine (code/qcommon/
	 * net_chan.c, code/server/sv_net_chan.c); the retail executable could not
	 * be read. */
	NETCHAN_WIRE_MOHAA,
	/* 1999: the public Q3Test releases, one profile over two channels that
	 * are chosen by the pinned release, never by protocol number (40 is also
	 * Elite Force single player's). 1.02 to 1.05 (protocols 35 and 36) keep
	 * the IHV channel -- two sequence words with reliable bits, one reliable
	 * block in flight, a word-wise key stream; 1.06 to 1.08 (38 to 40) send
	 * Quake III's shape with a per-byte scrambler and every slice of a message
	 * under one sequence. Neither has Huffman coding. Q3Test's own sessions
	 * carry both (code/compat/q3test_netchan.hpp,
	 * code/server/server_q3test_net.hpp). */
	NETCHAN_WIRE_Q3TEST_A,
	NETCHAN_WIRE_Q3TEST_B
} netchanWireProfile_t;

typedef enum {
	NETCHAN_FEATURE_CLIENT_QPORT = 1u << 0,
	NETCHAN_FEATURE_SEQUENCE_CHECKSUM = 1u << 1,
	NETCHAN_FEATURE_RELIABLE_XOR = 1u << 2,
	/* Elite Force Holomatch scrambles every sequenced packet after byte six
	 * with the pre-1.27 transposition routine, and writes game messages as
	 * plain LSB-first bits with no Huffman stage. */
	NETCHAN_FEATURE_PACKET_SCRAMBLE = 1u << 3,
	NETCHAN_FEATURE_RAW_MESSAGES = 1u << 4,
	/* Two sequence words carrying reliable bits, one reliable slot the peer
	 * acknowledges, and a key stream seeded from the first word. */
	NETCHAN_FEATURE_SEQUENCE_PAIR = 1u << 5,
	NETCHAN_FEATURE_RELIABLE_SLOT = 1u << 6,
	NETCHAN_FEATURE_PAYLOAD_KEYSTREAM = 1u << 7,
	/* A fragment's offset is a long, not Quake III's short, so the later
	 * fragments of a message past 32 KB keep their places. Retail alice.exe
	 * writes it with its 32-bit writer (0x0044710e) and the length with its
	 * 16-bit one (0x00447119), and reads it back with its 32-bit reader
	 * (0x00447423). */
	NETCHAN_FEATURE_LONG_FRAGMENT_START = 1u << 8
} netchanWireFeature_t;

#ifdef __cplusplus
#define NETCHAN_PROFILE_INLINE inline constexpr
#else
#define NETCHAN_PROFILE_INLINE static inline
#endif

NETCHAN_PROFILE_INLINE netchanWireProfile_t Netchan_SelectWireProfile(
	int protocol, int legacyCompat ) {
	if ( protocol == 66 ) return NETCHAN_WIRE_Q3_66;
	if ( protocol == 67 ) return NETCHAN_WIRE_Q3_67;
	if ( protocol == NETCHAN_RTCW_SP_PROTOCOL_VERSION ) return NETCHAN_WIRE_RTCW_SP;
	if ( protocol == NETCHAN_RTCW_MP_PROTOCOL_VERSION ) return NETCHAN_WIRE_RTCW_MP;
	if ( protocol == NETCHAN_WOLFET_PROTOCOL_VERSION ) return NETCHAN_WIRE_WOLFET;
	if ( protocol == NETCHAN_STVEF_HM_PROTOCOL_VERSION ) return NETCHAN_WIRE_STVEF_HM;
	if ( protocol == NETCHAN_IOEF_PROTOCOL_VERSION ) return NETCHAN_WIRE_IOEF;
	if ( protocol == NETCHAN_STVEF_SP_PROTOCOL_VERSION ) return NETCHAN_WIRE_STVEF_SP;
	if ( protocol == NETCHAN_QL_RETAIL_PROTOCOL_VERSION ) {
		return NETCHAN_WIRE_QL_RETAIL;
	}
	if ( protocol == NETCHAN_Q3_IHV_PROTOCOL_VERSION ) {
		return NETCHAN_WIRE_Q3_IHV;
	}
	if ( protocol == NETCHAN_ALICE_PROTOCOL_VERSION ) {
		return NETCHAN_WIRE_ALICE;
	}
	if ( protocol == NETCHAN_MOHAA_PROTOCOL_VERSION ) {
		return NETCHAN_WIRE_MOHAA;
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
	case NETCHAN_WIRE_IOEF:
		return NETCHAN_FEATURE_CLIENT_QPORT |
			NETCHAN_FEATURE_SEQUENCE_CHECKSUM;
	case NETCHAN_WIRE_STVEF_HM:
		// Retail stvoyHM.exe: qport, no checksum, no reliable XOR.
		return NETCHAN_FEATURE_CLIENT_QPORT |
			NETCHAN_FEATURE_PACKET_SCRAMBLE |
			NETCHAN_FEATURE_RAW_MESSAGES;
	case NETCHAN_WIRE_STVEF_SP:
		// Loopback only: no peer ever sees these bytes.
		return NETCHAN_FEATURE_CLIENT_QPORT;
	case NETCHAN_WIRE_Q3_IHV:
	case NETCHAN_WIRE_Q3TEST_A:
		return NETCHAN_FEATURE_CLIENT_QPORT |
			NETCHAN_FEATURE_RAW_MESSAGES |
			NETCHAN_FEATURE_SEQUENCE_PAIR |
			NETCHAN_FEATURE_RELIABLE_SLOT |
			NETCHAN_FEATURE_PAYLOAD_KEYSTREAM;
	case NETCHAN_WIRE_Q3TEST_B:
		return NETCHAN_FEATURE_CLIENT_QPORT |
			NETCHAN_FEATURE_RAW_MESSAGES |
			NETCHAN_FEATURE_SEQUENCE_PAIR |
			NETCHAN_FEATURE_PACKET_SCRAMBLE;
	case NETCHAN_WIRE_ALICE:
		/* Retail alice.exe 0x00447204 writes net_qport only when the socket
		 * is the client's, and 0x0044740e reads it back only when it is the
		 * server's; there is no checksum and no reliable XOR between them.
		 * 0x004472a0 scrambles and 0x004476f0 restores, and the bit writer
		 * at 0x00445520 has no Huffman stage -- its msg_t is twenty-eight
		 * bytes, without the oob flag a Huffman path would need. */
		return NETCHAN_FEATURE_CLIENT_QPORT |
			NETCHAN_FEATURE_PACKET_SCRAMBLE |
			NETCHAN_FEATURE_RAW_MESSAGES |
			NETCHAN_FEATURE_LONG_FRAGMENT_START;
	case NETCHAN_WIRE_MOHAA:
		return NETCHAN_FEATURE_CLIENT_QPORT |
			NETCHAN_FEATURE_RELIABLE_XOR |
			NETCHAN_FEATURE_LONG_FRAGMENT_START;
	case NETCHAN_WIRE_QL_RETAIL:
	case NETCHAN_WIRE_RTCW_MP:
	case NETCHAN_WIRE_WOLFET:
	case NETCHAN_WIRE_LEGACY_Q3:
	case NETCHAN_WIRE_Q3_66:
	case NETCHAN_WIRE_Q3_67:
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
	case NETCHAN_WIRE_Q3_66:
	case NETCHAN_WIRE_Q3_67:
	case NETCHAN_WIRE_IOQ3:
	case NETCHAN_WIRE_QL_RETAIL:
	case NETCHAN_WIRE_STVEF_HM:
	case NETCHAN_WIRE_IOEF:
	case NETCHAN_WIRE_STVEF_SP: return 64;
	case NETCHAN_WIRE_RTCW_SP:
	case NETCHAN_WIRE_RTCW_MP:
	case NETCHAN_WIRE_WOLFET: return 256;
	/* Retail alice.exe rings 128 commands of 1,024 bytes: the client's
	 * adder at 0x0042a220 errors past 128 outstanding and indexes with
	 * `and 0x7f`, and the server's at 0x00455a10 does the same. */
	case NETCHAN_WIRE_ALICE: return 128;
	/* OpenMoHAA rings 512; this engine's storage holds 256, which only
	 * changes when a client with that many unacknowledged commands is
	 * dropped, not which command a sequence number keys. */
	case NETCHAN_WIRE_MOHAA: return 256;
	/* IHV acknowledges one reliable block at a time, not a command ring.
	 * Q3Test from 1.06 rings 64 (1.06 SV_AddServerCommand 0x41d3b6,
	 * CL_AddReliableCommand 0x4052a0); up to 1.05 it sends one reliable block
	 * as IHV does, and this engine stages its commands in a ring of the same
	 * size before they join the block. */
	case NETCHAN_WIRE_Q3_IHV: return 0;
	case NETCHAN_WIRE_Q3TEST_A:
	case NETCHAN_WIRE_Q3TEST_B: return 64;
	default: return 0;
	}
}

NETCHAN_PROFILE_INLINE int Netchan_MessageLimit( netchanWireProfile_t profile ) {
	switch ( profile ) {
	case NETCHAN_WIRE_LEGACY_Q3:
	case NETCHAN_WIRE_Q3_66:
	case NETCHAN_WIRE_Q3_67:
	case NETCHAN_WIRE_IOQ3:
	case NETCHAN_WIRE_QL_RETAIL:
	case NETCHAN_WIRE_STVEF_HM:
	case NETCHAN_WIRE_IOEF:
	case NETCHAN_WIRE_STVEF_SP: return 16384;
	case NETCHAN_WIRE_RTCW_SP:
	case NETCHAN_WIRE_RTCW_MP:
	case NETCHAN_WIRE_WOLFET: return 32768;
	/* Three times Quake III's. Every retail alice.exe call site that
	 * starts a game message gives MSG_Init 0xc000 bytes, in a frame sized
	 * 0xc030 -- the buffer plus its locals, with no slack. */
	case NETCHAN_WIRE_ALICE: return 49152;
	/* OpenMoHAA's MAX_MSGLEN, as Alice's. */
	case NETCHAN_WIRE_MOHAA: return 49152;
	case NETCHAN_WIRE_Q3_IHV:
	case NETCHAN_WIRE_Q3TEST_A: return 16368;
	/* 1.06 reassembles 16384 bytes (Netchan_Process 0x415c51). */
	case NETCHAN_WIRE_Q3TEST_B: return 16384;
	default: return 0;
	}
}

/* The byte that ends a server message: Quake III's svc_EOF, except where a
 * profile numbers opcodes of its own before it. */
NETCHAN_PROFILE_INLINE int Netchan_ServerEOFOpcode( netchanWireProfile_t profile ) {
	return profile == NETCHAN_WIRE_MOHAA ? 11 : 8;
}

/* Medal of Honor's out-of-band direction byte: 1 from a server, 2 from a
 * client (OpenMoHAA NET_OutOfBandPrint), or 0 where a profile has none. */
NETCHAN_PROFILE_INLINE int Netchan_OutOfBandDirectionByte( netchanWireProfile_t profile, int fromServer ) {
	return profile == NETCHAN_WIRE_MOHAA ? ( fromServer ? 1 : 2 ) : 0;
}

#undef NETCHAN_PROFILE_INLINE

#endif
