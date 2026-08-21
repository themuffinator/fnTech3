/*
===========================================================================
Copyright (C) 2026 FnQL contributors

Shared, renderer-only underwater view helpers.  Keep this header independent
of a specific backend so the OpenGL-lineage and Vulkan compositors classify,
age, and evaluate the submerged view in exactly the same way.

The underwater view is a visual-only post-process layer.  It never changes
BSP data, collision, contents flags, visibility, prediction, snapshots,
protocol data, demo data, or VM/native-module interfaces.
===========================================================================
*/
#ifndef TR_UNDERWATER_H
#define TR_UNDERWATER_H

#include "../qcommon/q_shared.h"

#define UNDERWATER_CONTENTS_MASK ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA )

/* Entering and leaving a liquid ramps the whole layer instead of switching it
 * on for one frame: an instant full-strength warp reads as a rendering glitch
 * rather than as a surface crossing.  A scene-time gap larger than the reset
 * window (a load, a pause, a demo seek) snaps to the target instead of playing
 * a stale ramp from an unrelated moment. */
#define UNDERWATER_TRANSITION_MSEC 180.0f
#define UNDERWATER_TRANSITION_RESET_MSEC 500

/* Displacement is authored in pixels at a 1080-line reference view and scaled
 * by the real viewport height, so the warp keeps the same angular size at
 * every resolution.  r_underwaterWarp is a plain 0..2 multiplier of the base. */
#define UNDERWATER_REFERENCE_VIEW_HEIGHT 1080.0f
#define UNDERWATER_WARP_BASE_PIXELS 11.0f
#define UNDERWATER_WARP_SCALE_MAX 2.0f

/*
Screen-space wave field, evaluated per fragment on every backend.

  phase[i]  = dot( WAVE_K[i].xy, screen ) + WAVE_K[i].w * time
  field    += WAVE_DIR_AMP[i].xy * sin( phase[i] )
  |field|  <= 1

`screen` is the fragment's normalized viewport coordinate with x multiplied by
the viewport aspect ratio, so it is measured in view heights on both axes and
the wave field stays isotropic on any aspect.  Each direction vector is unit
length and the three amplitudes sum to exactly one, which bounds the field to
the unit disc and therefore bounds the displacement to the authored pixel
count.

The field is deliberately anchored to the screen rather than to the world.
This layer models light bending as it passes through the water immediately in
front of the eye, so the pattern belongs to the viewer, not to the scene; an
angle-anchored field would slide the crests across the view whenever the
camera turned, which reads as a smeared overlay instead of a wet lens.

Every octave speed is a multiple of 0.05 rad/s, so all octaves share the
common period 2*pi/0.05.  Backends wrap scene time to that period before
uploading it, which keeps trigonometric arguments small for low-precision
fragment paths without introducing a visible phase jump.  This matches the
liquid-surface wave model in tr_liquid.h; the two fields are independent but
share the wrapping contract.
*/
#define UNDERWATER_WAVE_OCTAVES 3
#define UNDERWATER_WAVE_TIME_PERIOD 125.66370614  /* 40*pi seconds */

static ID_INLINE float R_UnderwaterWaveTime( double sceneTime )
{
	return (float)fmod( sceneTime, UNDERWATER_WAVE_TIME_PERIOD );
}

static ID_INLINE void R_UnderwaterWaveOctave( int octave, vec4_t phaseCoeffs, vec3_t dirAmp )
{
	/* xy: screen-space wave vector (rad per view height), z: unused,
	 * w: angular speed (rad/s) */
	static const vec4_t waveK[UNDERWATER_WAVE_OCTAVES] = {
		{   6.10f,   7.40f, 0.0f, 1.15f },
		{ -13.80f,   9.90f, 0.0f, 1.70f },
		{  12.20f, -26.40f, 0.0f, 2.60f },
	};
	/* xy: unit displacement direction scaled by the octave amplitude.  The
	 * magnitudes are 0.55, 0.30, and 0.15 and therefore sum to one, which is
	 * what bounds the field to the unit disc; keep them exact if these are
	 * ever retuned. */
	static const vec3_t waveDirAmp[UNDERWATER_WAVE_OCTAVES] = {
		{  0.342234f,  0.430553f, 0.0f },	/* normalize( 0.62,  0.78) * 0.55 */
		{ -0.242491f,  0.176629f, 0.0f },	/* normalize(-0.81,  0.59) * 0.30 */
		{  0.062859f, -0.136194f, 0.0f },	/* normalize( 0.42, -0.91) * 0.15 */
	};

	if ( octave < 0 || octave >= UNDERWATER_WAVE_OCTAVES ) {
		octave = 0;
	}
	Vector4Copy( waveK[octave], phaseCoeffs );
	VectorCopy( waveDirAmp[octave], dirAmp );
}

static ID_INLINE void R_UnderwaterWaveField( float screenX, float screenY,
	float time, float *fieldX, float *fieldY )
{
	vec4_t k;
	vec3_t dirAmp;
	int i;

	*fieldX = 0.0f;
	*fieldY = 0.0f;
	for ( i = 0; i < UNDERWATER_WAVE_OCTAVES; i++ ) {
		float wave;

		R_UnderwaterWaveOctave( i, k, dirAmp );
		wave = sinf( k[0] * screenX + k[1] * screenY + k[3] * time );
		*fieldX += dirAmp[0] * wave;
		*fieldY += dirAmp[1] * wave;
	}
}

static ID_INLINE float R_UnderwaterViewHeightScale( int viewportHeight )
{
	return viewportHeight > 0 ?
		(float)viewportHeight / UNDERWATER_REFERENCE_VIEW_HEIGHT : 1.0f;
}

static ID_INLINE float R_UnderwaterWarpPixels( float warpScale, int viewportHeight )
{
	return Com_Clamp( 0.0f, UNDERWATER_WARP_SCALE_MAX, warpScale ) *
		UNDERWATER_WARP_BASE_PIXELS * R_UnderwaterViewHeightScale( viewportHeight );
}

/* Water refracts short wavelengths slightly more than long ones.  Splitting
 * the three channels across a fraction of the shared displacement is what
 * separates a plausible medium from a flat wobble, but the split has to stay
 * small: past a few percent of the warp it stops reading as dispersion and
 * starts reading as a broken colour channel. */
#define UNDERWATER_DISPERSION_MAX 0.22f

static ID_INLINE float R_UnderwaterDispersion( float dispersionScale )
{
	return Com_Clamp( 0.0f, 1.0f, dispersionScale ) * UNDERWATER_DISPERSION_MAX;
}

/* Sampling the warped colour has to stay inside the target.  Clamping to the
 * texel-centre interval keeps the edge stretched rather than wrapped, which is
 * the conventional and least distracting boundary for a full-screen warp. */
#define UNDERWATER_SAMPLE_MIN 0.002f
#define UNDERWATER_SAMPLE_MAX 0.998f

/* Light reaching the eye falls off toward the edge of the field of view under
 * water.  The radius is measured in unscaled normalized device coordinates, so
 * the darkening follows the screen ellipse instead of a circle inscribed in
 * it; a circular falloff on a wide viewport visibly darkens the sides only. */
#define UNDERWATER_VIGNETTE_INNER 0.55f
#define UNDERWATER_VIGNETTE_OUTER 1.30f

static ID_INLINE float R_UnderwaterVignette( float ndcX, float ndcY, float strength )
{
	const float radius = sqrtf( ndcX * ndcX + ndcY * ndcY );
	const float span = UNDERWATER_VIGNETTE_OUTER - UNDERWATER_VIGNETTE_INNER;
	const float t = Com_Clamp( 0.0f, 1.0f, ( radius - UNDERWATER_VIGNETTE_INNER ) / span );

	return 1.0f - strength * ( t * t * ( 3.0f - 2.0f * t ) );
}

/*
Beer-Lambert absorption against the opaque scene depth.  This is the single
largest contributor to a convincing submerged view: a flat colour wash reads as
a coloured filter over a dry scene, while distance-weighted absorption puts the
geometry inside a medium.  Nearby surfaces stay legible and distant ones fade
into the water colour, which is also why the layer is bounded below full
opacity - a saturated screen would hide gameplay-relevant contrast.
*/
#define UNDERWATER_FOG_MAX_OPACITY 0.92f

static ID_INLINE float R_UnderwaterAbsorption( float distance, float density )
{
	if ( distance <= 0.0f || density <= 0.0f ) {
		return 0.0f;
	}
	return 1.0f - expf( -density * distance );
}

/* Nonlinear window depth to eye-space distance.  The two depth conventions
 * differ only in which end of the range the stored value grows toward, which
 * the backends select with a flag rather than with two formulas. */
static ID_INLINE float R_UnderwaterSceneDistance( float depth, float zNear,
	float zFar, qboolean reversedDepth )
{
	float span;
	float denominator;

	if ( zNear < 0.0001f ) {
		zNear = 0.0001f;
	}
	if ( zFar < zNear + 0.0001f ) {
		zFar = zNear + 0.0001f;
	}
	span = zFar - zNear;
	denominator = reversedDepth ? zNear + depth * span : zFar - depth * span;
	if ( denominator < 0.0001f ) {
		denominator = 0.0001f;
	}
	return zNear * zFar / denominator;
}

/*
Per-medium appearance.  Water is a clear blue-green that stays readable at
short range; slime is murkier and loses the scene within a room; lava is
effectively opaque and only its own glow survives, so its warp is the
strongest of the three because it stands in for heat shimmer rather than
refraction.  Densities are per world unit.
*/
static ID_INLINE int R_UnderwaterClassifyContents( int contents )
{
	/* A point can carry more than one liquid bit.  Resolve to the densest
	 * medium so a water/lava overlap cannot look like clear water. */
	if ( contents & CONTENTS_LAVA ) {
		return CONTENTS_LAVA;
	}
	if ( contents & CONTENTS_SLIME ) {
		return CONTENTS_SLIME;
	}
	if ( contents & CONTENTS_WATER ) {
		return CONTENTS_WATER;
	}
	return 0;
}

static ID_INLINE void R_UnderwaterContentsColor( int contents, vec3_t color )
{
	if ( contents & CONTENTS_LAVA ) {
		color[0] = 0.55f;
		color[1] = 0.13f;
		color[2] = 0.02f;
	} else if ( contents & CONTENTS_SLIME ) {
		color[0] = 0.10f;
		color[1] = 0.22f;
		color[2] = 0.06f;
	} else {
		color[0] = 0.10f;
		color[1] = 0.28f;
		color[2] = 0.38f;
	}
}

static ID_INLINE float R_UnderwaterContentsDensity( int contents )
{
	if ( contents & CONTENTS_LAVA ) {
		return 0.0250f;
	}
	if ( contents & CONTENTS_SLIME ) {
		return 0.0060f;
	}
	return 0.0022f;
}

static ID_INLINE float R_UnderwaterContentsWarpScale( int contents )
{
	if ( contents & CONTENTS_LAVA ) {
		return 1.40f;
	}
	if ( contents & CONTENTS_SLIME ) {
		return 0.85f;
	}
	return 1.0f;
}

/*
====================
R_UnderwaterSceneColor

The medium colour is authored as a display-referred value, but the compositor
blends into the scene colour buffer, which the output transform still scales by
the overbright factor (and by the tone-map exposure in scene-linear mode).
Convert into that pre-output domain first, exactly as the global-fog layer
does; otherwise an authored mid-tone reaches the display at twice its intended
brightness and the medium reads as a uniform wash.

outputScale is the multiplier the output transform will apply.  sceneLinear
selects the linear-light scene buffer, where the authored sRGB value has to be
linearized as well.  The transfer function is duplicated rather than shared
with tr_global_fog.h so each layer's header stays independently includable.
====================
*/
static ID_INLINE float R_UnderwaterSrgbToLinear( float value )
{
	if ( value <= 0.0f ) {
		return 0.0f;
	}
	if ( value <= 0.04045f ) {
		return value / 12.92f;
	}
	if ( value < 1.0f ) {
		return powf( ( value + 0.055f ) / 1.055f, 2.4f );
	}
	return 1.0f;
}

static ID_INLINE void R_UnderwaterSceneColor( int contents, float outputScale,
	qboolean sceneLinear, vec3_t out )
{
	vec3_t authored;
	float scale;
	int i;

	if ( !out ) {
		return;
	}
	R_UnderwaterContentsColor( contents, authored );
	scale = ( outputScale > 0.0f ) ? 1.0f / outputScale : 1.0f;
	for ( i = 0; i < 3; i++ ) {
		out[i] = ( sceneLinear ? R_UnderwaterSrgbToLinear( authored[i] ) : authored[i] ) * scale;
	}
}

/*
====================
Submerged-view ramp state

The engine samples the eye's contents once per world scene and the renderer
ages the result here.  Only scene time drives the ramp, so a stereo pair or a
repeated submission at the same time cannot advance it twice, and a demo seek
backwards resets instead of playing a ramp from a future moment.
====================
*/
typedef struct underwaterViewState_s {
	int			sampleTime;
	int			contents;
	float		strength;
	qboolean	valid;
} underwaterViewState_t;

static ID_INLINE void R_UnderwaterResetView( underwaterViewState_t *state )
{
	if ( !state ) {
		return;
	}
	state->sampleTime = 0;
	state->contents = 0;
	state->strength = 0.0f;
	state->valid = qfalse;
}

static ID_INLINE float R_UnderwaterAdvanceView( underwaterViewState_t *state,
	int contents, int sceneTime )
{
	const int liquid = R_UnderwaterClassifyContents( contents );
	const float target = liquid ? 1.0f : 0.0f;
	int64_t elapsed;

	if ( !state ) {
		return 0.0f;
	}

	elapsed = (int64_t)sceneTime - (int64_t)state->sampleTime;
	if ( !state->valid || elapsed < 0 ||
		elapsed > UNDERWATER_TRANSITION_RESET_MSEC ) {
		state->contents = liquid;
		state->strength = target;
		state->sampleTime = sceneTime;
		state->valid = qtrue;
		return state->strength;
	}

	/* Swapping media while already submerged adopts the new colour and density
	 * immediately: both endpoints are liquids, so there is nothing to ramp
	 * between, and holding the previous medium would tint the wrong colour for
	 * as long as the ramp lasted. */
	if ( liquid && liquid != state->contents ) {
		state->contents = liquid;
	}

	if ( elapsed > 0 ) {
		const float step = (float)elapsed / UNDERWATER_TRANSITION_MSEC;

		state->strength = Com_Clamp( 0.0f, 1.0f,
			liquid ? state->strength + step : state->strength - step );
	}
	state->sampleTime = sceneTime;
	return state->strength;
}

#endif /* TR_UNDERWATER_H */
