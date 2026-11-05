"""Animated shader surfaces in the path tracer.

A stage's animMap frames cycle in the trace as raster cycles them: all of
them (up to the 64 a shader may name, not the eight a GPU stage holds), with
Medal of Honor's animMapPhase head start, Alice's animMapOnce and the Jedi
games' oneShotAnimMap holding the last frame, Alice's frameFromEntity and a
Jedi Academy RF_SETANIMINDEX entity choosing it (tr_shade.c
RB_BindAnimatedImage). The CPU packs the frames past a stage's eight, and
the phase, into records after the material's stages; the GLSL reads them.

The light a glowing shader gives off follows its stages over time: q3dm1's
hell flames cross-fade eight frames under a glow that swells and fades, and
the lights built from such a shader take, each frame, the fold of its stages
at the frame's shader time over that fold's mean over time (rt_lights.c
RT_AnimateLights, rt_backend.c RT_UploadLights). A q3map surface light
follows its surface as drawn lit, a glowing shader its glow.

The functions under test are compiled from the renderer's sources into
C++ harnesses; the enums come from code/rendererrtx/tr_local.h.
"""
from pathlib import Path
import re
import unittest

from rtx_surface_owner_tests import compile_run

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def function(source, signature):
    # the definition, not a declaration: no semicolon before its body
    start = source.index(signature)
    while ';' in source[start:source.index('\n{', start)]:
        start = source.index(signature, start + 1)
    body = source.index('\n{', start)
    return source[start:source.index('\n}', body) + 2] + '\n'


def enum(local, name):
    match = re.search(r'typedef enum \{[^{}]*?\} ' + name + ';', local, re.S)
    assert match is not None, name
    return match.group(0) + '\n'


def struct(local, name):
    match = re.search(r'typedef struct \{[^{}]*?\} ' + name + ';', local, re.S)
    assert match is not None, name
    return match.group(0) + '\n'


def glsl_constants(common, names):
    values = {}
    for name in names:
        match = re.search(r'const uint ' + name + r' = ([^;]+);', common)
        assert match is not None, name
        values[name] = match.group(1).strip()
    return values


def c_defines(gpu, names):
    values = {}
    for name in names:
        match = re.search(r'#define ' + name + r'\s+(.+)', gpu)
        assert match is not None, name
        values[name] = match.group(1).strip()
    return values


def evaluate(expression):
    expression = expression.replace('u', '').replace('(', ' ( ').replace(')', ' ) ')
    return eval(expression, {'__builtins__': {}})  # literal shifts and hex only


LOCAL = read('code/rendererrtx/tr_local.h')
MATERIAL = read('code/rendererrtx/rt/rt_material.c')
LIGHTS = read('code/rendererrtx/rt/rt_lights.c')
BACKEND = read('code/rendererrtx/rt/rt_backend.c')
GLSL = read('code/rendererrtx/shaders/rt_material.glsl')
GEOMETRY = read('code/rendererrtx/shaders/rt_geometry.glsl')
COMMON = read('code/rendererrtx/shaders/rt_common.glsl')
GPU = read('code/rendererrtx/rt/rt_gpu.h')
SHADE = read('code/rendererrtx/tr_shade.c')

ENGINE_TYPES = (
    '\n'.join(re.findall(r'^#define\s+GLS_\w+\s+[^\n]+', LOCAL, re.M)) + '\n' +
    enum(LOCAL, 'genFunc_t') + enum(LOCAL, 'alphaGen_t') + enum(LOCAL, 'colorGen_t') +
    enum(LOCAL, 'texCoordGen_t') + struct(LOCAL, 'waveForm_t')
)

PRELUDE = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <algorithm>
#include <vector>
typedef int qboolean;
enum { qfalse, qtrue };
typedef float vec3_t[3];
typedef union { uint8_t rgba[4]; uint32_t u32; } color4ub_t;
#define MAX( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )
#define MIN( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )
#define Com_Memset memset
#define Com_Memcpy memcpy
#define VectorScale( v, s, o ) ( ( o )[0] = ( v )[0] * ( s ), ( o )[1] = ( v )[1] * ( s ), ( o )[2] = ( v )[2] * ( s ) )
#define VectorCopy( a, b ) ( ( b )[0] = ( a )[0], ( b )[1] = ( a )[1], ( b )[2] = ( a )[2] )
#define VectorCompare( a, b ) ( ( a )[0] == ( b )[0] && ( a )[1] == ( b )[1] && ( a )[2] == ( b )[2] )
static float Com_Clamp( float lo, float hi, float v ) { return v < lo ? lo : ( v > hi ? hi : v ); }
static void *RT_HostAlloc( int bytes ) { return calloc( 1, (size_t)bytes ); }
static void RT_HostFree( void *p ) { free( p ); }
#define FUNCTABLE_SIZE 1024
#define FUNCTABLE_MASK ( FUNCTABLE_SIZE - 1 )
#define LIGHTMAP_BY_VERTEX -3
#define LIGHTMAP_INDEX_NONE 0
#define IMGFLAG_LIGHTMAP 0x80
#define PRINT_DEVELOPER 1
#define RT_STAGE_MAX_ANIMATIONS 64
#define RT_STAGE_MAX_TEXMODS 5
#include "code/rendererrtx/rt/rt_gpu.h"
'''

# The fixture's stage source: the fields the functions under test read.
STAGE = r'''
typedef struct { int flags; const char *imgName; float average[4]; } image_t;
typedef struct rtStageSource_s {
    image_t *images[RT_STAGE_MAX_ANIMATIONS];
    int numImages;
    float animSpeed, animPhase;
    qboolean animHoldLast, animFrameFromEntity;
    unsigned stateBits;
    texCoordGen_t tcGen;
    vec3_t tcGenVectors[2];
    colorGen_t rgbGen;
    waveForm_t rgbWave;
    alphaGen_t alphaGen;
    waveForm_t alphaWave;
    color4ub_t constantColor;
    int lightmap;
    qboolean vertexLightmap, isDetail, isVideoMap, isScreenMap;
} rtStageSource_t;
typedef struct {
    const char *name;
    int rtNumStages;
    rtStageSource_t *rtStages;
    double timeOffset, clampTime;
    int lightmapIndex;
} shader_t;
'''


class RtxAnimatedSurfaceTests(unittest.TestCase):
    def test_the_frame_rules_match_raster(self):
        # the rules the trace mirrors, as raster's RB_BindAnimatedImage has them
        bind = SHADE[SHADE.index('v = ( tess.shaderTime + bundle->mohaaAnimPhase ) * bundle->imageAnimationSpeed;'):]
        bind = bind[:bind.index('GL_Bind( bundle->image[ index ] );')]
        for line in ('index = (short)backEnd.currentEntity->e.alice.frame;',
                     'index = backEnd.currentEntity->e.skinNum;',
                     'index = 0;\t// may happen with shader time offsets',
                     '( bundle->aliceAnimMapOnce || bundle->jkaOneShotAnimMap ) &&',
                     'index = bundle->numImageAnimations - 1;',
                     'index %= bundle->numImageAnimations;'):
            self.assertIn(line, bind)
        snapshot = function(read('code/rendererrtx/tr_shader.c'), 'static void R_RtSnapshotStages( int numStages )')
        for line in ('source->numImages = MAX( 1, MIN( bundle->numImageAnimations, RT_STAGE_MAX_ANIMATIONS ) );',
                     'source->animPhase = bundle->mohaaAnimPhase;',
                     'source->animHoldLast = ( bundle->aliceAnimMapOnce || bundle->jkaOneShotAnimMap ) ? qtrue : qfalse;',
                     'source->animFrameFromEntity = bundle->aliceFrameFromEntity ? qtrue : qfalse;'):
            self.assertIn(line, snapshot)
        self.assertIn('#define RT_STAGE_MAX_ANIMATIONS MAX_IMAGE_ANIMATIONS',
                      read('code/rendererrtx/rt/rt_stage_source.h'))
        self.assertIn('#define MAX_IMAGE_ANIMATIONS\t\t64', LOCAL)

    def test_glsl_and_c_agree_on_the_frame_bits(self):
        names = ('RT_SF_HOLD_LAST', 'RT_SF_FRAME_FROM_ENTITY', 'RT_SF_FRAMES_SHIFT', 'RT_SF_FRAMES_MASK',
                 'RT_STAGE_INLINE_FRAMES', 'RT_ENTITY_FRAME_MASK', 'RT_ENTITY_FRAME_ALL')
        glsl = glsl_constants(COMMON, names)
        c = c_defines(GPU, names)
        for name in names:
            self.assertEqual(evaluate(glsl[name]), evaluate(c[name]), name)
        # the frame flags stay clear of the other stage flags and the count
        flags = [evaluate(c_defines(GPU, (n,))[n]) for n in ('RT_SF_LINEAR_SAMPLE', 'RT_SF_VERTEX_IS_LIGHT',
                                                             'RT_SF_ALPHA_TEST', 'RT_SF_DETAIL')]
        frames = evaluate(c['RT_SF_FRAMES_MASK']) << evaluate(c['RT_SF_FRAMES_SHIFT'])
        hold, entity = evaluate(c['RT_SF_HOLD_LAST']), evaluate(c['RT_SF_FRAME_FROM_ENTITY'])
        for bit in flags + [hold, entity]:
            self.assertEqual(bit & frames, 0)
            self.assertEqual(bit & 0xFFFF, 0)
        self.assertEqual(len({*flags, hold, entity}), 6)
        self.assertEqual(frames >> evaluate(c['RT_SF_FRAMES_SHIFT']), 63)
        self.assertLess(frames, 1 << 32)

    def test_cpu_packing_and_glsl_choose_raster_frames(self):
        packing = ''.join(function(MATERIAL, signature) for signature in (
            'static uint32_t RT_WaveFunc( genFunc_t func )',
            'static void RT_PackWave( float out[4], const waveForm_t *wave )',
            'static qboolean RT_GrowArray(',
            'static void RT_PackFrames(',
            'static uint32_t RT_StageFrameRecords(',
            'static void RT_TranslateStage(',
            'static qboolean RT_AppendFrameRecords(',
            'static int RT_StageFrameAt(',
        ))
        shader = ''.join(function(GLSL, signature) for signature in (
            'uint rt_stage_record(RtStage s)',
            'uint rt_stage_image(RtStage s, uint frame)',
            'uint rt_stage_frame(RtStage s, RtHitAttribs h, float time)',
        ))
        source = PRELUDE + ENGINE_TYPES + STAGE + r'''
static image_t images[RT_STAGE_MAX_ANIMATIONS];
static int32_t RT_ImageIndex( const image_t *image ) { return image ? (int32_t)( image - images ) : -1; }
static uint32_t RT_PackTexMods( const rtStageSource_t *, rtGpuStage_t * ) { return 0; }
static struct { rtGpuStage_t *stages; uint32_t numStages, maxStages; } s_rtMaterials;
''' + packing + r'''
// rt_material.glsl, compiled as C++
using uint = uint32_t;
struct uvec4 { uint x, y, z, w; uint operator[]( uint i ) const { return ( &x )[i]; } };
struct vec4 { float x, y, z, w; };
struct RtStage { uvec4 images, state; vec4 animTc0, tc1, rgbWave, alphaWave, texMod[RT_GPU_MAX_TEXMODS * 2]; };
static_assert( sizeof( RtStage ) == sizeof( rtGpuStage_t ), "the GLSL stage mirrors the C one" );
struct RtHitAttribs { uint entityFrame; bool entityFrameAll; };
template<class T> struct Buffer : std::vector<T> { int length() const { return (int)this->size(); } };
static struct { Buffer<RtStage> stages; } u_stages;
static float max( float a, double b ) { return a > b ? a : (float)b; }
static float clamp( float v, double lo, double hi ) { return v < lo ? (float)lo : v > hi ? (float)hi : v; }
using std::floor;
''' + shader + r'''
// raster's rule (tr_shade.c RB_BindAnimatedImage) for a stage drawn on an
// entity (or the world: frame 0, no RF_SETANIMINDEX)
static int RasterFrame( const rtStageSource_t *s, double time, int entityFrame, bool setAnimIndex )
{
    const int n = s->numImages;
    if ( n <= 1 ) return 0;
    double v = ( time + s->animPhase ) * s->animSpeed;
    int64_t index = (int64_t)v;
    if ( s->animFrameFromEntity ) index = (short)entityFrame;
    if ( setAnimIndex ) index = entityFrame;
    if ( index < 0 ) index = 0;
    if ( s->animHoldLast && index >= n ) index = n - 1;
    return (int)( index % n );
}
int main()
{
    static const int counts[] = { 1, 2, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64 };
    static const float speeds[] = { 10.0f, 2.5f, 0.7f };
    static const float phases[] = { 0.0f, 0.37f, -0.2f };
    int checked = 0, boundaries = 0;
    for ( int i = 0; i < RT_STAGE_MAX_ANIMATIONS; i++ ) images[i].flags = 0;
    for ( int count : counts ) for ( float speed : speeds ) for ( float phase : phases )
    for ( int hold = 0; hold < 2; hold++ ) for ( int fromEntity = 0; fromEntity < 2; fromEntity++ ) {
        rtStageSource_t s;
        shader_t shader;
        memset( &s, 0, sizeof( s ) );
        memset( &shader, 0, sizeof( shader ) );
        s.numImages = count;
        // frames name images out of order, so a wrong frame reads a wrong image
        for ( int f = 0; f < count; f++ ) s.images[f] = &images[( f * 37 + 11 ) % RT_STAGE_MAX_ANIMATIONS];
        s.animSpeed = speed;
        s.animPhase = phase;
        s.animHoldLast = hold;
        s.animFrameFromEntity = fromEntity;
        s.rgbGen = CGEN_IDENTITY;
        s.alphaGen = AGEN_IDENTITY;
        shader.rtNumStages = 1;
        shader.rtStages = &s;
        // the material's stages: an unanimated stage before it, as a material
        // holds several
        free( s_rtMaterials.stages );
        memset( &s_rtMaterials, 0, sizeof( s_rtMaterials ) );
        assert( RT_GrowArray( (void **)&s_rtMaterials.stages, &s_rtMaterials.maxStages, 2, sizeof( rtGpuStage_t ), 4u ) );
        memset( &s_rtMaterials.stages[0], 0, sizeof( rtGpuStage_t ) );
        s_rtMaterials.numStages = 1;
        const uint32_t first = s_rtMaterials.numStages;
        RT_TranslateStage( &shader, &s, RT_ROLE_BASE, 0, &s_rtMaterials.stages[ s_rtMaterials.numStages++ ] );
        assert( RT_AppendFrameRecords( &shader, first, 1 ) );
        // one record per eight frames past the stage's own, and one for a
        // phase alone; none for a stage that does not animate
        const uint32_t records = s_rtMaterials.numStages - 2;
        assert( records == ( count > 8 ? (uint32_t)( count - 1 ) / 8u : ( count > 1 && phase != 0.0f ) ? 1u : 0u ) );
        assert( ( s_rtMaterials.stages[first].tc1[3] != 0.0f ) == ( records != 0 ) );
        u_stages.stages.assign( s_rtMaterials.numStages, RtStage() );
        memcpy( u_stages.stages.data(), s_rtMaterials.stages, s_rtMaterials.numStages * sizeof( rtGpuStage_t ) );
        const RtStage &stage = u_stages.stages[first];
        for ( int k = -40; k < 2400; k++ ) {
            const double time = k * 0.0137;
            for ( int e = 0; e < 3; e++ ) {
                // the world (frame 0), an Alice entity's frame, a Jedi Academy
                // RF_SETANIMINDEX entity's skinNum
                const int entityFrame = e == 0 ? 0 : ( k * 7 ) % 90;
                const bool setAnimIndex = e == 2;
                RtHitAttribs h = { (uint)entityFrame, setAnimIndex };
                const uint frame = rt_stage_frame( stage, h, (float)time );
                const int raster = RasterFrame( &s, time, entityFrame, setAnimIndex );
                int expected = raster;
                // single precision may round a time a hair from a frame's
                // start into the frame either side of it
                const double v = ( time + s.animPhase ) * s.animSpeed;
                if ( (int)frame != expected && fabs( v - floor( v + 0.5 ) ) < 1e-4 * fmax( 1.0, fabs( v ) ) &&
                    ( (int)frame == RasterFrame( &s, time + 1e-3 / speed, entityFrame, setAnimIndex ) ||
                      (int)frame == RasterFrame( &s, time - 1e-3 / speed, entityFrame, setAnimIndex ) ) ) {
                    expected = (int)frame;
                    boundaries++;
                }
                if ( (int)frame != expected || rt_stage_image( stage, frame ) != (uint)RT_ImageIndex( s.images[expected] ) ) {
                    printf( "count %d speed %g phase %g hold %d entity %d time %g e %d: frame %u, raster %d\n",
                        count, speed, phase, hold, fromEntity, time, e, frame, expected );
                    return 1;
                }
                if ( e == 0 ) {
                    // the light builder's frame for a world surface, in
                    // double precision as raster takes it
                    assert( RT_StageFrameAt( &s, time ) == raster );
                }
                checked++;
            }
        }
    }
    // the boundary allowance stays rare
    printf( "%d frames, %d on a frame's boundary\n", checked, boundaries );
    assert( boundaries * 1000 < checked );
    return 0;
}
'''
        compile_run(self, source)

    def test_an_entity_frame_crosses_the_vertex_lane(self):
        lane = function(BACKEND, 'static float RT_EntityFrameLane( const trRefEntity_t *ent )')
        decode = GEOMETRY[GEOMETRY.index('float frameLane = tri.v0.normal.w;'):]
        decode = decode[:decode.index('h.entityFrameAll = (frameWord & RT_ENTITY_FRAME_ALL) != 0u;') +
                        len('h.entityFrameAll = (frameWord & RT_ENTITY_FRAME_ALL) != 0u;')]
        shadow = function(GEOMETRY, 'bool rt_casts_shadow(RtTriangle tri)')
        self.assertIn('return !tri.dynamic || tri.v0.normal.w >= 0.0;', shadow)
        stamp = function(BACKEND, 'static void RT_StampEntityVertices(')
        self.assertIn('v->normal[3] = frameLane;', stamp)
        self.assertNotIn('RF_NOSHADOW ) ? -1.0f : 0.0f', stamp)
        source = PRELUDE + r'''
typedef enum { COM_GAME_QUAKE3, COM_GAME_JKA_SP, COM_GAME_JKA_MP, COM_GAME_ALICE } comGameId_t;
static struct { comGameId_t gameId; } ri;
#define RF_NOSHADOW 0x40
typedef struct { struct { int renderfx, skinNum; struct { int frame; } alice; struct { qboolean setAnimIndex; } jka; } e; } trRefEntity_t;
''' + lane + r'''
using uint = uint32_t;
struct Hit { uint entityFrame; bool entityFrameAll; };
static Hit Decode( float lane, bool &castsShadow )
{
    struct { struct { struct { float w; } normal; } v0; bool dynamic; } tri = { { { lane } }, true };
    Hit h;
    auto clamp = []( float v, double lo, double hi ) { return v < lo ? (float)lo : v > hi ? (float)hi : v; };
''' + decode.replace('uint(', '(uint)(') + r'''
    castsShadow = !tri.dynamic || tri.v0.normal.w >= 0.0;
    return h;
}
int main()
{
    trRefEntity_t ent;
    for ( int game = 0; game < 4; game++ ) for ( int noShadow = 0; noShadow < 2; noShadow++ )
    for ( int set = 0; set < 2; set++ ) for ( int frame : { -5, 0, 1, 7, 31, 63, 300, 32767, 40000, 2097151 } ) {
        memset( &ent, 0, sizeof( ent ) );
        ri.gameId = (comGameId_t)game;
        ent.e.renderfx = noShadow ? RF_NOSHADOW : 0;
        ent.e.alice.frame = frame;
        ent.e.skinNum = frame;
        ent.e.jka.setAnimIndex = set;
        bool shadow;
        const Hit h = Decode( RT_EntityFrameLane( &ent ), shadow );
        assert( shadow == !noShadow );
        const bool jka = set && ( game == COM_GAME_JKA_SP || game == COM_GAME_JKA_MP );
        assert( h.entityFrameAll == jka );
        // Alice's frame is a short; a negative frame shows the first
        int expected = jka ? frame : (short)frame;
        if ( expected < 0 ) expected = 0;
        if ( expected > (int)RT_ENTITY_FRAME_MASK ) expected = (int)RT_ENTITY_FRAME_MASK;
        assert( (int)h.entityFrame == expected );
    }
    puts( "lanes ok" );
    return 0;
}
'''
        compile_run(self, source)

    def test_an_animated_emitter_follows_its_stages(self):
        folds = ''.join(function(MATERIAL, signature) for signature in (
            'static qboolean RT_GrowArray(',
            'static qboolean RT_StageIsLightmap( const rtStageSource_t *s )',
            'static qboolean RT_StageVertexIsLight( const shader_t *shader, const rtStageSource_t *s )',
            'static qboolean RT_StageAdds( const rtStageSource_t *s )',
            'static void RT_ResetImageAverages( void )',
            'static void RT_ImageAverage( const image_t *image, float rgba[4] )',
            'static float RT_WaveMean( const waveForm_t *wave )',
            'static float RT_WaveAt( const waveForm_t *wave, double time )',
            'static float RT_ColorWave( const waveForm_t *wave, float value )',
            'static int RT_StageFrameAt(',
            'static qboolean RT_StageAverage(',
            'static void RT_BlendFactor(',
            'static void RT_BlendAverage(',
            'static void RT_FoldStages(',
        ))
        averages = MATERIAL[MATERIAL.index('#define RT_IMAGE_AVERAGES'):MATERIAL.index('// The averages belong to one set of images')]
        section = LIGHTS[LIGHTS.index('#define RT_LIGHT_ANIMATION_SAMPLES'):LIGHTS.index('/*\n==================\nSurface lights')]
        source = PRELUDE + ENGINE_TYPES + STAGE + r'''
static struct { float identityLight; image_t *whiteImage, *identityLightImage;
    float sinTable[FUNCTABLE_SIZE], squareTable[FUNCTABLE_SIZE], triangleTable[FUNCTABLE_SIZE],
    sawToothTable[FUNCTABLE_SIZE], inverseSawToothTable[FUNCTABLE_SIZE]; } tr;
static struct { struct { int time; } refdef; } backEnd;
static float R_NoiseGet4f( float, float, float, double t ) { return (float)sin( t * 1.7 ); }
static float R_NoiseRandom( int index ) { return (float)( ( index * 2654435761u ) % 1000u ) / 500.0f; }
static int fileReads;
// an image's average, as the image loader sums it from its file
static qboolean R_ImageSums( const char *name, const float *, float ( * )( float, float, float ), float rgba[4], float * )
{
    extern image_t images[];
    fileReads++;
    for ( int i = 0; i < 64; i++ ) if ( images[i].imgName == name ) { memcpy( rgba, images[i].average, 16 ); return qtrue; }
    return qfalse;
}
static void NoPrint( int, const char *, ... ) {}
static struct { void ( *Printf )( int, const char *, ... ); } ri = { NoPrint };
static struct {
    uint32_t numLights;
    struct rtLightAnimation_s *animations;
    uint32_t numAnimations, maxAnimations;
    struct rtAnimatedLight_s *animated;
    uint32_t numAnimated, maxAnimated;
} s_rtLights;
''' + averages + folds + section + r'''
image_t images[64];
static char names[64][16];
static rtStageSource_t stages[4];
static shader_t shader;
static void Image( int i, float r, float g, float b )
{
    snprintf( names[i], sizeof( names[i] ), "img%d", i );
    images[i].imgName = names[i];
    images[i].average[0] = r; images[i].average[1] = g; images[i].average[2] = b; images[i].average[3] = 1.0f;
}
static void Stage( int i, unsigned blend, colorGen_t rgbGen, genFunc_t func, float base, float amplitude, float frequency )
{
    memset( &stages[i], 0, sizeof( stages[i] ) );
    stages[i].stateBits = blend | GLS_DEPTHMASK_TRUE;
    stages[i].tcGen = TCGEN_TEXTURE;
    stages[i].rgbGen = rgbGen;
    stages[i].alphaGen = AGEN_IDENTITY;
    stages[i].rgbWave.func = func;
    stages[i].rgbWave.base = base;
    stages[i].rgbWave.amplitude = amplitude;
    stages[i].rgbWave.frequency = frequency;
    stages[i].lightmap = LIGHTMAP_INDEX_NONE;
    stages[i].numImages = 1;
}
static float Luminance( const float c[3] ) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; }
// raster's EvalWaveForm for a table wave
static float Wave( const waveForm_t *w, double t, const float *table )
{
    return w->base + table[ (int64_t)( ( w->phase + t * w->frequency ) * FUNCTABLE_SIZE ) & FUNCTABLE_MASK ] * w->amplitude;
}
int main()
{
    for ( int i = 0; i < FUNCTABLE_SIZE; i++ ) {
        tr.sinTable[i] = (float)sin( i * 2.0 * 3.14159265358979 / ( FUNCTABLE_SIZE - 1 ) );
        tr.squareTable[i] = i < FUNCTABLE_SIZE / 2 ? 1.0f : -1.0f;
        tr.sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
        tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];
        tr.triangleTable[i] = i < FUNCTABLE_SIZE / 2 ? 2.0f * i / FUNCTABLE_SIZE : 2.0f - 2.0f * i / FUNCTABLE_SIZE;
    }
    tr.identityLight = 0.5f;
    shader.name = "textures/sfx/flame1_hell";
    shader.rtStages = stages;
    // q3dm1's hell flame: eight frames cross-faded ten times a second (two
    // animMaps a frame apart under inverse and plain sawtooth), and a glow
    // swelling and fading at 0.6 cycles a second
    static const float frameLuminance[8] = { 0.068f, 0.059f, 0.052f, 0.048f, 0.055f, 0.054f, 0.058f, 0.059f };
    for ( int f = 0; f < 8; f++ ) Image( f, frameLuminance[f] * 1.6f, frameLuminance[f] * 0.9f, frameLuminance[f] * 0.3f );
    Image( 8, 0.05f, 0.025f, 0.01f );
    Stage( 0, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, CGEN_WAVEFORM, GF_INVERSE_SAWTOOTH, 0, 1, 10 );
    Stage( 1, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, CGEN_WAVEFORM, GF_SAWTOOTH, 0, 1, 10 );
    Stage( 2, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, CGEN_WAVEFORM, GF_SIN, 0.6f, 0.2f, 0.6f );
    for ( int s = 0; s < 2; s++ ) {
        stages[s].numImages = 8;
        stages[s].animSpeed = 10;
        for ( int f = 0; f < 8; f++ ) stages[s].images[f] = &images[( f + s ) % 8];
    }
    stages[2].images[0] = &images[8];
    shader.rtNumStages = 3;
    s_rtLights.numLights = 2;	// its two triangle lights
    RT_AnimateNewLights( &shader, 1.0f, 0 );
    assert( s_rtLights.numAnimated == 2 && s_rtLights.numAnimations == 1 );
    const rtLightAnimation_t *animation = &s_rtLights.animations[0];
    assert( animation->mean > 0.0f && animation->light == 1.0f );
    for ( int i = 0; i < 2; i++ ) assert( s_rtLights.animated[i].light == (uint32_t)i && s_rtLights.animated[i].animation == 0 );
    // each frame's scale is what the stages show then over their mean, as
    // raster draws them; it averages one over time
    double sum = 0.0;
    float lo = 1e9f, hi = 0.0f;
    const int reads = fileReads;
    for ( int k = 0; k < 4000; k++ ) {
        const double now = 1000.0 + k * 0.0161;
        RT_AnimateLights( now );
        const int n = (int)( now * 10.0 );
        const float fade = Com_Clamp( 0, 1, Wave( &stages[0].rgbWave, now, tr.inverseSawToothTable ) * tr.identityLight );
        const float rise = Com_Clamp( 0, 1, Wave( &stages[1].rgbWave, now, tr.sawToothTable ) * tr.identityLight );
        const float glow = Com_Clamp( 0, 1, Wave( &stages[2].rgbWave, now, tr.sinTable ) * tr.identityLight );
        float shown[3];
        for ( int c = 0; c < 3; c++ ) {
            shown[c] = images[n % 8].average[c] * fade + images[( n + 1 ) % 8].average[c] * rise + images[8].average[c] * glow;
        }
        const float expected = Luminance( shown ) / animation->mean;
        assert( fabsf( animation->scale - expected ) <= 1e-4f * expected );
        sum += animation->scale;
        lo = fminf( lo, animation->scale );
        hi = fmaxf( hi, animation->scale );
    }
    // the averages were kept: no frame reads a file
    assert( fileReads == reads );
    assert( fabs( sum / 4000.0 - 1.0 ) < 0.01 );
    assert( lo < 0.9f && hi > 1.1f && hi < RT_LIGHT_ANIMATION_MAX );
    // the shader's time: the view's clock less its timeOffset, held at its clampTime
    shader.timeOffset = 2.0;
    assert( RT_ShaderTimeAt( &shader, 5.0 ) == 3.0 );
    shader.clampTime = 1.5;
    assert( RT_ShaderTimeAt( &shader, 5.0 ) == 1.5 );
    shader.timeOffset = shader.clampTime = 0.0;

    // the same frames cross-faded alike hold their light still: no animation
    {
        static shader_t steady;
        static rtStageSource_t same[2];
        steady = shader;
        steady.name = "steady";
        same[0] = stages[0];
        same[1] = stages[1];
        for ( int s = 0; s < 2; s++ ) for ( int f = 0; f < 8; f++ ) same[s].images[f] = &images[0];
        steady.rtStages = same;
        steady.rtNumStages = 2;
        s_rtLights.numLights = 4;
        RT_AnimateNewLights( &steady, 1.0f, 2 );
        assert( s_rtLights.numAnimated == 2 && s_rtLights.numAnimations == 2 && s_rtLights.animations[1].mean == 0.0f );
    }
    // a lit panel under fading letters: its surface light follows it lit,
    // where the letters are a small part; its glow alone swings widely
    {
        static shader_t panel;
        static rtStageSource_t layers[3];
        panel.name = "textures/sfx/computer_blocks17";
        panel.rtStages = layers;
        panel.rtNumStages = 3;
        Image( 9, 0.6f, 0.6f, 0.6f );
        Image( 10, 0.0f, 0.0f, 0.0f );
        Image( 11, 0.3f, 0.5f, 0.3f );
        memcpy( &layers[0], &stages[0], sizeof( layers[0] ) );
        Stage( 0, 0, CGEN_IDENTITY, GF_NONE, 0, 0, 0 );
        layers[0] = stages[0];
        layers[0].images[0] = &images[9];
        Stage( 1, GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO, CGEN_IDENTITY, GF_NONE, 0, 0, 0 );
        layers[1] = stages[1];
        layers[1].tcGen = TCGEN_LIGHTMAP;
        layers[1].images[0] = &images[10];
        Stage( 2, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, CGEN_WAVEFORM, GF_INVERSE_SAWTOOTH, 0, 1, 2 );
        layers[2] = stages[2];
        layers[2].numImages = 2;
        layers[2].animSpeed = 2;
        layers[2].images[0] = &images[11];
        layers[2].images[1] = &images[10];
        const int lit = RT_LightAnimationFor( &panel, 1.0f );
        const int dark = RT_LightAnimationFor( &panel, 0.0f );
        assert( lit >= 0 && dark >= 0 && lit != dark );
        float litLo = 1e9f, litHi = 0.0f, darkLo = 1e9f, darkHi = 0.0f;
        for ( int k = 0; k < 400; k++ ) {
            RT_AnimateLights( k * 0.0377 );
            litLo = fminf( litLo, s_rtLights.animations[lit].scale ); litHi = fmaxf( litHi, s_rtLights.animations[lit].scale );
            darkLo = fminf( darkLo, s_rtLights.animations[dark].scale ); darkHi = fmaxf( darkHi, s_rtLights.animations[dark].scale );
        }
        assert( darkLo < 0.05f && darkHi > 2.0f );
        assert( litLo > 0.7f && litHi < 1.4f );
    }
    // a shader that does not change, and one whose frames' averages could
    // only be read again from their files each frame, stay steady lights
    {
        static shader_t still;
        static rtStageSource_t plain;
        still.name = "still";
        Stage( 0, 0, CGEN_IDENTITY, GF_NONE, 0, 0, 0 );
        plain = stages[0];
        plain.images[0] = &images[9];
        still.rtStages = &plain;
        still.rtNumStages = 1;
        assert( RT_LightAnimationFor( &still, 1.0f ) < 0 );
        static shader_t unread;
        static rtStageSource_t pulse;
        unread = still;
        unread.name = "unread";
        Stage( 0, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, CGEN_WAVEFORM, GF_SIN, 0.5f, 0.5f, 1 );
        pulse = stages[0];
        Image( 12, 0.5f, 0.5f, 0.5f );
        pulse.images[0] = &images[12];
        unread.rtStages = &pulse;
        const int saved = s_rtImageAverageCount;
        s_rtImageAverageCount = RT_IMAGE_AVERAGES;	// the averages are full
        for ( int i = saved; i < RT_IMAGE_AVERAGES; i++ ) s_rtImageAverages[i].image = &images[9];
        assert( RT_LightAnimationFor( &unread, 1.0f ) < 0 );
        s_rtImageAverageCount = saved;
    }
    puts( "animated emitters ok" );
    return 0;
}
'''
        compile_run(self, source)

    def test_the_lights_take_their_shaders_stages_each_frame(self):
        surface = function(LIGHTS, 'static void RT_AddSurfaceLights( void )')
        # a surface light follows its surface lit, the shader it draws
        self.assertEqual(surface.count('RT_AnimateNewLights( RT_ResolveShader( shader ), 1.0f, firstLight );'), 2)
        self.assertIn('const uint32_t firstLight = s_rtLights.numLights;', surface)
        emissive = function(LIGHTS, 'static void RT_AddEmissiveSurfaces( void )')
        # a glowing shader's light is its glow
        self.assertIn('RT_AnimateNewLights( shader, 0.0f, firstLight );', emissive)
        free = function(LIGHTS, 'static void RT_LightsFree( void )')
        self.assertIn('if ( s_rtLights.animations ) RT_HostFree( s_rtLights.animations );', free)
        self.assertIn('if ( s_rtLights.animated ) RT_HostFree( s_rtLights.animated );', free)
        upload = function(BACKEND, 'static qboolean RT_UploadLights( rtFrame_t *frame )')
        animate = upload.index('RT_AnimateLights( backEnd.refdef.floatTime );')
        self.assertLess(upload.index('Com_Memcpy( dst, s_rtLights.lights, staticCount * sizeof( rtGpuLight_t ) );'), animate)
        self.assertLess(animate, upload.index('for ( i = 0; i < (int)backEnd.refdef.num_dlights && count < MAX_DLIGHTS; i++ ) {'))
        self.assertIn('VectorScale( s_rtLights.lights[ animated->light ].color,\n'
                      '\t\t\t\t\tanimate ? s_rtLights.animations[ animated->animation ].scale : 1.0f, dst[ animated->light ].color );',
                      upload)
        # rtx_rt_animated_lights 0 keeps each light at its mean; a change
        # starts the denoiser's history over
        self.assertIn('const qboolean animate = ( !rtx_rt_animated_lights || rtx_rt_animated_lights->integer ) ? qtrue : qfalse;',
                      upload)
        settings = function(BACKEND, 'static int RT_TraceSettingsCount( void )')
        self.assertIn('rtx_rt_animated_lights', settings)
        init = read('code/rendererrtx/tr_init.c')
        self.assertIn('rtx_rt_animated_lights = ri.Cvar_Get( "rtx_rt_animated_lights", "1", CVAR_ARCHIVE_ND );', init)
        self.assertIn('ri.Cvar_CheckRange( rtx_rt_animated_lights, "0", "1", CV_INTEGER );', init)
        self.assertIn('extern\tcvar_t\t*rtx_rt_animated_lights;', LOCAL)
        self.assertIn('`rtx_rt_animated_lights`', read('docs/RTX.md'))
        # a still view is averaged only while its clock stands still, when the
        # stages hold still too
        uniforms = BACKEND[BACKEND.index('s_rt.accumulateTime == backEnd.refdef.floatTime && s_rt.historyValid )') - 200:]
        self.assertIn('s_rt.accumulateTime == backEnd.refdef.floatTime', uniforms)

    def test_an_additive_flame_gives_off_all_it_adds(self):
        lit = function(MATERIAL, 'static qboolean RT_ShaderFullbrightLit( const shader_t *shader )')
        self.assertIn('blend = RT_StageBlend( &shader->rtStages[0] );', lit)
        self.assertIn('if ( blend != 0 && blend != ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO ) ) {\n\t\treturn qfalse;', lit)
        # a colour wave's mean is drawn at tr.identityLight, as raster and the
        # trace draw it
        average = function(MATERIAL, 'static qboolean RT_StageAverage(')
        self.assertIn('scale = RT_ColorWave( &s->rgbWave, time ? RT_WaveAt( &s->rgbWave, *time ) : RT_WaveMean( &s->rgbWave ) );',
                      average)
        color = function(MATERIAL, 'static float RT_ColorWave( const waveForm_t *wave, float value )')
        self.assertIn('if ( wave->func != GF_NOISE ) {\n\t\tvalue *= tr.identityLight;', color)
        glsl = function(GLSL, 'vec4 rt_stage_generator(RtStage s, RtHitAttribs h, float time, float light)')
        self.assertIn('if (func != GF_NOISE) {\n\t\t\tglow *= identityLight;', glsl)


if __name__ == '__main__':
    unittest.main()
