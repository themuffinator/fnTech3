/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 *
 * Read every asset path given on stdin through the same readers the engine
 * uses -- code/qcommon/bsp_rbsp.h for maps and code/compat/jka_model_format.hpp
 * for Ghoul2 meshes and skeletons -- and report one line per file:
 *   <path> <kind> <status> <detail>
 * Used by scripts/probe_jka_assets.py against a legitimate Jedi Academy
 * installation; no retail data is stored in the repository. */

#include "../../code/qcommon/q_shared.h"
#include "../../code/qcommon/bsp_rbsp.h"
#include "../../code/compat/jka_model_format.hpp"
#include "../../code/compat/jka_icarus_values.hpp"
#include "../../code/compat/jka_roff_format.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <iostream>
#include <string>
#include <vector>

void Q_strncpyz( char *out, const char *in, int size )
{
	if ( size <= 0 ) return;
	std::strncpy( out, in, size - 1 );
	out[size - 1] = 0;
}

namespace {
namespace model = fntech3::jka::model;
namespace roff = fntech3::jka::roff;
namespace icarus = fntech3::jka::icarus;

void *Alloc( int size ) { return std::calloc( 1, std::size_t( size ) ); }
void Free( void *pointer ) { std::free( pointer ); }

std::vector<unsigned char> Read( const std::string &path )
{
	std::ifstream file( path, std::ios::binary | std::ios::ate );
	const auto end = file.tellg();
	if ( !file || end <= 0 ) return {};
	std::vector<unsigned char> data( static_cast<std::size_t>( end ) );
	file.seekg( 0 );
	if ( !file.read( reinterpret_cast<char *>( data.data() ),
		static_cast<std::streamsize>( data.size() ) ) ) return {};
	return data;
}

std::uint32_t Ident( const std::vector<unsigned char> &data )
{
	if ( data.size() < 4 ) return 0;
	return std::uint32_t( data[0] ) | ( std::uint32_t( data[1] ) << 8 )
		| ( std::uint32_t( data[2] ) << 16 ) | ( std::uint32_t( data[3] ) << 24 );
}

void ReportMap( const std::string &path, const std::vector<unsigned char> &data )
{
	rbsp_translation_t translation{};
	char error[128] = "";
	const int version = data.size() >= 8
		? int( std::uint32_t( data[4] ) | ( std::uint32_t( data[5] ) << 8 )
			| ( std::uint32_t( data[6] ) << 16 ) | ( std::uint32_t( data[7] ) << 24 ) ) : -1;
	if ( !RBSP_TranslateToV46( data.data(), int( data.size() ), Alloc, Free,
		&translation, error, sizeof( error ) ) ) {
		std::printf( "%s map refused version=%d %s\n", path.c_str(), version, error );
		return;
	}
	dheader_t header;
	std::memcpy( &header, translation.data, sizeof( header ) );
	const auto lump = [&header]( int index ) {
		return std::make_pair( int( header.lumps[index].fileofs ), int( header.lumps[index].filelen ) );
	};
	std::printf( "%s map ok version=%d source=%zu translated=%d surfaces=%d verts=%d brushsides=%d grid=%d\n",
		path.c_str(), version, data.size(), translation.length,
		lump( LUMP_SURFACES ).second / int( sizeof( dsurface_t ) ),
		lump( LUMP_DRAWVERTS ).second / int( sizeof( drawVert_t ) ),
		lump( LUMP_BRUSHSIDES ).second / int( sizeof( dbrushside_t ) ),
		lump( LUMP_LIGHTGRID ).second / RBSP_GRID_BYTES );
	Free( translation.data );
}

void ReportMesh( const std::string &path, const std::vector<unsigned char> &data, int bones )
{
	model::Mesh mesh;
	const auto error = model::ParseMesh( data.data(), data.size(), bones, mesh );
	if ( error != model::Error::None ) {
		std::printf( "%s mesh refused %s\n", path.c_str(), model::ErrorName( error ) );
		return;
	}
	char skeleton[model::NameBytes + 1] = "";
	std::memcpy( skeleton, data.data() + mesh.animNameOffset, model::NameBytes );
	skeleton[model::NameBytes] = 0;
	std::printf( "%s mesh ok surfaces=%d lods=%d bones=%d verts=%d triangles=%d maxbone=%d weights=%d skeleton=%s\n",
		path.c_str(), mesh.numSurfaces, mesh.numLODs, mesh.numBones, mesh.totalVerts,
		mesh.totalTriangles, mesh.maxBoneReference, mesh.maxWeightsPerVertex, skeleton );
}

void ReportPath( const std::string &path, const std::vector<unsigned char> &data )
{
	roff::Path motion;
	const auto error = roff::Parse( data.data(), data.size(), motion );
	if ( error != roff::Error::None ) {
		std::printf( "%s roff refused %s\n", path.c_str(), roff::ErrorName( error ) );
		return;
	}
	std::printf( "%s roff ok version=%d entries=%d rate=%d notes=%d usednotes=%d\n",
		path.c_str(), motion.version, motion.entries, motion.frameRate, motion.notes,
		motion.maxNoteIndex + 1 );
}

void ReportScript( const std::string &path, const std::vector<unsigned char> &data )
{
	icarus::Script script;
	/* Count the distinct block and member numbers this script uses, so the
	 * sweep reports the container's shape without interpreting any of it. */
	std::set<std::int32_t> blockIds, memberIds;
	std::size_t text = 0;
	const auto error = icarus::Walk( data.data(), data.size(), script,
		[&]( const icarus::Block &block, const icarus::Member *member ) {
			blockIds.insert( block.id );
			if ( !member ) return;
			memberIds.insert( member->id );
			const char *value = nullptr;
			if ( icarus::TerminatedText( data.data(), *member, value ) ) ++text;
		} );
	if ( error != icarus::Error::None ) {
		std::printf( "%s script refused %s\n", path.c_str(), icarus::ErrorName( error ) );
		return;
	}
	/* The container reads; now group its members into values by the recorded
	 * rules, sized from what the container just counted. */
	std::vector<icarus::Statement> statements( std::size_t( script.blocks ) + 1 );
	std::vector<icarus::Value> values( std::size_t( script.members ) + 1 );
	icarus::ScriptValues view;
	view.statements = statements.data();
	view.statementCapacity = int( statements.size() );
	view.values = values.data();
	view.valueCapacity = int( values.size() );
	const auto valueError = icarus::ReadValues( data.data(), data.size(), view );
	if ( valueError != icarus::ValueError::None ) {
		std::printf( "%s script refused %s\n", path.c_str(),
			icarus::ValueErrorName( valueError ) );
		return;
	}
	std::size_t compares = 0, ranges = 0, ordered = 0;
	for ( int index = 0; index < view.statementCount; ++index ) {
		if ( view.statements[index].compare ) ++compares;
	}
	for ( int index = 0; index < view.valueCount; ++index ) {
		if ( view.values[index].kind != icarus::ValueKind::Range ) continue;
		++ranges;
		if ( view.values[index].numbers[0] < view.values[index].numbers[1] ) ++ordered;
	}
	std::printf( "%s script ok blocks=%d members=%d payload=%zu text=%zu blockids=%zu "
		"memberids=%zu statements=%d values=%d compares=%zu ranges=%zu ordered=%zu\n",
		path.c_str(), script.blocks, script.members, script.memberBytes, text,
		blockIds.size(), memberIds.size(), view.statementCount, view.valueCount,
		compares, ranges, ordered );
}

void ReportSkeleton( const std::string &path, const std::vector<unsigned char> &data )
{
	model::Skeleton skeleton;
	const auto error = model::ParseSkeleton( data.data(), data.size(), skeleton );
	if ( error != model::Error::None ) {
		std::printf( "%s skeleton refused %s\n", path.c_str(), model::ErrorName( error ) );
		return;
	}
	std::printf( "%s skeleton ok bones=%d frames=%d poses=%d children=%d scale=%g\n",
		path.c_str(), skeleton.numBones, skeleton.numFrames, skeleton.maxPoseIndex + 1,
		skeleton.maxBoneChildren, double( skeleton.scale ) );
}
} // namespace

int main( int argc, char **argv )
{
	/* Each line is a path, optionally followed by a tab and the bone count of
	 * the skeleton that mesh names, which cross-checks the pair. The argument
	 * supplies a default for lines that carry no count of their own. */
	const int fallback = argc > 1 ? std::atoi( argv[1] ) : 0;
	std::string line;
	while ( std::getline( std::cin, line ) ) {
		while ( !line.empty() && ( line.back() == '\r' || line.back() == '\n' ) ) line.pop_back();
		if ( line.empty() ) continue;
		int bones = fallback;
		std::string path = line;
		const auto tab = line.find( '\t' );
		if ( tab != std::string::npos ) {
			path = line.substr( 0, tab );
			bones = std::atoi( line.c_str() + tab + 1 );
		}
		const auto data = Read( path );
		if ( data.empty() ) {
			std::printf( "%s unknown unreadable\n", path.c_str() );
			continue;
		}
		switch ( Ident( data ) ) {
		case RBSP_IDENT: ReportMap( path, data ); break;
		case model::MeshIdent: ReportMesh( path, data, bones ); break;
		case model::SkeletonIdent: ReportSkeleton( path, data ); break;
		case roff::Ident: ReportPath( path, data ); break;
		case icarus::Ident: ReportScript( path, data ); break;
		default: std::printf( "%s unknown ident=%08x\n", path.c_str(), Ident( data ) ); break;
		}
	}
	return 0;
}
