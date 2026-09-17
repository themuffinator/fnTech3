"""Exercise ET's derived area visibility, waypoints and routing contracts."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

# The two accessors touch only the derived tables and the area count, so the
# probe supplies those fields and compiles the real bodies against them.
PROBE = r'''
#include <cassert>
#include <cstddef>
#include <cstring>
#include <vector>

using byte = unsigned char;
using qboolean = int;
static const int qfalse = 0, qtrue = 1;
using vec_t = float;
using vec3_t = vec_t[3];

struct World {
 int numareas;
 vec3_t *wolfetWaypoints;
 byte *wolfetVisibility;
 int wolfetVisibilityStride;
 qboolean wolfetVisibilityBuilt;
};
static World aasworld;
static int wolfet = 1;
static int Com_GameIsWolfET() { return wolfet; }
'''

CHECKS = r'''
int main() {
 const int areas = 20;
 const std::size_t stride = (areas + 7) / 8;
 std::vector<vec3_t> waypoints(areas);
 std::vector<byte> matrix(areas * stride, 0);
 std::memset(waypoints.data(), 0, waypoints.size() * sizeof(vec3_t));

 aasworld.numareas = areas;
 aasworld.wolfetWaypoints = waypoints.data();
 aasworld.wolfetVisibility = matrix.data();
 aasworld.wolfetVisibilityStride = static_cast<int>(stride);
 aasworld.wolfetVisibilityBuilt = qtrue;

 // Nothing is visible until a bit is set, including an area from itself.
 for (int a = 1; a < areas; ++a)
  for (int b = 1; b < areas; ++b)
   assert(!AAS_WolfETAreaVisible(a, b));

 const auto set = [&](int from, int to) {
  matrix[static_cast<std::size_t>(from) * stride + (to >> 3)] |= byte(1 << (to & 7));
 };
 set(3, 3); set(3, 11); set(11, 3); set(19, 1);
 assert(AAS_WolfETAreaVisible(3, 3));
 assert(AAS_WolfETAreaVisible(3, 11) && AAS_WolfETAreaVisible(11, 3));
 assert(!AAS_WolfETAreaVisible(11, 11));
 assert(AAS_WolfETAreaVisible(19, 1) && !AAS_WolfETAreaVisible(1, 19));

 // Area zero and anything at or past the count are outside the world.
 for (int bad : {-1, 0, areas, areas + 1, 1 << 20}) {
  assert(!AAS_WolfETAreaVisible(bad, 3));
  assert(!AAS_WolfETAreaVisible(3, bad));
 }

 // A cleared waypoint means the area produced none.
 assert(AAS_WolfETWaypoint(3) == nullptr);
 waypoints[3][0] = 16; waypoints[3][1] = -32; waypoints[3][2] = 8;
 const float *found = AAS_WolfETWaypoint(3);
 assert(found && found[0] == 16 && found[1] == -32 && found[2] == 8);
 for (int bad : {-1, 0, areas, areas + 1}) assert(AAS_WolfETWaypoint(bad) == nullptr);

 // Another profile sees neither, and an unbuilt world reports nothing.
 wolfet = 0;
 assert(!AAS_WolfETAreaVisible(3, 11) && AAS_WolfETWaypoint(3) == nullptr);
 wolfet = 1;
 aasworld.wolfetVisibilityBuilt = qfalse;
 assert(!AAS_WolfETAreaVisible(3, 11));
 aasworld.wolfetVisibilityBuilt = qtrue;
 aasworld.wolfetVisibility = nullptr;
 assert(!AAS_WolfETAreaVisible(3, 11));
 aasworld.wolfetWaypoints = nullptr;
 assert(AAS_WolfETWaypoint(3) == nullptr);
 return 0;
}
'''


def _source(name):
    return (ROOT/name).read_text(encoding='utf-8')


def _body(text, signature):
    begin = text.index(signature)
    return text[begin:text.index('\n}\n', begin) + 3]


class WolfETVisibilityTests(unittest.TestCase):
    def setUp(self):
        self.visibility = _source('code/botlib/wolfet_visibility.hpp')
        self.routing = _source('code/botlib/wolfet_routing.hpp')

    def test_accessors(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        bodies = (_body(self.visibility, 'int AAS_WolfETAreaVisible( int srcarea, int destarea )')
                  + _body(self.visibility, 'const float *AAS_WolfETWaypoint( int area )'))
        scratch = ROOT/'.tmp/wolfet-next'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'visibility.cpp'
            executable = folder/('visibility.exe' if os.name == 'nt' else 'visibility')
            source.write_text(PROBE + bodies + CHECKS, encoding='utf-8')
            target = compiler_target_args(compiler)
            built = subprocess.run([compiler, *target, '-std=c++17', str(source),
                '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT,
                capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_waypoint_derivation(self):
        """The drop trace, its retry and the same-area requirement are the contract."""
        body = _body(self.visibility, 'inline bool Waypoint( int area, vec3_t waypoint )')
        self.assertIn('end[2] -= 256;', body)
        self.assertIn('CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP', body)
        self.assertIn('trace.startsolid && trace.ent < ENTITYNUM_WORLD', body)
        self.assertIn('AAS_PointAreaNum( trace.endpos ) != area', body)
        self.assertIn('VectorClear( waypoint );', body)

    def test_visibility_is_symmetric_and_bounded(self):
        body = _body(self.visibility, 'void AAS_WolfETBuildVisibility( void )')
        self.assertIn('AAS_inPVS(', body)
        self.assertIn('set( from, to );', body)
        self.assertIn('set( to, from );', body)
        self.assertIn('set( from, from );', body)
        # The matrix is refused rather than truncated when it would be too large.
        self.assertIn('MaxMatrixBytes', body)
        self.assertIn('PRT_WARNING', body)
        self.assertIn('AAS_WolfETFreeVisibility();', body)

    def test_routing_uses_the_derived_tables(self):
        for signature, markers in (
                ('int AAS_WolfETListAreasInRange(', ('AAS_WolfETWaypoint', 'AAS_WolfETAreaVisible')),
                ('int AAS_WolfETAvoidDangerArea(', ('AAS_WolfETWaypoint',)),
                ('int AAS_WolfETNearestHideArea(', ('AAS_WolfETAreaVisible', 'AAS_WolfETCallbacks')),
                ('int AAS_WolfETRetreat(', ('AAS_WolfETWaypoint', 'wolfetDangerDistance'))):
            body = _body(self.routing, signature)
            for marker in markers:
                self.assertIn(marker, body, signature)
            # Every flood refuses to run against a world without the tables.
            self.assertIn('Ready()', body)

    def test_hide_search_is_bounded_and_needs_the_game(self):
        body = _body(self.routing, 'int AAS_WolfETNearestHideArea(')
        self.assertIn('maxLoops = 3000', body)
        self.assertIn('srcnum < 0', body)
        # Without the module callback no candidate can be proved hidden.
        self.assertIn('!callbacks.available || !callbacks.visible || !callbacks.available()', body)

    def test_cluster_flood_is_iterative(self):
        """A large cluster must not recurse into the server thread's stack."""
        body = _body(self.routing, 'inline void FloodCluster( int areanum )')
        self.assertIn('std::vector<int> pending', body)
        self.assertNotIn('FloodCluster(', body[body.index('{'):])

    def test_alternative_routes_use_wolfet_selection(self):
        body = _body(self.routing, 'int AAS_WolfETAlternativeRouteGoals(')
        self.assertIn('AREACONTENTS_ROUTEPORTAL', body)
        self.assertIn('AREACONTENTS_CLUSTERPORTAL', body)
        self.assertIn('500 + 3.0 * goaltraveltime', body)
        self.assertIn('dir[2] += 30;', body)
        self.assertIn('AAS_WolfETWaypoint(bestareanum)', body)

    def test_visibility_is_built_only_for_et(self):
        route = _source('code/botlib/be_aas_route.cpp')
        self.assertIn('AAS_WolfETBuildVisibility();', route)
        self.assertIn('AAS_WolfETFreeVisibility();', route)
        body = _body(self.visibility, 'void AAS_WolfETBuildVisibility( void )')
        self.assertIn('!Com_GameIsWolfET()', body)
        # The RTCW route cache loader stays RTCW's.
        self.assertIn('!Com_GameIsRTCW()', _source('code/botlib/rtcw_routecache_loader.hpp'))


if __name__ == '__main__':
    unittest.main()
