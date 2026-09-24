/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_ENTITY_SCENE_H
#define FNTECH3_TR_Q3_IHV_ENTITY_SCENE_H

qboolean RE_AddIHVEntitiesToScene( int count, const fnQ3IHVEntity_t *entities ) {
	int i, row, column;
	if ( ri.gameId != COM_GAME_QUAKE3_IHV ) {
		ri.Error( ERR_DROP, "IHV entities submitted outside their game profile" );
		return qfalse;
	}
	if ( !tr.registered || count < 0 || ( count && !entities ) ||
		r_numentities < 0 || r_numentities > MAX_REFENTITIES || count > MAX_REFENTITIES - r_numentities ) return qfalse;
	/* Validate the complete batch before changing any frame-owned storage. */
	for ( i = 0; i < count; ++i ) {
		const fnQ3IHVEntity_t *input = entities + i;
		/* Retained transform AND lighting depend on sorted draw order. Do not
		 * accept a partially implemented contract on either raster or ray paths. */
		if ( input->flags & Q3IHV_ENTITY_KEEP_TRANSFORM ) {
			ri.Printf( PRINT_WARNING, "IHV retained transform/lighting (flag 0x80) is not implemented\n" );
			return qfalse;
		}
		if ( input->model < 0 || input->model >= tr.numModels ||
			input->skin < 0 || input->skin >= tr.numSkins ||
			input->shader < 0 || input->shader >= tr.numShaders ) return qfalse;
		if ( ( input->model && !tr.models[input->model] ) ||
			( input->skin && !tr.skins[input->skin] ) ||
			( input->shader && !tr.shaders[input->shader] ) ) return qfalse;
		if ( input->model && tr.models[input->model]->type == MOD_IHV_BEAM && !tr.ihvBeamShader ) return qfalse;
		if ( !Q3IHV_EntityFinite( &input->backlerp ) ) return qfalse;
		for ( row = 0; row < 3; ++row ) {
			if ( !Q3IHV_EntityFinite( input->origin + row ) ||
				!Q3IHV_EntityFinite( input->oldorigin + row ) ) return qfalse;
			for ( column = 0; column < 3; ++column )
				if ( !Q3IHV_EntityFinite( &input->axis[row][column] ) ) return qfalse;
		}
	}
	for ( i = 0; i < count; ++i ) {
		const fnQ3IHVEntity_t *input = entities + i;
		trRefEntity_t *output = &backEndData->entities[r_numentities + i];
		refEntity_t *entity = &output->e;
		memset( output, 0, sizeof( *output ) );
		entity->q3IHV.active = 1;
		entity->q3IHV.flags = input->flags;
		entity->reType = ( input->flags & Q3IHV_ENTITY_PORTAL ) ? RT_PORTALSURFACE : RT_MODEL;
		entity->hModel = input->model;
		entity->customSkin = input->skin;
		entity->customShader = input->shader;
		if ( entity->reType == RT_MODEL && input->model && tr.models[input->model]->type == MOD_IHV_BEAM ) {
			entity->reType = RT_BEAM;
			entity->customShader = tr.ihvBeamShader->index;
		}
		/* These three meanings are proven; all other native bits remain native. */
		if ( input->flags & Q3IHV_ENTITY_THIRD_PERSON ) entity->renderfx |= RF_THIRD_PERSON;
		if ( input->flags & Q3IHV_ENTITY_FIRST_PERSON ) entity->renderfx |= RF_FIRST_PERSON;
		if ( input->flags & Q3IHV_ENTITY_DEPTH_HACK ) entity->renderfx |= RF_DEPTHHACK;
		memcpy( entity->axis, input->axis, sizeof( entity->axis ) );
		memcpy( entity->origin, input->origin, sizeof( entity->origin ) );
		memcpy( entity->oldorigin, input->oldorigin, sizeof( entity->oldorigin ) );
		memcpy( entity->shader.rgba, input->rgba, sizeof( input->rgba ) );
		entity->frame = input->frame; entity->oldframe = input->oldframe;
		entity->backlerp = input->backlerp; entity->skinNum = input->skinNum;
		entity->nonNormalizedAxes = input->nonNormalizedAxes ? qtrue : qfalse;
	}
	r_numentities += count;
	return qtrue;
}

#endif
