/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MESH_TAGS_H
#define FNTECH3_TR_MESH_TAGS_H

#include "tr_mds_entity.h"
#include "tr_wolfet_entity.h"

/* Indexed tag lookup is distinct from the legacy boolean LerpTag interface.
 * The start index is inclusive, allowing repeated names to be enumerated. */
static int R_IndexedMeshTag( const model_t *model, int frame, const char *name,
	int first, orientation_t *destination ) {
	int index, count;
	if ( first < 0 ) first = 0;
	if ( frame < 0 ) frame = 0;
	if ( model->type == MOD_MDC ) {
		const fnMdcView_t *view;
		if ( !model->mdc[0] || !model->mdc[0]->view.numFrames ) return -1;
		view = &model->mdc[0]->view;
		if ( (uint32_t)frame >= view->numFrames ) frame = (int)view->numFrames - 1;
		for ( index = first; (uint32_t)index < view->numTags; ++index ) {
			const char *tagName = (const char *)view->data + view->tagNames + (size_t)index * 64;
			if ( !strncmp( tagName, name, 64 ) ) {
				vec3_t angles;
				if ( !FnMdc_Tag( view, (uint32_t)frame, (uint32_t)index, destination->origin, angles ) ) return -1;
				AnglesToAxis( angles, destination->axis );
				return index;
			}
		}
	} else if ( model->type == MOD_MESH ) {
		const md3Header_t *header = model->md3[0];
		const md3Tag_t *tags;
		if ( !header || header->numFrames <= 0 || header->numTags <= 0 ) return -1;
		if ( frame >= header->numFrames ) frame = header->numFrames - 1;
		count = header->numTags;
		tags = (const md3Tag_t *)( (const byte *)header + header->ofsTags ) + (size_t)frame * count;
		for ( index = first; index < count; ++index ) {
			if ( !strncmp( tags[index].name, name, sizeof( tags[index].name ) ) ) {
				memcpy( destination->origin, tags[index].origin, sizeof( destination->origin ) );
				memcpy( destination->axis, tags[index].axis, sizeof( destination->axis ) );
				return index;
			}
		}
	}
	return -1;
}

int R_LerpTagEntity( orientation_t *destination, const refEntity_t *entity, const char *name, int first ) {
	orientation_t previous, current, result;
	const model_t *model;
	char tagName[MAX_QPATH];
	float front, back;
	int index = -1, row, column;
	if ( !destination ) return -1;
	memset( &result, 0, sizeof( result ) );
	result.axis[0][0] = result.axis[1][1] = result.axis[2][2] = 1;
	/* Capture aliased inputs before publishing either success or failure. */
	if ( !entity || !name ) goto done;
	model = R_GetModelByHandle( entity->hModel );
	if ( !model ) goto done;
	if ( model->type == MOD_WOLFET_MESH ) {
		/* ET poses a mesh with the skeletons its entity names, so a tag is
		 * placed from the entity alone; there is no per-frame tag to blend. */
		memset( &result, 0, sizeof( result ) );
		index = R_WolfETTag( model->wolfetMesh, entity, name, first, &result );
		goto done;
	}
	if ( model->type == MOD_MDS ) {
		/* Skeletal tags preserve their basis (no row normalization), use both
		 * animation channels and publish an all-zero orientation on failure. */
		memset( &result, 0, sizeof( result ) );
		index = R_MdsTag( model->mds, entity, ri.gameId, name, first, &result );
		goto done;
	}
	if ( !isfinite( entity->backlerp ) ) goto done;
	Q_strncpyz( tagName, name, sizeof( tagName ) );
	if ( R_IndexedMeshTag( model, entity->oldframe, tagName, first, &previous ) < 0 ) goto done;
	index = R_IndexedMeshTag( model, entity->frame, tagName, first, &current );
	if ( index < 0 ) goto done;
	/* Preserve RTCW's two rounded conversions, including very small backlerps. */
	front = (float)( 1.0 - entity->backlerp );
	back = (float)( 1.0 - front );
	for ( column = 0; column < 3; ++column ) {
		result.origin[column] = previous.origin[column] * back + current.origin[column] * front;
		for ( row = 0; row < 3; ++row ) result.axis[row][column] =
			previous.axis[row][column] * back + current.axis[row][column] * front;
	}
	for ( row = 0; row < 3; ++row ) {
		if ( !isfinite( result.origin[row] ) ) goto invalid;
		for ( column = 0; column < 3; ++column ) if ( !isfinite( result.axis[row][column] ) ) goto invalid;
		if ( !isfinite( VectorNormalize( result.axis[row] ) ) ) goto invalid;
	}
done:
	memcpy( destination, &result, sizeof( result ) );
	return index;
invalid:
	memset( &result, 0, sizeof( result ) );
	result.axis[0][0] = result.axis[1][1] = result.axis[2][2] = 1;
	index = -1;
	goto done;
}

#endif
