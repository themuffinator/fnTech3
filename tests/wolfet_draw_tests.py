"""ET's skeletal meshes reaching the renderer: model types, surface and draw call."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
COMMON = ROOT/'code/renderercommon'
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-drawing.md'
PROBE = ROOT/'scripts/probe_wolfet_skeletal_draw.py'

FIXTURE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include <assert.h>
const vec3_t vec3_origin = {0,0,0};
'''

BODY = r'''
#include "code/renderercommon/tr_wolfet_model_format.h"

enum { HEADER = FN_WOLFET_MDM_HEADER_BYTES, SURFACE = FN_WOLFET_MDM_SURFACE_BYTES,
       VERTS = 3, TRIS = 1, REFS = 1,
       VERTEX = FN_WOLFET_MDM_VERTEX_PREFIX_BYTES + FN_WOLFET_MDM_WEIGHT_BYTES,
       VERTS_AT = SURFACE, TRIS_AT = VERTS_AT + VERTS * VERTEX,
       COLLAPSE_AT = TRIS_AT + TRIS * FN_WOLFET_MDM_TRIANGLE_BYTES,
       REFS_AT = COLLAPSE_AT + VERTS * 4, SURFACE_END = REFS_AT + REFS * 4,
       BYTES = HEADER + SURFACE_END };

static unsigned char mesh[BYTES];
static unsigned char *surface = mesh + HEADER;

static void PutI(unsigned char *p, int32_t value) { memcpy(p, &value, 4); }
static void PutF(unsigned char *p, float value) { memcpy(p, &value, 4); }

/* One surface of three vertices and one triangle, every offset written the way
 * the compiler writes them: relative to the surface, not to the file. */
static void Build(void) {
	int i;
	memset(mesh, 0, sizeof(mesh));
	PutI(mesh, (int32_t)FN_WOLFET_MDM_IDENT);
	PutI(mesh + 4, FN_WOLFET_MDM_VERSION);
	PutF(mesh + 72, 1);              /* detail scale */
	PutI(mesh + 80, 1);              /* one surface */
	PutI(mesh + 84, HEADER);
	PutI(mesh + 96, BYTES);
	memcpy(surface + 4, "body", 5);
	PutI(surface + 144, VERTS);
	PutI(surface + 148, VERTS_AT);
	PutI(surface + 152, TRIS);
	PutI(surface + 156, TRIS_AT);
	PutI(surface + 160, COLLAPSE_AT);
	PutI(surface + 164, REFS);
	PutI(surface + 168, REFS_AT);
	PutI(surface + 172, SURFACE_END);
	for (i = 0; i < VERTS; ++i) {
		unsigned char *v = surface + VERTS_AT + i * VERTEX;
		PutF(v + 8, 1);              /* normal along z */
		PutI(v + 20, 1);             /* one weight */
		PutF(v + FN_WOLFET_MDM_VERTEX_PREFIX_BYTES + 4, 1);   /* full weight on bone 0 */
	}
	for (i = 0; i < 3; ++i) PutI(surface + TRIS_AT + i * 4, i);
	/* A collapse entry always points at an earlier vertex. */
	PutI(surface + COLLAPSE_AT + 8, 1);
}

int main(void) {
	fnWolfETMeshView_t view;
	fnWolfETMeshSurfaceView_t read;

	Build();
	assert(FnWolfET_Mesh(mesh, BYTES, &view));
	assert(FnWolfET_MeshSurface(&view, 0, &read));
	assert(read.numVerts == VERTS && read.numTriangles == TRIS);

	/* A triangle naming a vertex the surface does not have would index past the
	 * collapse map the draw call builds, so it is refused at read. */
	PutI(surface + TRIS_AT + 4, VERTS);
	assert(!FnWolfET_MeshSurface(&view, 0, &read));
	Build();

	/* So is a collapse entry that does not point backwards. */
	PutI(surface + COLLAPSE_AT + 8, 2);
	assert(!FnWolfET_MeshSurface(&view, 0, &read));
	Build();
	PutI(surface + COLLAPSE_AT, 1);
	assert(!FnWolfET_MeshSurface(&view, 0, &read));
	Build();

	/* And a weight naming a bone no skeleton could hold. */
	PutI(surface + VERTS_AT + FN_WOLFET_MDM_VERTEX_PREFIX_BYTES, FN_WOLFET_MAX_BONES);
	assert(!FnWolfET_MeshSurface(&view, 0, &read));
	Build();

	/* A bone reference past that maximum is refused the same way. */
	PutI(surface + REFS_AT, FN_WOLFET_MAX_BONES);
	assert(!FnWolfET_MeshSurface(&view, 0, &read));
	Build();
	assert(FnWolfET_MeshSurface(&view, 0, &read));
	return 0;
}
'''


def source(name):
    return (ROOT/name).read_text(encoding='utf-8')


class WolfETDrawTests(unittest.TestCase):
    def test_a_surface_is_refused_before_the_draw_call_can_misindex_it(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/wolfet-draw-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            path = folder/'draw.cpp'
            executable = folder/('draw.exe' if os.name == 'nt' else 'draw')
            path.write_text(FIXTURE + BODY, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            elif os.name != 'nt':
                target = ['-m32']
            built = subprocess.run([compiler, *target, '-std=c++17',
                '-I', str(ROOT), str(path), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=180)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True,
                                 text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_every_renderer_knows_both_model_types(self):
        for name in RENDERERS:
            text = source('code/%s/tr_local.h' % name)
            self.assertIn('MOD_WOLFET_MESH', text, name)
            self.assertIn('MOD_WOLFET_SKELETON', text, name)
            self.assertIn('wolfetMeshModel_t\t*wolfetMesh;', text, name)
            self.assertIn('wolfetSkeletonModel_t\t*wolfetSkeleton;', text, name)
            # A surface type is only usable if its dispatch table entry lines up.
            types = text[text.index('SF_BAD'):text.index('SF_NUM_SURFACE_TYPES')]
            order = [t.strip().rstrip(',') for t in types.split('\n')]
            order = [t.split('//')[0].strip().rstrip(',') for t in order]
            order = [t for t in order if t.startswith('SF_')]
            self.assertEqual(order[-1], 'SF_WOLFET', name)
            table = source('code/%s/tr_surface.c' % name)
            table = table[table.index('rb_surfaceTable'):]
            table = table[:table.index('};')]
            entries = re.findall(r'RB_\w+\)*\s*,?\s*//\s*(SF_\w+)', table)
            self.assertEqual(entries, order, name)
            self.assertIn('RB_SurfaceWolfET', table, name)

    def test_both_extensions_load_and_only_the_mesh_draws(self):
        for name in RENDERERS:
            model = source('code/%s/tr_model.c' % name)
            self.assertIn('{ "mdm", R_RegisterWolfETMesh }', model, name)
            self.assertIn('{ "mdx", R_RegisterWolfETSkeleton }', model, name)
            # ET answers no bounds for either of its skeletal types, and neither
            # does this: a skeleton's own box is written greater corner first.
            bounds = model[model.index('void R_ModelBounds('):]
            bounds = bounds[:bounds.index('\n}\n')]
            self.assertNotIn('MOD_WOLFET', bounds, name)
            main = source('code/%s/tr_main.c' % name)
            self.assertIn('case MOD_WOLFET_MESH:', main, name)
            self.assertIn('R_AddWolfETSurfaces( ent );', main, name)
            self.assertIn('case MOD_WOLFET_SKELETON:', main, name)

    def test_the_draw_path_carries_no_other_profile(self):
        """An ET mesh is posed by ET's rules alone; nothing here may consult the
        running game or reach for the other Wolfenstein profile's record."""
        for name in ('tr_wolfet_model.h', 'tr_wolfet_entity.h', 'tr_wolfet_material.h',
                     'tr_wolfet_surface.h', 'tr_wolfet_add_surfaces.h'):
            text = (COMMON/name).read_text(encoding='utf-8')
            self.assertNotIn('ri.gameId', text, name)
            self.assertNotIn('COM_GAME_', text, name)
            self.assertNotIn('->rtcw.', text, name)

    def test_detail_and_bounds_come_from_the_named_skeleton(self):
        surface = (COMMON/'tr_wolfet_surface.h').read_text(encoding='utf-8')
        self.assertIn('R_WolfETEntityFrame( entity->wolfet.frameModel, entity->frame )', surface)
        add = (COMMON/'tr_wolfet_add_surfaces.h').read_text(encoding='utf-8')
        self.assertIn("ent->e.wolfet.frameModel", add)
        self.assertIn("ent->e.wolfet.oldframeModel", add)

    def test_a_forced_full_detail_entity_skips_the_detail_choice(self):
        """ET's limbo panel asks for its heads at full detail. The flag reached
        the render record already but nothing acted on it."""
        for name in RENDERERS:
            mesh = source('code/%s/tr_mesh.c' % name)
            self.assertIn('ent->e.wolfet.forceNoLod ) ? 0 : R_ComputeLOD', mesh, name)
        adapter = source('code/compat/render_record_adapter.hpp')
        self.assertIn('result.wolfet.forceNoLod = source.renderfx &', adapter)
        # It is a detail-file choice, so it does not reach the skeletal path,
        # whose detail ET reads from the reference entity's own flags instead.
        self.assertNotIn('forceNoLod', (COMMON/'tr_wolfet_surface.h').read_text(encoding='utf-8'))

    def test_the_probe_runs_the_arithmetic_over_retail_models(self):
        probe = PROBE.read_text(encoding='utf-8')
        for claim in ('FnWolfET_DecodePose', 'FnWolfET_SkinVertex',
                      'FnWolfET_CollapseVertices', 'FnWolfET_TagOrientation',
                      'installationUnchanged'):
            self.assertIn(claim, probe)

    def test_the_record_states_what_is_and_is_not_claimed(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('No image has been produced', record)
        self.assertIn('greater corner of its box first', record)


if __name__ == '__main__':
    unittest.main()
