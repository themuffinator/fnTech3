/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_GHOUL2_SERVER_HPP
#define FNTECH3_GHOUL2_SERVER_HPP

/* Server-side Ghoul2: the models a Raven game asks the engine to hold for it.
 *
 * This is not a renderer concern. A Jedi Academy or Jedi Outcast server needs
 * Ghoul2 before it draws anything, because the module makes a model for an
 * entity while the level is still spawning and then asks the engine where that
 * model's bones and bolts are. Both titles stop here.
 *
 * What this holds is the model, not the pose: a `.glm` mesh, the `.gla`
 * skeleton it names, and the bone count each has. Every file is read through
 * the same bounded readers that swept both installations, so a model that
 * would not read there does not load here either.
 *
 * Multiplayer modules keep an opaque void* handle in their entity records.
 * The engine owns the instance separately: no module handle is dereferenced,
 * and a retired identity never becomes a new instance, even if its old heap
 * address is reused. The single-player STL container contract is separate.
 *
 * See docs/fntech3/2026-09-22-raven-ghoul2-server.md. */

#include "../compat/jka_model_format.hpp"
#include "ghoul2_pose.hpp"
#include "ghoul2_default.hpp"

#include <cmath>
#include <algorithm>
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fntech3 {
namespace ghoul2 {

namespace model = fntech3::jka::model;

/* What the engine must be able to do to read a model. A test supplies its own
 * files; the server reads the installation's. */
class Files {
public:
	virtual ~Files() = default;
	/* Read a whole file by its path. Return its size, or 0 when there is none.
	 * A size larger than `capacity` means nothing was written. */
	virtual std::size_t Read( const char *path, void *destination,
		std::size_t capacity ) = 0;
};

enum class LoadError {
	None,
	NoName,        /* an empty or unusable model name */
	Missing,       /* the mesh is not in the filesystem */
	Mesh,          /* the mesh does not read */
	SkeletonName,  /* the mesh names no skeleton */
	SkeletonFile,  /* the skeleton it names is not there */
	Skeleton,      /* the skeleton does not read */
	Bones,         /* the mesh wants a bone the skeleton does not have */
	Full,          /* this instance holds as many models as it may */
	HandleExhausted, /* no never-used opaque identity remains */
};

inline const char *LoadErrorName( LoadError error ) {
	switch ( error ) {
	case LoadError::None: return "none";
	case LoadError::NoName: return "no model name";
	case LoadError::Missing: return "no such model";
	case LoadError::Mesh: return "the mesh does not read";
	case LoadError::SkeletonName: return "the mesh names no skeleton";
	case LoadError::SkeletonFile: return "the skeleton it names is not there";
	case LoadError::Skeleton: return "the skeleton does not read";
	case LoadError::Bones: return "the mesh wants a bone its skeleton lacks";
	case LoadError::Full: return "the instance holds as many models as it may";
	case LoadError::HandleExhausted: return "opaque model identities exhausted";
	}
	return "unknown";
}

/* Raven's own bound on the models one instance carries. */
inline constexpr int MaxModelsPerInstance = 32;
/* The largest file either reader will be handed. A retail installation's
 * biggest mesh is under 3 MB and its biggest skeleton under 9. */
inline constexpr std::size_t MaxModelBytes = 32u * 1024u * 1024u;
/* This engine's own bound on the entity numbers an attachment may name. */
inline constexpr int MaxAttachedEntities = 1024;
/* And on how many names one model may be asked about, and how many bolts it
 * may carry. A retail humanoid has some seventy bones and forty surfaces. */
inline constexpr int MaxNamedBones = 256;
inline constexpr int MaxNamedSurfaces = 256;
inline constexpr int MaxBoltsPerModel = 64;
/* One animation per bone, and no more bones than this engine names. */
inline constexpr int MaxBoneAnimations = MaxNamedBones;
/* And on how many distinct model files this engine will keep hold of, so that
 * a level full of models cannot make a server keep every one of them. Both
 * games share a handful of skeletons across all their characters, so this is
 * generous rather than tight. */
inline constexpr std::size_t MaxKeptFiles = 64;

/* How one model of an instance names the bolt it hangs from: its link.
 *
 * Measured in retail jamp.exe 1.01. The word lives at +0x3c of each 0xc8-byte
 * model record. InitGhoul2Model stores -1 there (0x4de93e), meaning a model of
 * its own; SetBoltInfo stores whatever word the module passes (0x4df96f), and
 * CopySpecificGhoul2Model's record assignment carries it to the copy
 * (0x4dea73). The model sort (0x498be0), the skeleton build (0x49a050) and
 * the renderer (0x499bf0) all read any other value as the parent model
 * `(link >> 10) & 0x3ff` and that model's bolt `link & 0x3ff`. The pinned
 * OpenJK G2.h names the same widths (MODEL_SHIFT 10, BOLT_SHIFT 0, 10 bits
 * each). Only the masked bits are read, so the shift's sign does not matter. */
namespace link {
inline constexpr int None = -1;
inline int ParentModel( int value ) { return int( ( std::uint32_t( value ) >> 10 ) & 0x3ffu ); }
inline int ParentBolt( int value ) { return int( std::uint32_t( value ) & 0x3ffu ); }
} // namespace link

/* MP Ghoul2 handles are compared and passed back, never followed by the game.
 * Use the pointer-sized ABI slot as an identity rather than a heap address.
 * All Stores share one sequence, so a handle from another Store cannot resolve
 * here. Allocation is atomic across Stores; each Store's model operations are
 * serialized by its owning server frame. Zero permanently marks exhaustion,
 * rather than wrapping to an identity a stale module record could still hold. */
class OpaqueHandleSequence {
public:
	explicit OpaqueHandleSequence( std::uintptr_t first = 1 ) : next_( first ) {}
	void *Allocate() {
		std::uintptr_t candidate = next_.load( std::memory_order_relaxed );
		while ( candidate ) {
			const std::uintptr_t next = candidate == std::numeric_limits<std::uintptr_t>::max()
				? 0 : candidate + 1;
			if ( next_.compare_exchange_weak( candidate, next, std::memory_order_relaxed ) ) {
				return reinterpret_cast<void *>( candidate );
			}
		}
		return nullptr;
	}
private:
	std::atomic<std::uintptr_t> next_;
};
inline OpaqueHandleSequence ModelHandleIdentities;

/* The module's own field for an instance: pointer-sized in a native module,
 * a 32-bit word in a bytecode module's data segment (`fieldBytes` 4), whose
 * handles this store keeps below 2^32. The field may be unaligned. */
inline void *ReadField( const void *field, std::size_t fieldBytes )
{
	if ( fieldBytes == 4 ) {
		std::uint32_t word;
		std::memcpy( &word, field, sizeof( word ) );
		return reinterpret_cast<void *>( std::uintptr_t( word ) );
	}
	void *handle;
	std::memcpy( &handle, field, sizeof( handle ) );
	return handle;
}
inline bool FieldHolds( const void *handle, std::size_t fieldBytes )
{
	return fieldBytes != 4 || !( std::uint64_t( reinterpret_cast<std::uintptr_t>( handle ) ) >> 32 );
}
inline void WriteField( void *field, std::size_t fieldBytes, const void *handle )
{
	if ( fieldBytes == 4 ) {
		const auto word = std::uint32_t( reinterpret_cast<std::uintptr_t>( handle ) );
		std::memcpy( field, &word, sizeof( word ) );
		return;
	}
	std::memcpy( field, &handle, sizeof( handle ) );
}

/* A named place on a model that the game can ask the engine about later: a
 * surface of the mesh, or a bone of the skeleton. Which of the two it is
 * matters, because they are numbered separately. */
struct Bolt {
	std::string name;
	int surface = -1;
	int bone = -1;
	/* What the last build of this bolt's model left for it, held only by a
	 * store that keeps builds (see Store): jk2mp.exe 1.02a keeps it at +0x10
	 * of the bolt's own 64-byte record. False until a build reaches it, where
	 * retail appends the record with whatever its stack held there (its
	 * constructor 0x4aa8b0 sets only the first four words). */
	bool kept = false;
	pose::Matrix matrix{};
};

/* What the game set one bone playing: its frame range, speed, blend and clock.
 * The timing model below turns this into a pose frame when a bolt is asked for. */
struct BoneAnimation {
	int bone = -1;
	int startFrame = 0;
	int endFrame = 0;
	int flags = 0;
	float speed = 0.0f;
	int startTime = 0;
	float setFrame = -1.0f;
	int blendTime = 0;
	/* Owned transition sample, separate from the new clip's seeded clock.
	 * No external bone record or module pointer enters this representation. */
	pose::Frame previous{};
	int blendStart = 0;
	bool previousKnown = false;
	/* Jedi Outcast's own transition sample, as its bone record keeps it
	 * (jk2mp.exe 1.02a +0x4c and +0x50): the frame the prior clip had
	 * reached, and the whole frame to blend it with (see outcast::Control). */
	float blendFrame = 0.0f;
	int blendLerpFrame = 0;
};

/* Where a bone's animation has got to at a given moment.
 *
 * Measured out of retail jamp.exe's own timing routine, which begins at RVA
 * 0x97290 and is reached from the bolt matrix by way of the skeleton rebuild.
 * It reads the bone record it is handed at the offsets this engine keeps the
 * same fields at -- start frame, end frame, start time, pause time, speed --
 * and does this:
 *
 *     elapsed = (pauseTime ? pauseTime : now) - startTime
 *     if (elapsed < 0) elapsed = 0
 *     frame   = startFrame + (elapsed / 50) * speed
 *
 * Fifty is the engine's own divisor, a float at 0x566098, so a Ghoul2 frame
 * is fifty milliseconds and an animation speed of one runs at twenty frames a
 * second. The engine then splits that into three answers, which it writes to
 * three separate places: the whole part is the current frame, the fraction is
 * a blend weight, and the frame after it is the one to blend towards. Where
 * the next frame would run past the end, a set loop flag -- bit 0x10 of the
 * flags the game passed -- wraps it and a clear one holds it at the last
 * frame.
 *
 * Nothing here pauses: Jedi Academy's module never asks the engine to, and
 * there is no import in either title's table that would let it, so the pause
 * time is always nothing and is kept only because the engine's arithmetic
 * reads it. */
inline constexpr float FrameMilliseconds = 50.0f;
inline constexpr int LoopFlag = 0x10;

inline bool FiniteScalar( float value )
{
	std::uint32_t bits;
	std::memcpy( &bits, &value, sizeof( bits ) );
	/* A scalar parameter may carry compiler finite-only assumptions under
	 * fast math; retain an integer memory read for this contract check. */
	volatile std::uint32_t encoded = bits;
	return ( encoded & 0x7f800000u ) != 0x7f800000u;
}

struct Playhead {
	int currentFrame = 0;
	int newFrame = 0;
	float lerp = 0.0f;
};

inline Playhead PlayheadAt( const BoneAnimation &playing, int numFrames, int now )
{
	Playhead out;
	if ( numFrames <= 0 || !FiniteScalar( playing.speed ) ) return out;

	float elapsed = float( std::int64_t( now ) - playing.startTime ) / FrameMilliseconds;
	if ( elapsed < 0.0f ) elapsed = 0.0f;
	float frame = float( playing.startFrame ) + elapsed * playing.speed;

	/* The range the game gave, kept the right way round and inside the file.
	 * A range this engine cannot use is held at the first frame rather than
	 * indexed with, because an out-of-range frame is the game's mistake and
	 * not something to pass on to a reader. */
	int first = playing.startFrame;
	int last = playing.endFrame;
	if ( first < 0 ) first = 0;
	if ( first >= numFrames ) first = numFrames - 1;
	if ( last > numFrames ) last = numFrames;
	if ( last <= first ) { out.currentFrame = out.newFrame = first; return out; }
	if ( !FiniteScalar( frame ) ) { out.currentFrame = out.newFrame = first; return out; }

	const float span = float( last - first );
	if ( playing.flags & LoopFlag ) {
		frame = frame - span * std::floor( ( frame - float( first ) ) / span );
	} else if ( frame > float( last - 1 ) ) {
		frame = float( last - 1 );
	}
	if ( frame < float( first ) ) frame = float( first );

	out.currentFrame = int( frame );
	out.lerp = frame - float( out.currentFrame );
	out.newFrame = out.currentFrame + 1;
	if ( out.newFrame >= last ) {
		out.newFrame = ( playing.flags & LoopFlag ) ? first : last - 1;
	}
	if ( out.currentFrame < first ) out.currentFrame = first;
	if ( out.newFrame < first ) out.newFrame = first;
	return out;
}

/* The currently served forward timing subset, shared by palette evaluation
 * and the transition setter. Unfrozen terminal state is still unmeasured. */
inline bool ForwardPlayhead( const BoneAnimation &playing, int numFrames, int now,
	Playhead &out )
{
	if ( playing.flags & ~0xd8 || !(playing.flags & 0x18)
		|| !FiniteScalar(playing.speed) || playing.speed < 0.0f
		|| playing.startFrame < 0 || playing.endFrame > numFrames
		|| playing.endFrame <= playing.startFrame ) return false;
	const double elapsed = std::max(0.0, double(std::int64_t(now) - playing.startTime));
	const double frame = playing.startFrame + elapsed / 50.0 * playing.speed;
	if ( !(playing.flags & 0x10) && !(playing.flags & 0x40)
		&& frame > playing.endFrame - 1 ) return false;
	out = PlayheadAt(playing, numFrames, now);
	return true;
}

/* Jedi Outcast's clip timing, read from jk2mp.exe 1.02a with Capstone: the
 * sample its clip setter takes of the clip it replaces (0x4ab420), and the
 * frames and transition its bone transform poses (0x476380..0x476952). It
 * is not Academy's (PlayheadAt). A clip's age is its milliseconds times the
 * float 0.02 (0x4eaa44), not over fifty; a loop wraps with fmod; a reverse
 * clip (negative speed) takes paths of its own; and a clip that ended
 * without loop or freeze leaves its bone on its parent's frames.
 *
 * The engine runs its x87 at the C runtime's 53-bit precision (startup
 * 0x4d4b51 sets _PC_53), so its arithmetic rounds as IEEE doubles do and its
 * stores round to float; this does the same, step for step. A host whose
 * doubles keep x87 extended intermediates (GCC i686 without SSE math) can
 * differ from it in a last bit. */
namespace outcast {
inline constexpr int ClipBits = 0x18;       /* 0x08 or 0x10: a clip runs */
inline constexpr int LoopFlag = 0x10;
inline constexpr int FreezeFlags = 0x48;    /* 0x476770 wants both */
inline constexpr int BlendFlag = 0x80;
inline constexpr int AnimationBits = 0xd8;  /* the setter's mask, 0x4ab358 */
/* The float at 0x4eaa44, written exactly. */
inline constexpr float FrameScale = 0.0199999995529651641845703125f;

inline bool FiniteDouble( double value )
{
	std::uint64_t bits;
	std::memcpy( &bits, &value, sizeof( bits ) );
	volatile std::uint64_t encoded = bits;
	return ( encoded & 0x7ff0000000000000ull ) != 0x7ff0000000000000ull;
}

/* _ftol (0x4d4f00): a truncation to 64 bits, of which callers keep the low
 * word. NaN, the infinities and anything past 64 bits store the integer
 * indefinite, whose low word is 0. */
inline int Truncate( double value )
{
	if ( !FiniteDouble( value )
		|| !( value >= -9223372036854775808.0 && value < 9223372036854775808.0 ) ) return 0;
	const auto whole = static_cast<std::uint64_t>( static_cast<std::int64_t>( value ) );
	return static_cast<int>( static_cast<std::uint32_t>( whole ) );
}

/* A clip's age in frames (0x4764c8..0x4764fe, 0x4ab47c..0x4ab4fd): the time
 * since its start as the engine's int subtraction wraps it, times 0.02,
 * stored as a float and never below 0. The pause time it would use instead
 * is set by no import either title serves, so it is always 0. */
inline float Age( int now, int startTime )
{
	const auto elapsed = static_cast<std::int32_t>( std::uint32_t( now ) - std::uint32_t( startTime ) );
	const float age = float( double( elapsed ) * double( FrameScale ) );
	return age < 0.0f ? 0.0f : age;
}

/* The frame a clip has reached (0x4ab420), which the setter samples for a
 * transition: false when the bone runs no clip. A clip past its end loops
 * by fmod or, otherwise, stands one frame inside its end (outside it, for
 * a reverse clip) whether or not it was frozen. */
inline bool Sample( const BoneAnimation &clip, int now, float &frame )
{
	if ( !( clip.flags & ClipBits ) ) return false;
	const double speed = clip.speed;
	const double end = float( clip.endFrame );
	float at = float( double( clip.startFrame ) + double( Age( now, clip.startTime ) ) * speed );
	const int size = Truncate( end - double( float( clip.startFrame ) ) );
	if ( !size ) { frame = float( clip.startFrame ); return true; }
	const bool past = speed > 0.0 ? end - 1.0 < double( at ) : speed < 0.0 && !( end + 1.0 <= double( at ) );
	if ( past && ( clip.flags & LoopFlag ) ) {
		const bool kept = speed >= 0.0 ? double( at ) < end : double( at ) > end + 1.0;
		if ( !kept ) at = float( std::fmod( double( at ) - end, double( size ) ) + end - double( size ) );
	} else if ( past ) {
		at = float( speed > 0.0 ? clip.endFrame - 1 : clip.endFrame + 1 );
	}
	frame = at;
	return true;
}

/* One bone's frames and transition in a build (0x4763d3..0x476952), as a
 * control for pose::Evaluate. `has` stays false for a bone that runs
 * neither a clip nor a transition: it keeps its parent's frames and
 * transition, as the transform keeps them. False refuses what retail would
 * pose and this does not serve: flags it would read as angle controls or
 * not read at all, a speed that is not finite, and a negative frame, for
 * which retail abandons the bone and every bone below it with its last
 * matrices. Frames past the file are refused by Evaluate; retail reads
 * past its frame table. */
inline bool Control( const BoneAnimation &clip, int now, pose::Animation &out, bool &has )
{
	has = false;
	if ( clip.flags & ~0xf8 ) return false;
	if ( !( clip.flags & ( ClipBits | BlendFlag ) ) ) return true;
	pose::Animation staged;
	staged.bone = clip.bone;
	if ( clip.flags & BlendFlag ) {
		/* 0x4763f7..0x476465: inside its time, the transition poses the
		 * sample it took -- the whole frame it had reached weighted by its
		 * fraction and the frame after weighted by the rest (0x476959) --
		 * and blends to the clip by how far in it is. Outside, none. */
		const float age = float( static_cast<std::int32_t>( std::uint32_t( now ) - std::uint32_t( clip.blendStart ) ) );
		if ( !( age < 0.0f ) && double( clip.blendTime ) > double( age ) ) {
			if ( !FiniteScalar( clip.blendFrame ) || clip.blendFrame < 0.0f || clip.blendLerpFrame < 0 ) return false;
			const int whole = Truncate( clip.blendFrame );
			staged.previous = { clip.blendLerpFrame, whole, float( double( clip.blendFrame ) - double( whole ) ) };
			staged.transition = float( double( age ) / double( clip.blendTime ) );
			staged.blending = true;
		}
	}
	if ( !( clip.flags & ClipBits ) ) {
		staged.inheritFrame = true;
		out = staged;
		has = true;
		return true;
	}
	if ( !FiniteScalar( clip.speed ) ) return false;
	const double speed = clip.speed;
	const double end = float( clip.endFrame );
	const float at = float( double( clip.startFrame ) + double( Age( now, clip.startTime ) ) * speed );
	const int size = Truncate( end - double( float( clip.startFrame ) ) );
	const bool loop = ( clip.flags & LoopFlag ) != 0;
	int current = 0, next = 0;
	float lerp = 0.0f;
	bool own = true;
	if ( !size ) {
		/* 0x4768d3: a clip of no length. */
		current = speed >= 0.0 ? clip.endFrame - 1 : clip.endFrame + 1;
		if ( current < 0 ) current = 0;
		next = current;
	} else if ( speed > 0.0 ? end - 1.0 < double( at ) : speed < 0.0 && !( end + 1.0 <= double( at ) ) ) {
		/* 0x47659c: past the end. */
		if ( loop && speed >= 0.0 ) {
			const float last = float( end - 1.0 );
			if ( double( at ) > double( last ) && double( at ) < end ) {
				current = Truncate( at );
				lerp = float( double( at ) - double( current ) );
				next = clip.startFrame;
			} else {
				double wrapped = at;
				if ( !( double( at ) < end ) ) wrapped = std::fmod( double( at ) - end, double( size ) ) + end - double( size );
				current = Truncate( wrapped );
				lerp = float( wrapped - double( current ) );
				next = wrapped < double( last ) ? current + 1 : clip.startFrame;
			}
		} else if ( loop ) {
			const float first = float( end + 1.0 );
			if ( !( double( at ) >= double( first ) ) && !( double( at ) < end ) ) {
				lerp = float( double( first ) - double( at ) );
				current = Truncate( end );
				next = clip.startFrame;
			} else {
				float wrapped = at;
				if ( !( double( at ) > double( first ) ) ) {
					wrapped = float( std::fmod( double( at ) - end, double( size ) ) + end - double( size ) );
				}
				const double up = std::ceil( double( wrapped ) );
				lerp = float( up - double( wrapped ) );
				current = Truncate( up );
				next = double( wrapped ) > double( first ) ? current - 1 : clip.startFrame;
			}
		} else if ( ( clip.flags & FreezeFlags ) == FreezeFlags ) {
			current = next = speed > 0.0 ? clip.endFrame - 1 : clip.endFrame + 1;
		} else {
			own = false;
		}
	} else if ( speed > 0.0 ) {
		/* 0x4767d0 */
		current = Truncate( at );
		next = current + 1;
		lerp = float( double( at ) - double( current ) );
		if ( next >= Truncate( end ) ) next = loop ? clip.startFrame : clip.endFrame;
	} else {
		/* 0x476830: still, or running backwards. */
		const double up = std::ceil( double( at ) );
		lerp = float( up - double( at ) );
		current = Truncate( up );
		if ( current > clip.startFrame ) {
			current = next = clip.startFrame;
			lerp = 0.0f;
		} else {
			next = current - 1;
			if ( !( end + 1.0 <= double( next ) ) ) next = loop ? clip.startFrame : clip.endFrame + 1;
		}
	}
	if ( own ) {
		/* 0x476912: a negative frame abandons the bone. A zero fraction reads
		 * the current frame alone (0x4769f7). */
		if ( current < 0 || next < 0 ) return false;
		if ( lerp == 0.0f ) next = current;
		staged.frame = { current, next, lerp };
	} else {
		staged.inheritFrame = true;
	}
	out = staged;
	has = true;
	return true;
}
} // namespace outcast

/* The angles the game bends one bone to, and which way round it means them.
 * The three orientations say which axis of the bone is up, right and forward;
 * the engine records the numbers and does not turn them into a matrix, which
 * is a renderer's business and not measured here. */
struct BoneAngles {
	int bone = -1;
	float angles[3] = { 0.0f, 0.0f, 0.0f };
	int flags = 0;
	int up = 0;
	int right = 0;
	int forward = 0;
	int blendTime = 0;
	int startTime = 0;
};

/* The flags word jamp.exe 1.01 keeps in each bone record its setters reach
 * (+0x34 of the 0x2e8-byte record), which Academy's import 597 hands back
 * whole. The clip setter (0x4e32f0) first ORs in 0x8000, a mark only the
 * collision cache clears, then replaces only the animation bits 0xd8 with
 * the flags it was given -- the blend bit 0x80 dropped where the record had
 * no clip running -- so any other bit it is given stays. The angle setter
 * (0x4e3240) replaces only the low three bits with its flags, whole. Both
 * leave a record carrying 0x2000 untouched. Kept for every store and read
 * only by Academy's server; see
 * docs/fntech3/2026-10-07-jka-server-bone-anim.md. */
struct BoneFlags {
	int bone = -1;
	std::uint32_t word = 0;
};
inline constexpr std::uint32_t BoneMarkFlag = 0x8000u;
inline constexpr std::uint32_t BoneLockFlag = 0x2000u;
inline constexpr std::uint32_t BoneAnimationBits = 0xd8u;
inline constexpr std::uint32_t BoneAngleBits = 0x7u;

/* What Academy's import 597 reads for one bone (jamp.exe 1.01 0x4e3750 and
 * 0x4e3640). A running clip answers its frame at the clock, the record's
 * frame range, flags word and speed; a bone the skeleton has but no clip
 * runs on answers 0, 0..1, no flags and no speed. */
struct BoneClip {
	float frame = 0.0f;
	int startFrame = 0;
	int endFrame = 1;
	std::uint32_t flags = 0;
	float speed = 0.0f;
};
enum class BoneClipAnswer {
	NoModel,    /* no model there, or one removed */
	NoBone,     /* the skeleton has no bone of that name */
	Inactive,   /* a bone the skeleton has, with no clip running */
	Active,     /* a running clip this store poses */
	Unserved,   /* a running clip outside the served forward timing */
};

struct Model {
	std::string name;           /* the mesh, as the module named it */
	std::string skeletonName;   /* the `.gla` the mesh names */
	/* That skeleton as the mesh header writes it, with no `.gla`: what Jedi
	 * Outcast's import 593 hands back (jk2mp.exe 1.02a 0x4a7d80). */
	std::string animName;
	model::Mesh mesh;
	model::Skeleton skeleton;
	/* The two files themselves, owned by the store and shared between every
	 * model that names them, so that a bolt matrix can reach the vertices and
	 * the poses. A star selector is metadata-only unless this store enables
	 * its engine-provided skeleton representation. */
	const std::vector<unsigned char> *meshBytes = nullptr;
	const std::vector<unsigned char> *skeletonBytes = nullptr;
	/* The names the files carry, read once when the model loads, so that a
	 * bolt can be looked up without keeping the files themselves. */
	std::vector<std::string> surfaceNames;
	std::vector<std::string> boneNames;
	/* The bolts the game has asked for, in the order it asked. */
	std::vector<Bolt> bolts;
	/* One record per bone the game has set playing, and per bone it bends. */
	std::vector<fntech3::ghoul2::BoneAnimation> animations;
	std::vector<fntech3::ghoul2::BoneAngles> angles;
	/* Retail's flags word for each bone either setter has reached. */
	std::vector<fntech3::ghoul2::BoneFlags> boneFlags;
	bool present = true;        /* false once the module removes it */
	/* The skin the game put on this model, as a handle from the registry
	 * below, or zero for none. A server keeps the handle and nothing else. */
	int customSkin = 0;
	/* The bolt the game last told this model to hang from, as a link (see
	 * `link` above): retail's -1 until SetBoltInfo says otherwise. Only a
	 * store that serves attachments poses through it; the others keep it. */
	int boltInfo = link::None;
	/* When a store that keeps builds last posed this model, as jk2mp.exe's
	 * +0x98 of the 0xbc-byte record: -1 from its constructor (0x4a8578), the
	 * build's time once posed (0x479450), 0 once a bone of it is bent
	 * (0x4a7578). A copy carries it with the rest of the record (0x4a7b9a). */
	int builtAt = -1;
};

/* One entity's models. The module holds a pointer to this and nothing else. */
class Instance {
public:
	int Count() const { return int( models_.size() ); }
	/* Whether this instance holds a model that is still there. Both titles
	 * ask this before doing anything else with an instance, and both ask it
	 * the same way: their engines walk the models and stop at the first one
	 * that has not been removed. An instance whose models have all been taken
	 * away holds none, even though it still has their places. */
	bool HoldsAny() const
	{
		for ( const Model &model : models_ ) if ( model.present ) return true;
		return false;
	}

	/* Whether the game has said this instance is the server's own. Raven's
	 * engine uses it to keep a server-side instance out of the renderer; a
	 * dedicated server has no renderer, so it is recorded and not acted on. */
	bool ServerOnly() const { return serverOnly_; }
	const Model *At( int index ) const {
		return index >= 0 && index < Count() && models_[std::size_t( index )].present
			? &models_[std::size_t( index )] : nullptr;
	}

private:
	friend class Store;
	std::vector<Model> models_;
	bool serverOnly_ = false;
	void *handle_ = nullptr;
};

/* Retail's pose and draw order for an instance's models (jamp.exe 1.01
 * 0x498be0, the pinned OpenJK G2_Sort_Models): every present model with no
 * link, by index, then repeatedly every linked model whose parent is in the
 * previous round, by index. A model whose chain of parents does not reach an
 * unlinked present model -- a missing parent, or a cycle -- is never posed or
 * drawn. A weapon instance whose only model links to itself is one. */
inline void PoseOrder( const Instance &instance, std::vector<int> &order )
{
	order.clear();
	for ( int index = 0; index < instance.Count(); ++index ) {
		const Model *model = instance.At( index );
		if ( model && model->boltInfo == link::None ) order.push_back( index );
	}
	for ( std::size_t start = 0, end = order.size(); start != end;
		start = end, end = order.size() ) {
		for ( int index = 0; index < instance.Count(); ++index ) {
			const Model *model = instance.At( index );
			if ( !model || model->boltInfo == link::None ) continue;
			const int parent = link::ParentModel( model->boltInfo );
			for ( std::size_t at = start; at < end; ++at ) {
				if ( order[at] == parent ) { order.push_back( index ); break; }
			}
		}
	}
}

/* Why a model of an instance could not be posed through its links. */
enum class Posing {
	Served,
	Unposed,      /* retail's sort never reaches it (see PoseOrder) */
	MissingBolt,  /* its link names a bolt its parent does not have */
	Unserved,     /* a pose or tag this engine does not serve */
};

inline const char *PosingName( Posing posing ) {
	switch ( posing ) {
	case Posing::Served: return "served";
	case Posing::Unposed: return "never posed by retail's model sort";
	case Posing::MissingBolt: return "linked to a bolt its parent lacks";
	case Posing::Unserved: return "outside the served pose or tag boundary";
	}
	return "unknown";
}

enum class NameMatch { Exact, AsciiFolded };

class Store {
public:
	/* Each capability is opted into by the consumer whose retail contract was
	 * measured for it; every other store keeps its earlier behavior.
	 * serveAttachments poses linked models from their parent's bolt and
	 * gives surface bolts retail's tag frame (see TagFrame). keepBuilds
	 * answers KeptBolt as Jedi Outcast's engine does: from the matrices one
	 * build per time leaves in each bolt (see Build); it is meant with
	 * serveAttachments, whose tag frame that engine shares. It also sets and
	 * poses clips with that engine's timing (SetBoneAnim, outcast::Control)
	 * and registers the module fields its instances are made for, as that
	 * engine's clean reads them (CleanField). */
	explicit Store( Files &files, bool serveDefaultSkeleton = false,
		bool serveAnimationBlends = false, bool serveAttachments = false,
		bool keepBuilds = false, NameMatch nameMatch = NameMatch::Exact )
		: files_( &files ), serveAnimationBlends_(serveAnimationBlends),
		serveAttachments_(serveAttachments), keepBuilds_(keepBuilds), nameMatch_(nameMatch) {
		if ( serveDefaultSkeleton ) Keep( "*default", builtin::DefaultSkeleton() );
	}
	bool ServesAttachments() const { return serveAttachments_; }
	bool KeepsBuilds() const { return keepBuilds_; }

	void Clear() { attached_.clear(); live_.clear(); instances_.clear(); fields_.clear(); }
	std::size_t InstanceCount() const { return instances_.size(); }

	/* An instance this store owns, or null. Never follow a module's pointer
	 * without asking this first. */
	Instance *Resolve( void *pointer ) {
		const auto found = live_.find( pointer );
		return found == live_.end() ? nullptr : found->second;
	}

	/* Add a model to the instance at `slot`, making one if there is none.
	 * Returns the model's index within the instance, or -1 with `error` set.
	 * `slot` is the module's own pointer field, which is written only when a
	 * model actually loads; `slotBytes` is its width (see ReadField). */
	int AddModel( void **slot, const char *name, LoadError &error,
		std::size_t slotBytes = sizeof( void * ) );

	/* Mark an instance as the server's own. */
	bool OverrideServer( void *pointer );

	/* Copy one model out of one instance into a place in another. The game
	 * does this when an entity takes over a single model rather than a whole
	 * body -- a weapon moved from one holder to the next. `modelTo` may name
	 * a model the destination already has, which is replaced, or the one past
	 * its last, which appends. Anything else is refused. */
	bool CopyModel( void *from, int modelFrom, void *to, int modelTo );

	/* Every bone of one model, posed for one moment, twelve floats each in
	 * bone order. */
	bool BoneMatrices( const Model &model, int now, std::vector<float> &out ) const;
	/* The same owned animation/angle state as a full palette. Weighted mesh
	 * geometry uses deformation; raw bone bolts use bones. `root`, when
	 * given, replaces the fixed root basis: the parent's bolt for an
	 * attached model. */
	bool DeformationPalette( const Model &model, int now, pose::Palette &out,
		const pose::Matrix *root = nullptr ) const;
	/* Retail's skeleton build (jamp.exe 0x49a050), for a store serving
	 * attachments: a model with no link is posed from the root basis, and a
	 * linked one from its parent's bolt (0x499850), its own skeleton composed
	 * onto that matrix. One model and the parents it needs. */
	Posing LinkedPalette( const Instance &instance, int modelIndex, int now,
		pose::Palette &out ) const;
	/* Every model retail would pose, in PoseOrder, each with its palette.
	 * On failure `failed` names the model and nothing is published. */
	Posing InstancePalettes( const Instance &instance, int now, std::vector<int> &order,
		std::vector<pose::Palette> &palettes, int &failed ) const;
	/* BoltMatrix with the reason a linked model could not answer. */
	Posing LinkedBolt( void *pointer, int modelIndex, int boltIndex, int now,
		float out[12] );
	/* Where one of a model's bolts is, in the model's own space, at one
	 * moment. False if the model cannot answer -- no skeleton kept, a bolt
	 * that names nothing, a surface this engine cannot reach -- and in that
	 * case nothing is written, because a bolt matrix that is not the answer
	 * is worse than none. */
	bool BoltMatrix( void *pointer, int modelIndex, int boltIndex, int now,
		float out[12] );

	/* Jedi Outcast's skeleton build (jk2mp.exe 1.02a 0x4790f0), for a store
	 * that keeps builds: one per time, judged by model 0 alone. */
	void Build( Instance &instance, int now );
	/* Jedi Outcast's bolt query (jk2mp.exe 1.02a 0x4a7620), for a store that
	 * keeps builds: Build at `now` unless a no-rebuild query is pending, then
	 * the bolt's kept matrix, in the model's own space. `noRebuild` is import
	 * 587, whose handler sets that pending flag before the query runs. */
	Posing KeptBolt( void *pointer, int modelIndex, int boltIndex, int now,
		bool noRebuild, float out[12] );

	/* Tell one of an instance's models which bolt it is using. The engine
	 * keeps the number and reads nothing into it. */
	bool SetBoltInfo( void *pointer, int modelIndex, int boltInfo );

	/* Put a skin on one of an instance's models. False when the instance is
	 * not one of ours or the model is not one of its own, which is a refusal
	 * rather than a write through whatever was handed in. */
	bool SetSkin( void *pointer, int modelIndex, int customSkin );

	/* Copy an instance, models and all, into a new one. The game does this
	 * when one entity takes over another's models -- a client given its own
	 * copy of a body already loaded. The copy owns its models; releasing
	 * either instance leaves the other alone. False when the source is not
	 * one of ours, which is a refusal rather than a copy of whatever was
	 * handed in. */
	bool Duplicate( void *pointer, void **slot );
	/* Jedi Outcast's duplicate (import 596), into the module's field of
	 * `fieldBytes` (see ReadField): nothing when the field already holds an
	 * instance. False when nothing was made. */
	bool DuplicateInto( void *pointer, void *field, std::size_t fieldBytes );
	/* Jedi Outcast's clean (import 599): empties the module's field and frees
	 * what it held only when the field is one this store registered. */
	void CleanField( void *field, std::size_t fieldBytes );
	/* Jedi Outcast's import 593: the skeleton name a model's mesh header
	 * carries (Model::animName), or null. */
	const char *GLAName( void *pointer, int modelIndex );

	/* Bend one bone of one model. The bone is named, as an animation's is, and
	 * a name the skeleton does not carry is refused. A bone bent twice keeps
	 * one record, the later one. */
	bool SetBoneAngles( void *pointer, int modelIndex, const char *boneName,
		const float *angles, int flags, int up, int right, int forward,
		int blendTime, int currentTime );
	/* How a bone was bent, or null. */
	/* `struct`: the member takes the record's name, which GCC then refuses
	 * for the return type without it. */
	const struct BoneAngles *BoneAngles( void *pointer, int modelIndex, const char *boneName );

	/* Set one bone of one model playing. The bone is named, as bolts are, and
	 * a name the skeleton does not carry is refused rather than recorded
	 * against nothing. A bone set twice keeps one record, the later one. */
	bool SetBoneAnim( void *pointer, int modelIndex, const char *boneName,
		int startFrame, int endFrame, int flags, float speed, int currentTime,
		float setFrame, int blendTime );
	/* What a bone was set playing, or null. */
	const struct BoneAnimation *BoneAnimation( void *pointer, int modelIndex,
		const char *boneName );
	/* Academy's import 597 for one bone at one clock (see BoneClip). Its
	 * name is matched without regard to case, as retail's _stricmp
	 * (0x539336) matches it in its bone find (0x4e2be0) and add (0x4e2c70).
	 * Retail's add then gives a bone no setter reached an empty record of
	 * its own, which no served import tells from none. `out` is written
	 * only for Inactive and Active. */
	BoneClipAnswer BoneClipAt( void *pointer, int modelIndex, const char *boneName,
		int now, BoneClip &out );
	/* Retail's flags word for a bone (see BoneFlags), or 0 for one neither
	 * setter has reached. */
	std::uint32_t BoneFlagsWord( void *pointer, int modelIndex, const char *boneName );

	/* Add a bolt to one of an instance's models, by the name of a surface or
	 * a bone. Returns the bolt's index, which is what the game keeps, or -1
	 * when the model does not carry that name. A name added twice keeps its
	 * first index, because the game asks for the same bolt on every entity
	 * that uses the model. */
	int AddBolt( void *pointer, int modelIndex, const char *name );
	/* A bolt the game was given, or null. */
	const Bolt *BoltAt( void *pointer, int modelIndex, int boltIndex );

	/* Attach an instance to an entity number, so that another entity's bolt
	 * can be found through it. The game asks the engine to remember this and
	 * asks for it back by number. False when the instance is not one of ours
	 * or the number is outside the entity array; an attachment is never made
	 * to a pointer this store did not hand out. */
	bool Attach( void *pointer, int entityNum );
	/* Forget what is attached to one entity. */
	bool Detach( int entityNum );
	/* Forget every attachment. This is what the game asks for when a level
	 * ends, and it really does clear what the engine holds. */
	void ClearAttachments() { attached_.clear(); }
	/* The instance attached to an entity, or null. */
	Instance *Attached( int entityNum );
	int AttachmentCount() const { return int( attached_.size() ); }

	/* Forget an instance the module is finished with. */
	bool Release( void *pointer );
	/* Release an owned handle and clear exactly the module's pointer slot.
	 * An already-null handle is a no-op; stale/foreign handles are refused and
	 * leave the slot unchanged. Slots may be unaligned in module memory. */
	bool Clean( void **slot );

private:
	bool ReadFile( const std::string &path, std::vector<unsigned char> &into );
	LoadError Load( const char *name, Model &out );
	/* SetBoneAnim for a store keeping builds: Jedi Outcast's setter. */
	bool SetOutcastBoneAnim( void *pointer, int modelIndex, const char *boneName,
		int startFrame, int endFrame, int flags, float speed, int currentTime,
		float setFrame, int blendTime );

	/* One model's bolt from a palette already posed for it. */
	bool BoltFromPalette( const Model &model, const pose::Palette &palette,
		int boltIndex, float out[12] ) const;
	/* Palettes computed so far for one instance, so a parent shared by
	 * several attachments is posed once. */
	struct Posed {
		pose::Palette palette;
		Posing posing = Posing::Unposed;
		unsigned char state = 0; /* 0 not asked, 1 being posed, 2 answered */
	};
	Posing Pose( const Instance &instance, int index, int now,
		std::vector<Posed> &posed ) const;

	/* The mesh and skeleton files themselves, kept once each and shared by
	 * every model that names them.
	 *
	 * A bolt matrix has to reach a surface's vertices and a skeleton's bones
	 * and poses, and those live in the files. Re-reading a skeleton that runs
	 * to nine megabytes for every query would not do, and one copy per model
	 * would be worse: every humanoid in either game names the same skeleton.
	 * So one copy each, found by name. */
	struct Kept {
		std::string name;
		std::vector<unsigned char> bytes;
	};
	/* Held one at a time rather than packed together, because models keep
	 * pointers into these and a vector that grows would move them. */
	std::vector<std::unique_ptr<Kept>> kept_;
	const std::vector<unsigned char> *Keep( const std::string &name,
		std::vector<unsigned char> &&bytes );
	const std::vector<unsigned char> *Kept_( const std::string &name ) const;

	Files *files_ = nullptr;
	bool serveAnimationBlends_ = false;
	bool serveAttachments_ = false;
	bool keepBuilds_ = false;
	/* Academy's shared retail setters and AddBolt use _stricmp. Other
	 * consumers keep exact names until measured; see the bone names record. */
	NameMatch nameMatch_ = NameMatch::Exact;
	/* jk2mp.exe's global 0x1ae6634: set by import 587's handler (0x44c687),
	 * cleared only by a bolt query whose indices check out (0x4a76c1). Like
	 * the engine's, it outlives a map change. */
	bool noRebuildPending_ = false;
	/* For a store keeping builds, the module fields its instances were made
	 * for, by address, as jk2mp.exe lists them (0x1ae664c, 0x1ae6638): 588
	 * and 596 add one when they make an instance, and 599 frees only through
	 * one. A field made for twice is listed twice. */
	std::vector<const void *> fields_;
	std::vector<std::unique_ptr<Instance>> instances_;
	std::unordered_map<void *, Instance *> live_;
	std::unordered_map<int, void *> attached_;
};

inline bool Store::ReadFile( const std::string &path, std::vector<unsigned char> &into )
{
	const std::size_t size = files_->Read( path.c_str(), nullptr, 0 );
	if ( size == 0 || size > MaxModelBytes ) return false;
	into.assign( size, 0 );
	const std::size_t read = files_->Read( path.c_str(), into.data(), into.size() );
	return read == size;
}

inline const std::vector<unsigned char> *Store::Kept_( const std::string &name ) const
{
	for ( const std::unique_ptr<Kept> &entry : kept_ ) {
		if ( entry && entry->name == name ) return &entry->bytes;
	}
	return nullptr;
}

inline const std::vector<unsigned char> *Store::Keep( const std::string &name,
	std::vector<unsigned char> &&bytes )
{
	if ( const std::vector<unsigned char> *already = Kept_( name ) ) return already;
	if ( kept_.size() >= MaxKeptFiles ) return nullptr;
	std::unique_ptr<Kept> entry( new Kept );
	entry->name = name;
	entry->bytes = std::move( bytes );
	kept_.push_back( std::move( entry ) );
	return &kept_.back()->bytes;
}

inline LoadError Store::Load( const char *name, Model &out )
{
	if ( !name || !name[0] ) return LoadError::NoName;
	out = Model{};
	out.name = name;

	std::vector<unsigned char> mesh;
	if ( !ReadFile( out.name, mesh ) ) return LoadError::Missing;

	/* Read the mesh once with no skeleton to learn the one it names, then
	 * again against that skeleton's real bone count. The reader does the
	 * cross-check; nothing here assumes the two agree. */
	if ( model::ParseMesh( mesh.data(), mesh.size(), 0, out.mesh ) != model::Error::None ) {
		return LoadError::Mesh;
	}
	char named[model::NameBytes + 1] = { 0 };
	if ( out.mesh.animNameOffset == 0
		|| out.mesh.animNameOffset + model::NameBytes > mesh.size() ) {
		return LoadError::SkeletonName;
	}
	std::memcpy( named, mesh.data() + out.mesh.animNameOffset, model::NameBytes );
	named[model::NameBytes] = 0;
	if ( !named[0] ) return LoadError::SkeletonName;
	out.animName = named;

	/* A name beginning with a star is an internal engine selector, not a file.
	 * An explicitly enabled store owns the measured *default skeleton. Other
	 * stores retain their previous metadata-only behavior for star selectors.
	 *
	 * Measured, not assumed: across both retail installations `*default` is
	 * the only such name there is -- 41 of Jedi Academy's 127 meshes and 23 of
	 * Jedi Outcast's 71 carry it, including every lightsaber -- and every name
	 * that does not begin with a star resolves to a `.gla` that is present, in
	 * both. So the star is the rule and not an exception to chase. */
	const auto *provided = named[0] == '*' ? Kept_( named ) : nullptr;
	if ( named[0] == '*' && !provided ) {
		out.skeletonName = named;
		out.skeleton = model::Skeleton{};
		out.skeleton.numBones = out.mesh.numBones;
		return model::ParseMesh( mesh.data(), mesh.size(), out.mesh.numBones, out.mesh )
			== model::Error::None ? LoadError::None : LoadError::Bones;
	}

	out.skeletonName = provided ? std::string( named ) : std::string( named ) + ".gla";

	std::vector<unsigned char> skeleton;
	if ( provided ) skeleton = *provided;
	else if ( !ReadFile( out.skeletonName, skeleton ) ) return LoadError::SkeletonFile;
	if ( model::ParseSkeleton( skeleton.data(), skeleton.size(), out.skeleton )
		!= model::Error::None ) {
		return LoadError::Skeleton;
	}
	if ( model::ParseMesh( mesh.data(), mesh.size(), out.skeleton.numBones, out.mesh )
		!= model::Error::None ) {
		return LoadError::Bones;
	}

	/* Read the names now, while the files are still here. A file that names
	 * more than this engine will hold is read as far as it will hold and no
	 * further; a bolt it cannot name is then reported as not found, which is
	 * what a missing name means anyway. */
	std::memset( named, 0, sizeof( named ) );
	const int surfaces = out.mesh.numSurfaces < MaxNamedSurfaces
		? out.mesh.numSurfaces : MaxNamedSurfaces;
	for ( int index = 0; index < surfaces; ++index ) {
		out.surfaceNames.push_back(
			model::SurfaceName( mesh.data(), mesh.size(), out.mesh, index, named,
				sizeof( named ) ) ? named : std::string() );
	}
	const int bones = out.skeleton.numBones < MaxNamedBones
		? out.skeleton.numBones : MaxNamedBones;
	for ( int index = 0; index < bones; ++index ) {
		out.boneNames.push_back(
			model::BoneName( skeleton.data(), skeleton.size(), out.skeleton, index,
				named, sizeof( named ) ) ? named : std::string() );
	}

	/* Keep both files. A model that cannot be kept still loads and still
	 * answers every question that does not need the bytes; it is a bolt
	 * matrix that goes without, and it says so rather than inventing one. */
	out.meshBytes = Keep( out.name, std::move( mesh ) );
	out.skeletonBytes = Keep( out.skeletonName, std::move( skeleton ) );
	return LoadError::None;
}

inline int Store::AddModel( void **slot, const char *name, LoadError &error,
	std::size_t slotBytes )
{
	error = LoadError::None;
	/* A bytecode module's field is a 32-bit word: reading or writing a
	 * pointer's width there on a 64-bit host would run into the next field. */
	void *handle = slot ? ReadField( slot, slotBytes ) : nullptr;
	Instance *instance = Resolve( handle );

	Model loaded;
	error = Load( name, loaded );
	if ( error != LoadError::None ) return -1;

	if ( instance && instance->Count() >= MaxModelsPerInstance ) {
		error = LoadError::Full;
		return -1;
	}

	if ( !instance ) {
		handle = ModelHandleIdentities.Allocate();
		if ( !handle || ( slot && !FieldHolds( handle, slotBytes ) ) ) {
			error = LoadError::HandleExhausted;
			return -1;
		}
		instances_.push_back( std::make_unique<Instance>() );
		instance = instances_.back().get();
		instance->handle_ = handle;
		live_.emplace( handle, instance );
		/* jk2mp.exe's 588 lists the field it makes an instance for
		 * (0x4a6f29..0x4a6f51); its clean frees only through that list. */
		if ( keepBuilds_ && slot ) fields_.push_back( slot );
	}
	instance->models_.push_back( std::move( loaded ) );
	if ( slot ) WriteField( slot, slotBytes, instance->handle_ );
	return instance->Count() - 1;
}

inline bool Store::OverrideServer( void *pointer )
{
	Instance *instance = Resolve( pointer );
	if ( !instance ) return false;
	instance->serverOnly_ = true;
	return true;
}

inline bool Store::CopyModel( void *from, int modelFrom, void *to, int modelTo )
{
	Instance *source = Resolve( from );
	Instance *destination = Resolve( to );
	if ( !source || !destination ) return false;
	if ( modelFrom < 0 || modelFrom >= source->Count() ) return false;
	const Model &model = source->models_[std::size_t( modelFrom )];
	if ( !model.present ) return false;
	if ( modelTo < 0 || modelTo > destination->Count() ) return false;
	if ( modelTo == destination->Count() ) {
		if ( destination->Count() >= MaxModelsPerInstance ) return false;
		destination->models_.push_back( model );
		/* jk2mp.exe 0x4a7caa: a copy that grows the instance sets model 0's
		 * build time to 0, so the next query builds again. One that replaces
		 * a model does not. */
		if ( keepBuilds_ ) destination->models_.front().builtAt = 0;
		return true;
	}
	destination->models_[std::size_t( modelTo )] = model;
	return true;
}

/* Every bone's raw model-space transform, with frame-zero defaults and
 * inherited animation. Unsupported controls cause query failure; stored
 * metadata alone does not establish that its pose effect is implemented. */
inline bool Store::DeformationPalette( const Model &model, int now,
	pose::Palette &out, const pose::Matrix *root ) const
{
	if ( !model.skeletonBytes || model.skeleton.numBones <= 0 ) return false;
	const void *data = model.skeletonBytes->data();
	const std::size_t size = model.skeletonBytes->size();
	std::vector<pose::Animation> animations;
	for ( const auto &playing : model.animations ) {
		if ( keepBuilds_ ) {
			/* Jedi Outcast's own clip timing (outcast::Control). */
			pose::Animation control;
			bool has = false;
			if ( !outcast::Control( playing, now, control, has ) ) return false;
			if ( has ) animations.push_back( control );
			continue;
		}
		/* Only the Academy owned cgame store opts into the measured active
		 * transition. Other consumers retain their existing refusal. */
		if ( playing.flags & ~(serveAnimationBlends_ ? 0xd8 : 0x58)
			|| !FiniteScalar( playing.speed ) || playing.speed < 0.0f ) return false;
		if ( !(playing.flags & 0x18) ) {
			if ( playing.flags & 0x80 ) return false;
			continue;
		}
		Playhead at;
		if ( !ForwardPlayhead(playing, model.skeleton.numFrames, now, at) ) return false;
		pose::Animation control{playing.bone, {at.currentFrame, at.newFrame, at.lerp}};
		const std::int64_t age = std::int64_t(now) - playing.blendStart;
		if ( (playing.flags & 0x80) && age >= 0 && age < playing.blendTime ) {
			if ( !playing.previousKnown ) return false;
			control.previous = playing.previous;
			control.transition = float(age) / float(playing.blendTime);
			control.blending = true;
		}
		animations.push_back(control);
	}
	std::vector<pose::Angle> angles;
	for ( const auto &bent : model.angles ) {
		if ( !bent.flags ) continue;
		if ( bent.flags != 1 && bent.flags != 2 && bent.flags != 4 ) return false;
		/* Jedi Outcast's transform reads an angle's blend time only where it
		 * replaces the bone (jk2mp.exe 0x476b8b), and that path rescales the
		 * stored matrix in place on every build (0x476c18) and was not
		 * followed: a store keeping builds poses PREMULT and POSTMULT at any
		 * blend time (0x476d39, 0x476d99) and refuses REPLACE. Academy also
		 * reads boneBlendTime only inside REPLACE (jamp.exe 0x497c34,
		 * 0x497e21): PREMULT/POSTMULT at 0x4980ac/0x497d94 never read it.
		 * Its MP stores opt into this; earlier stores retain
		 * their existing zero-blend boundary. */
		if ( keepBuilds_ ? bent.flags == 4
			: bent.blendTime != 0 && (!serveAnimationBlends_ || bent.flags == 4) ) return false;
		pose::Matrix base, inverse, post;
		if ( !model::BoneRestMatrix(data, size, model.skeleton, bent.bone, base.data())
			|| !model::BoneRestMatrixInverse(data, size, model.skeleton, bent.bone, inverse.data())
			|| !pose::AngleMatrix(base, inverse, bent.angles, bent.flags, bent.up, bent.right, bent.forward, post) ) return false;
		angles.push_back({bent.bone, post, bent.flags});
	}
	return pose::Evaluate(data, size, model.skeleton, animations, angles, out,
		root ? *root : pose::RootBasis);
}

inline bool Store::BoneMatrices( const Model &model, int now,
	std::vector<float> &out ) const
{
	pose::Palette palette;
	if ( !DeformationPalette(model, now, palette) ) return false;
	std::vector<float> staged;
	staged.reserve(palette.bones.size()*12);
	for ( const auto &bone : palette.bones ) staged.insert(staged.end(), bone.begin(), bone.end());
	out.swap(staged);
	return true;
}

namespace detail {

inline void Cross( const float a[3], const float b[3], float out[3] )
{
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

inline bool Normalise( float v[3] )
{
	for ( int axis = 0; axis < 3; ++axis ) if ( !FiniteScalar(v[axis]) ) return false;
	const float length = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	if ( !FiniteScalar(length) || !( length > 1e-6f ) ) return false;
	v[0] /= length; v[1] /= length; v[2] /= length;
	return true;
}

}  /* namespace detail */

/* Retail's tag frame (jamp.exe 1.01 0x499476..0x499848, reached from
 * G2_GetBoltMatrixLow 0x499850 through 0x498df0; the pinned OpenJK
 * G2_ProcessSurfaceBolt2 reads the same). From the tag's first three
 * vertices p0, p1, p2 in their stored order (not a triangle's corners):
 *
 *     a = |p1 - p0|, b = |p0 - p2|, a = |a - (a.b) b|, c = |(p1 - p0) x (p0 - p2)|
 *     columns b, a, -c; origin p2
 *
 * A collapsed tag has no frame. Retail would carry zero vectors on; this
 * engine refuses it instead. */
inline bool TagFrame( const float point[3][3], float out[12] )
{
	float sides[3][3];
	for ( int side = 0; side < 3; ++side ) {
		for ( int axis = 0; axis < 3; ++axis ) {
			sides[side][axis] = point[( side + 1 ) % 3][axis] - point[side][axis];
		}
	}
	float first[3] = { sides[0][0], sides[0][1], sides[0][2] };
	float second[3] = { sides[2][0], sides[2][1], sides[2][2] };
	if ( !detail::Normalise( first ) || !detail::Normalise( second ) ) return false;
	const float along = first[0] * second[0] + first[1] * second[1] + first[2] * second[2];
	for ( int axis = 0; axis < 3; ++axis ) first[axis] -= along * second[axis];
	if ( !detail::Normalise( first ) ) return false;
	float normal[3];
	detail::Cross( sides[0], sides[2], normal );
	if ( !detail::Normalise( normal ) ) return false;
	for ( int row = 0; row < 3; ++row ) {
		out[row * 4 + 0] = second[row];
		out[row * 4 + 1] = first[row];
		out[row * 4 + 2] = -normal[row];
		out[row * 4 + 3] = point[2][row];
	}
	return true;
}

/* Where a bolt is.
 *
 * A bolt that names a bone is that bone's matrix. A bolt that names a surface
 * is a tag: a three-cornered surface whose vertices are carried by the bones
 * like any others, and whose position and orientation fall out of where those
 * three corners end up. Each corner is skinned the ordinary way -- the sum
 * over its weights of the bone's matrix applied to it -- and then the corners
 * give an origin and a set of axes.
 *
 * The axes go in as columns, not rows, which is how the retail engine writes
 * them: its element 0, element 4 and element 8 are one axis. A store serving
 * attachments uses retail's tag frame above; the others keep this engine's
 * earlier frame, which is not retail's (first triangle's corners, origin at
 * the first, columns along, side, normal). */
inline bool Store::BoltFromPalette( const Model &model, const pose::Palette &palette,
	int boltIndex, float out[12] ) const
{
	if ( !out || boltIndex < 0 || boltIndex >= int( model.bolts.size() ) ) return false;
	const Bolt &bolt = model.bolts[std::size_t( boltIndex )];
	if ( bolt.bone >= 0 ) {
		if ( bolt.bone >= model.skeleton.numBones
			|| std::size_t( bolt.bone ) >= palette.bones.size() ) return false;
		const float *found = palette.bones[std::size_t( bolt.bone )].data();
		for ( int i = 0; i < 12; ++i ) out[i] = found[i];
		return true;
	}
	if ( bolt.surface < 0 || !model.meshBytes ) return false;

	const void *mesh = model.meshBytes->data();
	const std::size_t meshSize = model.meshBytes->size();
	model::MeshSurface surface;
	if ( !model::SurfaceAt( mesh, meshSize, model.mesh, 0, bolt.surface, surface ) ) {
		return false;
	}
	int corner[3] = { 0, 1, 2 };
	if ( !serveAttachments_ && !model::TriangleCorners( mesh, meshSize, surface, 0, corner ) ) {
		return false;
	}

	/* Use the same deformation palette as weighted geometry. BasePose is
	 * already absent here; applying its inverse again would deform twice. */
	float point[3][3];
	for ( int which = 0; which < 3; ++which ) {
		model::SkinnedVertex vertex;
		if ( !model::VertexAt( mesh, meshSize, surface, corner[which], vertex ) ) {
			return false;
		}
		float at[3] = { 0.0f, 0.0f, 0.0f };
		for ( int weight = 0; weight < vertex.count; ++weight ) {
			const int bone = vertex.bone[weight];
			if ( bone < 0 || bone >= model.skeleton.numBones
				|| std::size_t( bone ) >= palette.deformation.size() ) return false;
			const float *carry = palette.deformation[std::size_t(bone)].data();
			for ( int axis = 0; axis < 3; ++axis ) {
				at[axis] += vertex.weight[weight]
					* ( carry[axis * 4 + 0] * vertex.position[0]
						+ carry[axis * 4 + 1] * vertex.position[1]
						+ carry[axis * 4 + 2] * vertex.position[2]
						+ carry[axis * 4 + 3] );
			}
		}
		for ( int axis = 0; axis < 3; ++axis ) {
			if ( !FiniteScalar(at[axis]) ) return false;
			point[which][axis] = at[axis];
		}
	}
	if ( serveAttachments_ ) {
		float staged[12];
		if ( !TagFrame( point, staged ) ) return false;
		std::memcpy( out, staged, sizeof( staged ) );
		return true;
	}

	/* Two edges of the tag, their normal, and a third axis square to both. A
	 * tag whose corners have collapsed onto one another has no orientation to
	 * give and is refused rather than answered with whatever the arithmetic
	 * happens to produce. */
	float along[3], across[3], normal[3], side[3];
	for ( int axis = 0; axis < 3; ++axis ) {
		along[axis] = point[1][axis] - point[0][axis];
		across[axis] = point[2][axis] - point[0][axis];
	}
	if ( !detail::Normalise( along ) ) return false;
	detail::Cross( along, across, normal );
	if ( !detail::Normalise( normal ) ) return false;
	detail::Cross( normal, along, side );
	if ( !detail::Normalise( side ) ) return false;

	for ( int row = 0; row < 3; ++row ) {
		out[row * 4 + 0] = along[row];
		out[row * 4 + 1] = side[row];
		out[row * 4 + 2] = normal[row];
		out[row * 4 + 3] = point[0][row];
	}
	return true;
}

inline Posing Store::Pose( const Instance &instance, int index, int now,
	std::vector<Posed> &posed ) const
{
	if ( index < 0 || index >= instance.Count()
		|| std::size_t( index ) >= posed.size() ) return Posing::Unposed;
	Posed &entry = posed[std::size_t( index )];
	if ( entry.state == 2 ) return entry.posing;
	/* A link cycle never reaches a model of its own, so retail never poses
	 * any model on it. */
	if ( entry.state == 1 ) return Posing::Unposed;
	entry.state = 1;
	Posing result = Posing::Unposed;
	const Model *model = instance.At( index );
	if ( model && model->boltInfo == link::None ) {
		result = DeformationPalette( *model, now, entry.palette )
			? Posing::Served : Posing::Unserved;
	} else if ( model ) {
		const int parentIndex = link::ParentModel( model->boltInfo );
		result = Pose( instance, parentIndex, now, posed );
		if ( result == Posing::Served ) {
			const Model &parent = *instance.At( parentIndex );
			const int boltIndex = link::ParentBolt( model->boltInfo );
			pose::Matrix root;
			/* Retail reads the parent's bolt without a bound (0x49986b);
			 * one that is not there is refused here rather than read. */
			if ( boltIndex >= int( parent.bolts.size() ) ) result = Posing::MissingBolt;
			else if ( !BoltFromPalette( parent, posed[std::size_t( parentIndex )].palette,
				boltIndex, root.data() ) ) result = Posing::Unserved;
			else result = DeformationPalette( *model, now, entry.palette, &root )
				? Posing::Served : Posing::Unserved;
		}
	}
	entry.posing = result;
	entry.state = 2;
	return result;
}

inline Posing Store::LinkedPalette( const Instance &instance, int modelIndex, int now,
	pose::Palette &out ) const
{
	if ( !serveAttachments_ ) return Posing::Unserved;
	std::vector<Posed> posed( std::size_t( instance.Count() ) );
	const Posing result = Pose( instance, modelIndex, now, posed );
	if ( result == Posing::Served ) out = std::move( posed[std::size_t( modelIndex )].palette );
	return result;
}

inline Posing Store::InstancePalettes( const Instance &instance, int now,
	std::vector<int> &order, std::vector<pose::Palette> &palettes, int &failed ) const
{
	failed = -1;
	std::vector<int> staged;
	PoseOrder( instance, staged );
	if ( !serveAttachments_ ) {
		failed = staged.empty() ? -1 : staged.front();
		return Posing::Unserved;
	}
	std::vector<Posed> posed( std::size_t( instance.Count() ) );
	for ( int index : staged ) {
		const Posing result = Pose( instance, index, now, posed );
		if ( result != Posing::Served ) { failed = index; return result; }
	}
	std::vector<pose::Palette> ready( staged.size() );
	for ( std::size_t at = 0; at < staged.size(); ++at ) {
		ready[at] = std::move( posed[std::size_t( staged[at] )].palette );
	}
	order.swap( staged );
	palettes.swap( ready );
	return Posing::Served;
}

inline Posing Store::LinkedBolt( void *pointer, int modelIndex, int boltIndex,
	int now, float out[12] )
{
	if ( !out ) return Posing::Unserved;
	Instance *instance = Resolve( pointer );
	if ( !instance ) return Posing::Unserved;
	const Model *model = instance->At( modelIndex );
	if ( !model ) return Posing::Unposed;
	if ( boltIndex < 0 || boltIndex >= int( model->bolts.size() ) ) return Posing::MissingBolt;
	pose::Palette palette;
	Posing result = Posing::Served;
	/* Retail's bolt query builds the whole instance's skeleton first
	 * (0x4dfa00 calls 0x49a050), so an attached model answers from where
	 * its parent's bolt has put it. */
	if ( serveAttachments_ ) result = LinkedPalette( *instance, modelIndex, now, palette );
	else if ( !DeformationPalette( *model, now, palette ) ) result = Posing::Unserved;
	if ( result != Posing::Served ) return result;
	return BoltFromPalette( *model, palette, boltIndex, out ) ? Posing::Served : Posing::Unserved;
}

inline bool Store::BoltMatrix( void *pointer, int modelIndex, int boltIndex,
	int now, float out[12] )
{
	return LinkedBolt( pointer, modelIndex, boltIndex, now, out ) == Posing::Served;
}

/* Jedi Outcast's build (jk2mp.exe 1.02a 0x4790f0). Nothing is done when model
 * 0 was last built at `now` (0x479148). Otherwise each model retail's sort
 * reaches (0x479010, PoseOrder), in that order, is posed at `now` from the
 * fixed root basis or, when linked, from its parent's kept bolt, which retail
 * reads without a bound (0x479577..0x4795d4), and keeps the matrix of every
 * bolt it has: a bone's at the end of the transform (0x476ee0), a tag's in
 * retail's frame (0x4780b0). A model the sort does not reach keeps what it
 * had. What this engine does not pose -- a pose or tag outside the served
 * boundary, or a parent's bolt that is missing or was never kept -- leaves
 * that model's bolts unkept rather than computed from what retail would read.
 *
 * Outcast's engine smooths bones over 200 ms (r_ghoul2animsmooth, 0x4795e8)
 * and renormalizes them after (r_ghoul2unsqashaftersmooth, 0x4797b9) only
 * when its renderer has registered those cvars (R_Register 0x47ed00), which a
 * dedicated server never does (CL_Init is skipped, 0x429e29): this build is
 * the dedicated server's. r_noserverghoul2 (cheat, default 0) is not read. */
inline void Store::Build( Instance &instance, int now )
{
	if ( instance.models_.empty() || instance.models_.front().builtAt == now ) return;
	std::vector<int> order;
	PoseOrder( instance, order );
	for ( int index : order ) {
		Model &model = instance.models_[std::size_t( index )];
		model.builtAt = now;
		pose::Matrix root = pose::RootBasis;
		bool rooted = true;
		if ( model.boltInfo != link::None ) {
			/* PoseOrder places a linked model only after its parent. */
			const Model &parent = instance.models_[std::size_t( link::ParentModel( model.boltInfo ) )];
			const std::size_t bolt = std::size_t( link::ParentBolt( model.boltInfo ) );
			rooted = bolt < parent.bolts.size() && parent.bolts[bolt].kept;
			if ( rooted ) root = parent.bolts[bolt].matrix;
		}
		pose::Palette palette;
		const bool posed = rooted && DeformationPalette( model, now, palette, &root );
		for ( std::size_t at = 0; at < model.bolts.size(); ++at ) {
			Bolt &bolt = model.bolts[at];
			float matrix[12];
			bolt.kept = posed && BoltFromPalette( model, palette, int( at ), matrix );
			if ( bolt.kept ) std::memcpy( bolt.matrix.data(), matrix, sizeof( matrix ) );
		}
	}
}

/* Jedi Outcast's bolt query (jk2mp.exe 1.02a 0x4a7620): an instance this
 * store holds, a model index below its count and a bolt index inside that
 * model's list (0x4a762f..0x4a7679), then the build, skipped while a
 * no-rebuild query is pending, then the bolt's kept matrix (0x4a76cb). The
 * pending flag is cleared only by a query that passed those checks, so a 587
 * refused for its indices leaves the next query unbuilt as well. Retail
 * answers a bolt no build has reached with what its record held; this store
 * refuses it and writes nothing. */
inline Posing Store::KeptBolt( void *pointer, int modelIndex, int boltIndex, int now,
	bool noRebuild, float out[12] )
{
	if ( noRebuild ) noRebuildPending_ = true;
	if ( !out || !keepBuilds_ ) return Posing::Unserved;
	Instance *instance = Resolve( pointer );
	if ( !instance ) return Posing::Unserved;
	if ( modelIndex < 0 || modelIndex >= instance->Count() ) return Posing::Unposed;
	const Model &model = instance->models_[std::size_t( modelIndex )];
	if ( boltIndex < 0 || boltIndex >= int( model.bolts.size() ) ) return Posing::MissingBolt;
	const bool build = !noRebuildPending_;
	noRebuildPending_ = false;
	if ( build ) Build( *instance, now );
	const Bolt &bolt = model.bolts[std::size_t( boltIndex )];
	if ( !bolt.kept ) return Posing::Unposed;
	std::memcpy( out, bolt.matrix.data(), sizeof( float ) * 12 );
	return Posing::Served;
}

inline bool Store::SetBoltInfo( void *pointer, int modelIndex, int boltInfo )
{
	Instance *instance = Resolve( pointer );
	if ( !instance || modelIndex < 0 || modelIndex >= instance->Count() ) return false;
	Model &model = instance->models_[std::size_t( modelIndex )];
	if ( !model.present ) return false;
	model.boltInfo = boltInfo;
	return true;
}

inline bool Store::SetSkin( void *pointer, int modelIndex, int customSkin )
{
	Instance *instance = Resolve( pointer );
	if ( !instance || customSkin < 0 ) return false;
	if ( modelIndex < 0 || modelIndex >= instance->Count() ) return false;
	Model &model = instance->models_[std::size_t( modelIndex )];
	if ( !model.present ) return false;
	model.customSkin = customSkin;
	return true;
}

inline bool Store::Duplicate( void *pointer, void **slot )
{
	Instance *source = Resolve( pointer );
	if ( !source || !slot ) return false;
	void *handle = ModelHandleIdentities.Allocate();
	if ( !handle ) return false;
	instances_.push_back( std::make_unique<Instance>() );
	Instance *copy = instances_.back().get();
	copy->handle_ = handle;
	live_.emplace( handle, copy );
	copy->models_ = source->models_;
	std::memcpy( slot, &handle, sizeof( handle ) );
	return true;
}

namespace detail {
/* Locale-independent ASCII folding for Academy's _stricmp name contract
 * (jamp.exe 1.01 0x539336). Non-ASCII bytes remain exact. */
inline bool NameEqual( const std::string &candidate, const char *name, NameMatch match )
{
	if ( !name ) return false;
	if ( match == NameMatch::Exact ) return candidate == name;
	const auto fold = []( char c ) {
		return c >= 'A' && c <= 'Z' ? char( c - 'A' + 'a' ) : c;
	};
	std::size_t at = 0;
	while ( at < candidate.size() && name[at] && fold( candidate[at] ) == fold( name[at] ) ) ++at;
	return at == candidate.size() && !name[at];
}

/* The first bone carrying a name under this consumer's contract, or -1. */
inline int BoneNamed( const Model &model, const char *name, NameMatch match = NameMatch::Exact )
{
	if ( !name || !name[0] ) return -1;
	for ( int index = 0; index < int( model.boneNames.size() ); ++index ) {
		if ( NameEqual( model.boneNames[std::size_t( index )], name, match ) ) return index;
	}
	return -1;
}

inline int BoneNamedFolded( const Model &model, const char *name )
{
	return BoneNamed( model, name, NameMatch::AsciiFolded );
}

/* A bone's flags word (see BoneFlags), made empty the first time a setter
 * reaches the bone. */
inline std::uint32_t &BoneFlagsOf( Model &model, int bone )
{
	for ( auto &record : model.boneFlags ) {
		if ( record.bone == bone ) return record.word;
	}
	model.boneFlags.push_back( { bone, 0u } );
	return model.boneFlags.back().word;
}

inline std::uint32_t BoneFlagsWord( const Model &model, int bone )
{
	for ( const auto &record : model.boneFlags ) {
		if ( record.bone == bone ) return record.word;
	}
	return 0u;
}

/* The clip setter's word (jamp.exe 1.01 0x4e3340..0x4e3589). */
inline void RecordClipFlags( Model &model, int bone, int flags )
{
	std::uint32_t &word = BoneFlagsOf( model, bone );
	if ( word & BoneLockFlag ) return;
	word |= BoneMarkFlag;
	std::uint32_t given = std::uint32_t( flags );
	if ( !( word & 0x18u ) ) given &= ~0x80u;
	word &= ~BoneAnimationBits;
	if ( word & 0x80000000u ) word = 0u;
	word |= given;
}

/* The angle setter's word (jamp.exe 1.01 0x4e326e..0x4e32aa). */
inline void RecordAngleFlags( Model &model, int bone, int flags )
{
	std::uint32_t &word = BoneFlagsOf( model, bone );
	if ( word & BoneLockFlag ) return;
	word = ( word & ~BoneAngleBits ) | std::uint32_t( flags );
}
} // namespace detail

inline bool Store::SetBoneAnim( void *pointer, int modelIndex, const char *boneName,
	int startFrame, int endFrame, int flags, float speed, int currentTime,
	float setFrame, int blendTime )
{
	if ( keepBuilds_ ) return SetOutcastBoneAnim( pointer, modelIndex, boneName, startFrame, endFrame, flags, speed, currentTime, setFrame, blendTime );
	if ( !FiniteScalar( speed ) || !FiniteScalar( setFrame ) ) return false;
	int startTime = currentTime;
	if ( setFrame != -1.0f ) {
		if ( setFrame < float( startFrame ) || setFrame > float( endFrame ) || speed == 0.0f ) return false;
		/* Retail jamp.exe VA 0x4e3505 compares the -1.0f sentinel; the path at
		 * 0x4e3523-0x4e353e shifts the clock by a fractional frame and truncates
		 * the resulting milliseconds. The import's ninth argument is a float,
		 * even though the start and end frame arguments are integers. */
		const double shifted = double( currentTime )
			- ( double( setFrame - float( startFrame ) ) * 50.0 ) / speed;
		if ( shifted < INT_MIN || shifted > INT_MAX ) return false;
		startTime = static_cast<int>( shifted );
	}
	Instance *instance = Resolve( pointer );
	if ( !instance || modelIndex < 0 || modelIndex >= instance->Count() ) return false;
	Model &model = instance->models_[std::size_t( modelIndex )];
	if ( !model.present ) return false;
	const int bone = detail::BoneNamed( model, boneName, nameMatch_ );
	if ( bone < 0 ) return false;

	fntech3::ghoul2::BoneAnimation playing;
	playing.bone = bone;
	playing.startFrame = startFrame;
	playing.endFrame = endFrame;
	playing.flags = flags;
	playing.speed = speed;
	playing.startTime = startTime;
	playing.setFrame = setFrame;
	playing.blendTime = blendTime;
	if ( serveAnimationBlends_ && (flags & 0x80) ) {
		const auto *prior = BoneAnimation(pointer, modelIndex, boneName);
		if ( !prior || !(prior->flags & 0x18) ) {
			playing.flags &= ~0x80;
			playing.blendTime = 0;
		} else {
			playing.blendStart = currentTime;
			/* Primary0x4e339e preserves the frozen prior sample when another
			 * replacement arrives at the same blend clock, changing duration.
			 * A plain prior clip has cleared its sample and blendStart to0. */
			if ( prior->blendStart == currentTime ) {
				playing.previous = prior->previous;
				playing.previousKnown = !(prior->flags & 0x80) || prior->previousKnown;
			} else {
				Playhead sampled;
				if ( ForwardPlayhead(*prior, model.skeleton.numFrames, currentTime, sampled) ) {
					/* Primary0x497a80 weights the frozen current frame by its
					 * fraction and its successor by1-fraction, unlike the new
					 * clip. Retain this data contract in the Fn pose kernel. */
					playing.previous = {sampled.currentFrame, sampled.newFrame, 1-sampled.lerp};
					playing.previousKnown = true;
				}
			}
		}
	}

	for ( auto &existing : model.animations ) {
		if ( existing.bone == bone ) {
			existing = playing;
			detail::RecordClipFlags( model, bone, flags );
			return true;
		}
	}
	if ( int( model.animations.size() ) >= MaxBoneAnimations ) return false;
	model.animations.push_back( playing );
	detail::RecordClipFlags( model, bone, flags );
	return true;
}

inline const fntech3::ghoul2::BoneAnimation *Store::BoneAnimation( void *pointer,
	int modelIndex, const char *boneName )
{
	Instance *instance = Resolve( pointer );
	if ( !instance || modelIndex < 0 || modelIndex >= instance->Count() ) return nullptr;
	const Model &model = instance->models_[std::size_t( modelIndex )];
	if ( !model.present ) return nullptr;
	const int bone = detail::BoneNamed( model, boneName, nameMatch_ );
	if ( bone < 0 ) return nullptr;
	for ( const auto &existing : model.animations ) {
		if ( existing.bone == bone ) return &existing;
	}
	return nullptr;
}

inline BoneClipAnswer Store::BoneClipAt( void *pointer, int modelIndex,
	const char *boneName, int now, BoneClip &out )
{
	Instance *instance = Resolve( pointer );
	const Model *model = instance ? instance->At( modelIndex ) : nullptr;
	if ( !model ) return BoneClipAnswer::NoModel;
	const int bone = detail::BoneNamedFolded( *model, boneName );
	if ( bone < 0 ) return BoneClipAnswer::NoBone;
	/* 0x4e368d: a record whose word has neither 0x08 nor 0x10 runs no clip,
	 * whatever else the word carries. */
	const std::uint32_t word = detail::BoneFlagsWord( *model, bone );
	if ( !( word & 0x18u ) ) { out = BoneClip{}; return BoneClipAnswer::Inactive; }
	const fntech3::ghoul2::BoneAnimation *playing = nullptr;
	for ( const auto &existing : model->animations ) {
		if ( existing.bone == bone ) playing = &existing;
	}
	/* The word's animation bits must be this clip's own: not given through
	 * the angle setter, nor kept by a record whose 0x2000 refused the clip
	 * since. And the clip must be one the forward timing (0x497290) is
	 * served for: completing without loop or freeze, which retail's timing
	 * answers by clearing the word's animation bits, a reverse clip and
	 * other flag bits are not. */
	Playhead head;
	if ( !playing || ( word & BoneLockFlag )
		|| ( word & BoneAnimationBits ) != ( std::uint32_t( playing->flags ) & BoneAnimationBits )
		|| !ForwardPlayhead( *playing, model->skeleton.numFrames, now, head ) )
		return BoneClipAnswer::Unserved;
	/* 0x4e36af..0x4e36c8: the whole frame, then its fraction added. */
	out.frame = float( head.currentFrame ) + head.lerp;
	out.startFrame = playing->startFrame;
	out.endFrame = playing->endFrame;
	out.flags = word;
	out.speed = playing->speed;
	return BoneClipAnswer::Active;
}

inline std::uint32_t Store::BoneFlagsWord( void *pointer, int modelIndex, const char *boneName )
{
	Instance *instance = Resolve( pointer );
	const Model *model = instance ? instance->At( modelIndex ) : nullptr;
	const int bone = model ? detail::BoneNamedFolded( *model, boneName ) : -1;
	return bone < 0 ? 0u : detail::BoneFlagsWord( *model, bone );
}

inline bool Store::SetBoneAngles( void *pointer, int modelIndex, const char *boneName,
	const float *angles, int flags, int up, int right, int forward,
	int blendTime, int currentTime )
{
	Instance *instance = Resolve( pointer );
	/* Jedi Outcast's core sets the model's build time to 0 once its index
	 * checks out, before the bone is looked up (jk2mp.exe 0x4a7578), so the
	 * next query of an instance whose model 0 was bent builds again. */
	if ( keepBuilds_ && instance && modelIndex >= 0 && modelIndex < instance->Count() ) {
		instance->models_[std::size_t( modelIndex )].builtAt = 0;
	}
	if ( !instance || !angles ) return false;
	for ( int axis = 0; axis < 3; ++axis ) if ( !FiniteScalar(angles[axis]) ) return false;
	if ( modelIndex < 0 || modelIndex >= instance->Count() ) return false;
	Model &model = instance->models_[std::size_t( modelIndex )];
	if ( !model.present ) return false;
	const int bone = detail::BoneNamed( model, boneName, nameMatch_ );
	if ( bone < 0 ) return false;

	fntech3::ghoul2::BoneAngles bent;
	bent.bone = bone;
	for ( int axis = 0; axis < 3; ++axis ) bent.angles[axis] = angles[axis];
	bent.flags = flags;
	bent.up = up;
	bent.right = right;
	bent.forward = forward;
	bent.blendTime = blendTime;
	bent.startTime = currentTime;

	for ( auto &existing : model.angles ) {
		if ( existing.bone == bone ) {
			existing = bent;
			detail::RecordAngleFlags( model, bone, flags );
			return true;
		}
	}
	if ( int( model.angles.size() ) >= MaxBoneAnimations ) return false;
	model.angles.push_back( bent );
	detail::RecordAngleFlags( model, bone, flags );
	return true;
}

inline const fntech3::ghoul2::BoneAngles *Store::BoneAngles( void *pointer,
	int modelIndex, const char *boneName )
{
	Instance *instance = Resolve( pointer );
	if ( !instance || modelIndex < 0 || modelIndex >= instance->Count() ) return nullptr;
	const Model &model = instance->models_[std::size_t( modelIndex )];
	if ( !model.present ) return nullptr;
	const int bone = detail::BoneNamed( model, boneName, nameMatch_ );
	if ( bone < 0 ) return nullptr;
	for ( const auto &existing : model.angles ) {
		if ( existing.bone == bone ) return &existing;
	}
	return nullptr;
}

inline int Store::AddBolt( void *pointer, int modelIndex, const char *name )
{
	Instance *instance = Resolve( pointer );
	if ( !instance || !name || !name[0] ) return -1;
	if ( modelIndex < 0 || modelIndex >= instance->Count() ) return -1;
	Model &model = instance->models_[std::size_t( modelIndex )];
	if ( !model.present ) return -1;

	for ( int index = 0; index < int( model.bolts.size() ); ++index ) {
		if ( detail::NameEqual( model.bolts[std::size_t( index )].name, name, nameMatch_ ) ) return index;
	}
	if ( int( model.bolts.size() ) >= MaxBoltsPerModel ) return -1;

	Bolt bolt;
	bolt.name = name;
	/* A surface first, then a bone: the two are numbered separately, and a
	 * name that is neither is not a bolt. */
	for ( int index = 0; index < int( model.surfaceNames.size() ); ++index ) {
		if ( detail::NameEqual( model.surfaceNames[std::size_t( index )], name, nameMatch_ ) ) { bolt.surface = index; break; }
	}
	if ( bolt.surface < 0 ) {
		bolt.bone = detail::BoneNamed( model, name, nameMatch_ );
	}
	if ( bolt.surface < 0 && bolt.bone < 0 ) return -1;
	model.bolts.push_back( std::move( bolt ) );
	return int( model.bolts.size() ) - 1;
}

inline const Bolt *Store::BoltAt( void *pointer, int modelIndex, int boltIndex )
{
	Instance *instance = Resolve( pointer );
	if ( !instance || modelIndex < 0 || modelIndex >= instance->Count() ) return nullptr;
	const Model &model = instance->models_[std::size_t( modelIndex )];
	if ( !model.present || boltIndex < 0 || boltIndex >= int( model.bolts.size() ) ) {
		return nullptr;
	}
	return &model.bolts[std::size_t( boltIndex )];
}

inline bool Store::Attach( void *pointer, int entityNum )
{
	if ( !Resolve( pointer ) ) return false;
	if ( entityNum < 0 || entityNum >= MaxAttachedEntities ) return false;
	attached_[entityNum] = pointer;
	return true;
}

inline bool Store::Detach( int entityNum )
{
	return attached_.erase( entityNum ) != 0;
}

inline Instance *Store::Attached( int entityNum )
{
	const auto found = attached_.find( entityNum );
	return found == attached_.end() ? nullptr : Resolve( found->second );
}

inline bool Store::Release( void *pointer )
{
	Instance *instance = Resolve( pointer );
	if ( !instance ) return false;
	for ( auto it = attached_.begin(); it != attached_.end(); ) {
		it = it->second == pointer ? attached_.erase( it ) : std::next( it );
	}
	live_.erase( pointer );
	for ( auto it = instances_.begin(); it != instances_.end(); ++it ) {
		if ( it->get() == instance ) { instances_.erase( it ); return true; }
	}
	return false;
}

inline bool Store::Clean( void **slot )
{
	if ( !slot ) return false;
	void *handle = nullptr;
	std::memcpy( &handle, slot, sizeof( handle ) );
	if ( !handle ) return true;
	if ( !Release( handle ) ) return false;
	handle = nullptr;
	std::memcpy( slot, &handle, sizeof( handle ) );
	return true;
}

/* Jedi Outcast's clip setter, read from jk2mp.exe 1.02a. Import 592's core
 * (0x4a7440) holds an end frame outside 1..99999 at 1 and a start frame
 * outside 0..99999 at 0, and keeps a set frame only when it is -1, within
 * 0..100000 or not a number, else 0. It sets the model's build time to 0
 * once the model index checks out, before the bone is looked up, and then
 * reaches the bone's record through the setter (0x4ab390, 0x4ab070):
 *
 * - A transition (0x80), when the record runs a clip, samples it at the
 *   time given (outcast::Sample): forwards, the frame reached and the whole
 *   frame after it, moved inside that clip's range; backwards, its floor
 *   twice. A record already sampled at that time keeps its sample and takes
 *   the new duration. With no clip running the bit is dropped, the sample
 *   and duration zeroed; without the bit, the start time too.
 * - The new range and speed replace the old; the clip starts at the time
 *   given, or earlier by the set frame's distance from the start at fifty
 *   milliseconds a frame over the speed, truncated as _ftol truncates.
 * - The record's animation bits 0xd8 are replaced by the flags given
 *   (without 0x80 where dropped); any other bit given is added to it.
 *
 * Retail accepts any speed or set frame; what cannot be posed from them is
 * refused when a build poses it (outcast::Control). A new record here is
 * zeroed where retail's carries its stack's words, past its bone and
 * flags (0x4aaa0c). */
inline bool Store::SetOutcastBoneAnim( void *pointer, int modelIndex, const char *boneName,
	int startFrame, int endFrame, int flags, float speed, int currentTime,
	float setFrame, int blendTime )
{
	if ( endFrame <= 0 || endFrame >= 100000 ) endFrame = 1;
	if ( startFrame < 0 || startFrame >= 100000 ) startFrame = 0;
	std::uint32_t setBits;
	std::memcpy( &setBits, &setFrame, sizeof( setBits ) );
	const bool setNaN = ( setBits & 0x7f800000u ) == 0x7f800000u && ( setBits & 0x7fffffu ) != 0u;
	if ( !setNaN && setFrame != -1.0f && !( setFrame >= 0.0f && setFrame <= 100000.0f ) ) setFrame = 0.0f;

	Instance *instance = Resolve( pointer );
	if ( !instance || modelIndex < 0 || modelIndex >= instance->Count() ) return false;
	Model &model = instance->models_[std::size_t( modelIndex )];
	model.builtAt = 0;
	if ( !model.present ) return false;
	const int bone = detail::BoneNamed( model, boneName );
	if ( bone < 0 ) return false;
	fntech3::ghoul2::BoneAnimation *record = nullptr;
	for ( auto &existing : model.animations ) {
		if ( existing.bone == bone ) record = &existing;
	}
	if ( !record && int( model.animations.size() ) >= MaxBoneAnimations ) return false;
	fntech3::ghoul2::BoneAnimation clip = record ? *record : fntech3::ghoul2::BoneAnimation{};
	clip.bone = bone;

	int given = flags;
	float reached = 0.0f;
	if ( ( flags & outcast::BlendFlag ) && outcast::Sample( clip, currentTime, reached ) ) {
		if ( clip.blendStart != currentTime ) {
			if ( clip.speed < 0.0f ) {
				clip.blendFrame = float( std::floor( double( reached ) ) );
				clip.blendLerpFrame = outcast::Truncate( std::floor( double( reached ) ) );
			} else {
				clip.blendFrame = reached;
				clip.blendLerpFrame = outcast::Truncate( double( reached ) + 1.0 );
				const bool looped = ( clip.flags & outcast::LoopFlag ) != 0;
				if ( !( double( clip.blendFrame ) < double( clip.endFrame ) ) ) {
					if ( looped ) clip.blendFrame = float( clip.startFrame );
					else if ( clip.endFrame <= 0 ) clip.blendLerpFrame = 0;
					else clip.blendFrame = float( clip.endFrame - 1 );
				}
				if ( clip.blendLerpFrame >= clip.endFrame ) {
					if ( looped ) clip.blendLerpFrame = clip.startFrame;
					else clip.blendLerpFrame = clip.endFrame <= 0 ? 0 : clip.endFrame - 1;
				}
			}
			clip.blendStart = currentTime;
		}
		clip.blendTime = blendTime;
	} else {
		if ( !( flags & outcast::BlendFlag ) ) clip.blendStart = 0;
		clip.blendFrame = 0.0f;
		clip.blendLerpFrame = 0;
		clip.blendTime = 0;
		given &= ~outcast::BlendFlag;
	}

	clip.startFrame = startFrame;
	clip.endFrame = endFrame;
	clip.speed = speed;
	clip.setFrame = setFrame;
	if ( !setNaN && setFrame == -1.0f ) clip.startTime = currentTime;
	else if ( speed == 0.0f ) clip.startTime = 0;
	else clip.startTime = outcast::Truncate( double( currentTime )
		- ( double( setFrame ) - double( startFrame ) ) * 50.0 / double( speed ) );
	clip.flags = ( clip.flags & ~outcast::AnimationBits ) | given;
	if ( record ) *record = clip;
	else model.animations.push_back( clip );
	return true;
}

/* Jedi Outcast's duplicate (jk2mp.exe 1.02a 0x4a7ce0, handler 0x44c857): a
 * field already holding an instance is left as it is and nothing is done.
 * Otherwise a new instance goes into the field, which is listed as 588
 * lists one, and the instance copy (0x4a79e0, every model) fills it with
 * the source's records whole: removed models, links, bolts with their kept
 * matrices, clips, bends and build times. Retail makes the new instance
 * before it reads the source; a source this store does not hold is refused
 * here and the field left empty. */
inline bool Store::DuplicateInto( void *pointer, void *field, std::size_t fieldBytes )
{
	if ( !field || ReadField( field, fieldBytes ) ) return false;
	Instance *source = Resolve( pointer );
	if ( !source ) return false;
	void *handle = ModelHandleIdentities.Allocate();
	if ( !handle || !FieldHolds( handle, fieldBytes ) ) return false;
	instances_.push_back( std::make_unique<Instance>() );
	Instance *copy = instances_.back().get();
	copy->handle_ = handle;
	live_.emplace( handle, copy );
	copy->models_ = source->models_;
	WriteField( field, fieldBytes, handle );
	fields_.push_back( field );
	return true;
}

/* Jedi Outcast's clean (jk2mp.exe 1.02a 0x4a6e00, handler 0x44c8aa): a field
 * holding nothing is left alone. Otherwise the field is emptied, and the
 * instance it held is freed only if the field is listed (0x1ae664c, then
 * 0x1ae6638), its listing going with it; an unlisted field holding another
 * field's instance leaves that instance alive. Retail deletes whatever a
 * listed field points at; an instance this store does not hold is not
 * freed. */
inline void Store::CleanField( void *field, std::size_t fieldBytes )
{
	if ( !field ) return;
	void *handle = ReadField( field, fieldBytes );
	if ( !handle ) return;
	const auto listed = std::find( fields_.begin(), fields_.end(), static_cast<const void *>( field ) );
	if ( listed != fields_.end() ) {
		fields_.erase( listed );
		Release( handle );
	}
	WriteField( field, fieldBytes, nullptr );
}

/* Jedi Outcast's GLA name (jk2mp.exe 1.02a 0x4a7d80): for an instance and a
 * model index below its count, the skeleton name its mesh header carries
 * (mdxm +0x48). Retail answers for a removed model's record too; this does
 * not. */
inline const char *Store::GLAName( void *pointer, int modelIndex )
{
	Instance *instance = Resolve( pointer );
	const Model *model = instance ? instance->At( modelIndex ) : nullptr;
	return model ? model->animName.c_str() : nullptr;
}

/* Skins, as a server knows them.
 *
 * Raven's own engine asks its renderer for a *server* skin, separately from
 * the one a client registers, precisely because a server needs the handle and
 * not the shaders: the game hands that handle back when it sets a skin on a
 * model. So this reads the file and remembers which surfaces it names, and
 * nothing about how any of them are drawn.
 *
 * A skin that is not there registers as no skin, which is handle zero. That is
 * what this lineage has always meant by zero, and it is what the game checks. */

inline constexpr int MaxSkinSurfaces = 256;

struct Skin {
	std::string name;
	/* Each surface the file names, in the order it names them. */
	std::vector<std::string> surfaces;
};

class SkinRegistry {
public:
	explicit SkinRegistry( Files &files ) : files_( &files ) {}

	void Clear() { skins_.clear(); }
	int Count() const { return int( skins_.size() ); }

	/* Handles are one-based, so zero stays "no skin". */
	const Skin *At( int handle ) const {
		return handle >= 1 && handle <= Count() ? &skins_[std::size_t( handle - 1 )] : nullptr;
	}

	/* Register a skin by name, returning its handle or 0. A name registered
	 * twice keeps its first handle: the game may ask for the same skin on
	 * every one of a level's entities. */
	int Register( const char *name );

private:
	Files *files_ = nullptr;
	std::vector<Skin> skins_;
};

inline int SkinRegistry::Register( const char *name )
{
	if ( !name || !name[0] ) return 0;
	for ( int handle = 1; handle <= Count(); ++handle ) {
		if ( skins_[std::size_t( handle - 1 )].name == name ) return handle;
	}

	const std::size_t size = files_->Read( name, nullptr, 0 );
	if ( size == 0 || size > MaxModelBytes ) return 0;
	std::vector<char> text( size + 1, 0 );
	if ( files_->Read( name, text.data(), size ) != size ) return 0;
	text[size] = 0;

	Skin skin;
	skin.name = name;
	/* Each line names a surface and what to draw it with, separated by a
	 * comma. A line with no comma names nothing, and a line beginning with two
	 * slashes is a remark. */
	const char *at = text.data();
	while ( *at && int( skin.surfaces.size() ) < MaxSkinSurfaces ) {
		const char *lineEnd = at;
		while ( *lineEnd && *lineEnd != '\n' && *lineEnd != '\r' ) ++lineEnd;
		std::string line( at, std::size_t( lineEnd - at ) );
		at = *lineEnd ? lineEnd + 1 : lineEnd;
		const std::size_t remark = line.find( "//" );
		if ( remark != std::string::npos ) line.erase( remark );
		const std::size_t comma = line.find( ',' );
		if ( comma == std::string::npos ) continue;
		std::string surface = line.substr( 0, comma );
		while ( !surface.empty() && ( surface.back() == ' ' || surface.back() == '\t' ) ) {
			surface.pop_back();
		}
		std::size_t begin = 0;
		while ( begin < surface.size() && ( surface[begin] == ' ' || surface[begin] == '\t' ) ) {
			++begin;
		}
		surface.erase( 0, begin );
		if ( !surface.empty() ) skin.surfaces.push_back( surface );
	}
	/* A file that names no surface at all is not a skin. */
	if ( skin.surfaces.empty() ) return 0;
	skins_.push_back( std::move( skin ) );
	return Count();
}

} // namespace ghoul2
} // namespace fntech3

#endif
