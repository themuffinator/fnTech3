/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_ENTITY_H
#define FNTECH3_TR_WOLFET_ENTITY_H

/* Binds Enemy Territory's reference entity to the pose arithmetic. ET names a
 * skeleton beside every frame index, so the four handles are resolved here and
 * an entity whose skeletons cannot be resolved is refused rather than posed
 * against whatever model happens to hold that handle. */

static inline const wolfetSkeletonModel_t *R_WolfETSkeletonModel( qhandle_t handle ) {
	const model_t *model;
	if ( handle <= 0 ) return NULL;
	model = R_GetModelByHandle( handle );
	if ( !model || model->type != MOD_WOLFET_SKELETON ) return NULL;
	return model->wolfetSkeleton;
}

static inline int R_WolfETPoseParams( const refEntity_t *entity, fnWolfETPoseParams_t *params ) {
	const wolfetSkeletonModel_t *frame, *oldFrame, *torso, *oldTorso;
	if ( !entity || !params ) return 0;
	memset( params, 0, sizeof( *params ) );
	frame = R_WolfETSkeletonModel( entity->wolfet.frameModel );
	oldFrame = R_WolfETSkeletonModel( entity->wolfet.oldframeModel );
	torso = R_WolfETSkeletonModel( entity->wolfet.torsoFrameModel );
	oldTorso = R_WolfETSkeletonModel( entity->wolfet.oldTorsoFrameModel );
	if ( !frame || !oldFrame ) return 0;
	if ( entity->frame < 0 || entity->oldframe < 0 ||
		entity->wolfet.torsoFrame < 0 || entity->wolfet.oldTorsoFrame < 0 ) return 0;
	params->frameView = &frame->view;
	params->oldFrameView = &oldFrame->view;
	params->torsoView = torso ? &torso->view : NULL;
	params->oldTorsoView = oldTorso ? &oldTorso->view : NULL;
	params->frame = (uint32_t)entity->frame;
	params->oldFrame = (uint32_t)entity->oldframe;
	params->torsoFrame = (uint32_t)entity->wolfet.torsoFrame;
	params->oldTorsoFrame = (uint32_t)entity->wolfet.oldTorsoFrame;
	params->backlerp = entity->backlerp;
	params->torsoBacklerp = entity->wolfet.torsoBacklerp;
	memcpy( params->torsoAxis, entity->wolfet.torsoAxis, sizeof( params->torsoAxis ) );
	return 1;
}

static inline int R_WolfETEntityPose( const refEntity_t *entity, fnWolfETPose_t *pose ) {
	fnWolfETPoseParams_t params;
	return R_WolfETPoseParams( entity, &params ) && FnWolfET_DecodePose( &params, pose );
}

/* The bounds and the detail radius both come from the skeleton the current
 * frame was read from, as ET's own culling and level-of-detail do. */
static inline const md3Frame_t *R_WolfETEntityFrame( qhandle_t handle, int frame ) {
	const wolfetSkeletonModel_t *skeleton = R_WolfETSkeletonModel( handle );
	if ( !skeleton || frame < 0 || (uint32_t)frame >= skeleton->view.numFrames ) return NULL;
	return skeleton->frames + frame;
}

static inline int R_WolfETTag( const wolfetMeshModel_t *mesh, const refEntity_t *entity,
	const char *name, int first, orientation_t *destination ) {
	fnWolfETPoseParams_t params;
	fnWolfETPose_t pose;
	fnWolfETTag_t tag;
	orientation_t result;
	int index;
	if ( !mesh || !entity || !name || !destination ) return -1;
	index = FnWolfET_FindTag( &mesh->view, name, first );
	if ( index < 0 ) return -1;
	if ( !FnWolfET_MeshTag( &mesh->view, (uint32_t)index, &tag, NULL ) ) return -1;
	if ( !R_WolfETPoseParams( entity, &params ) ) return -1;
	/* Only the tag's own bone and its ancestry need posing. */
	if ( !FnWolfET_DecodePoseSelected( &params, (int)tag.bone, &pose ) ) return -1;
	/* Placed into local storage first, so a tag refused halfway leaves the
	 * caller's orientation as it was rather than partly written. */
	memset( &result, 0, sizeof( result ) );
	if ( !FnWolfET_TagOrientation( &mesh->view, (uint32_t)index, &pose,
		result.origin, result.axis ) ) return -1;
	memcpy( destination, &result, sizeof( result ) );
	return index;
}

#endif
