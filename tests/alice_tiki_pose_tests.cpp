/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for how Alice poses a skeleton. Every
 * skeleton and animation here is written by this test; no retail file is
 * used. The rules under test were measured against retail alice.exe and the
 * retail files (docs/fntech3/2026-09-23-alice-tiki.md); these checks pin them
 * so a later change cannot quietly compose parent-first, drop the transpose,
 * or read a version 2 animation at version 3's offsets. */

#include "../code/compat/alice_tiki_pose.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::pose;
using Bytes = std::vector<std::uint8_t>;

static bool Near( float a, float b, float eps = 1e-4f )
{
	return std::fabs( a - b ) <= eps;
}

static bool NearQuat( const Quat &a, const Quat &b, float eps = 1e-4f )
{
	return Near( a.x, b.x, eps ) && Near( a.y, b.y, eps ) && Near( a.z, b.z, eps ) && Near( a.w, b.w, eps );
}

static void PutInt( Bytes &file, std::size_t at, std::int32_t value )
{
	std::memcpy( file.data() + at, &value, sizeof( value ) );
}

static void PutFloat( Bytes &file, std::size_t at, float value )
{
	std::memcpy( file.data() + at, &value, sizeof( value ) );
}

static void PutShort( Bytes &file, std::size_t at, std::int16_t value )
{
	std::memcpy( file.data() + at, &value, sizeof( value ) );
}

struct BoneSpec {
	int parent;
	int flags;
	const char *name;
};

static Bytes SkeletonFile( int version, const std::vector<BoneSpec> &bones )
{
	const std::size_t ofsBones = 96;
	Bytes file( ofsBones + BoneBytes * bones.size(), 0 );
	PutInt( file, 0, std::int32_t( SkeletonIdent ) );
	PutInt( file, 4, version );
	PutInt( file, 72, 0 );
	PutInt( file, 76, std::int32_t( bones.size() ) );
	PutInt( file, 80, std::int32_t( ofsBones ) );
	PutInt( file, 84, std::int32_t( file.size() ) );
	for ( std::size_t b = 0; b < bones.size(); ++b ) {
		const std::size_t at = ofsBones + BoneBytes * b;
		PutInt( file, at, bones[b].parent );
		PutInt( file, at + 4, bones[b].flags );
		std::memcpy( file.data() + at + 8, bones[b].name, std::strlen( bones[b].name ) );
	}
	return file;
}

struct PoseSpec {
	Quat q;
	float t[3];
};

/* One animation of `frames` frames over `bones` bones; pose(frame, bone)
 * gives each stored pose. Version 3 packs them as shorts, version 2 as
 * floats, and the counts sit at 76 or 72. */
template <class PoseOf>
static Bytes AnimFile( int version, int frames, int bones, PoseOf pose )
{
	const std::size_t ofsFrames = 112;
	const std::size_t poseBytes = version == 3 ? 16 : 28;
	const std::size_t stride = FrameHeaderBytes + poseBytes * bones;
	Bytes file( ofsFrames + stride * frames, 0 );
	PutInt( file, 0, std::int32_t( SkeletalAnimIdent ) );
	PutInt( file, 4, version );
	const std::size_t counts = version == 3 ? 76 : 72;
	PutInt( file, counts, frames );
	PutInt( file, counts + 4, bones );
	PutFloat( file, counts + 8, 0.1f * frames );
	PutFloat( file, counts + 12, 0.1f );
	PutFloat( file, counts + 16, 7.0f );
	PutInt( file, 104, std::int32_t( ofsFrames ) );
	for ( int f = 0; f < frames; ++f ) {
		const std::size_t frameAt = ofsFrames + stride * f;
		PutFloat( file, frameAt, -1.0f - f );      /* mins x */
		PutFloat( file, frameAt + 12, 1.0f + f );  /* maxs x */
		PutFloat( file, frameAt + 24, 2.0f + f );  /* radius */
		PutFloat( file, frameAt + 28, 3.0f + f );  /* delta x */
		for ( int b = 0; b < bones; ++b ) {
			const PoseSpec p = pose( f, b );
			const std::size_t at = frameAt + FrameHeaderBytes + poseBytes * b;
			if ( version == 3 ) {
				PutShort( file, at, std::int16_t( std::lround( p.q.x * 32767.0f ) ) );
				PutShort( file, at + 2, std::int16_t( std::lround( p.q.y * 32767.0f ) ) );
				PutShort( file, at + 4, std::int16_t( std::lround( p.q.z * 32767.0f ) ) );
				PutShort( file, at + 6, std::int16_t( std::lround( p.q.w * 32767.0f ) ) );
				for ( int k = 0; k < 3; ++k ) {
					PutShort( file, at + 8 + 2 * k, std::int16_t( std::lround( p.t[k] * 63.0f ) ) );
				}
			} else {
				PutFloat( file, at, p.q.x );
				PutFloat( file, at + 4, p.q.y );
				PutFloat( file, at + 8, p.q.z );
				PutFloat( file, at + 12, p.q.w );
				for ( int k = 0; k < 3; ++k ) PutFloat( file, at + 16 + 4 * k, p.t[k] );
			}
		}
	}
	return file;
}

static Quat AxisAngle( float x, float y, float z, float degrees )
{
	const float h = degrees * 3.14159265358979f / 360.0f;
	const float s = std::sin( h );
	return Quat{ x * s, y * s, z * s, std::cos( h ) };
}

static void CheckQuaternionRules()
{
	/* Hamilton order on (x, y, z, w): i j = k and j i = -k. */
	const Quat i{ 1, 0, 0, 0 }, j{ 0, 1, 0, 0 };
	CHECK( NearQuat( Mul( i, j ), Quat{ 0, 0, 1, 0 } ) );
	CHECK( NearQuat( Mul( j, i ), Quat{ 0, 0, -1, 0 } ) );
	CHECK( NearQuat( Mul( IdentityQuat, AxisAngle( 0, 0, 1, 30 ) ), AxisAngle( 0, 0, 1, 30 ) ) );

	/* The matrix is stored with rows as axes, and a bone-local vector is
	 * v0*row0 + v1*row1 + v2*row2. A quarter turn about z therefore sends
	 * local x to model -y: this is the transpose that the untransposed
	 * misreading drops. */
	float m[3][3];
	ToMatrix( AxisAngle( 0, 0, 1, 90 ), 1.0f, m );
	const float x[3] = { 1, 0, 0 };
	float out[3];
	RotateT( x, m, out );
	CHECK( Near( out[0], 0 ) && Near( out[1], -1 ) && Near( out[2], 0 ) );

	/* A scale multiplies the whole matrix. */
	ToMatrix( IdentityQuat, 2.5f, m );
	CHECK( Near( m[0][0], 2.5f ) && Near( m[1][1], 2.5f ) && Near( m[2][2], 2.5f ) && Near( m[0][1], 0 ) );

	/* Slerp: endpoints, the short arc, and the near-equal linear fallback,
	 * which is not renormalised. */
	const Quat a = AxisAngle( 0, 0, 1, 0 ), b = AxisAngle( 0, 0, 1, 90 );
	CHECK( NearQuat( Slerp( a, b, 0.0f ), a ) );
	CHECK( NearQuat( Slerp( a, b, 1.0f ), b ) );
	CHECK( NearQuat( Slerp( a, b, 0.5f ), AxisAngle( 0, 0, 1, 45 ) ) );
	const Quat negB{ -b.x, -b.y, -b.z, -b.w };
	CHECK( NearQuat( Slerp( a, negB, 0.5f ), AxisAngle( 0, 0, 1, 45 ) ) );
	const Quat c{ 0.0f, 0.0f, 0.0f, 1.0f }, d{ 0.0f, 0.0f, 0.0005f, 1.0f };
	const Quat mid = Slerp( c, d, 0.5f );
	CHECK( Near( mid.z, 0.00025f, 1e-7f ) && Near( mid.w, 1.0f, 1e-7f ) );
}

static void CheckSkeletonRefusals()
{
	const std::vector<BoneSpec> good = { { -1, 0, "root" }, { 0, 1, "leg" }, { 0, 0, "spine" } };
	Bytes file = SkeletonFile( 3, good );
	Skeleton s;
	CHECK( OpenSkeleton( file.data(), file.size(), s ) );
	CHECK( s.bones == 3 && BoneParent( s, 2 ) == 0 && BoneFlags( s, 1 ) == 1 );
	CHECK( !std::strcmp( BoneName( s, 2 ), "spine" ) );
	file = SkeletonFile( 2, good );
	CHECK( OpenSkeleton( file.data(), file.size(), s ) && s.version == 2 );

	/* A bad identity, an unknown version, a truncated file. */
	file = SkeletonFile( 3, good );
	file[0] = 'X';
	CHECK( !OpenSkeleton( file.data(), file.size(), s ) );
	file = SkeletonFile( 4, good );
	CHECK( !OpenSkeleton( file.data(), file.size(), s ) );
	file = SkeletonFile( 3, good );
	CHECK( !OpenSkeleton( file.data(), file.size() - 1, s ) );
	CHECK( !OpenSkeleton( file.data(), 91, s ) );
	CHECK( !OpenSkeleton( nullptr, 0, s ) );

	/* A parent after its child, or itself, would leave a hierarchy pass
	 * reading an unposed bone; a name without a terminator would run off
	 * the record. */
	file = SkeletonFile( 3, { { -1, 0, "root" }, { 2, 0, "a" }, { 0, 0, "b" } } );
	CHECK( !OpenSkeleton( file.data(), file.size(), s ) );
	file = SkeletonFile( 3, { { -1, 0, "root" }, { 1, 0, "self" } } );
	CHECK( !OpenSkeleton( file.data(), file.size(), s ) );
	file = SkeletonFile( 3, { { -2, 0, "root" } } );
	CHECK( !OpenSkeleton( file.data(), file.size(), s ) );
	file = SkeletonFile( 3, good );
	std::memset( file.data() + 96 + 8, 'n', 64 );
	CHECK( !OpenSkeleton( file.data(), file.size(), s ) );

	/* Retail refuses more than two hundred bones (0x00459302). */
	std::vector<BoneSpec> many( MaxBones + 1, BoneSpec{ 0, 0, "b" } );
	many[0].parent = -1;
	file = SkeletonFile( 3, many );
	CHECK( !OpenSkeleton( file.data(), file.size(), s ) );
	many.resize( MaxBones );
	file = SkeletonFile( 3, many );
	CHECK( OpenSkeleton( file.data(), file.size(), s ) );
}

static void CheckAnimations()
{
	auto pose = []( int f, int b ) {
		return PoseSpec{ AxisAngle( 0, 0, 1, 10.0f * f + b ), { 1.0f + f, 0.5f * b, -2.0f } };
	};
	for ( int version : { 2, 3 } ) {
		const Bytes file = AnimFile( version, 3, 2, pose );
		SkeletalAnim a;
		CHECK( OpenSkeletalAnim( file.data(), file.size(), a ) );
		CHECK( a.version == version && a.frames == 3 && a.bones == 2 );
		CHECK( a.stride == FrameHeaderBytes + ( version == 3 ? 16u : 28u ) * 2 );
		CHECK( Near( a.frameTime, 0.1f ) && Near( a.delta[0], 7.0f ) );

		float mins[3], maxs[3], delta[3];
		FrameBounds( a, 2, mins, maxs );
		CHECK( Near( mins[0], -3.0f ) && Near( maxs[0], 3.0f ) );
		CHECK( Near( FrameRadius( a, 1 ), 3.0f ) );
		FrameDelta( a, 0, delta );
		CHECK( Near( delta[0], 3.0f ) );
		/* Out-of-range frames clamp to the ends. */
		FrameBounds( a, 99, mins, maxs );
		CHECK( Near( maxs[0], 3.0f ) );
		FrameBounds( a, -5, mins, maxs );
		CHECK( Near( maxs[0], 1.0f ) );

		/* Version 3 stores a quaternion over 32,767 and an offset over 63. */
		const float eps = version == 3 ? 1.0f / 60.0f : 1e-6f;
		const LocalPose p = DecodePose( a, 2, 1 );
		CHECK( NearQuat( p.q, AxisAngle( 0, 0, 1, 21 ), version == 3 ? 1e-4f : 1e-6f ) );
		CHECK( Near( p.t[0], 3.0f, eps ) && Near( p.t[1], 0.5f, eps ) && Near( p.t[2], -2.0f, eps ) );
		const LocalPose none = DecodePose( a, 0, 5 );
		CHECK( NearQuat( none.q, IdentityQuat ) && none.t[0] == 0.0f );

		/* A frame count the file cannot hold is refused. */
		Bytes shortFile = file;
		shortFile.resize( shortFile.size() - 1 );
		CHECK( !OpenSkeletalAnim( shortFile.data(), shortFile.size(), a ) );
	}

	/* Reading version 2's counts at version 3's offset would take the bone
	 * count for the frame count; the version decides where they are. */
	const Bytes v2 = AnimFile( 2, 4, 1, pose );
	SkeletalAnim a;
	CHECK( OpenSkeletalAnim( v2.data(), v2.size(), a ) && a.frames == 4 && a.bones == 1 );

	Bytes bad = AnimFile( 3, 1, 1, pose );
	PutInt( bad, 4, 5 );
	CHECK( !OpenSkeletalAnim( bad.data(), bad.size(), a ) );
	bad = AnimFile( 3, 1, 1, pose );
	PutInt( bad, 76, 0 );
	CHECK( !OpenSkeletalAnim( bad.data(), bad.size(), a ) );
	bad = AnimFile( 3, 1, 1, pose );
	PutInt( bad, 104, -4 );
	CHECK( !OpenSkeletalAnim( bad.data(), bad.size(), a ) );
	CHECK( !OpenSkeletalAnim( bad.data(), 107, a ) );
}

static void CheckBlending()
{
	auto cur = []( int f, int ) { return PoseSpec{ AxisAngle( 0, 0, 1, f ? 90.0f : 0.0f ), { f ? 4.0f : 0.0f, 0, 0 } }; };
	auto other = []( int, int ) { return PoseSpec{ AxisAngle( 1, 0, 0, 90 ), { 0, 8.0f, 0 } }; };
	const Bytes curFile = AnimFile( 2, 2, 1, cur );
	const Bytes otherFile = AnimFile( 2, 1, 1, other );
	SkeletalAnim anim, cross;
	CHECK( OpenSkeletalAnim( curFile.data(), curFile.size(), anim ) );
	CHECK( OpenSkeletalAnim( otherFile.data(), otherFile.size(), cross ) );

	/* The old frame is weighted by the back-lerp. */
	Sample s;
	s.anim = &anim;
	s.frame = 1;
	s.oldAnim = &anim;
	s.oldFrame = 0;
	s.backlerp = 0.25f;
	LocalPose p = SampleBone( s, 0 );
	CHECK( NearQuat( p.q, AxisAngle( 0, 0, 1, 67.5f ) ) );
	CHECK( Near( p.t[0], 3.0f ) );

	/* A cross-blend weight is the new animation's share; zero is all the
	 * source's, one is all the new animation's, a negative one is none. */
	s.backlerp = 0.0f;
	s.crossAnim = &cross;
	s.crossFrame = 0;
	s.crossWeight = 0.0f;
	p = SampleBone( s, 0 );
	CHECK( NearQuat( p.q, AxisAngle( 1, 0, 0, 90 ) ) && Near( p.t[1], 8.0f ) );
	s.crossWeight = 1.0f;
	p = SampleBone( s, 0 );
	CHECK( NearQuat( p.q, AxisAngle( 0, 0, 1, 90 ) ) && Near( p.t[0], 4.0f ) && Near( p.t[1], 0.0f ) );
	s.crossWeight = 0.5f;
	p = SampleBone( s, 0 );
	CHECK( Near( p.t[0], 2.0f ) && Near( p.t[1], 4.0f ) );
	s.crossWeight = -1.0f;
	p = SampleBone( s, 0 );
	CHECK( NearQuat( p.q, AxisAngle( 0, 0, 1, 90 ) ) && Near( p.t[1], 0.0f ) );
}

static void CheckHierarchy()
{
	/* Three bones: a root, a leg under it and a spine under it. */
	const Bytes skelFile = SkeletonFile( 3, { { -1, 0, "root" }, { 0, 1, "leg" }, { 0, 0, "spine" } } );
	Skeleton skel;
	CHECK( OpenSkeleton( skelFile.data(), skelFile.size(), skel ) );

	auto legs = []( int, int b ) {
		return b == 0 ? PoseSpec{ AxisAngle( 0, 0, 1, 90 ), { 10.0f, 0, 0 } }
			: PoseSpec{ AxisAngle( 1, 0, 0, 30 ), { 2.0f, 0, 0 } };
	};
	auto torso = []( int, int b ) {
		return b == 0 ? PoseSpec{ IdentityQuat, { 0, 0, 0 } } : PoseSpec{ AxisAngle( 0, 1, 0, 45 ), { 0, 0, 3.0f } };
	};
	const Bytes legsFile = AnimFile( 2, 1, 3, legs );
	const Bytes torsoFile = AnimFile( 2, 1, 3, torso );
	SkeletalAnim legsAnim, torsoAnim;
	CHECK( OpenSkeletalAnim( legsFile.data(), legsFile.size(), legsAnim ) );
	CHECK( OpenSkeletalAnim( torsoFile.data(), torsoFile.size(), torsoAnim ) );

	Sample legSample, torsoSample;
	legSample.anim = &legsAnim;
	torsoSample.anim = &torsoAnim;

	/* With a torso channel only the flag-1 bone keeps the legs' pose. */
	LocalPose local[3];
	PoseLocal( skel, legSample, &torsoSample, local );
	CHECK( NearQuat( local[0].q, IdentityQuat ) );
	CHECK( NearQuat( local[1].q, AxisAngle( 1, 0, 0, 30 ) ) );
	CHECK( NearQuat( local[2].q, AxisAngle( 0, 1, 0, 45 ) ) );
	/* Without one the legs channel drives every bone. */
	PoseLocal( skel, legSample, nullptr, local );
	CHECK( NearQuat( local[0].q, AxisAngle( 0, 0, 1, 90 ) ) && NearQuat( local[2].q, AxisAngle( 1, 0, 0, 30 ) ) );

	/* Q = L (x) Q_parent, and origin = the parent's origin plus the offset
	 * through the parent's matrix. The root's quarter turn about z sends the
	 * child's local x offset of 2 to model -y. */
	BoneTransform world[3];
	PoseWorld( skel, local, nullptr, 0, world );
	CHECK( Near( world[0].origin[0], 10.0f ) );
	CHECK( NearQuat( world[1].q, Mul( AxisAngle( 1, 0, 0, 30 ), AxisAngle( 0, 0, 1, 90 ) ) ) );
	CHECK( Near( world[1].origin[0], 10.0f ) && Near( world[1].origin[1], -2.0f ) && Near( world[1].origin[2], 0.0f ) );
	/* Composing parent-first is a different rotation. */
	CHECK( !NearQuat( world[1].q, Mul( AxisAngle( 0, 0, 1, 90 ), AxisAngle( 1, 0, 0, 30 ) ) ) );

	/* Skinning: a weight adds the bone's matrix applied to the bone-local
	 * offset plus the bone's origin, times the weight. */
	float accum[3] = { 0, 0, 0 };
	const float offset[3] = { 1.0f, 0, 0 };
	AddWeight( world[0], 0.5f, offset, accum );
	CHECK( Near( accum[0], 5.0f ) && Near( accum[1], -0.5f ) && Near( accum[2], 0.0f ) );

	/* A controller post-multiplies its bone and its scale runs down the
	 * chain; with two naming one bone the last wins. */
	Controller controllers[2] = {
		{ 0, AxisAngle( 0, 0, 1, 45 ), 3.0f },
		{ 0, AxisAngle( 0, 0, 1, 90 ), 2.0f },
	};
	PoseWorld( skel, local, controllers, 2, world );
	CHECK( NearQuat( world[0].q, Mul( AxisAngle( 0, 0, 1, 90 ), AxisAngle( 0, 0, 1, 90 ) ) ) );
	CHECK( Near( world[0].scale, 2.0f ) && Near( world[1].scale, 2.0f ) && Near( world[2].scale, 2.0f ) );
	CHECK( Near( world[0].axis[2][2], 2.0f ) );
	/* The scaled root matrix carries the child's offset twice as far. */
	CHECK( Near( world[1].origin[0], 6.0f ) && Near( world[1].origin[1], 0.0f, 1e-3f ) );
	const Controller none{ -1, IdentityQuat, 5.0f };
	PoseWorld( skel, local, &none, 1, world );
	CHECK( Near( world[2].scale, 1.0f ) );
}

static void CheckTanFrame()
{
	/* A TAN frame is sixty-eight bytes, the frame time last. */
	static_assert( TanFrameBytes == 68, "a TAN frame is sixty-eight bytes" );
	std::uint8_t frame[68];
	for ( int k = 0; k < 17; ++k ) {
		const float v = float( k );
		std::memcpy( frame + 4 * k, &v, 4 );
	}
	const TanFrame f = ReadTanFrame( frame );
	CHECK( f.mins[0] == 0 && f.maxs[0] == 3 && f.scale[0] == 6 && f.offset[0] == 9 );
	CHECK( f.delta[2] == 14 && f.radius == 15 && f.frameTime == 16 );
}

int main()
{
	CheckQuaternionRules();
	CheckSkeletonRefusals();
	CheckAnimations();
	CheckBlending();
	CheckHierarchy();
	CheckTanFrame();
	std::cout << "alice tiki pose tests passed\n";
	return 0;
}
