/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RITUAL_ADDITIVE_LIGHT_H
#define FNTECH3_TR_RITUAL_ADDITIVE_LIGHT_H

/* Alice's and F.A.K.K.2's client games mark a dynamic light additive (its
 * type's bit 4, RE_AddAliceLightToScene). Retail's projected light adds such
 * a light to the world and brush models whatever the surface, GL_ONE GL_ONE,
 * where it modulates any other light by what it lights, GL_DST_COLOR GL_ONE
 * (fakk2.exe 1.02 0x0047d32b..0x0047d33d): the green glow round the dung pile
 * on creeperpens. The per-pixel pass keeps its own falloff and facing and
 * lights those surfaces from white in place of their texture, so the light
 * adds its own colour. A model keeps its texture: retail lights models by its
 * model rule (R_RitualAddEnclosedDlights). Every other light, and every other
 * profile's, is as before. */
static ID_INLINE qboolean RB_RitualAdditiveWorldLight( const dlight_t *dl ) {
	const model_t *model;
	if ( !dl || !dl->additive || !( dl->ritualRadius > 0.0f ) || !backEnd.currentEntity ) {
		return qfalse;
	}
	if ( backEnd.currentEntity == &tr.worldEntity ) {
		return qtrue;
	}
	if ( backEnd.currentEntity->e.reType != RT_MODEL ) {
		return qfalse;
	}
	model = R_GetModelByHandle( backEnd.currentEntity->e.hModel );
	return model && model->type == MOD_BRUSH ? qtrue : qfalse;
}

#endif
