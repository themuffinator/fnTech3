/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for the renderer's side of Alice's models
 * (code/renderercommon/tr_alice_tiki.cpp), which all three renderers share.
 * Every skeleton, animation and mesh here is written by this test; no retail
 * file is used.
 *
 * Two retail rules are pinned. A skeletal surface is found by index in the
 * records the engine's reader kept when the file was read, never by walking
 * the chain of surfaces, and it is the surface the walk would have found.
 * And an animation or frame an entity names that its model does not have is
 * corrected as retail alice.exe's renderer corrects it -- to animation 0 and
 * frame 0, never clamped to the last frame -- with retail's developer
 * message (R_AddTikiSurfaces 0x004a6320, the frame at 0x004a6421;
 * R_AddSkelSurfaces 0x004a3a10 through 0x004a1f60). */

#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/alice_tiki.h"
#include "../code/renderercommon/tr_alice_tiki.h"
#include "alice_pak_reader.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice;
using fntech3::alice::tiki::Animation;
using fntech3::alice::tiki::File;
using fntech3::alice::tiki::Model;
using Bytes = std::vector<unsigned char>;

/* The engine's alias lists are not exercised; the model's deleter only has
 * to exist. */
namespace fntech3 { namespace alice { namespace alias {
void ListDeleter::operator()( List * ) const {}
} } }

static void PutInt( Bytes &b, std::size_t at, std::int32_t v ) { std::memcpy( b.data() + at, &v, 4 ); }
static void PutFloat( Bytes &b, std::size_t at, float v ) { std::memcpy( b.data() + at, &v, 4 ); }
static void PutShort( Bytes &b, std::size_t at, std::int16_t v ) { std::memcpy( b.data() + at, &v, 2 ); }
static void PutName( Bytes &b, std::size_t at, const char *s ) { std::memcpy( b.data() + at, s, std::strlen( s ) ); }

/* ---- what the renderer reports ------------------------------------------ */

static std::vector<std::pair<int, std::string>> reports;

static void QDECL Capture( printParm_t level, const char *fmt, ... ) {
	char text[512];
	va_list ap;
	va_start( ap, fmt );
	std::vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	reports.emplace_back( static_cast<int>( level ), text );
}

/* ---- a skeleton ------------------------------------------------------------ */

/* Two bones and three surfaces whose vertices carry different weight counts,
 * so the surfaces differ in size and a walk has real offsets to follow. */
static const int kSurfaces = 3;
static const int kVerts[kSurfaces] = { 3, 4, 3 };
static const int kTris[kSurfaces] = { 1, 2, 1 };

static Bytes Skeleton() {
	const std::uint32_t ident = model::SkbIdent;
	Bytes b( model::SkbHeaderBytes, 0 );
	std::memcpy( b.data(), &ident, 4 );
	PutInt( b, 4, 3 );
	PutName( b, 8, "models/test/test.skb" );
	PutInt( b, 72, kSurfaces );
	PutInt( b, 76, 2 );
	PutInt( b, 80, static_cast<std::int32_t>( model::SkbHeaderBytes ) );
	for ( int bone = 0; bone < 2; ++bone ) {
		const std::size_t at = b.size();
		b.resize( at + model::SkbBoneBytes, 0 );
		PutInt( b, at, bone - 1 );
		PutName( b, at + 8, bone ? "child" : "root" );
	}
	PutInt( b, 84, static_cast<std::int32_t>( b.size() ) );
	for ( int s = 0; s < kSurfaces; ++s ) {
		const std::size_t at = b.size();
		const std::size_t tris = model::SkbTriangleBytes * kTris[s];
		std::size_t verts = 0;
		for ( int v = 0; v < kVerts[s]; ++v ) verts += model::SkbVertexFixedBytes + model::SkbWeightBytes * ( 1 + v % 2 );
		const std::size_t end = model::SkbSurfaceHeaderBytes + tris + verts + model::SkbCollapseBytes * kVerts[s];
		b.resize( at + end, 0 );
		std::memcpy( b.data() + at, &ident, 4 );
		const std::string name = "surface" + std::to_string( s );
		PutName( b, at + 4, name.c_str() );
		PutInt( b, at + 68, kTris[s] );
		PutInt( b, at + 72, kVerts[s] );
		PutInt( b, at + 76, kVerts[s] );
		PutInt( b, at + 80, static_cast<std::int32_t>( model::SkbSurfaceHeaderBytes ) );
		PutInt( b, at + 84, static_cast<std::int32_t>( model::SkbSurfaceHeaderBytes + tris ) );
		PutInt( b, at + 88, static_cast<std::int32_t>( model::SkbSurfaceHeaderBytes + tris + verts ) );
		PutInt( b, at + 92, static_cast<std::int32_t>( end ) );
		for ( int t = 0; t < kTris[s]; ++t ) {
			const std::size_t tri = at + model::SkbSurfaceHeaderBytes + model::SkbTriangleBytes * t;
			PutInt( b, tri, 0 );
			PutInt( b, tri + 4, 1 + t );
			PutInt( b, tri + 8, 2 + t );
		}
		std::size_t v = at + model::SkbSurfaceHeaderBytes + tris;
		for ( int i = 0; i < kVerts[s]; ++i ) {
			const int weights = 1 + i % 2;
			PutFloat( b, v + 8, 1.0f );                    /* normal +z */
			PutFloat( b, v + 12, 0.25f * i );              /* s */
			PutInt( b, v + model::SkbWeightCountOffset, weights );
			for ( int w = 0; w < weights; ++w ) {
				const std::size_t at2 = v + model::SkbVertexFixedBytes + model::SkbWeightBytes * w;
				PutInt( b, at2, w );
				PutFloat( b, at2 + 4, 1.0f / weights );
				PutFloat( b, at2 + 8, 1.0f + s + i );      /* offset x */
				PutFloat( b, at2 + 12, 2.0f * w );         /* offset y */
			}
			v += model::SkbVertexFixedBytes + model::SkbWeightBytes * weights;
		}
		for ( int i = 0; i < kVerts[s]; ++i ) PutInt( b, v + 4 * i, i );
	}
	PutInt( b, 88, static_cast<std::int32_t>( b.size() ) );
	return b;
}

/* A version 3 skeletal animation of `frames` frames on two bones. Each frame
 * has its own bounds, radius and root offset, so a wrong frame shows. */
static Bytes SkeletalAnim( int frames ) {
	const std::size_t stride = pose::FrameHeaderBytes + 16 * 2;
	Bytes b( 108 + stride * frames, 0 );
	const std::uint32_t ident = pose::SkeletalAnimIdent;
	std::memcpy( b.data(), &ident, 4 );
	PutInt( b, 4, 3 );
	PutInt( b, 76, frames );
	PutInt( b, 80, 2 );
	PutFloat( b, 84, 0.05f * frames );
	PutFloat( b, 88, 0.05f );
	PutInt( b, 104, 108 );
	for ( int f = 0; f < frames; ++f ) {
		const std::size_t at = 108 + stride * f;
		for ( int k = 0; k < 3; ++k ) {
			PutFloat( b, at + 4 * k, -1.0f - f );
			PutFloat( b, at + 12 + 4 * k, 1.0f + f );
		}
		PutFloat( b, at + 24, 10.0f + f );                 /* radius */
		for ( int bone = 0; bone < 2; ++bone ) {
			const std::size_t p = at + pose::FrameHeaderBytes + 16 * bone;
			PutShort( b, p + 6, 32767 );                   /* identity rotation */
			PutShort( b, p + 8, static_cast<std::int16_t>( 63 * ( f + 1 ) * ( bone + 1 ) ) );
		}
	}
	return b;
}

/* ---- a vertex-animated mesh ---------------------------------------------- */

/* One triangle over `frames` frames. The reader's directory is filled by hand:
 * what is pinned here is how the renderer reads frames, not the file reader,
 * which tests/alice_tan_tests.cpp covers. */
static std::unique_ptr<File> Tan( int frames, float radiusBase ) {
	auto file = std::make_unique<File>();
	file->kind = File::Kind::Tan;
	const int verts = 3;
	const std::size_t frameData = 0;
	const std::size_t xyz = frameData + pose::TanFrameBytes * frames;
	const std::size_t st = xyz + 8u * verts * frames;
	const std::size_t tris = st + 8u * verts;
	const std::size_t collapse = tris + 12;
	file->bytes.assign( collapse + 4u * verts, 0 );
	Bytes &b = file->bytes;
	for ( int f = 0; f < frames; ++f ) {
		const std::size_t at = frameData + pose::TanFrameBytes * f;
		for ( int k = 0; k < 3; ++k ) {
			PutFloat( b, at + 4 * k, -1.0f - f );
			PutFloat( b, at + 12 + 4 * k, 1.0f + f );
			PutFloat( b, at + 24 + 4 * k, 1.0f );          /* scale */
			PutFloat( b, at + 36 + 4 * k, 100.0f * f );    /* offset: frame f is at 100 f */
		}
		PutFloat( b, at + 60, radiusBase + f );
		for ( int v = 0; v < verts; ++v ) {
			const std::size_t q = xyz + 8u * ( f * verts + v );
			const std::uint16_t coords[4] = { static_cast<std::uint16_t>( v ), 0, 0, 0 };
			std::memcpy( b.data() + q, coords, sizeof( coords ) );
		}
	}
	PutInt( b, tris, 0 );
	PutInt( b, tris + 4, 1 );
	PutInt( b, tris + 8, 2 );
	for ( int v = 0; v < verts; ++v ) PutInt( b, collapse + 4 * v, v );
	file->tan.frames = frames;
	file->tan.surfaces = 1;
	file->tan.frameData = frameData;
	auto &s = file->tan.surface[0];
	s.verts = verts;
	s.triangles = 1;
	s.minLod = verts;
	s.xyzNormalData = xyz;
	s.texCoordData = st;
	s.triangleData = tris;
	s.collapseData = collapse;
	return file;
}

static Animation Anim( const File *file ) {
	Animation a{};
	a.file = file;
	return a;
}

/* ---- the lookup the chain walk used to make ------------------------------ */

/* The walk the renderer made before, kept here as the reference: from the
 * first surface, follow each surface's offset to the next. */
struct Walked {
	bool found = false;
	int triangles = 0, verts = 0, minLod = 0;
	std::size_t triangleData = 0, vertexData = 0, collapseData = 0;
};

static Walked Walk( const File &f, int index ) {
	Walked out;
	const unsigned char *d = f.bytes.data();
	const std::size_t size = f.bytes.size();
	std::size_t at = f.skeleton.ofsSurfaces;
	for ( int i = 0; i <= index; ++i ) {
		if ( at + 96 > size ) return out;
		std::int32_t v[7];
		std::memcpy( v, d + at + 68, sizeof( v ) );
		if ( i == index ) {
			out.found = true;
			out.triangles = v[0];
			out.verts = v[1];
			out.minLod = v[2];
			out.triangleData = at + v[3];
			out.vertexData = at + v[4];
			out.collapseData = at + v[5];
			return out;
		}
		if ( v[6] <= 0 ) return out;
		at += static_cast<std::size_t>( v[6] );
	}
	return out;
}

/* The engine's own steps for a skeleton (alice_tiki.cpp LoadFile); null for
 * one the engine refuses. */
static std::unique_ptr<File> TryLoadSkeleton( const Bytes &bytes ) {
	auto file = std::make_unique<File>();
	file->bytes = bytes;
	if ( !pose::OpenSkeleton( file->bytes.data(), file->bytes.size(), file->skeleton ) ) return nullptr;
	auto skb = std::make_unique<model::Skb>();
	if ( model::ParseSkb( file->bytes.data(), file->bytes.size(), *skb ) != model::SkbError::None ) return nullptr;
	file->skbSurfaces.assign( skb->surface, skb->surface + skb->surfaces );
	file->kind = File::Kind::Skeleton;
	return file;
}

static std::unique_ptr<File> LoadSkeleton( const Bytes &bytes ) {
	auto file = TryLoadSkeleton( bytes );
	CHECK( file );
	return file;
}

static std::unique_ptr<File> LoadSkeletalAnim( const Bytes &bytes ) {
	auto file = std::make_unique<File>();
	file->bytes = bytes;
	CHECK( pose::OpenSkeletalAnim( file->bytes.data(), file->bytes.size(), file->anim ) );
	file->kind = File::Kind::SkeletalAnim;
	return file;
}

static aliceRenderEntity_t Entity() {
	aliceRenderEntity_t e;
	std::memset( &e, 0, sizeof( e ) );
	e.crossblendLerp = -1.0f;
	e.torsoCrossblendLerp = -1.0f;
	return e;
}

static void CheckSkeletalSurfaces() {
	const auto skeleton = LoadSkeleton( Skeleton() );
	const auto walk = LoadSkeletalAnim( SkeletalAnim( 2 ) );
	Model m{};
	m.skeleton = skeleton.get();
	m.scale = 1.0f;
	m.animations.push_back( Anim( walk.get() ) );

	/* Every index, in range or not, answers what the walk answered. */
	for ( int s = -2; s <= kSurfaces + 2; ++s ) {
		const Walked w = Walk( *skeleton, s );
		int verts = -1, indexes = -1;
		const int ok = RA_TikiSurfaceSize( &m, s, &verts, &indexes );
		CHECK( ok == ( w.found ? 1 : 0 ) );
		CHECK( verts == ( w.found ? w.verts : 0 ) );
		CHECK( indexes == ( w.found ? w.triangles * 3 : 0 ) );
		if ( s >= 0 && s < kSurfaces ) {
			const model::SkbSurface &r = skeleton->skbSurfaces[s];
			CHECK( r.triangleData == w.triangleData && r.vertexData == w.vertexData );
			CHECK( r.collapseData == w.collapseData && r.minLod == w.minLod );
		}
	}

	/* The whole model tessellates as it did: vertex i of surface s sits at
	 * the weighted offsets the fixture wrote, and the indexes are the
	 * triangles', moved by the first vertex. */
	aliceRenderEntity_t e = Entity();
	e.useLegs = qtrue;
	const void *bones = RA_TikiPose( &m, &e );
	CHECK( bones );
	for ( int s = 0; s < kSurfaces; ++s ) {
		float xyz[8][4], normal[8][4], st[8][2];
		unsigned int indexes[16];
		int verts = 0, count = 0;
		CHECK( RA_TikiTessellate( &m, s, &e, bones, xyz, normal, st, indexes, 10, 8, 16, &verts, &count ) );
		CHECK( verts == kVerts[s] && count == 3 * kTris[s] );
		for ( int t = 0; t < kTris[s]; ++t ) {
			CHECK( indexes[3 * t] == 10u && indexes[3 * t + 1] == 11u + t && indexes[3 * t + 2] == 12u + t );
		}
		for ( int i = 0; i < kVerts[s]; ++i ) CHECK( std::fabs( st[i][0] - 0.25f * i ) < 1e-6f );
	}

	/* The records are what the renderer reads: with the chain broken after
	 * the first surface, a walk finds nothing past it, and the lookup still
	 * finds every surface. */
	File broken = *skeleton;
	PutInt( broken.bytes, broken.skbSurfaces[0].header + 92, 0 );
	CHECK( !Walk( broken, 2 ).found );
	Model mb{};
	mb.skeleton = &broken;
	mb.scale = 1.0f;
	int verts = 0, indexes = 0;
	CHECK( RA_TikiSurfaceSize( &mb, 2, &verts, &indexes ) && verts == kVerts[2] && indexes == 3 * kTris[2] );
}

/* Every skeleton a legitimate installation ships, when one is present: the
 * records answer every surface index as the walk did. Nothing is copied. */
static void CheckInstalledSkeletons() {
	int skeletons = 0, surfaces = 0, refused = 0;
	for ( const char *pakName : { "pak0.pk3", "pak1_large.pk3", "pak1_small.pk3", "Pak2.pk3", "Pak3.pk3" } ) {
		fntech3_tests::AlicePak pak( pakName );
		if ( !pak.Available() ) continue;
		for ( const auto &entry : pak.Entries() ) {
			if ( !fntech3_tests::EndsWith( fntech3_tests::Lower( entry.name ), ".skb" ) ) continue;
			std::string data;
			CHECK( pak.Read( entry, data ) );
			const auto skeleton = TryLoadSkeleton( Bytes( data.begin(), data.end() ) );
			if ( !skeleton ) {
				++refused;   /* the engine refuses it too; nothing to draw */
				continue;
			}
			Model m{};
			m.skeleton = skeleton.get();
			m.scale = 1.0f;
			for ( int s = -1; s <= skeleton->skeleton.surfaces + 1; ++s ) {
				const Walked w = Walk( *skeleton, s );
				int verts = -1, indexes = -1;
				const int ok = RA_TikiSurfaceSize( &m, s, &verts, &indexes );
				CHECK( ( ok != 0 ) == ( w.found && w.verts > 0 && w.triangles > 0 ) );
				CHECK( verts == ( w.found ? w.verts : 0 ) && indexes == ( w.found ? w.triangles * 3 : 0 ) );
				if ( w.found ) {
					const model::SkbSurface &r = skeleton->skbSurfaces[s];
					CHECK( r.triangleData == w.triangleData && r.vertexData == w.vertexData );
					CHECK( r.collapseData == w.collapseData && r.minLod == w.minLod );
					++surfaces;
				}
			}
			++skeletons;
		}
	}
	if ( skeletons ) {
		std::cout << "installed skeletons: " << skeletons << ", surfaces: " << surfaces
			<< ", refused by the reader: " << refused << "\n";
	} else {
		std::cout << "no Alice installation; installed skeletons skipped\n";
	}
}

/* ---- retail's corrections -------------------------------------------------- */

static void CheckVertexCorrections() {
	const auto three = Tan( 3, 10.0f ), five = Tan( 5, 20.0f );
	Model m{};
	m.scale = 1.0f;
	m.animations.push_back( Anim( three.get() ) );
	m.animations.push_back( Anim( five.get() ) );
	const char *name = "models/test/mesh.tik";

	/* In range: nothing said, nothing changed. */
	aliceRenderEntity_t e = Entity();
	e.anim = 1; e.frame = 4; e.oldanim = 0; e.oldframe = 2;
	reports.clear();
	RA_TikiCheckAnimation( &m, &e, name, Capture );
	CHECK( reports.empty() && e.anim == 1 && e.frame == 4 && e.oldanim == 0 && e.oldframe == 2 );

	/* A frame past the animation's last is frame 0, not its last. */
	e = Entity();
	e.anim = 0; e.frame = 7;
	reports.clear();
	RA_TikiCheckAnimation( &m, &e, name, Capture );
	CHECK( e.frame == 0 && e.anim == 0 );
	CHECK( reports.size() == 1 && reports[0].first == PRINT_DEVELOPER );
	CHECK( reports[0].second == "R_AddTikiSurfaces: no such frame 7 in anim 0 for 'models/test/mesh.tik'\n" );

	/* So is a negative one. */
	e = Entity();
	e.anim = 1; e.frame = -1;
	reports.clear();
	RA_TikiCheckAnimation( &m, &e, name, Capture );
	CHECK( e.frame == 0 && reports.size() == 1 );
	CHECK( reports[0].second == "R_AddTikiSurfaces: no such frame -1 in anim 1 for 'models/test/mesh.tik'\n" );

	/* An animation the model does not have is its first, at frame 0. */
	e = Entity();
	e.anim = 5; e.frame = 2;
	reports.clear();
	RA_TikiCheckAnimation( &m, &e, name, Capture );
	CHECK( e.anim == 0 && e.frame == 0 && reports.size() == 1 );
	CHECK( reports[0].second == "R_AddTikiSurfaces: no such anim 5 for 'models/test/mesh.tik'\n" );

	/* A bad old animation: retail's message names the current animation. */
	e = Entity();
	e.anim = 1; e.frame = 3; e.oldanim = -1; e.oldframe = 2;
	reports.clear();
	RA_TikiCheckAnimation( &m, &e, name, Capture );
	CHECK( e.oldanim == 0 && e.oldframe == 0 && reports.size() == 1 );
	CHECK( reports[0].second == "R_AddTikiSurfaces: no such oldanim 1 for 'models/test/mesh.tik'\n" );

	/* An old frame is checked against the old animation. */
	e = Entity();
	e.anim = 1; e.frame = 4; e.oldanim = 0; e.oldframe = 4;
	reports.clear();
	RA_TikiCheckAnimation( &m, &e, name, Capture );
	CHECK( e.frame == 4 && e.oldframe == 0 && reports.size() == 1 );
	CHECK( reports[0].second == "R_AddTikiSurfaces: no such oldframe 4 in anim 0 for 'models/test/mesh.tik'\n" );

	/* Without a printer the correction still happens. */
	e = Entity();
	e.anim = 0; e.frame = 3;
	RA_TikiCheckAnimation( &m, &e, name, nullptr );
	CHECK( e.frame == 0 );

	/* An animation whose file the image names but the installation lacks is
	 * left alone; retail stops drawing there instead of checking. */
	Model missing{};
	missing.scale = 1.0f;
	missing.animations.push_back( Anim( three.get() ) );
	missing.animations.push_back( Anim( nullptr ) );
	e = Entity();
	e.anim = 1; e.frame = 9; e.oldanim = 7; e.oldframe = 9;
	reports.clear();
	RA_TikiCheckAnimation( &missing, &e, name, Capture );
	CHECK( e.anim == 1 && e.frame == 9 && e.oldanim == 7 && reports.empty() );

	/* What the renderer reads for an entity nobody corrected is frame 0 as
	 * well: its level-of-detail sphere, its lighting point and its vertices. */
	aliceRenderEntity_t past = Entity(), first = Entity();
	past.anim = first.anim = 1;
	past.frame = 99;
	past.oldanim = first.oldanim = 1;
	past.oldframe = 99;
	float c0[3], c1[3], r0 = 0.0f, r1 = 0.0f;
	CHECK( RA_TikiLodSphere( &m, &first, c0, &r0 ) && RA_TikiLodSphere( &m, &past, c1, &r1 ) );
	CHECK( r0 == 20.0f && r1 == 20.0f );
	float p0[3], p1[3];
	CHECK( RA_TikiLightingPoint( &m, &first, p0 ) && RA_TikiLightingPoint( &m, &past, p1 ) );
	CHECK( std::memcmp( p0, p1, sizeof( p0 ) ) == 0 );
	past.backlerp = first.backlerp = 0.5f;
	float x0[3][4], x1[3][4], n[3][4], st[3][2];
	unsigned int idx[3];
	int nv = 0, ni = 0;
	CHECK( RA_TikiTessellate( &m, 0, &first, nullptr, x0, n, st, idx, 0, 3, 3, &nv, &ni ) );
	CHECK( RA_TikiTessellate( &m, 0, &past, nullptr, x1, n, st, idx, 0, 3, 3, &nv, &ni ) );
	CHECK( std::memcmp( x0, x1, sizeof( x0 ) ) == 0 );
	/* frame 0 of animation 1 has its offset at 0; the last frame's is 400 */
	CHECK( x1[0][0] == 0.0f && x1[2][0] == 2.0f );
}

static void CheckSkeletalCorrections() {
	const auto skeleton = LoadSkeleton( Skeleton() );
	const auto three = LoadSkeletalAnim( SkeletalAnim( 3 ) );
	const auto four = LoadSkeletalAnim( SkeletalAnim( 4 ) );
	Model m{};
	m.skeleton = skeleton.get();
	m.scale = 1.0f;
	m.animations.push_back( Anim( three.get() ) );
	m.animations.push_back( Anim( four.get() ) );
	const char *name = "models/test/skel.tik";

	/* The legs' pairs only while the legs animation is in use. */
	aliceRenderEntity_t e = Entity();
	e.anim = 1; e.frame = 9; e.oldanim = 3; e.oldframe = 1;
	e.torsoAnim = 8; e.torsoFrame = 8;
	reports.clear();
	RA_TikiCheckAnimation( &m, &e, name, Capture );
	CHECK( reports.empty() && e.frame == 9 && e.oldanim == 3 && e.torsoAnim == 8 );

	e.useLegs = qtrue;
	reports.clear();
	RA_TikiCheckAnimation( &m, &e, name, Capture );
	CHECK( e.anim == 1 && e.frame == 0 && e.oldanim == 0 && e.oldframe == 0 );
	CHECK( e.torsoAnim == 8 && e.torsoFrame == 8 );
	CHECK( reports.size() == 2 && reports[0].first == PRINT_DEVELOPER && reports[1].first == PRINT_DEVELOPER );
	CHECK( reports[0].second == "R_AddSkelSurfaces: no such frame 9 in anim 1 for 'models/test/skel.tik'\n" );
	CHECK( reports[1].second == "R_AddSkelSurfaces: no such anim 3 for 'models/test/skel.tik'\n" );

	/* The torso's three pairs, its cross-blend whatever its weight. */
	e = Entity();
	e.useTorso = qtrue;
	e.torsoAnim = 0; e.torsoFrame = 3;
	e.torsoOldanim = 1; e.torsoOldframe = 3;
	e.torsoCrossblendAnim = -4; e.torsoCrossblendFrame = 2;
	e.crossblendAnim = 9; e.crossblendFrame = 9; e.crossblendLerp = 0.5f;
	reports.clear();
	RA_TikiCheckAnimation( &m, &e, name, Capture );
	CHECK( e.torsoFrame == 0 && e.torsoOldframe == 3 );
	CHECK( e.torsoCrossblendAnim == 0 && e.torsoCrossblendFrame == 0 );
	CHECK( e.crossblendAnim == 9 && e.crossblendFrame == 9 );  /* legs not in use */
	CHECK( reports.size() == 2 );
	CHECK( reports[0].second == "R_AddSkelSurfaces: no such frame 3 in anim 0 for 'models/test/skel.tik'\n" );
	CHECK( reports[1].second == "R_AddSkelSurfaces: no such anim -4 for 'models/test/skel.tik'\n" );

	/* The legs' cross-blend only with a weight that is not negative. */
	e = Entity();
	e.useLegs = qtrue;
	e.crossblendAnim = 1; e.crossblendFrame = 6; e.crossblendLerp = -1.0f;
	reports.clear();
	RA_TikiCheckAnimation( &m, &e, name, Capture );
	CHECK( e.crossblendFrame == 6 && reports.empty() );
	e.crossblendLerp = 0.0f;
	RA_TikiCheckAnimation( &m, &e, name, Capture );
	CHECK( e.crossblendFrame == 0 && reports.size() == 1 );
	CHECK( reports[0].second == "R_AddSkelSurfaces: no such frame 6 in anim 1 for 'models/test/skel.tik'\n" );

	/* Uncorrected, the renderer still reads frame 0, not the last. */
	aliceRenderEntity_t past = Entity(), first = Entity();
	past.useLegs = first.useLegs = qtrue;
	past.anim = first.anim = 1;
	past.frame = 40;
	float c[3], r0 = 0.0f, r1 = 0.0f;
	CHECK( RA_TikiLodSphere( &m, &first, c, &r0 ) && RA_TikiLodSphere( &m, &past, c, &r1 ) );
	CHECK( r0 == 10.0f && r1 == 10.0f );
	const auto *b0 = static_cast<const pose::BoneTransform *>( RA_TikiPose( &m, &first ) );
	const auto *b1 = static_cast<const pose::BoneTransform *>( RA_TikiPose( &m, &past ) );
	CHECK( b0 && b1 && std::memcmp( b0, b1, 2 * sizeof( *b0 ) ) == 0 );
	aliceRenderEntity_t last = first;
	last.frame = 3;
	const auto *b2 = static_cast<const pose::BoneTransform *>( RA_TikiPose( &m, &last ) );
	CHECK( b2 && std::memcmp( b0, b2, 2 * sizeof( *b0 ) ) != 0 );
	RA_TikiBeginFrame();
}

/* ---- what the frame's store keys a surface by ---------------------------- */

/* The renderers keep each surface they tessellate for the frame's later
 * passes, keyed by the surface, the pose, the frames and lerp, the scale and
 * the detail (code/renderercommon/tr_alice_tiki_cache.h). That holds only if
 * nothing else an entity says reaches the triangles, if each pose made in a
 * frame is a skeleton of its own, and if the poses' generation moves on with
 * the frame. */
static aliceRenderEntity_t Unkeyed( aliceRenderEntity_t e ) {
	e.torsoAnim = 0; e.torsoFrame = 1; e.torsoOldanim = 0; e.torsoOldframe = 1;
	e.torsoBacklerp = 0.5f; e.useTorso = qtrue;
	e.crossblendAnim = 0; e.crossblendFrame = 1; e.crossblendLerp = 0.5f;
	e.useControllers = qtrue;
	e.controllers[0].bone = 0;
	e.controllers[0].quat[3] = 1.0f;
	e.controllers[0].scale = 2.0f;
	e.customShaderPass = qtrue;
	e.renderfx = 0x7fff;
	e.surfaces[0] = 0x46;
	e.shaderData[0] = 12;
	e.bones = &kSurfaces;
	return e;
}

static bool SameTriangles( const Model &m, int s, const aliceRenderEntity_t &a, const aliceRenderEntity_t &b,
	const void *bones, float lod ) {
	float xa[8][4] = {}, na[8][4] = {}, sa[8][2] = {}, xb[8][4] = {}, nb[8][4] = {}, sb[8][2] = {};
	unsigned int ia[16] = {}, ib[16] = {};
	int va = 0, ca = 0, vb = 0, cb = 0;
	CHECK( RA_TikiTessellateLod( &m, s, &a, bones, lod, xa, na, sa, ia, 5, 8, 16, &va, &ca ) );
	CHECK( RA_TikiTessellateLod( &m, s, &b, bones, lod, xb, nb, sb, ib, 5, 8, 16, &vb, &cb ) );
	return va == vb && ca == cb && std::memcmp( xa, xb, sizeof( xa ) ) == 0 &&
		std::memcmp( na, nb, sizeof( na ) ) == 0 && std::memcmp( sa, sb, sizeof( sa ) ) == 0 &&
		std::memcmp( ia, ib, sizeof( ia ) ) == 0;
}

static void CheckTessellationKey() {
	const auto skeleton = LoadSkeleton( Skeleton() );
	const auto walk = LoadSkeletalAnim( SkeletalAnim( 2 ) );
	Model m{};
	m.skeleton = skeleton.get();
	m.scale = 1.0f;
	m.animations.push_back( Anim( walk.get() ) );

	RA_TikiBeginFrame();
	const unsigned int generation = RA_TikiPoseGeneration();
	CHECK( generation != 0u );
	aliceRenderEntity_t e = Entity();
	e.useLegs = qtrue;
	const void *bones = RA_TikiPose( &m, &e );
	const void *again = RA_TikiPose( &m, &e );
	CHECK( bones && again && again != bones );
	/* A skeletal surface reads its pose from the skeleton alone. */
	for ( int s = 0; s < kSurfaces; ++s ) {
		CHECK( SameTriangles( m, s, e, Unkeyed( e ), bones, 1.0f ) );
	}
	aliceRenderEntity_t scaled = e;
	scaled.scale = 2.0f;
	CHECK( !SameTriangles( m, 0, e, scaled, bones, 1.0f ) );
	CHECK( RA_TikiPoseGeneration() == generation );
	RA_TikiBeginFrame();
	CHECK( RA_TikiPoseGeneration() != generation && RA_TikiPoseGeneration() != 0u );

	/* A vertex-animated surface reads its frames, its lerp and its scale. */
	const auto three = Tan( 3, 10.0f ), five = Tan( 5, 20.0f );
	Model v{};
	v.scale = 1.0f;
	v.animations.push_back( Anim( three.get() ) );
	v.animations.push_back( Anim( five.get() ) );
	aliceRenderEntity_t f = Entity();
	f.anim = 1; f.frame = 4; f.oldanim = 0; f.oldframe = 2; f.backlerp = 0.25f;
	CHECK( SameTriangles( v, 0, f, Unkeyed( f ), nullptr, 1.0f ) );
	aliceRenderEntity_t g = f;
	g.frame = 3;
	CHECK( !SameTriangles( v, 0, f, g, nullptr, 1.0f ) );
	g = f;
	g.oldframe = 1;
	CHECK( !SameTriangles( v, 0, f, g, nullptr, 1.0f ) );
	g = f;
	g.backlerp = 0.5f;
	CHECK( !SameTriangles( v, 0, f, g, nullptr, 1.0f ) );
	g = f;
	g.scale = 2.0f;
	CHECK( !SameTriangles( v, 0, f, g, nullptr, 1.0f ) );
	RA_TikiBeginFrame();
}

int main() {
	CheckSkeletalSurfaces();
	CheckInstalledSkeletons();
	CheckVertexCorrections();
	CheckSkeletalCorrections();
	CheckTessellationKey();
	std::cout << "alice tiki render tests passed\n";
	return 0;
}
