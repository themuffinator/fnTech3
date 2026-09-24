/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_TIKI_POSE_HPP
#define FNTECH3_COMPAT_ALICE_TIKI_POSE_HPP

/* How American McGee's Alice poses its skeletal and vertex-animated models.
 *
 * Header-only and free of engine state, because three places need exactly the
 * same answers: the game module's tag queries on the server, the client
 * game's, and the renderer's skinning. Retail alice.exe computes all three
 * with the same few routines, and a disagreement between them shows up as a
 * weapon held a hand's width from the hand that holds it.
 *
 * Every rule here was read out of retail alice.exe and then checked against
 * the retail files: posing the first and last frame of every animation on all
 * 66 skeletons reproduces the bounds each animation file stores for that
 * frame, to a median of 0.067 units for the compressed version 3 poses and
 * 0.0001 for the float version 2 ones, while the two plausible misreadings
 * -- composing parent-first, or leaving the matrix untransposed -- miss by
 * 15 and 31 units. See docs/fntech3/2026-09-23-alice-tiki.md.
 *
 *   A pose        version 3 stores a quaternion as four shorts over 32,767
 *                 and an offset as three shorts over 63 (0x0045f5bb,
 *                 0x004a3a10); version 2 stores both as floats.
 *   A matrix      is built from a quaternion with its rows as the axes
 *                 (0x0044a5b0), and a bone-local vector reaches model space
 *                 as v0*row0 + v1*row1 + v2*row2 (0x0044a130).
 *   A hierarchy   runs in bone order, parents first: Q = L (x) Q_parent, the
 *                 Hamilton product on (x, y, z, w), and origin = the parent's
 *                 origin plus the offset through the parent's matrix.
 *   A controller  multiplies a bone by its quaternion on the right, after the
 *                 bone has its parent's rotation, and its scale multiplies
 *                 down the chain into every matrix below it.
 *   Legs & torso  a bone flagged 1 in the skeleton is a leg; when both
 *                 animations are in use the legs one drives the leg bones and
 *                 the torso one everything else.
 *   Blending      frames lerp by the back-lerp with a slerp; a cross-blend
 *                 then slerps from the previous animation's pose toward that
 *                 result by its own weight, and a negative weight means none.
 *   Skinning      a vertex is the weighted sum of each bone's matrix applied
 *                 to its bone-local offset plus the bone's origin (0x0049ffe0).
 *
 * Nothing here allocates or reads a file; callers hand in views over bytes
 * they have already bounded. */

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3 {
namespace alice {
namespace pose {

struct Quat {
	float x, y, z, w;
};

inline constexpr Quat IdentityQuat{ 0.0f, 0.0f, 0.0f, 1.0f };

/* The Hamilton product a (x) b, on quaternions stored x, y, z, w. */
inline Quat Mul( const Quat &a, const Quat &b ) {
	return Quat{
		a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
		a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
		a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
		a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
	};
}

/* Rows are the axes. `scale` multiplies the whole matrix, which is how the
 * renderer carries a controller's scale into a bone (0x004a2020). */
inline void ToMatrix( const Quat &q, float scale, float m[3][3] ) {
	const float t = 2.0f * scale;
	const float xx = t * q.x * q.x, yy = t * q.y * q.y, zz = t * q.z * q.z;
	const float xy = t * q.x * q.y, xz = t * q.x * q.z, yz = t * q.y * q.z;
	const float wx = t * q.w * q.x, wy = t * q.w * q.y, wz = t * q.w * q.z;
	m[0][0] = scale - ( yy + zz ); m[0][1] = xy - wz;             m[0][2] = xz + wy;
	m[1][0] = xy + wz;             m[1][1] = scale - ( xx + zz ); m[1][2] = yz - wx;
	m[2][0] = xz - wy;             m[2][1] = yz + wx;             m[2][2] = scale - ( xx + yy );
}

/* A bone-local vector in the bone's parent frame: v0*row0 + v1*row1 + v2*row2. */
inline void RotateT( const float v[3], const float m[3][3], float out[3] ) {
	for ( int k = 0; k < 3; ++k ) {
		out[k] = v[0] * m[0][k] + v[1] * m[1][k] + v[2] * m[2][k];
	}
}

/* Retail's slerp (0x0044a690): along the shorter arc, falling back to a plain
 * lerp when the two are nearly equal, and not renormalised afterwards. */
inline Quat Slerp( const Quat &from, const Quat &to, float t ) {
	float cosom = from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w;
	Quat target = to;
	if ( cosom < 0.0f ) {
		cosom = -cosom;
		target = Quat{ -to.x, -to.y, -to.z, -to.w };
	}
	float scale0, scale1;
	if ( 1.0f - cosom > 1e-6f ) {
		const float omega = std::acos( cosom );
		const float sinom = std::sin( omega );
		scale0 = std::sin( ( 1.0f - t ) * omega ) / sinom;
		scale1 = std::sin( t * omega ) / sinom;
	} else {
		scale0 = 1.0f - t;
		scale1 = t;
	}
	return Quat{
		scale0 * from.x + scale1 * target.x,
		scale0 * from.y + scale1 * target.y,
		scale0 * from.z + scale1 * target.z,
		scale0 * from.w + scale1 * target.w
	};
}

namespace detail {

inline std::int32_t Int( const std::uint8_t *p ) {
	std::int32_t v;
	std::memcpy( &v, p, sizeof( v ) );
	return v;
}

inline float Float( const std::uint8_t *p ) {
	float v;
	std::memcpy( &v, p, sizeof( v ) );
	return v;
}

inline std::int16_t Short( const std::uint8_t *p ) {
	std::int16_t v;
	std::memcpy( &v, p, sizeof( v ) );
	return v;
}

} // namespace detail

/* ---- skeletons ---------------------------------------------------------- */

/* A view over an `SKL ` 2 or 3 skeleton. The directory is at 72: the surface
 * count, the bone count, and the offsets of the bones and of the first
 * surface. A bone is 72 bytes -- parent, flags, a 64-byte name. */
struct Skeleton {
	const std::uint8_t *data = nullptr;
	std::size_t bytes = 0;
	int version = 0;
	int surfaces = 0;
	int bones = 0;
	std::size_t ofsBones = 0;
	std::size_t ofsSurfaces = 0;
};

inline constexpr std::uint32_t SkeletonIdent = 'S' | ( 'K' << 8 ) | ( 'L' << 16 ) | ( ' ' << 24 );
inline constexpr std::size_t BoneBytes = 72;
/* Retail refuses a skeleton with more bones than this (0x00459302). */
inline constexpr int MaxBones = 200;

inline bool OpenSkeleton( const void *file, std::size_t bytes, Skeleton &out ) {
	out = Skeleton{};
	const auto *data = static_cast<const std::uint8_t *>( file );
	if ( !data || bytes < 92 ) return false;
	if ( static_cast<std::uint32_t>( detail::Int( data ) ) != SkeletonIdent ) return false;
	out.version = detail::Int( data + 4 );
	if ( out.version != 2 && out.version != 3 ) return false;
	out.surfaces = detail::Int( data + 72 );
	out.bones = detail::Int( data + 76 );
	const std::int32_t ofsBones = detail::Int( data + 80 );
	const std::int32_t ofsSurfaces = detail::Int( data + 84 );
	if ( out.bones <= 0 || out.bones > MaxBones || out.surfaces < 0 || out.surfaces > 256 ) return false;
	if ( ofsBones < 0 || static_cast<std::size_t>( ofsBones ) > bytes
		|| BoneBytes * static_cast<std::size_t>( out.bones ) > bytes - static_cast<std::size_t>( ofsBones ) ) {
		return false;
	}
	if ( ofsSurfaces < 0 || static_cast<std::size_t>( ofsSurfaces ) > bytes ) return false;
	out.data = data;
	out.bytes = bytes;
	out.ofsBones = static_cast<std::size_t>( ofsBones );
	out.ofsSurfaces = static_cast<std::size_t>( ofsSurfaces );
	/* Parents come first, so a hierarchy pass in bone order sees every
	 * parent before its children; a file that breaks that is refused. */
	for ( int b = 0; b < out.bones; ++b ) {
		const std::int32_t parent = detail::Int( data + out.ofsBones + BoneBytes * b );
		if ( parent >= b || parent < -1 ) {
			out = Skeleton{};
			return false;
		}
		if ( !std::memchr( data + out.ofsBones + BoneBytes * b + 8, 0, 64 ) ) {
			out = Skeleton{};
			return false;
		}
	}
	return true;
}

inline int BoneParent( const Skeleton &s, int bone ) {
	return detail::Int( s.data + s.ofsBones + BoneBytes * bone );
}

inline int BoneFlags( const Skeleton &s, int bone ) {
	return detail::Int( s.data + s.ofsBones + BoneBytes * bone + 4 );
}

inline const char *BoneName( const Skeleton &s, int bone ) {
	return reinterpret_cast<const char *>( s.data + s.ofsBones + BoneBytes * bone + 8 );
}

/* ---- skeletal animations ----------------------------------------------- */

/* A view over an `SKAN` 2 or 3 animation. The two versions share their
 * layout apart from where the header's counts sit and how wide a pose is. A
 * frame is a forty-byte header -- bounds, radius, delta -- and then one pose a
 * bone. */
struct SkeletalAnim {
	const std::uint8_t *data = nullptr;
	std::size_t bytes = 0;
	int version = 0;
	int frames = 0;
	int bones = 0;
	float totalTime = 0.0f;
	float frameTime = 0.0f;
	float delta[3] = { 0.0f, 0.0f, 0.0f };
	std::size_t ofsFrames = 0;
	std::size_t stride = 0;
};

inline constexpr std::uint32_t SkeletalAnimIdent = 'S' | ( 'K' << 8 ) | ( 'A' << 16 ) | ( 'N' << 24 );
inline constexpr std::size_t FrameHeaderBytes = 40;

inline bool OpenSkeletalAnim( const void *file, std::size_t bytes, SkeletalAnim &out ) {
	out = SkeletalAnim{};
	const auto *data = static_cast<const std::uint8_t *>( file );
	if ( !data || bytes < 108 ) return false;
	if ( static_cast<std::uint32_t>( detail::Int( data ) ) != SkeletalAnimIdent ) return false;
	out.version = detail::Int( data + 4 );
	std::size_t counts;
	if ( out.version == 3 ) {
		counts = 76;
	} else if ( out.version == 2 ) {
		counts = 72;
	} else {
		return false;
	}
	out.frames = detail::Int( data + counts );
	out.bones = detail::Int( data + counts + 4 );
	out.totalTime = detail::Float( data + counts + 8 );
	out.frameTime = detail::Float( data + counts + 12 );
	for ( int k = 0; k < 3; ++k ) out.delta[k] = detail::Float( data + counts + 16 + 4 * k );
	const std::int32_t ofsFrames = detail::Int( data + 104 );
	out.stride = FrameHeaderBytes + ( out.version == 3 ? 16u : 28u ) * static_cast<std::size_t>( out.bones > 0 ? out.bones : 0 );
	if ( out.frames <= 0 || out.bones <= 0 || out.bones > 1024 || ofsFrames < 0
		|| static_cast<std::size_t>( ofsFrames ) > bytes
		|| static_cast<std::size_t>( out.frames ) > ( bytes - static_cast<std::size_t>( ofsFrames ) ) / out.stride ) {
		out = SkeletalAnim{};
		return false;
	}
	out.data = data;
	out.bytes = bytes;
	out.ofsFrames = static_cast<std::size_t>( ofsFrames );
	return true;
}

inline const std::uint8_t *FrameAt( const SkeletalAnim &a, int frame ) {
	if ( frame < 0 ) frame = 0;
	if ( frame >= a.frames ) frame = a.frames - 1;
	return a.data + a.ofsFrames + a.stride * static_cast<std::size_t>( frame );
}

/* A frame's stored bounds, radius and movement. */
inline void FrameBounds( const SkeletalAnim &a, int frame, float mins[3], float maxs[3] ) {
	const std::uint8_t *f = FrameAt( a, frame );
	for ( int k = 0; k < 3; ++k ) {
		mins[k] = detail::Float( f + 4 * k );
		maxs[k] = detail::Float( f + 12 + 4 * k );
	}
}

inline float FrameRadius( const SkeletalAnim &a, int frame ) {
	return detail::Float( FrameAt( a, frame ) + 24 );
}

inline void FrameDelta( const SkeletalAnim &a, int frame, float delta[3] ) {
	const std::uint8_t *f = FrameAt( a, frame );
	for ( int k = 0; k < 3; ++k ) delta[k] = detail::Float( f + 28 + 4 * k );
}

struct LocalPose {
	Quat q;
	float t[3];
};

inline LocalPose DecodePose( const SkeletalAnim &a, int frame, int bone ) {
	LocalPose p{ IdentityQuat, { 0.0f, 0.0f, 0.0f } };
	if ( bone < 0 || bone >= a.bones ) return p;
	const std::uint8_t *f = FrameAt( a, frame ) + FrameHeaderBytes;
	if ( a.version == 3 ) {
		const std::uint8_t *b = f + 16 * static_cast<std::size_t>( bone );
		constexpr float qs = 1.0f / 32767.0f, ts = 1.0f / 63.0f;
		p.q = Quat{ detail::Short( b ) * qs, detail::Short( b + 2 ) * qs,
			detail::Short( b + 4 ) * qs, detail::Short( b + 6 ) * qs };
		p.t[0] = detail::Short( b + 8 ) * ts;
		p.t[1] = detail::Short( b + 10 ) * ts;
		p.t[2] = detail::Short( b + 12 ) * ts;
	} else {
		const std::uint8_t *b = f + 28 * static_cast<std::size_t>( bone );
		p.q = Quat{ detail::Float( b ), detail::Float( b + 4 ), detail::Float( b + 8 ), detail::Float( b + 12 ) };
		p.t[0] = detail::Float( b + 16 );
		p.t[1] = detail::Float( b + 20 );
		p.t[2] = detail::Float( b + 24 );
	}
	return p;
}

/* ---- sampling ----------------------------------------------------------- */

/* What one animation channel contributes: the current frame, the frame it
 * lerps from -- which may belong to a different animation -- and optionally
 * the pose of the animation it is cross-blending away from. */
struct Sample {
	const SkeletalAnim *anim = nullptr;
	int frame = 0;
	const SkeletalAnim *oldAnim = nullptr;
	int oldFrame = 0;
	float backlerp = 0.0f;
	const SkeletalAnim *crossAnim = nullptr;
	int crossFrame = 0;
	/* The weight of the new animation against the cross-blend source; a
	 * negative weight means there is no cross-blend. */
	float crossWeight = -1.0f;
};

inline LocalPose SampleBone( const Sample &s, int bone ) {
	LocalPose cur = s.anim ? DecodePose( *s.anim, s.frame, bone )
		: LocalPose{ IdentityQuat, { 0.0f, 0.0f, 0.0f } };
	LocalPose out = cur;
	if ( s.oldAnim && s.backlerp != 0.0f ) {
		const LocalPose old = DecodePose( *s.oldAnim, s.oldFrame, bone );
		out.q = Slerp( old.q, cur.q, 1.0f - s.backlerp );
		for ( int k = 0; k < 3; ++k ) {
			out.t[k] = old.t[k] * s.backlerp + cur.t[k] * ( 1.0f - s.backlerp );
		}
	}
	if ( s.crossAnim && s.crossWeight >= 0.0f ) {
		const LocalPose cross = DecodePose( *s.crossAnim, s.crossFrame, bone );
		const float w = s.crossWeight;
		out.q = Slerp( cross.q, out.q, w );
		for ( int k = 0; k < 3; ++k ) {
			out.t[k] = cross.t[k] * ( 1.0f - w ) + out.t[k] * w;
		}
	}
	return out;
}

/* Every bone's local pose. With a torso channel, only the leg bones -- those
 * the skeleton flags 1 -- keep the legs channel's pose. */
inline void PoseLocal( const Skeleton &skeleton, const Sample &legs, const Sample *torso,
	LocalPose *out ) {
	for ( int b = 0; b < skeleton.bones; ++b ) {
		const bool leg = BoneFlags( skeleton, b ) == 1;
		out[b] = ( torso && !leg ) ? SampleBone( *torso, b ) : SampleBone( legs, b );
	}
}

/* ---- the hierarchy ------------------------------------------------------ */

struct Controller {
	int bone;       /* -1 for none */
	Quat q;
	float scale;
};

struct BoneTransform {
	Quat q;
	float origin[3];
	float axis[3][3];  /* rows are the axes, carrying the chain's scale */
	float scale;
};

/* Model-space bones from local poses. `controllers` may be null. Retail's
 * renderer keeps one controller a bone -- the last one naming it wins -- and
 * that is what this does; its tag routine applies every controller that
 * names a bone, which differs only when two name the same bone, which no
 * retail module does. */
inline void PoseWorld( const Skeleton &skeleton, const LocalPose *local,
	const Controller *controllers, int numControllers, BoneTransform *out ) {
	for ( int b = 0; b < skeleton.bones; ++b ) {
		const Controller *control = nullptr;
		for ( int c = 0; c < numControllers; ++c ) {
			if ( controllers && controllers[c].bone == b ) {
				control = &controllers[c];
			}
		}
		const int parent = BoneParent( skeleton, b );
		BoneTransform &t = out[b];
		if ( parent < 0 ) {
			t.q = local[b].q;
			t.scale = 1.0f;
			for ( int k = 0; k < 3; ++k ) t.origin[k] = local[b].t[k];
		} else {
			const BoneTransform &p = out[parent];
			t.q = Mul( local[b].q, p.q );
			t.scale = p.scale;
			float moved[3];
			RotateT( local[b].t, p.axis, moved );
			for ( int k = 0; k < 3; ++k ) t.origin[k] = p.origin[k] + moved[k];
		}
		if ( control ) {
			t.q = Mul( t.q, control->q );
			t.scale *= control->scale;
		}
		ToMatrix( t.q, t.scale, t.axis );
	}
}

/* ---- skinning ----------------------------------------------------------- */

/* One weight's contribution: the bone's matrix applied to the bone-local
 * offset, plus the bone's origin, times the weight. */
inline void AddWeight( const BoneTransform &bone, float weight, const float offset[3], float accum[3] ) {
	float moved[3];
	RotateT( offset, bone.axis, moved );
	for ( int k = 0; k < 3; ++k ) accum[k] += ( moved[k] + bone.origin[k] ) * weight;
}

/* ---- vertex animation --------------------------------------------------- */

/* A `TAN ` frame is sixty-eight bytes: bounds, a scale, an offset, a delta,
 * a radius and a frame time. A vertex is three unsigned shorts against the
 * frame's scale and offset, then a packed normal. */
inline constexpr std::size_t TanFrameBytes = 68;

struct TanFrame {
	float mins[3], maxs[3];
	float scale[3];
	float offset[3];
	float delta[3];
	float radius;
	float frameTime;
};

inline TanFrame ReadTanFrame( const std::uint8_t *f ) {
	TanFrame out;
	for ( int k = 0; k < 3; ++k ) {
		out.mins[k] = detail::Float( f + 4 * k );
		out.maxs[k] = detail::Float( f + 12 + 4 * k );
		out.scale[k] = detail::Float( f + 24 + 4 * k );
		out.offset[k] = detail::Float( f + 36 + 4 * k );
		out.delta[k] = detail::Float( f + 48 + 4 * k );
	}
	out.radius = detail::Float( f + 60 );
	out.frameTime = detail::Float( f + 64 );
	return out;
}

} // namespace pose
} // namespace alice
} // namespace fntech3

#endif
