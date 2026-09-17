/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_MARKS_H
#define FNTECH3_RTCW_MARKS_H
#include <stdint.h>
#include <string.h>

static inline int FnRTCWMarkFinite( float value ) {
	uint32_t bits;
	memcpy( &bits, &value, sizeof( bits ) );
	return (bits & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000);
}

/* Engine-owned result. The client marshals the external XYZST contract.
 * A negative fragment count means this point's texture coordinates are valid. */
typedef struct {
	float xyz[3];
	float st[2];
} fnRTCWMarkPoint_t;

#endif
