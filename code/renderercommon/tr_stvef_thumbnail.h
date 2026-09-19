/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_STVEF_THUMBNAIL_H
#define FNTECH3_TR_STVEF_THUMBNAIL_H

/*
 * The picture Elite Force single player keeps of the screen, which its
 * savegames hold and its loading screen draws (stvoy.exe 0x44fd80). Each of
 * the result's pixels averages twelve of the screen's, four across by three
 * down, taken at x * width / (4 side) and y * height / (3 side); the result
 * keeps the screen's row order, the bottom row first, and is opaque.
 *
 * The source is the screen read as RGB rows of width pixels, each padded to
 * padlen extra bytes, the bottom row first. Each renderer's tr_init.c
 * includes this after tr_local.h.
 */

#define STVEF_THUMBNAIL_COLUMNS 4
#define STVEF_THUMBNAIL_ROWS 3

static ID_INLINE void R_STVEFThumbnail( const byte *source, int width, int height, int padlen,
	byte *rgba, int side )
{
	const int stride = width * 3 + padlen;
	const float xStep = (float)( (double)width / ( (double)side * STVEF_THUMBNAIL_COLUMNS ) );
	const float yStep = (float)( (double)height / ( (double)side * STVEF_THUMBNAIL_ROWS ) );
	int x, y, i, j;

	if ( !source || !rgba || width <= 0 || height <= 0 || side <= 0 ) {
		return;
	}

	for ( y = 0; y < side; y++ ) {
		for ( x = 0; x < side; x++ ) {
			int sum[3];
			byte *pixel;

			sum[0] = sum[1] = sum[2] = 0;
			for ( i = 0; i < STVEF_THUMBNAIL_ROWS; i++ ) {
				int row = (int)( (double)( y * STVEF_THUMBNAIL_ROWS + i ) * (double)yStep );
				if ( row >= height ) {
					row = height - 1;
				}
				for ( j = 0; j < STVEF_THUMBNAIL_COLUMNS; j++ ) {
					int column = (int)( (double)( x * STVEF_THUMBNAIL_COLUMNS + j ) * (double)xStep );
					const byte *from;

					if ( column >= width ) {
						column = width - 1;
					}
					from = source + (size_t)row * (size_t)stride + (size_t)column * 3u;
					sum[0] += from[0];
					sum[1] += from[1];
					sum[2] += from[2];
				}
			}
			pixel = rgba + ( (size_t)y * (size_t)side + (size_t)x ) * 4u;
			pixel[0] = (byte)( sum[0] / ( STVEF_THUMBNAIL_COLUMNS * STVEF_THUMBNAIL_ROWS ) );
			pixel[1] = (byte)( sum[1] / ( STVEF_THUMBNAIL_COLUMNS * STVEF_THUMBNAIL_ROWS ) );
			pixel[2] = (byte)( sum[2] / ( STVEF_THUMBNAIL_COLUMNS * STVEF_THUMBNAIL_ROWS ) );
			pixel[3] = 255;
		}
	}
}

#endif
