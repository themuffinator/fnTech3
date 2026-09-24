/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_PAK_CONTRACT_H
#define FNTECH3_Q3_PAK_CONTRACT_H

/* Final stock update pak for the selected, implemented live Q3 contract.
 * Verified against archived id 1.29f/1.30 (pak6), 1.31 (pak7), and 1.32
 * (pak8). Earlier contracts are not enabled by this filesystem policy. */
static inline unsigned Com_Q3RequiredPakMask( int protocol ) {
	if ( protocol == 66 ) return 0x07fu;
	if ( protocol == 67 ) return 0x0ffu;
	return 0x1ffu;
}

/* Official 1.29f/g and 1.29h pak6 central-directory CRC checksums. Their
 * modules differ from 1.30's pak6 even though all three speak protocol 66.
 * These exceptions never relax another pak number or protocol's policy. */
static inline int Com_Q3PointPakChecksum( int protocol, int pak, unsigned checksum ) {
	return protocol == 66 && pak == 6 &&
		( checksum == 1913835400u || checksum == 1324612836u );
}

#endif
