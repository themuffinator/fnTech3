"""Alice's and F.A.K.K.2's static models in the world take their baked light.

Both games' map compiler turns a static model into vertex-lit world triangles,
and those surfaces keep the model's shader. Many of those shaders say
`rgbGen lightingDiffuse` (F.A.K.K.2's plant and fern leaves, banners, lamps and
cloth; 745 surfaces over six Alice maps), which lights by the current entity.
The world entity has no light, so this engine drew them black. Retail
fakk2.exe 1.02 calls no lighting function for the world entity and leaves the
colours it last wrote (0x0047d45a). This engine lights them with the colours
the map baked on their vertices, as it does Medal of Honor's static models.

This compiles each renderer's production RB_CalcDiffuseColor with a minimal
back end and checks the world and entity cases for every profile kind.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

FIXTURE = r'''
#include <assert.h>
#include <string.h>
typedef unsigned char byte;
typedef float vec3_t[3];
typedef struct { byte rgba[4]; } color4ub_t;
enum { COM_GAME_QUAKE3, COM_GAME_ALICE, COM_GAME_FAKK2, COM_GAME_MOHAA };
#define COM_GAME_ID_IS_RITUAL( id ) ( ( id ) == COM_GAME_ALICE || ( id ) == COM_GAME_FAKK2 )
typedef struct { int e; } trRefEntity_t;
static struct { int gameId; } ri;
static struct { const trRefEntity_t *currentEntity; } backEnd;
static struct { trRefEntity_t worldEntity; float identityLight; } tr;
static struct { int numVertexes; color4ub_t vertexColors[4]; } tess;
static trRefEntity_t model;
static int entityLit;
/* The entity path is the renderer's own; here it only records that it ran. */
static void RB_CalcDiffuseColor_scalar( unsigned char *colors ) {
    int i;
    entityLit++;
    for ( i = 0; i < tess.numVertexes * 4; i++ ) colors[i] = 7;
}
'''

CHECKS = r'''
int main( void ) {
    const int games[4] = { COM_GAME_QUAKE3, COM_GAME_ALICE, COM_GAME_FAKK2, COM_GAME_MOHAA };
    int g, i;
    tess.numVertexes = 2;
    tess.vertexColors[0].rgba[0] = 200; tess.vertexColors[0].rgba[1] = 100;
    tess.vertexColors[0].rgba[2] = 50; tess.vertexColors[0].rgba[3] = 9;
    tess.vertexColors[1].rgba[0] = 34; tess.vertexColors[1].rgba[1] = 29;
    tess.vertexColors[1].rgba[2] = 20; tess.vertexColors[1].rgba[3] = 9;
    for ( g = 0; g < 4; g++ ) {
        const int ritual = games[g] == COM_GAME_ALICE || games[g] == COM_GAME_FAKK2;
        float light;
        for ( light = 0.5f; light <= 1.0f; light += 0.5f ) {
            byte colors[8];
            ri.gameId = games[g];
            tr.identityLight = light;
            /* A world surface: the baked colours in a Ritual game, the entity
             * path (with the world entity's light) in every other. */
            backEnd.currentEntity = &tr.worldEntity;
            entityLit = 0;
            memset( colors, 0, sizeof( colors ) );
            RB_CalcDiffuseColor( colors );
            if ( ritual ) {
                assert( entityLit == 0 );
                for ( i = 0; i < 2; i++ ) {
                    assert( colors[i * 4 + 0] == (byte)( tess.vertexColors[i].rgba[0] * light ) );
                    assert( colors[i * 4 + 1] == (byte)( tess.vertexColors[i].rgba[1] * light ) );
                    assert( colors[i * 4 + 2] == (byte)( tess.vertexColors[i].rgba[2] * light ) );
                    assert( colors[i * 4 + 3] == 255 );
                }
            } else {
                assert( entityLit == 1 && colors[0] == 7 );
            }
            /* A model keeps its entity lighting everywhere. */
            backEnd.currentEntity = &model;
            entityLit = 0;
            RB_CalcDiffuseColor( colors );
            assert( entityLit == 1 && colors[0] == 7 );
        }
    }
    return 0;
}
'''


def production(renderer):
    source = (ROOT / f'code/{renderer}/tr_shade_calc.c').read_text(encoding='utf-8').replace('\r\n', '\n')
    start = source.index('void RB_CalcDiffuseColor( unsigned char *colors )\n{')
    return source[start:source.index('\n}\n', start) + 3]


class RitualWorldDiffuseTests(unittest.TestCase):
    def test_world_static_models_take_their_baked_colours(self):
        compiler = next((path for name in ('clang', 'gcc', 'cc') if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest('requires a C11 compiler')
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ritual-diffuse-', dir=scratch) as folder:
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    cfile = Path(folder) / f'{renderer}.c'
                    exe = Path(folder) / f'{renderer}.exe'
                    cfile.write_text(FIXTURE + production(renderer) + CHECKS, encoding='utf-8')
                    build = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c11', '-Wall', '-Wextra',
                                            '-Werror', str(cfile), '-o', str(exe)],
                                           capture_output=True, text=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_the_three_renderers_agree(self):
        self.assertEqual(len({production(renderer) for renderer in RENDERERS}), 1)


if __name__ == '__main__':
    unittest.main()
