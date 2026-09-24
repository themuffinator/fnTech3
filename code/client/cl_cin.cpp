/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/*****************************************************************************
 * name:		cl_cin.cpp
 *
 * desc:		video and cinematic playback
 *
 * $Archive: /MissionPack/code/client/cl_cin.c $
 *
 * cl_glconfig.hwtype trtypes 3dfx/ragepro need 256x256
 *
 *****************************************************************************/

extern "C" {
#include "client.h"
#include "audio/snd_local.h"
}

#include "client_cpp.h"
#include "client_alice_cinematic.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>

using fnql::FileRead;
using fnql::OpenFileRead;
using fnql::ScopedFileHandle;
using fnql::ScopedTempMemory;
using fnql::ToQboolean;

namespace {

constexpr int MAXSIZE = 8;
constexpr int MINSIZE = 4;

constexpr int DEFAULT_CIN_WIDTH = 512;
constexpr int DEFAULT_CIN_HEIGHT = 512;

// Original RTCW SP's movie presentation contract; MP/Q3/QL do not use bit 0x20.
constexpr int RTCW_SP_CIN_LETTERBOX = 0x20;
constexpr int RTCW_SP_CIN_BAR_HEIGHT = 105;

[[maybe_unused]] constexpr int ROQ_QUAD = 0x1000;
constexpr int ROQ_QUAD_INFO = 0x1001;
constexpr int ROQ_CODEBOOK = 0x1002;
constexpr int ROQ_QUAD_VQ = 0x1011;
constexpr int ROQ_QUAD_JPEG = 0x1012;
constexpr int ROQ_QUAD_HANG = 0x1013;
constexpr int ROQ_PACKET = 0x1030;
constexpr int ZA_SOUND_MONO = 0x1020;
constexpr int ZA_SOUND_STEREO = 0x1021;

template <typename T>
static byte *AsWritableBytes( T *ptr )
{
	return reinterpret_cast<byte *>( ptr );
}

static void CopyBytes( const byte *src, byte *dst, std::size_t count )
{
	std::copy_n( src, count, dst );
}

static void MoveBytes( const byte *src, byte *dst, std::size_t count )
{
	std::memmove( dst, src, count );
}

} // namespace

extern "C" {
extern	int		s_paintedtime;
extern	int		s_rawend;
}


static qboolean RoQ_init( void );
static void CIN_SetLooping (int handle, qboolean loop);
static void CL_AliceCinematicEnd( void );

static const char *CL_CinematicBasename( const char *path )
{
	const char *slash;
	const char *backslash;

	if ( !path ) {
		return "";
	}

	slash = strrchr( path, '/' );
	backslash = strrchr( path, '\\' );

	if ( slash && ( !backslash || slash > backslash ) ) {
		return slash + 1;
	}
	if ( backslash ) {
		return backslash + 1;
	}

	return path;
}

static qboolean CL_ShouldSkipIdLogoCinematic( const char *arg )
{
	const char *base;
	const char *dot;
	size_t len;

	if ( !Cvar_VariableIntegerValue( "com_skipIdLogo" ) ) {
		return qfalse;
	}

	base = CL_CinematicBasename( arg );
	dot = strrchr( base, '.' );
	len = dot ? (size_t)( dot - base ) : strlen( base );

	if ( len != 6 || Q_stricmpn( base, "idlogo", 6 ) != 0 ) {
		return qfalse;
	}

	if ( dot && Q_stricmp( dot, ".roq" ) != 0 ) {
		return qfalse;
	}

	return qtrue;
}

static void CL_RunCinematicNextmap( void )
{
	const char *s;

	// Alice's end prefixes `map`, stops at a leading ';' and resumes its
	// engine UI's intro (CIN_Finish 0x004257a0).
	if ( Com_GameIsAlice() ) {
		CL_AliceCinematicEnd();
		return;
	}

	s = Cvar_VariableString( "nextmap" );
	if ( s[0] ) {
		Cbuf_ExecuteText( EXEC_APPEND, va("%s\n", s) );
		Cvar_Set( "nextmap", "" );
	}
}

/******************************************************************************
*
* Class:		trFMV
*
* Description:	RoQ/RnR manipulation routines
*				not entirely complete for first run
*
******************************************************************************/

static std::array<long, 256> ROQ_YY_tab;
static std::array<long, 256> ROQ_UB_tab;
static std::array<long, 256> ROQ_UG_tab;
static std::array<long, 256> ROQ_VG_tab;
static std::array<long, 256> ROQ_VR_tab;
static std::array<unsigned short, 256 * 16 * 4> vq2;
static std::array<unsigned short, 256 * 64 * 4> vq4;
static std::array<unsigned short, 256 * 256 * 4> vq8;


struct cinematics_t {
	std::array<byte, DEFAULT_CIN_WIDTH * DEFAULT_CIN_HEIGHT * 4 * 2> linbuf;
	// A legal RoQ chunk may be a full 65536 bytes. Keep header slack for
	// the initial file header; subsequent chunks are read without lookahead.
	std::array<byte, 65536 + 8> file;
	std::array<short, 256> sqrTable;

	std::array<int, 256> mcomp;
	byte				*qStatus[2][32768];

	long				oldXOff, oldYOff;
	unsigned long		oldysize, oldxsize;

	int					currentHandle;
};

struct cin_cache {
	std::array<char, MAX_OSPATH> fileName;
	int					CIN_WIDTH, CIN_HEIGHT;
	int					xpos, ypos, width, height;
	bool				looping, holdAtEnd, dirty, alterGameState, silent, shader;
	bool				rtcwLetterBox;
	ScopedFileHandle	iFile;
	e_status			status;
	int					startTime;
	int					lastTime;
	long				tfps;
	long				RoQPlayed;
	long				ROQSize;
	unsigned int		RoQFrameSize;
	long				onQuad;
	long				numQuads;
	long				samplesPerLine;
	unsigned int		roq_id;
	long				screenDelta;

	void ( *VQ0)(byte **status, byte *qdata );
	void ( *VQ1)(byte **status, byte *qdata );
	void ( *VQNormal)(byte **status, byte *qdata );
	void ( *VQBuffer)(byte **status, byte *qdata );

	long				samplesPerPixel;				// defaults to 2
	byte*				gray;
	unsigned int		xsize, ysize, maxsize, minsize;

	bool				half, smootheddouble;
	long				normalBuffer0;
	long				roq_flags;
	long				roqF0;
	long				roqF1;
	std::array<long, 2>	t;
	long				roqFPS;
	int					playonwalls;
	byte*				buf;
	long				drawX, drawY;
};

static cinematics_t		cin;
static std::array<cin_cache, MAX_VIDEO_HANDLES> cinTable;
static int				currentHandle = -1;
static int				CL_handle = -1;

extern "C" {
extern int				s_soundtime;		// sample PAIRS
}

extern int				CL_ScaledMilliseconds( void );

void CIN_CloseAllVideos( void ) {
	int		i;

	for ( i = 0 ; i < MAX_VIDEO_HANDLES ; i++ ) {
		if (cinTable[i].fileName[0] != '\0' ) {
			CIN_StopCinematic(i);
		}
		// Stop may return early before the first frame, at EOF, or after a
		// connection-state change. Bulk teardown still owns these resources.
		cinTable[i].iFile.reset();
		cinTable[i].fileName[0] = '\0';
		cinTable[i].buf = nullptr;
		cinTable[i].status = FMV_IDLE;
		cinTable[i].dirty = false;
	}
	currentHandle = cin.currentHandle = CL_handle = -1;
}


static int CIN_HandleForVideo( void ) {
	int		i;

	for ( i = 0 ; i < MAX_VIDEO_HANDLES ; i++ ) {
		if ( cinTable[i].fileName[0] == '\0' ) {
			return i;
		}
	}
	Com_Error( ERR_DROP, "CIN_HandleForVideo: none free" );
	return -1;
}


//-----------------------------------------------------------------------------
// RllSetupTable
//
// Allocates and initializes the square table.
//
// Parameters:	None
//
// Returns:		Nothing
//-----------------------------------------------------------------------------
static void RllSetupTable( void )
{
	int z;

	for (z=0;z<128;z++) {
		cin.sqrTable[z] = static_cast<short>( z * z );
		cin.sqrTable[z+128] = static_cast<short>( -cin.sqrTable[z] );
	}
}


//-----------------------------------------------------------------------------
// RllDecodeMonoToMono
//
// Decode mono source data into a mono buffer.
//
// Parameters:	from -> buffer holding encoded data
//				to ->	buffer to hold decoded data
//				size =	number of bytes of input (= # of shorts of output)
//				signedOutput = 0 for unsigned output, non-zero for signed output
//				flag = flags from asset header
//
// Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
long RllDecodeMonoToMono(unsigned char *from,short *to,unsigned int size,char signedOutput ,unsigned short flag)
{
	unsigned int z;
	int prev;
	
	if (signedOutput)	
		prev =  flag - 0x8000;
	else 
		prev = flag;

	for (z=0;z<size;z++) {
		prev = to[z] = static_cast<short>( prev + cin.sqrTable[from[z]] ); 
	}
	return size;
}


//-----------------------------------------------------------------------------
// RllDecodeMonoToStereo
//
// Decode mono source data into a stereo buffer. Output is 4 times the number
// of bytes in the input.
//
// Parameters:	from -> buffer holding encoded data
//				to ->	buffer to hold decoded data
//				size =	number of bytes of input (= 1/4 # of bytes of output)
//				signedOutput = 0 for unsigned output, non-zero for signed output
//				flag = flags from asset header
//
// Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
static long RllDecodeMonoToStereo(unsigned char *from,short *to,unsigned int size,char signedOutput,unsigned short flag)
{
	unsigned int z;
	int prev;
	
	if (signedOutput)	
		prev =  flag - 0x8000;
	else 
		prev = flag;

	for (z = 0; z < size; z++) {
		prev = static_cast<short>( prev + cin.sqrTable[from[z]] );
		to[z*2+0] = to[z*2+1] = static_cast<short>( prev );
	}
	
	return size;
}


//-----------------------------------------------------------------------------
// RllDecodeStereoToStereo
//
// Decode stereo source data into a stereo buffer.
//
// Parameters:	from -> buffer holding encoded data
//				to ->	buffer to hold decoded data
//				size =	number of bytes of input (= 1/2 # of bytes of output)
//				signedOutput = 0 for unsigned output, non-zero for signed output
//				flag = flags from asset header
//
// Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
static long RllDecodeStereoToStereo(unsigned char *from,short *to,unsigned int size,char signedOutput, unsigned short flag)
{
	unsigned int z;
	unsigned char *zz = from;
	int	prevL, prevR;

	if (signedOutput) {
		prevL = (flag & 0xff00) - 0x8000;
		prevR = ((flag & 0x00ff) << 8) - 0x8000;
	} else {
		prevL = flag & 0xff00;
		prevR = (flag & 0x00ff) << 8;
	}

	for (z=0;z<size;z+=2) {
                prevL = static_cast<short>( prevL + cin.sqrTable[*zz++] ); 
                prevR = static_cast<short>( prevR + cin.sqrTable[*zz++] );
                to[z+0] = static_cast<short>( prevL );
                to[z+1] = static_cast<short>( prevR );
	}
	
	return size >> 1;
}


//-----------------------------------------------------------------------------
// RllDecodeStereoToMono
//
// Decode stereo source data into a mono buffer.
//
// Parameters:	from -> buffer holding encoded data
//				to ->	buffer to hold decoded data
//				size =	number of bytes of input (= # of bytes of output)
//				signedOutput = 0 for unsigned output, non-zero for signed output
//				flag = flags from asset header
//
// Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
long RllDecodeStereoToMono(unsigned char *from,short *to,unsigned int size,char signedOutput, unsigned short flag)
{
	unsigned int z;
	int prevL,prevR;
	
	if (signedOutput) {
		prevL = (flag & 0xff00) - 0x8000;
		prevR = ((flag & 0x00ff) << 8) -0x8000;
	} else {
		prevL = flag & 0xff00;
		prevR = (flag & 0x00ff) << 8;
	}

	for (z=0;z<size;z+=1) {
		prevL= prevL + cin.sqrTable[from[z*2]];
		prevR = prevR + cin.sqrTable[from[z*2+1]];
		to[z] = static_cast<short>( ( prevL + prevR ) / 2 );
	}

	return size;
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void move8_32( byte *src, byte *dst, int spl )
{
	int i;

	for(i = 0; i < 8; ++i)
	{
		MoveBytes( src, dst, 32 );
		src += spl;
		dst += spl;
	}
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void move4_32( byte *src, byte *dst, int spl  )
{
	int i;

	for(i = 0; i < 4; ++i)
	{
		MoveBytes( src, dst, 16 );
		src += spl;
		dst += spl;
	}
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void blit8_32( byte *src, byte *dst, int spl  )
{
	int i;

	for(i = 0; i < 8; ++i)
	{
		CopyBytes( src, dst, 32 );
		src += 32;
		dst += spl;
	}
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/
static void blit4_32( byte *src, byte *dst, int spl  )
{
	int i;

	for(i = 0; i < 4; ++i)
	{
		CopyBytes( src, dst, 16 );
		src += 16;
		dst += spl;
	}
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void blit2_32( byte *src, byte *dst, int spl  )
{
	CopyBytes( src, dst, 8 );
	CopyBytes( src + 8, dst + spl, 8 );
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void blitVQQuad32fs( byte **status, unsigned char *data )
{
unsigned short	newd, celdata, code;
unsigned int	index, i;
int		spl;

	newd	= 0;
	celdata = 0;
	index	= 0;
	
        spl = cinTable[currentHandle].samplesPerLine;
        
	do {
		if (!newd) { 
			newd = 7;
			celdata = data[0] + data[1]*256;
			data += 2;
		} else {
			newd--;
		}

		code = static_cast<unsigned short>( celdata & 0xc000 );
		celdata <<= 2;
		
		switch (code) {
			case	0x8000:													// vq code
				blit8_32( AsWritableBytes( &vq8[(*data)*128] ), status[index], spl );
				data++;
				index += 5;
				break;
			case	0xc000:													// drop
				index++;													// skip 8x8
				for(i=0;i<4;i++) {
					if (!newd) { 
						newd = 7;
						celdata = data[0] + data[1]*256;
						data += 2;
					} else {
						newd--;
					}
						
					code = static_cast<unsigned short>( celdata & 0xc000 ); celdata <<= 2;

					switch (code) {											// code in top two bits of code
						case	0x8000:										// 4x4 vq code
							blit4_32( AsWritableBytes( &vq4[(*data)*32] ), status[index], spl );
							data++;
							break;
						case	0xc000:										// 2x2 vq code
							blit2_32( AsWritableBytes( &vq2[(*data)*8] ), status[index], spl );
							data++;
							blit2_32( AsWritableBytes( &vq2[(*data)*8] ), status[index]+8, spl );
							data++;
							blit2_32( AsWritableBytes( &vq2[(*data)*8] ), status[index]+spl*2, spl );
							data++;
							blit2_32( AsWritableBytes( &vq2[(*data)*8] ), status[index]+spl*2+8, spl );
							data++;
							break;
						case	0x4000:										// motion compensation
							move4_32( status[index] + cin.mcomp[(*data)], status[index], spl );
							data++;
							break;
					}
					index++;
				}
				break;
			case	0x4000:													// motion compensation
				move8_32( status[index] + cin.mcomp[(*data)], status[index], spl );
				data++;
				index += 5;
				break;
			case	0x0000:
				index += 5;
				break;
		}
	} while ( status[index] != nullptr );
}

/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void ROQ_GenYUVTables( void )
{
	float t_ub,t_vr,t_ug,t_vg;
	long i;

	t_ub = (1.77200f/2.0f) * static_cast<float>( 1 << 6 ) + 0.5f;
	t_vr = (1.40200f/2.0f) * static_cast<float>( 1 << 6 ) + 0.5f;
	t_ug = (0.34414f/2.0f) * static_cast<float>( 1 << 6 ) + 0.5f;
	t_vg = (0.71414f/2.0f) * static_cast<float>( 1 << 6 ) + 0.5f;
	for(i=0;i<256;i++) {
		float x = static_cast<float>( 2 * i - 255 );
	
		ROQ_UB_tab[i] = static_cast<long>( ( t_ub * x) + (1<<5) );
		ROQ_VR_tab[i] = static_cast<long>( ( t_vr * x) + (1<<5) );
		ROQ_UG_tab[i] = static_cast<long>( -t_ug * x );
		ROQ_VG_tab[i] = static_cast<long>( (-t_vg * x) + (1<<5) );
		ROQ_YY_tab[i] = static_cast<long>( (i << 6) | (i >> 2) );
	}
}

#define VQ2TO4(a,b,c,d) { \
    	*c++ = a[0];	\
	*d++ = a[0];	\
	*d++ = a[0];	\
	*c++ = a[1];	\
	*d++ = a[1];	\
	*d++ = a[1];	\
	*c++ = b[0];	\
	*d++ = b[0];	\
	*d++ = b[0];	\
	*c++ = b[1];	\
	*d++ = b[1];	\
	*d++ = b[1];	\
	*d++ = a[0];	\
	*d++ = a[0];	\
	*d++ = a[1];	\
	*d++ = a[1];	\
	*d++ = b[0];	\
	*d++ = b[0];	\
	*d++ = b[1];	\
	*d++ = b[1];	\
	a += 2; b += 2; }
 
#define VQ2TO2(a,b,c,d) { \
	*c++ = *a;	\
	*d++ = *a;	\
	*d++ = *a;	\
	*c++ = *b;	\
	*d++ = *b;	\
	*d++ = *b;	\
	*d++ = *a;	\
	*d++ = *a;	\
	*d++ = *b;	\
	*d++ = *b;	\
	a++; b++; }

/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static unsigned short yuv_to_rgb( long y, long u, long v )
{ 
	long r,g,b,YY = static_cast<long>( ROQ_YY_tab[(y)] );

	r = (YY + ROQ_VR_tab[v]) >> 9;
	g = (YY + ROQ_UG_tab[u] + ROQ_VG_tab[v]) >> 8;
	b = (YY + ROQ_UB_tab[u]) >> 9;
	
	if (r < 0) r = 0;
	if (g < 0) g = 0;
	if (b < 0) b = 0;
	if (r > 31) r = 31;
	if (g > 63) g = 63;
	if (b > 31) b = 31;

	return static_cast<unsigned short>( (r<<11)+(g<<5)+(b) );
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/
static unsigned int yuv_to_rgb24( long y, long u, long v )
{ 
	long r,g,b,YY = static_cast<long>( ROQ_YY_tab[(y)] );

	r = (YY + ROQ_VR_tab[v]) >> 6;
	g = (YY + ROQ_UG_tab[u] + ROQ_VG_tab[v]) >> 6;
	b = (YY + ROQ_UB_tab[u]) >> 6;
	
	if (r < 0) r = 0;
	if (g < 0) g = 0;
	if (b < 0) b = 0;
	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;
	
	return LittleLong ((unsigned long)((r)|(g<<8)|(b<<16))|(255UL<<24));
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void decodeCodeBook( byte *input, unsigned short roq_flags, unsigned payloadBytes )
{
	long	i, j, two, four;
	unsigned short	*aptr, *bptr, *cptr, *dptr;
	long	y0,y1,y2,y3,cr,cb;
	byte	*bbptr, *baptr, *bcptr, *bdptr;
	union {
		unsigned int *i;
		unsigned short *s;
	} iaptr, ibptr, icptr, idptr;

	two = (roq_flags >> 8) ? (roq_flags >> 8) : 256;
	four = roq_flags & 0xff;
	// A zero large-entry count means either no update or all 256 entries.
	// Stock Q3/RTCW movies use both forms; the payload length disambiguates.
	if ( !four && payloadBytes > static_cast<unsigned>( two * 6 ) ) four = 256;

	four *= 2;

	bptr = vq2.data();

	if (!cinTable[currentHandle].half) {
		if (!cinTable[currentHandle].smootheddouble) {
//
// normal height
//
			if (cinTable[currentHandle].samplesPerPixel==2) {
				for(i=0;i<two;i++) {
					y0 = static_cast<long>( *input++ );
					y1 = static_cast<long>( *input++ );
					y2 = static_cast<long>( *input++ );
					y3 = static_cast<long>( *input++ );
					cr = static_cast<long>( *input++ );
					cb = static_cast<long>( *input++ );
					*bptr++ = yuv_to_rgb( y0, cr, cb );
					*bptr++ = yuv_to_rgb( y1, cr, cb );
					*bptr++ = yuv_to_rgb( y2, cr, cb );
					*bptr++ = yuv_to_rgb( y3, cr, cb );
				}

				cptr = vq4.data();
				dptr = vq8.data();
		
				for(i=0;i<four;i++) {
					aptr = vq2.data() + (*input++)*4;
					bptr = vq2.data() + (*input++)*4;
					for(j=0;j<2;j++)
						VQ2TO4(aptr,bptr,cptr,dptr);
				}
			} else if (cinTable[currentHandle].samplesPerPixel==4) {
				ibptr.s = bptr;
				for(i=0;i<two;i++) {
					y0 = static_cast<long>( *input++ );
					y1 = static_cast<long>( *input++ );
					y2 = static_cast<long>( *input++ );
					y3 = static_cast<long>( *input++ );
					cr = static_cast<long>( *input++ );
					cb = static_cast<long>( *input++ );
					*ibptr.i++ = yuv_to_rgb24( y0, cr, cb );
					*ibptr.i++ = yuv_to_rgb24( y1, cr, cb );
					*ibptr.i++ = yuv_to_rgb24( y2, cr, cb );
					*ibptr.i++ = yuv_to_rgb24( y3, cr, cb );
				}

				icptr.s = vq4.data();
				idptr.s = vq8.data();
	
				for(i=0;i<four;i++) {
					iaptr.s = vq2.data();
					iaptr.i += (*input++)*4;
					ibptr.s = vq2.data();
					ibptr.i += (*input++)*4;
					for(j=0;j<2;j++) 
						VQ2TO4(iaptr.i, ibptr.i, icptr.i, idptr.i);
				}
			} else if (cinTable[currentHandle].samplesPerPixel==1) {
				bbptr = AsWritableBytes( bptr );
				for(i=0;i<two;i++) {
					*bbptr++ = cinTable[currentHandle].gray[*input++];
					*bbptr++ = cinTable[currentHandle].gray[*input++];
					*bbptr++ = cinTable[currentHandle].gray[*input++];
					*bbptr++ = cinTable[currentHandle].gray[*input]; input +=3;
				}

				bcptr = AsWritableBytes( vq4.data() );
				bdptr = AsWritableBytes( vq8.data() );
	
				for(i=0;i<four;i++) {
					baptr = AsWritableBytes( vq2.data() ) + (*input++)*4;
					bbptr = AsWritableBytes( vq2.data() ) + (*input++)*4;
					for(j=0;j<2;j++) 
						VQ2TO4(baptr,bbptr,bcptr,bdptr);
				}
			}
		} else {
//
// double height, smoothed
//
			if (cinTable[currentHandle].samplesPerPixel==2) {
				for(i=0;i<two;i++) {
					y0 = static_cast<long>( *input++ );
					y1 = static_cast<long>( *input++ );
					y2 = static_cast<long>( *input++ );
					y3 = static_cast<long>( *input++ );
					cr = static_cast<long>( *input++ );
					cb = static_cast<long>( *input++ );
					*bptr++ = yuv_to_rgb( y0, cr, cb );
					*bptr++ = yuv_to_rgb( y1, cr, cb );
					*bptr++ = yuv_to_rgb( ((y0*3)+y2)/4, cr, cb );
					*bptr++ = yuv_to_rgb( ((y1*3)+y3)/4, cr, cb );
					*bptr++ = yuv_to_rgb( (y0+(y2*3))/4, cr, cb );
					*bptr++ = yuv_to_rgb( (y1+(y3*3))/4, cr, cb );
					*bptr++ = yuv_to_rgb( y2, cr, cb );
					*bptr++ = yuv_to_rgb( y3, cr, cb );
				}

				cptr = vq4.data();
				dptr = vq8.data();
		
				for(i=0;i<four;i++) {
					aptr = vq2.data() + (*input++)*8;
					bptr = vq2.data() + (*input++)*8;
					for(j=0;j<2;j++) {
						VQ2TO4(aptr,bptr,cptr,dptr);
						VQ2TO4(aptr,bptr,cptr,dptr);
					}
				}
			} else if (cinTable[currentHandle].samplesPerPixel==4) {
				ibptr.s = bptr;
				for(i=0;i<two;i++) {
					y0 = static_cast<long>( *input++ );
					y1 = static_cast<long>( *input++ );
					y2 = static_cast<long>( *input++ );
					y3 = static_cast<long>( *input++ );
					cr = static_cast<long>( *input++ );
					cb = static_cast<long>( *input++ );
					*ibptr.i++ = yuv_to_rgb24( y0, cr, cb );
					*ibptr.i++ = yuv_to_rgb24( y1, cr, cb );
					*ibptr.i++ = yuv_to_rgb24( ((y0*3)+y2)/4, cr, cb );
					*ibptr.i++ = yuv_to_rgb24( ((y1*3)+y3)/4, cr, cb );
					*ibptr.i++ = yuv_to_rgb24( (y0+(y2*3))/4, cr, cb );
					*ibptr.i++ = yuv_to_rgb24( (y1+(y3*3))/4, cr, cb );
					*ibptr.i++ = yuv_to_rgb24( y2, cr, cb );
					*ibptr.i++ = yuv_to_rgb24( y3, cr, cb );
				}

				icptr.s = vq4.data();
				idptr.s = vq8.data();
	
				for(i=0;i<four;i++) {
					iaptr.s = vq2.data();
					iaptr.i += (*input++)*8;
					ibptr.s = vq2.data();
					ibptr.i += (*input++)*8;
					for(j=0;j<2;j++) {
						VQ2TO4(iaptr.i, ibptr.i, icptr.i, idptr.i);
						VQ2TO4(iaptr.i, ibptr.i, icptr.i, idptr.i);
					}
				}
			} else if (cinTable[currentHandle].samplesPerPixel==1) {
				bbptr = AsWritableBytes( bptr );
				for(i=0;i<two;i++) {
					y0 = static_cast<long>( *input++ );
					y1 = static_cast<long>( *input++ );
					y2 = static_cast<long>( *input++ );
					y3 = static_cast<long>( *input ); input+= 3;
					*bbptr++ = cinTable[currentHandle].gray[y0];
					*bbptr++ = cinTable[currentHandle].gray[y1];
					*bbptr++ = cinTable[currentHandle].gray[((y0*3)+y2)/4];
					*bbptr++ = cinTable[currentHandle].gray[((y1*3)+y3)/4];
					*bbptr++ = cinTable[currentHandle].gray[(y0+(y2*3))/4];
					*bbptr++ = cinTable[currentHandle].gray[(y1+(y3*3))/4];						
					*bbptr++ = cinTable[currentHandle].gray[y2];
					*bbptr++ = cinTable[currentHandle].gray[y3];
				}

				bcptr = AsWritableBytes( vq4.data() );
				bdptr = AsWritableBytes( vq8.data() );
	
				for(i=0;i<four;i++) {
					baptr = AsWritableBytes( vq2.data() ) + (*input++)*8;
					bbptr = AsWritableBytes( vq2.data() ) + (*input++)*8;
					for(j=0;j<2;j++) {
						VQ2TO4(baptr,bbptr,bcptr,bdptr);
						VQ2TO4(baptr,bbptr,bcptr,bdptr);
					}
				}
			}			
		}
	} else {
//
// 1/4 screen
//
		if (cinTable[currentHandle].samplesPerPixel==2) {
			for(i=0;i<two;i++) {
				y0 = static_cast<long>( *input ); input+=2;
				y2 = static_cast<long>( *input ); input+=2;
				cr = static_cast<long>( *input++ );
				cb = static_cast<long>( *input++ );
				*bptr++ = yuv_to_rgb( y0, cr, cb );
				*bptr++ = yuv_to_rgb( y2, cr, cb );
			}

			cptr = vq4.data();
			dptr = vq8.data();
	
			for(i=0;i<four;i++) {
				aptr = vq2.data() + (*input++)*2;
				bptr = vq2.data() + (*input++)*2;
				for(j=0;j<2;j++) { 
					VQ2TO2(aptr,bptr,cptr,dptr);
				}
			}
		} else if (cinTable[currentHandle].samplesPerPixel == 1) {
			bbptr = AsWritableBytes( bptr );
				
			for(i=0;i<two;i++) {
				*bbptr++ = cinTable[currentHandle].gray[*input]; input+=2;
				*bbptr++ = cinTable[currentHandle].gray[*input]; input+=4;
			}

			bcptr = AsWritableBytes( vq4.data() );
			bdptr = AsWritableBytes( vq8.data() );
	
			for(i=0;i<four;i++) {
				baptr = AsWritableBytes( vq2.data() ) + (*input++)*2;
				bbptr = AsWritableBytes( vq2.data() ) + (*input++)*2;
				for(j=0;j<2;j++) { 
					VQ2TO2(baptr,bbptr,bcptr,bdptr);
				}
			}			
		} else if (cinTable[currentHandle].samplesPerPixel == 4) {
			ibptr.s = bptr;
			for(i=0;i<two;i++) {
				y0 = static_cast<long>( *input ); input+=2;
				y2 = static_cast<long>( *input ); input+=2;
				cr = static_cast<long>( *input++ );
				cb = static_cast<long>( *input++ );
				*ibptr.i++ = yuv_to_rgb24( y0, cr, cb );
				*ibptr.i++ = yuv_to_rgb24( y2, cr, cb );
			}

			icptr.s = vq4.data();
			idptr.s = vq8.data();
	
			for(i=0;i<four;i++) {
				iaptr.s = vq2.data();
				iaptr.i += (*input++)*2;
				ibptr.s = vq2.data() + (*input++)*2;
				ibptr.i += (*input++)*2;
				for(j=0;j<2;j++) { 
					VQ2TO2(iaptr.i,ibptr.i,icptr.i,idptr.i);
				}
			}
		}
	}
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void recurseQuad( long startX, long startY, long quadSize, long xOff, long yOff )
{
	byte *scroff;
	long bigx, bigy, lowx, lowy, useY;
	long offset;

	offset = cinTable[currentHandle].screenDelta;
	
	lowx = lowy = 0;
	bigx = cinTable[currentHandle].xsize;
	bigy = cinTable[currentHandle].ysize;

	if (bigx > cinTable[currentHandle].CIN_WIDTH) bigx = cinTable[currentHandle].CIN_WIDTH;
	if (bigy > cinTable[currentHandle].CIN_HEIGHT) bigy = cinTable[currentHandle].CIN_HEIGHT;

	if ( (startX >= lowx) && (startX+quadSize) <= (bigx) && (startY+quadSize) <= (bigy) && (startY >= lowy) && quadSize <= MAXSIZE) {
		useY = startY;
		scroff = cin.linbuf.data() + (useY+((cinTable[currentHandle].CIN_HEIGHT-bigy)>>1)+yOff)*(cinTable[currentHandle].samplesPerLine) + (((startX+xOff))*cinTable[currentHandle].samplesPerPixel);

		cin.qStatus[0][cinTable[currentHandle].onQuad  ] = scroff;
		cin.qStatus[1][cinTable[currentHandle].onQuad++] = scroff+offset;
	}

	if ( quadSize != MINSIZE ) {
		quadSize >>= 1;
		recurseQuad( startX,		  startY		  , quadSize, xOff, yOff );
		recurseQuad( startX+quadSize, startY		  , quadSize, xOff, yOff );
		recurseQuad( startX,		  startY+quadSize , quadSize, xOff, yOff );
		recurseQuad( startX+quadSize, startY+quadSize , quadSize, xOff, yOff );
	}
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void setupQuad( long xOff, long yOff )
{
	long numQuadCels, i,x,y;
	byte *temp;

	if (xOff == cin.oldXOff && yOff == cin.oldYOff && cinTable[currentHandle].ysize == cin.oldysize && cinTable[currentHandle].xsize == cin.oldxsize) {
		return;
	}

	cin.oldXOff = xOff;
	cin.oldYOff = yOff;
	cin.oldysize = cinTable[currentHandle].ysize;
	cin.oldxsize = cinTable[currentHandle].xsize;

	numQuadCels  = (cinTable[currentHandle].xsize*cinTable[currentHandle].ysize) / (16);
	numQuadCels += numQuadCels/4;
	numQuadCels += 64;							  // for overflow

	cinTable[currentHandle].onQuad = 0;

	for(y=0;y<static_cast<long>( cinTable[currentHandle].ysize );y+=16)
		for(x=0;x<static_cast<long>( cinTable[currentHandle].xsize );x+=16)
			recurseQuad( x, y, 16, xOff, yOff );

	temp = nullptr;

	for(i=(numQuadCels-64);i<numQuadCels;i++) {
		cin.qStatus[0][i] = temp;			  // eoq
		cin.qStatus[1][i] = temp;			  // eoq
	}
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void readQuadInfo( byte *qData )
{
	const glconfig_t *config;
	if (currentHandle < 0) return;

	cinTable[currentHandle].xsize    = qData[0]+qData[1]*256;
	cinTable[currentHandle].ysize    = qData[2]+qData[3]*256;
	cinTable[currentHandle].maxsize  = qData[4]+qData[5]*256;
	cinTable[currentHandle].minsize  = qData[6]+qData[7]*256;
	
	cinTable[currentHandle].CIN_HEIGHT = cinTable[currentHandle].ysize;
	cinTable[currentHandle].CIN_WIDTH  = cinTable[currentHandle].xsize;

	cinTable[currentHandle].samplesPerLine = cinTable[currentHandle].CIN_WIDTH*cinTable[currentHandle].samplesPerPixel;
	cinTable[currentHandle].screenDelta = cinTable[currentHandle].CIN_HEIGHT*cinTable[currentHandle].samplesPerLine;

	cinTable[currentHandle].half = false;
	cinTable[currentHandle].smootheddouble = false;
	
	cinTable[currentHandle].VQ0 = cinTable[currentHandle].VQNormal;
	cinTable[currentHandle].VQ1 = cinTable[currentHandle].VQBuffer;

	cinTable[currentHandle].t[0] = cinTable[currentHandle].screenDelta;
	cinTable[currentHandle].t[1] = -cinTable[currentHandle].screenDelta;

	cinTable[currentHandle].drawX = cinTable[currentHandle].CIN_WIDTH;
	cinTable[currentHandle].drawY = cinTable[currentHandle].CIN_HEIGHT;

	// rage pro is very slow at 512 wide textures, voodoo can't do it at all
	config = re.GetConfig();
	if ( config->hardwareType == GLHW_RAGEPRO || config->maxTextureSize <= 256 ) {
		if ( cinTable[currentHandle].drawX > 256 ) {
			cinTable[currentHandle].drawX = 256;
		}
		if ( cinTable[currentHandle].drawY > 256 ) {
			cinTable[currentHandle].drawY = 256;
		}
		if ( cinTable[currentHandle].CIN_WIDTH != 256 || cinTable[currentHandle].CIN_HEIGHT != 256 ) {
			Com_Printf( "HACK: approxmimating cinematic for Rage Pro or Voodoo\n" );
		}
	}
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void RoQPrepMcomp( long xoff, long yoff ) 
{
	long i, j, x, y, temp, temp2;

	i=cinTable[currentHandle].samplesPerLine; j=cinTable[currentHandle].samplesPerPixel;
	if ( cinTable[currentHandle].xsize == (cinTable[currentHandle].ysize*4) && !cinTable[currentHandle].half ) { j = j+j; i = i+i; }
	
	for(y=0;y<16;y++) {
		temp2 = (y+yoff-8)*i;
		for(x=0;x<16;x++) {
			temp = (x+xoff-8)*j;
			cin.mcomp[(x*16)+y] = cinTable[currentHandle].normalBuffer0-(temp2+temp);
		}
	}
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void initRoQ( void ) 
{
	if (currentHandle < 0) return;

	cinTable[currentHandle].VQNormal = blitVQQuad32fs;
	cinTable[currentHandle].VQBuffer = blitVQQuad32fs;
	cinTable[currentHandle].samplesPerPixel = 4;
	ROQ_GenYUVTables();
	RllSetupTable();
}

/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/
/*
static byte* RoQFetchInterlaced( byte *source ) {
	int x, *src, *dst;

	if (currentHandle < 0) return nullptr;

	src = (int *)source;
	dst = (int *)cinTable[currentHandle].buf2;

	for(x=0;x<256*256;x++) {
		*dst = *src;
		dst++; src += 2;
	}
	return cinTable[currentHandle].buf2;
}
*/


static void RoQReject( const char *reason ) {
	auto &movie = cinTable[currentHandle];
	Com_Printf( "Invalid RoQ movie %s: %s\n", movie.fileName.data(), reason );
	// Corruption must not restart a looping shader movie forever.
	movie.looping = false;
	movie.holdAtEnd = false;
	movie.status = FMV_EOF;
}

static bool RoQReadChunkHeader( const byte *header ) {
	auto &movie = cinTable[currentHandle];
	const std::uint32_t size = static_cast<std::uint32_t>( header[2] ) |
		(static_cast<std::uint32_t>( header[3] ) << 8) |
		(static_cast<std::uint32_t>( header[4] ) << 16) |
		(static_cast<std::uint32_t>( header[5] ) << 24);
	const unsigned id = header[0] | (header[1] << 8);
	if ( size > 65536 || id == 0 || id == 0x1084 ) return false;
	movie.roq_id = id;
	movie.RoQFrameSize = size;
	movie.roq_flags = header[6] | (header[7] << 8);
	movie.roqF0 = static_cast<signed char>( header[7] );
	movie.roqF1 = static_cast<signed char>( header[6] );
	return true;
}

// Validate the complete VQ instruction stream before the legacy blitter writes
// either frame. The control word can be interleaved with index/motion bytes.
static bool RoQValidateVQ( byte **status, const byte *data, size_t bytes ) {
	const auto &movie = cinTable[currentHandle];
	if ( movie.numQuads < 0 || movie.onQuad <= 0 || movie.screenDelta <= 0 ) return false;
	size_t cursor = 0;
	unsigned codes = 0, remaining = 0;
	auto readCode = [&]( unsigned &code ) {
		if ( !remaining ) {
			if ( bytes - cursor < 2 ) return false;
			codes = data[cursor] | (data[cursor + 1] << 8);
			cursor += 2;
			remaining = 8;
		}
		code = (codes >> 14) & 3;
		codes <<= 2;
		--remaining;
		return true;
	};
	auto block = [&]( unsigned code, long index, int side ) {
		const size_t count = code == 3 ? 4 : code == 0 ? 0 : 1;
		if ( count > bytes - cursor ) return false;
		if ( code == 1 ) {
			const std::int64_t offset = status[index] - cin.linbuf.data();
			const std::int64_t source = offset + cin.mcomp[data[cursor]];
			const std::int64_t end = source + (side - 1) * movie.samplesPerLine + side * 4;
			const std::int64_t previous = movie.normalBuffer0 > 0 ? movie.screenDelta : 0;
			if ( source < previous || end > previous + movie.screenDelta ) return false;
		}
		cursor += count;
		return true;
	};
	for ( long index = 0; index < movie.onQuad; ) {
		unsigned code;
		if ( !readCode( code ) || movie.onQuad - index < 5 ) return false;
		if ( code == 3 ) {
			++index;
			for ( int child = 0; child < 4; ++child, ++index ) {
				if ( !readCode( code ) || !block( code, index, 4 ) ) return false;
			}
		} else {
			if ( !block( code, index, 8 ) ) return false;
			index += 5;
		}
	}
	return true;
}

static void RoQReset( void ) {
	
	if (currentHandle < 0) return;

	cinTable[currentHandle].iFile.reset();
	cinTable[currentHandle].ROQSize = OpenFileRead(
		cinTable[currentHandle].fileName.data(), cinTable[currentHandle].iFile, qtrue );
	if ( cinTable[currentHandle].ROQSize < 16 ||
		FileRead( cinTable[currentHandle].iFile.get(), cin.file.data(), 16 ) != 16 ) {
		RoQReject( "truncated file header on restart" );
		return;
	}
	if ( !RoQ_init() ) {
		RoQReject( "invalid file header on restart" );
		return;
	}
	cinTable[currentHandle].status = FMV_LOOPED;
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void RoQInterrupt()
{
	byte				*framedata;
	std::array<short, 32768> sbuf;
	int ssize;

	if (currentHandle < 0) return;
	auto &movie = cinTable[currentHandle];
	if ( movie.roq_id == 0 ) {
		if ( movie.numQuads <= 0 ) {
			RoQReject( "movie contains no video frames" );
			return;
		}
		if (!cinTable[currentHandle].holdAtEnd) {
			if (cinTable[currentHandle].looping) {
				RoQReset();
			} else {
				cinTable[currentHandle].status = FMV_EOF;
			}
		} else {
			cinTable[currentHandle].status = FMV_IDLE;
		}
		return; 
	}

	const unsigned payloadBytes = movie.RoQFrameSize;
	if ( payloadBytes > 65536 || movie.RoQPlayed > movie.ROQSize ||
		static_cast<long>( payloadBytes ) > movie.ROQSize - movie.RoQPlayed ||
		FileRead( movie.iFile.get(), cin.file.data(), payloadBytes ) != static_cast<int>( payloadBytes ) ) {
		RoQReject( "truncated chunk payload" );
		return;
	}
	movie.RoQPlayed += payloadBytes;
	const bool packed = movie.roq_id == ROQ_PACKET;
	const unsigned chunks = packed ? static_cast<unsigned>( movie.roq_flags ) : 1;
	if ( packed && (!chunks || chunks > payloadBytes / 8) ) {
		RoQReject( "invalid packet chunk count" );
		return;
	}
	size_t offset = 0;
	for ( unsigned chunk = 0; chunk < chunks; ++chunk ) {
		if ( packed ) {
			if ( payloadBytes - offset < 8 || !RoQReadChunkHeader( cin.file.data() + offset ) ) {
				RoQReject( "invalid packet header" );
				return;
			}
			offset += 8;
			if ( movie.RoQFrameSize > payloadBytes - offset ) {
				RoQReject( "truncated packet payload" );
				return;
			}
		}
		framedata = cin.file.data() + offset;
		switch(cinTable[currentHandle].roq_id)
		{
			case	ROQ_QUAD_VQ:
				movie.normalBuffer0 = movie.t[movie.numQuads & 1];
				RoQPrepMcomp( movie.roqF0, movie.roqF1 );
				if ( !RoQValidateVQ( cin.qStatus[movie.numQuads & 1], framedata, movie.RoQFrameSize ) ) {
					RoQReject( "invalid VQ payload or motion range" );
					return;
				}
				if ((cinTable[currentHandle].numQuads&1)) {
					cinTable[currentHandle].VQ1( cin.qStatus[1], framedata );
					cinTable[currentHandle].buf = 	cin.linbuf.data() + cinTable[currentHandle].screenDelta;
				} else {
					cinTable[currentHandle].VQ0( cin.qStatus[0], framedata );
					cinTable[currentHandle].buf = 	cin.linbuf.data();
				}
				if (cinTable[currentHandle].numQuads == 0) {		// first frame
					std::copy_n( cin.linbuf.data(),
						cinTable[currentHandle].samplesPerLine * cinTable[currentHandle].ysize,
						cin.linbuf.data() + cinTable[currentHandle].screenDelta );
				}
				cinTable[currentHandle].numQuads++;
				cinTable[currentHandle].dirty = true;
				break;
			case	ROQ_CODEBOOK:
			{
				unsigned smallEntries = static_cast<unsigned>( movie.roq_flags ) >> 8;
				if ( !smallEntries ) smallEntries = 256;
				unsigned largeEntries = movie.roq_flags & 255;
				if ( !largeEntries && movie.RoQFrameSize > smallEntries * 6 ) largeEntries = 256;
				if ( smallEntries * 6 + largeEntries * 4 > movie.RoQFrameSize ) {
					RoQReject( "truncated codebook" );
					return;
				}
				decodeCodeBook( framedata, static_cast<unsigned short>( movie.roq_flags ), movie.RoQFrameSize );
				break;
			}
			case	ZA_SOUND_MONO:
				// Alice decodes a movie's own sound and plays none of it
				// (alice.exe 0x00425ac1, 0x00425ae5); its sound is the MP3.
				if (!cinTable[currentHandle].silent && !Com_GameIsAlice()) {
					// two output shorts per input byte
					unsigned int decodeBytes = cinTable[currentHandle].RoQFrameSize;
					if ( decodeBytes > sbuf.size() / 2 )
						decodeBytes = static_cast<unsigned int>( sbuf.size() / 2 );
					ssize = RllDecodeMonoToStereo( framedata, sbuf.data(), decodeBytes, 0, static_cast<unsigned short>( cinTable[currentHandle].roq_flags ));
						S_RawSamples( ssize, 22050, 2, 1, AsWritableBytes( sbuf.data() ), s_volume->value );
				}
				break;
			case	ZA_SOUND_STEREO:
				if ( movie.RoQFrameSize & 1 ) {
					RoQReject( "incomplete stereo sample" );
					return;
				}
				if (!cinTable[currentHandle].silent && !Com_GameIsAlice()) {
					if (cinTable[currentHandle].numQuads == -1) {
						S_Update( 333 );
						s_rawend = s_soundtime;
					}
					// one output short per input byte
					unsigned int decodeBytes = cinTable[currentHandle].RoQFrameSize;
					if ( decodeBytes > sbuf.size() )
						decodeBytes = static_cast<unsigned int>( sbuf.size() );
					ssize = RllDecodeStereoToStereo( framedata, sbuf.data(), decodeBytes, 0, static_cast<unsigned short>( cinTable[currentHandle].roq_flags ));
						S_RawSamples( ssize, 22050, 2, 2, AsWritableBytes( sbuf.data() ), s_volume->value );
				}
				break;
			case	ROQ_QUAD_INFO:
			{
				if ( movie.RoQFrameSize < 8 ) {
					RoQReject( "truncated quad information" );
					return;
				}
				const unsigned width = framedata[0] | (framedata[1] << 8);
				const unsigned height = framedata[2] | (framedata[3] << 8);
				// setupQuad emits complete 16x16 groups into two fixed frames.
				if ( !width || !height || width > DEFAULT_CIN_WIDTH || height > DEFAULT_CIN_HEIGHT ||
					(width & 15) || (height & 15) ) {
					RoQReject( "unsupported quad dimensions" );
					return;
				}
				if (cinTable[currentHandle].numQuads == -1) {
					readQuadInfo( framedata );
					setupQuad( 0, 0 );
					cinTable[currentHandle].startTime = cinTable[currentHandle].lastTime = CL_ScaledMilliseconds();
				}
				if (cinTable[currentHandle].numQuads != 1) cinTable[currentHandle].numQuads = 0;
				break;
			}
			case	ROQ_QUAD_HANG:
				if ( movie.RoQFrameSize ) {
					RoQReject( "nonempty hang chunk" );
					return;
				}
				break;
			case	ROQ_QUAD_JPEG:
				// Original Q3/RTCW engines skip this chunk without publishing a frame.
				break;
			default:
				RoQReject( "unsupported chunk type" );
				return;
		}
		offset += movie.RoQFrameSize;
	}
	if ( offset != payloadBytes ) {
		RoQReject( "packet payload does not match its chunk count" );
		return;
	}
	if ( movie.RoQPlayed == movie.ROQSize ) {
		// Leave the final decoded frame visible until the next playback tick.
		movie.roq_id = 0;
		movie.RoQFrameSize = 0;
		return;
	}
	std::array<byte, 8> header;
	if ( movie.ROQSize - movie.RoQPlayed < 8 ||
		FileRead( movie.iFile.get(), header.data(), header.size() ) != 8 ||
		!RoQReadChunkHeader( header.data() ) ) {
		RoQReject( "invalid or truncated chunk header" );
		return;
	}
	movie.RoQPlayed += 8;
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

// Callers provide a complete 16-byte file/first-chunk header. Validate the full
// 32-bit chunk length before it can reach the fixed input buffer.
static qboolean RoQ_init( void )
{
	cinTable[currentHandle].startTime = cinTable[currentHandle].lastTime = CL_ScaledMilliseconds();

	cinTable[currentHandle].RoQPlayed = 16;

/*	get frame rate */
	cinTable[currentHandle].roqFPS	 = cin.file[ 6] + cin.file[ 7]*256;

	if (!cinTable[currentHandle].roqFPS) cinTable[currentHandle].roqFPS = 30;

	cinTable[currentHandle].numQuads = -1;
	cinTable[currentHandle].onQuad = 0;
	cinTable[currentHandle].screenDelta = 0;
	// The shared quad pointers belong to the most recently decoded movie.
	cin.oldxsize = cin.oldysize = 0;
	if ( cin.file[0] != 0x84 || cin.file[1] != 0x10 ||
		!RoQReadChunkHeader( cin.file.data() + 8 ) || !cinTable[currentHandle].RoQFrameSize ) {
		cinTable[currentHandle].RoQFrameSize = 0;
		return qfalse;
	}

	return qtrue;
}


/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void RoQShutdown( void ) {
	if ( currentHandle < 0 ) return;
	if ( Com_GameIsAlice() && currentHandle == CL_handle ) {
		// The cinematic's MP3 goes with it (alice.exe 0x004257f4).
		S_AliceMP3Free();
	}
	Com_DPrintf("finished cinematic\n");
	cinTable[currentHandle].status = FMV_IDLE;

	cinTable[currentHandle].iFile.reset();

	if (cinTable[currentHandle].alterGameState && cls.state == CA_CINEMATIC) {
		cls.state = CA_DISCONNECTED;
		// we can't just do a vstr nextmap, because
		// if we are aborting the intro cinematic with
		// a devmap command, nextmap would be valid by
		// the time it was referenced
		CL_RunCinematicNextmap();
		CL_handle = -1;
	}
	cinTable[currentHandle].fileName[0] = '\0';
	cinTable[currentHandle].buf = nullptr;
	cinTable[currentHandle].dirty = false;
	currentHandle = -1;
}

/*
==================
CIN_StopCinematic
==================
*/
e_status CIN_StopCinematic( int handle ) {
	
	if (handle < 0 || handle>= MAX_VIDEO_HANDLES) return FMV_EOF;
	currentHandle = handle;

	Com_DPrintf("trFMV::stop(), closing %s\n", cinTable[currentHandle].fileName.data());

	if (cinTable[currentHandle].alterGameState) {
		if ( cls.state != CA_CINEMATIC ) {
			return cinTable[currentHandle].status;
		}
	}
	cinTable[currentHandle].status = FMV_EOF;
	RoQShutdown();

	return FMV_EOF;
}


/*
==================
CIN_RunCinematic

Fetch and decompress the pending frame
==================
*/
static long CIN_FrameForTime( int handle, int now ) {
	const auto &movie = cinTable[handle];
	const auto elapsed = static_cast<std::uint32_t>( now ) -
		static_cast<std::uint32_t>( movie.startTime );
	if ( elapsed > INT_MAX ) return 0; // A clock preceding this movie's origin.
	// SP advertises its movie rate in the RoQ header (wolfintro uses 24).
	// Original MP and the shared Q3/QL path retain their 30 fps contract.
	const auto rate = Com_GameIsRTCWSP() ? movie.roqFPS : 30;
	const std::uint64_t frame = static_cast<std::uint64_t>( elapsed ) * rate / 1000;
	return static_cast<long>( std::min<std::uint64_t>( frame, INT_MAX ) );
}

e_status CIN_RunCinematic( int handle )
{
	int start = 0;
	int thisTime = 0;

	if (handle < 0 || handle>= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF) return FMV_EOF;

	if (cin.currentHandle != handle) {
		currentHandle = handle;
		cin.currentHandle = currentHandle;
		cinTable[currentHandle].status = FMV_EOF;
		RoQReset();
	}

	if (cinTable[handle].playonwalls < -1)
	{
		return cinTable[handle].status;
	}

	currentHandle = handle;

	if (cinTable[currentHandle].alterGameState) {
		if ( cls.state != CA_CINEMATIC ) {
			return cinTable[currentHandle].status;
		}
	}

	if (cinTable[currentHandle].status == FMV_IDLE) {
		return cinTable[currentHandle].status;
	}

	thisTime = CL_ScaledMilliseconds();
	const auto delta = static_cast<std::uint32_t>( thisTime ) -
		static_cast<std::uint32_t>( cinTable[currentHandle].lastTime );
	// Before the first client frame the clock uses system time. Rebase when
	// client time starts (or rewinds), retaining the movie's elapsed time.
	// A regular 32-bit clock wrap has a small positive unsigned delta.
	if ( delta > INT_MAX || ( cinTable[currentHandle].shader && delta > 100 ) ) {
		cinTable[currentHandle].startTime = static_cast<int>(
			static_cast<std::uint32_t>( cinTable[currentHandle].startTime ) + delta );
	}
	cinTable[currentHandle].tfps = CIN_FrameForTime( currentHandle, thisTime );

	start = cinTable[currentHandle].startTime;
	// Console startup waits synchronously for a first frame while cls.realtime
	// is fixed. Consume setup/audio chunks even when the target time is zero.
	while(  (cinTable[currentHandle].numQuads < 1 ||
		cinTable[currentHandle].tfps > cinTable[currentHandle].numQuads)
		&& (cinTable[currentHandle].status == FMV_PLAY) ) 
	{
		RoQInterrupt();
		if (start != cinTable[currentHandle].startTime) {
			thisTime = CL_ScaledMilliseconds();
			cinTable[currentHandle].tfps = CIN_FrameForTime( currentHandle, thisTime );
			start = cinTable[currentHandle].startTime;
		}
	}

	cinTable[currentHandle].lastTime = thisTime;

	if (cinTable[currentHandle].status == FMV_LOOPED) {
		cinTable[currentHandle].status = FMV_PLAY;
	}

	if (cinTable[currentHandle].status == FMV_EOF) {
	  if (cinTable[currentHandle].looping) {
		RoQReset();
	  } else {
		RoQShutdown();
		return FMV_EOF;
	  }
	}

	return cinTable[currentHandle].status;
}


/*
==================
CIN_PlayCinematic
==================
*/
int CIN_PlayCinematic( const char *arg, int x, int y, int w, int h, int systemBits ) {
	unsigned short RoQID;
	std::array<char, MAX_OSPATH> name;
	int		i;

	if ( CL_ShouldSkipIdLogoCinematic( arg )
		&& ( ( systemBits & CIN_system ) != 0 || cls.state < CA_LOADING ) ) {
		if ( ( systemBits & CIN_system ) != 0 ) {
			CL_RunCinematicNextmap();
		}
		return -1;
	}

	if (strchr(arg, '/') == nullptr && strchr(arg, '\\') == nullptr) {
		Com_sprintf( name.data(), static_cast<int>( name.size() ), "video/%s", arg );
	} else {
		Com_sprintf( name.data(), static_cast<int>( name.size() ), "%s", arg );
	}

	if (!(systemBits & CIN_system)) {
		for ( i = 0 ; i < MAX_VIDEO_HANDLES ; i++ ) {
			if (!strcmp(cinTable[i].fileName.data(), name.data()) ) {
				return i;
			}
		}
	}

	Com_DPrintf("CIN_PlayCinematic( %s )\n", arg);

	// The decoder cache is several MiB. Aggregate assignment creates a stack
	// temporary with MSVC; reset the trivially stored decoder state in place.
	Com_Memset( &cin, 0, sizeof( cin ) );
	currentHandle = CIN_HandleForVideo();

	cin.currentHandle = currentHandle;

	Q_strncpyz( cinTable[currentHandle].fileName.data(), name.data(), static_cast<int>( cinTable[currentHandle].fileName.size() ) );

	cinTable[currentHandle].ROQSize = OpenFileRead(
		cinTable[currentHandle].fileName.data(),
		cinTable[currentHandle].iFile,
		qtrue );

	if (cinTable[currentHandle].ROQSize < 16) {
		Com_DPrintf("play(%s), missing or truncated RoQ file header\n", arg);
		cinTable[currentHandle].fileName[0] = '\0';
		cinTable[currentHandle].iFile.reset();
		cinTable[currentHandle].buf = nullptr;
		cinTable[currentHandle].status = FMV_IDLE;
		return -1;
	}
	cinTable[currentHandle].buf = nullptr;

	CIN_SetExtents(currentHandle, x, y, w, h);
	CIN_SetLooping(currentHandle, ToQboolean( (systemBits & CIN_loop) != 0 ) );

	cinTable[currentHandle].CIN_HEIGHT = DEFAULT_CIN_HEIGHT;
	cinTable[currentHandle].CIN_WIDTH  =  DEFAULT_CIN_WIDTH;
	cinTable[currentHandle].holdAtEnd = (systemBits & CIN_hold) != 0;
	cinTable[currentHandle].alterGameState = (systemBits & CIN_system) != 0;
	cinTable[currentHandle].playonwalls = 1;
	cinTable[currentHandle].silent = (systemBits & CIN_silent) != 0;
	cinTable[currentHandle].shader = (systemBits & CIN_shader) != 0;
	cinTable[currentHandle].rtcwLetterBox = Com_GameIsRTCWSP() &&
		(systemBits & RTCW_SP_CIN_LETTERBOX) != 0;

	if (cinTable[currentHandle].alterGameState) {
		// close the menu
		if ( uivm ) {
			VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_NONE );
		}
	} else {
		cinTable[currentHandle].playonwalls = cl_inGameVideo->integer;
	}

	initRoQ();
					
	if ( FileRead( cinTable[currentHandle].iFile.get(), cin.file.data(), 16 ) != 16 ) {
		Com_DPrintf("trFMV::play(), truncated RoQ file header\n");
		RoQShutdown();
		return -1;
	}

	RoQID = static_cast<unsigned short>( cin.file[0] ) + static_cast<unsigned short>( cin.file[1] ) * 256;
	if (RoQID == 0x1084 && RoQ_init())
	{
//		FS_Read (cin.file, cinTable[currentHandle].RoQFrameSize+8, cinTable[currentHandle].iFile);

		cinTable[currentHandle].status = FMV_PLAY;
		Com_DPrintf("trFMV::play(), playing %s\n", arg);

		if (cinTable[currentHandle].alterGameState) {
			cls.state = CA_CINEMATIC;
		}
		
		Con_Close();

		if ( !cinTable[currentHandle].silent ) {
			s_rawend = s_soundtime;
		}

		return currentHandle;
	}
	Com_DPrintf("trFMV::play(), invalid RoQ ID or frame size\n");

	RoQShutdown();
	return -1;
}


void CIN_SetExtents( int handle, int x, int y, int w, int h ) {
	if (handle < 0 || handle>= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF) return;
	cinTable[handle].xpos = x;
	cinTable[handle].ypos = y;
	cinTable[handle].width = w;
	cinTable[handle].height = h;
	cinTable[handle].dirty = true;
}


static void CIN_SetLooping( int handle, qboolean loop ) {
	if (handle < 0 || handle>= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF) return;
	cinTable[handle].looping = loop != qfalse;
}


/*
==================
CIN_ResampleCinematic

Resample cinematic to 256x256 and store in buf2
==================
*/
static void CIN_ResampleCinematic( int handle, int *buf2 ) {
	int ix, iy, *buf3, xm, ym, ll;
	byte	*buf;

	buf = cinTable[handle].buf;

	xm = cinTable[handle].CIN_WIDTH/256;
	ym = cinTable[handle].CIN_HEIGHT/256;
	ll = 8;
	if (cinTable[handle].CIN_WIDTH==512) {
		ll = 9;
	}

	buf3 = reinterpret_cast<int *>( buf );
	if (xm==2 && ym==2) {
		byte *bc2, *bc3;
		int	ic, iiy;

		bc2 = AsWritableBytes( buf2 );
		bc3 = AsWritableBytes( buf3 );
		for (iy = 0; iy<256; iy++) {
			iiy = iy<<12;
			for (ix = 0; ix<2048; ix+=8) {
				for(ic = ix;ic<(ix+4);ic++) {
					*bc2=(bc3[iiy+ic]+bc3[iiy+4+ic]+bc3[iiy+2048+ic]+bc3[iiy+2048+4+ic])>>2;
					bc2++;
				}
			}
		}
	} else if (xm==2 && ym==1) {
		byte *bc2, *bc3;
		int	ic, iiy;

		bc2 = AsWritableBytes( buf2 );
		bc3 = AsWritableBytes( buf3 );
		for (iy = 0; iy<256; iy++) {
			iiy = iy<<11;
			for (ix = 0; ix<2048; ix+=8) {
				for(ic = ix;ic<(ix+4);ic++) {
					*bc2=(bc3[iiy+ic]+bc3[iiy+4+ic])>>1;
					bc2++;
				}
			}
		}
	} else {
		for (iy = 0; iy<256; iy++) {
			for (ix = 0; ix<256; ix++) {
					buf2[(iy<<8)+ix] = buf3[((iy*ym)<<ll) + (ix*xm)];
			}
		}
	}
}


/*
==================
CIN_DrawCinematic
==================
*/
static void CIN_DrawCinematicInternal( int handle, qboolean uniformAspect ) {
	float	x, y, w, h;
	byte	*buf;

	if (handle < 0 || handle>= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF) return;

	if (!cinTable[handle].buf) {
		return;
	}

	x = cinTable[handle].xpos;
	y = cinTable[handle].ypos;
	w = cinTable[handle].width;
	h = cinTable[handle].height;
	buf = cinTable[handle].buf;

	if ( uniformAspect ) {
		SCR_AdjustFrom640Uniform( &x, &y, &w, &h );
	} else {
		SCR_AdjustFrom640( &x, &y, &w, &h );
	}

	if ( cinTable[handle].rtcwLetterBox ) {
		const float screenHeight = static_cast<float>( cls.glconfig.vidHeight );
		const float barHeight = ( RTCW_SP_CIN_BAR_HEIGHT / 480.0f ) * screenHeight;
		re.SetColor( colorBlack );
		// SP anchors bars to the screen, using the scaled movie width. The
		// bottom bar overlaps one pixel to cover fractional-resolution seams.
		re.DrawStretchPic( 0, 0, w, barHeight, 0, 0, 0, 0, cls.whiteShader );
		re.DrawStretchPic( 0, screenHeight - barHeight - 1, w, barHeight + 1,
			0, 0, 0, 0, cls.whiteShader );
		re.SetColor( nullptr );
	}

	if (cinTable[handle].dirty && (cinTable[handle].CIN_WIDTH != cinTable[handle].drawX || cinTable[handle].CIN_HEIGHT != cinTable[handle].drawY)) {
		auto tempBuffer = ScopedTempMemory::Allocate( 256 * 256 * 4 );
		int *buf2 = tempBuffer.as<int>();

		CIN_ResampleCinematic(handle, buf2);

		re.DrawStretchRaw( x, y, w, h, 256, 256, reinterpret_cast<byte *>( buf2 ), handle, qtrue);
		cinTable[handle].dirty = false;
		return;
	}

	re.DrawStretchRaw( x, y, w, h, cinTable[handle].drawX, cinTable[handle].drawY, buf, handle, ToQboolean( cinTable[handle].dirty ) );
	cinTable[handle].dirty = false;
}


void CIN_DrawCinematic( int handle ) {
	CIN_DrawCinematicInternal( handle, qfalse );
}


void CIN_DrawCinematicUI( int handle ) {
	CIN_DrawCinematicInternal( handle, ToQboolean( !Com_GameIsRTCW() &&
		cl_cinematicAspect && cl_cinematicAspect->integer ) );
}


/*
=============================================================================

American McGee's Alice's cinematic player (client_alice_cinematic.hpp holds
its steps and their retail addresses). Its movies are the system cinematic:
the intro's logos and New Game's opening. It never loops one; the end runs
`nextmap` as a map and resumes the engine UI's intro.

=============================================================================
*/

// client_alice_ui.hpp's, which cannot follow client.h into this file.
bool CL_AliceLoadingActive( void );
void CL_AliceCinematicPlay( const char *name, const char *flags );

// The movie playing now: whether its end resumes the intro, and where.
static fntech3::alice::cinematic::Record aliceCinematic;

struct AliceCinematicHost {
	bool Playing() const { return cls.state == CA_CINEMATIC; }
	void StartHunkUsers() { CL_StartHunkUsers(); }
	void StopAllSounds() {
		S_StopAllSounds();
		// Alice's cinematic start frees the music (alice.exe 0x00427303).
		S_AliceMusicFreeAll();
	}
	int VideoLength( const char *path ) { return FS_FOpenFileRead( path, nullptr, qfalse ); }
	void MissingVideo( const char *name ) { Com_DPrintf( "play(%s), ROQSize<=0\n", name ); }
	/* Retail's SV_Shutdown (0x00455910) never disconnects the local client. It
	 * sends the final `print` and `disconnect` (SV_FinalMessage 0x004558a0),
	 * shuts the game down and clears the server; the client keeps its cgame
	 * and its connection state until CIN_Play puts it at CA_CINEMATIC
	 * (0x0042750d) and the movie's end at CA_DISCONNECTED (CIN_Finish
	 * 0x004257fe). The stale connection goes with the next CL_Disconnect
	 * (0x0042abb0), which the next map load runs first (0x0042ab26). This
	 * engine's SV_Shutdown disconnects the local client unless sv_killserver
	 * is 2, the request CL_PlayDemo_f makes for the same reason. */
	void ShutdownServer() {
		Cvar_Set( "sv_killserver", "2" );
		SV_Shutdown( "" );
		Cvar_Set( "sv_killserver", "0" );
	}
	// The MP3 is read before the movie's header (alice.exe 0x0042745d).
	bool PreloadSound( const char *path ) { return S_AliceMP3Preload( path ) != qfalse; }
	void SoundNotPreloaded( const char *path ) {
		Com_Printf( "ERROR: Could not preload mp3 file %s.\n", path );
	}
	bool Open( const char *path, bool holdAtEnd ) {
		// The system cinematic, held at its end or not, and never looped.
		const int bits = CIN_system | ( holdAtEnd ? CIN_hold : 0 );
		CL_handle = CIN_PlayCinematic( path, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, bits );
		return CL_handle >= 0;
	}
	void BadHeader( const char *name ) {
		Com_DPrintf( "trFMV::play(), invalid RoQ ID\n" );
		(void)name;
		// The preloaded sound is not kept for a movie that never started.
		S_AliceMP3Free();
	}
	void Opened( const char *name ) { Com_Printf( "trFMV::play(), playing %s\n", name ); }
	void ClearSoundBuffer() { S_ClearSoundBuffer(); }
	void DecodeFirstFrame() {
		// A malformed movie may close its handle before producing a first frame.
		do {
			SCR_RunCinematic();
		} while ( CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES &&
			cinTable[CL_handle].buf == nullptr && cinTable[CL_handle].status == FMV_PLAY );
	}
	// Once its first frame is decoded, for a single pass (0x00427569).
	bool PlaySound() { return S_AliceMP3PlayPreloaded( qfalse ) != qfalse; }
	void SoundNotPlayed() { Com_Printf( "ERROR: Could not play preloaded MP3.\n" ); }

	std::string NextMap() const { return Cvar_VariableString( "nextmap" ); }
	void Append( const char *text ) { Cbuf_ExecuteText( EXEC_APPEND, text ); }
	void ClearNextMap() { Cvar_Set( "nextmap", "" ); }
	bool MapLoading() const { return CL_AliceLoadingActive(); }
	void SetIntroState( int state ) { CL_AliceUISetIntroState( state ); }
};

/*
====================
CL_AliceCinematicPlay

CIN_Play (alice.exe 0x004272e0), from the `cinematic` command and from the
engine UI's intro.
====================
*/
void CL_AliceCinematicPlay( const char *name, const char *flags ) {
	AliceCinematicHost host;
	fntech3::alice::cinematic::Play( host, aliceCinematic, name, flags );
}

/*
====================
CL_AliceStopCinematic

CIN_Stop (alice.exe 0x00425770) as the server's `map`, `loadgame` and
`loadbrokengame` call it before they spawn (0x00451118, 0x00451c53,
0x00451b09): nothing unless a cinematic plays.
====================
*/
void CL_AliceStopCinematic( void ) {
	if ( cls.state == CA_CINEMATIC ) {
		SCR_StopCinematic();
	}
}

/*
====================
CL_AliceCinematicEnd

The end of Alice's system cinematic, at EOF or when stopped: RoQShutdown has
closed it and put the client back at CA_DISCONNECTED (CIN_Finish 0x004257a0).
====================
*/
static void CL_AliceCinematicEnd( void ) {
	AliceCinematicHost host;
	fntech3::alice::cinematic::End( host, aliceCinematic );
}

void CL_PlayCinematic_f( void ) {
	const char	*arg, *s;
	int bits = CIN_system;

	arg = Cmd_Argv( 1 );
	s = Cmd_Argv(2);

	// Alice's `cinematic` (0x004276c0) ignores a request while one plays.
	if ( Com_GameIsAlice() ) {
		if ( cls.state != CA_CINEMATIC ) {
			Com_DPrintf( "SCR_PlayCinematic( %s )\n", arg );
			CL_AliceCinematicPlay( arg, s );
		}
		return;
	}

	Com_DPrintf("CL_PlayCinematic_f\n");

	if ( CL_ShouldSkipIdLogoCinematic( arg ) ) {
		CL_RunCinematicNextmap();
		return;
	}

	if (cls.state == CA_CINEMATIC) {
		SCR_StopCinematic();
	}

	if ((s && s[0] == '1') || Q_stricmp(arg,"demoend.roq")==0 || Q_stricmp(arg,"end.roq")==0) {
		bits |= CIN_hold;
	}
	if (s && s[0] == '2') {
		bits |= CIN_loop;
	}
	const bool letterBox = Com_GameIsRTCWSP() && s && s[0] == '3';
	if ( letterBox ) bits |= RTCW_SP_CIN_LETTERBOX;

	S_StopAllSounds ();

	const int inset = letterBox ? RTCW_SP_CIN_BAR_HEIGHT : 0;
	CL_handle = CIN_PlayCinematic( arg, 0, inset, SCREEN_WIDTH, SCREEN_HEIGHT - 2 * inset, bits );
	if (CL_handle >= 0) {
		do {
			SCR_RunCinematic();
		} while ( CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES &&
			cinTable[CL_handle].buf == nullptr && cinTable[CL_handle].status == FMV_PLAY );
		// A malformed movie may close its handle before producing a first frame.
	} else {
		// An optional logo may be absent from an installed release. Continue
		// the queued movie/map instead of abandoning the startup sequence.
		CL_RunCinematicNextmap();
	}
}


void SCR_DrawCinematic( void ) {
	if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES) {
		CIN_DrawCinematicUI( CL_handle );
	}
}


void SCR_RunCinematic( void ) {
	if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES) {
		CIN_RunCinematic(CL_handle);
	}
}


void SCR_StopCinematic( void ) {
	// Alice's CIN_Stop (0x00425770): a movie stopped while it plays ends
	// without resuming the intro where its flags said; the intro goes to its
	// main menu instead.
	if ( Com_GameIsAlice() ) {
		fntech3::alice::cinematic::Stop( cls.state == CA_CINEMATIC, aliceCinematic );
	}
	if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES) {
		CIN_StopCinematic(CL_handle);
		S_StopAllSounds ();
		CL_handle = -1;
	}
}


void CIN_UploadCinematic( int handle ) {
	if (handle >= 0 && handle < MAX_VIDEO_HANDLES) {
		if (!cinTable[handle].buf) {
			return;
		}
		if (cinTable[handle].playonwalls <= 0 && cinTable[handle].dirty) {
			if (cinTable[handle].playonwalls == 0) {
				cinTable[handle].playonwalls = -1;
			} else {
				if (cinTable[handle].playonwalls == -1) {
					cinTable[handle].playonwalls = -2;
				} else {
					cinTable[handle].dirty = false;
				}
			}
		}

		// Resample the video if needed
		if (cinTable[handle].dirty && (cinTable[handle].CIN_WIDTH != cinTable[handle].drawX || cinTable[handle].CIN_HEIGHT != cinTable[handle].drawY))  {
			auto tempBuffer = ScopedTempMemory::Allocate( 256 * 256 * 4 );
			int *buf2 = tempBuffer.as<int>();

			CIN_ResampleCinematic(handle, buf2);

			re.UploadCinematic( cinTable[handle].CIN_WIDTH, cinTable[handle].CIN_HEIGHT, 256, 256,
					reinterpret_cast<byte *>( buf2 ), handle, qtrue);
			cinTable[handle].dirty = false;
		} else {
			// Upload video at normal resolution
			re.UploadCinematic( cinTable[handle].CIN_WIDTH, cinTable[handle].CIN_HEIGHT, cinTable[handle].drawX, cinTable[handle].drawY,
					cinTable[handle].buf, handle, ToQboolean( cinTable[handle].dirty ) );
			cinTable[handle].dirty = false;
		}

		if (cl_inGameVideo->integer == 0 && cinTable[handle].playonwalls == 1) {
			cinTable[handle].playonwalls--;
		}
		else if (cl_inGameVideo->integer != 0 && cinTable[handle].playonwalls != 1) {
			cinTable[handle].playonwalls = 1;
		}
	}
}
