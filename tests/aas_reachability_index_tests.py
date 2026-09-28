"""Number a point's reachability area, a cluster portal's included.

AAS_PointReachabilityAreaIndex numbers every reachability area of every
cluster in one run: each cluster's reachability areas follow those of all the
clusters below it. A cluster portal is numbered in both clusters it joins, and
the function takes its number in the front one. It used to read that number
from portals[-frontcluster], a record before the portal table, because it had
already replaced the portal's number with its front cluster.

The world here is six slabs along x with solid past them: two clusters joined
by two portals, one facing each way, and one area without reachabilities. The
records before the portal table are poisoned, and each portal area's own
settings carry a number that is not the portal's, so a read of either shows in
the index. The function, the point query it starts from and the reachability
count it checks are compiled from their own source.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/botlib/l_memory.h"
#include "code/botlib/aasfile.h"
#include "code/botlib/botlib.h"
#include "code/botlib/be_aas.h"
#include "code/botlib/be_interface.h"
#include "code/botlib/be_aas_funcs.h"
#include "code/botlib/be_aas_def.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

static aas_t world;
aas_t *aas_currentWorld = &world;
botlib_import_t botimport;
static int errors;
int Com_GameIsRTCW( void ) { return 0; }
int Com_GameIsRTCWSP( void ) { return 0; }
void QDECL AAS_Error( const char *, ... ) { ++errors; }
static void QDECL Print( int type, const char *, ... ) { if ( type == PRT_ERROR || type == PRT_FATAL ) ++errors; }
'''

WORLD = r'''
/* Node n splits x at 10n: area n lies behind it, and past node 6 is solid. */
enum { AREAS = 7, CLUSTERS = 3, PORTALS = 3, POISONED = 4 };
static aas_plane_t planes[AREAS - 1];
static aas_node_t nodes[AREAS];
static aas_areasettings_t settings[AREAS];
static aas_portal_t portalRecords[POISONED + PORTALS];
static aas_portalindex_t portalIndex[4];
static aas_cluster_t clusters[CLUSTERS];

/* Clustering never numbers a portal area in its own settings; give it a number the portal does not have. */
static constexpr int DECOY = 77;

static void area( int n, int cluster, int number, int reachabilities ) {
	settings[n].cluster = cluster;
	settings[n].clusterareanum = number;
	settings[n].numreachableareas = reachabilities;
	settings[n].firstreachablearea = reachabilities ? n : 0;
}

static void build( void ) {
	botimport.Print = Print;
	for ( int n = 1; n < AREAS; n++ ) {
		planes[n - 1].normal[0] = 1.0f;
		planes[n - 1].dist = 10.0f * n;
		nodes[n].planenum = n - 1;
		nodes[n].children[0] = n + 1 < AREAS ? n + 1 : 0;
		nodes[n].children[1] = -n;
	}
	world.numplanes = AREAS - 1;
	world.planes = planes;
	world.numnodes = AREAS;
	world.nodes = nodes;

	/* As AAS_NumberClusterAreas numbers them: a cluster's own areas with
	 * reachabilities, then its portals with reachabilities in index order,
	 * then its areas without. Cluster 1: areas 1 and 2, portal 1 (its front),
	 * portal 2 (its back), area 5. Cluster 2: area 4, portal 1 (its back),
	 * portal 2 (its front). */
	area( 1, 1, 0, 1 );
	area( 2, 1, 1, 1 );
	area( 3, -1, DECOY, 1 );
	area( 4, 2, 0, 1 );
	area( 5, 1, 4, 0 );
	area( 6, -2, DECOY, 1 );
	world.numareas = world.numareasettings = AREAS;
	world.areasettings = settings;

	for ( int i = 0; i < POISONED; i++ ) {
		portalRecords[i].areanum = -1;
		portalRecords[i].frontcluster = portalRecords[i].backcluster = -1;
		portalRecords[i].clusterareanum[0] = 1000 + i;
		portalRecords[i].clusterareanum[1] = 2000 + i;
	}
	aas_portal_t *portals = portalRecords + POISONED;
	portals[1] = { 3, 1, 2, { 2, 1 } };
	portals[2] = { 6, 2, 1, { 2, 3 } };
	world.numportals = PORTALS;
	world.portals = portals;

	portalIndex[0] = 1; portalIndex[1] = 2; portalIndex[2] = 1; portalIndex[3] = 2;
	world.portalindexsize = 4;
	world.portalindex = portalIndex;
	clusters[1] = { 5, 4, 2, 0 };
	clusters[2] = { 3, 3, 2, 2 };
	world.numclusters = CLUSTERS;
	world.clusters = clusters;
	world.loaded = world.initialized = qtrue;
}

static int indexAt( float x ) {
	vec3_t point = { x, 0.0f, 0.0f };
	return AAS_PointReachabilityAreaIndex( point );
}
'''

CHECKS = r'''
int main( void ) {
#ifdef _MSC_VER
	_set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
#endif
	build();
	for ( int n = 1; n < AREAS; n++ ) {
		vec3_t point = { 10.0f * n - 5.0f, 0.0f, 0.0f };
		assert( AAS_PointAreaNum( point ) == n );
	}

	/* Every cluster's reachability areas, each portal once in each cluster it joins. */
	const int total = AAS_PointReachabilityAreaIndex( nullptr );
	assert( total == 4 + 3 );

	/* Ordinary areas: the clusters below, then the area's own number. */
	assert( indexAt( 5.0f ) == 0 );
	assert( indexAt( 15.0f ) == 1 );
	assert( indexAt( 35.0f ) == 4 + 0 );
	/* Portal 1 faces cluster 1, which has none below it. */
	assert( indexAt( 25.0f ) == 0 + 2 );
	/* Portal 2 faces cluster 2: cluster 1's four come first. */
	assert( indexAt( 55.0f ) == 4 + 2 );
	/* No reachabilities, and solid: nothing to number. */
	assert( indexAt( 45.0f ) == 0 );
	assert( indexAt( 65.0f ) == 0 );
	assert( errors == 0 );

	/* Each reachability area has an index of its own, below the count. */
	const int indexes[] = { indexAt( 5.0f ), indexAt( 15.0f ), indexAt( 25.0f ), indexAt( 35.0f ), indexAt( 55.0f ) };
	for ( int i = 0; i < 5; i++ ) {
		assert( indexes[i] >= 0 && indexes[i] < total );
		for ( int j = 0; j < i; j++ ) assert( indexes[i] != indexes[j] );
	}

	/* The portal's number in the cluster behind it is not the one taken. */
	world.portals[1].clusterareanum[1] = 0;
	world.portals[2].clusterareanum[1] = 0;
	assert( indexAt( 25.0f ) == 2 );
	assert( indexAt( 55.0f ) == 6 );
	assert( errors == 0 );

	/* Not initialized: nothing, not even the count. */
	world.initialized = qfalse;
	assert( AAS_PointReachabilityAreaIndex( nullptr ) == 0 );
	assert( indexAt( 25.0f ) == 0 );
	puts( "ok" );
	return 0;
}
'''


def probe_source():
    sample = (ROOT / 'code/botlib/be_aas_sample.cpp').read_text(encoding='utf-8')
    reach = (ROOT / 'code/botlib/be_aas_reach.cpp').read_text(encoding='utf-8')
    return '\n'.join((PRELUDE, function(sample, 'int AAS_PointAreaNum('), function(reach, 'int AAS_AreaReachability('),
                      function(sample, 'int AAS_PointReachabilityAreaIndex('), WORLD, CHECKS))


class ReachabilityAreaIndexTests(unittest.TestCase):
    def compile_and_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='aas-reach-index-', dir=ROOT / '.tmp',
                                         ignore_cleanup_errors=True) as directory:
            folder = Path(directory)
            cpp, exe = folder / 'probe.cpp', folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([compiler, *target, '-O2', '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                                    '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertEqual(run.stdout.strip(), 'ok')

    def test_ordinary_and_portal_areas_number_in_their_clusters(self):
        self.compile_and_run(probe_source())


if __name__ == '__main__':
    unittest.main()
