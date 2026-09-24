/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_SAVESHOT_H
#define FNTECH3_COMPAT_ALICE_SAVESHOT_H

/*
 * American McGee's Alice's saveshot: the picture its savegames keep, made
 * from the frame the engine has just drawn. Retail alice.exe's renderer does
 * it in the routine its `screenshot <path> <width> <height>` form reaches
 * (0x00474000, called from 0x004746b0..0x00474759):
 *
 *   - the whole frame is read as RGB, the bottom row first (glReadPixels);
 *   - each output pixel sums four samples across and three down, at
 *     x = (int)((4 * ox + c) * xs) and y = (int)((3 * oy + r) * ys), where the
 *     steps are floats: xs = width / (4 * outWidth), ys = height / (3 * outHeight);
 *   - it keeps the largest of the three channel sums -- red when red beats
 *     both, else blue when blue beats both, else green -- divided by twelve,
 *     in all three bytes: the picture is grey;
 *   - then every byte is multiplied by 2.55f on the x87 stack and clamped
 *     at 255 (0x00474288). The product is compared and truncated at x87
 *     precision, not rounded to float first, so 100 gives 254 and not 255;
 *     double arithmetic reproduces that exactly.
 *
 * The output rows keep the frame's order, the bottom row first, which is the
 * order of an uncompressed TGA whose descriptor byte is zero.
 *
 * `stride` is the length of one source row in bytes. Retail indexes its read
 * as tightly packed (vidWidth * 3), which shears the picture on a frame whose
 * rows the driver pads; this honours the real stride instead.
 *
 * This header is C as well as C++: each renderer's tr_init.c includes it
 * after tr_local.h, and code/compat/alice_savegame.hpp wraps it for the rest
 * of the engine and the tests.
 */

#if defined( __cplusplus )
#define FNTECH3_ALICE_SAVESHOT_INLINE inline
#elif defined( _MSC_VER )
#define FNTECH3_ALICE_SAVESHOT_INLINE __inline
#else
#define FNTECH3_ALICE_SAVESHOT_INLINE inline
#endif

enum {
	FNTECH3_ALICE_SAVESHOT_SIDE = 256,        /* "saveshot %s 256 256" 0x0051a18c */
	FNTECH3_ALICE_SAVESHOT_COLUMNS = 4,
	FNTECH3_ALICE_SAVESHOT_ROWS = 3,
	FNTECH3_ALICE_TGA_HEADER_BYTES = 18
};

/* The float at 0x005002d0: 2.54999995231628..., 2.55 rounded to float. */
#define FNTECH3_ALICE_SAVESHOT_GAIN 2.55f

static FNTECH3_ALICE_SAVESHOT_INLINE void FNTECH3_AliceSaveshot( const unsigned char *source,
	int width, int height, int stride, unsigned char *out, int outWidth, int outHeight )
{
	const float xs = (float)( (double)width / (double)( FNTECH3_ALICE_SAVESHOT_COLUMNS * outWidth ) );
	const float ys = (float)( (double)height / (double)( FNTECH3_ALICE_SAVESHOT_ROWS * outHeight ) );
	int ox, oy, r, c;

	if ( !source || !out || width <= 0 || height <= 0 || outWidth <= 0 || outHeight <= 0
		|| stride < width * 3 ) {
		return;
	}

	for ( oy = 0; oy < outHeight; oy++ ) {
		for ( ox = 0; ox < outWidth; ox++ ) {
			int red = 0, green = 0, blue = 0, value;
			double scaled;
			unsigned char *pixel;

			for ( r = 0; r < FNTECH3_ALICE_SAVESHOT_ROWS; r++ ) {
				int sy = (int)( (double)( oy * FNTECH3_ALICE_SAVESHOT_ROWS + r ) * (double)ys );
				if ( sy >= height ) {
					sy = height - 1;
				}
				for ( c = 0; c < FNTECH3_ALICE_SAVESHOT_COLUMNS; c++ ) {
					const unsigned char *from;
					int sx = (int)( (double)( ox * FNTECH3_ALICE_SAVESHOT_COLUMNS + c ) * (double)xs );
					if ( sx >= width ) {
						sx = width - 1;
					}
					from = source + (size_t)sy * (size_t)stride + (size_t)sx * 3u;
					red += from[0];
					green += from[1];
					blue += from[2];
				}
			}

			if ( red > blue && red > green ) {
				value = red;
			} else if ( blue > red && blue > green ) {
				value = blue;
			} else {
				value = green;
			}
			value /= FNTECH3_ALICE_SAVESHOT_COLUMNS * FNTECH3_ALICE_SAVESHOT_ROWS;

			scaled = (double)value * (double)FNTECH3_ALICE_SAVESHOT_GAIN;
			value = scaled > 255.0 ? 255 : (int)scaled;

			pixel = out + ( (size_t)oy * (size_t)outWidth + (size_t)ox ) * 3u;
			pixel[0] = pixel[1] = pixel[2] = (unsigned char)value;
		}
	}
}

/* The 18 bytes retail writes in front of it: uncompressed true colour, the
 * size, 24 bits and a zero descriptor (bottom-left origin); every other byte
 * zero (0x00474050..0x00474082). */
static FNTECH3_ALICE_SAVESHOT_INLINE void FNTECH3_AliceTgaHeader( unsigned char *header,
	int width, int height )
{
	int i;

	for ( i = 0; i < FNTECH3_ALICE_TGA_HEADER_BYTES; i++ ) {
		header[i] = 0;
	}
	header[2] = 2;
	header[12] = (unsigned char)( width & 0xff );
	header[13] = (unsigned char)( ( width >> 8 ) & 0xff );
	header[14] = (unsigned char)( height & 0xff );
	header[15] = (unsigned char)( ( height >> 8 ) & 0xff );
	header[16] = 24;
}

#endif
