"""Run Academy's sky portal view through the adapter and the renderers' own rule.

The retail 1.01 client game draws a level's sky portal first in each frame, as
a scene of its own with Raven's rdflags 0x18 (cgamex86.dll 0x30062719), then
the view it is seen from with 0x10 (0x3002df11). Retail jamp.exe 1.01's
renderer latches a level-wide flag on the portal's scene (RE_RenderScene
0x4abe18), clears it as a world loads (0x490fc3), and while it is set leaves
every other view's sky undrawn (RB_EndSurface 0x4af5c9, RB_StageIteratorSky
0x4b96ad), so the portal's picture shows there.

This compiles the production adapter (code/compat/jka_render_adapter.hpp), the
shared rule (code/renderercommon/tr_jka_sky_portal.h) and each renderer's own
scene lines, sliced out of its tr_scene.c, and runs a level's frames through
them. See docs/fntech3/2026-10-15-jka-siege.md.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
HOOK = "\t// Jedi Academy's sky portal: once its scene has been drawn in this"
MEDIUM = "\t// Jedi Academy's sky portal is part of what the eye sees, drawn before"


def read(path):
    return (ROOT / path).read_text(encoding='utf-8').replace('\r\n', '\n')


def block(text, start):
    """The `if` block that follows a comment, through its closing brace."""
    first = text.index(start)
    return text[first:text.index('\n\t}\n', first) + len('\n\t}\n')]


def adapter_source():
    return r'''
#include "code/compat/jka_render_adapter.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
namespace raven = fntech3::jka::render;
static fntech3_jka_mp_refdef_t Valid() {
	fntech3_jka_mp_refdef_t view{};
	view.width = 640; view.height = 480; view.fov_x = 90; view.fov_y = 73.74f;
	view.vieworg[0] = 7424; view.vieworg[1] = -128; view.vieworg[2] = 1160;
	for ( int axis = 0; axis < 3; ++axis ) view.viewaxis[axis][axis] = 1;
	view.time = 4321; view.areamask[3] = 0x10;
	return view;
}
int main() {
	const auto base = Valid();
	refdef_t plain{};
	assert( raven::View( base, plain ) == raven::Status::Ok && !plain.jka.skyboxPortal && !plain.rdflags );
	// Every combination of Raven's eight named flags: served exactly when it
	// asks for nothing but no world (0x1), hyperspace (0x4), the sky portal's
	// scene (0x8) with its world (0x10), and draw-sky (0x10).
	for ( uint32_t flags = 0; flags < 0x100; ++flags ) {
		auto view = base; view.rdflags = int32_t( flags );
		const bool served = !( flags & ~0x1du ) && !( ( flags & 0x8u ) && !( flags & 0x10u ) );
		refdef_t out{}; std::memset( &out, 0x5a, sizeof( out ) );
		const auto before = out;
		const auto status = raven::View( view, out );
		assert( ( status == raven::Status::Ok ) == served );
		const std::string name = raven::ViewFlagsName( view.rdflags );
		if ( !served ) {
			assert( status == raven::Status::ViewFlags && !std::memcmp( &out, &before, sizeof( out ) ) );
			assert( name != "served" );
			if ( flags & 0x20u ) assert( name == "the automap (0x20)" );
			else if ( flags & 0x40u ) assert( name == "no global fog in this view (0x40)" );
			else if ( flags & 0x80u ) assert( name == "force sight (0x80)" );
			else if ( flags & 0x2u ) assert( name == "a bit Raven never named" );
			else assert( name == "a sky portal's scene without its world (0x8 without 0x10)" );
			continue;
		}
		assert( name == "served" );
		// The portal crosses as its own engine field and nothing else moves:
		// no Quake III flag stands for it, and the record is otherwise the
		// same conversion as a plain view's.
		assert( out.jka.skyboxPortal == ( ( flags & 0x8u ) ? qtrue : qfalse ) );
		assert( out.rdflags == int( ( flags & 1u ? RDF_NOWORLDMODEL : 0 ) | ( flags & 4u ? RDF_HYPERSPACE : 0 ) ) );
		auto expected = plain; expected.rdflags = out.rdflags; expected.jka = out.jka;
		assert( !std::memcmp( &out, &expected, sizeof( out ) ) );
	}
	for ( uint32_t high : { 0x100u, 0x118u, 0x80000018u, 0xffffffffu } ) {
		auto view = base; view.rdflags = int32_t( high );
		refdef_t out{};
		assert( raven::View( view, out ) == raven::Status::ViewFlags );
		assert( std::string( raven::ViewFlagsName( view.rdflags ) ) != "served" );
	}
	// The exact words the retail client game submits.
	for ( int32_t flags : { 0x10, 0x18, 0x1, 0x5 } ) {
		auto view = base; view.rdflags = flags; refdef_t out{};
		assert( raven::View( view, out ) == raven::Status::Ok );
	}
	for ( int32_t flags : { 0x21, 0x50 } ) {
		auto view = base; view.rdflags = flags; refdef_t out{};
		assert( raven::View( view, out ) == raven::Status::ViewFlags );
	}
	// A portal's record read through the boundary, unaligned.
	auto view = base; view.rdflags = 0x18;
	unsigned char bytes[sizeof( view ) + 3]; std::memcpy( bytes + 3, &view, sizeof( view ) );
	refdef_t out{};
	assert( raven::ReadView( bytes + 3, out ) == raven::Status::Ok && out.jka.skyboxPortal && !out.rdflags );
	assert( out.vieworg[0] == 7424 && out.areamask[3] == 0x10 && out.time == 4321 );
	std::puts( "sky portal adapter: ok" );
	return 0;
}
'''


def scene_source(renderer):
    scene = read(f'code/{renderer}/tr_scene.c')
    hook = block(scene, HOOK)
    medium = block(scene, MEDIUM)
    portal_scene = ''
    if renderer == 'rendererrtx':
        expression = re.search(r'tr\.refdef\.skyboxPortalScene = (.*?);\n', scene, re.S)[1]
        portal_scene = 'tr.refdef.skyboxPortalScene = ' + expression + ';'
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/renderercommon/tr_types.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
static struct { int gameId; } ri;
static int usesRTCWLighting;
static qboolean R_UsesRTCWLighting() { return usesRTCWLighting ? qtrue : qfalse; }
static struct {
	qboolean jkaSkyPortal;
	struct {
		struct { qboolean drawSky; } rtcwFog;
		int underwaterContents; float underwaterStrength;
		qboolean skyboxPortalScene;
	} refdef;
} tr;
#include "code/renderercommon/tr_jka_sky_portal.h"
// The scene's own lines, in RE_RenderScene's order: the draw-sky word is the
// scene's copy, captured as R_CopyRTCWFogToScene leaves it (qtrue for this
// profile), then this profile's rule; the medium after the underwater copy.
static qboolean Scene( const refdef_t *fd, float medium ) {
	tr.refdef.rtcwFog.drawSky = qtrue;
	tr.refdef.underwaterContents = medium > 0 ? 32 : 0;
	tr.refdef.underwaterStrength = medium;
	''' + portal_scene + '\n' + hook + medium + r'''
	return tr.refdef.rtcwFog.drawSky;
}
static refdef_t View( int portal, int noWorld ) {
	refdef_t view{}; view.jka.skyboxPortal = portal ? qtrue : qfalse;
	view.rdflags = noWorld ? RDF_NOWORLDMODEL : 0;
	return view;
}
int main() {
	ri.gameId = COM_GAME_JKA_MP;
	const auto portal = View( 1, 0 ), eye = View( 0, 0 ), model = View( 0, 1 );
	// A level whose views come before any portal draws its sky as usual.
	assert( Scene( &eye, 0 ) && !tr.jkaSkyPortal );
	// The portal's scene draws its own sky and marks the level...
	assert( Scene( &portal, 0.5f ) && tr.jkaSkyPortal );
	// ...and takes no medium: the eye's view composites it once.
	assert( !tr.refdef.underwaterContents && tr.refdef.underwaterStrength == 0 );
	// The view it is seen from leaves its sky to it, with the eye's medium.
	assert( !Scene( &eye, 0.5f ) && tr.refdef.underwaterStrength == 0.5f && tr.refdef.underwaterContents == 32 );
	// So does every later view of the level, models' scenes included, frame
	// after frame; the portal's own scene keeps drawing its sky.
	for ( int frame = 0; frame < 3; ++frame ) {
		assert( Scene( &portal, 0 ) && tr.jkaSkyPortal );
		assert( !Scene( &eye, 0 ) && !Scene( &model, 0 ) );
	}
	// The next level starts with `tr` cleared (R_Init), so its own first views
	// draw their sky until its own portal appears, if it has one.
	std::memset( &tr, 0, sizeof( tr ) );
	assert( Scene( &eye, 0 ) && !tr.jkaSkyPortal );
	// Another profile's scenes are never touched by Academy's rule.
	for ( int game : { int( COM_GAME_QUAKE3 ), int( COM_GAME_RTCW_MP ), int( COM_GAME_WOLFET ), int( COM_GAME_JKA_SP ) } ) {
		std::memset( &tr, 0, sizeof( tr ) ); ri.gameId = game;
		usesRTCWLighting = game == COM_GAME_RTCW_MP;
		assert( Scene( &portal, 0.5f ) && Scene( &eye, 0.5f ) && !tr.jkaSkyPortal );
		assert( tr.refdef.underwaterStrength == 0.5f );
	}
#ifdef RTX_SCENE
	// The RTX renderer's path tracer leaves a portal's scene to raster.
	std::memset( &tr, 0, sizeof( tr ) ); ri.gameId = COM_GAME_JKA_MP; usesRTCWLighting = 0;
	Scene( &portal, 0 ); assert( tr.refdef.skyboxPortalScene );
	Scene( &eye, 0 ); assert( !tr.refdef.skyboxPortalScene );
	ri.gameId = COM_GAME_QUAKE3; Scene( &portal, 0 ); assert( !tr.refdef.skyboxPortalScene );
#endif
	assert( FnJKASkyPortalDrawsSky( nullptr, &eye ) && FnJKASkyPortalDrawsSky( &tr.jkaSkyPortal, nullptr ) );
	std::puts( "sky portal scene: ok" );
	return 0;
}
'''


class JKASkyPortalTests(unittest.TestCase):
    def compile_run(self, source, name, definitions=()):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        parent = ROOT / '.tmp/tests/jka-sky-portal'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary).resolve()
            cpp = folder / f'{name}.cpp'
            exe = folder / f'{name}.exe'
            cpp.write_text(source, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([compiler, *target, *definitions, '-O2', '-std=c++17',
                                    '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=folder, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            return run.stdout

    def test_adapter_serves_the_portal_scene_and_names_every_refusal(self):
        self.assertIn('sky portal adapter: ok', self.compile_run(adapter_source(), 'adapter'))

    def test_each_renderer_leaves_the_sky_to_the_portal(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                definitions = ('-DRTX_SCENE',) if renderer == 'rendererrtx' else ()
                self.assertIn('sky portal scene: ok',
                              self.compile_run(scene_source(renderer), f'scene-{renderer}', definitions))

    def test_the_rule_is_wired_where_each_renderer_draws_and_resets(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                scene = read(f'code/{renderer}/tr_scene.c')
                render = function(scene, 'void RE_RenderScene(')
                # The rule follows the scene's draw-sky capture and the medium
                # follows the underwater copy, before the view is issued.
                self.assertLess(render.index('R_CopyRTCWFogToScene( fd );'), render.index(HOOK))
                self.assertLess(render.index('R_CopyUnderwaterViewToRefdef( tr.refdef.time );'), render.index(MEDIUM))
                self.assertIn('#include "../renderercommon/tr_jka_sky_portal.h"', scene)
                # The latch lives in `tr`, which R_Init clears before every
                # world load, as retail's RE_LoadWorldMap clears its own.
                local = read(f'code/{renderer}/tr_local.h')
                self.assertIn('qboolean\t\t\t\tjkaSkyPortal;', local)
                self.assertIn('Com_Memset( &tr, 0, sizeof( tr ) );', function(read(f'code/{renderer}/tr_init.c'),
                                                                              'void R_Init( void ) {'))
                # A view that leaves its sky draws none: the sky iterator is the
                # only way a sky is drawn, and it reads the scene's word.
                sky = function(read(f'code/{renderer}/tr_sky.c'), 'void RB_StageIteratorSky( void ) {')
                self.assertIn('!backEnd.refdef.rtcwFog.drawSky', sky)
        registration = function(read('code/renderer/tr_model.c'), 'void RE_BeginRegistration( glconfig_t *glconfigOut ) {')
        self.assertIn('R_Init();', registration)

    def test_client_hooks_treat_the_portal_as_part_of_the_eyes_view(self):
        client = read('code/client/cl_cgame.cpp')
        for name in ('static void CL_UpdateUnderwaterView( const refdef_t *refdef ) {',
                     'static void CL_UpdateLocalViewLiquidInteraction( const refdef_t *refdef ) {'):
            with self.subTest(hook=name):
                body = function(client, name)
                # Enemy Territory's and RTCW's portal skies are scenes of their
                # own the same way, and reach these hooks through their adapters.
                self.assertIn('if ( refdef->jka.skyboxPortal || refdef->wolfet.skyboxPortal || '
                              'refdef->rtcw.skyboxPortal ) {\n\t\treturn;\n\t}', body)
                self.assertLess(body.index('RDF_NOWORLDMODEL'), body.index('refdef->jka.skyboxPortal'))
        editor = function(read('code/client/cl_entity_editor.cpp'), 'void CL_EntityEditorAdjustView( refdef_t *refdef )')
        self.assertIn('refdef->jka.skyboxPortal', editor[:editor.index('if ( !portal )')])
        host = read('code/client/client_jka_cgame_host.hpp')
        case = host[host.index('    case Call::CG_R_RENDERSCENE: {'):host.index('    case Call::CG_R_ADDLIGHTTOSCENE:')]
        self.assertIn('render::ViewFlagsName(source.rdflags)', case)


if __name__ == '__main__':
    unittest.main()
