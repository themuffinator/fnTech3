"""American McGee's Alice's renderer features stay alike in all three renderers.

The OpenGL renderer (code/renderer) holds the reference implementation of
Alice's level of detail, entity lighting, view filter, lens flares, typed
dynamic lights and loading-screen stages; the Vulkan and RTX renderers carry
the same functions. Their front-end text must match GL's exactly. The lens
flares' back end differs only in how it reaches the view's normalised device
coordinates, which Vulkan does through its own matrices rather than GL's
matrix stack.
"""
from pathlib import Path
import re
import unittest

from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
VULKAN = ('renderervk', 'rendererrtx')


def read(renderer, name):
    return (ROOT / 'code' / renderer / name).read_text(encoding='latin-1')


ALICE_FUNCTIONS = (
    'static float R_AliceProjectRadius(',
    'static int R_AliceLodSlot(',
    'static void R_AliceTikiLod(',
    'static qboolean R_AliceTikiLightOrigin(',
    'qboolean R_AliceEntityInView(',
    'void R_AliceRegisterCvars(',
    'static void R_AliceLoadLensFlare(',
    'void R_AliceInitLensFlares(',
    'void R_AliceSetSunFlare(',
    'void R_AliceSetSunFlareInPortalSky(',
    'static void R_AliceFlareSetup(',
    'static qboolean R_AliceFlareProject(',
    'static float R_AliceFlareDot(',
    'static qboolean R_AliceFlareVisible(',
    'static void RB_AliceFlareQuad(',
    'static void RB_AliceFlareChain(',
    'static void RB_AliceFlareLight(',
    'static void RB_AliceFlareFullscreen(',
    'static void RB_AliceFlareShaderColor(',
    'static void R_AlicePlaceSunFlare(',
)


class AliceRendererParityTests(unittest.TestCase):
    def test_front_end_functions_are_gl_text(self):
        gl_alice = read('renderer', 'tr_alice.c')
        gl_light = read('renderer', 'tr_light.c')
        gl_scene = read('renderer', 'tr_scene.c')
        for renderer in VULKAN:
            alice = read(renderer, 'tr_alice.c')
            for signature in ALICE_FUNCTIONS:
                with self.subTest(renderer=renderer, function=signature):
                    self.assertEqual(function(alice, signature), function(gl_alice, signature))
            with self.subTest(renderer=renderer, function='R_SetupAliceEntityLighting'):
                self.assertEqual(function(read(renderer, 'tr_light.c'), 'void R_SetupAliceEntityLighting('),
                                 function(gl_light, 'void R_SetupAliceEntityLighting('))
            with self.subTest(renderer=renderer, function='RE_AddAliceLightToScene'):
                self.assertEqual(function(read(renderer, 'tr_scene.c'), 'void RE_AddAliceLightToScene('),
                                 function(gl_scene, 'void RE_AddAliceLightToScene('))

    def test_tiki_surfaces_use_lod_lighting_and_shadows(self):
        gl_alice = read('renderer', 'tr_alice.c')
        gl_shadows = gl_alice[gl_alice.index('\t\t/* Shadows go ahead of the surface,'):
                              gl_alice.index('\t\tif ( !personalModel ) {\n\t\t\tR_AddDrawSurf( &surface->surfaceType, shader, fogNum, 0 );')]
        for renderer in VULKAN:
            with self.subTest(renderer=renderer):
                alice = read(renderer, 'tr_alice.c')
                add = function(alice, 'void R_AddAliceTikiSurfaces(')
                self.assertIn('\tR_AliceTikiLod( ent, mod );\n', add)
                self.assertIn('R_SetupAliceEntityLighting( &tr.refdef, ent, lightOrigin, lightPoint, 0.5f * radius );',
                              add)
                self.assertIn('R_SetupAliceEntityLighting( &tr.refdef, ent, lightOrigin, NULL, 0.0f );', add)
                self.assertNotIn('R_SetupEntityLighting(', add)
                self.assertIn(gl_shadows, add)
                tess = function(alice, 'void RB_SurfaceAliceTiki(')
                self.assertIn('RA_TikiTessellateLod(', tess)
                self.assertIn('ent->aliceLod[R_AliceLodSlot( &backEnd.viewParms )]', tess)

    def test_tiki_animation_state_is_checked_before_anything_reads_it(self):
        # retail corrects a bad animation or frame to 0 before it culls
        # (alice.exe 0x004a63a0..0x004a64e3); every renderer asks the shared
        # TIKI code to, first, with the same words
        check = ('\tRA_TikiCheckAnimation( mod->aliceModel, &ent->e.alice, mod->name, ri.Printf );\n')
        for renderer in ('renderer',) + VULKAN:
            with self.subTest(renderer=renderer):
                add = function(read(renderer, 'tr_alice.c'), 'void R_AddAliceTikiSurfaces(')
                self.assertEqual(add.count(check), 1)
                self.assertLess(add.index(check), add.index('if ( R_CullLocalBox( bounds ) == CULL_OUT ) {'))
                self.assertLess(add.index(check), add.index('RA_TikiPose('))
                self.assertLess(add.index(check), add.index('R_AliceTikiLod( ent, mod );'))
        shared = (ROOT / 'code/renderercommon/tr_alice_tiki.cpp').read_text(encoding='latin-1')
        # frames are corrected to 0, not clamped to the last frame
        self.assertNotIn('- 1 : e->frame', shared)
        self.assertNotIn('- 1 : e->oldframe', shared)
        # a skeletal surface is an index into the reader's records
        lookup = function(shared, 'const model::SkbSurface *SkelSurfaceAt(')
        self.assertIn('f->skbSurfaces[', lookup)
        self.assertNotIn('ofsSurfaces', lookup)

    def test_tiki_surface_flags_choose_the_raw_image_mipmaps_and_picmip(self):
        # retail passes nomipmaps and nopicmip apart, with wrapping on
        # (alice.exe 0x0048989a..0x004898be into R_FindShader 0x0048afe0)
        gl_load = function(read('renderer', 'tr_alice.c'), 'qboolean R_LoadAliceTiki(')
        self.assertIn('R_FindAliceSurfaceShader( shaderName,\n'
                      '\t\t\t\t( flags & ALICE_SURFACE_NOMIPMAPS ) ? qfalse : qtrue,\n'
                      '\t\t\t\t( flags & ALICE_SURFACE_NOPICMIP ) ? qfalse : qtrue )', gl_load)
        self.assertNotIn('R_FindShader(', gl_load)
        gl_find = function(read('renderer', 'tr_shader.c'), 'shader_t *R_FindAliceSurfaceShader(')
        self.assertIn('LIGHTMAP_NONE, qtrue, qfalse, qfalse,\n'
                      '\t\t( mipmaps ? IMGFLAG_MIPMAP : 0 ) | ( picmip ? IMGFLAG_PICMIP : 0 ) );', gl_find)
        for renderer in ('renderer',) + VULKAN:
            with self.subTest(renderer=renderer):
                self.assertEqual(function(read(renderer, 'tr_alice.c'), 'qboolean R_LoadAliceTiki('), gl_load)
                shader = read(renderer, 'tr_shader.c')
                self.assertEqual(function(shader, 'shader_t *R_FindAliceSurfaceShader('), gl_find)
                internal = function(shader, 'static shader_t *R_FindShaderInternal(')
                # the chosen flags replace the raw image's, and only when chosen;
                # r_picmipFilter may still take picmip away from Ritual's and
                # Medal of Honor's (ritual_shader_gaps_tests.py)
                self.assertIn('else if ( rawImageFlags >= 0 )\n\t\t{\n'
                              '\t\t\t// the caller chose the image\'s flags (R_FindAliceSurfaceShader)\n'
                              '\t\t\tflags = (imgFlags_t)rawImageFlags;\n', internal)
                chosen = internal.index('else if ( rawImageFlags >= 0 )')
                self.assertLess(internal.index('if ( ( COM_GAME_ID_IS_RITUAL( ri.gameId ) || ri.gameId == COM_GAME_MOHAA ) '
                                               '&& shader.noPicMip ) {', chosen),
                                internal.index('\t\telse if (mipRawImage)', chosen))
                self.assertEqual(shader.count('R_FindShaderInternal('), 5)
                self.assertEqual(shader.count(', -1 );'), 3)
                self.assertIn('#define ALICE_SURFACE_NOPICMIP\t\t0x200', read(renderer, 'tr_local.h'))

    def test_lens_flares_reach_normalised_device_coordinates(self):
        for renderer in VULKAN:
            with self.subTest(renderer=renderer):
                flares = function(read(renderer, 'tr_alice.c'), 'void RB_AliceLensFlares(')
                self.assertNotIn('qgl', flares)
                self.assertIn('vk_update_mvp( aliceFlareNDC );', flares)
                self.assertIn('vk_update_mvp( NULL );', flares)
                self.assertIn('VPF_ALICE_SKY_PORTAL', flares)
                backend = read(renderer, 'tr_backend.c')
                self.assertRegex(backend, r'RB_RenderFlares\(\);\n\n\t// Alice.s lens flares[^\n]*\n[^\n]*\n\tRB_AliceLensFlares\(\);')
                main = read(renderer, 'tr_main.c')
                self.assertIn('if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) && !R_AliceEntityInView( ent ) ) {', main)
                bsp = read(renderer, 'tr_bsp.c')
                self.assertIn('!strcmp( keyname, "sunflare" )', bsp)
                self.assertIn('!strcmp( keyname, "sunflare_inportalsky" )', bsp)
                init = read(renderer, 'tr_init.c')
                self.assertIn('R_AliceRegisterCvars();', init)
                self.assertIn('R_AliceInitLensFlares();', init)
                self.assertIn('re.AddAliceLightToScene = RE_AddAliceLightToScene;', init)
                self.assertIn('ri.Cvar_Get( "r_lodbias", COM_GAME_ID_IS_RITUAL( ri.gameId ) ? "0" : "-2", CVAR_ARCHIVE_ND )', init)
                # Alice's r_lodscale is unprotected; Medal of Honor's is its
                # own archived setting (tests/mohaa_permanent_decal_tests.py)
                self.assertIn('ri.Cvar_Get( "r_lodscale", ri.gameId == COM_GAME_MOHAA ? "1.1" : "5",\n'
                              '\t\tCOM_GAME_ID_IS_RITUAL( ri.gameId ) ? 0 : ri.gameId == COM_GAME_MOHAA ? '
                              'CVAR_ARCHIVE : CVAR_CHEAT )', init)
                self.assertEqual(read(renderer, 'tr_scene.c').count('dl->aliceFlare = 0;'), 2)
                # the stencil shadows' multiply is GL's 0.6, not the fog
                # colour or intensity the last stage's uniform carries, in
                # every profile: RTCW's global fog darkened it as Alice's did
                finish = function(read(renderer, 'tr_shadows.c'), 'void RB_ShadowFinish(')
                self.assertIn('\t{\n\t\tvkUniform_t finishUniform;', finish)
                self.assertIn('finishUniform.texFactors[0] = 1.0f;', finish)
                self.assertNotIn('ri.gameId', finish)
                self.assertLess(finish.index('VK_PushUniform( &finishUniform )'),
                                finish.index('vk_bind_pipeline( vk.shadow_finish_pipeline );'))

    def test_world_load_sets_the_loading_stages(self):
        public = (ROOT / 'code/renderercommon/tr_public.h').read_text(encoding='latin-1')
        imports = public[public.index('typedef struct refimport_s {'):public.index('} refimport_t;')]
        # appended after every earlier entry, so a shorter table is a prefix;
        # only Medal of Honor's table, appended since, follows it
        stage = 'void\t(*AliceLoadingStage)( int stage );'
        after = [line.strip() for line in imports[imports.index(stage) + len(stage):].splitlines()
                 if line.strip() and not line.strip().startswith('//')]
        self.assertEqual(after, ['const struct mohaaRenderImport_s *MOHAARender;'])
        client = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='latin-1')
        self.assertIn('rimp.AliceLoadingStage = Com_AliceLoadingStage;', client)
        for renderer in ('renderer',) + VULKAN:
            with self.subTest(renderer=renderer):
                bsp = read(renderer, 'tr_bsp.c')
                load = function(bsp, 'void RE_LoadWorldMap( const char *name ) {')
                body = load[load.index('{'):]
                pattern = r'if \( COM_GAME_ID_IS_RITUAL\( ri\.gameId \) && ri\.AliceLoadingStage \) \{\n\t\tri\.AliceLoadingStage\( (\d) \);\n\t\}'
                calls = list(re.finditer(pattern, body))
                self.assertEqual([m.group(1) for m in calls], ['7', '8'])
                # 7 before anything the load does, 8 after everything; inline,
                # since fixtures (bsp_profile_loader_tests) slice this alone
                self.assertLess(calls[0].start(), body.index('if ( tr.worldMapLoaded )'))
                self.assertEqual(body[calls[1].end():].strip(), '}')


if __name__ == '__main__':
    unittest.main()
