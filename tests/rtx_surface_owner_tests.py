"""Execute the path tracer's draw-surface ownership.

The real RT_EntityClass and vk_rt_surface_owner (code/rendererrtx/rt/
rt_backend.c) run against a bounded host: which surfaces the trace replaces,
which the base pass keeps, and which the raster overlay draws.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


def host():
    """The owner functions and the engine types they read."""
    local = (ROOT / 'code/rendererrtx/tr_local.h').read_text(encoding='utf-8')
    vk = (ROOT / 'code/rendererrtx/vk.h').read_text(encoding='utf-8')
    material = (ROOT / 'code/rendererrtx/rt/rt_material.c').read_text(encoding='utf-8')
    backend = (ROOT / 'code/rendererrtx/rt/rt_backend.c').read_text(encoding='utf-8')
    source = '''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/renderercommon/tr_types.h"
#include "code/qcommon/game_profile.h"
#include <assert.h>
#include <initializer_list>
'''
    for text, pattern in ((local, r'typedef enum\s*\{[^{}]*SF_BAD[^{}]*\}\s*surfaceType_t;'),
                          (local, r'typedef enum\s*\{[^{}]*MOD_BAD[^{}]*\}\s*modtype_t;'),
                          (local, r'typedef enum\s*\{[^{}]*PV_NONE[^{}]*\}\s*portalView_t;'),
                          (vk, r'typedef enum\s*\{[^{}]*RT_SURFACE_RASTER[^{}]*\}\s*rtSurfaceOwner_t;'),
                          (material, r'typedef enum\s*\{[^{}]*RT_OWNER_RASTER[^{}]*\}\s*rtOwner_t;')):
        source += re.search(pattern, text)[0] + '\n'
    source += r'''
#define REFENTITYNUM_WORLD 1023
typedef struct shader_s { uint32_t material; } shader_t;
typedef struct { modtype_t type; } model_t;
typedef struct { refEntity_t e; } trRefEntity_t;
static struct { comGameId_t gameId; } ri = { COM_GAME_QUAKE3 };
static struct { struct { int activeRtMode; } caps; } vk = { { 2 } };
static struct { qboolean failed; } s_rt;
static struct {
    struct { trRefEntity_t *entities; int rdflags; } refdef;
    struct { portalView_t portalView; } viewParms;
} backEnd;
static model_t models[16];
static rtOwner_t materialOwners[4] = { RT_OWNER_RASTER, RT_OWNER_TRACED, RT_OWNER_BASE_RASTER, RT_OWNER_TRACED };
static const model_t *R_GetModelByHandle( qhandle_t handle ) {
    return handle > 0 && handle < 16 ? &models[handle] : NULL;
}
static uint32_t RT_MaterialIndexForShader( const shader_t *shader ) { return shader->material; }
static rtOwner_t RT_MaterialOwner( uint32_t material ) { return materialOwners[material]; }
'''
    source += function(backend, 'static int RT_EntityClass( const trRefEntity_t *ent )')
    source += function(backend, 'rtSurfaceOwner_t vk_rt_surface_owner(')
    source += r'''
static const shader_t rasterShader = { 0 }, tracedShader = { 1 }, skyShader = { 2 };
static trRefEntity_t entities[4];
/* a fresh model entity; handle 1 names model type `type` */
static trRefEntity_t *Model( modtype_t type ) {
    models[1].type = type;
    memset( &entities[0], 0, sizeof( entities[0] ) );
    entities[0].e.reType = RT_MODEL;
    entities[0].e.hModel = 1;
    backEnd.refdef.entities = entities;
    return &entities[0];
}
'''
    return source


def compile_run(test, source):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('C++17 compiler unavailable')
    flags = compiler_target_args(compiler)
    if os.name == 'nt' and not flags:
        flags = ['-DFNTECH3_Q3_ONLY']
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rtx-owner-', dir=ROOT / '.tmp') as temp:
        cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
        cpp.write_text(source, encoding='utf-8')
        built = subprocess.run([compiler, *flags, '-std=c++17', '-fno-operator-names', '-O2', '-I', str(ROOT),
                                str(cpp), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
        test.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


CHECKS = r'''
int main() {
    const int world = REFENTITYNUM_WORLD;
    /* The world's BSP surfaces follow their material; a fogged traced surface
     * returns to the overlay for its fog pass. */
    for ( int type : { SF_FACE, SF_GRID, SF_TRIANGLES } ) {
        assert( vk_rt_surface_owner( world, &tracedShader, 0, type ) == RT_SURFACE_TRACED );
        assert( vk_rt_surface_owner( world, &tracedShader, 3, type ) == RT_SURFACE_TRACED_FOGGED );
        assert( vk_rt_surface_owner( world, &rasterShader, 0, type ) == RT_SURFACE_RASTER );
        assert( vk_rt_surface_owner( world, &skyShader, 0, type ) == RT_SURFACE_BASE_RASTER );
    }
    /* Everything the trace does not reproduce stays raster: sprites, beams,
     * rails and lightning (SF_ENTITY), polys, particles, flares, ET foliage and
     * decals, Alice's models and swipes, and model surfaces with no entity. */
    for ( int type : { SF_BAD, SF_SKIP, SF_POLY, SF_FLARE, SF_ENTITY, SF_WOLFET_FOLIAGE, SF_WOLFET_DECAL,
                       SF_IHV_PARTICLES, SF_ALICE_TIKI, SF_ALICE_SWIPE, SF_MD3, SF_MDC, SF_MDS, SF_WOLFET } ) {
        assert( vk_rt_surface_owner( world, &tracedShader, 0, type ) == RT_SURFACE_RASTER );
        Model( MOD_MESH );
        if ( type != SF_MD3 && type != SF_MDC && type != SF_MDS && type != SF_WOLFET )
            assert( vk_rt_surface_owner( 0, &tracedShader, 0, type ) == RT_SURFACE_RASTER );
    }
    /* The model formats the trace poses, and brush models. */
    const struct { modtype_t model; int surface; } posed[] = {
        { MOD_MESH, SF_MD3 }, { MOD_MDR, SF_MDR }, { MOD_IQM, SF_IQM }, { MOD_MDC, SF_MDC },
        { MOD_MDS, SF_MDS }, { MOD_WOLFET_MESH, SF_WOLFET }, { MOD_BRUSH, SF_FACE }, { MOD_BRUSH, SF_GRID },
        { MOD_BRUSH, SF_TRIANGLES } };
    for ( const auto &p : posed ) {
        Model( p.model );
        assert( RT_EntityClass( &entities[0] ) == 0 );
        assert( vk_rt_surface_owner( 0, &tracedShader, 0, p.surface ) == RT_SURFACE_TRACED );
        assert( vk_rt_surface_owner( 0, &tracedShader, 2, p.surface ) == RT_SURFACE_TRACED_FOGGED );
        assert( vk_rt_surface_owner( 0, &rasterShader, 0, p.surface ) == RT_SURFACE_RASTER );
        /* an entity's sky or portal shader is not the world's base: overlay */
        assert( vk_rt_surface_owner( 0, &skyShader, 0, p.surface ) == RT_SURFACE_RASTER );
    }
    /* Models the trace does not pose, other entity types and bad handles. */
    for ( modtype_t type : { MOD_BAD, MOD_WOLFET_SKELETON, MOD_IHV_BEAM, MOD_ALICE_TIKI, MOD_ALICE_SPRITE } ) {
        Model( type );
        assert( RT_EntityClass( &entities[0] ) < 0 );
        assert( vk_rt_surface_owner( 0, &tracedShader, 0, SF_MD3 ) == RT_SURFACE_RASTER );
    }
    for ( int reType = RT_POLY; reType < RT_MAX_REF_ENTITY_TYPE; ++reType ) {
        Model( MOD_MESH )->e.reType = (refEntityType_t)reType;
        assert( RT_EntityClass( &entities[0] ) < 0 );
        assert( vk_rt_surface_owner( 0, &tracedShader, 0, SF_ENTITY ) == RT_SURFACE_RASTER );
    }
    for ( qhandle_t handle : { -1, 0, 16, 1 << 20 } ) {
        Model( MOD_MESH )->e.hModel = handle;
        assert( RT_EntityClass( &entities[0] ) < 0 );
    }
    /* Classes: the view weapon only in the primary view, never a portal's
     * or with first-person entities suppressed; a third-person body is kept
     * for its shadow, whatever else it is flagged. */
    Model( MOD_MESH )->e.renderfx = RF_FIRST_PERSON;
    assert( RT_EntityClass( &entities[0] ) == 1 );
    entities[0].e.renderfx = RF_DEPTHHACK;
    assert( RT_EntityClass( &entities[0] ) == 1 );
    backEnd.refdef.rdflags = RDF_NOFIRSTPERSON;
    assert( RT_EntityClass( &entities[0] ) < 0 );
    backEnd.refdef.rdflags = 0;
    backEnd.viewParms.portalView = PV_PORTAL;
    assert( RT_EntityClass( &entities[0] ) < 0 );
    backEnd.viewParms.portalView = PV_NONE;
    entities[0].e.renderfx = RF_THIRD_PERSON;
    assert( RT_EntityClass( &entities[0] ) == 2 );
    entities[0].e.renderfx = RF_THIRD_PERSON | RF_FIRST_PERSON | RF_DEPTHHACK;
    assert( RT_EntityClass( &entities[0] ) == 2 );
    entities[0].e.renderfx = RF_MINLIGHT | RF_NOSHADOW | RF_LIGHTING_ORIGIN;
    assert( RT_EntityClass( &entities[0] ) == 0 );
    /* Elite Force's forced alpha blends an entity only while its alpha is 1
     * to 254 (Holomatch's fading holodeck room): raster draws it then, and
     * the trace keeps it at 0 and 255, where raster draws it as it is. */
    for ( int alpha : { 1, 128, 254 } ) {
        Model( MOD_MESH )->e.stvef.forceAlpha = qtrue;
        entities[0].e.shader.rgba[3] = (byte)alpha;
        assert( RT_EntityClass( &entities[0] ) < 0 );
        assert( vk_rt_surface_owner( 0, &tracedShader, 0, SF_MD3 ) == RT_SURFACE_RASTER );
        entities[0].e.stvef.forceAlpha = qfalse;
        assert( RT_EntityClass( &entities[0] ) == 0 );
    }
    for ( int alpha : { 0, 255 } ) {
        Model( MOD_MESH )->e.stvef.forceAlpha = qtrue;
        entities[0].e.shader.rgba[3] = (byte)alpha;
        assert( RT_EntityClass( &entities[0] ) == 0 );
        assert( vk_rt_surface_owner( 0, &tracedShader, 0, SF_MD3 ) == RT_SURFACE_TRACED );
    }
    /* Raster mode, or a failed tracer, leaves every surface to raster. */
    for ( int mode : { 0, 1 } ) {
        vk.caps.activeRtMode = mode;
        assert( vk_rt_surface_owner( world, &tracedShader, 0, SF_FACE ) == RT_SURFACE_RASTER );
        assert( vk_rt_surface_owner( 0, &tracedShader, 0, SF_MD3 ) == RT_SURFACE_RASTER );
    }
    vk.caps.activeRtMode = 2;
    s_rt.failed = qtrue;
    assert( vk_rt_surface_owner( world, &tracedShader, 0, SF_FACE ) == RT_SURFACE_RASTER );
    assert( vk_rt_surface_owner( world, &skyShader, 0, SF_FACE ) == RT_SURFACE_RASTER );
    s_rt.failed = qfalse;
    return 0;
}
'''


class RtxSurfaceOwnerTests(unittest.TestCase):
    def test_trace_owns_posed_models_and_opaque_world_surfaces_only(self):
        compile_run(self, host() + CHECKS)


if __name__ == '__main__':
    unittest.main()
