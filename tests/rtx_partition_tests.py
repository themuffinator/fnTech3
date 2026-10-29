"""Execute the path tracer's triangle partition (rt_backend.c
RT_PartitionTriangles) against the rule it replaced.

Each frame's moving models, and each world at load, are reordered into the
partitions their instances trace: the world's opaque and masked triangles,
those its lights pass through (each opaque then masked), its special (sky,
portal) triangles and those that filter its light, a traced filtering
triangle in its no-shadow partition and the filter partition both. The
partition used to walk every triangle once per partition. It now judges
each triangle once and writes the partitions in order, each keeping the
triangles' own order; the indexes it writes and the counts it returns must
be the old ones exactly, for world and model geometry, with degenerate and
out-of-range triangles dropped, a ragged index count, an empty geometry, a
growth that fails and a scratch that cannot be had.
"""
from pathlib import Path
import unittest

from rtx_surface_owner_tests import compile_run

ROOT = Path(__file__).resolve().parents[1]


def section(text, start, end):
    begin = text.index(start)
    return text[begin:text.index(end, begin) + len(end)]


HARNESS = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
typedef int qboolean;
typedef unsigned char byte;
enum { qfalse, qtrue };
#define Com_Memset memset
#define Com_Memcpy memcpy
#define MAX( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )
enum { RT_PART_OPAQUE, RT_PART_MASKED, RT_PART_NOSHADOW_OPAQUE, RT_PART_NOSHADOW_MASKED, RT_PART_SPECIAL,
       RT_PART_FILTER, RT_WORLD_PARTS };
#define RT_MARK_NOSHADOW 1
#define RT_MARK_FILTER 2
typedef enum { RT_OWNER_TRACED, RT_OWNER_BASE_RASTER, RT_OWNER_RASTER } rtOwner_t;
struct rtxRtPackedVertex_t { float xyz[4]; float normal[4]; float lightmapData[4]; uint32_t materialIndex; };
struct rtxRtCpuGeometry_t { rtxRtPackedVertex_t *vertices; uint32_t *indices; uint32_t numVertices, numIndices,
    indexCapacity; };
static std::vector<rtOwner_t> owners;
static std::vector<int> masked;
static rtOwner_t RT_MaterialOwner( uint32_t m ) { return m < owners.size() ? owners[m] : RT_OWNER_RASTER; }
static qboolean RT_MaterialMasked( uint32_t m ) { return m < masked.size() && masked[m] ? qtrue : qfalse; }
static int failAlloc, failReserve;
static void *RT_HostAlloc( int bytes ) { return failAlloc ? NULL : malloc( (size_t)bytes ); }
static void RT_HostFree( void *p ) { free( p ); }
static qboolean vk_rt_cpu_geometry_reserve( rtxRtCpuGeometry_t *g, uint32_t v, uint32_t i ) {
    (void)v;
    if ( failReserve ) return qfalse;
    if ( g->numIndices + i > g->indexCapacity ) {
        g->indexCapacity = g->numIndices + i;
        g->indices = (uint32_t *)realloc( g->indices, g->indexCapacity * sizeof( uint32_t ) );
    }
    return qtrue;
}
static struct { uint32_t *partition; byte *partitionCodes; uint32_t partitionCapacity; } s_rt;
'''

CHECKS = r'''
/* The rule the partition replaced: one walk of every triangle per partition. */
static void Reference( const rtxRtCpuGeometry_t *g, qboolean world, std::vector<uint32_t> &out, uint32_t counts[6] ) {
    memset( counts, 0, 6 * sizeof( counts[0] ) );
    out.clear();
    if ( g->numIndices < 3 ) return;
    for ( uint32_t pass = 0; pass < RT_WORLD_PARTS; pass++ ) {
        for ( uint32_t i = 0; i + 2 < g->numIndices; i += 3 ) {
            const uint32_t i0 = g->indices[i], i1 = g->indices[i + 1], i2 = g->indices[i + 2];
            uint32_t bucket = RT_WORLD_PARTS, marker;
            if ( i0 >= g->numVertices || i1 >= g->numVertices || i2 >= g->numVertices ||
                i0 == i1 || i1 == i2 || i0 == i2 ) continue;
            const uint32_t material = g->vertices[i0].materialIndex;
            const rtOwner_t owner = RT_MaterialOwner( material );
            marker = world ? (uint32_t)( g->vertices[i0].lightmapData[3] + 0.5f ) : 0u;
            if ( owner == RT_OWNER_TRACED ) {
                bucket = ( RT_MaterialMasked( material ) || ( !world && g->vertices[i0].normal[3] < 0.0f ) ) ?
                    RT_PART_MASKED : RT_PART_OPAQUE;
                if ( marker & ( RT_MARK_NOSHADOW | RT_MARK_FILTER ) ) bucket += RT_PART_NOSHADOW_OPAQUE;
            } else if ( owner == RT_OWNER_BASE_RASTER && world ) {
                bucket = RT_PART_SPECIAL;
                marker = 0;
            }
            if ( !( bucket == pass || ( pass == RT_PART_FILTER && ( marker & RT_MARK_FILTER ) ) ) ) continue;
            out.push_back( i0 ); out.push_back( i1 ); out.push_back( i2 );
            counts[pass]++;
        }
    }
}

static uint32_t seed = 2463534242u;
static uint32_t Next() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }

static void Build( rtxRtCpuGeometry_t &g, uint32_t verts, uint32_t indexes ) {
    g.vertices = (rtxRtPackedVertex_t *)calloc( verts ? verts : 1, sizeof( rtxRtPackedVertex_t ) );
    g.numVertices = verts;
    for ( uint32_t v = 0; v < verts; v++ ) {
        g.vertices[v].materialIndex = Next() % 9;            // past the table: raster's
        g.vertices[v].lightmapData[3] = (float)( Next() % 4 ); // markers 0 to 3
        g.vertices[v].normal[3] = ( Next() % 5 ) ? 0.0f : -1.0f; // a model's no-shadow policy
    }
    g.indexCapacity = indexes ? indexes : 1;
    g.indices = (uint32_t *)malloc( g.indexCapacity * sizeof( uint32_t ) );
    g.numIndices = indexes;
    for ( uint32_t i = 0; i < indexes; i++ ) {
        const uint32_t r = Next() % 100;
        // now and then repeat the last index (a degenerate triangle) or
        // name a vertex past the end
        g.indices[i] = r < 6 && i ? g.indices[i - 1] : r < 9 ? verts + Next() % 4 : Next() % ( verts ? verts : 1 );
    }
}

int main() {
    owners = { RT_OWNER_TRACED, RT_OWNER_TRACED, RT_OWNER_BASE_RASTER, RT_OWNER_RASTER, RT_OWNER_TRACED,
               RT_OWNER_TRACED, RT_OWNER_RASTER, RT_OWNER_BASE_RASTER };
    masked = { 0, 1, 0, 0, 1, 0, 1, 0 };
    int cases = 0, filtered = 0, special = 0, dropped = 0;
    for ( int round = 0; round < 400; round++ ) {
        const qboolean world = ( round & 1 ) ? qtrue : qfalse;
        const uint32_t verts = Next() % 300;
        uint32_t indexes = ( Next() % 400 ) * 3 + ( round % 7 == 0 ? Next() % 3 : 0 );  // ragged now and then
        if ( round % 50 == 0 ) indexes = Next() % 3;                                   // too few to make one
        rtxRtCpuGeometry_t g = {}, h = {};
        Build( g, verts, indexes );
        h = g;
        h.indices = (uint32_t *)malloc( g.indexCapacity * sizeof( uint32_t ) );
        memcpy( h.indices, g.indices, g.indexCapacity * sizeof( uint32_t ) );
        std::vector<uint32_t> expected;
        uint32_t want[6], got[6];
        Reference( &h, world, expected, want );
        assert( RT_PartitionTriangles( &g, world, got ) );
        assert( memcmp( want, got, sizeof( want ) ) == 0 );
        assert( g.numIndices == expected.size() );
        assert( expected.empty() || memcmp( g.indices, expected.data(), expected.size() * sizeof( uint32_t ) ) == 0 );
        filtered += want[RT_PART_FILTER];
        special += want[RT_PART_SPECIAL];
        uint32_t kept = 0;
        for ( uint32_t c = 0; c < RT_PART_FILTER; c++ ) kept += want[c];
        dropped += (int)( h.numIndices / 3 ) - (int)kept;
        cases++;
        free( g.vertices ); free( g.indices ); free( h.indices );
    }
    // every partition was exercised, a filter triangle written twice among them
    assert( filtered > 0 && special > 0 && dropped > 0 );

    // a world that grows past its index room when filters repeat triangles,
    // and growth refused
    for ( int refuse = 1; refuse >= 0; refuse-- ) {
        // a hundred traced triangles that all filter: each is written twice
        rtxRtCpuGeometry_t g = {};
        Build( g, 64, 300 );
        for ( uint32_t v = 0; v < g.numVertices; v++ ) {
            g.vertices[v].materialIndex = 0;
            g.vertices[v].lightmapData[3] = (float)RT_MARK_FILTER;
        }
        for ( uint32_t t = 0; t < 100; t++ ) {
            g.indices[t * 3] = t % 60; g.indices[t * 3 + 1] = t % 60 + 1; g.indices[t * 3 + 2] = t % 60 + 2;
        }
        uint32_t got[6];
        failReserve = refuse;
        if ( refuse ) {
            assert( !RT_PartitionTriangles( &g, qtrue, got ) );
            assert( g.numIndices == 300 );
        } else {
            assert( RT_PartitionTriangles( &g, qtrue, got ) );
            assert( got[RT_PART_NOSHADOW_OPAQUE] == 100 && got[RT_PART_FILTER] == 100 && g.numIndices == 600 );
            assert( memcmp( g.indices, g.indices + 300, 300 * sizeof( uint32_t ) ) == 0 );
        }
        failReserve = 0;
        free( g.vertices ); free( g.indices );
    }
    // no scratch to be had: refused, the geometry left as it was
    {
        free( s_rt.partition ); s_rt.partition = NULL; s_rt.partitionCodes = NULL; s_rt.partitionCapacity = 0;
        rtxRtCpuGeometry_t g = {};
        Build( g, 16, 30 );
        uint32_t got[6];
        failAlloc = 1;
        assert( !RT_PartitionTriangles( &g, qfalse, got ) );
        assert( g.numIndices == 30 );
        failAlloc = 0;
    }
    printf( "partition %d cases\n", cases );
    return 0;
}
'''


class RtxPartitionTests(unittest.TestCase):
    def test_one_judgement_a_triangle_writes_the_old_partitions_exactly(self):
        backend = (ROOT / 'code/rendererrtx/rt/rt_backend.c').read_text(encoding='utf-8').replace('\r\n', '\n')
        source = HARNESS
        source += section(backend, 'static qboolean RT_EnsurePartitionScratch( uint32_t count )', '\n}\n')
        source += section(backend, '#define RT_PART_CODE_FILTER', '\n')
        source += section(backend, 'static qboolean RT_PartitionTriangles(', '\n}\n')
        compile_run(self, source + CHECKS)

    def test_each_triangle_is_judged_once(self):
        backend = (ROOT / 'code/rendererrtx/rt/rt_backend.c').read_text(encoding='utf-8').replace('\r\n', '\n')
        partition = section(backend, 'static qboolean RT_PartitionTriangles(', '\n}\n')
        # one walk to judge, one to write: no walk per partition
        self.assertNotIn('for ( pass = 0; pass < RT_WORLD_PARTS; pass++ )', partition)
        self.assertEqual(2, partition.count('for ( i = 0, t = 0; i + 2 < geometry->numIndices; i += 3, t++ )'))
        self.assertEqual(1, partition.count('RT_MaterialOwner( material )'))


if __name__ == '__main__':
    unittest.main()
