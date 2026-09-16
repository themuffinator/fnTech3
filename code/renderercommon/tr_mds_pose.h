/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDS_POSE_H
#define FNTECH3_TR_MDS_POSE_H

#include "tr_mds_format.h"

/* Model-space transforms, independent of the module refEntity ABI. The caller
 * supplies the engine's AnglesToAxis/AngleVectors helpers before this header.
 * Immutable input plus local pose storage avoids cross-entity bone caches.
 */
typedef struct { float axis[3][3], origin[3]; } fnMdsTransform_t;
typedef struct { uint32_t numBones; fnMdsTransform_t bones[FN_MDS_MAX_BONES]; } fnMdsPose_t;
typedef struct {
	uint32_t frame, oldFrame, torsoFrame, oldTorsoFrame;
	float backlerp, torsoBacklerp, torsoAxis[3][3];
} fnMdsPoseParams_t;

/* RTCW angle wrapping quantizes to a 16-bit turn, rather than using fmod.
 * Validate before narrowing so malformed extrapolation cannot overflow int. */
static inline int FnMds_WrapAngle( float angle, float *destination ) {
	const double ticks = (double)angle * ( 65536.0 / 360.0 );
	float result;
	if ( !isfinite( ticks ) || ticks < INT32_MIN || ticks > INT32_MAX ) return 0;
	result = (float)( ( (uint32_t)(int32_t)ticks & 65535u ) * ( 360.0 / 65536.0 ) );
	if ( result > 180 ) result -= 360;
	*destination = result;
	return 1;
}

static inline int FnMds_BoneAngles( const fnMdsCompressedBone_t *current,
	const fnMdsCompressedBone_t *previous, float back, int interpolate, float angles[3] ) {
	int i;
	for ( i = 0; i < 3; ++i ) {
		const float now = (float)( current->angles[i] * ( 360.0 / 65536.0 ) );
		angles[i] = now;
		if ( interpolate ) {
			const float old = (float)( previous->angles[i] * ( 360.0 / 65536.0 ) );
			float delta;
			if ( !FnMds_WrapAngle( now - old, &delta ) ) return 0;
			angles[i] = now - back * delta;
		}
		if ( !isfinite( angles[i] ) ) return 0;
	}
	return 1;
}

static inline void FnMds_OffsetDirection( const fnMdsCompressedBone_t *bone, float direction[3] ) {
	float angles[3];
	angles[0] = (float)( bone->offsetAngles[0] * ( 360.0 / 65536.0 ) );
	angles[1] = (float)( bone->offsetAngles[1] * ( 360.0 / 65536.0 ) );
	angles[2] = 0;
	AngleVectors( angles, direction, NULL, NULL );
}

static inline int FnMds_BlendDirection( const float from[3], const float to[3], float fraction, float destination[3] ) {
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

static inline int FnMds_AnimatedDirection( const fnMdsCompressedBone_t *current,
	const fnMdsCompressedBone_t *previous, float back, int interpolate, float direction[3] ) {
	float old[3];
	FnMds_OffsetDirection( current, direction );
	if ( !interpolate ) return 1;
	FnMds_OffsetDirection( previous, old );
	return FnMds_BlendDirection( old, direction, 1.0f - back, direction );
}

static inline int FnMds_FiniteTransform( const fnMdsTransform_t *transform ) {
	int row, column;
	for ( row = 0; row < 3; ++row ) {
		if ( !isfinite( transform->origin[row] ) ) return 0;
		for ( column = 0; column < 3; ++column ) if ( !isfinite( transform->axis[row][column] ) ) return 0;
	}
	return 1;
}

static inline int FnMds_RawBone( const fnMdsView_t *view, const fnMdsPoseParams_t *params,
	const fnMdsBone_t *info, uint32_t index, const fnMdsFrame_t *frame, const fnMdsFrame_t *oldFrame,
	const fnMdsTransform_t *parent, float back, float torsoBack, fnMdsTransform_t *destination ) {
	fnMdsCompressedBone_t current, previous, torso, oldTorso;
	const int fullTorso = info->torsoWeight == 1.0f, interpolate = back != 0 || torsoBack != 0;
	float angles[3], upperAngles[3], direction[3], upperDirection[3];
	int i;
	if ( !FnMds_CompressedBone( view, params->frame, index, &current ) ||
		!FnMds_CompressedBone( view, params->oldFrame, index, &previous ) ) return 0;
	if ( info->torsoWeight != 0 &&
		( !FnMds_CompressedBone( view, params->torsoFrame, index, &torso ) ||
		!FnMds_CompressedBone( view, params->oldTorsoFrame, index, &oldTorso ) ) ) return 0;
	if ( fullTorso ) {
		if ( !FnMds_BoneAngles( &torso, &oldTorso, torsoBack, interpolate, angles ) ) return 0;
	} else {
		if ( !FnMds_BoneAngles( &current, &previous, back, interpolate, angles ) ) return 0;
		if ( info->torsoWeight != 0 ) {
			if ( !FnMds_BoneAngles( &torso, &oldTorso, torsoBack, interpolate, upperAngles ) ) return 0;
			for ( i = 0; i < 3; ++i ) {
				float delta = upperAngles[i] - angles[i];
				if ( fabsf( delta ) > 180 && !FnMds_WrapAngle( delta, &delta ) ) return 0;
				angles[i] += info->torsoWeight * delta;
				if ( !isfinite( angles[i] ) ) return 0;
			}
		}
	}
	AnglesToAxis( angles, destination->axis );
	if ( parent ) {
		if ( fullTorso ) {
			if ( !FnMds_AnimatedDirection( &torso, &oldTorso, torsoBack, interpolate, direction ) ) return 0;
		} else {
			if ( !FnMds_AnimatedDirection( &current, &previous, back, interpolate, direction ) ) return 0;
			if ( info->torsoWeight != 0 ) {
				if ( !FnMds_AnimatedDirection( &torso, &oldTorso, torsoBack, interpolate, upperDirection ) ||
					!FnMds_BlendDirection( direction, upperDirection, info->torsoWeight, direction ) ) return 0;
			}
		}
		for ( i = 0; i < 3; ++i ) destination->origin[i] = parent->origin[i] + info->parentDist * direction[i];
	} else {
		for ( i = 0; i < 3; ++i ) destination->origin[i] = interpolate ?
			( 1.0f - back ) * frame->parentOffset[i] + back * oldFrame->parentOffset[i] : frame->parentOffset[i];
	}
	return FnMds_FiniteTransform( destination );
}

/* A selected bone needs only its ancestry and, when rotated, the torso pivot.
 * A negative selection retains the complete mesh-pose contract. */
static inline int FnMds_DecodePoseSelected( const fnMdsView_t *view, const fnMdsPoseParams_t *params,
	int selected, fnMdsPose_t *destination ) {
	fnMdsPose_t result;
	fnMdsTransform_t raw[FN_MDS_MAX_BONES];
	fnMdsBone_t info[FN_MDS_MAX_BONES];
	unsigned char ready[FN_MDS_MAX_BONES] = {0};
	unsigned char needed[FN_MDS_MAX_BONES] = {0};
	uint32_t ancestry[FN_MDS_MAX_BONES], index;
	fnMdsFrame_t frame, oldFrame;
	float back, torsoBack;
	int row, column, needsTorso;
	if ( !view || !params || !destination || !view->numBones || view->numBones > FN_MDS_MAX_BONES ||
		view->torsoParent >= view->numBones || params->frame >= view->numFrames || params->oldFrame >= view->numFrames ||
		selected >= (int)view->numBones ) return 0;
	for ( index = 0; index < view->numBones; ++index ) if ( !FnMds_Bone( view, index, &info[index] ) ) return 0;
	needsTorso = selected < 0;
	if ( selected < 0 ) memset( needed, 1, view->numBones );
	else {
		int dependency;
		for ( dependency = 0; dependency < 2; ++dependency ) {
			uint32_t current = dependency ? view->torsoParent : (uint32_t)selected, depth = 0;
			if ( dependency && info[selected].torsoWeight <= 0 ) break;
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
	if ( needsTorso && ( params->torsoFrame >= view->numFrames || params->oldTorsoFrame >= view->numFrames ) ) return 0;
	back = params->frame == params->oldFrame ? 0 : params->backlerp;
	torsoBack = !needsTorso || params->torsoFrame == params->oldTorsoFrame ? 0 : params->torsoBacklerp;
	if ( !isfinite( back ) || !isfinite( torsoBack ) || !FnMds_Frame( view, params->frame, &frame ) ||
		!FnMds_Frame( view, params->oldFrame, &oldFrame ) ) return 0;
	if ( selected < 0 || info[selected].torsoWeight > 0 )
		for ( row = 0; row < 3; ++row ) for ( column = 0; column < 3; ++column )
			if ( !isfinite( params->torsoAxis[row][column] ) ) return 0;
	/* Resolve ancestry iteratively. Serialized order need not be topological,
	 * and every root uses the authored frame parentOffset. */
	for ( index = 0; index < view->numBones; ++index ) {
		uint32_t current = index, depth = 0;
		if ( !needed[index] ) continue;
		while ( !ready[current] ) {
			if ( depth == view->numBones ) return 0;
			ancestry[depth++] = current;
			if ( info[current].parent < 0 ) break;
			current = (uint32_t)info[current].parent;
		}
		while ( depth ) {
			const uint32_t bone = ancestry[--depth];
			const int parent = info[bone].parent;
			if ( !FnMds_RawBone( view, params, &info[bone], bone, &frame, &oldFrame,
				parent >= 0 ? &raw[parent] : NULL, back, torsoBack, &raw[bone] ) ) return 0;
			ready[bone] = 1;
		}
	}
	memset( &result, 0, sizeof( result ) ); result.numBones = view->numBones;
	for ( index = 0; index < view->numBones; ++index ) {
		fnMdsTransform_t *output = &result.bones[index];
		const float weight = info[index].torsoWeight, inverse = 1.0f - weight;
		const float *pivot;
		float relative[3];
		if ( selected >= 0 && index != (uint32_t)selected ) continue;
		*output = raw[index];
		if ( weight <= 0 ) continue;
		pivot = raw[view->torsoParent].origin;
		for ( row = 0; row < 3; ++row ) relative[row] = raw[index].origin[row] - pivot[row];
		if ( info[index].flags & 1u ) {
			/* Attachments rotate each basis vector; mesh bones compose matrices.
			 * The two contracts intentionally differ for nontrivial rotations. */
			for ( row = 0; row < 3; ++row ) {
				for ( column = 0; column < 3; ++column ) output->axis[row][column] = inverse * raw[index].axis[row][column] + weight *
					( raw[index].axis[row][0] * params->torsoAxis[0][column] + raw[index].axis[row][1] * params->torsoAxis[1][column] + raw[index].axis[row][2] * params->torsoAxis[2][column] );
				output->origin[row] = inverse * relative[row] + weight * ( relative[0] * params->torsoAxis[0][row] +
					relative[1] * params->torsoAxis[1][row] + relative[2] * params->torsoAxis[2][row] ) + pivot[row];
			}
		} else {
			for ( row = 0; row < 3; ++row ) {
				float rotation[3];
				for ( column = 0; column < 3; ++column ) {
					rotation[column] = weight * params->torsoAxis[column][row];
					if ( row == column ) rotation[column] += inverse;
				}
				for ( column = 0; column < 3; ++column ) output->axis[row][column] =
					rotation[0] * raw[index].axis[0][column] + rotation[1] * raw[index].axis[1][column] + rotation[2] * raw[index].axis[2][column];
				output->origin[row] = rotation[0] * relative[0] + rotation[1] * relative[1] + rotation[2] * relative[2] + pivot[row];
			}
		}
		if ( !FnMds_FiniteTransform( output ) ) return 0;
	}
	memcpy( destination, &result, sizeof( result ) );
	return 1;
}

static inline int FnMds_DecodePose( const fnMdsView_t *view, const fnMdsPoseParams_t *params, fnMdsPose_t *destination ) {
	return FnMds_DecodePoseSelected( view, params, -1, destination );
}

static inline int FnMds_DecodeBone( const fnMdsView_t *view, const fnMdsPoseParams_t *params,
	uint32_t bone, fnMdsTransform_t *destination ) {
	fnMdsPose_t pose;
	if ( !view || !destination || bone >= view->numBones ||
		!FnMds_DecodePoseSelected( view, params, (int)bone, &pose ) ) return 0;
	*destination = pose.bones[bone];
	return 1;
}

static inline int FnMds_SkinVertex( const fnMdsVertexView_t *vertex, const fnMdsPose_t *pose, float xyz[3], float normal[3] ) {
	float position[3] = {0}, direction[3];
	uint32_t index;
	if ( !vertex || !pose || !xyz || !normal || !vertex->numWeights || !pose->numBones || pose->numBones > FN_MDS_MAX_BONES ) return 0;
	for ( index = 0; index < vertex->numWeights; ++index ) {
		fnMdsWeight_t weight;
		const fnMdsTransform_t *bone;
		int row;
		if ( !FnMds_Weight( vertex, index, &weight ) || weight.bone >= pose->numBones ) return 0;
		bone = &pose->bones[weight.bone];
		for ( row = 0; row < 3; ++row ) {
			position[row] += weight.weight * ( weight.offset[0] * bone->axis[row][0] + weight.offset[1] * bone->axis[row][1] +
				weight.offset[2] * bone->axis[row][2] + bone->origin[row] );
			/* RTCW transforms normals with the first weight's bone only. */
			if ( !index ) direction[row] = vertex->normal[0] * bone->axis[row][0] + vertex->normal[1] * bone->axis[row][1] + vertex->normal[2] * bone->axis[row][2];
		}
	}
	for ( index = 0; index < 3; ++index ) if ( !isfinite( position[index] ) || !isfinite( direction[index] ) ) return 0;
	memcpy( xyz, position, sizeof( position ) );
	memcpy( normal, direction, sizeof( direction ) );
	return 1;
}

#endif
