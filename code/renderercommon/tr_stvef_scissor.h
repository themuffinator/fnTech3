/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_STVEF_SCISSOR_H
#define FNTECH3_TR_STVEF_SCISSOR_H

#include <math.h>

typedef struct {
	int x, y, width, height; /* top-left framebuffer coordinates */
} fnScissorRect_t;

/* Retail stvoy.exe 0x45175e: negative x restores the whole framebuffer;
 * otherwise truncate x, (videoHeight-y-h), w and h to integers, in that
 * order of coordinate conversion. Clip before any narrowing conversion so
 * malformed module values cannot overflow a driver rectangle. */
static int FN_STVEFScissorRect( float x, float y, float w, float h,
	int videoWidth, int videoHeight, fnScissorRect_t *out ) {
	double left, bottom, right, top;
	if ( !out || videoWidth < 0 || videoHeight < 0 ||
		!isfinite( x ) || !isfinite( y ) || !isfinite( w ) || !isfinite( h ) ) return 0;
	if ( x < 0 ) {
		out->x = out->y = 0;
		out->width = videoWidth;
		out->height = videoHeight;
		return 1;
	}
	if ( w < 0 || h < 0 ) return 0;
	left = trunc( (double)x );
	bottom = trunc( (double)videoHeight - y - h );
	right = left + trunc( (double)w );
	top = bottom + trunc( (double)h );
	left = fmin( videoWidth, fmax( 0.0, left ) );
	right = fmin( videoWidth, fmax( left, right ) );
	bottom = fmin( videoHeight, fmax( 0.0, bottom ) );
	top = fmin( videoHeight, fmax( bottom, top ) );
	out->x = (int)left;
	out->y = videoHeight - (int)top;
	out->width = (int)right - (int)left;
	out->height = (int)top - (int)bottom;
	return 1;
}

typedef struct {
	int commandId;
	fnScissorRect_t rect;
} stvefScissorCommand_t;

#endif
