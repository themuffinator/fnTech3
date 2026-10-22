/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_CSM_MODEL_LIGHT_H
#define FNTECH3_TR_CSM_MODEL_LIGHT_H

/* The light grid folds many lights into one direction. Its projection onto
 * the authored sun is an estimate, not a recovered light list. Keep only
 * that part, bounded by the sun, before adding dynamic lights. Ambient and
 * local lights must not disappear when a model enters a sun shadow. */
static ID_INLINE void R_CSMCaptureEntitySun( trRefEntity_t *ent ) {
	vec3_t direction;
	float alignment, scale;
	int i, bits;

	VectorClear( ent->csmSunLight );
	if ( !tr.worldSun.valid || !r_csmShadows || !r_csmShadows->integer ) return;
	VectorNormalize2( ent->lightDir, direction );
	alignment = Com_Clamp( 0.0f, 1.0f, DotProduct( direction, tr.worldSun.directionToSun ) );
	bits = r_mapOverBrightBits->integer - tr.overbrightBits;
	scale = (float)ldexp( 1.0, bits < -16 ? -16 : bits > 16 ? 16 : bits );
	if ( tr.worldSun.unitColour ) {
		const float length = VectorLength( tr.worldSun.color );
		if ( length > 0.0f ) scale /= length;
	}
	for ( i = 0; i < 3; ++i ) {
		ent->csmSunLight[i] = MAX( 0.0f, MIN( ent->directedLight[i] * alignment,
			tr.worldSun.light[i] * scale ) );
	}
}

/* A multiplicative receiver pass needs the removable fraction, not the
 * total light. Match the diffuse pass's byte saturation, retain its ambient
 * floor, and remove no more than the estimated sun-facing contribution.
 * The fraction travels in the existing colour stream; no extra GPU fetch. */
static ID_INLINE void R_CSMModelShadowColor( const trRefEntity_t *ent,
	const float *normal, const vec3_t toSun, byte *out ) {
	const float incoming = MAX( 0.0f, DotProduct( normal, ent->lightDir ) );
	const float facing = MAX( 0.0f, DotProduct( normal, toSun ) );
	int i;

	for ( i = 0; i < 3; ++i ) {
		const float ambient = Com_Clamp( 0.0f, 255.0f, ent->ambientLight[i] );
		const float total = Com_Clamp( ambient, 255.0f, ambient + ent->directedLight[i] * incoming );
		const float sun = MIN( total - ambient, ent->csmSunLight[i] * facing );
		out[i] = (byte)( 255.0f * Com_Clamp( 0.0f, 1.0f, sun / MAX( total, 1.0f ) ) );
	}
	out[3] = 255;
}

#endif
