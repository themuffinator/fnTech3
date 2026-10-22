/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Independently implemented from the viewport/FOV behavior of GPL-2.0-or-later
FnQuake3 at 7d8c9629cdc7979f09697266470359101ffd9eda. See CREDITS.md.
===========================================================================
*/
#ifndef FNTECH3_TR_Q3_FOV_H
#define FNTECH3_TR_Q3_FOV_H

#include <float.h>
#include <math.h>

/* Quake III's underwater warp (CG_CalcFov's WAVE_AMPLITUDE, kept by RTCW,
 * Enemy Territory, Elite Force and the Jedi games) adds a degree or so to the
 * horizontal field and takes as much from the vertical one, after deriving the
 * vertical from the horizontal for the shape the game drew at: its view's own
 * (a game told the real screen) or its 4:3 frame's (a game told a 4:3 one).
 * Answer that warp in degrees: the shape nearer the pair, then the base field
 * whose pair, warped, gives the same sum. A pair no warp of R_FOV_WARP_MAX
 * explains, or one within R_FOV_WARP_MIN of plain, answers 0. */
#define R_FOV_WARP_MIN 0.01
#define R_FOV_WARP_MAX 5.0
static double R_Q3ViewportFovWarp( double fovX, double fovY, double viewAspect, double frameAspect ) {
	const double degreesPerRadian = 57.2957795130823208768;
	double ratio, gameAspect, low, high, base = fovX, warp;
	int i;
	ratio = tan( fovY / ( 2.0 * degreesPerRadian ) ) / tan( fovX / ( 2.0 * degreesPerRadian ) );
	if ( !( ratio > 0.0 && ratio <= DBL_MAX ) ) {
		return 0.0;
	}
	gameAspect = fabs( log( ratio * viewAspect ) ) <= fabs( log( ratio * frameAspect ) ) ? viewAspect : frameAspect;
	low = fovX - R_FOV_WARP_MAX > 0.5 ? fovX - R_FOV_WARP_MAX : 0.5;
	high = fovX + R_FOV_WARP_MAX < 178.5 ? fovX + R_FOV_WARP_MAX : 178.5;
	/* base + vertical( base ) rises with base */
#define R_FOV_WARP_SUM( b ) ( ( b ) + 2.0 * degreesPerRadian * atan( tan( ( b ) / ( 2.0 * degreesPerRadian ) ) / gameAspect ) )
	if ( fovX + fovY < R_FOV_WARP_SUM( low ) || fovX + fovY > R_FOV_WARP_SUM( high ) ) {
		return 0.0;
	}
	for ( i = 0; i < 64; ++i ) {
		base = 0.5 * ( low + high );
		if ( R_FOV_WARP_SUM( base ) < fovX + fovY ) low = base;
		else high = base;
	}
#undef R_FOV_WARP_SUM
	warp = fovX - base;
	return fabs( warp ) < R_FOV_WARP_MIN || fabs( warp ) > R_FOV_WARP_MAX ? 0.0 : warp;
}

/* Preserve the vertical framing of a horizontal FOV authored for a 4:3
 * screen. A game sizes its view with the screen, so a view it cuts from the
 * screen, such as a letterboxed one, was authored at the shape that cut has
 * on a 4:3 screen and keeps that framing on any other; a view filling the
 * screen was authored at 4:3. A preserve-aspect presentation uses render
 * pixels; a stretched presentation also accounts for the physical display
 * aspect. The game's warp of the pair is carried over in degrees, as the
 * game applied it. Invalid inputs are inert. */
static void R_CorrectQ3ViewportFov( int width, int height,
	int renderWidth, int renderHeight, float displayAspect, qboolean preserveAspect,
	float *fovX, float *fovY ) {
	const double screenAspect = 4.0 / 3.0;
	const double degreesPerRadian = 57.2957795130823208768;
	double aspect, viewAspect, authoredAspect = screenAspect, verticalTangent, warp, x, y;
	if ( !fovX || !fovY || width <= 0 || height <= 0
		|| !( *fovX > 0.0f && *fovX < 179.0f )
		|| !( *fovY > 0.0f && *fovY < 179.0f ) ) {
		return;
	}
	aspect = viewAspect = (double)width / (double)height;
	if ( renderWidth > 0 && renderHeight > 0 ) {
		authoredAspect = aspect * screenAspect * (double)renderHeight / (double)renderWidth;
	}
	if ( !preserveAspect && renderWidth > 0 && renderHeight > 0
		&& displayAspect > 0.0f && displayAspect <= FLT_MAX ) {
		aspect *= (double)displayAspect * (double)renderHeight / (double)renderWidth;
	}
	if ( !( aspect > 0.0 && aspect <= DBL_MAX )
		|| !( authoredAspect > 0.0 && authoredAspect <= DBL_MAX )
		|| fabs( aspect - authoredAspect ) < 0.001 ) {
		return;
	}
	warp = R_Q3ViewportFovWarp( *fovX, *fovY, viewAspect, authoredAspect );
	verticalTangent = tan( ( (double)*fovX - warp ) / ( 2.0 * degreesPerRadian ) ) / authoredAspect;
	x = 2.0 * degreesPerRadian * atan( verticalTangent * aspect ) + warp;
	y = 2.0 * degreesPerRadian * atan( verticalTangent ) - warp;
	if ( !( x > 0.0 && x < 179.0 && y > 0.0 && y < 179.0 ) ) {
		x -= warp;
		y += warp;
	}
	*fovX = (float)x;
	*fovY = (float)y;
}

/* Quake Live's client game keeps a 4:3 view's vertical field and widens the
 * horizontal one for the r_aspectRatio preset its engine publishes: 4:3 (0),
 * 16:9 (1), 16:10 (2) or 5:4 (3). On any other shape the pair does not fit
 * the view: at 21:9 its 16:9 field is spread across 21:9, stretching the
 * world sideways. Its horizontal field is widened by the view's shape over the
 * preset's, keeping its vertical field and its underwater warp; on a preset's
 * own shape nothing changes. Invalid inputs are inert. */
static void R_CorrectQLViewportFov( int width, int height,
	int renderWidth, int renderHeight, float displayAspect, qboolean preserveAspect,
	int preset, float *fovX, const float *fovY ) {
	const double degreesPerRadian = 57.2957795130823208768;
	const double presetAspect = preset == 1 ? 16.0 / 9.0 : preset == 2 ? 16.0 / 10.0
		: preset == 3 ? 5.0 / 4.0 : 4.0 / 3.0;
	double aspect, factor;
	if ( !fovX || !fovY || width <= 0 || height <= 0
		|| !( *fovX > 0.0f && *fovX < 179.0f )
		|| !( *fovY > 0.0f && *fovY < 179.0f ) ) {
		return;
	}
	aspect = (double)width / (double)height;
	if ( !preserveAspect && renderWidth > 0 && renderHeight > 0
		&& displayAspect > 0.0f && displayAspect <= FLT_MAX ) {
		aspect *= (double)displayAspect * (double)renderHeight / (double)renderWidth;
	}
	if ( !( aspect > 0.0 && aspect <= DBL_MAX ) ) {
		return;
	}
	factor = aspect / presetAspect;
	if ( fabs( factor - 1.0 ) < 0.001 ) {
		return;
	}
	*fovX = (float)( 2.0 * degreesPerRadian * atan( tan( (double)*fovX / ( 2.0 * degreesPerRadian ) ) * factor ) );
}

/* The canonical ROM/INIT profile mirror is set before renderer startup.
 * Cache it once per renderer module, so an unrelated cvar cannot choose the
 * projection contract per frame. Every game but Quake Live authors its view
 * for a 4:3 screen -- the 1999 tests and both Jedi games included -- and takes
 * the 4:3 rule; Quake Live, whose modules fit their own view to four shapes,
 * takes its own rule for the rest. The retail QL cgame reads its field nowhere
 * else and projects points onto its 2D through the renderer's transforms, so
 * its markers follow the corrected view. */
static void R_ApplyProfileFovCorrection( const refdef_t *fd,
	float *fovX, float *fovY ) {
	static qboolean initialized;
	static qboolean quakeLive;
	static cvar_t *correction;
	static cvar_t *qlAspectPreset;
	qboolean preserveAspect = qfalse;
	if ( !initialized ) {
		initialized = qtrue;
		const char *profile = ri.Cvar_VariableString( "com_gameProfile" );
		quakeLive = !Q_stricmp( profile, "quakelive" ) ? qtrue : qfalse;
		correction = ri.Cvar_Get( "r_fovCorrection", "1", CVAR_ARCHIVE_ND );
		ri.Cvar_CheckRange( correction, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( correction, quakeLive
			? "Fit Quake Live's FOV to any screen shape, keeping its vertical framing: its own Hor+ fits only 4:3, 5:4, 16:10 and 16:9. 0 draws its FOV as retail did."
			: "Fit the game's 4:3-authored FOV to the viewport, keeping its vertical framing. Disable when game code supplies corrected FOV." );
		if ( quakeLive ) {
			// the preset the client publishes for the cgame (cl_main.cpp CL_InitRenderer)
			qlAspectPreset = ri.Cvar_Get( "r_aspectRatio", "0", 0 );
		}
	}
	if ( !correction || !correction->integer || !fd
		|| ( fd->rdflags & ( RDF_NOFOVCORRECTION | RDF_NOWORLDMODEL ) ) ) {
		return;
	}
#if defined( USE_FBO ) || defined( USE_VULKAN )
	if ( r_fbo && r_fbo->integer && r_renderScale && r_renderScale->integer > 0
		&& ( ( r_renderScale->integer - 1 ) & 1 )
		&& ( gls.windowWidth != glConfig.vidWidth || gls.windowHeight != glConfig.vidHeight ) ) {
		preserveAspect = qtrue;
	}
#endif
	if ( quakeLive ) {
		R_CorrectQLViewportFov( fd->width, fd->height,
			glConfig.vidWidth, glConfig.vidHeight, glConfig.windowAspect,
			preserveAspect, qlAspectPreset ? qlAspectPreset->integer : 0, fovX, fovY );
		return;
	}
	R_CorrectQ3ViewportFov( fd->width, fd->height,
		glConfig.vidWidth, glConfig.vidHeight, glConfig.windowAspect,
		preserveAspect, fovX, fovY );
}

#endif /* FNTECH3_TR_Q3_FOV_H */
