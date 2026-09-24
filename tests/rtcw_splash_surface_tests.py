"""RTCW and Enemy Territory generated entity types in every renderer.

Retail WolfMP.exe (SHA-256 7cf99a938cf18ed2...) accepts reference entity
types 2 through 8 as generated surfaces in R_AddEntitySurfaces (0x467af0),
skips type 9 (portal) and stops with "Bad reType" on type 1 (polygon) and
anything above 9. Its RB_SurfaceEntity (0x4851c0) draws type 3, the water
ripple, as a quad lying in the world's x/y plane (0x4851de) and sends type 6,
the tapered rail core, to the default debug axis. The client adapters
translate those module ordinals to the engine's own enum, so every engine
type an adapter can produce must reach a surface here, or a bullet striking
water ends the session with "R_AddEntitySurfaces: Bad reType".
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def case_labels(body):
    return set(re.findall(r'\bcase\s+(RT_\w+)\s*:', body))


def adapter_types():
    """Every engine entity type each client adapter can produce."""
    adapter = read('code/compat/render_record_adapter.hpp')
    produced = {}
    for name in ('LegacyEntity', 'RTCWEntity', 'WolfETEntity', 'STVEFEntity'):
        start = adapter.index(f'bool {name}(')
        table = re.search(r'types\[\]\s*=\s*\{([^}]*)\}', adapter[start:])
        produced[name] = re.findall(r'RT_\w+', table.group(1))
    # Elite Force single player keeps Quake III's numbers; it casts 0..7.
    enum = read('code/renderercommon/tr_types.h')
    body = enum[enum.index('typedef enum {\n\tRT_MODEL'):enum.index('} refEntityType_t;')]
    canonical = re.findall(r'^\s*(RT_\w+)', body, re.M)
    produced['STVEFSPEntity'] = canonical[:canonical.index('RT_PORTALSURFACE') + 1]
    return produced


HARNESS = r'''
#include <cassert>
#include <cstring>
#include <initializer_list>
typedef float vec3_t[3];
typedef unsigned char byte;
typedef struct { byte rgba[4]; } color4ub_t;
typedef int qboolean;
enum { qfalse, qtrue };
enum { PV_NONE, PV_PORTAL, PV_MIRROR };
static const vec3_t vec3_origin = { 0, 0, 0 };
#define VectorSet( v, x, y, z ) ( ( v )[0] = ( x ), ( v )[1] = ( y ), ( v )[2] = ( z ) )
#define VectorSubtract( a, b, c ) ( ( c )[0] = ( a )[0] - ( b )[0], ( c )[1] = ( a )[1] - ( b )[1], ( c )[2] = ( a )[2] - ( b )[2] )
struct viewParms_t { int portalView; };
struct refEntity_t { float origin[3]; float radius; color4ub_t shader; };
struct trRefEntity_t { refEntity_t e; };
static struct { trRefEntity_t *currentEntity; viewParms_t viewParms; } backEnd;
static qboolean R_ViewPassIsMirror( const viewParms_t *view ) { return view->portalView == PV_MIRROR ? qtrue : qfalse; }
static int stamps;
static float stampOrigin[3], stampLeft[3], stampUp[3];
static color4ub_t stampColor;
static void RB_AddQuadStamp( const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color ) {
	++stamps;
	std::memcpy( stampOrigin, origin, sizeof( stampOrigin ) );
	std::memcpy( stampLeft, left, sizeof( stampLeft ) );
	std::memcpy( stampUp, up, sizeof( stampUp ) );
	stampColor = color;
}
'''

CHECKS = r'''
static void Expect( const float *actual, float x, float y, float z ) {
	assert( actual[0] == x && actual[1] == y && actual[2] == z );
}
int main() {
	trRefEntity_t ripple{};
	ripple.e.origin[0] = 100.0f; ripple.e.origin[1] = -50.0f; ripple.e.origin[2] = 12.5f;
	ripple.e.shader = { { 255, 128, 64, 32 } };
	backEnd.currentEntity = &ripple;
	for ( float radius : { 32.0f, 150.0f, 300.0f, 0.0f } ) {
		ripple.e.radius = radius;
		// A primary view and a portal draw it the same way: left is -x and
		// up is +y, so the four corners share the ripple's height.
		for ( int view : { PV_NONE, PV_PORTAL } ) {
			backEnd.viewParms.portalView = view;
			stamps = 0;
			RB_SurfaceSplash();
			assert( stamps == 1 );
			Expect( stampOrigin, 100.0f, -50.0f, 12.5f );
			Expect( stampLeft, -radius, 0.0f, 0.0f );
			Expect( stampUp, 0.0f, radius, 0.0f );
			assert( !std::memcmp( &stampColor, &ripple.e.shader, sizeof( stampColor ) ) );
		}
		// A mirror negates left and keeps up, as retail's isMirror test does.
		backEnd.viewParms.portalView = PV_MIRROR;
		stamps = 0;
		RB_SurfaceSplash();
		assert( stamps == 1 );
		Expect( stampLeft, radius, 0.0f, 0.0f );
		Expect( stampUp, 0.0f, radius, 0.0f );
	}
	return 0;
}
'''


class RTCWSplashSurfaceTests(unittest.TestCase):
    def compile_and_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(compiler)
        if os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-splash-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2', str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_every_translated_type_reaches_the_renderer(self):
        produced = adapter_types()
        self.assertIn('RT_SPLASH', produced['RTCWEntity'])
        self.assertIn('RT_RAIL_CORE_TAPER', produced['WolfETEntity'])
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                source = read(f'code/{renderer}/tr_main.c')
                body = function(source, 'static void R_AddEntitySurfaces( void ) {')
                accepted = case_labels(body)
                self.assertIn('ri.Error( ERR_DROP, "R_AddEntitySurfaces: Bad reType" );', body)
                for adapter, types in produced.items():
                    for entity_type in types:
                        if entity_type == 'RT_POLY':
                            # Polygons go through RE_AddPolyToScene; every retail
                            # renderer stops on a polygon-typed entity as well.
                            self.assertNotIn(entity_type, accepted)
                            continue
                        self.assertIn(entity_type, accepted, f'{adapter} produces {entity_type}')
                # Both RTCW-family types share the generated-surface branch
                # with sprites: third-person skip, custom shader and fog.
                generated = body[body.index('case RT_SPRITE:'):body.index('case RT_MODEL:')]
                for entity_type in ('RT_SPLASH', 'RT_RAIL_CORE_TAPER'):
                    self.assertIn(f'case {entity_type}:', generated)
                self.assertIn('R_SpriteFogNum( ent )', generated)

    def test_backend_draws_the_ripple_and_leaves_the_tapered_core_on_the_axis(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                source = read(f'code/{renderer}/tr_surface.c')
                body = function(source, 'static void RB_SurfaceEntity( const surfaceType_t *surfType ) {')
                self.assertRegex(body, r'case RT_SPLASH:\s*RB_SurfaceSplash\(\);\s*break;')
                # Retail's switch sends type 6 to its default, the debug axis.
                self.assertNotIn('RT_RAIL_CORE_TAPER', case_labels(body))
                default = body[body.index('default:'):]
                self.assertIn('RB_SurfaceAxis();', default)

    def test_ripple_quad_matches_retail(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                source = read(f'code/{renderer}/tr_surface.c')
                splash = function(source, 'static void RB_SurfaceSplash( void ) {')
                self.compile_and_run(HARNESS + splash + CHECKS)


if __name__ == '__main__':
    unittest.main()
