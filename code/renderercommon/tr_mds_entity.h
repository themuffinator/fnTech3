/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDS_ENTITY_H
#define FNTECH3_TR_MDS_ENTITY_H

static inline fnMdsPoseParams_t R_MdsEntityPoseParams( const refEntity_t *entity, comGameId_t game ) {
	fnMdsPoseParams_t params;
	memset( &params, 0, sizeof( params ) );
	params.frame = (uint32_t)entity->frame;
	params.oldFrame = (uint32_t)entity->oldframe;
	params.backlerp = entity->backlerp;
	if ( game == COM_GAME_RTCW_SP || game == COM_GAME_RTCW_MP ) {
		params.torsoFrame = (uint32_t)entity->rtcw.torsoFrame;
		params.oldTorsoFrame = (uint32_t)entity->rtcw.oldTorsoFrame;
		params.torsoBacklerp = entity->rtcw.torsoBacklerp;
		memcpy( params.torsoAxis, entity->rtcw.torsoAxis, sizeof( params.torsoAxis ) );
	} else {
		/* Legacy modules can use MDS assets with their one animation channel. */
		params.torsoFrame = params.frame;
		params.oldTorsoFrame = params.oldFrame;
		params.torsoBacklerp = params.backlerp;
		params.torsoAxis[0][0] = params.torsoAxis[1][1] = params.torsoAxis[2][2] = 1;
	}
	return params;
}

static inline int R_MdsEntityPose( const fnMdsView_t *view, const refEntity_t *entity,
	comGameId_t game, fnMdsPose_t *pose ) {
	const fnMdsPoseParams_t params = R_MdsEntityPoseParams( entity, game );
	return FnMds_DecodePose( view, &params, pose );
}

static inline void R_MdsSanitizeFrames( const fnMdsView_t *view, refEntity_t *entity, comGameId_t game ) {
	if ( game != COM_GAME_RTCW_SP && game != COM_GAME_RTCW_MP && ( entity->renderfx & RF_WRAP_FRAMES ) ) {
		entity->frame %= (int)view->numFrames;
		entity->oldframe %= (int)view->numFrames;
	}
	if ( (uint32_t)entity->frame >= view->numFrames || (uint32_t)entity->oldframe >= view->numFrames ||
		( ( game == COM_GAME_RTCW_SP || game == COM_GAME_RTCW_MP ) &&
		( (uint32_t)entity->rtcw.torsoFrame >= view->numFrames || (uint32_t)entity->rtcw.oldTorsoFrame >= view->numFrames ) ) ) {
		entity->frame = entity->oldframe = entity->rtcw.torsoFrame = entity->rtcw.oldTorsoFrame = 0;
	}
}

static inline int R_MdsTag( const mdsModel_t *model, const refEntity_t *entity, comGameId_t game,
	const char *name, int first, orientation_t *destination ) {
	fnMdsTransform_t transform;
	int index;
	if ( !model ) return -1;
	if ( first < 0 ) first = 0;
	for ( index = first; (uint32_t)index < model->view.numTags; ++index ) {
		const char *tagName = (const char *)model->view.data + model->view.tags + (size_t)index * FN_MDS_TAG_BYTES;
		if ( !strcmp( tagName, name ) ) {
			fnMdsTag_t tag;
			const fnMdsPoseParams_t params = R_MdsEntityPoseParams( entity, game );
			if ( !FnMds_Tag( &model->view, (uint32_t)index, &tag ) ||
				!FnMds_DecodeBone( &model->view, &params, tag.bone, &transform ) ) return -1;
			memcpy( destination->origin, transform.origin, sizeof( destination->origin ) );
			memcpy( destination->axis, transform.axis, sizeof( destination->axis ) );
			return index;
		}
	}
	return -1;
}

#endif
