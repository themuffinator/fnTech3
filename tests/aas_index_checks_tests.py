"""A bot navigation file's loaders refuse a number that leaves its table before following it.

The botlib's Quake III loader, which reads the navigation files of Quake III,
Team Arena, Quake Live and Elite Force (EAAS versions 4 and 5), used to swap a
file's bytes and hand its tables to every caller as they were. Nothing checked
that a node's plane or children, an area's faces, a face's edges and plane, an
edge's vertexes, an area's reachabilities and cluster, a portal's area and
clusters or a cluster's portals stayed inside their tables. The point query
walks node children from node 1 with its bounds checks compiled out, so a child
out of range read past the nodes and a cycle never returned; routing indexes
its caches by the numbers clusters give their areas; and a negative lump
length wrapped into a huge record count. RTCW's loader already refused most of
this in its own reader, but not through dummy records (face 0, area 0,
portal 0), the face index 0 no face carries, or portals a cluster lists that
are not on its border.

Both loaders now check every such number with code/botlib/aas_index_checks.hpp
before the file is marked loaded, and walk the node tree from node 1 once,
refusing a node reached twice; Quake III's loader also refuses a lump outside
the file, and reachability ranges that would overflow its routing tables. A
refused file is reported as the botlib reports a load error, and the map runs
without bots. Every installed navigation file passes
(docs/fntech3/2026-09-28-aas-index-checks.md).

The fixtures compile the engine's own checks and loaders and feed them small
synthetic files; no retail file is read. Tables and scratch sit flush against
pages nothing may read, so a check that read past a table would crash the
fixture rather than pass it.
"""
from pathlib import Path
import os
import shutil
import struct
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
import rtcw_aas_load_tests
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

# A navigation file of two clusters joined by one portal area, in the field
# order of code/botlib/aasfile.h. Record 0 of each table is a dummy, as the
# compiler writes it. Area 1 is in cluster 1, area 3 in cluster 2, and area 2
# is portal 1 between them, numbered 1 in both. The tree: node 1 splits into
# node 2 (area 1 and solid) and node 3 (areas 2 and 3).
WORLD = dict(
    bboxes=[(2, 0, (-15, -15, -24), (15, 15, 32))],
    vertexes=[(0, 0, 0), (0, 0, 64), (64, 0, 64), (64, 0, 0), (64, 64, 0)],
    planes=[((1, 0, 0), 32, 0), ((-1, 0, 0), -32, 0), ((0, 1, 0), 32, 1), ((0, -1, 0), -32, 1)],
    edges=[(0, 0), (1, 2), (2, 3), (3, 4)],
    edgeindex=[1, -2, 3],
    faces=[(0, 0, 0, 0, 0, 0), (0, 0, 2, 0, 1, 2), (2, 0, 1, 2, 2, 0)],
    faceindex=[1, -1, 2],
    areas=[(0, 0, 0, (0, 0, 0), (0, 0, 0), (0, 0, 0)),
           (1, 1, 0, (0, 0, 0), (32, 64, 64), (16, 32, 32)),
           (2, 2, 1, (32, 0, 0), (64, 32, 64), (48, 16, 32)),
           (3, 0, 3, (32, 32, 0), (64, 64, 64), (48, 48, 32))],
    areasettings=[(0, 0, 0, 0, 0, 0, 1), (0, 1, 2, 1, 0, 1, 1), (0, 1, 2, -1, 0, 1, 2), (0, 1, 2, 2, 0, 1, 3)],
    reachability=[(0, 0, 0, (0, 0, 0), (0, 0, 0), 0, 0),
                  (2, 0, 0, (16, 32, 0), (48, 16, 0), 2, 100),
                  (3, 0, 0, (48, 16, 0), (48, 48, 0), 2, 100),
                  (2, 0, 0, (48, 48, 0), (48, 16, 0), 2, 100)],
    nodes=[(0, (0, 0)), (0, (2, 3)), (2, (-1, 0)), (2, (-2, -3))],
    portals=[(0, 0, 0, (0, 0)), (2, 1, 2, (1, 1))],
    portalindex=[1, 1],
    clusters=[(0, 0, 0, 0), (2, 2, 1, 0), (2, 2, 1, 1)],
)
LUMPS = ('bboxes', 'vertexes', 'planes', 'edges', 'edgeindex', 'faces', 'faceindex', 'areas', 'areasettings',
         'reachability', 'nodes', 'portals', 'portalindex', 'clusters')
FORMATS = dict(bboxes='<2i6f', vertexes='<3f', planes='<4fi', edges='<2i', edgeindex='<i', faces='<6i',
               faceindex='<i', areas='<3i9f', areasettings='<7i', reachability='<3i6fiH2x', nodes='<3i',
               portals='<5i', portalindex='<i', clusters='<4i')
CHECKSUM = 0xcafebabe  # the fixtures' sv_mapChecksum, -889275714


def flat(record):
    if isinstance(record, (tuple, list)):
        return [value for part in record for value in flat(part)]
    return [record]


def cxx(record):
    if isinstance(record, (tuple, list)):
        return '{' + ','.join(cxx(part) for part in record) + '}'
    return repr(record)


def encode(world, version=5, lengths=None):
    """The file the compiler would write: RTCW's version 8 adds each area's ground steepness."""
    lumps = []
    for name in LUMPS:
        form = FORMATS[name]
        records = world[name]
        if name == 'areasettings' and version == 8:
            form, records = '<7if', [tuple(record) + (0.5,) for record in records]
        lumps.append(b''.join(struct.pack(form, *flat(record)) for record in records))
    header = bytearray(struct.pack('<4sII', b'EAAS', version, CHECKSUM))
    offset = 124
    for i, lump in enumerate(lumps):
        length = len(lump) if lengths is None or i not in lengths else lengths[i]
        header += struct.pack('<ii', offset, length)
        offset += len(lump)
    if version != 4:
        for i in range(8, 124):
            header[i] ^= ((i - 8) * 119) & 255
    return bytes(header) + b''.join(lumps)


def changed(**tables):
    world = {name: list(records) for name, records in WORLD.items()}
    for name, edits in tables.items():
        for index, record in edits.items():
            if index == 'size':
                world[name] = world[name][:record]
            else:
                world[name][index] = record
    return world


FENCE = r'''
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <cstddef>
#include <cstdlib>

/* `bytes` ending where a page nothing may touch begins */
extern "C" void *FencedAlloc( std::size_t bytes ) {
#ifdef _WIN32
	SYSTEM_INFO info;
	GetSystemInfo( &info );
	const std::size_t page = info.dwPageSize;
#else
	const std::size_t page = (std::size_t)sysconf( _SC_PAGESIZE );
#endif
	const std::size_t total = ( bytes + page - 1 ) / page * page + page;
#ifdef _WIN32
	char *base = (char *)VirtualAlloc( nullptr, total, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
	DWORD old;
	if ( !base || !VirtualProtect( base + total - page, page, PAGE_NOACCESS, &old ) ) abort();
#else
	char *base = (char *)mmap( nullptr, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
	if ( base == MAP_FAILED || mprotect( base + total - page, page, PROT_NONE ) ) abort();
#endif
	return base + total - page - bytes;
}
'''

CHECKER_PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/botlib/aasfile.h"
#include "code/botlib/aas_index_checks.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" void *FencedAlloc( std::size_t bytes );

/* A table copied to the end of its own fenced block: reading past it faults. */
template <class T> static T *Fence( const std::vector<T> &table ) {
	T *out = (T *)FencedAlloc( table.size() * sizeof( T ) );
	if ( !table.empty() ) memcpy( out, table.data(), table.size() * sizeof( T ) );
	return out;
}

/* The tables a loader hands the checks, under the names of aas_t. */
struct World {
	int numvertexes, numplanes;
	aas_edge_t *edges; int numedges;
	aas_edgeindex_t *edgeindex; int edgeindexsize;
	aas_face_t *faces; int numfaces;
	aas_faceindex_t *faceindex; int faceindexsize;
	aas_area_t *areas; int numareas;
	aas_areasettings_t *areasettings; int numareasettings;
	aas_reachability_t *reachability; int reachabilitysize;
	aas_node_t *nodes; int numnodes;
	aas_portal_t *portals; int numportals;
	aas_portalindex_t *portalindex; int portalindexsize;
	aas_cluster_t *clusters; int numclusters;
};

struct Tables {
	int numvertexes, numplanes;
	std::vector<aas_edge_t> edges;
	std::vector<aas_edgeindex_t> edgeindex;
	std::vector<aas_face_t> faces;
	std::vector<aas_faceindex_t> faceindex;
	std::vector<aas_area_t> areas;
	std::vector<aas_areasettings_t> areasettings;
	std::vector<aas_reachability_t> reachability;
	std::vector<aas_node_t> nodes;
	std::vector<aas_portal_t> portals;
	std::vector<aas_portalindex_t> portalindex;
	std::vector<aas_cluster_t> clusters;
};

static World Fenced( const Tables &t ) {
	World w;
	w.numvertexes = t.numvertexes; w.numplanes = t.numplanes;
	w.edges = Fence( t.edges ); w.numedges = (int)t.edges.size();
	w.edgeindex = Fence( t.edgeindex ); w.edgeindexsize = (int)t.edgeindex.size();
	w.faces = Fence( t.faces ); w.numfaces = (int)t.faces.size();
	w.faceindex = Fence( t.faceindex ); w.faceindexsize = (int)t.faceindex.size();
	w.areas = Fence( t.areas ); w.numareas = (int)t.areas.size();
	w.areasettings = Fence( t.areasettings ); w.numareasettings = (int)t.areasettings.size();
	w.reachability = Fence( t.reachability ); w.reachabilitysize = (int)t.reachability.size();
	w.nodes = Fence( t.nodes ); w.numnodes = (int)t.nodes.size();
	w.portals = Fence( t.portals ); w.numportals = (int)t.portals.size();
	w.portalindex = Fence( t.portalindex ); w.portalindexsize = (int)t.portalindex.size();
	w.clusters = Fence( t.clusters ); w.numclusters = (int)t.clusters.size();
	return w;
}

static int failures;

/* The checks over fenced tables, in fenced scratch of exactly the size they
 * ask for, first filled with a pattern they must not rely on. */
static aasIndexFault_t Check( const World &w, bool quakeRouting, char *text, size_t textSize ) {
	const size_t size = AAS_IndexScratchSize( w );
	unsigned char *scratch = (unsigned char *)FencedAlloc( size );
	memset( scratch, 0xa5, size );
	return AAS_IndexFault( w, quakeRouting, scratch, text, textSize );
}

static void Expect( const Tables &t, bool quakeRouting, aasIndexFault_t fault, const char *text, int line ) {
	char words[256] = "unset";
	const aasIndexFault_t found = Check( Fenced( t ), quakeRouting, words, sizeof( words ) );
	if ( found != fault || strcmp( words, text ) ) {
		fprintf( stderr, "line %d (%s routing): expected %d \"%s\", found %d \"%s\"\n", line,
			quakeRouting ? "Quake" : "RTCW", fault, text, found, words );
		failures++;
	}
}

/* Both loaders refuse it alike, or both load it. */
#define EXPECT( t, fault, text ) ( Expect( t, true, fault, text, __LINE__ ), Expect( t, false, fault, text, __LINE__ ) )
/* Only Quake III's routing tables have this limit. */
#define EXPECT_QUAKE( t, fault, text ) ( Expect( t, true, fault, text, __LINE__ ), Expect( t, false, AAS_INDEX_VALID, "", __LINE__ ) )
'''

HELPER_CHECKS = r'''
int main() {
	/* Empty and whole ranges fit, even at the end of a table or of an empty one;
	 * nothing negative, past the end, or whose end would overflow does. */
	assert( AAS_RangeInside( 0, 0, 0 ) && AAS_RangeInside( 5, 0, 5 ) && AAS_RangeInside( 0, 5, 5 ) && AAS_RangeInside( 2, 3, 5 ) );
	assert( !AAS_RangeInside( -1, 0, 5 ) && !AAS_RangeInside( 0, -1, 5 ) && !AAS_RangeInside( 6, 0, 5 ) );
	assert( !AAS_RangeInside( 3, 3, 5 ) && !AAS_RangeInside( INT_MAX, INT_MAX, INT_MAX ) && !AAS_RangeInside( 1, INT_MAX, INT_MAX ) );
	assert( AAS_IndexInside( 0, 1 ) && AAS_IndexInside( 4, 5 ) );
	assert( !AAS_IndexInside( 5, 5 ) && !AAS_IndexInside( -1, 5 ) && !AAS_IndexInside( 0, 0 ) && !AAS_IndexInside( INT_MIN, INT_MAX ) );
	/* A face or edge index names record |index| but never record 0. */
	assert( AAS_SignedIndexInside( 1, 2 ) && AAS_SignedIndexInside( -1, 2 ) && AAS_SignedIndexInside( -4, 5 ) );
	assert( !AAS_SignedIndexInside( 0, 5 ) && !AAS_SignedIndexInside( 5, 5 ) && !AAS_SignedIndexInside( -5, 5 ) );
	assert( !AAS_SignedIndexInside( INT_MIN, INT_MAX ) && !AAS_SignedIndexInside( INT_MAX, INT_MAX ) );
	/* A child is a node, an area as -area, or 0 for solid. */
	assert( AAS_ChildInside( 0, 2, 1 ) && AAS_ChildInside( 1, 2, 1 ) && AAS_ChildInside( -1, 2, 2 ) && AAS_ChildInside( -3, 2, 4 ) );
	assert( !AAS_ChildInside( 2, 2, 4 ) && !AAS_ChildInside( -2, 2, 2 ) && !AAS_ChildInside( INT_MIN, 2, INT_MAX ) );
	assert( !AAS_ChildInside( INT_MAX, INT_MAX, 1 ) );
	return 0;
}
'''

TREE_CHECKS = r'''
/* Trees over four areas, in fenced tables and scratch. */
enum { AREAS = 4 };
static int faultNode, faultChild;

static aasTreeFault_t Walk( const std::vector<aas_node_t> &table ) {
	const size_t size = AAS_NodeTreeScratchSize( (int)table.size() );
	unsigned char *scratch = (unsigned char *)FencedAlloc( size );
	memset( scratch, 0xa5, size );
	return AAS_NodeTreeFault( Fence( table ), (int)table.size(), AREAS, scratch, &faultNode, &faultChild );
}

static void ExpectTree( const std::vector<aas_node_t> &table, aasTreeFault_t fault, int node, int child, int line ) {
	const aasTreeFault_t found = Walk( table );
	if ( found != fault || ( fault != AAS_TREE_VALID && ( faultNode != node || faultChild != child ) ) ) {
		fprintf( stderr, "line %d: expected fault %d at child %d of node %d, found %d at child %d of node %d\n",
			line, fault, child, node, found, faultChild, faultNode );
		failures++;
	}
}
#define EXPECT_TREE( table, fault, node, child ) ExpectTree( table, fault, node, child, __LINE__ )

/* Nodes 1 to length - 1 in a line, each over the next and area 1; the last
 * over areas 2 and 3. Node 0 is the solid dummy. */
static std::vector<aas_node_t> Line( int length ) {
	std::vector<aas_node_t> table( length );
	table[0] = aas_node_t{ 0, { 0, 0 } };
	for ( int i = 1; i < length; i++ ) {
		table[i] = aas_node_t{ 0, { i + 1 < length ? i + 1 : -2, i + 1 < length ? -1 : -3 } };
	}
	return table;
}

int main() {
	std::vector<aas_node_t> t;
	/* no tree at all: the index checks refuse it first */
	EXPECT_TREE( std::vector<aas_node_t>(), AAS_TREE_VALID, -1, -1 );
	EXPECT_TREE( Line( 1 ), AAS_TREE_VALID, -1, -1 );
	/* lines of any depth: no descent recurses, so none is too deep */
	EXPECT_TREE( Line( 2 ), AAS_TREE_VALID, -1, -1 );
	EXPECT_TREE( Line( 100001 ), AAS_TREE_VALID, -1, -1 );
	/* node 0 is never read, whatever it holds */
	t = Line( 4 ); t[0].children[0] = 1; t[0].children[1] = 99;
	EXPECT_TREE( t, AAS_TREE_VALID, -1, -1 );
	/* a child naming its own node, node 1, or a node far up a long line */
	t = Line( 4 ); t[2].children[1] = 2;
	EXPECT_TREE( t, AAS_TREE_CYCLE, 2, 2 );
	t = Line( 4 ); t[3].children[0] = 1;
	EXPECT_TREE( t, AAS_TREE_CYCLE, 3, 1 );
	t = Line( 1500 ); t[1499].children[1] = 750;
	EXPECT_TREE( t, AAS_TREE_CYCLE, 1499, 750 );
	t = Line( 100001 ); t[100000].children[0] = 2;
	EXPECT_TREE( t, AAS_TREE_CYCLE, 100000, 2 );
	/* both children of a node naming one node, and two nodes naming one */
	t = Line( 3 ); t[1].children[1] = 2;
	EXPECT_TREE( t, AAS_TREE_SHARED, 1, 2 );
	t.assign( 5, aas_node_t{ 0, { 0, 0 } } );
	t[1] = aas_node_t{ 0, { 2, 3 } }; t[2] = aas_node_t{ 0, { 4, -1 } }; t[3] = aas_node_t{ 0, { 4, -2 } };
	t[4] = aas_node_t{ 0, { -1, -3 } };
	EXPECT_TREE( t, AAS_TREE_SHARED, 3, 4 );
	/* 64 nodes each naming the next with both children: 2^63 paths, refused
	 * at the first share the walk meets, at the bottom */
	t = Line( 65 );
	for ( int i = 1; i < 64; i++ ) t[i].children[1] = i + 1;
	EXPECT_TREE( t, AAS_TREE_SHARED, 63, 64 );
	/* children outside the nodes and the areas */
	t = Line( 4 ); t[1].children[1] = 4;
	EXPECT_TREE( t, AAS_TREE_OUTSIDE, 1, 4 );
	t = Line( 4 ); t[3].children[1] = -AREAS;
	EXPECT_TREE( t, AAS_TREE_OUTSIDE, 3, -AREAS );
	t = Line( 4 ); t[2].children[1] = INT_MIN;
	EXPECT_TREE( t, AAS_TREE_OUTSIDE, 2, INT_MIN );
	/* several leaves naming one area, solid on both sides, nodes node 1 does
	 * not reach (one naming itself and node 1), children numbered first */
	t = Line( 3 ); t[1].children[1] = -2; t[2].children[0] = -2; t[2].children[1] = -2;
	EXPECT_TREE( t, AAS_TREE_VALID, -1, -1 );
	t = Line( 3 ); t[2].children[0] = 0; t[2].children[1] = 0;
	EXPECT_TREE( t, AAS_TREE_VALID, -1, -1 );
	t = Line( 5 ); t[4] = aas_node_t{ 0, { 4, 1 } }; t[3].children[0] = -1;
	EXPECT_TREE( t, AAS_TREE_VALID, -1, -1 );
	t.assign( 4, aas_node_t{ 0, { 0, 0 } } );
	t[1] = aas_node_t{ 0, { 3, -1 } }; t[3] = aas_node_t{ 0, { 2, -1 } }; t[2] = aas_node_t{ 0, { -2, -3 } };
	EXPECT_TREE( t, AAS_TREE_VALID, -1, -1 );
	/* a full tree of 65,535 nodes below node 1, walked whole, then a share
	 * and a cycle at its last nodes */
	t.assign( 65536, aas_node_t{ 0, { 0, 0 } } );
	for ( int i = 1; i < 32768; i++ ) t[i] = aas_node_t{ 0, { 2 * i, 2 * i + 1 } };
	for ( int i = 32768; i < 65536; i++ ) t[i] = aas_node_t{ 0, { -1, -2 } };
	EXPECT_TREE( t, AAS_TREE_VALID, -1, -1 );
	t[32767].children[1] = 65534;
	EXPECT_TREE( t, AAS_TREE_SHARED, 32767, 65534 );
	t[32767].children[1] = 65535; t[65535].children[0] = 32767;
	EXPECT_TREE( t, AAS_TREE_CYCLE, 65535, 32767 );
	return failures ? 1 : 0;
}
'''

INDEX_CHECKS = r'''
int main() {
	Tables t;
	/* the synthetic file, and the shape of every optimised retail file: no
	 * vertexes at all, and only the dummy edge and face, whose zeroes name a
	 * vertex no table holds, but which nothing can reach */
	EXPECT( Valid(), AAS_INDEX_VALID, "" );
	t = Valid(); t.numvertexes = 0; t.edges.resize( 1 ); t.edgeindex.clear(); t.faces.resize( 1 ); t.faceindex.clear();
	for ( auto &area : t.areas ) { area.firstface = 0; area.numfaces = 0; }
	EXPECT( t, AAS_INDEX_VALID, "" );
	/* dummies nothing reads may hold anything */
	t = Valid(); t.nodes[0] = aas_node_t{ 99, { 99, -99 } }; t.faces[0] = aas_face_t{ 99, 0, 99, 99, 99, 99 };
	t.edges[0] = aas_edge_t{ { 99, -99 } };
	EXPECT( t, AAS_INDEX_VALID, "" );
	/* RTCW gives area 0 reachabilities in five installed files */
	t = Valid(); t.areasettings[0].firstreachablearea = 1; t.areasettings[0].numreachableareas = 1;
	EXPECT( t, AAS_INDEX_VALID, "" );
	/* a portal back onto its own cluster takes one number there */
	t = Valid(); t.portals[1].backcluster = 1; t.clusters[1].numportals = 2; t.clusters[2].numportals = 0;
	t.clusters[2].firstportal = 2;
	EXPECT( t, AAS_INDEX_VALID, "" );
	/* a file with no clusters has its clusters built as it loads */
	t = Valid(); t.clusters.clear(); t.portals.clear(); t.portalindex.clear();
	for ( auto &settings : t.areasettings ) { settings.cluster = 0; settings.clusterareanum = 0; }
	EXPECT( t, AAS_INDEX_VALID, "" );

	/* the counts every descent and every paired plane relies on */
	t = Valid(); t.nodes.resize( 1 );
	EXPECT( t, AAS_INDEX_NO_ROOT, "has no root node" );
	t = Valid(); t.numplanes = 3;
	EXPECT( t, AAS_INDEX_PLANE_PAIR, "plane 2 has no opposite plane" );
	t = Valid(); t.areasettings.resize( 3 );
	EXPECT( t, AAS_INDEX_SETTINGS_COUNT, "has 3 area settings for 4 areas" );

	/* the tree */
	t = Valid(); t.nodes[2].planenum = 4;
	EXPECT( t, AAS_INDEX_NODE_PLANE, "node 2 has a bad plane" );
	t = Valid(); t.nodes[1].planenum = -1;
	EXPECT( t, AAS_INDEX_NODE_PLANE, "node 1 has a bad plane" );
	t = Valid(); t.nodes[3].children[1] = -4;
	EXPECT( t, AAS_INDEX_NODE_CHILD, "node 3 has a bad child" );
	t = Valid(); t.nodes[1].children[0] = 4;
	EXPECT( t, AAS_INDEX_NODE_CHILD, "node 1 has a bad child" );
	t = Valid(); t.nodes[2].children[1] = INT_MIN;
	EXPECT( t, AAS_INDEX_NODE_CHILD, "node 2 has a bad child" );
	/* a node node 1 does not reach is left out of the walk, but not out of the checks */
	t = Valid(); t.nodes.push_back( aas_node_t{ 0, { 99, -1 } } );
	EXPECT( t, AAS_INDEX_NODE_CHILD, "node 4 has a bad child" );
	t = Valid(); t.nodes.push_back( aas_node_t{ 9, { -1, -1 } } );
	EXPECT( t, AAS_INDEX_NODE_PLANE, "node 4 has a bad plane" );
	t = Valid(); t.nodes.push_back( aas_node_t{ 0, { 4, 1 } } );
	EXPECT( t, AAS_INDEX_VALID, "" );
	t = Valid(); t.nodes[3].children[0] = 1;
	EXPECT( t, AAS_INDEX_TREE_CYCLE, "child 1 of node 3 leads back to that node or one above it" );
	t = Valid(); t.nodes[2].children[1] = 2;
	EXPECT( t, AAS_INDEX_TREE_CYCLE, "child 2 of node 2 leads back to that node or one above it" );
	t = Valid(); t.nodes[3].children[0] = 2;
	EXPECT( t, AAS_INDEX_TREE_SHARED, "child 2 of node 3 is already below another node" );

	/* the geometry */
	t = Valid(); t.areas[3].firstface = 4;
	EXPECT( t, AAS_INDEX_AREA_FACES, "area 3 has a bad face range" );
	t = Valid(); t.areas[2].numfaces = 3;
	EXPECT( t, AAS_INDEX_AREA_FACES, "area 2 has a bad face range" );
	t = Valid(); t.areas[0].numfaces = -1;
	EXPECT( t, AAS_INDEX_AREA_FACES, "area 0 has a bad face range" );
	t = Valid(); t.faceindex[1] = 0;
	EXPECT( t, AAS_INDEX_FACE_INDEX, "face index 1 names no face" );
	t = Valid(); t.faceindex[2] = -3;
	EXPECT( t, AAS_INDEX_FACE_INDEX, "face index 2 names no face" );
	t = Valid(); t.faceindex[0] = INT_MIN;
	EXPECT( t, AAS_INDEX_FACE_INDEX, "face index 0 names no face" );
	t = Valid(); t.faces[2].planenum = 4;
	EXPECT( t, AAS_INDEX_FACE_PLANE, "face 2 has a bad plane" );
	t = Valid(); t.faces[1].firstedge = 2;
	EXPECT( t, AAS_INDEX_FACE_EDGES, "face 1 has a bad edge range" );
	t = Valid(); t.faces[2].numedges = -1;
	EXPECT( t, AAS_INDEX_FACE_EDGES, "face 2 has a bad edge range" );
	t = Valid(); t.faces[2].backarea = 4;
	EXPECT( t, AAS_INDEX_FACE_AREA, "face 2 has a bad area" );
	t = Valid(); t.faces[1].frontarea = -1;
	EXPECT( t, AAS_INDEX_FACE_AREA, "face 1 has a bad area" );
	t = Valid(); t.edgeindex[0] = 0;
	EXPECT( t, AAS_INDEX_EDGE_INDEX, "edge index 0 names no edge" );
	t = Valid(); t.edgeindex[1] = -4;
	EXPECT( t, AAS_INDEX_EDGE_INDEX, "edge index 1 names no edge" );
	t = Valid(); t.edges[3].v[1] = 5;
	EXPECT( t, AAS_INDEX_EDGE_VERTEX, "edge 3 has a bad vertex" );
	t = Valid(); t.edges[1].v[0] = -1;
	EXPECT( t, AAS_INDEX_EDGE_VERTEX, "edge 1 has a bad vertex" );

	/* the routing tables */
	t = Valid(); t.clusters[2].numreachabilityareas = 3;
	EXPECT( t, AAS_INDEX_CLUSTER_COUNTS, "cluster 2 has bad area counts" );
	t = Valid(); t.clusters[0].numareas = -1;
	EXPECT( t, AAS_INDEX_CLUSTER_COUNTS, "cluster 0 has bad area counts" );
	t = Valid(); t.clusters[2].firstportal = 2;
	EXPECT( t, AAS_INDEX_CLUSTER_PORTALS, "cluster 2 has a bad portal range" );
	t = Valid(); t.clusters[0].numportals = 3;
	EXPECT( t, AAS_INDEX_CLUSTER_PORTALS, "cluster 0 has a bad portal range" );
	t = Valid(); t.portalindex[1] = 2;
	EXPECT( t, AAS_INDEX_PORTAL_INDEX, "portal index 1 names no portal" );
	t = Valid(); t.portals[1].areanum = 4;
	EXPECT( t, AAS_INDEX_PORTAL_AREA, "portal 1 has a bad area" );
	t = Valid(); t.portals[0].areanum = -1;
	EXPECT( t, AAS_INDEX_PORTAL_AREA, "portal 0 has a bad area" );
	t = Valid(); t.portals[1].backcluster = 3;
	EXPECT( t, AAS_INDEX_PORTAL_CLUSTER, "portal 1 has a bad cluster" );
	t = Valid(); t.portals[0].frontcluster = 3;
	EXPECT( t, AAS_INDEX_PORTAL_CLUSTER, "portal 0 has a bad cluster" );
	t = Valid(); t.portals[1].clusterareanum[1] = 2;
	EXPECT( t, AAS_INDEX_PORTAL_NUMBER, "portal 1 has a bad number in cluster 2" );
	t = Valid(); t.portals[0].clusterareanum[0] = -1;
	EXPECT( t, AAS_INDEX_PORTAL_NUMBER, "portal 0 has a bad number in cluster 0" );
	/* portal 0 joins no cluster, so its numbers need only not be negative */
	t = Valid(); t.portals[0].clusterareanum[0] = 5; t.portals[0].clusterareanum[1] = 7;
	EXPECT( t, AAS_INDEX_VALID, "" );
	t = Valid(); t.areasettings[3].numreachableareas = 2;
	EXPECT( t, AAS_INDEX_AREA_REACHABILITIES, "area 3 has a bad reachability range" );
	t = Valid(); t.areasettings[0].numreachableareas = 9;
	EXPECT( t, AAS_INDEX_AREA_REACHABILITIES, "area 0 has a bad reachability range" );
	t = Valid(); t.areasettings[1].firstreachablearea = -1;
	EXPECT( t, AAS_INDEX_AREA_REACHABILITIES, "area 1 has a bad reachability range" );
	t = Valid(); t.areasettings[1].cluster = 3;
	EXPECT( t, AAS_INDEX_AREA_CLUSTER, "area 1 has a bad cluster" );
	t = Valid(); t.areasettings[2].cluster = -2;
	EXPECT( t, AAS_INDEX_AREA_CLUSTER, "area 2 has a bad cluster" );
	t = Valid(); t.areasettings[2].cluster = INT_MIN;
	EXPECT( t, AAS_INDEX_AREA_CLUSTER, "area 2 has a bad cluster" );
	t = Valid(); t.areasettings[1].clusterareanum = 2;
	EXPECT( t, AAS_INDEX_AREA_NUMBER, "area 1 has a bad number in cluster 1" );
	t = Valid(); t.areasettings[3].clusterareanum = -1;
	EXPECT( t, AAS_INDEX_AREA_NUMBER, "area 3 has a bad number in cluster 2" );
	t = Valid(); t.areasettings[3].cluster = -1;
	EXPECT( t, AAS_INDEX_AREA_PORTAL, "area 3 is portal 1, which is another area" );
	t = Valid(); t.areasettings[3].cluster = 0;
	EXPECT( t, AAS_INDEX_AREA_UNCLUSTERED, "area 3 has reachabilities but no cluster" );
	/* an area outside every cluster is fine when nothing leaves it */
	t = Valid(); t.areasettings[3].cluster = 0; t.areasettings[3].numreachableareas = 0;
	EXPECT( t, AAS_INDEX_VALID, "" );
	t = Valid(); t.areasettings[2].cluster = 1; t.areasettings[2].clusterareanum = 1;
	EXPECT( t, AAS_INDEX_PORTAL_OWNER, "portal 1 is area 2, which is not that portal" );
	t = Valid(); t.portals[1].backcluster = 1;
	EXPECT( t, AAS_INDEX_PORTAL_BORDER, "cluster 2 lists portal 1, which is not on its border" );
	t = Valid(); t.reachability[2].areanum = 4;
	EXPECT( t, AAS_INDEX_REACH_AREA, "reachability 2 leads to a bad area" );
	t = Valid(); t.reachability[0].areanum = -1;
	EXPECT( t, AAS_INDEX_REACH_AREA, "reachability 0 leads to a bad area" );
	/* four areas and two portals take at most eight numbers */
	t = Valid(); t.clusters[2].numareas = 6;
	EXPECT( t, AAS_INDEX_VALID, "" );
	t = Valid(); t.clusters[2].numareas = 7;
	EXPECT( t, AAS_INDEX_CLUSTER_NUMBERS, "clusters hold more numbers than 4 areas and 2 portals can take" );
	/* a hundred clusters holding nothing: the checks' scratch is then mostly
	 * each cluster's first number */
	t = Valid(); t.clusters.resize( 103, aas_cluster_t{ 0, 0, 0, 2 } );
	EXPECT( t, AAS_INDEX_VALID, "" );
	t = Valid(); t.areasettings[3].clusterareanum = 1;
	EXPECT( t, AAS_INDEX_CLUSTER_NUMBER_SHARED, "area 3 shares number 1 in cluster 2 with another area" );
	t = Valid(); t.areasettings[1].clusterareanum = 1;
	EXPECT( t, AAS_INDEX_CLUSTER_NUMBER_SHARED, "area 2 shares number 1 in cluster 1 with another area" );

	/* Quake III's routing: the reversed links fit their table, which the
	 * compiler's own ranges, one after another, always do */
	t = Valid(); t.areasettings[1].firstreachablearea = 0; t.areasettings[1].numreachableareas = 4;
	EXPECT_QUAKE( t, AAS_INDEX_REVERSED_LINKS, "areas lay out more reachabilities than the file's 4" );
	t = Valid(); t.areasettings[1].firstreachablearea = 0; t.areasettings[1].numreachableareas = 2;
	EXPECT( t, AAS_INDEX_VALID, "" );
	/* routing reverses only the first 128 of an area's reachabilities, so 200
	 * of them and two ranges of two that overlap still fit 202 */
	t = Valid(); t.reachability.resize( 202, t.reachability[1] );
	t.areasettings[1].firstreachablearea = 1; t.areasettings[1].numreachableareas = 200;
	for ( int area : { 2, 3 } ) { t.areasettings[area].firstreachablearea = 200; t.areasettings[area].numreachableareas = 2; }
	EXPECT( t, AAS_INDEX_VALID, "" );
	return failures ? 1 : 0;
}
'''

TRAVEL_CHECKS = r'''
/* Routing's travel times, in files whose reachability ranges overlap: area 1
 * has up to 20,000 reachabilities, and fourteen areas lead into it 1,789
 * times, all within the reversed links' table; its travel-time rows alone
 * would take up to 20,000 x (1,792 x 2 + a pointer) bytes. Areas past 15
 * hold nothing, and cost routing a row pointer each. */
static Tables Overlapping( int rows, int areas ) {
	Tables t;
	const int reach = 22000;
	t.numvertexes = 1; t.numplanes = 2;
	t.edges.assign( 1, aas_edge_t{ { 0, 0 } } );
	t.faces.assign( 1, aas_face_t{ 0, 0, 0, 0, 0, 0 } );
	t.areas.assign( areas, aas_area_t{} );
	t.areasettings.assign( areas, aas_areasettings_t{} );
	for ( int i = 1; i < 16; i++ ) {
		t.areasettings[i] = aas_areasettings_t{ 0, 1, 2, 1, i - 1, i == 1 ? rows : i == 15 ? 125 : 128,
			i == 1 ? 1 : 20000 };
	}
	t.reachability.assign( reach, aas_reachability_t{} );
	for ( int i = 1; i < 20000; i++ ) t.reachability[i].areanum = 2;
	for ( int i = 20000; i < reach; i++ ) t.reachability[i].areanum = 1;
	t.nodes.assign( 2, aas_node_t{ 0, { -1, 0 } } );
	t.portals.assign( 1, aas_portal_t{} );
	t.clusters.assign( 2, aas_cluster_t{ 0, 0, 0, 0 } );
	t.clusters[1] = aas_cluster_t{ 15, 15, 0, 0 };
	return t;
}

/* The bytes AAS_CalculateAreaTravelTimes lays out for Overlapping( rows, areas ),
 * with this build's pointers and longs: a row pointer for each area, and for
 * each reachability of each area a row pointer and a time for each link in,
 * padded to a long. Area 1 has 13 x 128 + 125 links in, area 2 has 128
 * (area 1's first), and areas 3 to 15 none. */
static long long Padded( long long links ) {
	return ( links + (long long)sizeof( long ) - 1 ) / (long long)sizeof( long ) * (long long)sizeof( long );
}

static long long TravelBytes( int rows, int areas ) {
	const long long pointer = sizeof( unsigned short * );
	return areas * (long long)sizeof( unsigned short ** ) + rows * ( pointer + Padded( 13 * 128 + 125 ) * 2 )
		+ 128 * ( pointer + Padded( 128 ) * 2 ) + ( 12 * 128 + 125 ) * pointer;
}

int main() {
	/* at the limit, and a row past it; with 100,000 areas their pointers
	 * alone move the limit by a hundred rows */
	for ( int areas : { 16, 100000 } ) {
		int rows = 16000;
		while ( TravelBytes( rows + 1, areas ) <= AAS_MAX_TRAVEL_TIME_BYTES ) rows++;
		assert( rows < 20000 );
		EXPECT( Overlapping( rows, areas ), AAS_INDEX_VALID, "" );
		EXPECT_QUAKE( Overlapping( rows + 1, areas ), AAS_INDEX_TRAVEL_TIMES,
			"routing would need more than 67108864 bytes of travel times" );
	}
	EXPECT_QUAKE( Overlapping( 19999, 16 ), AAS_INDEX_TRAVEL_TIMES,
		"routing would need more than 67108864 bytes of travel times" );
	return failures ? 1 : 0;
}
'''


def valid_tables():
    """The synthetic file as the checks' fixture tables."""
    w = WORLD
    code = 'static Tables Valid() {\n\tTables t;\n'
    code += f'\tt.numvertexes = {len(w["vertexes"])}; t.numplanes = {len(w["planes"])};\n'
    for name, kind in (('edges', 'aas_edge_t'), ('edgeindex', 'aas_edgeindex_t'), ('faces', 'aas_face_t'),
                       ('faceindex', 'aas_faceindex_t'), ('areas', 'aas_area_t'),
                       ('areasettings', 'aas_areasettings_t'), ('reachability', 'aas_reachability_t'),
                       ('nodes', 'aas_node_t'), ('portals', 'aas_portal_t'), ('portalindex', 'aas_portalindex_t'),
                       ('clusters', 'aas_cluster_t')):
        # an edge is one array: brace it as one
        records = ','.join(f'{kind}{cxx((record,) if name == "edges" else record)}' if isinstance(record, tuple)
                           else f'{kind}({record})' for record in w[name])
        code += f'\tt.{name} = {{ {records} }};\n'
    return code + '\treturn t;\n}\n'


def build_and_run(test, name, sources):
    compiler = shutil.which('clang++')
    if not compiler:
        test.skipTest('Clang C++ compiler unavailable')
    target = compiler_target_args(compiler)
    abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if os.name == 'nt' else ['-m32']
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as directory:
        paths = []
        for index, text in enumerate(sources):
            path = Path(directory) / f'{name}{index}.cpp'
            path.write_text(text, encoding='utf-8')
            paths.append(str(path))
        exe = Path(directory) / f'{name}.exe'
        run = subprocess.run([compiler, *target, *abi, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS', '-Wall', '-Wextra',
                              '-Werror', '-Wno-unused-function', '-I', str(ROOT), *paths, '-o', str(exe)],
                             cwd=ROOT, capture_output=True, text=True, timeout=180)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        run = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True, timeout=60, **options)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)


LOADER_CHECKS = r'''
static void load(const std::vector<byte> &file,int expected,const char *error,int line){
 files["maps/test.aas"]=file;lastError.clear();printed.clear();
 const int result=AAS_LoadAASFile("maps/test.aas");
 const std::string &said=Com_GameIsRTCW()?printed:lastError;
 if(result!=expected||said!=error||fileOpen||(aasworld.loaded!=0)!=(expected==0)
  ||(expected&&(aasworld.nodes||aasworld.areas||aasworld.reachability))){
  fprintf(stderr,"line %d: expected %d \"%s\", loaded %d \"%s\"\n",line,expected,error,result,said.c_str());abort();
 }
}
#define LOAD(file,expected,error) load(std::vector<byte>(std::begin(file),std::end(file)),expected,error,__LINE__)
int main(){
 profile=&profiles[0];assert(!Com_GameIsRTCW());initialize();
 LOAD(quake,0,"");assert(aasworld.numnodes==4&&aasworld.nodes[3].children[1]==-3);
 LOAD(quake4,0,"");assert(aasworld.numareas==4&&aasworld.portals[1].areanum==2);
 LOAD(optimised,0,"");assert(!aasworld.numvertexes&&aasworld.numedges==1);
 // a lump past the end, or a negative length, is refused before anything is read into memory
 LOAD(pastEnd,BLERR_CANNOTREADAASLUMP,"maps/test.aas: lump 13 is outside the file\n");
 LOAD(negative,BLERR_CANNOTREADAASLUMP,"maps/test.aas: lump 10 is outside the file\n");
 LOAD(shortHeader,BLERR_WRONGAASFILEID,"maps/test.aas is not an AAS file\n");
 LOAD(cycle,BLERR_CANNOTREADAASLUMP,"maps/test.aas: child 1 of node 3 leads back to that node or one above it\n");
 LOAD(shared,BLERR_CANNOTREADAASLUMP,"maps/test.aas: child 2 of node 3 is already below another node\n");
 LOAD(badPlane,BLERR_CANNOTREADAASLUMP,"maps/test.aas: node 2 has a bad plane\n");
 LOAD(badFaces,BLERR_CANNOTREADAASLUMP,"maps/test.aas: area 3 has a bad face range\n");
 LOAD(badNumber,BLERR_CANNOTREADAASLUMP,"maps/test.aas: area 1 has a bad number in cluster 1\n");
 LOAD(overlap,BLERR_CANNOTREADAASLUMP,"maps/test.aas: areas lay out more reachabilities than the file's 4\n");
 // a refused file leaves the previous one unloaded and loads again once repaired
 LOAD(quake,0,"");
 shutdown();
 profile=&profiles[2];assert(Com_GameIsRTCW());initialize();
 LOAD(rtcw,0,"");assert(aasworld.rtcwGroundSteepness[2]==.5f);
 // what RTCW's own reader lets through: the shared checks refuse it
 LOAD(rtcwFaceZero,BLERR_CANNOTREADAASLUMP,"Invalid RTCW AAS maps/test.aas: face index 1 names no face\n");
 LOAD(rtcwPortalZero,BLERR_CANNOTREADAASLUMP,"Invalid RTCW AAS maps/test.aas: portal 0 has a bad cluster\n");
 LOAD(rtcwBorder,BLERR_CANNOTREADAASLUMP,"Invalid RTCW AAS maps/test.aas: cluster 2 lists portal 1, which is not on its border\n");
 LOAD(rtcwAreaZero,BLERR_CANNOTREADAASLUMP,"Invalid RTCW AAS maps/test.aas: area 0 has a bad face range\n");
 // and its own reader still refuses what it did
 LOAD(rtcwCycle,BLERR_CANNOTREADAASLUMP,"Invalid RTCW AAS maps/test.aas: cyclic or shared node tree\n");
 // RTCW lays out its routing with limits of its own, which overlapping ranges meet
 LOAD(rtcwOverlap,0,"");
 LOAD(rtcw,0,"");
 shutdown();
 return 0;
}
'''


def loader_source():
    source = rtcw_aas_load_tests.loader_source()
    quiet = 'void QDECL AAS_Error(const char*,...){}'
    assert source.count(quiet) == 1
    source = source.replace(quiet, 'static std::string lastError;\n'
                            'void QDECL AAS_Error(const char *fmt,...){char text[1024];va_list args;va_start(args,fmt);'
                            'vsnprintf(text,sizeof(text),fmt,args);va_end(args);lastError=text;}')
    silent = 'static void QDECL print(int,const char*,...){}'
    assert source.count(silent) == 1
    source = source.replace(silent, 'static std::string printed;\n'
                            'static void QDECL print(int kind,const char *fmt,...){if(kind!=PRT_ERROR)return;'
                            'char text[1024];va_list args;va_start(args,fmt);vsnprintf(text,sizeof(text),fmt,args);'
                            'va_end(args);printed=text;}')
    return source


def loader_files():
    optimised = changed(vertexes={'size': 0}, edges={'size': 1}, edgeindex={'size': 0}, faces={'size': 1},
                        faceindex={'size': 0},
                        areas={i: (i, 0, 0, (0, 0, 0), (1, 1, 1), (0, 0, 0)) for i in range(1, 4)})
    whole = encode(WORLD)
    files = dict(
        quake=whole,
        quake4=encode(WORLD, version=4),
        optimised=encode(optimised),
        pastEnd=encode(WORLD, lengths={13: len(whole)}),
        negative=encode(WORLD, lengths={10: -12}),
        shortHeader=whole[:100],
        cycle=encode(changed(nodes={3: (2, (1, -3))})),
        shared=encode(changed(nodes={3: (2, (2, -3))})),
        badPlane=encode(changed(nodes={2: (4, (-1, 0))})),
        badFaces=encode(changed(areas={3: (3, 0, 4, (32, 32, 0), (64, 64, 64), (48, 48, 32))})),
        badNumber=encode(changed(areasettings={1: (0, 1, 2, 1, 2, 1, 1)})),
        overlap=encode(changed(areasettings={1: (0, 1, 2, 1, 0, 4, 0)})),
        rtcw=encode(WORLD, version=8),
        rtcwFaceZero=encode(changed(faceindex={1: 0}), version=8),
        rtcwPortalZero=encode(changed(portals={0: (0, 3, 0, (0, 0))}), version=8),
        rtcwBorder=encode(changed(portals={1: (2, 1, 1, (1, 1))},
                                  clusters={1: (2, 2, 1, 0), 2: (2, 2, 1, 1)}), version=8),
        rtcwAreaZero=encode(changed(areas={0: (0, 5, 0, (0, 0, 0), (0, 0, 0), (0, 0, 0))}), version=8),
        rtcwCycle=encode(changed(nodes={3: (2, (1, -3))}), version=8),
        rtcwOverlap=encode(changed(areasettings={1: (0, 1, 2, 1, 0, 4, 0)}), version=8),
    )
    return ''.join('\nstatic const byte ' + name + '[]={' + ','.join(map(str, data)) + '};\n'
                   for name, data in files.items())


class AasIndexHelperTests(unittest.TestCase):
    def test_the_shared_checks_bound_every_range_and_index(self):
        build_and_run(self, 'aashelpers', [CHECKER_PRELUDE + HELPER_CHECKS, FENCE])

    def test_the_tree_walk_finds_cycles_and_shares_at_any_depth(self):
        build_and_run(self, 'aastrees', [CHECKER_PRELUDE + TREE_CHECKS, FENCE])


class AasIndexCheckTests(unittest.TestCase):
    def test_every_number_a_table_gives_is_checked_against_the_table_it_names(self):
        build_and_run(self, 'aasindexes', [CHECKER_PRELUDE + valid_tables() + INDEX_CHECKS, FENCE])

    def test_quake_routing_refuses_travel_times_that_would_overflow(self):
        build_and_run(self, 'aastravel', [CHECKER_PRELUDE + TRAVEL_CHECKS, FENCE])


class AasLoaderTests(unittest.TestCase):
    def test_both_loaders_refuse_damaged_files_as_they_report_load_errors(self):
        rtcw_aas_load_tests.RTCWAASLoadTests.compile_and_run(
            self, loader_source() + loader_files() + LOADER_CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++', str(ROOT / 'code/compat/game_abi_adapter.c')])

    def test_every_check_comes_before_anything_follows_the_file(self):
        loader = (ROOT / 'code/botlib/be_aas_file.cpp').read_text(encoding='utf-8')
        quake = function(loader, 'int AAS_LoadAASFile(const char *filename)')
        # the lumps' extents are measured before the first is allocated, and
        # the tables' numbers before the file is marked loaded
        self.assertLess(quake.index('AAS_RangeInside(LittleLong(header.lumps[i].fileofs)'),
                        quake.index('AAS_LoadAASLump<aas_bbox_t>('))
        self.assertLess(quake.index('AAS_SwapAASData();'), quake.index('AAS_IndexesInside(true, fault, sizeof(fault))'))
        self.assertLess(quake.index('AAS_IndexesInside(true, fault, sizeof(fault))'), quake.index('aasworld.loaded = qtrue;'))
        self.assertEqual(quake.count('aasworld.loaded = qtrue;'), 1)
        rtcw = (ROOT / 'code/botlib/rtcw_aas_loader.hpp').read_text(encoding='utf-8')
        self.assertLess(rtcw.index('AAS_IndexesInside(false, fault, sizeof(fault))'), rtcw.index('aasworld.loaded = qtrue;'))
        self.assertEqual(rtcw.count('aasworld.loaded = qtrue;'), 1)
        # the scratch goes back before a refusal is reported
        helper = function(loader, 'static bool AAS_IndexesInside(')
        self.assertLess(helper.index('AAS_IndexFault(aasworld'), helper.index('FreeMemory(scratch);'))
        # the point query and the traces keep the checks they had compiled out
        sample = (ROOT / 'code/botlib/be_aas_sample.cpp').read_text(encoding='utf-8')
        self.assertIn('nodenum = 1;\n\twhile (nodenum > 0)', sample.replace('\r\n', '\n'))


if __name__ == '__main__':
    unittest.main()
