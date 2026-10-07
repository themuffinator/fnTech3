/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* The models a Raven server holds for its game module. Every mesh and skeleton
 * here is written by this test; no retail file is used, and nothing is posed,
 * skinned or drawn. What is checked is that a model only loads when it really
 * reads, and that a pointer handed back by a module is never followed unless
 * this store gave it out. */

#include "../code/ghoul2/ghoul2_server.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

namespace ghoul2 = fntech3::ghoul2;
namespace model = fntech3::jka::model;
using ghoul2::LoadError;
using Bytes = std::vector<std::uint8_t>;

static void AppendInt( Bytes &file, std::int32_t value )
{
	for ( int i = 0; i < 4; ++i ) file.push_back( std::uint8_t( std::uint32_t( value ) >> ( 8 * i ) ) );
}

static void AppendFloat( Bytes &file, float value )
{
	std::uint32_t word;
	std::memcpy( &word, &value, sizeof( word ) );
	AppendInt( file, std::int32_t( word ) );
}

static void PutInt( Bytes &file, std::size_t at, std::int32_t value )
{
	for ( int i = 0; i < 4; ++i ) file[at + i] = std::uint8_t( std::uint32_t( value ) >> ( 8 * i ) );
}

static void AppendName( Bytes &file, const std::string &name )
{
	CHECK( name.size() < model::NameBytes );
	file.insert( file.end(), name.begin(), name.end() );
	file.insert( file.end(), model::NameBytes - name.size(), 0 );
}

/* One surface: its header, then bone references, triangles, vertices and their
 * texture coordinates, in the order Raven writes them. */
static void AppendSurface( Bytes &file, int verts, int triangles, int boneReferences )
{
	const std::size_t start = file.size();
	const std::size_t boneRefs = model::SurfaceBytes;
	const std::size_t triangleAt = boneRefs + std::size_t( boneReferences ) * 4;
	const std::size_t vertAt = triangleAt + std::size_t( triangles ) * model::TriangleBytes;
	const std::size_t texAt = vertAt + std::size_t( verts ) * model::VertexBytes;
	const std::size_t end = texAt + std::size_t( verts ) * model::TexCoordBytes;

	AppendInt( file, 0 );
	AppendInt( file, 0 );
	AppendInt( file, -std::int32_t( start ) );
	AppendInt( file, verts );
	AppendInt( file, std::int32_t( vertAt ) );
	AppendInt( file, triangles );
	AppendInt( file, std::int32_t( triangleAt ) );
	AppendInt( file, boneReferences );
	AppendInt( file, std::int32_t( boneRefs ) );
	AppendInt( file, std::int32_t( end ) );

	for ( int i = 0; i < boneReferences; ++i ) AppendInt( file, i );
	for ( int i = 0; i < triangles; ++i ) {
		for ( int corner = 0; corner < 3; ++corner ) AppendInt( file, ( i + corner ) % verts );
	}
	for ( int i = 0; i < verts; ++i ) {
		AppendFloat( file, 0 ); AppendFloat( file, 0 ); AppendFloat( file, 1 );
		AppendFloat( file, float( i ) ); AppendFloat( file, 0 ); AppendFloat( file, 0 );
		/* One weight, referencing the first bone. */
		AppendInt( file, std::int32_t( 0u ) );
		for ( int weight = 0; weight < model::MaxWeightsPerVertex; ++weight ) file.push_back( 0 );
	}
	for ( int i = 0; i < verts; ++i ) { AppendFloat( file, 0.25f ); AppendFloat( file, 0.5f ); }
}

/* A mesh of one surface at one level of detail, naming a skeleton. */
static Bytes Mesh( const std::string &skeleton, int bones )
{
	Bytes file;
	AppendInt( file, std::int32_t( model::MeshIdent ) );
	AppendInt( file, model::MeshVersion );
	AppendName( file, "models/players/test/model.glm" );
	AppendName( file, skeleton );
	AppendInt( file, 0 );                 // animIndex
	AppendInt( file, bones );
	AppendInt( file, 1 );                 // numLODs
	const std::size_t lodField = file.size();
	AppendInt( file, 0 );
	AppendInt( file, 1 );                 // numSurfaces
	AppendInt( file, std::int32_t( model::MeshHeaderBytes + 4 ) );
	const std::size_t endField = file.size();
	AppendInt( file, 0 );
	CHECK( file.size() == model::MeshHeaderBytes );

	const std::size_t hierarchy = file.size();
	AppendInt( file, 0 );
	PutInt( file, hierarchy, std::int32_t( file.size() - model::MeshHeaderBytes ) );
	AppendName( file, "root" );
	AppendInt( file, 0 );                 // flags
	AppendName( file, "models/players/test/skin" );
	AppendInt( file, 0 );                 // shaderIndex
	AppendInt( file, -1 );                // parentIndex
	AppendInt( file, 0 );                 // numChildren

	PutInt( file, lodField, std::int32_t( file.size() ) );
	const std::size_t lod = file.size();
	AppendInt( file, 0 );
	const std::size_t table = file.size();
	AppendInt( file, 0 );
	PutInt( file, table, std::int32_t( file.size() - table ) );
	AppendSurface( file, 3, 1, 1 );
	PutInt( file, lod, std::int32_t( file.size() - lod ) );
	PutInt( file, endField, std::int32_t( file.size() ) );
	return file;
}

/* A skeleton of `bones` bones and one frame. */
static Bytes Skeleton( const std::string &name, int bones )
{
	Bytes file;
	AppendInt( file, std::int32_t( model::SkeletonIdent ) );
	AppendInt( file, model::SkeletonVersion );
	AppendName( file, name );
	AppendFloat( file, 1.0f );
	AppendInt( file, 1 );                 // numFrames
	const std::size_t framesField = file.size();
	AppendInt( file, 0 );
	AppendInt( file, bones );
	const std::size_t poolField = file.size();
	AppendInt( file, 0 );
	AppendInt( file, std::int32_t( model::SkeletonHeaderBytes + bones * 4 ) );
	const std::size_t endField = file.size();
	AppendInt( file, 0 );
	CHECK( file.size() == model::SkeletonHeaderBytes );

	const std::size_t table = file.size();
	for ( int bone = 0; bone < bones; ++bone ) AppendInt( file, 0 );
	for ( int bone = 0; bone < bones; ++bone ) {
		PutInt( file, table + std::size_t( bone ) * 4,
			std::int32_t( file.size() - model::SkeletonHeaderBytes ) );
		AppendName( file, bone ? "child" : "model_root" );
		AppendInt( file, 0 );
		AppendInt( file, bone - 1 );
		for ( int matrix = 0; matrix < 24; ++matrix ) AppendFloat( file, matrix % 5 ? 0.0f : 1.0f );
		const int children = bone + 1 < bones ? 1 : 0;
		AppendInt( file, children );
		if ( children ) AppendInt( file, bone + 1 );
	}

	PutInt( file, framesField, std::int32_t( file.size() ) );
	for ( int bone = 0; bone < bones; ++bone ) {
		file.push_back( 0 ); file.push_back( 0 ); file.push_back( 0 );
	}
	while ( file.size() % 4 ) file.push_back( 0 );
	PutInt( file, poolField, std::int32_t( file.size() ) );
	for ( std::size_t i = 0; i < model::CompressedBoneBytes; ++i ) {
		file.push_back( std::uint8_t( i ) );
	}
	PutInt( file, endField, std::int32_t( file.size() ) );
	return file;
}

class TestFiles final : public ghoul2::Files {
public:
	std::map<std::string, Bytes> files;
	int reads = 0;
	/* Every path the store asked for, so a test can say that something was
	 * never opened as well as that it was. */
	std::set<std::string> asked;

	std::size_t Read( const char *path, void *destination, std::size_t capacity ) override
	{
		++reads;
		asked.insert( path ? path : "" );
		const auto found = files.find( path );
		if ( found == files.end() ) return 0;
		if ( !destination || capacity < found->second.size() ) return found->second.size();
		std::memcpy( destination, found->second.data(), found->second.size() );
		return found->second.size();
	}
};

static TestFiles Installed()
{
	TestFiles files;
	files.files["models/players/test/model.glm"] = Mesh( "models/players/_humanoid/_humanoid", 3 );
	files.files["models/players/_humanoid/_humanoid.gla"] =
		Skeleton( "models/players/_humanoid/_humanoid", 3 );
	return files;
}

static void CheckAModelLoads()
{
	TestFiles files = Installed();
	ghoul2::Store store( files );

	void *slot = nullptr;
	LoadError error = LoadError::None;
	const int index = store.AddModel( &slot, "models/players/test/model.glm", error );
	CHECK( index == 0 && error == LoadError::None );
	CHECK( slot != nullptr && store.InstanceCount() == 1 );

	const ghoul2::Instance *instance = store.Resolve( slot );
	CHECK( instance && instance->Count() == 1 );
	const ghoul2::Model *loaded = instance->At( 0 );
	CHECK( loaded && loaded->name == "models/players/test/model.glm" );
	CHECK( loaded->skeletonName == "models/players/_humanoid/_humanoid.gla" );
	CHECK( loaded->mesh.numBones == 3 && loaded->skeleton.numBones == 3 );
	CHECK( loaded->mesh.numSurfaces == 1 && loaded->mesh.numLODs == 1 );
	CHECK( instance->At( 1 ) == nullptr && instance->At( -1 ) == nullptr );

	/* A second model joins the same instance rather than making another. */
	void *same = slot;
	CHECK( store.AddModel( &same, "models/players/test/model.glm", error ) == 1 );
	CHECK( same == slot && store.InstanceCount() == 1 );
	CHECK( store.Resolve( slot )->Count() == 2 );
}

/* A skin goes on a model by handle. An instance this store never gave out, or
 * a model an instance does not have, is refused rather than written through. */
static void CheckSkinsGoOnModels()
{
	TestFiles files = Installed();
	ghoul2::Store store( files );
	void *slot = nullptr;
	LoadError error = LoadError::None;
	CHECK( store.AddModel( &slot, "models/players/test/model.glm", error ) == 0 );
	CHECK( store.Resolve( slot )->At( 0 )->customSkin == 0 );

	CHECK( store.SetSkin( slot, 0, 7 ) );
	CHECK( store.Resolve( slot )->At( 0 )->customSkin == 7 );

	/* An instance that holds a model says so; the question is whether any
	 * model is still there, which is what both games' engines ask. */
	CHECK( store.Resolve( slot )->HoldsAny() );

	/* An instance can be marked the server's own, and a copy is not. */
	CHECK( !store.Resolve( slot )->ServerOnly() );
	CHECK( store.OverrideServer( slot ) && store.Resolve( slot )->ServerOnly() );
	int notOurs = 0;
	CHECK( !store.OverrideServer( nullptr ) && !store.OverrideServer( &notOurs ) );

	/* Which bolt a model uses is kept the same way and is its own value. */
	CHECK( store.Resolve( slot )->At( 0 )->boltInfo == ghoul2::link::None );
	CHECK( store.SetBoltInfo( slot, 0, 0x1234 ) );
	CHECK( store.Resolve( slot )->At( 0 )->boltInfo == 0x1234 );
	CHECK( store.Resolve( slot )->At( 0 )->customSkin == 7 );   /* untouched */
	int elsewhereBolt = 0;
	CHECK( !store.SetBoltInfo( slot, 1, 1 ) && !store.SetBoltInfo( nullptr, 0, 1 ) );
	CHECK( !store.SetBoltInfo( &elsewhereBolt, 0, 1 ) );
	CHECK( store.Resolve( slot )->At( 0 )->boltInfo == 0x1234 );
	CHECK( store.SetSkin( slot, 0, 0 ) );                 /* back to no skin */
	CHECK( store.Resolve( slot )->At( 0 )->customSkin == 0 );

	CHECK( !store.SetSkin( slot, 1, 7 ) );                /* no such model */
	CHECK( !store.SetSkin( slot, -1, 7 ) );
	CHECK( !store.SetSkin( slot, 0, -1 ) );               /* no such handle */
	CHECK( !store.SetSkin( nullptr, 0, 7 ) );
	int elsewhere = 0;
	CHECK( !store.SetSkin( &elsewhere, 0, 7 ) );          /* not ours */
}

/* An instance can be copied, and the copy is its own. */
static void CheckAnInstanceCopies()
{
	TestFiles files = Installed();
	ghoul2::Store store( files );
	void *first = nullptr, *second = nullptr;
	LoadError error = LoadError::None;
	CHECK( store.AddModel( &first, "models/players/test/model.glm", error ) == 0 );
	CHECK( store.SetSkin( first, 0, 5 ) );

	CHECK( store.Duplicate( first, &second ) );
	CHECK( second && second != first && store.InstanceCount() == 2 );
	const fntech3::ghoul2::Instance *copy = store.Resolve( second );
	CHECK( copy && copy->Count() == 1 );
	CHECK( copy->At( 0 )->name == "models/players/test/model.glm" );
	CHECK( copy->At( 0 )->customSkin == 5 );   /* the skin comes with it */

	/* The copy is its own: changing one leaves the other alone. */
	CHECK( store.SetSkin( second, 0, 9 ) );
	CHECK( store.Resolve( second )->At( 0 )->customSkin == 9 );
	CHECK( store.Resolve( first )->At( 0 )->customSkin == 5 );

	/* And releasing one leaves the other whole. */
	CHECK( store.Release( first ) && store.InstanceCount() == 1 );
	CHECK( store.Resolve( second ) && store.Resolve( second )->Count() == 1 );
	CHECK( store.Resolve( first ) == nullptr );

	/* A source this store never gave out is refused, not copied. */
	void *nowhere = nullptr;
	int elsewhere = 0;
	CHECK( !store.Duplicate( nullptr, &nowhere ) && nowhere == nullptr );
	CHECK( !store.Duplicate( &elsewhere, &nowhere ) && nowhere == nullptr );
	CHECK( !store.Duplicate( second, nullptr ) );
	CHECK( store.InstanceCount() == 1 );
}

/* An instance can be attached to an entity, and the engine really holds it. */
static void CheckAttachmentsAreHeld()
{
	TestFiles files = Installed();
	ghoul2::Store store( files );
	void *first = nullptr, *second = nullptr;
	LoadError error = LoadError::None;
	CHECK( store.AddModel( &first, "models/players/test/model.glm", error ) == 0 );
	CHECK( store.AddModel( &second, "models/players/test/model.glm", error ) == 0 );
	CHECK( store.AttachmentCount() == 0 && store.Attached( 3 ) == nullptr );

	CHECK( store.Attach( first, 3 ) );
	CHECK( store.Attached( 3 ) == store.Resolve( first ) );
	CHECK( store.AttachmentCount() == 1 );
	CHECK( store.Attached( 4 ) == nullptr );

	/* An entity holds one attachment; attaching again replaces it. */
	CHECK( store.Attach( second, 3 ) && store.AttachmentCount() == 1 );
	CHECK( store.Attached( 3 ) == store.Resolve( second ) );

	/* An instance this store never gave out is never attached. */
	int elsewhere = 0;
	CHECK( !store.Attach( nullptr, 5 ) && !store.Attach( &elsewhere, 5 ) );
	CHECK( !store.Attach( first, -1 ) );
	CHECK( !store.Attach( first, fntech3::ghoul2::MaxAttachedEntities ) );
	CHECK( store.AttachmentCount() == 1 );

	CHECK( store.Detach( 3 ) && store.AttachmentCount() == 0 );
	CHECK( !store.Detach( 3 ) );

	/* Releasing an instance takes its attachments with it, so nothing is left
	 * pointing at an instance that is gone. */
	CHECK( store.Attach( first, 7 ) && store.Attach( second, 8 ) );
	CHECK( store.Release( first ) );
	CHECK( store.Attached( 7 ) == nullptr && store.AttachmentCount() == 1 );
	CHECK( store.Attached( 8 ) == store.Resolve( second ) );

	/* And clearing them really clears them, which is what the game's own
	 * clean call is answered with. */
	store.ClearAttachments();
	CHECK( store.AttachmentCount() == 0 && store.Attached( 8 ) == nullptr );
	CHECK( store.Resolve( second ) != nullptr );   /* the instance survives */
}

/* The names a model carries are read when it loads, and a bolt is looked up
 * by one of them. */
static void CheckBoltsAreLookedUpByName()
{
	TestFiles files = Installed();
	ghoul2::Store store( files );
	void *slot = nullptr;
	LoadError error = LoadError::None;
	CHECK( store.AddModel( &slot, "models/players/test/model.glm", error ) == 0 );

	/* The fixture's mesh names one surface and its skeleton three bones. */
	const ghoul2::Model *loaded = store.Resolve( slot )->At( 0 );
	CHECK( loaded->surfaceNames.size() == 1 && loaded->surfaceNames[0] == "root" );
	CHECK( loaded->boneNames.size() == 3 );
	CHECK( loaded->boneNames[0] == "model_root" && loaded->boneNames[1] == "child" );
	CHECK( loaded->bolts.empty() );

	/* A surface is found first, because surfaces and bones are numbered
	 * separately and a name that is both would mean the surface. */
	const int surfaceBolt = store.AddBolt( slot, 0, "root" );
	CHECK( surfaceBolt == 0 );
	const ghoul2::Bolt *bolt = store.BoltAt( slot, 0, surfaceBolt );
	CHECK( bolt && bolt->name == "root" && bolt->surface == 0 && bolt->bone == -1 );

	const int boneBolt = store.AddBolt( slot, 0, "model_root" );
	CHECK( boneBolt == 1 );
	bolt = store.BoltAt( slot, 0, boneBolt );
	CHECK( bolt && bolt->bone == 0 && bolt->surface == -1 );

	/* The same name twice keeps its index: a level asks for the same bolt on
	 * every entity that uses the model. */
	CHECK( store.AddBolt( slot, 0, "root" ) == surfaceBolt );
	CHECK( store.Resolve( slot )->At( 0 )->bolts.size() == 2 );

	/* A name the model does not carry is not a bolt, and adds nothing. */
	CHECK( store.AddBolt( slot, 0, "no_such_bone" ) == -1 );
	CHECK( store.Resolve( slot )->At( 0 )->bolts.size() == 2 );
	CHECK( store.AddBolt( slot, 0, nullptr ) == -1 );
	CHECK( store.AddBolt( slot, 0, "" ) == -1 );

	/* And neither is a model or an instance this store does not hold. */
	int elsewhere = 0;
	CHECK( store.AddBolt( slot, 1, "root" ) == -1 );
	CHECK( store.AddBolt( slot, -1, "root" ) == -1 );
	CHECK( store.AddBolt( nullptr, 0, "root" ) == -1 );
	CHECK( store.AddBolt( &elsewhere, 0, "root" ) == -1 );
	CHECK( store.BoltAt( slot, 0, 2 ) == nullptr );
	CHECK( store.BoltAt( slot, 0, -1 ) == nullptr );
	CHECK( store.BoltAt( &elsewhere, 0, 0 ) == nullptr );

	/* A copy carries the bolts its source had. */
	void *copy = nullptr;
	CHECK( store.Duplicate( slot, &copy ) );
	CHECK( store.BoltAt( copy, 0, surfaceBolt )->name == "root" );
}

/* A bone animation keeps its contract fields and the measured initial clock. */
static void CheckBoneAnimationsAreHeld()
{
	TestFiles files = Installed();
	ghoul2::Store store( files );
	void *slot = nullptr;
	LoadError error = LoadError::None;
	CHECK( store.AddModel( &slot, "models/players/test/model.glm", error ) == 0 );
	CHECK( store.BoneAnimation( slot, 0, "model_root" ) == nullptr );

	CHECK( store.SetBoneAnim( slot, 0, "model_root", 4, 19, 0x20, 1.5f, 1000, 4, 200 ) );
	const fntech3::ghoul2::BoneAnimation *playing =
		store.BoneAnimation( slot, 0, "model_root" );
	CHECK( playing && playing->bone == 0 );
	CHECK( playing->startFrame == 4 && playing->endFrame == 19 );
	CHECK( playing->flags == 0x20 && playing->speed == 1.5f );
	CHECK( playing->startTime == 1000 && playing->setFrame == 4 );
	CHECK( playing->blendTime == 200 );

	/* A second bone is its own record. */
	CHECK( store.SetBoneAnim( slot, 0, "child", 0, 3, 0, 1.0f, 1200, 0, 0 ) );
	CHECK( store.Resolve( slot )->At( 0 )->animations.size() == 2 );
	CHECK( store.BoneAnimation( slot, 0, "child" )->bone == 1 );
	CHECK( store.BoneAnimation( slot, 0, "model_root" )->startFrame == 4 );

	/* The same bone twice keeps one record, the later one. */
	CHECK( store.SetBoneAnim( slot, 0, "model_root", 7, 9, 1, 2.0f, 1500, 7, 0 ) );
	CHECK( store.Resolve( slot )->At( 0 )->animations.size() == 2 );
	playing = store.BoneAnimation( slot, 0, "model_root" );
	CHECK( playing->startFrame == 7 && playing->startTime == 1500 );
	CHECK( store.SetBoneAnim( slot, 0, "model_root", 4, 19, 0x20, 1.0f, 1000, 5.5f, 0 ) );
	playing = store.BoneAnimation( slot, 0, "model_root" );
	CHECK( playing->setFrame == 5.5f && playing->startTime == 925 );
	const auto seeded = ghoul2::PlayheadAt( *playing, 64, 1000 );
	CHECK( seeded.currentFrame == 5 && seeded.newFrame == 6 && std::fabs( seeded.lerp - .5f ) < .001f );
	const auto afterSeed = ghoul2::PlayheadAt( *playing, 64, 1025 );
	CHECK( afterSeed.currentFrame == 6 && afterSeed.newFrame == 7 );
	CHECK( store.SetBoneAnim( slot, 0, "model_root", 4, 19, 0x20, 1.0f, 1000, -1.0f, 0 ) );
	CHECK( store.BoneAnimation( slot, 0, "model_root" )->startTime == 1000 );
	const auto beforeMalformed = *store.BoneAnimation( slot, 0, "model_root" );
	for ( std::uint32_t bits : { 0x7f800000u, 0xff800000u, 0x7fc00001u } ) {
		float bad; std::memcpy( &bad, &bits, sizeof( bad ) );
		CHECK( !store.SetBoneAnim( slot, 0, "model_root", 4, 19, 0, bad, 1000, -1.0f, 0 ) );
		CHECK( !store.SetBoneAnim( slot, 0, "model_root", 4, 19, 0, 1.0f, 1000, bad, 0 ) );
	}
	CHECK( !store.SetBoneAnim( slot, 0, "model_root", 4, 19, 0, 0.0f, 1000, 5.5f, 0 ) );
	CHECK( !store.SetBoneAnim( slot, 0, "model_root", 4, 19, 0, 1.0f, 1000, 3.5f, 0 ) );
	CHECK( !store.SetBoneAnim( slot, 0, "model_root", 4, 19, 0, 1.0f, 1000, 20.0f, 0 ) );
	CHECK( !store.SetBoneAnim( slot, 0, "model_root", 0, 2, 0, 1.0f, INT_MIN, 2.0f, 0 ) );
	CHECK( !std::memcmp( &beforeMalformed, store.BoneAnimation( slot, 0, "model_root" ), sizeof( beforeMalformed ) ) );
	CHECK( store.SetBoneAnim( slot, 0, "model_root", 7, 9, 1, 2.0f, 1500, 7, 0 ) );

	/* A bone the skeleton does not carry is refused, and a surface is not a
	 * bone: an animation goes on a bone or nowhere. */
	CHECK( !store.SetBoneAnim( slot, 0, "no_such_bone", 0, 1, 0, 1.0f, 0, 0, 0 ) );
	CHECK( !store.SetBoneAnim( slot, 0, "root", 0, 1, 0, 1.0f, 0, 0, 0 ) );
	CHECK( !store.SetBoneAnim( slot, 0, nullptr, 0, 1, 0, 1.0f, 0, 0, 0 ) );
	CHECK( store.Resolve( slot )->At( 0 )->animations.size() == 2 );
	CHECK( store.BoneAnimation( slot, 0, "root" ) == nullptr );

	/* And neither a model nor an instance this store does not hold. */
	int elsewhere = 0;
	CHECK( !store.SetBoneAnim( slot, 1, "model_root", 0, 1, 0, 1.0f, 0, 0, 0 ) );
	CHECK( !store.SetBoneAnim( nullptr, 0, "model_root", 0, 1, 0, 1.0f, 0, 0, 0 ) );
	CHECK( !store.SetBoneAnim( &elsewhere, 0, "model_root", 0, 1, 0, 1.0f, 0, 0, 0 ) );
	CHECK( store.BoneAnimation( &elsewhere, 0, "model_root" ) == nullptr );
	CHECK( store.BoneAnimation( slot, -1, "model_root" ) == nullptr );

	/* A copy carries what its source was playing. */
	void *copy = nullptr;
	CHECK( store.Duplicate( slot, &copy ) );
	CHECK( store.BoneAnimation( copy, 0, "model_root" )->startFrame == 7 );
}

/* A bone can be bent, and the numbers come back as they went in. */
static void CheckBoneAnglesAreHeld()
{
	TestFiles files = Installed();
	ghoul2::Store store( files );
	void *slot = nullptr;
	LoadError error = LoadError::None;
	CHECK( store.AddModel( &slot, "models/players/test/model.glm", error ) == 0 );
	CHECK( store.BoneAngles( slot, 0, "model_root" ) == nullptr );

	const float bent[3] = { 10.0f, -20.0f, 30.5f };
	CHECK( store.SetBoneAngles( slot, 0, "model_root", bent, 0x4, 1, 2, 0, 150, 900 ) );
	const fntech3::ghoul2::BoneAngles *held = store.BoneAngles( slot, 0, "model_root" );
	CHECK( held && held->bone == 0 );
	CHECK( held->angles[0] == 10.0f && held->angles[1] == -20.0f && held->angles[2] == 30.5f );
	CHECK( held->flags == 0x4 && held->up == 1 && held->right == 2 && held->forward == 0 );
	CHECK( held->blendTime == 150 && held->startTime == 900 );

	/* Bending the same bone again keeps one record. */
	const float again[3] = { 1.0f, 2.0f, 3.0f };
	CHECK( store.SetBoneAngles( slot, 0, "model_root", again, 0, 0, 0, 0, 0, 1000 ) );
	CHECK( store.Resolve( slot )->At( 0 )->angles.size() == 1 );
	CHECK( store.BoneAngles( slot, 0, "model_root" )->angles[0] == 1.0f );

	/* Bending and playing are separate records on the same bone. */
	CHECK( store.SetBoneAnim( slot, 0, "model_root", 0, 5, 0, 1.0f, 1000, 0, 0 ) );
	CHECK( store.Resolve( slot )->At( 0 )->angles.size() == 1 );
	CHECK( store.Resolve( slot )->At( 0 )->animations.size() == 1 );

	/* A name the skeleton does not carry is refused, and so is a surface. */
	int elsewhere = 0;
	CHECK( !store.SetBoneAngles( slot, 0, "root", bent, 0, 0, 0, 0, 0, 0 ) );
	CHECK( !store.SetBoneAngles( slot, 0, "no_such_bone", bent, 0, 0, 0, 0, 0, 0 ) );
	CHECK( !store.SetBoneAngles( slot, 0, "model_root", nullptr, 0, 0, 0, 0, 0, 0 ) );
	CHECK( !store.SetBoneAngles( &elsewhere, 0, "model_root", bent, 0, 0, 0, 0, 0, 0 ) );
	CHECK( !store.SetBoneAngles( slot, 1, "model_root", bent, 0, 0, 0, 0, 0, 0 ) );
	CHECK( store.Resolve( slot )->At( 0 )->angles.size() == 1 );
}

/* A mesh whose skeleton name begins with a star carries its own bones, and no
 * file is looked for. Both retail installations use exactly one such name. */
static void CheckAStarNamedSkeletonIsNotAFile()
{
	TestFiles files;
	files.files["models/weapons2/saber/saber.glm"] = Mesh( "*default", 3 );

	ghoul2::Store store( files );
	void *slot = nullptr;
	LoadError error = LoadError::None;
	const int index = store.AddModel( &slot, "models/weapons2/saber/saber.glm", error );
	CHECK( index == 0 && error == LoadError::None );

	const ghoul2::Model *loaded = store.Resolve( slot )->At( 0 );
	CHECK( loaded && loaded->skeletonName == "*default" );
	CHECK( loaded->skeleton.numBones == loaded->mesh.numBones );
	CHECK( loaded->skeleton.numFrames == 0 );   /* there is no skeleton file */

	/* Nothing was opened: the store never asked the filesystem for a `.gla`. */
	CHECK( files.asked.count( "*default.gla" ) == 0 );
	CHECK( files.asked.count( "*default" ) == 0 );

	/* An ordinary name that is not there is still reported as not there, so
	 * the star is a rule and not a way of passing everything. */
	files.files["models/weapons2/other/other.glm"] = Mesh( "models/absent/absent", 3 );
	void *second = nullptr;
	CHECK( store.AddModel( &second, "models/weapons2/other/other.glm", error ) == -1 );
	CHECK( error == LoadError::SkeletonFile );
}

/* One model can be copied out of one instance into a place in another. */
static void CheckOneModelCopiesBetweenInstances()
{
	TestFiles files = Installed();
	ghoul2::Store store( files );
	void *first = nullptr, *second = nullptr;
	LoadError error = LoadError::None;
	CHECK( store.AddModel( &first, "models/players/test/model.glm", error ) == 0 );
	CHECK( store.SetSkin( first, 0, 3 ) && store.SetBoltInfo( first, 0, 9 ) );
	CHECK( store.AddModel( &second, "models/players/test/model.glm", error ) == 0 );

	/* Onto a model the destination already has: it is replaced. */
	CHECK( store.CopyModel( first, 0, second, 0 ) );
	CHECK( store.Resolve( second )->Count() == 1 );
	CHECK( store.Resolve( second )->At( 0 )->customSkin == 3 );
	CHECK( store.Resolve( second )->At( 0 )->boltInfo == 9 );

	/* Onto the one past its last: it is appended. */
	CHECK( store.CopyModel( first, 0, second, 1 ) );
	CHECK( store.Resolve( second )->Count() == 2 );
	CHECK( store.Resolve( second )->At( 1 )->customSkin == 3 );

	/* And the copy is its own from then on. */
	CHECK( store.SetSkin( second, 1, 6 ) );
	CHECK( store.Resolve( first )->At( 0 )->customSkin == 3 );

	/* Anything else is refused, and nothing is half-done. */
	int elsewhere = 0;
	CHECK( !store.CopyModel( first, 0, second, 3 ) );     /* past the end */
	CHECK( !store.CopyModel( first, 0, second, -1 ) );
	CHECK( !store.CopyModel( first, 1, second, 0 ) );     /* no such source */
	CHECK( !store.CopyModel( nullptr, 0, second, 0 ) );
	CHECK( !store.CopyModel( first, 0, nullptr, 0 ) );
	CHECK( !store.CopyModel( &elsewhere, 0, second, 0 ) );
	CHECK( !store.CopyModel( first, 0, &elsewhere, 0 ) );
	CHECK( store.Resolve( second )->Count() == 2 );
	CHECK( store.Resolve( second )->At( 1 )->customSkin == 6 );
}

static void CheckAPointerIsNeverTrusted()
{
	TestFiles files = Installed();
	ghoul2::Store store( files );
	void *slot = nullptr;
	LoadError error = LoadError::None;
	CHECK( store.AddModel( &slot, "models/players/test/model.glm", error ) == 0 );

	/* Anything this store did not hand out is refused, not followed. */
	int elsewhere = 0;
	CHECK( store.Resolve( &elsewhere ) == nullptr );
	CHECK( store.Resolve( nullptr ) == nullptr );
	CHECK( store.Resolve( reinterpret_cast<void *>( std::uintptr_t( 0xdeadbeef ) ) ) == nullptr );

	/* Once released it is refused too, and a module holding the old pointer
	 * gets a refusal rather than a freed instance. */
	void *stale = slot;
	CHECK( store.Release( slot ) );
	CHECK( store.Resolve( stale ) == nullptr );
	CHECK( !store.Release( stale ) );
	CHECK( store.InstanceCount() == 0 );

	/* A model asked for against a stale pointer starts a fresh instance
	 * rather than writing through it. */
	void *reused = stale;
	CHECK( store.AddModel( &reused, "models/players/test/model.glm", error ) == 0 );
	CHECK( reused != nullptr && store.Resolve( reused ) != nullptr );
	CHECK( reused != stale && store.Resolve( stale ) == nullptr );
}

static void CheckRefusals()
{
	LoadError error = LoadError::None;
	void *slot = nullptr;

	{	/* No name, and a name nothing answers. */
		TestFiles files = Installed();
		ghoul2::Store store( files );
		CHECK( store.AddModel( &slot, "", error ) == -1 && error == LoadError::NoName );
		CHECK( store.AddModel( &slot, nullptr, error ) == -1 && error == LoadError::NoName );
		CHECK( store.AddModel( &slot, "models/players/absent/model.glm", error ) == -1 );
		CHECK( error == LoadError::Missing );
		/* Nothing was written into the module's field by any of that. */
		CHECK( slot == nullptr && store.InstanceCount() == 0 );
	}
	{	/* A mesh that does not read. */
		TestFiles files = Installed();
		files.files["models/players/test/model.glm"][1] = 'X';
		ghoul2::Store store( files );
		slot = nullptr;
		CHECK( store.AddModel( &slot, "models/players/test/model.glm", error ) == -1 );
		CHECK( error == LoadError::Mesh && slot == nullptr );
	}
	{	/* A mesh whose skeleton is not installed. */
		TestFiles files = Installed();
		files.files.erase( "models/players/_humanoid/_humanoid.gla" );
		ghoul2::Store store( files );
		slot = nullptr;
		CHECK( store.AddModel( &slot, "models/players/test/model.glm", error ) == -1 );
		CHECK( error == LoadError::SkeletonFile );
	}
	{	/* A skeleton that does not read. */
		TestFiles files = Installed();
		files.files["models/players/_humanoid/_humanoid.gla"][1] = 'X';
		ghoul2::Store store( files );
		slot = nullptr;
		CHECK( store.AddModel( &slot, "models/players/test/model.glm", error ) == -1 );
		CHECK( error == LoadError::Skeleton );
	}
	{	/* A mesh that wants more bones than its skeleton has. */
		TestFiles files;
		files.files["models/players/test/model.glm"] =
			Mesh( "models/players/_humanoid/_humanoid", 3 );
		files.files["models/players/_humanoid/_humanoid.gla"] =
			Skeleton( "models/players/_humanoid/_humanoid", 1 );
		ghoul2::Store store( files );
		slot = nullptr;
		CHECK( store.AddModel( &slot, "models/players/test/model.glm", error ) == -1 );
		CHECK( error == LoadError::Bones );
	}
	{	/* An instance holds only so many. */
		TestFiles files = Installed();
		ghoul2::Store store( files );
		slot = nullptr;
		for ( int i = 0; i < ghoul2::MaxModelsPerInstance; ++i ) {
			CHECK( store.AddModel( &slot, "models/players/test/model.glm", error ) == i );
		}
		CHECK( store.AddModel( &slot, "models/players/test/model.glm", error ) == -1 );
		CHECK( error == LoadError::Full );
		CHECK( store.Resolve( slot )->Count() == ghoul2::MaxModelsPerInstance );
	}

	for ( LoadError value : { LoadError::None, LoadError::NoName, LoadError::Missing,
		LoadError::Mesh, LoadError::SkeletonName, LoadError::SkeletonFile,
		LoadError::Skeleton, LoadError::Bones, LoadError::Full,
		LoadError::HandleExhausted } ) {
		const char *name = ghoul2::LoadErrorName( value );
		CHECK( name && name[0] && std::strcmp( name, "unknown" ) );
	}
	CHECK( !std::strcmp( ghoul2::LoadErrorName( (LoadError)42 ), "unknown" ) );
}

static void CheckClearing()
{
	TestFiles files = Installed();
	ghoul2::Store store( files );
	void *first = nullptr, *second = nullptr;
	LoadError error = LoadError::None;
	CHECK( store.AddModel( &first, "models/players/test/model.glm", error ) == 0 );
	CHECK( store.AddModel( &second, "models/players/test/model.glm", error ) == 0 );
	CHECK( first != second && store.InstanceCount() == 2 );
	store.Clear();
	CHECK( store.InstanceCount() == 0 );
	CHECK( store.Resolve( first ) == nullptr && store.Resolve( second ) == nullptr );
}

static void CheckOwnedCleanupAndIdentityLifetime()
{
	TestFiles files = Installed();
	ghoul2::Store store( files ), other( files );
	void *first = nullptr, *sibling = nullptr, *foreign = nullptr;
	LoadError error = LoadError::None;
	CHECK( store.AddModel( &first, "models/players/test/model.glm", error ) == 0 );
	CHECK( other.AddModel( &foreign, "models/players/test/model.glm", error ) == 0 );
	CHECK( first != foreign && !store.Resolve( foreign ) && !other.Resolve( first ) );
	CHECK( first != static_cast<void *>( store.Resolve( first ) ) );
	CHECK( store.SetSkin( first, 0, 17 ) && store.SetBoltInfo( first, 0, 23 ) );
	CHECK( store.AddBolt( first, 0, "model_root" ) == 0 );
	CHECK( store.Duplicate( first, &sibling ) && first != sibling );
	const ghoul2::Model *held = store.Resolve( sibling )->At( 0 );
	CHECK( held && held->meshBytes && held->skeletonBytes );
	const auto *meshBytes = held->meshBytes;
	const auto *skeletonBytes = held->skeletonBytes;
	const Bytes meshCopy = *meshBytes, skeletonCopy = *skeletonBytes;
	CHECK( store.Attach( first, 0 ) && store.Attach( first, 1023 ) );
	CHECK( store.Attach( sibling, 3 ) );
	void *stale = first;
	CHECK( store.Clean( &first ) && !first && store.InstanceCount() == 1 );
	CHECK( !store.Resolve( stale ) && store.AttachmentCount() == 1 );
	CHECK( !store.Attached( 0 ) && !store.Attached( 1023 ) );
	CHECK( store.Attached( 3 ) == store.Resolve( sibling ) );
	CHECK( held == store.Resolve( sibling )->At( 0 ) && held->customSkin == 17 );
	CHECK( held->boltInfo == 23 && held->bolts.size() == 1 );
	CHECK( *meshBytes == meshCopy && *skeletonBytes == skeletonCopy );
	CHECK( store.Clean( &first ) && !store.Clean( nullptr ) );
	CHECK( !store.Clean( &stale ) && !store.Clean( &foreign ) );
	CHECK( foreign && other.Resolve( foreign ) && store.InstanceCount() == 1 );

	std::set<void *> retired{ stale, sibling, foreign };
	for ( int iteration = 0; iteration < 256; ++iteration ) {
		void *fresh = nullptr;
		CHECK( store.AddModel( &fresh, "models/players/test/model.glm", error ) == 0 );
		CHECK( retired.insert( fresh ).second && !store.Resolve( stale ) );
		CHECK( store.Release( fresh ) && !store.Release( fresh ) );
	}
	store.Clear();
	store.Clear();
	CHECK( !store.InstanceCount() && !store.AttachmentCount() );
	CHECK( !store.Clean( &sibling ) && !store.Clean( &stale ) );
	void *fresh = nullptr;
	CHECK( store.AddModel( &fresh, "models/players/test/model.glm", error ) == 0 );
	CHECK( retired.insert( fresh ).second && !store.Resolve( sibling ) );
	CHECK( *meshBytes == meshCopy && *skeletonBytes == skeletonCopy );
	CHECK( other.Clean( &foreign ) && !foreign );

	/* Slot writes are exactly pointer-sized, including an unaligned VM slot. */
	unsigned char bytes[sizeof( void * ) + 2];
	std::memset( bytes, 0xa5, sizeof( bytes ) );
	std::memcpy( bytes + 1, &fresh, sizeof( fresh ) );
	CHECK( store.Clean( reinterpret_cast<void **>( bytes + 1 ) ) );
	void *cleared = fresh;
	std::memcpy( &cleared, bytes + 1, sizeof( cleared ) );
	CHECK( !cleared && bytes[0] == 0xa5 && bytes[sizeof( bytes ) - 1] == 0xa5 );
	CHECK( !store.Resolve( fresh ) && store.InstanceCount() == 0 );
}

static void CheckIdentityExhaustion()
{
	const std::uintptr_t last = std::numeric_limits<std::uintptr_t>::max();
	ghoul2::OpaqueHandleSequence sequence( last - 1 );
	CHECK( reinterpret_cast<std::uintptr_t>( sequence.Allocate() ) == last - 1 );
	CHECK( reinterpret_cast<std::uintptr_t>( sequence.Allocate() ) == last );
	for ( int iteration = 0; iteration < 10; ++iteration ) CHECK( !sequence.Allocate() );
	ghoul2::OpaqueHandleSequence exhausted( 0 );
	CHECK( !exhausted.Allocate() && !exhausted.Allocate() );
}

/* A skin is registered for its handle and its surface names, not for how any
 * of it is drawn: that is what a server needs and all it is told. */
static void CheckSkinsRegister()
{
	TestFiles files;
	files.files["models/players/test/model_default.skin"] = Bytes();
	const std::string skin =
		"// a remark\n"
		"torso,models/players/test/torso.tga\n"
		"  l_arm  ,models/players/test/arm.tga\n"
		"nothing-here\n"
		"tag_head,\n"
		"r_arm,models/players/test/arm.tga // trailing remark\n";
	files.files["models/players/test/model_default.skin"].assign( skin.begin(), skin.end() );

	fntech3::ghoul2::SkinRegistry skins( files );
	CHECK( skins.Count() == 0 );

	const int handle = skins.Register( "models/players/test/model_default.skin" );
	CHECK( handle == 1 );
	const fntech3::ghoul2::Skin *registered = skins.At( handle );
	CHECK( registered && registered->surfaces.size() == 4 );
	CHECK( registered->surfaces[0] == "torso" );
	CHECK( registered->surfaces[1] == "l_arm" );   /* its padding is trimmed */
	CHECK( registered->surfaces[2] == "tag_head" ); /* a tag names a surface too */
	CHECK( registered->surfaces[3] == "r_arm" );

	/* The same skin twice keeps its handle, because a level asks for one skin
	 * on many entities. */
	CHECK( skins.Register( "models/players/test/model_default.skin" ) == handle );
	CHECK( skins.Count() == 1 );

	/* Zero means no skin, and it is what everything unusable registers as. */
	CHECK( skins.At( 0 ) == nullptr && skins.At( 2 ) == nullptr && skins.At( -1 ) == nullptr );
	CHECK( skins.Register( nullptr ) == 0 );
	CHECK( skins.Register( "" ) == 0 );
	CHECK( skins.Register( "models/players/test/absent.skin" ) == 0 );

	/* A file that names no surface is not a skin. */
	const std::string empty = "// only a remark\nnothing-here\n";
	files.files["models/players/test/empty.skin"].assign( empty.begin(), empty.end() );
	CHECK( skins.Register( "models/players/test/empty.skin" ) == 0 );
	CHECK( skins.Count() == 1 );

	skins.Clear();
	CHECK( skins.Count() == 0 && skins.At( 1 ) == nullptr );
}

/* The timing model, against the arithmetic read out of the retail engine:
 * frame = startFrame + ((now - startTime) / 50) * speed, split into a whole
 * frame, the frame after it, and the fraction between them. */
static void CheckTheTimingModel()
{
	fntech3::ghoul2::BoneAnimation playing;
	playing.bone = 0;
	playing.startFrame = 10;
	playing.endFrame = 20;
	playing.speed = 1.0f;
	playing.startTime = 1000;
	playing.flags = 0;

	// At the instant it starts, it is on its first frame and has not moved.
	fntech3::ghoul2::Playhead at =
		fntech3::ghoul2::PlayheadAt( playing, 64, 1000 );
	CHECK( at.currentFrame == 10 );
	CHECK( at.newFrame == 11 );
	CHECK( at.lerp > -0.001f && at.lerp < 0.001f );

	// Fifty milliseconds is one frame, at a speed of one.
	at = fntech3::ghoul2::PlayheadAt( playing, 64, 1050 );
	CHECK( at.currentFrame == 11 && at.newFrame == 12 );

	// Half a frame in, the blend weight is a half.
	at = fntech3::ghoul2::PlayheadAt( playing, 64, 1075 );
	CHECK( at.currentFrame == 11 );
	CHECK( at.lerp > 0.49f && at.lerp < 0.51f );

	// Twice the speed covers twice the frames in the same time.
	playing.speed = 2.0f;
	at = fntech3::ghoul2::PlayheadAt( playing, 64, 1100 );
	CHECK( at.currentFrame == 14 );
	playing.speed = 1.0f;

	// A time before it started does not run it backwards.
	at = fntech3::ghoul2::PlayheadAt( playing, 64, 0 );
	CHECK( at.currentFrame == 10 );

	// Past the end it holds the last frame, and says so on both answers.
	at = fntech3::ghoul2::PlayheadAt( playing, 64, 100000 );
	CHECK( at.currentFrame == 19 && at.newFrame == 19 );

	// With the loop flag it wraps instead, and never leaves the range.
	playing.flags = fntech3::ghoul2::LoopFlag;
	for ( int step = 0; step < 400; ++step ) {
		at = fntech3::ghoul2::PlayheadAt( playing, 64, 1000 + step * 37 );
		CHECK( at.currentFrame >= 10 && at.currentFrame < 20 );
		CHECK( at.newFrame >= 10 && at.newFrame < 20 );
		CHECK( at.lerp >= 0.0f && at.lerp < 1.0f );
	}
	// Exactly one span on brings it back to where it began.
	at = fntech3::ghoul2::PlayheadAt( playing, 64, 1000 + 10 * 50 );
	CHECK( at.currentFrame == 10 );
	playing.flags = 0;

	// A range the file cannot hold is held at its first frame rather than
	// used to index with.
	playing.endFrame = 1000;
	at = fntech3::ghoul2::PlayheadAt( playing, 12, 5000 );
	CHECK( at.currentFrame >= 0 && at.currentFrame < 12 );
	CHECK( at.newFrame >= 0 && at.newFrame < 12 );

	// A skeleton with no frames answers nothing rather than divide by it.
	at = fntech3::ghoul2::PlayheadAt( playing, 0, 5000 );
	CHECK( at.currentFrame == 0 && at.newFrame == 0 );

	// A backwards range is refused rather than walked.
	playing.startFrame = 20;
	playing.endFrame = 10;
	at = fntech3::ghoul2::PlayheadAt( playing, 64, 5000 );
	CHECK( at.currentFrame == 20 && at.newFrame == 20 );
	playing.startFrame = 1000;
	at = fntech3::ghoul2::PlayheadAt( playing, 64, 5000 );
	CHECK( at.currentFrame == 63 && at.newFrame == 63 );
	playing.startFrame = 0; playing.endFrame = 64; playing.startTime = INT_MIN;
	at = fntech3::ghoul2::PlayheadAt( playing, 64, INT_MAX );
	CHECK( at.currentFrame >= 0 && at.currentFrame < 64 );
	CHECK( at.newFrame >= 0 && at.newFrame < 64 );
}

/* A skeleton of one bone that rests at the origin, with the undo of that rest
 * stated beside it as the files do. One frame, whose pose is a rotation of
 * nothing and a displacement of nothing in the engine's own encoding. */
static Bytes TagSkeleton( const std::string &name )
{
	Bytes file;
	AppendInt( file, std::int32_t( model::SkeletonIdent ) );
	AppendInt( file, model::SkeletonVersion );
	AppendName( file, name );
	AppendFloat( file, 1.0f );
	AppendInt( file, 1 );                 // numFrames
	const std::size_t framesField = file.size();
	AppendInt( file, 0 );
	AppendInt( file, 1 );                 // numBones
	const std::size_t poolField = file.size();
	AppendInt( file, 0 );
	AppendInt( file, std::int32_t( model::SkeletonHeaderBytes + 4 ) );
	const std::size_t endField = file.size();
	AppendInt( file, 0 );
	CHECK( file.size() == model::SkeletonHeaderBytes );

	const std::size_t table = file.size();
	AppendInt( file, 0 );
	PutInt( file, table, std::int32_t( file.size() - model::SkeletonHeaderBytes ) );
	AppendName( file, "tag_bone" );
	AppendInt( file, 0 );                 // flags
	AppendInt( file, -1 );                // parent
	/* Rest, then the undo of it: both the identity, so a vertex modelled in
	 * the model's own space stays exactly where it was modelled. */
	for ( int half = 0; half < 2; ++half ) {
		for ( int row = 0; row < 3; ++row ) {
			for ( int column = 0; column < 4; ++column ) {
				AppendFloat( file, row == column ? 1.0f : 0.0f );
			}
		}
	}
	AppendInt( file, 0 );                 // numChildren

	PutInt( file, framesField, std::int32_t( file.size() ) );
	file.push_back( 0 ); file.push_back( 0 ); file.push_back( 0 );
	while ( file.size() % 4 ) file.push_back( 0 );
	PutInt( file, poolField, std::int32_t( file.size() ) );
	const int word[7] = { 3 * 16383, 2 * 16383, 2 * 16383, 2 * 16383,
		512 * 64, 512 * 64, 512 * 64 };
	for ( int i = 0; i < 7; ++i ) {
		file.push_back( std::uint8_t( word[i] & 0xff ) );
		file.push_back( std::uint8_t( ( word[i] >> 8 ) & 0xff ) );
	}
	PutInt( file, endField, std::int32_t( file.size() ) );
	return file;
}

/* A mesh of one three-cornered surface -- a tag -- whose corners are not in a
 * line, so that it has an orientation to give. */
static Bytes TagMesh( const std::string &skeleton )
{
	Bytes file;
	AppendInt( file, std::int32_t( model::MeshIdent ) );
	AppendInt( file, model::MeshVersion );
	AppendName( file, "models/players/tagtest/model.glm" );
	AppendName( file, skeleton );
	AppendInt( file, 0 );                 // animIndex
	AppendInt( file, 1 );                 // numBones
	AppendInt( file, 1 );                 // numLODs
	const std::size_t lodField = file.size();
	AppendInt( file, 0 );
	AppendInt( file, 1 );                 // numSurfaces
	AppendInt( file, std::int32_t( model::MeshHeaderBytes + 4 ) );
	const std::size_t endField = file.size();
	AppendInt( file, 0 );
	CHECK( file.size() == model::MeshHeaderBytes );

	const std::size_t hierarchy = file.size();
	AppendInt( file, 0 );
	PutInt( file, hierarchy, std::int32_t( file.size() - model::MeshHeaderBytes ) );
	AppendName( file, "*tag" );
	AppendInt( file, 0 );
	AppendName( file, "models/players/tagtest/skin" );
	AppendInt( file, 0 );
	AppendInt( file, -1 );
	AppendInt( file, 0 );

	PutInt( file, lodField, std::int32_t( file.size() ) );
	const std::size_t lod = file.size();
	AppendInt( file, 0 );
	const std::size_t table = file.size();
	AppendInt( file, 0 );
	PutInt( file, table, std::int32_t( file.size() - table ) );

	/* The surface, written by hand so its corners can be placed. */
	const std::size_t start = file.size();
	const std::size_t boneRefs = model::SurfaceBytes;
	const std::size_t triangleAt = boneRefs + 4;
	const std::size_t vertAt = triangleAt + model::TriangleBytes;
	const std::size_t texAt = vertAt + 3 * model::VertexBytes;
	const std::size_t end = texAt + 3 * model::TexCoordBytes;
	AppendInt( file, 0 );
	AppendInt( file, 0 );
	AppendInt( file, -std::int32_t( start ) );
	AppendInt( file, 3 );                             // numVerts
	AppendInt( file, std::int32_t( vertAt ) );
	AppendInt( file, 1 );                             // numTriangles
	AppendInt( file, std::int32_t( triangleAt ) );
	AppendInt( file, 1 );                             // numBoneReferences
	AppendInt( file, std::int32_t( boneRefs ) );
	AppendInt( file, std::int32_t( end ) );
	AppendInt( file, 0 );                             // the one bone reference
	AppendInt( file, 0 ); AppendInt( file, 1 ); AppendInt( file, 2 );
	const float corner[3][3] = {
		{ 10.0f, 20.0f, 30.0f },
		{ 11.0f, 20.0f, 30.0f },
		{ 10.0f, 21.0f, 30.0f },
	};
	for ( int vertex = 0; vertex < 3; ++vertex ) {
		AppendFloat( file, 0 ); AppendFloat( file, 0 ); AppendFloat( file, 1 );
		AppendFloat( file, corner[vertex][0] );
		AppendFloat( file, corner[vertex][1] );
		AppendFloat( file, corner[vertex][2] );
		AppendInt( file, std::int32_t( 0u ) );        // one weight, bone reference 0
		for ( int weight = 0; weight < model::MaxWeightsPerVertex; ++weight ) file.push_back( 0 );
	}
	for ( int vertex = 0; vertex < 3; ++vertex ) {
		AppendFloat( file, 0.25f ); AppendFloat( file, 0.5f );
	}
	PutInt( file, lod, std::int32_t( file.size() - lod ) );
	PutInt( file, endField, std::int32_t( file.size() ) );
	return file;
}

struct TagFiles final : public ghoul2::Files {
	std::map<std::string, Bytes> held;
	TagFiles()
	{
		held["models/players/tagtest/model.glm"] = TagMesh( "models/players/tagtest/skeleton" );
		held["models/players/tagtest/skeleton.gla"] = TagSkeleton( "models/players/tagtest/skeleton" );
	}
	std::size_t Read( const char *path, void *destination, std::size_t capacity ) override
	{
		auto found = held.find( path ? path : "" );
		if ( found == held.end() ) return 0;
		if ( destination && capacity >= found->second.size() ) {
			std::memcpy( destination, found->second.data(), found->second.size() );
		}
		return found->second.size();
	}
};

/* A tag's corners are carried by frame zero and the fixed Ghoul2 root basis.
 *
 * This is the earlier frame -- origin at the first triangle's first corner,
 * columns along its first edge, square to it, and its normal -- which retail
 * does not use. It is pinned deliberately: the stores that do not serve
 * attachments keep it (the UI's and single player's). Retail's frame, served
 * by both Jedi servers and Jedi Academy's client game, is checked below. */
static void CheckABoltMatrix()
{
	TagFiles files;
	ghoul2::Store store( files );
	void *slot = nullptr;
	LoadError error = LoadError::None;
	const int index = store.AddModel( &slot, "models/players/tagtest/model.glm", error );
	CHECK( index == 0 );
	CHECK( error == LoadError::None );

	const int bolt = store.AddBolt( slot, 0, "*tag" );
	CHECK( bolt == 0 );
	const ghoul2::Bolt *held = store.BoltAt( slot, 0, bolt );
	CHECK( held && held->surface == 0 && held->bone < 0 );

	float matrix[12];
	CHECK( store.BoltMatrix( slot, 0, bolt, 0, matrix ) );

	/* Frame zero is identity; the root basis rotates (10,20,30) by +90 Z. */
	CHECK( matrix[3] > -20.001f && matrix[3] < -19.999f );
	CHECK( matrix[7] > 9.999f && matrix[7] < 10.001f );
	CHECK( matrix[11] > 29.999f && matrix[11] < 30.001f );
	/* Along the first edge, square to it, and the normal, as columns. */
	const float earlier[12] = { 0, -1, 0, -20,  1, 0, 0, 10,  0, 0, 1, 30 };
	for ( int i = 0; i < 12; ++i ) {
		CHECK( matrix[i] > earlier[i] - 0.0001f && matrix[i] < earlier[i] + 0.0001f );
	}

	/* Its axes are a set: each a unit, each square to the others. They are
	 * held as columns, which is how the retail engine writes them. */
	for ( int column = 0; column < 3; ++column ) {
		float length = 0.0f;
		for ( int row = 0; row < 3; ++row ) {
			length += matrix[row * 4 + column] * matrix[row * 4 + column];
		}
		CHECK( length > 0.999f && length < 1.001f );
	}
	for ( int a = 0; a < 3; ++a ) {
		for ( int b = a + 1; b < 3; ++b ) {
			float dot = 0.0f;
			for ( int row = 0; row < 3; ++row ) {
				dot += matrix[row * 4 + a] * matrix[row * 4 + b];
			}
			CHECK( dot > -0.001f && dot < 0.001f );
		}
	}

	/* A bolt that is not there, a model that is not there, and a pointer this
	 * store never gave out are all refused rather than answered. */
	CHECK( !store.BoltMatrix( slot, 0, 1, 0, matrix ) );
	CHECK( !store.BoltMatrix( slot, 0, -1, 0, matrix ) );
	CHECK( !store.BoltMatrix( slot, 7, bolt, 0, matrix ) );
	void *stranger = reinterpret_cast<void *>( std::uintptr_t( 0x1234 ) );
	CHECK( !store.BoltMatrix( stranger, 0, bolt, 0, matrix ) );
	CHECK( !store.BoltMatrix( slot, 0, bolt, 0, nullptr ) );
}

/* Retail's tag frame (jamp.exe 1.01 0x499476..0x499848), which a store
 * serving attachments gives: Jedi Academy's server and client game. From the
 * tag's first three vertices -- here p0 (10,20,30), p1 (11,20,30) and
 * p2 (10,21,30), carried by the +90 Z root basis -- its columns are
 * |p0 - p2|, |p1 - p0| made square to it and -|(p1 - p0) x (p0 - p2)|, and
 * its origin is p2. That is no turn at all at (-21,10,30), where the earlier
 * frame above turns a quarter and sits at p0. */
static void CheckAServedTagFrame()
{
	TagFiles files;
	ghoul2::Store store( files, false, false, true );
	void *slot = nullptr;
	LoadError error = LoadError::None;
	CHECK( store.AddModel( &slot, "models/players/tagtest/model.glm", error ) == 0 );
	const int bolt = store.AddBolt( slot, 0, "*tag" );
	CHECK( bolt == 0 );
	float matrix[12];
	CHECK( store.BoltMatrix( slot, 0, bolt, 0, matrix ) );
	const float retail[12] = { 1, 0, 0, -21,  0, 1, 0, 10,  0, 0, 1, 30 };
	for ( int i = 0; i < 12; ++i ) {
		CHECK( matrix[i] > retail[i] - 0.0001f && matrix[i] < retail[i] + 0.0001f );
	}
}

int main()
{
	CheckAModelLoads();
	CheckSkinsRegister();
	CheckSkinsGoOnModels();
	CheckAnInstanceCopies();
	CheckAttachmentsAreHeld();
	CheckBoltsAreLookedUpByName();
	CheckBoneAnimationsAreHeld();
	CheckBoneAnglesAreHeld();
	CheckAStarNamedSkeletonIsNotAFile();
	CheckOneModelCopiesBetweenInstances();
	CheckAPointerIsNeverTrusted();
	CheckRefusals();
	CheckClearing();
	CheckOwnedCleanupAndIdentityLifetime();
	CheckIdentityExhaustion();
	CheckTheTimingModel();
	CheckABoltMatrix();
	CheckAServedTagFrame();
	std::cout << "Raven server-side Ghoul2 model store tests passed\n";
	return 0;
}
