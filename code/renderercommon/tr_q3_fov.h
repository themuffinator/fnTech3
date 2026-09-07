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

/* Preserve the vertical framing of Q3's authored 4:3 horizontal FOV.
 * A preserve-aspect presentation uses render pixels; a stretched presentation
 * also accounts for the physical display aspect. Invalid inputs are inert. */
static void R_CorrectQ3ViewportFov( int width, int height,
	int renderWidth, int renderHeight, float displayAspect, qboolean preserveAspect,
	float *fovX, float *fovY ) {
	const double authoredAspect = 4.0 / 3.0;
	const double degreesPerRadian = 57.2957795130823208768;
	double aspect, verticalTangent;
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
	if ( !( aspect > 0.0 && aspect <= DBL_MAX )
		|| fabs( aspect - authoredAspect ) < 0.001 ) {
		return;
	}
	verticalTangent = tan( (double)*fovX / ( 2.0 * degreesPerRadian ) ) / authoredAspect;
	*fovX = (float)( 2.0 * degreesPerRadian * atan( verticalTangent * aspect ) );
	*fovY = (float)( 2.0 * degreesPerRadian * atan( verticalTangent ) );
}

/* The canonical ROM/INIT profile mirror is set before renderer startup.
 * Cache it once per renderer module, so an unrelated cvar cannot choose the
 * projection contract per frame. QL never registers or applies this option. */
static void R_ApplyProfileFovCorrection( const refdef_t *fd,
	float *fovX, float *fovY ) {
	static qboolean initialized;
	static cvar_t *correction;
	qboolean preserveAspect = qfalse;
	if ( !initialized ) {
		initialized = qtrue;
		if ( !Q_stricmp( ri.Cvar_VariableString( "com_gameProfile" ), "quake3" ) ) {
			correction = ri.Cvar_Get( "r_fovCorrection", "1", CVAR_ARCHIVE_ND );
			ri.Cvar_CheckRange( correction, "0", "1", CV_INTEGER );
			ri.Cvar_SetDescription( correction,
				"Fit Q3's authored 4:3 FOV to the viewport. Disable when game code supplies corrected FOV." );
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
	R_CorrectQ3ViewportFov( fd->width, fd->height,
		glConfig.vidWidth, glConfig.vidHeight, glConfig.windowAspect,
		preserveAspect, fovX, fovY );
}

#endif /* FNTECH3_TR_Q3_FOV_H */
