/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_POSE_H
#define FNTECH3_TR_WOLFET_POSE_H

#include "tr_wolfet_model_format.h"

/* Model-space bone transforms for Enemy Territory's skeletons, independent of
 * the module refEntity ABI. The caller supplies the engine's AnglesToAxis and
 * AngleVectors before this header. Immutable input plus local pose storage, so
 * no bone cache is shared between entities.
 *
 * ET differs from the other Wolfenstein profile in where the frames come from.
 * Its reference entity names a model handle beside every frame index, so the
 * current frame, the previous frame and the two torso frames may each be read
 * from a different skeleton file. The bone table itself -- each bone's parent,
 * its distance from that parent and its torso weight -- is read from the
 * skeleton the current frame came from, which is what ET's own renderer does.
 */
typedef struct { float axis[3][3], origin[3]; } fnWolfETTransform_t;
typedef struct { uint32_t numBones; fnWolfETTransform_t bones[FN_WOLFET_MAX_BONES]; } fnWolfETPose_t;

typedef struct {
	/* Four skeletons, one per frame index. A null torso view means the entity
	 * has no separate torso animation. */
	const fnWolfETSkeletonView_t *frameView, *oldFrameView;
	const fnWolfETSkeletonView_t *torsoView, *oldTorsoView;
	uint32_t frame, oldFrame, torsoFrame, oldTorsoFrame;
	float backlerp, torsoBacklerp, torsoAxis[3][3];
} fnWolfETPoseParams_t;

/* The angle wrap quantizes to a sixteen-bit turn rather than using fmod, which
 * is what the compressed poses are stored in. Validate before narrowing so a
 * malformed extrapolation cannot overflow the integer. */
static inline int FnWolfET_WrapAngle( float angle, float *destination ) {
	const double ticks = (double)angle * ( 65536.0 / 360.0 );
	float result;
	if ( !isfinite( ticks ) || ticks < INT32_MIN || ticks > INT32_MAX ) return 0;
	result = (float)( ( (uint32_t)(int32_t)ticks & 65535u ) * ( 360.0 / 65536.0 ) );
	if ( result > 180 ) result -= 360;
	*destination = result;
	return 1;
}

static inline int FnWolfET_BoneAngles( const fnWolfETBonePose_t *current,
	const fnWolfETBonePose_t *previous, float back, int interpolate, float angles[3] ) {
	int i;
	for ( i = 0; i < 3; ++i ) {
		const float now = (float)( current->angles[i] * ( 360.0 / 65536.0 ) );
		angles[i] = now;
		if ( interpolate ) {
			const float old = (float)( previous->angles[i] * ( 360.0 / 65536.0 ) );
			float delta;
			if ( !FnWolfET_WrapAngle( now - old, &delta ) ) return 0;
			angles[i] = now - back * delta;
		}
		if ( !isfinite( angles[i] ) ) return 0;
	}
	return 1;
}

static inline void FnWolfET_OffsetDirection( const fnWolfETBonePose_t *bone, float direction[3] ) {
	float angles[3];
	angles[0] = (float)( bone->offsetAngles[0] * ( 360.0 / 65536.0 ) );
	angles[1] = (float)( bone->offsetAngles[1] * ( 360.0 / 65536.0 ) );
	angles[2] = 0;
	AngleVectors( angles, direction, NULL, NULL );
}

static inline int FnWolfET_BlendDirection( const float from[3], const float to[3],
	float fraction, float destination[3] ) {
	const float inverse = (float)( 1.0 - fraction );
	float result[3], length;
	int i;
	for ( i = 0; i < 3; ++i ) result[i] = from[i] * inverse + to[i] * fraction;
	length = (float)sqrt( result[0] * result[0] + result[1] * result[1] + result[2] * result[2] );
	if ( !isfinite( length ) ) return 0;
	if ( length > 0 ) { const float scale = 1.0f / length; for ( i = 0; i < 3; ++i ) result[i] *= scale; }
	for ( i = 0; i < 3; ++i ) if ( !isfinite( result[i] ) ) return 0;
	memcpy( destination, result, sizeof( result ) );
	return 1;
}

static inline int FnWolfET_AnimatedDirection( const fnWolfETBonePose_t *current,
	const fnWolfETBonePose_t *previous, float back, int interpolate, float direction[3] ) {
	float old[3];
	FnWolfET_OffsetDirection( current, direction );
	if ( !interpolate ) return 1;
	FnWolfET_OffsetDirection( previous, old );
	return FnWolfET_BlendDirection( old, direction, 1.0f - back, direction );
}

static inline int FnWolfET_FiniteTransform( const fnWolfETTransform_t *transform ) {
	int row, column;
	for ( row = 0; row < 3; ++row ) {
		if ( !isfinite( transform->origin[row] ) ) return 0;
		for ( column = 0; column < 3; ++column )
			if ( !isfinite( transform->axis[row][column] ) ) return 0;
	}
	return 1;
}

/* Every skeleton a pose reads from must describe the same bones, or a frame
 * would be blended against a different skeleton's joint. */
static inline int FnWolfET_SkeletonsAgree( const fnWolfETSkeletonView_t *a,
	const fnWolfETSkeletonView_t *b ) {
	return a && b && a->numBones == b->numBones && a->torsoParent == b->torsoParent;
}

static inline int FnWolfET_RawBone( const fnWolfETPoseParams_t *params,
	const fnWolfETBone_t *info, uint32_t index, const fnWolfETFrame_t *frame,
	const fnWolfETFrame_t *oldFrame, const fnWolfETTransform_t *parent,
	float back, float torsoBack, fnWolfETTransform_t *destination ) {
	fnWolfETBonePose_t current, previous, torso, oldTorso;
	const int fullTorso = info->torsoWeight == 1.0f;
	const int interpolate = back != 0 || torsoBack != 0;
	float angles[3], upperAngles[3], direction[3], upperDirection[3];
	int i;
	if ( !FnWolfET_BonePose( params->frameView, params->frame, index, &current ) ||
		!FnWolfET_BonePose( params->oldFrameView, params->oldFrame, index, &previous ) ) return 0;
	if ( info->torsoWeight != 0 &&
		( !FnWolfET_BonePose( params->torsoView, params->torsoFrame, index, &torso ) ||
		!FnWolfET_BonePose( params->oldTorsoView, params->oldTorsoFrame, index, &oldTorso ) ) ) return 0;
	if ( fullTorso ) {
		if ( !FnWolfET_BoneAngles( &torso, &oldTorso, torsoBack, interpolate, angles ) ) return 0;
	} else {
		if ( !FnWolfET_BoneAngles( &current, &previous, back, interpolate, angles ) ) return 0;
		if ( info->torsoWeight != 0 ) {
			if ( !FnWolfET_BoneAngles( &torso, &oldTorso, torsoBack, interpolate, upperAngles ) ) return 0;
			for ( i = 0; i < 3; ++i ) {
				float delta = upperAngles[i] - angles[i];
				if ( fabsf( delta ) > 180 && !FnWolfET_WrapAngle( delta, &delta ) ) return 0;
				angles[i] += info->torsoWeight * delta;
				if ( !isfinite( angles[i] ) ) return 0;
			}
		}
	}
	AnglesToAxis( angles, destination->axis );
	if ( parent ) {
		if ( fullTorso ) {
			if ( !FnWolfET_AnimatedDirection( &torso, &oldTorso, torsoBack, interpolate, direction ) ) return 0;
		} else {
			if ( !FnWolfET_AnimatedDirection( &current, &previous, back, interpolate, direction ) ) return 0;
			if ( info->torsoWeight != 0 ) {
				if ( !FnWolfET_AnimatedDirection( &torso, &oldTorso, torsoBack, interpolate, upperDirection ) ||
					!FnWolfET_BlendDirection( direction, upperDirection, info->torsoWeight, direction ) ) return 0;
			}
		}
		for ( i = 0; i < 3; ++i )
			destination->origin[i] = parent->origin[i] + info->parentDist * direction[i];
	} else {
		/* A root bone sits at the frame's own parent offset, blended between
		 * the two frames when they differ. */
		for ( i = 0; i < 3; ++i ) destination->origin[i] = interpolate ?
			( 1.0f - back ) * frame->parentOffset[i] + back * oldFrame->parentOffset[i] :
			frame->parentOffset[i];
	}
	return FnWolfET_FiniteTransform( destination );
}

/* Decodes the whole skeleton, or one bone and its ancestry when `selected` is
 * not negative. Returns 0 rather than a partial pose. */
static inline int FnWolfET_DecodePoseSelected( const fnWolfETPoseParams_t *params,
	int selected, fnWolfETPose_t *destination ) {
	fnWolfETPose_t result;
	fnWolfETTransform_t raw[FN_WOLFET_MAX_BONES];
	fnWolfETBone_t info[FN_WOLFET_MAX_BONES];
	unsigned char ready[FN_WOLFET_MAX_BONES];
	unsigned char needed[FN_WOLFET_MAX_BONES];
	uint32_t ancestry[FN_WOLFET_MAX_BONES], index;
	const fnWolfETSkeletonView_t *view;
	fnWolfETFrame_t frame, oldFrame;
	float back, torsoBack;
	int row, column, needsTorso;

	if ( !params || !destination ) return 0;
	view = params->frameView;
	if ( !view || !view->numBones || view->numBones > FN_WOLFET_MAX_BONES ) return 0;
	if ( !FnWolfET_SkeletonsAgree( view, params->oldFrameView ) ) return 0;
	if ( params->frame >= view->numFrames || params->oldFrame >= params->oldFrameView->numFrames ) return 0;
	if ( selected >= (int)view->numBones ) return 0;
	memset( ready, 0, sizeof( ready ) );
	memset( needed, 0, sizeof( needed ) );
	for ( index = 0; index < view->numBones; ++index )
		if ( !FnWolfET_Bone( view, index, &info[index], NULL ) ) return 0;

	needsTorso = selected < 0;
	if ( selected < 0 ) memset( needed, 1, view->numBones );
	else {
		int dependency;
		for ( dependency = 0; dependency < 2; ++dependency ) {
			uint32_t current = dependency ? (uint32_t)( view->torsoParent < 0 ? 0 : view->torsoParent )
				: (uint32_t)selected;
			uint32_t depth = 0;
			if ( dependency && ( info[selected].torsoWeight <= 0 || view->torsoParent < 0 ) ) break;
			while ( !needed[current] ) {
				if ( depth++ == view->numBones ) return 0;
				needed[current] = 1;
				if ( info[current].torsoWeight != 0 ) needsTorso = 1;
				if ( info[current].parent < 0 ) break;
				current = (uint32_t)info[current].parent;
				if ( current >= view->numBones ) return 0;
			}
		}
	}
	if ( needsTorso ) {
		if ( !FnWolfET_SkeletonsAgree( view, params->torsoView ) ||
			!FnWolfET_SkeletonsAgree( view, params->oldTorsoView ) ) return 0;
		if ( params->torsoFrame >= params->torsoView->numFrames ||
			params->oldTorsoFrame >= params->oldTorsoView->numFrames ) return 0;
	}
	/* A frame blended against itself, from the same skeleton, needs no lerp. */
	back = ( params->frame == params->oldFrame && params->frameView == params->oldFrameView )
		? 0 : params->backlerp;
	torsoBack = ( !needsTorso || ( params->torsoFrame == params->oldTorsoFrame &&
		params->torsoView == params->oldTorsoView ) ) ? 0 : params->torsoBacklerp;
	if ( !isfinite( back ) || !isfinite( torsoBack ) ) return 0;
	if ( !FnWolfET_Frame( params->frameView, params->frame, &frame ) ||
		!FnWolfET_Frame( params->oldFrameView, params->oldFrame, &oldFrame ) ) return 0;
	if ( selected < 0 || info[selected].torsoWeight > 0 )
		for ( row = 0; row < 3; ++row ) for ( column = 0; column < 3; ++column )
			if ( !isfinite( params->torsoAxis[row][column] ) ) return 0;

	/* Resolve ancestry iteratively: the serialized order need not be
	 * topological even though the reader requires a parent to precede its
	 * child, and every root uses the authored frame parent offset. */
	for ( index = 0; index < view->numBones; ++index ) {
		uint32_t current = index, depth = 0;
		if ( !needed[index] ) continue;
		while ( !ready[current] ) {
			if ( depth == view->numBones ) return 0;
			ancestry[depth++] = current;
			if ( info[current].parent < 0 ) break;
			current = (uint32_t)info[current].parent;
			if ( current >= view->numBones ) return 0;
		}
		while ( depth-- ) {
			const uint32_t bone = ancestry[depth];
			const int parent = info[bone].parent;
			if ( !FnWolfET_RawBone( params, &info[bone], bone, &frame, &oldFrame,
				parent < 0 ? NULL : &raw[parent], back, torsoBack, &raw[bone] ) ) return 0;
			ready[bone] = 1;
		}
	}

	/* The entity's own torso rotation turns every bone that carries torso
	 * weight about the torso parent's resolved position, scaled by that
	 * weight. ET applies it the same way to every such bone: the separate
	 * treatment its renderer keeps for attachment bones is commented out of
	 * the shipped source, where the other Wolfenstein profile still takes it,
	 * so an ET tag turns with the torso exactly as a mesh bone does. */
	memset( &result, 0, sizeof( result ) );
	result.numBones = view->numBones;
	for ( index = 0; index < view->numBones; ++index ) {
		fnWolfETTransform_t *output = &result.bones[index];
		const float weight = info[index].torsoWeight, inverse = 1.0f - weight;
		const float *pivot;
		float relative[3];
		if ( !needed[index] ) continue;
		*output = raw[index];
		if ( weight <= 0 ) continue;
		/* A bone cannot be turned about a torso parent the file does not name. */
		if ( view->torsoParent < 0 ) return 0;
		pivot = raw[view->torsoParent].origin;
		for ( row = 0; row < 3; ++row ) relative[row] = raw[index].origin[row] - pivot[row];
		for ( row = 0; row < 3; ++row ) {
			float rotation[3];
			for ( column = 0; column < 3; ++column ) {
				rotation[column] = weight * params->torsoAxis[column][row];
				if ( row == column ) rotation[column] += inverse;
			}
			for ( column = 0; column < 3; ++column ) output->axis[row][column] =
				rotation[0] * raw[index].axis[0][column] +
				rotation[1] * raw[index].axis[1][column] +
				rotation[2] * raw[index].axis[2][column];
			output->origin[row] = rotation[0] * relative[0] + rotation[1] * relative[1] +
				rotation[2] * relative[2] + pivot[row];
		}
		if ( !FnWolfET_FiniteTransform( output ) ) return 0;
	}
	*destination = result;
	return 1;
}

static inline int FnWolfET_DecodePose( const fnWolfETPoseParams_t *params,
	fnWolfETPose_t *destination ) {
	return FnWolfET_DecodePoseSelected( params, -1, destination );
}

/* Places one mesh vertex using the posed skeleton. A weight's bone index names
 * a bone in that skeleton, not a position in the surface's bone reference list:
 * ET's header says otherwise but its renderer and its shipped assets agree on
 * the global index, and the reference list is only a hint about which bones a
 * surface touches. The normal follows the first weight's bone alone, which is
 * what ET's own deformation does. */
static inline int FnWolfET_SkinVertex( const fnWolfETVertexView_t *vertex,
	const fnWolfETPose_t *pose, float xyz[3], float normal[3] ) {
	float position[3] = { 0, 0, 0 }, direction[3] = { 0, 0, 0 };
	uint32_t index;
	if ( !vertex || !pose || !xyz || !normal || !vertex->numWeights ||
		!pose->numBones || pose->numBones > FN_WOLFET_MAX_BONES ) return 0;
	for ( index = 0; index < vertex->numWeights; ++index ) {
		fnWolfETWeight_t weight;
		const fnWolfETTransform_t *bone;
		int row;
		if ( !FnWolfET_MeshWeight( vertex, index, &weight ) ||
			weight.bone >= pose->numBones ) return 0;
		bone = &pose->bones[weight.bone];
		for ( row = 0; row < 3; ++row ) {
			position[row] += weight.weight * ( weight.offset[0] * bone->axis[row][0] +
				weight.offset[1] * bone->axis[row][1] + weight.offset[2] * bone->axis[row][2] +
				bone->origin[row] );
			if ( !index ) direction[row] = vertex->normal[0] * bone->axis[row][0] +
				vertex->normal[1] * bone->axis[row][1] + vertex->normal[2] * bone->axis[row][2];
		}
	}
	for ( index = 0; index < 3; ++index )
		if ( !isfinite( position[index] ) || !isfinite( direction[index] ) ) return 0;
	memcpy( xyz, position, sizeof( position ) );
	memcpy( normal, direction, sizeof( direction ) );
	return 1;
}

/* Places a mesh tag. ET's tag carries its own axis and offset in the space of
 * the bone it hangs from, where the other Wolfenstein profile's tag is simply
 * that bone, so the bone transform and the tag's own frame are composed here.
 * Returns the index on success so a caller can continue searching past it. */
static inline int FnWolfET_TagOrientation( const fnWolfETMeshView_t *mesh, uint32_t index,
	const fnWolfETPose_t *pose, float origin[3], float axis[3][3] ) {
	fnWolfETTag_t tag;
	const fnWolfETTransform_t *bone;
	int row, column;
	if ( !mesh || !pose || !origin || !axis ) return 0;
	if ( !FnWolfET_MeshTag( mesh, index, &tag, NULL ) ) return 0;
	if ( tag.bone >= pose->numBones ) return 0;
	bone = &pose->bones[tag.bone];
	for ( row = 0; row < 3; ++row ) {
		origin[row] = bone->origin[row] + tag.offset[0] * bone->axis[row][0] +
			tag.offset[1] * bone->axis[row][1] + tag.offset[2] * bone->axis[row][2];
		if ( !isfinite( origin[row] ) ) return 0;
	}
	for ( row = 0; row < 3; ++row ) for ( column = 0; column < 3; ++column ) {
		axis[row][column] = tag.axis[row][0] * bone->axis[column][0] +
			tag.axis[row][1] * bone->axis[column][1] + tag.axis[row][2] * bone->axis[column][2];
		if ( !isfinite( axis[row][column] ) ) return 0;
	}
	return 1;
}

/* Finds a named tag from a starting index, as ET's tag lookup does. */
static inline int FnWolfET_FindTag( const fnWolfETMeshView_t *mesh, const char *name, int first ) {
	uint32_t index;
	if ( !mesh || !name ) return -1;
	if ( first < 0 ) first = 0;
	for ( index = (uint32_t)first; index < mesh->numTags; ++index ) {
		fnWolfETTag_t tag;
		const char *tagName = NULL;
		if ( !FnWolfET_MeshTag( mesh, index, &tag, &tagName ) ) return -1;
		if ( tagName && !strcmp( tagName, name ) ) return (int)index;
	}
	return -1;
}

#endif
