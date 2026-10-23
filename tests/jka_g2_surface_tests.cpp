/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Jedi Academy multiplayer's server Ghoul2 surfaces and model removal: the
 * override list import 609 and a server skin write (jamp.exe 1.01 0x4ee700,
 * 0x4ee7e0), import 612's render status (0x4eee70), the skin loader
 * (0x4a0370, 0x4a0160, 0x4a0540), removal (0x4decc0, 0x4deea0), the init's
 * reuse of an empty place (0x4de5c0) and the collision walk reading the
 * list. Every mesh, skeleton and skin here is written by this test. See
 * docs/fntech3/2026-10-22-jka-g2-surfaces.md. */
#include "../code/ghoul2/ghoul2_instance_collision.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace ghoul2 = fntech3::ghoul2;
namespace model = fntech3::jka::model;
using Bytes = std::vector<std::uint8_t>;

static int failures = 0;
#define CHECK( condition ) do { if ( !( condition ) ) { \
	std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition ); ++failures; } } while ( 0 )

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
	file.insert( file.end(), name.begin(), name.end() );
	file.insert( file.end(), model::NameBytes - name.size(), 0 );
}

struct SurfaceSpec {
	std::string name;
	std::uint32_t flags;
	int parent;
};

/* A mesh of the given hierarchy at one level of detail, each surface one
 * triangle weighted wholly to bone 0. */
static Bytes Mesh( const std::vector<SurfaceSpec> &surfaces )
{
	const int count = int( surfaces.size() );
	Bytes file;
	AppendInt( file, std::int32_t( model::MeshIdent ) );
	AppendInt( file, model::MeshVersion );
	AppendName( file, "models/players/test/model.glm" );
	AppendName( file, "models/players/test/skeleton" );
	AppendInt( file, 0 );
	AppendInt( file, 1 );                 // numBones
	AppendInt( file, 1 );                 // numLODs
	const std::size_t lodField = file.size();
	AppendInt( file, 0 );
	AppendInt( file, count );
	AppendInt( file, std::int32_t( model::MeshHeaderBytes + std::size_t( count ) * 4 ) );
	const std::size_t endField = file.size();
	AppendInt( file, 0 );

	const std::size_t table = file.size();
	for ( int i = 0; i < count; ++i ) AppendInt( file, 0 );
	for ( int i = 0; i < count; ++i ) {
		PutInt( file, table + std::size_t( i ) * 4, std::int32_t( file.size() - model::MeshHeaderBytes ) );
		AppendName( file, surfaces[std::size_t( i )].name );
		AppendInt( file, std::int32_t( surfaces[std::size_t( i )].flags ) );
		AppendName( file, "models/players/test/skin" );
		AppendInt( file, 0 );
		AppendInt( file, surfaces[std::size_t( i )].parent );
		std::vector<int> children;
		for ( int child = 0; child < count; ++child ) {
			if ( surfaces[std::size_t( child )].parent == i ) children.push_back( child );
		}
		AppendInt( file, int( children.size() ) );
		for ( int child : children ) AppendInt( file, child );
	}

	PutInt( file, lodField, std::int32_t( file.size() ) );
	const std::size_t lod = file.size();
	AppendInt( file, 0 );
	const std::size_t offsets = file.size();
	for ( int i = 0; i < count; ++i ) AppendInt( file, 0 );
	for ( int i = 0; i < count; ++i ) {
		PutInt( file, offsets + std::size_t( i ) * 4, std::int32_t( file.size() - offsets ) );
		const std::size_t start = file.size();
		const std::size_t triangleAt = model::SurfaceBytes + 4;
		const std::size_t vertAt = triangleAt + model::TriangleBytes;
		const std::size_t end = vertAt + 3 * ( model::VertexBytes + model::TexCoordBytes );
		AppendInt( file, 0 );
		AppendInt( file, i );
		AppendInt( file, -std::int32_t( start ) );
		AppendInt( file, 3 );
		AppendInt( file, std::int32_t( vertAt ) );
		AppendInt( file, 1 );
		AppendInt( file, std::int32_t( triangleAt ) );
		AppendInt( file, 1 );
		AppendInt( file, std::int32_t( model::SurfaceBytes ) );
		AppendInt( file, std::int32_t( end ) );
		AppendInt( file, 0 );                                  // bone reference
		AppendInt( file, 0 ); AppendInt( file, 1 ); AppendInt( file, 2 );
		for ( int v = 0; v < 3; ++v ) {
			AppendFloat( file, 0 ); AppendFloat( file, 0 ); AppendFloat( file, 1 );
			AppendFloat( file, float( i ) ); AppendFloat( file, float( v ) ); AppendFloat( file, float( v == 2 ) );
			AppendInt( file, 0 );
			for ( int w = 0; w < model::MaxWeightsPerVertex; ++w ) file.push_back( 0 );
		}
		for ( int v = 0; v < 3; ++v ) { AppendFloat( file, 0.0f ); AppendFloat( file, 0.0f ); }
	}
	PutInt( file, lod, std::int32_t( file.size() - lod ) );
	PutInt( file, endField, std::int32_t( file.size() ) );
	return file;
}

/* One bone, one frame, at rest. */
static Bytes Skeleton()
{
	Bytes file;
	AppendInt( file, std::int32_t( model::SkeletonIdent ) );
	AppendInt( file, model::SkeletonVersion );
	AppendName( file, "models/players/test/skeleton" );
	AppendFloat( file, 1.0f );
	AppendInt( file, 1 );
	const std::size_t framesField = file.size();
	AppendInt( file, 0 );
	AppendInt( file, 1 );
	const std::size_t poolField = file.size();
	AppendInt( file, 0 );
	AppendInt( file, std::int32_t( model::SkeletonHeaderBytes + 4 ) );
	const std::size_t endField = file.size();
	AppendInt( file, 0 );
	const std::size_t table = file.size();
	AppendInt( file, 0 );
	PutInt( file, table, std::int32_t( file.size() - model::SkeletonHeaderBytes ) );
	AppendName( file, "model_root" );
	AppendInt( file, 0 );
	AppendInt( file, -1 );
	/* Its rest matrix and that matrix's inverse, both identity. */
	for ( int matrix = 0; matrix < 24; ++matrix ) AppendFloat( file, ( matrix % 12 ) % 5 ? 0.0f : 1.0f );
	AppendInt( file, 0 );
	PutInt( file, framesField, std::int32_t( file.size() ) );
	file.push_back( 0 ); file.push_back( 0 ); file.push_back( 0 );
	while ( file.size() % 4 ) file.push_back( 0 );
	PutInt( file, poolField, std::int32_t( file.size() ) );
	/* An identity rotation (w 1, x y z 0) and no translation. */
	for ( int raw : { 49149, 32766, 32766, 32766, 32768, 32768, 32768 } ) {
		file.push_back( std::uint8_t( raw ) );
		file.push_back( std::uint8_t( raw >> 8 ) );
	}
	PutInt( file, endField, std::int32_t( file.size() ) );
	return file;
}

class TestFiles final : public ghoul2::Files {
public:
	std::map<std::string, Bytes> files;
	std::vector<std::string> asked;
	std::size_t Read( const char *path, void *destination, std::size_t capacity ) override
	{
		asked.push_back( path ? path : "" );
		const auto found = files.find( path );
		if ( found == files.end() ) return 0;
		if ( !destination || capacity < found->second.size() ) return found->second.size();
		std::memcpy( destination, found->second.data(), found->second.size() );
		return found->second.size();
	}
	void Text( const std::string &path, const std::string &text ) { files[path].assign( text.begin(), text.end() ); }
};

/* Kyle's shape, cut down: names, own flags and parents as the retail mesh
 * writes them, and two surfaces whose names differ only in case. */
static const std::vector<SurfaceSpec> Body = {
	{ "stupidtriangle_off", 0x2, -1 },  // 0
	{ "hips", 0x0, 0 },                 // 1
	{ "torso", 0x0, 1 },                // 2
	{ "head", 0x0, 2 },                 // 3
	{ "*head_eyes", 0x1, 3 },           // 4
	{ "head_cap_torso_off", 0x2, 3 },   // 5
	{ "r_arm", 0x0, 2 },                // 6
	{ "r_hand", 0x0, 6 },               // 7
	{ "dup", 0x100, 1 },                // 8
	{ "DUP", 0x0, 1 },                  // 9
	{ "under_dup", 0x0, 9 },            // 10
	{ "capped", 0x100, 1 },             // 11
	{ "veil", 0x2, 2 },                 // 12
};

static TestFiles Installed()
{
	TestFiles files;
	files.files["models/players/test/model.glm"] = Mesh( Body );
	files.files["models/players/test/skeleton.gla"] = Skeleton();
	return files;
}

static ghoul2::Model *Load( ghoul2::Store &store, void *&slot )
{
	ghoul2::LoadError error = ghoul2::LoadError::None;
	const int index = store.AddModel( &slot, "models/players/test/model.glm", error );
	CHECK( index >= 0 && error == ghoul2::LoadError::None );
	return store.ModelAt( slot, index );
}

static void LoadTests()
{
	TestFiles files = Installed();
	ghoul2::Store store( files, true, true, true, false, ghoul2::NameMatch::AsciiFolded, true );
	void *slot = nullptr;
	const ghoul2::Model *body = Load( store, slot );
	CHECK( body && body->surfaceNames.size() == Body.size() );
	CHECK( body->surfaceFlags.size() == Body.size() && body->surfaceParents.size() == Body.size() );
	for ( std::size_t i = 0; body && i < Body.size(); ++i ) {
		CHECK( body->surfaceNames[i] == Body[i].name );
		CHECK( body->surfaceFlags[i] == Body[i].flags && body->surfaceParents[i] == Body[i].parent );
	}
	CHECK( body && body->surfaceOverrides.empty() );
}

static void SwitchTests()
{
	TestFiles files = Installed();
	ghoul2::Store store( files, true, true, true, false, ghoul2::NameMatch::AsciiFolded, true );
	void *slot = nullptr;
	ghoul2::Model *body = Load( store, slot );
	if ( !body ) return;

	/* A name the model lacks, or none, switches nothing; the server keeps the
	 * file's names, so the client loader's trimmed "head_cap_torso" is not
	 * one. */
	for ( const char *name : { "nonexistent", "", "head_cap_torso", "stupidtriangle" } ) {
		CHECK( !ghoul2::SetSurfaceOnOff( *body, name, 2 ) );
	}
	CHECK( !ghoul2::SetSurfaceOnOff( *body, nullptr, 2 ) );
	CHECK( body->surfaceOverrides.empty() );

	/* Names fold case; only OFF and NODESCENDANTS are taken. */
	CHECK( ghoul2::SetSurfaceOnOff( *body, "TORSO", 0xffffffffu ) );
	CHECK( body->surfaceOverrides.size() == 1 && body->surfaceOverrides.at( 2 ) == 0x102u );
	CHECK( ghoul2::SetSurfaceOnOff( *body, "torso", 0x100 ) && body->surfaceOverrides.at( 2 ) == 0x100u );
	/* Back to its own flags drops the entry. */
	CHECK( ghoul2::SetSurfaceOnOff( *body, "torso", 1 ) && body->surfaceOverrides.empty() );
	/* Other bits of the surface's own flags stay. */
	CHECK( ghoul2::SetSurfaceOnOff( *body, "*head_eyes", 2 ) && body->surfaceOverrides.at( 4 ) == 0x3u );
	CHECK( ghoul2::SetSurfaceOnOff( *body, "*HEAD_EYES", 0 ) && !body->surfaceOverrides.count( 4 ) );
	/* A surface off in its file can be switched on. */
	CHECK( ghoul2::SetSurfaceOnOff( *body, "head_cap_torso_off", 0 ) && body->surfaceOverrides.at( 5 ) == 0u );
	/* The first of two names that fold alike is the one switched. */
	CHECK( ghoul2::SetSurfaceOnOff( *body, "Dup", 0 ) && body->surfaceOverrides.at( 8 ) == 0u );
	CHECK( !body->surfaceOverrides.count( 9 ) );
}

static void StatusTests()
{
	TestFiles files = Installed();
	ghoul2::Store store( files, true, true, true, false, ghoul2::NameMatch::AsciiFolded, true );
	void *slot = nullptr;
	ghoul2::Model *body = Load( store, slot );
	if ( !body ) return;
	using ghoul2::SurfaceRenderStatus;

	CHECK( SurfaceRenderStatus( *body, "nonexistent" ) == -1 );
	CHECK( SurfaceRenderStatus( *body, "" ) == -1 );
	CHECK( SurfaceRenderStatus( *body, nullptr ) == -1 );
	CHECK( SurfaceRenderStatus( *body, "HEAD" ) == 0 );
	CHECK( SurfaceRenderStatus( *body, "stupidtriangle_off" ) == 2 );
	CHECK( SurfaceRenderStatus( *body, "*head_eyes" ) == 1 );
	/* A file NODESCENDANTS of an ancestor turns every descendant off. */
	CHECK( SurfaceRenderStatus( *body, "capped" ) == 0x100 );
	CHECK( SurfaceRenderStatus( *body, "dup" ) == 0x100 );

	/* A listed NODESCENDANTS: the surface answers its listed flags, every
	 * descendant its own file flags with OFF. */
	CHECK( ghoul2::SetSurfaceOnOff( *body, "torso", 0x100 ) );
	CHECK( SurfaceRenderStatus( *body, "torso" ) == 0x100 );
	CHECK( SurfaceRenderStatus( *body, "head" ) == 2 );
	CHECK( SurfaceRenderStatus( *body, "*head_eyes" ) == 3 );
	CHECK( SurfaceRenderStatus( *body, "r_hand" ) == 2 );
	CHECK( SurfaceRenderStatus( *body, "hips" ) == 0 );
	CHECK( ghoul2::SetSurfaceOnOff( *body, "torso", 0 ) );

	/* A surface whose own file flags are not zero answers them, whatever
	 * its list says (0x4eef47). */
	CHECK( ghoul2::SetSurfaceOnOff( *body, "*head_eyes", 2 ) && SurfaceRenderStatus( *body, "*head_eyes" ) == 1 );
	CHECK( ghoul2::SetSurfaceOnOff( *body, "head_cap_torso_off", 0 )
		&& SurfaceRenderStatus( *body, "head_cap_torso_off" ) == 2 );
	CHECK( ghoul2::SetSurfaceOnOff( *body, "head", 2 ) && SurfaceRenderStatus( *body, "head" ) == 2 );
	/* OFF alone does not reach the children. */
	CHECK( SurfaceRenderStatus( *body, "head_cap_torso_off" ) == 2 );
	CHECK( SurfaceRenderStatus( *body, "*head_eyes" ) == 1 );

	/* An ancestor is looked up by its name: under_dup's parent is "DUP"
	 * (surface 9), whose name folds to "dup", surface 8, which carries
	 * NODESCENDANTS in its file. */
	CHECK( SurfaceRenderStatus( *body, "under_dup" ) == 2 );
	CHECK( ghoul2::SetSurfaceOnOff( *body, "dup", 0 ) && SurfaceRenderStatus( *body, "under_dup" ) == 0 );
}

static void SkinLoaderTests()
{
	TestFiles files;
	ghoul2::SkinRegistry skins( files );
	const auto names = []( const ghoul2::Skin *skin ) {
		std::string all;
		for ( const auto &surface : skin ? skin->surfaces : std::vector<ghoul2::SkinSurface>() ) {
			all += surface.name + ( surface.off ? "=off;" : ";" );
		}
		return all;
	};

	/* Quotes, block remarks, "_off" lines trimmed or skipped, "*off" only
	 * as written, and a tag line whose shader is then read as the next
	 * name: the comma after that starts the next name, as retail reads it. */
	files.Text( "models/a.skin",
		"\"Quoted Name\",\"*off\"\n"
		"/* a block\n remark */ waist,w.tga\n"
		"leg_off,*off\n"
		"arm_off,caps.tga\n"
		"_off,s.tga\n"
		"off,*OFF\n"
		"tag_head,models/x.tga\n"
		"after,a.tga\n" );
	const int a = skins.Register( "models/a.skin" );
	CHECK( a == 1 );
	CHECK( names( skins.At( a ) ) == "quoted name=off;waist;arm;;off;models/x.tga;,a.tga;" );

	/* A byte at or above 0x80 ends a word and is skipped as space. */
	files.Text( "models/b.skin", "left\xc3\xa9,b.tga\nright,c.tga\n" );
	CHECK( names( skins.At( skins.Register( "models/b.skin" ) ) ) == "left;right;" );

	/* A word of 1024 bytes or more is empty, which ends the file. */
	files.Text( "models/c.skin", "first,c.tga\n" + std::string( 1024, 'x' ) + ",d.tga\nlast,e.tga\n" );
	CHECK( names( skins.At( skins.Register( "models/c.skin" ) ) ) == "first;" );

	/* Names are cut to 63 bytes; at 128 surfaces the file stops. */
	std::string many = std::string( 70, 'n' ) + ",long.tga\n";
	for ( int i = 0; i < 140; ++i ) many += "s" + std::to_string( i ) + ",x.tga\n";
	files.Text( "models/d.skin", many );
	const ghoul2::Skin *d = skins.At( skins.Register( "models/d.skin" ) );
	CHECK( d && d->surfaces.size() == std::size_t( ghoul2::MaxSkinSurfaces ) );
	CHECK( d && d->surfaces[0].name == std::string( 63, 'n' ) && d->surfaces.back().name == "s126" );

	/* A name of 64 bytes or more is no skin and takes no place. */
	const int before = skins.Count();
	CHECK( skins.Register( ( "models/" + std::string( 57, 'z' ) ).c_str() ) == 0 );
	CHECK( skins.Count() == before );

	/* Three parts: path/head.skin, path/torso.skin, path/lower.skin, read
	 * into one skin in turn. A missing part answers 0, but the parts before
	 * it are kept, so the same name answers its handle the next time. */
	files.Text( "models/p/h.skin", "head,h.tga\n" );
	files.Text( "models/p/t.skin", "torso,*off\n" );
	files.Text( "models/p/l.skin", "legs,l.tga\n" );
	const int three = skins.Register( "models/p/|h|t|l" );
	CHECK( three && names( skins.At( three ) ) == "head;torso=off;legs;" );
	const int broken = skins.Register( "models/p/|h|missing|l" );
	CHECK( broken == 0 );
	const int again = skins.Register( "models/p/|h|missing|l" );
	CHECK( again && names( skins.At( again ) ) == "head;" );
	/* Two parts is one file name, which is not there. */
	CHECK( skins.Register( "models/p/|h|t" ) == 0 );
	CHECK( files.asked.back() == "models/p/|h|t" );

	/* At 1023 skins no more are registered. */
	for ( int i = skins.Count(); i < ghoul2::MaxSkins; ++i ) skins.Register( ( "models/n" + std::to_string( i ) ).c_str() );
	CHECK( skins.Count() == ghoul2::MaxSkins );
	CHECK( skins.Register( "models/a.skin" ) == a );
	CHECK( skins.Register( "models/new.skin" ) == 0 && skins.Count() == ghoul2::MaxSkins );
}

static void SkinVisibilityTests()
{
	TestFiles files = Installed();
	files.Text( "models/menu.skin",
		"hips,*off\n"            // off
		"head,h.tga\n"           // on: already on
		"capped,c.tga\n"         // on: its NODESCENDANTS is cleared
		"head_cap_torso_off,*off\n"  // skipped
		"r_arm,*off\n"
		"r_arm,a.tga\n"          // a later line wins
		"stupidtriangle_off,s.tga\n" ); // trimmed: no surface of that name
	files.Text( "models/on.skin", "*head_eyes,e.tga\nveil,v.tga\n" );
	ghoul2::SkinRegistry skins( files );
	const int menu = skins.Register( "models/menu.skin" );
	const int on = skins.Register( "models/on.skin" );
	CHECK( menu && on );
	ghoul2::Store store( files, true, true, true, false, ghoul2::NameMatch::AsciiFolded, true );
	void *slot = nullptr;
	ghoul2::Model *body = Load( store, slot );
	if ( !body ) return;

	CHECK( ghoul2::SetSurfaceOnOff( *body, "torso", 0x100 ) );
	ghoul2::ApplySkinVisibility( *body, skins.At( menu ) );
	/* The list was emptied first: torso is back. */
	const std::map<int, std::uint32_t> expected{ { 1, 0x2u }, { 11, 0x0u } };
	CHECK( body->surfaceOverrides == expected );
	CHECK( ghoul2::SurfaceRenderStatus( *body, "torso" ) == 0 );

	/* An "on" line does not turn on a surface off in its file, nor change a
	 * surface whose only bit is another. */
	ghoul2::ApplySkinVisibility( *body, skins.At( on ) );
	CHECK( body->surfaceOverrides.empty() );

	/* A skin whose file gave no surface only empties the list, and so does
	 * no skin (the server refuses an unregistered handle before this). */
	const int absent = skins.Register( "models/absent.skin" );
	CHECK( absent == 0 && skins.At( 3 ) && skins.At( 3 )->surfaces.empty() );
	CHECK( ghoul2::SetSurfaceOnOff( *body, "hips", 2 ) );
	ghoul2::ApplySkinVisibility( *body, skins.At( 3 ) );
	CHECK( body->surfaceOverrides.empty() );
	CHECK( ghoul2::SetSurfaceOnOff( *body, "hips", 2 ) );
	ghoul2::ApplySkinVisibility( *body, nullptr );
	CHECK( body->surfaceOverrides.empty() );
}

static void RemovalTests()
{
	TestFiles files = Installed();
	ghoul2::Store store( files, true, true, true, false, ghoul2::NameMatch::AsciiFolded, true );
	using Removal = ghoul2::Store::Removal;
	ghoul2::LoadError error = ghoul2::LoadError::None;
	const char *mesh = "models/players/test/model.glm";

	void *field = nullptr;
	CHECK( store.AddModel( &field, mesh, error ) == 0 && store.AddModel( &field, mesh, error ) == 1
		&& store.AddModel( &field, mesh, error ) == 2 );
	void *const handle = field;
	CHECK( store.Attach( handle, 5 ) );
	CHECK( store.SetSkin( handle, 1, 9 ) && store.AddBolt( handle, 1, "hips" ) == 0 );
	CHECK( ghoul2::SetSurfaceOnOff( *store.ModelAt( handle, 1 ), "torso", 2 ) );

	/* Nothing to remove: an index past the places, or an empty place. */
	CHECK( store.RemoveModel( &field, sizeof( field ), 3 ) == Removal::NoModel );
	CHECK( store.RemoveModel( &field, sizeof( field ), 1 ) == Removal::Removed );
	CHECK( store.RemoveModel( &field, sizeof( field ), 1 ) == Removal::NoModel );
	const ghoul2::Instance *instance = store.Resolve( handle );
	CHECK( field == handle && instance && instance->Count() == 3 && !instance->At( 1 ) && instance->HoldsAny() );

	/* The init fills the first empty place, fresh. */
	CHECK( store.AddModel( &field, mesh, error ) == 1 && field == handle );
	const ghoul2::Model *refilled = instance->At( 1 );
	CHECK( refilled && refilled->customSkin == 0 && refilled->bolts.empty() && refilled->surfaceOverrides.empty() );

	/* Empty places at the end are dropped. */
	CHECK( store.RemoveModel( &field, sizeof( field ), 1 ) == Removal::Removed );
	CHECK( store.RemoveModel( &field, sizeof( field ), 2 ) == Removal::Removed );
	CHECK( instance->Count() == 1 && instance->At( 0 ) );
	CHECK( store.AddModel( &field, mesh, error ) == 1 && instance->Count() == 2 );

	/* An instance left with no place is freed, its field emptied and its
	 * attachments forgotten. */
	CHECK( store.RemoveModel( &field, sizeof( field ), 0 ) == Removal::Removed && instance->Count() == 2 );
	CHECK( store.RemoveModel( &field, sizeof( field ), 1 ) == Removal::Deleted );
	CHECK( field == nullptr && !store.Resolve( handle ) && !store.Attached( 5 ) );
	CHECK( store.InstanceCount() == 0 );
	CHECK( store.RemoveModel( &field, sizeof( field ), 0 ) == Removal::NoInstance );
	void *foreign = reinterpret_cast<void *>( std::uintptr_t( 0x1234 ) );
	CHECK( store.RemoveModel( &foreign, sizeof( foreign ), 0 ) == Removal::NoInstance && foreign );
	CHECK( store.RemoveModel( nullptr, sizeof( foreign ), 0 ) == Removal::NoInstance );

	/* Every model at once, through a four-byte field. */
	void *second = nullptr;
	CHECK( store.AddModel( &second, mesh, error ) == 0 && store.AddModel( &second, mesh, error ) == 1 );
	std::uint32_t word = std::uint32_t( reinterpret_cast<std::uintptr_t>( second ) );
	CHECK( store.RemoveModels( &word, sizeof( word ) ) == Removal::Deleted && word == 0 );
	CHECK( !store.Resolve( second ) && store.RemoveModels( &word, sizeof( word ) ) == Removal::NoInstance );

	/* A store that does not reuse places appends after its last. */
	ghoul2::Store plain( files, true, true, true );
	void *third = nullptr;
	CHECK( plain.AddModel( &third, mesh, error ) == 0 && plain.AddModel( &third, mesh, error ) == 1 );
	CHECK( plain.RemoveModel( &third, sizeof( third ), 0 ) == Removal::Removed );
	CHECK( plain.AddModel( &third, mesh, error ) == 2 );
}

static void CopyTests()
{
	TestFiles files = Installed();
	ghoul2::Store store( files, true, true, true, false, ghoul2::NameMatch::AsciiFolded, true );
	void *first = nullptr;
	ghoul2::Model *body = Load( store, first );
	if ( !body ) return;
	CHECK( ghoul2::SetSurfaceOnOff( *body, "torso", 0x100 ) );
	void *copy = nullptr;
	CHECK( store.Duplicate( first, &copy ) );
	CHECK( store.ModelAt( copy, 0 ) && store.ModelAt( copy, 0 )->surfaceOverrides == body->surfaceOverrides );
	CHECK( store.CopyModel( first, 0, copy, 1 ) && store.ModelAt( copy, 1 )->surfaceOverrides.at( 2 ) == 0x100u );
	/* The copies are their own. */
	CHECK( ghoul2::SetSurfaceOnOff( *store.ModelAt( copy, 0 ), "torso", 0 ) );
	CHECK( body->surfaceOverrides.size() == 1 );
}

static void CollisionTests()
{
	TestFiles files = Installed();
	ghoul2::Store store( files, true, true, true, false, ghoul2::NameMatch::AsciiFolded, true );
	ghoul2::LoadError error = ghoul2::LoadError::None;
	void *slot = nullptr;
	CHECK( store.AddModel( &slot, "models/players/test/model.glm", error ) == 0
		&& store.AddModel( &slot, "models/players/test/model.glm", error ) == 1 );
	const ghoul2::Instance *instance = store.Resolve( slot );
	if ( !instance ) return;
	CHECK( ghoul2::SetSurfaceOnOff( *store.ModelAt( slot, 1 ), "torso", 0x100 ) );
	ghoul2::collision::GeometryCache cache;
	ghoul2::collision::Prepared prepared;
	CHECK( ghoul2::collision::Prepare( store, *instance, 0, 0, cache,
		ghoul2::collision::StoredOverrides( *instance ), prepared ) );
	CHECK( prepared.models.size() == 2 );
	CHECK( prepared.models.size() == 2 && !prepared.models[0].overrides
		&& prepared.models[1].overrides == &instance->At( 1 )->surfaceOverrides );

	/* A ray through every surface's triangle: the rest pose puts surface i's
	 * corners at model (i, v, v == 2), which the root basis turns to
	 * (-v, i, v == 2). Model 1's torso and everything under it are left
	 * out. */
	ghoul2::collision::Query query;
	query.entityNum = 3;
	query.start = { -0.25f, -1.0f, 0.05f };
	query.end = { -0.25f, 20.0f, 0.05f };
	ghoul2::collision::Records records;
	const int count = ghoul2::collision::Detect( prepared.models, query, records );
	int perModel[2] = { 0, 0 };
	for ( int i = 0; i < count; ++i ) {
		if ( records[std::size_t( i )].modelIndex >= 0 && records[std::size_t( i )].modelIndex < 2 ) {
			++perModel[records[std::size_t( i )].modelIndex];
		}
	}
	/* Model 0 walks hips, torso, head, r_arm, r_hand, DUP and under_dup's
	 * triangles (the off, NODESCENDANTS and other-bit surfaces stay out);
	 * model 1 only hips and DUP's branch. */
	CHECK( perModel[0] == 7 );
	CHECK( perModel[1] == 3 );
}

int main()
{
	LoadTests();
	SwitchTests();
	StatusTests();
	SkinLoaderTests();
	SkinVisibilityTests();
	RemovalTests();
	CopyTests();
	CollisionTests();
	if ( failures ) {
		std::printf( "%d failure(s)\n", failures );
		return 1;
	}
	std::printf( "Jedi Academy Ghoul2 surface and removal tests passed\n" );
	return 0;
}
