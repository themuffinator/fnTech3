"""Raven's surface sprites: the stage words and the sprites drawn from them.

All four Jedi engines read `surfaceSprites <type> <width> <height> <density>
<fadedist>` and the optional ss words the same way (jamp.exe and jasp.exe
1.01, jk2mp.exe and jk2sp.exe 1.02a): the rest of the line goes to the
sprite parser, a bad or unknown word only warns, and only Academy single
player knows `flattened`. The stage then draws as sprites over the surface's
triangles after its own passes, from the second stage on. The first test runs
each renderer's whole production parser (alice_shader_parse_tests' host)
under every profile; the second compiles the production generator,
code/renderercommon/tr_jedi_sprite_draw.h, over a hand-made batch. The
shader lines are written for this test (record
docs/fntech3/2026-10-15-jedi-surface-sprites.md).
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

HOST_CHECKS = r'''
#include <math.h>
static int failures;
#define CHECK(test) do { if(!(test)) { fprintf(stderr,"failed line %d: %s (%s)\n",__LINE__,#test,hostWarning);++failures; } } while(0)
static qboolean Parse(comGameId_t game,const char *text) {
 HostInit(game);return HostParse("textures/test/grass",text,text+strlen(text));
}
static qboolean Near(float a,float b) { return fabsf(a-b)<1e-4f ? qtrue : qfalse; }
/* a ground shader: lightmap, texture, then a sprite stage */
static const char *Ground(const char *sprite) {
 static char text[2048];
 Com_sprintf(text,sizeof(text),"{\n{\nmap $lightmap\n}\n{\nmap textures/test/ground\nblendFunc GL_DST_COLOR GL_ZERO\n}\n"
  "{\nmap gfx/test/blade\n%s\nalphaFunc GE128\nblendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\nrgbGen vertex\n}\n}\n",sprite);
 return text;
}
static const jediSurfaceSprite_t *Sprite(void) { return &stages[2].jediSprite; }
static int HostChecks(void) {
 comGameId_t game;
 for(game=COM_GAME_QUAKE3;game<COM_GAME_COUNT;game=(comGameId_t)(game+1)) {
  const qboolean jedi=game==COM_GAME_JKA_SP||game==COM_GAME_JKA_MP||game==COM_GAME_JK2_SP||game==COM_GAME_JK2_MP;
  const jediSurfaceSprite_t *ss=Sprite();
  shaderStage_t taken[MAX_SHADER_STAGES];
  int numTaken=-1;
  if(!jedi) {
   /* every other profile still refuses the words, as its retail engine would */
   CHECK(!Parse(game,Ground("surfaceSprites vertical 32 24 42 500")) && strstr(hostWarning,"'surfaceSprites'"));
   CHECK(!Parse(game,Ground("ssWind 0.5")) && strstr(hostWarning,"'ssWind'"));
   continue;
  }
  /* a grass stage: every field as written, wind setting the idle sway */
  CHECK(Parse(game,Ground("surfaceSprites vertical 32 24 42 500\nssFademax 1500\nssFadescale 1\nssVariance 1 2\nssWind 0.5")) && !hostWarning[0]);
  CHECK(ss->type==JEDI_SPRITE_VERTICAL && ss->width==32.0f && ss->height==24.0f && ss->density==42.0f);
  CHECK(ss->fadeDist==500.0f && ss->fadeMax==1500.0f && ss->fadeScale==1.0f);
  CHECK(ss->variance[0]==1.0f && ss->variance[1]==2.0f && ss->wind==0.5f && ss->windIdle==0.5f);
  CHECK(ss->facing==JEDI_SPRITE_FACING_NORMAL && ss->fxDuration==1000.0f && ss->fxAlphaStart==1.0f && ss->fxAlphaEnd==0.0f);
  CHECK(ss->fxGrow[0]==0.0f && ss->fxGrow[1]==0.0f && ss->vertSkew==0.0f);
  CHECK(stages[2].active && (stages[2].stateBits & GLS_ATEST_BITS) && R_ALICE_GEN(&stages[2])->rgbGen==CGEN_VERTEX);
  CHECK(stages[0].jediSprite.type==JEDI_SPRITE_NONE && stages[1].jediSprite.type==JEDI_SPRITE_NONE);
  /* the fade maximum defaults to 1.33 times the fade distance in double precision */
  CHECK(Parse(game,Ground("surfaceSprites oriented 16 16 40 333")) && ss->type==JEDI_SPRITE_ORIENTED);
  CHECK(ss->fadeMax==(float)(333.0*1.33));
  /* surfaceSprites resets a facing given before it */
  CHECK(Parse(game,Ground("ssFaceup\nsurfaceSprites oriented 8 8 20 400")) && ss->facing==JEDI_SPRITE_FACING_NORMAL);
  /* the words are matched without regard to case */
  CHECK(Parse(game,Ground("SURFACESPRITES EFFECT 4 4 60 800\nSSFXDURATION 400\nssfxgrow 0.5 1\nSsFxAlphaRange 0.8 0.1\nssfxweather")) && !hostWarning[0]);
  CHECK(ss->type==JEDI_SPRITE_WEATHERFX && ss->fxDuration==400.0f && ss->fxGrow[0]==0.5f && ss->fxGrow[1]==1.0f);
  CHECK(Near(ss->fxAlphaStart,0.8f) && Near(ss->fxAlphaEnd,0.1f));
  /* the rest of the line belongs to the sprite word: depthWrite never applies */
  CHECK(Parse(game,Ground("surfaceSprites vertical 32 24 42 500 depthWrite")) && ss->type==JEDI_SPRITE_VERTICAL);
  CHECK(!(stages[2].stateBits & GLS_DEPTHMASK_TRUE));
  CHECK(Parse(game,Ground("surfaceSprites oriented 8 8 20 400\nssFaceup depthWrite")) && ss->facing==JEDI_SPRITE_FACING_UP);
  CHECK(!(stages[2].stateBits & GLS_DEPTHMASK_TRUE));
  /* bad lines warn and store nothing; the shader still loads */
  CHECK(Parse(game,Ground("surfaceSprites vertical 0 24 42 500")) && strstr(hostWarning,"invalid width") && ss->type==JEDI_SPRITE_NONE);
  CHECK(Parse(game,Ground("surfaceSprites vertical 32 -1 42 500")) && strstr(hostWarning,"invalid height") && ss->type==JEDI_SPRITE_NONE);
  CHECK(Parse(game,Ground("surfaceSprites vertical 32 24 0 500")) && strstr(hostWarning,"invalid density") && ss->type==JEDI_SPRITE_NONE);
  CHECK(Parse(game,Ground("surfaceSprites vertical 32 24 42 31.9")) && strstr(hostWarning,"invalid fadedist") && ss->type==JEDI_SPRITE_NONE);
  CHECK(Parse(game,Ground("surfaceSprites vertical 32 24 42 32")) && !hostWarning[0] && ss->type==JEDI_SPRITE_VERTICAL);
  CHECK(Parse(game,Ground("surfaceSprites vertical 32 24 42")) && strstr(hostWarning,"missing surfaceSprites params") && ss->type==JEDI_SPRITE_NONE);
  CHECK(Parse(game,Ground("surfaceSprites")) && strstr(hostWarning,"missing surfaceSprites params"));
  CHECK(Parse(game,Ground("surfaceSprites grass 32 24 42 500")) && strstr(hostWarning,"invalid type") && ss->type==JEDI_SPRITE_NONE);
  /* flattened is Academy single player's alone */
  CHECK(Parse(game,Ground("surfaceSprites flattened 12 12 30 600")));
  if(game==COM_GAME_JKA_SP) CHECK(!hostWarning[0] && ss->type==JEDI_SPRITE_FLATTENED);
  else CHECK(strstr(hostWarning,"invalid type") && ss->type==JEDI_SPRITE_NONE);
  /* ss words may come first on a zeroed record; surfaceSprites resets all
   * but the vertical skew */
  CHECK(Parse(game,Ground("ssWindidle 2\nssVertskew 0.25\nssHangdown\nsurfaceSprites vertical 8 40 30 700")) && !hostWarning[0]);
  CHECK(ss->type==JEDI_SPRITE_VERTICAL && ss->windIdle==0.0f && ss->facing==JEDI_SPRITE_FACING_NORMAL && ss->vertSkew==0.25f);
  /* a given idle sway outlasts a later wind */
  CHECK(Parse(game,Ground("surfaceSprites vertical 8 40 30 700\nssWindidle 2\nssWind 0.5")) && ss->wind==0.5f && ss->windIdle==2.0f);
  /* option checks: each warns and keeps what was stored before it */
  CHECK(Parse(game,Ground("surfaceSprites vertical 8 40 30 700\nssFademax 700")) && strstr(hostWarning,"invalid surfacesprite fademax"));
  CHECK(ss->fadeMax==(float)(700.0*1.33));
  CHECK(Parse(game,Ground("surfaceSprites vertical 8 40 30 700\nssVariance 1 -1")) && strstr(hostWarning,"variance height"));
  CHECK(ss->variance[0]==1.0f && ss->variance[1]==0.0f);
  CHECK(Parse(game,Ground("surfaceSprites effect 8 8 30 700\nssFXAlphaRange 0.5 2")) && strstr(hostWarning,"fxalpha end"));
  CHECK(ss->fxAlphaStart==0.5f && ss->fxAlphaEnd==0.0f);
  CHECK(Parse(game,Ground("surfaceSprites effect 8 8 30 700\nssFXAlphaRange 1.5 0")) && strstr(hostWarning,"fxalpha start") && ss->fxAlphaStart==1.0f);
  CHECK(Parse(game,Ground("surfaceSprites effect 8 8 30 700\nssFXDuration 0")) && strstr(hostWarning,"duration") && ss->fxDuration==1000.0f);
  CHECK(Parse(game,Ground("surfaceSprites vertical 8 8 30 700\nssWind -1")) && strstr(hostWarning,"surfacesprite wind") && ss->wind==0.0f);
  CHECK(Parse(game,Ground("surfaceSprites vertical 8 8 30 700\nssFXWeather")) && strstr(hostWarning,"weather applied") && ss->type==JEDI_SPRITE_VERTICAL);
  CHECK(Parse(game,Ground("surfaceSprites vertical 8 8 30 700\nssHangdown\nssFaceup")) && strstr(hostWarning,"Faceup facing overrides"));
  CHECK(ss->facing==JEDI_SPRITE_FACING_DOWN);
  CHECK(Parse(game,Ground("surfaceSprites vertical 8 8 30 700\nssFademax")) && strstr(hostWarning,"missing surfacesprite fademax"));
  /* any other word starting with ss warns, the shader still loads */
  CHECK(Parse(game,Ground("ssBogus 1 2 3")) && strstr(hostWarning,"invalid optional surfacesprite param 'ssBogus'"));
  CHECK(Parse(game,Ground("ss")) && strstr(hostWarning,"invalid optional surfacesprite param 'ss'"));

  /* FinishShader's split: the sprite stage leaves the surface's passes */
  CHECK(Parse(game,Ground("surfaceSprites vertical 32 24 42 500")));
  shader.needsNormal=qfalse;
  CHECK(R_JediTakeSpriteStages(3,taken,&numTaken)==2 && numTaken==1);
  CHECK(taken[0].jediSprite.type==JEDI_SPRITE_VERTICAL && taken[0].jediSprite.width==32.0f);
  CHECK(stages[0].active && stages[1].active && !stages[2].active && shader.needsNormal);
  /* a first-stage sprite never draws, as in retail */
  CHECK(Parse(game,"{\n{\nmap gfx/test/drop\nsurfaceSprites effect 4 4 50 900\n}\n{\nmap gfx/test/drop\nsurfaceSprites effect 4 4 50 900\n}\n}\n"));
  CHECK(R_JediTakeSpriteStages(2,taken,&numTaken)==0 && numTaken==1);
  /* a stage whose line failed stays an ordinary stage */
  CHECK(Parse(game,Ground("surfaceSprites vertical 0 24 42 500")));
  CHECK(R_JediTakeSpriteStages(3,taken,&numTaken)==3 && numTaken==0);
 }
 return failures?1:0;
}
'''

# The production generator over a small batch, with the renderer's types
# reduced to the fields it reads and writes.
GENERATOR = r'''
#include "code/qcommon/q_shared.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define SHADER_MAX_VERTEXES 1000
#define SHADER_MAX_INDEXES (6*SHADER_MAX_VERTEXES)
#define USE_PMLIGHT
#define USE_VBO
#define GLS_SRCBLEND_ONE 0x2
#define GLS_SRCBLEND_SRC_ALPHA 0x5
#define GLS_SRCBLEND_BITS 0xf
#define GLS_DSTBLEND_ONE 0x20
#define GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA 0x60
#define GLS_DSTBLEND_BITS 0xf0
#define VPF_DLIGHT_SHADOW 0x20
typedef uint32_t glIndex_t;
typedef enum { SF_BAD, SF_TRIANGLES } surfaceType_t;
#include "code/renderercommon/tr_jedi_sprite.h"
typedef struct { unsigned stateBits; } shaderStage_t;
typedef struct shader_s {
 qboolean defaultShader;
 shaderStage_t *stages[1];
 int numJediSprites;
 jediSpritePass_t *jediSprites;
} shader_t;
typedef struct { vec3_t origin; vec3_t axis[3]; vec3_t viewOrigin; } orientationr_t;
static struct {
 glIndex_t indexes[SHADER_MAX_INDEXES];
 vec4_t xyz[SHADER_MAX_VERTEXES];
 vec4_t normal[SHADER_MAX_VERTEXES];
 vec2_t texCoords[2][SHADER_MAX_VERTEXES];
 color4ub_t vertexColors[SHADER_MAX_VERTEXES];
 surfaceType_t surfType;
 int vboIndex, numIndexes, numVertexes, fogNum;
 qboolean allowVBO, dlightPass, csmCasterPass, csmShadowPass;
 shader_t *shader;
} tess;
static struct {
 struct { orientationr_t or; unsigned passFlags; } viewParms;
 orientationr_t or;
 struct { float fov_x; int time; } refdef;
 qboolean projection2D;
} backEnd;
static struct { cvar_t *(*Cvar_Get)( const char *, const char *, int ); } ri;
static cvar_t cvars[2];
static cvar_t *HostCvarGet( const char *name, const char *value, int flags ) {
 cvar_t *c = !strcmp( name, "r_surfaceSprites" ) ? &cvars[0] : &cvars[1];
 (void)flags;
 if ( !c->name ) { c->name = (char *)name; c->value = (float)atof( value ); c->integer = atoi( value ); }
 return c;
}
/* each batch drawn, per sprite shader */
static int begun, ended, quads[4], restarted;
static float minZ = 1e9f, maxZ = -1e9f, maxWidth;
static int alphaCount[256];
static shader_t *drawing;
static vec3_t firstQuad[4];
static int captured;
static void RB_BeginSurface( shader_t *shader, int fogNum ) {
 tess.shader = shader; tess.fogNum = fogNum; tess.numIndexes = tess.numVertexes = 0; tess.allowVBO = qfalse; ++begun;
}
static void RB_EndSurface( void ) {
 int i;
 ++ended;
 for ( i = 0; i < tess.numVertexes; i += 4 ) {
  int k;
  ++quads[ tess.shader == drawing ? 0 : 1 ];
  if ( !captured ) {
   for ( k = 0; k < 4; k++ ) VectorCopy( tess.xyz[i+k], firstQuad[k] );
   captured = 1;
  }
  for ( k = 0; k < 4; k++ ) {
   if ( tess.xyz[i+k][2] < minZ ) minZ = tess.xyz[i+k][2];
   if ( tess.xyz[i+k][2] > maxZ ) maxZ = tess.xyz[i+k][2];
  }
  ++alphaCount[ tess.vertexColors[i].rgba[3] ];
  {
   const float w = sqrtf( ( tess.xyz[i][0] - tess.xyz[i+3][0] ) * ( tess.xyz[i][0] - tess.xyz[i+3][0] ) +
    ( tess.xyz[i][1] - tess.xyz[i+3][1] ) * ( tess.xyz[i][1] - tess.xyz[i+3][1] ) );
   if ( w > maxWidth ) maxWidth = w;
  }
  assert( tess.texCoords[0][i][0] == 1.0f && tess.texCoords[0][i][1] == 1.0f );
  assert( tess.texCoords[0][i+2][0] == 0.0f && tess.texCoords[0][i+2][1] == 0.0f );
 }
 tess.numIndexes = tess.numVertexes = 0;
}
static void RB_CheckOverflow( int verts, int indexes ) {
 if ( tess.numVertexes + verts <= SHADER_MAX_VERTEXES && tess.numIndexes + indexes <= SHADER_MAX_INDEXES ) return;
 ++restarted;
 { shader_t *s = tess.shader; int f = tess.fogNum; RB_EndSurface(); RB_BeginSurface( s, f ); }
}
#define RB_CHECKOVERFLOW( v, i ) RB_CheckOverflow( v, i )
#include "code/renderercommon/tr_jedi_sprite_draw.h"

static shader_t parent, child;
static shaderStage_t childStage;
static jediSpritePass_t pass;

/* a 512-unit floor square at z 0, two triangles, normals up */
static void Floor( float z, float nz ) {
 static const float corners[4][2] = { { 0, 0 }, { 512, 0 }, { 512, 512 }, { 0, 512 } };
 int i;
 RB_BeginSurface( &parent, 3 );
 for ( i = 0; i < 4; i++ ) {
  VectorSet( tess.xyz[i], corners[i][0], corners[i][1], z );
  VectorSet( tess.normal[i], 0, 0, nz );
  tess.vertexColors[i].rgba[0] = 200; tess.vertexColors[i].rgba[1] = 100; tess.vertexColors[i].rgba[2] = 50; tess.vertexColors[i].rgba[3] = 255;
 }
 tess.indexes[0] = 0; tess.indexes[1] = 1; tess.indexes[2] = 2;
 tess.indexes[3] = 0; tess.indexes[4] = 2; tess.indexes[5] = 3;
 tess.numVertexes = 4; tess.numIndexes = 6;
}
static void Reset( void ) {
 begun = ended = restarted = 0; memset( quads, 0, sizeof( quads ) ); memset( alphaCount, 0, sizeof( alphaCount ) );
 minZ = 1e9f; maxZ = -1e9f; maxWidth = 0.0f; captured = 0;
}
static int Run( float viewDistance ) {
 Reset();
 VectorSet( backEnd.or.viewOrigin, 256, 256, viewDistance );
 RB_JediSurfaceSprites();
 return quads[0];
}
int main( void ) {
 int i, near, again, far, total, faded;
 backEnd.refdef.fov_x = 90.0f;
 backEnd.refdef.time = 5000;
 for ( i = 0; i < 3; i++ ) { VectorClear( backEnd.or.axis[i] ); backEnd.or.axis[i][i] = 1.0f; }
 VectorSet( backEnd.viewParms.or.axis[0], 1, 0, 0 );
 VectorSet( backEnd.viewParms.or.axis[1], 0, 1, 0 );
 VectorSet( backEnd.viewParms.or.axis[2], 0, 0, 1 );
 ri.Cvar_Get = HostCvarGet;
 childStage.stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
 child.stages[0] = &childStage;
 drawing = &child;
 pass.shader = &child;
 pass.parms.type = JEDI_SPRITE_VERTICAL;
 pass.parms.width = 32; pass.parms.height = 24; pass.parms.density = 42;
 pass.parms.fadeDist = 500; pass.parms.fadeMax = 1500; pass.parms.fxDuration = 1000;
 pass.parms.fxAlphaStart = 1.0f;
 parent.numJediSprites = 1; parent.jediSprites = &pass;

 /* in range: a grid of grass over both triangles, standing 24 up */
 Floor( 0, 1 );
 near = Run( 64 );
 /* triangles of 512x512/2 at a 42-unit grid hold about 75 each */
 assert( near > 100 && near < 200 );
 assert( fabsf( maxZ - 24.0f ) < 0.01f && minZ == 0.0f );
 assert( alphaCount[255] == near );
 /* the batch goes back to the surface's shader, as it was begun */
 assert( tess.shader == &parent && tess.fogNum == 3 && begun == 2 && ended == 1 );
 /* the same view places the same sprites: the random numbers are seeded */
 Floor( 0, 1 );
 again = Run( 64 );
 assert( again == near );
 /* past the fade maximum nothing draws; inside the band some fade */
 Floor( 0, 1 );
 far = Run( 1600 );
 assert( far == 0 );
 Floor( 0, 1 );
 total = Run( 1150 );
 faded = total - alphaCount[255];
 assert( total > 0 && total < near && faded > 0 );
 /* walls and ceilings take no upright grass; a hanging sprite needs a ceiling */
 Floor( 0, 0.4f );
 assert( Run( 64 ) == 0 );
 Floor( 0, -1 );
 assert( Run( 64 ) == 0 );
 pass.parms.facing = JEDI_SPRITE_FACING_DOWN;
 Floor( 0, -1 );
 assert( Run( 64 ) == near && fabsf( minZ + 24.0f ) < 0.01f );
 pass.parms.facing = JEDI_SPRITE_FACING_NORMAL;
 /* the variance only ever enlarges a sprite */
 pass.parms.variance[0] = 1.0f;
 Floor( 0, 1 );
 Run( 64 );
 assert( maxWidth > 32.0f * 1.2f && maxWidth <= 64.0f * 1.1f );
 pass.parms.variance[0] = 0.0f;
 /* idle sway moves a sprite's top with time, its foot stays */
 pass.parms.windIdle = 1.0f;
 Floor( 0, 1 );
 Run( 64 );
 {
  vec3_t foot, top;
  VectorCopy( firstQuad[0], foot );
  VectorCopy( firstQuad[1], top );
  backEnd.refdef.time = 5600;
  Floor( 0, 1 );
  Run( 64 );
  backEnd.refdef.time = 5000;
  assert( VectorCompare( foot, firstQuad[0] ) );
  assert( !VectorCompare( top, firstQuad[1] ) && top[2] == firstQuad[1][2] );
  /* the sway's radius: 0.075 of the height for windIdle 1 */
  assert( Distance( top, firstQuad[1] ) <= 2.0f * 24.0f * 0.075f + 0.01f );
 }
 pass.parms.windIdle = 0.0f;
 /* sprites off: nothing drawn, the batch untouched */
 cvars[0].integer = 0;
 Floor( 0, 1 );
 assert( Run( 64 ) == 0 && begun == 0 && tess.shader == &parent && tess.numIndexes == 6 );
 cvars[0].integer = 1;
 /* light passes, shadow views and the 2D pass draw none */
 Floor( 0, 1 ); tess.dlightPass = qtrue;
 assert( Run( 64 ) == 0 ); tess.dlightPass = qfalse;
 Floor( 0, 1 ); backEnd.viewParms.passFlags = VPF_DLIGHT_SHADOW;
 assert( Run( 64 ) == 0 ); backEnd.viewParms.passFlags = 0;
 /* weather effects wait for weather (r_surfaceWeather, 0 by default) */
 pass.parms.type = JEDI_SPRITE_WEATHERFX;
 Floor( 0, 1 );
 assert( Run( 64 ) == 0 );
 cvars[1].value = 1.0f;
 Floor( 0, 1 );
 assert( Run( 64 ) > 0 );
 cvars[1].value = 0.0f;
 /* an effect fades over its cycle: start 1, end 0 fades in and out */
 pass.parms.type = JEDI_SPRITE_EFFECT;
 Floor( 0, 1 );
 total = Run( 64 );
 assert( total > 0 && alphaCount[255] < total );
 /* an additive stage takes its fade as brightness, alpha full */
 childStage.stateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
 Floor( 0, 1 );
 total = Run( 64 );
 assert( total > 0 && alphaCount[255] == total );
 childStage.stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
 /* a dense stage over a huge floor overflows the batch and restarts it */
 pass.parms.type = JEDI_SPRITE_ORIENTED;
 pass.parms.density = 4; pass.parms.fadeMax = 5000; pass.parms.fadeDist = 4000;
 Floor( 0, 1 );
 total = Run( 64 );
 assert( total > SHADER_MAX_VERTEXES / 4 && restarted > 0 && tess.shader == &parent );
 printf( "ok near=%d band=%d\n", near, faded );
 return 0;
}
'''


class JediSurfaceSpriteTests(unittest.TestCase):
    def test_whole_parser_under_every_profile(self):
        from alice_shader_parse_tests import RENDERERS as HOST_RENDERERS, build_host
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='jedi-sprite-host-', dir=ROOT / '.tmp') as temp:
            for renderer in HOST_RENDERERS:
                with self.subTest(renderer=renderer):
                    exe = build_host(Path(temp), f'host-{renderer}', HOST_CHECKS, renderer=renderer)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_generator_over_a_batch(self):
        compiler = shutil.which('clang')
        if not compiler:
            self.skipTest('Clang unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='jedi-sprite-draw-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            source = folder / 'generator.c'
            exe = folder / 'generator.exe'
            source.write_text(GENERATOR, encoding='utf-8')
            command = [compiler, *target, '-std=c11', '-O1', '-I', str(ROOT), '-D_CRT_SECURE_NO_WARNINGS',
                       '-DWIN32', '-D_WINDOWS', '-DNOMINMAX',
                       str(source), str(ROOT / 'code/qcommon/q_shared.c'), str(ROOT / 'code/qcommon/q_math.c'),
                       '-o', str(exe)]
            if os.name == 'nt':
                command += ['-fuse-ld=lld', '-Wl,/force:unresolved']
            else:
                command += ['-lm', '-Wl,--unresolved-symbols=ignore-all']
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('ok near=', run.stdout)


if __name__ == '__main__':
    unittest.main()
