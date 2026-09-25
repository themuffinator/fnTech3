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
 * Heavy Metal: F.A.K.K.2's saveshot comes from the same `screenshot <path>
 * <width> <height>` form (fakk2.exe 1.02, 0x004603a4 -> 0x0045ffa0): the same
 * read and the same four by three samples, but each channel keeps its own sum
 * divided by twelve (0x0046014f..0x00460189), stored blue, green, red as a
 * TGA's bytes are. Its picture is in colour, with no grey step and no gain.
 * Retail then puts it through its hardware gamma table when overbright bits
 * are on (0x004601c8..0x004601e7), which R_SetColorMappings allows only in a
 * fullscreen mode with hardware gamma (0x0046ff7f..0x0046ffa2); the renderers
 * leave that step to their screenshots' own gamma.
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

/* The twelve samples under one output pixel, each channel summed apart. */
static FNTECH3_ALICE_SAVESHOT_INLINE void FNTECH3_SaveshotSums( const unsigned char *source,
	int width, int height, int stride, float xs, float ys, int ox, int oy, int sums[3] )
{
	int r, c;

	sums[0] = sums[1] = sums[2] = 0;
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
			sums[0] += from[0];
			sums[1] += from[1];
			sums[2] += from[2];
		}
	}
}

static FNTECH3_ALICE_SAVESHOT_INLINE int FNTECH3_SaveshotArgumentsValid( const unsigned char *source,
	int width, int height, int stride, const unsigned char *out, int outWidth, int outHeight )
{
	return source && out && width > 0 && height > 0 && outWidth > 0 && outHeight > 0
		&& stride >= width * 3;
}

static FNTECH3_ALICE_SAVESHOT_INLINE void FNTECH3_AliceSaveshot( const unsigned char *source,
	int width, int height, int stride, unsigned char *out, int outWidth, int outHeight )
{
	const float xs = (float)( (double)width / (double)( FNTECH3_ALICE_SAVESHOT_COLUMNS * outWidth ) );
	const float ys = (float)( (double)height / (double)( FNTECH3_ALICE_SAVESHOT_ROWS * outHeight ) );
	int ox, oy;

	if ( !FNTECH3_SaveshotArgumentsValid( source, width, height, stride, out, outWidth, outHeight ) ) {
		return;
	}

	for ( oy = 0; oy < outHeight; oy++ ) {
		for ( ox = 0; ox < outWidth; ox++ ) {
			int sums[3], red, green, blue, value;
			double scaled;
			unsigned char *pixel;

			FNTECH3_SaveshotSums( source, width, height, stride, xs, ys, ox, oy, sums );
			red = sums[0];
			green = sums[1];
			blue = sums[2];

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

/* F.A.K.K.2's colour reduction (see above): blue, green, red, bottom row first. */
static FNTECH3_ALICE_SAVESHOT_INLINE void FNTECH3_Fakk2Saveshot( const unsigned char *source,
	int width, int height, int stride, unsigned char *out, int outWidth, int outHeight )
{
	const float xs = (float)( (double)width / (double)( FNTECH3_ALICE_SAVESHOT_COLUMNS * outWidth ) );
	const float ys = (float)( (double)height / (double)( FNTECH3_ALICE_SAVESHOT_ROWS * outHeight ) );
	const int samples = FNTECH3_ALICE_SAVESHOT_COLUMNS * FNTECH3_ALICE_SAVESHOT_ROWS;
	int ox, oy;

	if ( !FNTECH3_SaveshotArgumentsValid( source, width, height, stride, out, outWidth, outHeight ) ) {
		return;
	}

	for ( oy = 0; oy < outHeight; oy++ ) {
		for ( ox = 0; ox < outWidth; ox++ ) {
			int sums[3];
			unsigned char *pixel = out + ( (size_t)oy * (size_t)outWidth + (size_t)ox ) * 3u;

			FNTECH3_SaveshotSums( source, width, height, stride, xs, ys, ox, oy, sums );
			pixel[0] = (unsigned char)( sums[2] / samples );
			pixel[1] = (unsigned char)( sums[1] / samples );
			pixel[2] = (unsigned char)( sums[0] / samples );
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
