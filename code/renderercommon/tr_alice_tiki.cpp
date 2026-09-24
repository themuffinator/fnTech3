/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* See tr_alice_tiki.h. Only the engine's data types are used here: every
 * definition arrives through refimport_t, and nothing in the engine is called
 * directly, because this is part of a separately loaded renderer. */

#include "../qcommon/q_shared.h"
#include "../qcommon/alice_tiki.h"
#include "../compat/alice_tiki_lod.hpp"
#include "tr_alice_tiki.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

using namespace fntech3::alice;
using fntech3::alice::tiki::File;
using fntech3::alice::tiki::Model;

namespace {

const Model *AsModel( const void *model ) {
	return static_cast<const Model *>( model );
}

/* ---- the file whose surfaces the model draws ---------------------------- */

/* A skeletal model draws its skeleton's surfaces; a vertex-animated one draws
 * the surfaces its animations share, which the definition checked agree. */
const File *ShapeFile( const Model *m ) {
	if ( m->skeleton ) return m->skeleton;
	for ( const auto &a : m->animations ) {
		if ( a.file && a.file->kind == File::Kind::Tan ) return a.file;
	}
	return nullptr;
}

/* A skeleton's surface, as the engine's full reader (ParseSkb) located it
 * when the file was read: the triangle and vertex counts, the lowest level of
 * detail, and where the triangles, the variable-stride vertices and the
 * collapse map begin. That reader refuses a skeleton whose chain of surfaces
 * does not tile the file exactly, so every record is inside the file and a
 * lookup is an index, not a walk from the first surface. */
const model::SkbSurface *SkelSurfaceAt( const File *f, int index ) {
	if ( index < 0 || static_cast<std::size_t>( index ) >= f->skbSurfaces.size() ) return nullptr;
	return &f->skbSurfaces[static_cast<std::size_t>( index )];
}

/* ---- this frame's poses ------------------------------------------------- */

/* Poses are handed out of fixed blocks so a pointer stays good until the
 * next frame begins, however many entities are posed before then. */
constexpr std::size_t kBlockBones = 16384;
std::vector<std::unique_ptr<pose::BoneTransform[]>> poseBlocks;
std::size_t poseBlock = 0;
std::size_t poseUsed = 0;

pose::BoneTransform *AllocBones( int count ) {
	if ( count <= 0 || static_cast<std::size_t>( count ) > kBlockBones ) return nullptr;
	if ( poseBlock < poseBlocks.size() && poseUsed + count > kBlockBones ) {
		++poseBlock;
		poseUsed = 0;
	}
	if ( poseBlock >= poseBlocks.size() ) {
		poseBlocks.emplace_back( new pose::BoneTransform[kBlockBones] );
		poseUsed = 0;
	}
	pose::BoneTransform *out = poseBlocks[poseBlock].get() + poseUsed;
	poseUsed += static_cast<std::size_t>( count );
	return out;
}

const pose::SkeletalAnim *AnimView( const Model *m, int anim ) {
	if ( anim < 0 || anim >= static_cast<int>( m->animations.size() ) ) return nullptr;
	const File *f = m->animations[anim].file;
	return f && f->kind == File::Kind::SkeletalAnim ? &f->anim : nullptr;
}

const File *TanFile( const Model *m, int anim ) {
	if ( anim < 0 || anim >= static_cast<int>( m->animations.size() ) ) return nullptr;
	const File *f = m->animations[anim].file;
	return f && f->kind == File::Kind::Tan ? f : nullptr;
}

/* A frame an animation does not have is drawn as its first: retail's
 * renderer puts 0 in its place (alice.exe 0x004a6437 for a vertex-animated
 * model, 0x004a2001 and 0x004a21b1 for a skeletal one) where Quake III's
 * MD3 code would clamp to the last. RA_TikiCheckAnimation corrects the entity
 * itself so; this answers the same for one it has not corrected. */
int FrameIn( int frame, int frames ) {
	return frame >= 0 && frame < frames ? frame : 0;
}

int SkelFrame( const pose::SkeletalAnim *a, int frame ) {
	return a ? FrameIn( frame, a->frames ) : 0;
}

/* The frame retail measures an entity by, for its level of detail and the
 * point it is lit at: a vertex-animated model's current frame of its current
 * animation (0x004a643d), a skeletal one's legs frame while its legs
 * animation is in use and its torso frame otherwise (0x004a3b62..0x004a3c64,
 * kept at entity +0x1ac). */
struct FrameView {
	bool ok = false;
	float mins[3] = { 0.0f, 0.0f, 0.0f };
	float maxs[3] = { 0.0f, 0.0f, 0.0f };
	float radius = 0.0f;
};

FrameView CurrentFrame( const Model *m, const aliceRenderEntity_t *e ) {
	FrameView out;
	if ( m->skeleton ) {
		const pose::SkeletalAnim *a = AnimView( m, e->useLegs ? e->anim : e->torsoAnim );
		if ( !a ) return out;
		const int frame = SkelFrame( a, e->useLegs ? e->frame : e->torsoFrame );
		pose::FrameBounds( *a, frame, out.mins, out.maxs );
		out.radius = pose::FrameRadius( *a, frame );
		out.ok = true;
		return out;
	}
	const File *cur = TanFile( m, e->anim );
	if ( !cur || cur->tan.frames <= 0 ) return out;
	const int frame = FrameIn( e->frame, cur->tan.frames );
	const pose::TanFrame f = pose::ReadTanFrame( cur->bytes.data() + cur->tan.frameData
		+ pose::TanFrameBytes * static_cast<std::size_t>( frame ) );
	VectorCopy( f.mins, out.mins );
	VectorCopy( f.maxs, out.maxs );
	out.radius = f.radius;
	out.ok = true;
	return out;
}

float EntityScale( const Model *m, const aliceRenderEntity_t *e ) {
	return ( e->scale > 0.0f ? e->scale : 1.0f ) * m->scale;
}

/* ---- retail's checks of an entity's animation state --------------------- */

using Report = void ( QDECL * )( printParm_t level, const char *fmt, ... );

bool AnimInRange( const Model *m, int anim ) {
	return anim >= 0 && anim < static_cast<int>( m->animations.size() );
}

/* An animation's frame count, from the file of the kind the model draws
 * with; -1 when that file is missing, where retail's renderer prints
 * "couldn't get anim" instead and, for the animations it draws from, stops
 * (0x004a63f1, 0x004a1fbf). A definition compiled from text never has one:
 * its loader drops an animation whose file does not load. */
int AnimFrames( const Model *m, int anim ) {
	if ( m->skeleton ) {
		const pose::SkeletalAnim *a = AnimView( m, anim );
		return a ? a->frames : -1;
	}
	const File *f = TanFile( m, anim );
	return f ? f->tan.frames : -1;
}

/* A skeletal model's animation and frame, as 0x004a1f60 checks them (and
 * 0x004a2110 for a version 2 skeleton, which reads the frame count where
 * version 2 keeps it). False when the animation's file is missing. */
bool CheckSkelPair( const Model *m, int &anim, int &frame, const char *name, Report report ) {
	if ( !AnimInRange( m, anim ) ) {
		report( PRINT_DEVELOPER, "R_AddSkelSurfaces: no such anim %d for '%s'\n", anim, name );
		anim = 0;
		frame = 0;
	}
	const int frames = AnimFrames( m, anim );
	if ( frames < 0 ) return false;
	if ( frame < 0 || frame >= frames ) {
		report( PRINT_DEVELOPER, "R_AddSkelSurfaces: no such frame %d in anim %d for '%s'\n", frame, anim, name );
		frame = 0;
	}
	return true;
}

void QDECL SilentReport( printParm_t, const char *, ... ) {
}

/* ---- level of detail ---------------------------------------------------- */

/* The remap of the surface being tessellated; the back end draws one surface
 * at a time. */
std::vector<int> lodMap;

/* How many vertices a surface keeps at `fraction`, with lodMap filled for
 * them. A surface whose collapse map cannot be followed stays whole. */
int LodCount( int numVerts, int minLod, float fraction, const unsigned char *collapse ) {
	int count = lod::VertexCount( numVerts, minLod, fraction );
	if ( count <= 0 ) return 0;
	if ( count < numVerts ) {
		if ( lodMap.size() < static_cast<std::size_t>( numVerts ) ) lodMap.resize( numVerts );
		if ( !collapse || !lod::Remap( collapse, numVerts, count, lodMap.data() ) ) count = numVerts;
	}
	if ( lodMap.empty() ) lodMap.resize( 1 );
	return count;
}

/* The packed normal a vertex-animated vertex carries, as Quake III packs an
 * MD3's: a latitude in the high byte and a longitude in the low one, each a
 * 256th of a turn -- retail indexes its 1,024-entry sine table with the byte
 * times four (0x0049fa6c..0x0049faa7). */
void DecodeNormal( unsigned short packed, float out[3] ) {
	const float lat = ( ( packed >> 8 ) & 0xff ) * ( 2.0f * static_cast<float>( M_PI ) / 256.0f );
	const float lng = ( packed & 0xff ) * ( 2.0f * static_cast<float>( M_PI ) / 256.0f );
	out[0] = std::cos( lat ) * std::sin( lng );
	out[1] = std::sin( lat ) * std::sin( lng );
	out[2] = std::cos( lng );
}

} // namespace

extern "C" {

int RA_TikiNumSurfaces( const void *model ) {
	return model ? static_cast<int>( AsModel( model )->surfaces.size() ) : 0;
}

int RA_TikiSurfaceNumSkins( const void *model, int surface ) {
	const Model *m = AsModel( model );
	if ( !m || surface < 0 || surface >= static_cast<int>( m->surfaces.size() ) ) return 0;
	return m->surfaces[surface].numSkins;
}

const char *RA_TikiSurfaceShader( const void *model, int surface, int skin ) {
	const Model *m = AsModel( model );
	if ( !m || surface < 0 || surface >= static_cast<int>( m->surfaces.size() ) ) return nullptr;
	const auto &s = m->surfaces[surface];
	if ( s.numSkins <= 0 ) return nullptr;
	if ( skin < 0 || skin >= s.numSkins ) skin = 0;
	return s.shaders[skin];
}

int RA_TikiSurfaceFlags( const void *model, int surface ) {
	const Model *m = AsModel( model );
	if ( !m || surface < 0 || surface >= static_cast<int>( m->surfaces.size() ) ) return 0;
	return m->surfaces[surface].flags;
}

int RA_TikiSurfaceSize( const void *model, int surface, int *numVerts, int *numIndexes ) {
	*numVerts = *numIndexes = 0;
	const Model *m = AsModel( model );
	if ( !m ) return 0;
	const File *shape = ShapeFile( m );
	if ( !shape ) return 0;
	if ( shape->kind == File::Kind::Skeleton ) {
		const model::SkbSurface *s = SkelSurfaceAt( shape, surface );
		if ( !s ) return 0;
		*numVerts = s->verts;
		*numIndexes = s->triangles * 3;
	} else {
		if ( surface < 0 || surface >= shape->tan.surfaces ) return 0;
		*numVerts = shape->tan.surface[surface].verts;
		*numIndexes = shape->tan.surface[surface].triangles * 3;
	}
	return *numVerts > 0 && *numIndexes > 0;
}

void RA_TikiModelBounds( const void *model, float mins[3], float maxs[3] ) {
	const Model *m = AsModel( model );
	if ( !m ) {
		VectorClear( mins );
		VectorClear( maxs );
		return;
	}
	VectorScale( m->mins, m->scale, mins );
	VectorScale( m->maxs, m->scale, maxs );
}

float RA_TikiModelRadius( const void *model ) {
	const Model *m = AsModel( model );
	return m ? m->radius : 0.0f;
}

qboolean RA_TikiIsSkeletal( const void *model ) {
	const Model *m = AsModel( model );
	return m && m->skeleton ? qtrue : qfalse;
}

void RA_TikiCheckAnimation( const void *model, aliceRenderEntity_t *e, const char *name,
	void ( QDECL *print )( printParm_t level, const char *fmt, ... ) ) {
	const Model *m = AsModel( model );
	if ( !m || !e ) return;
	const Report report = print ? print : SilentReport;
	if ( !name ) name = "";

	if ( m->skeleton ) {
		/* R_AddSkelSurfaces (0x004a3a10, 0x004a2420): the legs' pairs when
		 * the legs animation is in use, then the torso's three when the
		 * torso's is, then the legs' cross-blend when the legs are in use
		 * and its weight is not negative (0x004a3e82). Retail culls on the
		 * legs' frames between the first two groups and before the last;
		 * this renderer culls on the model's bounds, so it checks them all
		 * first, and only a culled entity's messages can differ. A missing
		 * legs or torso file ends the checks, as it ends retail's draw. */
		if ( e->useLegs ) {
			if ( !CheckSkelPair( m, e->anim, e->frame, name, report ) ) return;
			CheckSkelPair( m, e->oldanim, e->oldframe, name, report );
		}
		if ( e->useTorso ) {
			if ( !CheckSkelPair( m, e->torsoAnim, e->torsoFrame, name, report ) ) return;
			CheckSkelPair( m, e->torsoOldanim, e->torsoOldframe, name, report );
			CheckSkelPair( m, e->torsoCrossblendAnim, e->torsoCrossblendFrame, name, report );
		}
		if ( e->useLegs && e->crossblendLerp >= 0.0f ) {
			CheckSkelPair( m, e->crossblendAnim, e->crossblendFrame, name, report );
		}
		return;
	}

	/* R_AddTikiSurfaces (0x004a63a0..0x004a64e3). Its message for a bad old
	 * animation prints the current animation's number, as retail's does. */
	if ( !AnimInRange( m, e->anim ) ) {
		report( PRINT_DEVELOPER, "R_AddTikiSurfaces: no such anim %d for '%s'\n", e->anim, name );
		e->anim = 0;
		e->frame = 0;
	}
	const int frames = AnimFrames( m, e->anim );
	if ( frames < 0 ) return;
	if ( e->frame < 0 || e->frame >= frames ) {
		report( PRINT_DEVELOPER, "R_AddTikiSurfaces: no such frame %d in anim %d for '%s'\n", e->frame, e->anim, name );
		e->frame = 0;
	}
	if ( !AnimInRange( m, e->oldanim ) ) {
		report( PRINT_DEVELOPER, "R_AddTikiSurfaces: no such oldanim %d for '%s'\n", e->anim, name );
		e->oldanim = 0;
		e->oldframe = 0;
	}
	const int oldFrames = AnimFrames( m, e->oldanim );
	if ( oldFrames < 0 ) return;
	if ( e->oldframe < 0 || e->oldframe >= oldFrames ) {
		report( PRINT_DEVELOPER, "R_AddTikiSurfaces: no such oldframe %d in anim %d for '%s'\n", e->oldframe,
			e->oldanim, name );
		e->oldframe = 0;
	}
}

qboolean RA_TikiLodSphere( const void *model, const aliceRenderEntity_t *e, float center[3], float *radius ) {
	VectorClear( center );
	*radius = 0.0f;
	const Model *m = AsModel( model );
	if ( !m || !e ) return qfalse;
	const FrameView f = CurrentFrame( m, e );
	if ( !f.ok ) return qfalse;
	const float scale = EntityScale( m, e );
	/* The model's origin is scaled but not turned with the entity: retail
	 * adds it to the entity's origin as it stands (0x004a6784, 0x004a3c68). */
	VectorScale( m->origin, scale, center );
	*radius = f.radius * scale;
	return qtrue;
}

float RA_TikiLodFraction( const void *model, float projectedRadius, float lodScaleCvar, float lodBiasCvar ) {
	const Model *m = AsModel( model );
	if ( !m ) return 1.0f;
	return lod::Fraction( projectedRadius, lodScaleCvar, lodBiasCvar, m->lodScale, m->lodBias );
}

qboolean RA_TikiLightingPoint( const void *model, const aliceRenderEntity_t *e, float point[3] ) {
	VectorClear( point );
	const Model *m = AsModel( model );
	if ( !m || !e ) return qfalse;
	const FrameView f = CurrentFrame( m, e );
	if ( !f.ok ) return qfalse;
	const float scale = EntityScale( m, e );
	/* The middle of the frame's bounds, moved by the model's origin and its
	 * `lightoffset`, and scaled (0x004aab31..0x004aabd1). */
	for ( int k = 0; k < 3; ++k ) {
		point[k] = ( ( f.mins[k] + f.maxs[k] ) * 0.5f + m->origin[k] + m->lightOffset[k] ) * scale;
	}
	return qtrue;
}

const void *RA_TikiPose( const void *model, const aliceRenderEntity_t *e ) {
	const Model *m = AsModel( model );
	if ( !m || !m->skeleton || !e ) return nullptr;
	const pose::Skeleton &skeleton = m->skeleton->skeleton;

	pose::Sample legs, torso;
	legs.anim = AnimView( m, e->anim );
	legs.frame = SkelFrame( legs.anim, e->frame );
	legs.oldAnim = AnimView( m, e->oldanim );
	legs.oldFrame = SkelFrame( legs.oldAnim, e->oldframe );
	legs.backlerp = e->backlerp;
	legs.crossAnim = AnimView( m, e->crossblendAnim );
	legs.crossFrame = SkelFrame( legs.crossAnim, e->crossblendFrame );
	legs.crossWeight = legs.crossAnim ? e->crossblendLerp : -1.0f;
	torso.anim = AnimView( m, e->torsoAnim );
	torso.frame = SkelFrame( torso.anim, e->torsoFrame );
	torso.oldAnim = AnimView( m, e->torsoOldanim );
	torso.oldFrame = SkelFrame( torso.oldAnim, e->torsoOldframe );
	torso.backlerp = e->torsoBacklerp;
	torso.crossAnim = AnimView( m, e->torsoCrossblendAnim );
	torso.crossFrame = SkelFrame( torso.crossAnim, e->torsoCrossblendFrame );
	torso.crossWeight = torso.crossAnim ? e->torsoCrossblendLerp : -1.0f;

	/* With neither animation in use retail's renderer refuses the model. */
	const bool legsOn = e->useLegs && legs.anim, torsoOn = e->useTorso && torso.anim;
	if ( !legsOn && !torsoOn ) return nullptr;

	pose::BoneTransform *bones = AllocBones( skeleton.bones );
	if ( !bones ) return nullptr;
	std::vector<pose::LocalPose> local( skeleton.bones );
	if ( legsOn && torsoOn ) {
		pose::PoseLocal( skeleton, legs, &torso, local.data() );
	} else {
		pose::PoseLocal( skeleton, legsOn ? legs : torso, nullptr, local.data() );
	}

	/* Retail's renderer keeps one controller a bone and uses them only when
	 * the entity says to (its useAngles word). */
	pose::Controller controllers[5];
	int numControllers = 0;
	if ( e->useControllers ) {
		for ( const aliceBoneController_t &c : e->controllers ) {
			if ( c.bone < 0 || c.bone >= skeleton.bones ) continue;
			/* A record the module cleared and never set is no rotation. */
			if ( c.quat[0] == 0.0f && c.quat[1] == 0.0f && c.quat[2] == 0.0f && c.quat[3] == 0.0f ) continue;
			controllers[numControllers].bone = c.bone;
			controllers[numControllers].q = pose::Quat{ c.quat[0], c.quat[1], c.quat[2], c.quat[3] };
			controllers[numControllers].scale = c.scale;
			++numControllers;
		}
	}
	pose::PoseWorld( skeleton, local.data(), controllers, numControllers, bones );
	return bones;
}

int RA_TikiTessellate( const void *model, int surface, const aliceRenderEntity_t *e,
	const void *posed, float ( *xyz )[4], float ( *normal )[4], float ( *st )[2],
	unsigned int *indexes, unsigned int firstVertex, int maxVerts, int maxIndexes,
	int *outVerts, int *outIndexes ) {
	return RA_TikiTessellateLod( model, surface, e, posed, 1.0f, xyz, normal, st, indexes,
		firstVertex, maxVerts, maxIndexes, outVerts, outIndexes );
}

int RA_TikiTessellateLod( const void *model, int surface, const aliceRenderEntity_t *e,
	const void *posed, float lodFraction, float ( *xyz )[4], float ( *normal )[4], float ( *st )[2],
	unsigned int *indexes, unsigned int firstVertex, int maxVerts, int maxIndexes,
	int *outVerts, int *outIndexes ) {
	*outVerts = *outIndexes = 0;
	const Model *m = AsModel( model );
	if ( !m || !e ) return 0;
	const File *shape = ShapeFile( m );
	if ( !shape ) return 0;
	const float scale = EntityScale( m, e );

	if ( shape->kind == File::Kind::Skeleton ) {
		const auto *bones = static_cast<const pose::BoneTransform *>( posed );
		const model::SkbSurface *s = SkelSurfaceAt( shape, surface );
		if ( !bones || !s ) return 0;
		if ( s->verts > maxVerts || s->triangles > maxIndexes / 3 ) return 0;
		const unsigned char *d = shape->bytes.data();
		const std::size_t size = shape->bytes.size();
		const int numBones = shape->skeleton.bones;

		/* Only the vertices the level of detail keeps are skinned: the vertex
		 * block is walked in order and left at the count (0x004a01d5). */
		const int count = LodCount( s->verts, s->minLod, lodFraction, s->collapseData ? d + s->collapseData : nullptr );
		if ( count <= 0 ) return 0;
		std::size_t at = s->vertexData;
		for ( int v = 0; v < count; ++v ) {
			if ( at + 24 > size ) return 0;
			float n[3], tc[2];
			std::int32_t numWeights;
			std::memcpy( n, d + at, 12 );
			std::memcpy( tc, d + at + 12, 8 );
			std::memcpy( &numWeights, d + at + 20, 4 );
			at += 24;
			if ( numWeights < 0 || at > size || static_cast<std::size_t>( numWeights ) > ( size - at ) / 20u ) return 0;
			float pos[3] = { 0.0f, 0.0f, 0.0f };
			int firstBone = -1;
			for ( int w = 0; w < numWeights; ++w ) {
				std::int32_t bone;
				float weight, offset[3];
				std::memcpy( &bone, d + at, 4 );
				std::memcpy( &weight, d + at + 4, 4 );
				std::memcpy( offset, d + at + 8, 12 );
				at += 20;
				if ( bone < 0 || bone >= numBones ) continue;
				if ( firstBone < 0 ) firstBone = bone;
				pose::AddWeight( bones[bone], weight, offset, pos );
			}
			VectorScale( pos, scale, xyz[v] );
			xyz[v][3] = 1.0f;
			/* The normal goes through the first weight's bone only, and
			 * retail leaves it unnormalised. */
			if ( firstBone >= 0 ) {
				pose::RotateT( n, bones[firstBone].axis, normal[v] );
			} else {
				VectorCopy( n, normal[v] );
			}
			normal[v][3] = 0.0f;
			st[v][0] = tc[0];
			st[v][1] = tc[1];
		}
		const int written = lod::Triangles( d + s->triangleData, s->triangles, s->verts, count,
			lodMap.data(), firstVertex, indexes );
		if ( written < 0 ) return 0;
		*outVerts = count;
		*outIndexes = written;
		return 1;
	}

	/* Vertex animation: the current frame of the current animation, lerped
	 * from the old frame of the old one, both against their own frame's scale
	 * and offset. */
	const File *cur = TanFile( m, e->anim );
	if ( !cur ) return 0;
	const File *old = TanFile( m, e->oldanim );
	if ( surface < 0 || surface >= cur->tan.surfaces ) return 0;
	const model::TanSurface &cs = cur->tan.surface[surface];
	if ( cs.verts > maxVerts || cs.triangles > maxIndexes / 3 ) return 0;
	const int frame = FrameIn( e->frame, cur->tan.frames );
	const pose::TanFrame cf = pose::ReadTanFrame( cur->bytes.data() + cur->tan.frameData
		+ pose::TanFrameBytes * static_cast<std::size_t>( frame ) );
	const float backlerp = old && old->tan.surfaces == cur->tan.surfaces ? e->backlerp : 0.0f;
	const float front = 1.0f - backlerp;
	pose::TanFrame of = cf;
	const unsigned char *oldVerts = nullptr;
	if ( backlerp != 0.0f ) {
		const int oframe = FrameIn( e->oldframe, old->tan.frames );
		of = pose::ReadTanFrame( old->bytes.data() + old->tan.frameData
			+ pose::TanFrameBytes * static_cast<std::size_t>( oframe ) );
		const model::TanSurface &os = old->tan.surface[surface];
		if ( os.verts != cs.verts ) return 0;
		oldVerts = old->bytes.data() + os.xyzNormalData + 8u * static_cast<std::size_t>( oframe ) * os.verts;
	}
	const unsigned char *curVerts = cur->bytes.data() + cs.xyzNormalData
		+ 8u * static_cast<std::size_t>( frame ) * cs.verts;
	const unsigned char *tcs = cur->bytes.data() + cs.texCoordData;
	/* Only the vertices the level of detail keeps are computed (0x0049fe64
	 * is handed the count). */
	const int count = LodCount( cs.verts, cs.minLod, lodFraction, cur->bytes.data() + cs.collapseData );
	if ( count <= 0 ) return 0;
	for ( int v = 0; v < count; ++v ) {
		unsigned short q[4];
		std::memcpy( q, curVerts + 8 * v, 8 );
		float n[3];
		DecodeNormal( q[3], n );
		for ( int k = 0; k < 3; ++k ) {
			xyz[v][k] = ( q[k] * cf.scale[k] + cf.offset[k] ) * front;
		}
		if ( oldVerts ) {
			unsigned short oq[4];
			std::memcpy( oq, oldVerts + 8 * v, 8 );
			float on[3];
			DecodeNormal( oq[3], on );
			for ( int k = 0; k < 3; ++k ) {
				xyz[v][k] += ( oq[k] * of.scale[k] + of.offset[k] ) * backlerp;
				n[k] = n[k] * front + on[k] * backlerp;
			}
			VectorNormalize( n );
		}
		for ( int k = 0; k < 3; ++k ) {
			xyz[v][k] = ( xyz[v][k] + m->origin[k] ) * scale;
		}
		xyz[v][3] = 1.0f;
		VectorCopy( n, normal[v] );
		normal[v][3] = 0.0f;
		std::memcpy( st[v], tcs + 8 * v, 8 );
	}
	const int written = lod::Triangles( cur->bytes.data() + cs.triangleData, cs.triangles, cs.verts,
		count, lodMap.data(), firstVertex, indexes );
	if ( written < 0 ) return 0;
	*outVerts = count;
	*outIndexes = written;
	return 1;
}

void RA_TikiBeginFrame( void ) {
	poseBlock = 0;
	poseUsed = 0;
}

} // extern "C"
