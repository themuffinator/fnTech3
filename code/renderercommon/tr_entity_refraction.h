/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_ENTITY_REFRACTION_H
#define FNTECH3_TR_ENTITY_REFRACTION_H

/*
 * Fn's refraction pass for entities a game marks as distortion copies. Jedi
 * Academy multiplayer's client game sets renderfx 0x2000; the client adapter
 * turns it into refEntity_t.jka.distortion
 * (code/compat/jka_render_adapter.hpp).
 *
 * Retail jamp.exe 1.01 (docs/fntech3/2026-10-15-jka-entity-refraction.md):
 * - The surface list posts the entity after the rest of the view (0x48aba6,
 *   mask 0x202400; renderercommon/tr_jka_entity.h).
 * - Before its first posted surface the backend copies a square of
 *   radius x radius screen pixels (radius truncated from the entity's own,
 *   0x48af4e) into the screen image with glCopyTexImage2D, GL_RGBA16
 *   (0x48afe8). The square is centred on the entity origin's projection,
 *   mirrored horizontally (vidWidth - x - radius / 2), and clamped inside the
 *   screen.
 * - Every single-texture stage then samples that copy in place of its own
 *   image, two-sided (0x4af0cf), at the stage's own texture coordinates and
 *   colours. A forced alpha (0x400) replaces the alpha and the state with
 *   source-alpha blending (0x4af168, state 0x65).
 * - The 1.01 client game's spawn shield copy (cgamex86.dll 0x3004db31) uses
 *   shader effects/refract_2 (rgbGen entity, alphaGen entity, tcMod stretch
 *   triangle 2 1 0 0.4), radius 256, half the shell's scale, and entity
 *   colour (0, 255, 0, 100) for invulnerability. The copy therefore shows
 *   the background magnified one to three times on a 2.5 s cycle, filtered
 *   by the entity colour, at 100/255 opacity.
 *
 * Fn (AGENTS.md invariant 11) draws such an entity's surfaces with its liquid
 * refraction programs instead: each fragment samples the scene snapshot at
 * its own screen position, bent by the liquid wave field in the entity's
 * model space. Like the liquid underlay it replaces the background rather
 * than blending a bent copy over an unbent one. The module's data still
 * decide the result:
 * - presence: the entity's forced alpha, or full without one, scales how far
 *   the scene bends (the liquid pass folds r_liquidRefraction the same way);
 * - tint: the entity's colour, when the first stage takes it (rgbGen entity
 *   or the 0x800 tint), filters the bent scene at that presence.
 * Shadow and light passes skip the entity, and so does any view the pass
 * cannot serve (no snapshot, portal or mirror view, a view without the
 * world, stereo, r_entityRefraction 0): retail's copy only bends what is
 * behind it, so leaving it out loses the bend and nothing else.
 *
 * Each renderer includes this after tr_local.h, where r_entityRefraction is
 * declared.
 */

/* Displacement at full presence and r_entityRefraction 1, as a fraction of
 * the view height, before the wave field's own amplitude (at most 1) and its
 * distance and grazing fades.
 *
 * Retail's bend magnifies a fixed 256-pixel square one to three times: a
 * sample moves by (1 - 1/s) of its distance from the square's centre, up to
 * 85 pixels at the edge whatever the resolution. Averaged over the triangle
 * wave, 1 - ln(3)/2 = 0.45 of the square's mean distance from its centre
 * (98 pixels) is about 44 pixels, which the shield shows at 100/255: about
 * 17 pixels (an estimate: the shell's texture coordinates are not measured).
 * The wave field moved the scene behind the shield by a mean 4.4 (GLx) and
 * 5.9 (Vulkan) pixels at 768 lines with 0.06, about 86 pixels per unit, so
 * 0.20 brings it to that mean, scaled with the view height. */
#define ENTITY_REFRACTION_WARP_FRACTION 0.20f

/* The profiles whose client games submit distortion copies. */
static ID_INLINE qboolean R_EntityRefractionProfile( void ) {
	return ri.gameId == COM_GAME_JKA_MP ? qtrue : qfalse;
}

/* Whether the scene snapshot the pass samples is allocated for it. */
static ID_INLINE qboolean R_EntityRefractionAllocates( void ) {
	return R_EntityRefractionProfile() && r_entityRefraction &&
		r_entityRefraction->value > 0.0f ? qtrue : qfalse;
}

/* Whether an entity is a distortion copy. */
static ID_INLINE qboolean R_EntityRefracts( const trRefEntity_t *entity ) {
	return entity && entity->e.jka.distortion ? qtrue : qfalse;
}

/* How much of the bend the entity shows: its forced alpha, or all of it. */
static ID_INLINE float R_EntityRefractionPresence( const trRefEntity_t *entity ) {
	if ( entity->e.jka.forceAlpha ) {
		return (float)entity->e.shader.rgba[3] * ( 1.0f / 255.0f );
	}
	return 1.0f;
}

/* The colour the bent scene is multiplied by. rgbGen is the first stage's
 * own colour source, which the 0x800 tint replaces with the entity's. */
static ID_INLINE void R_EntityRefractionTint( const trRefEntity_t *entity, colorGen_t rgbGen,
	vec3_t tint ) {
	const float presence = R_EntityRefractionPresence( entity );
	int i;

	for ( i = 0; i < 3; i++ ) {
		float color = 1.0f;

		if ( entity->e.jka.rgbTint || rgbGen == CGEN_ENTITY ) {
			color = (float)entity->e.shader.rgba[i] * ( 1.0f / 255.0f );
		}
		tint[i] = 1.0f + ( color - 1.0f ) * presence;
	}
}

/* The largest bend in pixels for a view of this height. */
static ID_INLINE float R_EntityRefractionWarpPixels( const trRefEntity_t *entity, int viewHeight ) {
	const float scale = r_entityRefraction ? Com_Clamp( 0.0f, 4.0f, r_entityRefraction->value ) : 0.0f;

	return scale * ENTITY_REFRACTION_WARP_FRACTION * (float)MAX( 0, viewHeight ) *
		R_EntityRefractionPresence( entity );
}

#endif /* FNTECH3_TR_ENTITY_REFRACTION_H */
