"""Raven's alphaFunc GE192 through each renderer's whole shader parser.

All four Jedi engines know a fourth alphaFunc beside Quake III's GT0, LT128
and GE128: their NameToAFunc returns state bit 0x80000000 for GE192 (jamp.exe
1.01 0x4b24da, jasp.exe 1.01 0x4785f0, jk2mp.exe 1.02a 0x48dc72, jk2sp.exe
0x47daa2) and GL_State applies it as glAlphaFunc( GL_GEQUAL, 0.75f )
(0x48a4c2, 0x454bf2, 0x46d3d8, 0x45c3f8). fnTech3 stores it in the
comparison-and-reference alpha test of Alice's `alphatest` -- greaterequal,
reference 192, applied as 192 / 256 -- for jka-sp, jka-mp, jk2-sp and jk2-mp
only; every other profile still warns and draws the stage untested, as its
retail engine has no such name. The GL renderer keeps such stages off its
static world buffers and GLX streaming, whose programs know only Quake III's
three tests. The fixture is alice_shader_parse_tests' host around each
renderer's tr_shader.c; the shader lines are written for this test (record
docs/fntech3/2026-10-16-jedi-alphafunc-ge192.md).
"""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
static int failures;
#define CHECK(test) do { if(!(test)) { fprintf(stderr,"failed line %d: %s (%s)\n",__LINE__,#test,hostWarning);++failures; } } while(0)
#define ATEST(st) ( (st)->stateBits & ( GLS_ATEST_BITS | GLS_ALICE_ATEST_BITS ) )
static void *HostHunkAlloc( int size, ha_pref preference ) { (void)preference; return calloc( 1, (size_t)size ); }
#ifdef RENDERER_CLASSIC
/* SortNewShader walks an empty render command list */
backEndData_t *backEndData;
#endif
static qboolean Parse( comGameId_t game, const char *text ) {
 HostInit( game );
 ri.Hunk_Alloc = HostHunkAlloc;
#ifdef RENDERER_CLASSIC
 if ( !backEndData ) backEndData = (backEndData_t *)calloc( 1, sizeof( *backEndData ) );
#endif
 return HostParse( "textures/test/grass", text, text + strlen( text ) );
}
static qboolean Jedi( comGameId_t game ) {
 return game == COM_GAME_JKA_SP || game == COM_GAME_JKA_MP || game == COM_GAME_JK2_SP || game == COM_GAME_JK2_MP ? qtrue : qfalse;
}
static int HostChecks( void ) {
 const char *grate = "{\n{\nmap textures/test/grate\nalphaFunc GE192\nblendFunc GL_ONE GL_ZERO\ndepthWrite\nrgbGen identity\n}\n}\n";
 const char *lower = "{\n{\nmap textures/test/grate\nalphafunc ge192\ndepthWrite\n}\n}\n";
 const char *sprite = "{\n{\nmap textures/test/ground\n}\n{\nmap gfx/test/grass\nsurfaceSprites vertical 32 24 42 500\n"
  "ssFademax 1500\nssVariance 1 2\nssWind 0.5\nalphaFunc GE192\nblendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\ndepthWrite\n}\n}\n";
 const unsigned ge192 = ( 5u << GLS_ALICE_ATEST_FUNC_SHIFT ) | ( 192u << GLS_ALICE_ATEST_REF_SHIFT );
 comGameId_t game;
 CHECK( GLS_JEDI_ATEST_GE_C0 == ge192 );
 /* 192 / 256, the reference GL_State and vk.c apply, is retail's 0.75 exactly */
 CHECK( ( ( ge192 & GLS_ALICE_ATEST_REF_BITS ) >> GLS_ALICE_ATEST_REF_SHIFT ) * ( 1.0f / 256.0f ) == 0.75f );
 for ( game = COM_GAME_QUAKE3; game < COM_GAME_COUNT; game = (comGameId_t)( game + 1 ) ) {
  const qboolean jedi = Jedi( game );
  /* Quake III's three are every profile's, unchanged */
  CHECK( Parse( game, "{\n{\nmap a.tga\nalphaFunc GT0\n}\n}\n" ) && ATEST( &stages[0] ) == GLS_ATEST_GT_0 );
  CHECK( Parse( game, "{\n{\nmap a.tga\nalphaFunc LT128\n}\n}\n" ) && ATEST( &stages[0] ) == GLS_ATEST_LT_80 );
  CHECK( Parse( game, "{\n{\nmap a.tga\nalphaFunc GE128\n}\n}\n" ) && ATEST( &stages[0] ) == GLS_ATEST_GE_80 );
  /* an unknown name only warns, in every profile, as retail's does */
  CHECK( Parse( game, "{\n{\nmap a.tga\nalphaFunc GE64\n}\n}\n" ) && ATEST( &stages[0] ) == 0 &&
   strstr( hostWarning, "invalid alphaFunc name 'GE64'" ) );
  CHECK( Parse( game, grate ) );
  if ( !jedi ) {
   CHECK( ATEST( &stages[0] ) == 0 && strstr( hostWarning, "invalid alphaFunc name 'GE192'" ) );
   CHECK( Parse( game, lower ) && ATEST( &stages[0] ) == 0 );
   continue;
  }
  CHECK( !hostWarning[0] && ATEST( &stages[0] ) == ge192 );
  CHECK( ( stages[0].stateBits & GLS_DEPTHMASK_TRUE ) && !( stages[0].stateBits & GLS_BLEND_BITS & ~( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO ) ) );
  /* retail compares the name without case (Q_stricmpn, 99999) */
  CHECK( Parse( game, lower ) && !hostWarning[0] && ATEST( &stages[0] ) == ge192 );
  /* a surface sprite stage keeps it until FinishShader takes the stage */
  CHECK( Parse( game, sprite ) && !hostWarning[0] && ATEST( &stages[0] ) == 0 && ATEST( &stages[1] ) == ge192 );
#ifdef RENDERER_CLASSIC
  {
   shader_t *finished;
   /* the GL renderer draws such stages on the CPU path, where GL_State
    * applies the comparison and reference */
   CHECK( Parse( game, grate ) );
   finished = FinishShader();
   CHECK( finished && finished != tr.defaultShader && finished->aliceNoStatic &&
    finished->stages[0] && finished->stages[0]->aliceDynamic && ATEST( finished->stages[0] ) == ge192 );
   CHECK( Parse( game, sprite ) );
   finished = FinishShader();
   CHECK( finished && finished->numJediSprites == 1 && finished->stages[0] && !finished->stages[0]->aliceDynamic );
   if ( finished && finished->numJediSprites == 1 ) {
    const shader_t *pass = finished->jediSprites[0].shader;
    CHECK( pass && pass->aliceNoStatic && pass->stages[0] && pass->stages[0]->aliceDynamic && ATEST( pass->stages[0] ) == ge192 );
   }
   /* Quake III's own tests stay on the static and streamed paths */
   CHECK( Parse( game, "{\n{\nmap a.tga\nalphaFunc GE128\ndepthWrite\n}\n}\n" ) );
   finished = FinishShader();
   CHECK( finished && !finished->aliceNoStatic && finished->stages[0] && !finished->stages[0]->aliceDynamic );
  }
#endif
 }
 return failures ? 1 : 0;
}
'''


class JediAlphaFuncGE192Tests(unittest.TestCase):
    def test_whole_parser_under_every_profile(self):
        from alice_shader_parse_tests import RENDERERS, build_host
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='jedi-ge192-host-', dir=ROOT / '.tmp') as temp:
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    exe = build_host(Path(temp), f'host-{renderer}', CHECKS, renderer=renderer)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_back_ends_apply_the_reference(self):
        # GL_State and both Vulkan families read the comparison and reference
        # from the same bits, as ref / 256; the GL static and GLX streamed
        # paths defer a stage the parser marked to the CPU path.
        backend = (ROOT / 'code/renderer/tr_backend.c').read_text(encoding='utf-8')
        self.assertIn('qglAlphaFunc( func < ARRAY_LEN( aliceFuncs ) ? aliceFuncs[func] : GL_ALWAYS, ref * ( 1.0f / 256.0f ) );', backend)
        self.assertIn('GL_ALWAYS, GL_EQUAL, GL_NOTEQUAL, GL_GREATER, GL_LESS, GL_GEQUAL, GL_LEQUAL', backend)
        for renderer in ('renderervk', 'rendererrtx'):
            vk = (ROOT / 'code' / renderer / 'vk.c').read_text(encoding='utf-8')
            self.assertIn('frag_spec_data[1].f = (float)ref * ( 1.0f / 256.0f );', vk)
            self.assertIn('static const int aliceDiscard[7] = { 0, 4, 1, 5, 2, 3, 6 };', vk)
        self.assertIn('if ( shader->aliceNoStatic )\n\t\treturn qfalse;',
                      (ROOT / 'code/renderer/tr_vbo.c').read_text(encoding='utf-8'))
        self.assertIn('if ( pStage->aliceDynamic ) return qfalse;',
                      (ROOT / 'code/renderer/tr_shade.c').read_text(encoding='utf-8'))
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text(encoding='utf-8')
            name_to = source[source.index('static unsigned NameToAFunc('):]
            name_to = name_to[:name_to.index('\n}\n')]
            self.assertIn('R_JediAlphaFuncGE192( funcname )', name_to)
            self.assertIn('return GLS_JEDI_ATEST_GE_C0;', name_to)


if __name__ == '__main__':
    unittest.main()
