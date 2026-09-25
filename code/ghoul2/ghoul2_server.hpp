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
 * The module keeps a pointer to an instance in its own entity record. That
 * pointer is never trusted on the way back: an instance is owned here, kept in
 * a table, and a pointer that is not in that table is refused rather than
 * followed. A module that hands back a stale or forged pointer gets a refusal,
 * not a jump into whatever it points at.
 *
 * See docs/fntech3/2026-09-22-raven-ghoul2-server.md. */

#include "../compat/jka_model_format.hpp"

#include <cmath>
#include <cstring>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

/* A named place on a model that the game can ask the engine about later: a
 * surface of the mesh, or a bone of the skeleton. Which of the two it is
 * matters, because they are numbered separately. */
struct Bolt {
	std::string name;
	int surface = -1;
	int bone = -1;
};

/* What the game set one bone playing: the range of frames, how fast, how it
 * blends, and when it started. The engine holds this and does not interpret
 * it -- working out which frame a bone is on at a given moment is arithmetic
 * that has not been measured, and this does not pretend to know it. */
struct BoneAnimation {
	int bone = -1;
	int startFrame = 0;
	int endFrame = 0;
	int flags = 0;
	float speed = 0.0f;
	int startTime = 0;
	int setFrame = 0;
	int blendTime = 0;
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

struct Playhead {
	int currentFrame = 0;
	int newFrame = 0;
	float lerp = 0.0f;
};

inline Playhead PlayheadAt( const BoneAnimation &playing, int numFrames, int now )
{
	Playhead out;
	if ( numFrames <= 0 ) return out;

	float elapsed = float( now - playing.startTime ) / FrameMilliseconds;
	if ( elapsed < 0.0f ) elapsed = 0.0f;
	float frame = float( playing.startFrame ) + elapsed * playing.speed;

	/* The range the game gave, kept the right way round and inside the file.
	 * A range this engine cannot use is held at the first frame rather than
	 * indexed with, because an out-of-range frame is the game's mistake and
	 * not something to pass on to a reader. */
	int first = playing.startFrame;
	int last = playing.endFrame;
	if ( first < 0 ) first = 0;
	if ( last > numFrames ) last = numFrames;
	if ( last <= first ) { out.currentFrame = out.newFrame = first; return out; }

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

struct Model {
	std::string name;           /* the mesh, as the module named it */
	std::string skeletonName;   /* the `.gla` the mesh names */
	model::Mesh mesh;
	model::Skeleton skeleton;
	/* The two files themselves, owned by the store and shared between every
	 * model that names them, so that a bolt matrix can reach the vertices and
	 * the poses. Null for a mesh that carries its own bones and names no
	 * skeleton file. */
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
	bool present = true;        /* false once the module removes it */
	/* The skin the game put on this model, as a handle from the registry
	 * below, or zero for none. A server keeps the handle and nothing else. */
	int customSkin = 0;
	/* The bolt the game last told this model to use. The engine keeps the
	 * number; what it selects is the game's business. */
	int boltInfo = 0;
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
};

class Store {
public:
	explicit Store( Files &files ) : files_( &files ) {}

	void Clear() { instances_.clear(); live_.clear(); attached_.clear(); }
	std::size_t InstanceCount() const { return instances_.size(); }

	/* An instance this store owns, or null. Never follow a module's pointer
	 * without asking this first. */
	Instance *Resolve( void *pointer ) {
		if ( !pointer || !live_.count( pointer ) ) return nullptr;
		return static_cast<Instance *>( pointer );
	}

	/* Add a model to the instance at `slot`, making one if there is none.
	 * Returns the model's index within the instance, or -1 with `error` set.
	 * `slot` is the module's own pointer field, which is written only when a
	 * model actually loads. */
	int AddModel( void **slot, const char *name, LoadError &error );

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
	/* Where one of a model's bolts is, in the model's own space, at one
	 * moment. False if the model cannot answer -- no skeleton kept, a bolt
	 * that names nothing, a surface this engine cannot reach -- and in that
	 * case nothing is written, because a bolt matrix that is not the answer
	 * is worse than none. */
	bool BoltMatrix( void *pointer, int modelIndex, int boltIndex, int now,
		float out[12] );

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
		int setFrame, int blendTime );
	/* What a bone was set playing, or null. */
	const struct BoneAnimation *BoneAnimation( void *pointer, int modelIndex,
		const char *boneName );

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

private:
	bool ReadFile( const std::string &path, std::vector<unsigned char> &into );
	LoadError Load( const char *name, Model &out );

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
	std::vector<std::unique_ptr<Instance>> instances_;
	std::unordered_set<void *> live_;
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

	/* A name beginning with a star is not a file. The mesh carries its own
	 * bones and there is nothing to open, so opening it would be the mistake.
	 *
	 * Measured, not assumed: across both retail installations `*default` is
	 * the only such name there is -- 41 of Jedi Academy's 127 meshes and 23 of
	 * Jedi Outcast's 71 carry it, including every lightsaber -- and every name
	 * that does not begin with a star resolves to a `.gla` that is present, in
	 * both. So the star is the rule and not an exception to chase. */
	if ( named[0] == '*' ) {
		out.skeletonName = named;
		out.skeleton = model::Skeleton{};
		out.skeleton.numBones = out.mesh.numBones;
		return model::ParseMesh( mesh.data(), mesh.size(), out.mesh.numBones, out.mesh )
			== model::Error::None ? LoadError::None : LoadError::Bones;
	}

	out.skeletonName = std::string( named ) + ".gla";

	std::vector<unsigned char> skeleton;
	if ( !ReadFile( out.skeletonName, skeleton ) ) return LoadError::SkeletonFile;
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

inline int Store::AddModel( void **slot, const char *name, LoadError &error )
{
	error = LoadError::None;
	Instance *instance = slot ? Resolve( *slot ) : nullptr;

	Model loaded;
	error = Load( name, loaded );
	if ( error != LoadError::None ) return -1;

	if ( instance && instance->Count() >= MaxModelsPerInstance ) {
		error = LoadError::Full;
		return -1;
	}

	if ( !instance ) {
		instances_.push_back( std::make_unique<Instance>() );
		instance = instances_.back().get();
		live_.insert( instance );
	}
	instance->models_.push_back( std::move( loaded ) );
	if ( slot ) *slot = instance;
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
		return true;
	}
	destination->models_[std::size_t( modelTo )] = model;
	return true;
}

/* Every bone of a model, posed for one moment.
 *
 * A bone the game has set playing is posed at the frame its playhead has
 * reached, blended towards the next by the fraction between them, element by
 * element across the whole matrix -- which is what the retail engine does, and
 * not a quaternion slerp. A bone the game has never touched stays where it was
 * modelled. */
inline bool Store::BoneMatrices( const Model &model, int now,
	std::vector<float> &out ) const
{
	out.clear();
	if ( !model.skeletonBytes || model.skeleton.numBones <= 0 ) return false;
	const void *data = model.skeletonBytes->data();
	const std::size_t size = model.skeletonBytes->size();
	out.assign( std::size_t( model.skeleton.numBones ) * 12, 0.0f );

	for ( int bone = 0; bone < model.skeleton.numBones; ++bone ) {
		float *slot = &out[std::size_t( bone ) * 12];
		const fntech3::ghoul2::BoneAnimation *playing = nullptr;
		for ( const fntech3::ghoul2::BoneAnimation &entry : model.animations ) {
			if ( entry.bone == bone ) { playing = &entry; break; }
		}

		float current[12];
		if ( !playing ) {
			if ( !model::BoneRestMatrix( data, size, model.skeleton, bone, slot ) ) {
				return false;
			}
			continue;
		}
		const Playhead at = PlayheadAt( *playing, model.skeleton.numFrames, now );
		if ( !model::BoneMatrix( data, size, model.skeleton, at.currentFrame,
			bone, current ) ) {
			if ( !model::BoneRestMatrix( data, size, model.skeleton, bone, slot ) ) {
				return false;
			}
			continue;
		}
		float next[12];
		if ( at.lerp <= 0.0f || at.newFrame == at.currentFrame
			|| !model::BoneMatrix( data, size, model.skeleton, at.newFrame,
				bone, next ) ) {
			for ( int i = 0; i < 12; ++i ) slot[i] = current[i];
			continue;
		}
		const float toward = at.lerp;
		for ( int i = 0; i < 12; ++i ) {
			slot[i] = current[i] * ( 1.0f - toward ) + next[i] * toward;
		}
	}
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
	const float length = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	if ( !( length > 1e-6f ) ) return false;
	v[0] /= length; v[1] /= length; v[2] /= length;
	return true;
}

}  /* namespace detail */

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
 * them: its element 0, element 4 and element 8 are one axis. */
inline bool Store::BoltMatrix( void *pointer, int modelIndex, int boltIndex,
	int now, float out[12] )
{
	if ( !out ) return false;
	Instance *instance = Resolve( pointer );
	if ( !instance ) return false;
	const Model *model = instance->At( modelIndex );
	if ( !model ) return false;
	if ( boltIndex < 0 || boltIndex >= int( model->bolts.size() ) ) return false;
	const Bolt &bolt = model->bolts[std::size_t( boltIndex )];

	std::vector<float> bones;
	if ( !BoneMatrices( *model, now, bones ) ) return false;

	if ( bolt.bone >= 0 ) {
		if ( bolt.bone >= model->skeleton.numBones ) return false;
		const float *found = &bones[std::size_t( bolt.bone ) * 12];
		for ( int i = 0; i < 12; ++i ) out[i] = found[i];
		return true;
	}
	if ( bolt.surface < 0 || !model->meshBytes ) return false;

	const void *mesh = model->meshBytes->data();
	const std::size_t meshSize = model->meshBytes->size();
	model::MeshSurface surface;
	if ( !model::SurfaceAt( mesh, meshSize, model->mesh, 0, bolt.surface, surface ) ) {
		return false;
	}
	int corner[3];
	if ( !model::TriangleCorners( mesh, meshSize, surface, 0, corner ) ) return false;

	/* A vertex is modelled where it belongs in the whole model, not relative
	 * to the bone that carries it, so moving it means undoing where that bone
	 * rests before applying where it is now. With nothing animated the two
	 * cancel and the vertex stays exactly where it was modelled, which is the
	 * property this was checked against. */
	const void *skeletonData = model->skeletonBytes->data();
	const std::size_t skeletonSize = model->skeletonBytes->size();
	float point[3][3];
	for ( int which = 0; which < 3; ++which ) {
		model::SkinnedVertex vertex;
		if ( !model::VertexAt( mesh, meshSize, surface, corner[which], vertex ) ) {
			return false;
		}
		float at[3] = { 0.0f, 0.0f, 0.0f };
		for ( int weight = 0; weight < vertex.count; ++weight ) {
			const int bone = vertex.bone[weight];
			if ( bone < 0 || bone >= model->skeleton.numBones ) return false;
			float undo[12], carry[12];
			if ( !model::BoneRestMatrixInverse( skeletonData, skeletonSize,
				model->skeleton, bone, undo ) ) return false;
			model::ComposeMatrix( &bones[std::size_t( bone ) * 12], undo, carry );
			for ( int axis = 0; axis < 3; ++axis ) {
				at[axis] += vertex.weight[weight]
					* ( carry[axis * 4 + 0] * vertex.position[0]
						+ carry[axis * 4 + 1] * vertex.position[1]
						+ carry[axis * 4 + 2] * vertex.position[2]
						+ carry[axis * 4 + 3] );
			}
		}
		for ( int axis = 0; axis < 3; ++axis ) point[which][axis] = at[axis];
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
	instances_.push_back( std::make_unique<Instance>() );
	Instance *copy = instances_.back().get();
	live_.insert( copy );
	copy->models_ = source->models_;
	*slot = copy;
	return true;
}

namespace detail {
/* Which bone of a model carries a name, or -1. */
inline int BoneNamed( const Model &model, const char *name )
{
	if ( !name || !name[0] ) return -1;
	for ( int index = 0; index < int( model.boneNames.size() ); ++index ) {
		if ( model.boneNames[std::size_t( index )] == name ) return index;
	}
	return -1;
}
} // namespace detail

inline bool Store::SetBoneAnim( void *pointer, int modelIndex, const char *boneName,
	int startFrame, int endFrame, int flags, float speed, int currentTime,
	int setFrame, int blendTime )
{
	Instance *instance = Resolve( pointer );
	if ( !instance || modelIndex < 0 || modelIndex >= instance->Count() ) return false;
	Model &model = instance->models_[std::size_t( modelIndex )];
	if ( !model.present ) return false;
	const int bone = detail::BoneNamed( model, boneName );
	if ( bone < 0 ) return false;

	fntech3::ghoul2::BoneAnimation playing;
	playing.bone = bone;
	playing.startFrame = startFrame;
	playing.endFrame = endFrame;
	playing.flags = flags;
	playing.speed = speed;
	playing.startTime = currentTime;
	playing.setFrame = setFrame;
	playing.blendTime = blendTime;

	for ( auto &existing : model.animations ) {
		if ( existing.bone == bone ) { existing = playing; return true; }
	}
	if ( int( model.animations.size() ) >= MaxBoneAnimations ) return false;
	model.animations.push_back( playing );
	return true;
}

inline const fntech3::ghoul2::BoneAnimation *Store::BoneAnimation( void *pointer,
	int modelIndex, const char *boneName )
{
	Instance *instance = Resolve( pointer );
	if ( !instance || modelIndex < 0 || modelIndex >= instance->Count() ) return nullptr;
	const Model &model = instance->models_[std::size_t( modelIndex )];
	if ( !model.present ) return nullptr;
	const int bone = detail::BoneNamed( model, boneName );
	if ( bone < 0 ) return nullptr;
	for ( const auto &existing : model.animations ) {
		if ( existing.bone == bone ) return &existing;
	}
	return nullptr;
}

inline bool Store::SetBoneAngles( void *pointer, int modelIndex, const char *boneName,
	const float *angles, int flags, int up, int right, int forward,
	int blendTime, int currentTime )
{
	Instance *instance = Resolve( pointer );
	if ( !instance || !angles ) return false;
	if ( modelIndex < 0 || modelIndex >= instance->Count() ) return false;
	Model &model = instance->models_[std::size_t( modelIndex )];
	if ( !model.present ) return false;
	const int bone = detail::BoneNamed( model, boneName );
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
		if ( existing.bone == bone ) { existing = bent; return true; }
	}
	if ( int( model.angles.size() ) >= MaxBoneAnimations ) return false;
	model.angles.push_back( bent );
	return true;
}

inline const fntech3::ghoul2::BoneAngles *Store::BoneAngles( void *pointer,
	int modelIndex, const char *boneName )
{
	Instance *instance = Resolve( pointer );
	if ( !instance || modelIndex < 0 || modelIndex >= instance->Count() ) return nullptr;
	const Model &model = instance->models_[std::size_t( modelIndex )];
	if ( !model.present ) return nullptr;
	const int bone = detail::BoneNamed( model, boneName );
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
		if ( model.bolts[std::size_t( index )].name == name ) return index;
	}
	if ( int( model.bolts.size() ) >= MaxBoltsPerModel ) return -1;

	Bolt bolt;
	bolt.name = name;
	/* A surface first, then a bone: the two are numbered separately, and a
	 * name that is neither is not a bolt. */
	for ( int index = 0; index < int( model.surfaceNames.size() ); ++index ) {
		if ( model.surfaceNames[std::size_t( index )] == name ) { bolt.surface = index; break; }
	}
	if ( bolt.surface < 0 ) {
		for ( int index = 0; index < int( model.boneNames.size() ); ++index ) {
			if ( model.boneNames[std::size_t( index )] == name ) { bolt.bone = index; break; }
		}
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
