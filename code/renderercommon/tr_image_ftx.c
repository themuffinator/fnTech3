/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

FTX images

American McGee's Alice keeps nearly all of its textures as Ritual's FTX: a
twelve-byte header -- width, height and a flag word -- followed by the image
as uncompressed RGBA, one row after another, with no palette, padding or
compression. The retail loader at 0x00483e80 reads the header with one
twelve-byte read, hands the first word back as the width and the second as
the height, allocates width * height * 4 bytes and reads the pixels with one
more read. All 5,893 FTX files the installation carries are exactly that
size.

The third word is handed back to the caller and not otherwise used by the
loader; it is not needed to decode the pixels and is ignored here.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"

#include <limits.h>

#define FTX_HEADER_BYTES 12
/* No retail FTX is wider or taller than this; a larger header is a damaged
 * file, and refusing it keeps width * height * 4 far from overflow. */
#define FTX_MAX_DIMENSION 8192

void R_LoadFTX( const char *name, byte **pic, int *width, int *height )
{
	union {
		byte *b;
		void *v;
	} buffer;
	int length;
	int w, h;
	size_t pixels;

	*pic = NULL;
	if ( width ) {
		*width = 0;
	}
	if ( height ) {
		*height = 0;
	}

	length = ri.FS_ReadFile( name, &buffer.v );
	if ( !buffer.b || length < 0 ) {
		return;
	}
	if ( length < FTX_HEADER_BYTES ) {
		ri.Printf( PRINT_WARNING, "R_LoadFTX: %s has a truncated header\n", name );
		ri.FS_FreeFile( buffer.v );
		return;
	}

	w = (int)( buffer.b[0] | ( buffer.b[1] << 8 ) | ( buffer.b[2] << 16 ) | ( (unsigned)buffer.b[3] << 24 ) );
	h = (int)( buffer.b[4] | ( buffer.b[5] << 8 ) | ( buffer.b[6] << 16 ) | ( (unsigned)buffer.b[7] << 24 ) );
	if ( w <= 0 || h <= 0 || w > FTX_MAX_DIMENSION || h > FTX_MAX_DIMENSION ) {
		ri.Printf( PRINT_WARNING, "R_LoadFTX: %s has an invalid size %ix%i\n", name, w, h );
		ri.FS_FreeFile( buffer.v );
		return;
	}

	pixels = (size_t)w * (size_t)h * 4;
	if ( (size_t)( length - FTX_HEADER_BYTES ) < pixels ) {
		ri.Printf( PRINT_WARNING, "R_LoadFTX: %s is shorter than its %ix%i image\n", name, w, h );
		ri.FS_FreeFile( buffer.v );
		return;
	}

	*pic = (byte *)ri.Malloc( (int)pixels );
	Com_Memcpy( *pic, buffer.b + FTX_HEADER_BYTES, pixels );
	if ( width ) {
		*width = w;
	}
	if ( height ) {
		*height = h;
	}
	ri.FS_FreeFile( buffer.v );
}
