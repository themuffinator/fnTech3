"""IHV sky colors, timing and stage lifecycle against original numeric facts."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function
from model_shader_tests import common
from q3_ihv_fog_tests import records

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

HOST = r'''
#include <cmath>
#define Com_Memset memset
struct shaderStage_t {int id;};
struct shader_t {skyParms_t sky;};
static shader_t shader;
static shaderStage_t stages[MAX_SHADER_STAGES],*stagePointers[MAX_SHADER_STAGES];
struct shaderCommands_t {
    shader_t *shader; shaderStage_t **xstages;
    int numVertexes,numIndexes,depthRange;
    qboolean ihvSkyActive; byte ihvSkyColors[MAX_SHADER_STAGES];
    struct {color4ub_t colors[SHADER_MAX_VERTEXES+1];} svars;
};
static shaderCommands_t tess;
static struct {struct {int time;struct {qboolean drawSky;} rtcwFog;} refdef;
    qboolean skyRenderedThisView;} backEnd;
static cvar_t fastSky{},showSky{};
static cvar_t *r_fastsky=&fastSky,*r_showsky=&showSky;
static struct {qboolean clearAttachment;} vk{qtrue};
static struct {image_t *defaultImage;float identityLight;} tr;
enum {CLS_NONE,CLS_TEXCOORD_ARRAY,CT_FRONT_SIDED,
    DEPTH_RANGE_NORMAL,DEPTH_RANGE_ZERO,DEPTH_RANGE_ONE};
static float sky_min,sky_max,sky_min_depth;
static int fillVertices,draws,fallbackColors,clipCalls;
static byte drawn[MAX_SHADER_STAGES][4];
static void GL_ClientState(int,int){}
static void qglColor4f(float,float,float,float){}
static void GL_State(int){}
static void GL_Cull(int){}
static void qglDepthRange(double,double){}
static void RB_RTCWStageFog(void*){}
static void RB_RTCWFogToggle(qboolean){}
static void DrawSkyBox(shader_t*){}
static void RB_ClipSkyPolygons(shaderCommands_t*){++clipCalls;}
static void FillCloudBox(const shader_t*){tess.numVertexes=fillVertices;}
'''

CHECKS = r'''
static void Reset(int active,int lightning){
    shader={};shader.sky.cloudHeight=512;shader.sky.ihvClouds=1;
    shader.sky.ihvLightning=lightning;ihvSkySeed=0;
    tess={};tess.shader=&shader;tess.xstages=stagePointers;
    for(int i=0;i<MAX_SHADER_STAGES;++i){stages[i].id=i;stagePointers[i]=i<active?stages+i:nullptr;}
    fillVertices=5;draws=fallbackColors=clipCalls=0;fastSky={};showSky={};
    backEnd={};backEnd.refdef.rtcwFog.drawSky=qtrue;
    memset(tess.svars.colors,99,sizeof tess.svars.colors);
}
int main(){
    for(const auto &v:vectors){
        if(!v.time)Reset(v.stages,v.lightning);
        draws=0;backEnd.refdef.time=v.time;RB_StageIteratorSky();
        assert(draws==v.stages && !fallbackColors && !tess.ihvSkyActive);
        assert(ihvSkySeed==v.seed && fabsf(shader.sky.ihvLastFlash-v.last)<.000001f);
        for(int stage=0;stage<v.stages;++stage){
            for(int c=0;c<3;++c)assert(drawn[stage][c]==v.color[stage][c]);
            assert(drawn[stage][3]==255);
        }
        assert(backEnd.skyRenderedThisView);
    }
    // A skipped sky must not consume the global stream. With no geometry the
    // iterator still visits the original stages, including its terminator.
    for(int skip:{0,1}){
        Reset(2,1);if(skip)fastSky.integer=1;else backEnd.refdef.rtcwFog.drawSky=qfalse;
        RB_StageIteratorSky();assert(!ihvSkySeed&&!draws&&!clipCalls&&!tess.ihvSkyActive);
    }
    Reset(2,1);fillVertices=0;RB_StageIteratorSky();
    assert(ihvSkySeed==69070u&&!draws&&!tess.ihvSkyActive);
    Reset(2,1);shader.sky.cloudHeight=0;RB_StageIteratorSky();
    assert(!ihvSkySeed&&!draws&&!tess.ihvSkyActive);
    for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKE3_IHV,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,
            COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){
        ri.gameId=profile;
        for(int legacy:{0,1}){
            Reset(2,1);shader.sky.ihvClouds=shader.sky.ihvLightning=legacy;
            RB_StageIteratorSky();const bool ihv=(profile==COM_GAME_QUAKE3||profile==COM_GAME_QUAKE3_IHV)&&legacy;
            assert(draws==2 && fallbackColors==(ihv?0:2));
            assert(ihvSkySeed==(ihv?69070u:0));
            assert(drawn[0][0]==(ihv?255:42)&&drawn[1][0]==(ihv?0:42));
            // Following ordinary geometry receives its own color generator.
            const int before=fallbackColors;Compute(stages);assert(fallbackColors==before+1);
        }
    }
    ri.gameId=COM_GAME_QUAKE3;Reset(8,1);RB_PrepareQ3IHVSky();
    // Maximum vertex count, opaque alpha, unknown stages, and output canaries.
    tess.numVertexes=SHADER_MAX_VERTEXES;Compute(stages+7);
    for(int i=0;i<SHADER_MAX_VERTEXES;++i){assert(!tess.svars.colors[i].rgba[0]);assert(tess.svars.colors[i].rgba[3]==255);}
    for(byte c:tess.svars.colors[SHADER_MAX_VERTEXES].rgba)assert(c==99);
    color4ub_t color;memset(&color,71,sizeof color);shaderStage_t unknown{};
    assert(!RB_Q3IHVSkyColors(&unknown,color.rgba));for(byte c:color.rgba)assert(c==71);
    tess.numVertexes=0;Compute(stages);assert(!fallbackColors);
    // The random stream spans shaders but each sky owns its flash timestamp.
    Reset(2,1);backEnd.refdef.time=1000;RB_PrepareQ3IHVSky();const auto firstSeed=ihvSkySeed;
    shader_t second=shader;second.sky.ihvLastFlash=7;tess.shader=&second;
    RB_PrepareQ3IHVSky();assert(ihvSkySeed!=firstSeed&&second.sky.ihvLastFlash==7&&shader.sky.ihvLastFlash==0);
}
'''


def fixture(renderer):
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    sky = (ROOT / 'code' / renderer / 'tr_sky.c').read_text()
    shade = (ROOT / 'code' / renderer / 'tr_shade.c').read_text()
    limits = '\n'.join(re.findall(r'^#define (?:MAX_SHADER_STAGES|SHADER_MAX_VERTEXES)\s+[^\n]+', local, re.M))
    source = common(renderer) + limits + '\n' + records(local, ('skyParms_t',)) + HOST
    if renderer != 'renderer':
        source += '#define USE_VULKAN\n'
    source += '#include "code/renderercommon/tr_q3_ihv_sky_render.h"\n'
    # Keep the actual dispatch and empty-batch check. Other color generators
    # have separate coverage; the sentinel proves they are/aren't selected.
    color = function(shade, 'void R_ComputeColors(')
    source += color[:color.index('\n\t//')] + '\n++fallbackColors;\n'
    source += ('memset(tess.svars.colors,42,tess.numVertexes*4);}\n'
               'static void Compute(const shaderStage_t *stage){R_ComputeColors(stage);}\n'
               if renderer == 'renderer' else
               'memset(dest,42,tess.numVertexes*4);}\n'
               'static void Compute(const shaderStage_t *stage){R_ComputeColors(0,tess.svars.colors,stage);}\n')
    source += r'''
static void RB_StageIteratorGeneric(){
    for(int stage=0;stage<MAX_SHADER_STAGES&&tess.xstages[stage];++stage){
        Compute(tess.xstages[stage]);memcpy(drawn[stage],tess.svars.colors[0].rgba,4);++draws;
    }
}
'''
    source += function(sky, 'static void R_BuildCloudData(')
    source += function(sky, 'void RB_StageIteratorSky(')
    data = json.loads((ROOT / 'tests/fixtures/q3_ihv_sky_vectors.json').read_text())
    source += 'struct Vector {int stages,lightning,time;uint32_t seed;float last;byte color[8][3];};static const Vector vectors[]={\n'
    for case in data['cases']:
        for frame in case['frames']:
            colors = ','.join('{' + ','.join(str(round(255*c)) for c in rgb) + '}' for rgb in frame['stage_rgb'])
            source += (f'{{{case["stages"]},{case["lightning"]},{frame["time"]},{frame["seed"]}u,'
                       f'{frame["last_flash"]:.9e}f,{{{colors}}}}},\n')
    return source + '};\n' + CHECKS


def pipeline_fixture(renderer):
    shader_source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    source = common(renderer) + records(local, ('skyParms_t',)) + r'''
#define USE_VULKAN
#define Com_Memset memset
#define NUM_TEXTURE_BUNDLES 2
enum {TESS_ST0=1,TESS_RGBA0=4,TYPE_SIGNLE_TEXTURE=11};
struct textureBundle_t {image_t *image[1];};
struct shaderStage_t {qboolean active;int tessFlags;textureBundle_t bundle[NUM_TEXTURE_BUNDLES];};
static struct {skyParms_t sky;int numUnfoggedPasses;} shader;
static shaderStage_t stages[3];
static bool EqualTCgen(int,shaderStage_t *a,shaderStage_t *){return a!=nullptr;}
static bool EqualRGBgen(shaderStage_t *a,shaderStage_t *){return a!=nullptr;}
static bool EqualACgen(shaderStage_t *a,shaderStage_t *){return a!=nullptr;}
'''
    source += function((ROOT / 'code/renderercommon/tr_q3_ihv_shader.h').read_text(), 'static qboolean R_Q3IHVSeparateSkyStages(')
    # Compile the original prologue, including conditional compilation. Neither
    # the Vulkan path nor the caller may enter a retail multitexture collapse.
    collapse = function(shader_source, 'static int CollapseMultitexture(')
    source += collapse[:collapse.index('\n\tif ( shader.rtcwNoFog')] + '\nreturn 1;}\n'
    start = shader_source.index('\t\t\tif ( R_Q3IHVSeparateSkyStages() ) {')
    end = shader_source.index('\n\t\t\t}', start) + len('\n\t\t\t}')
    source += 'static int Pipeline(shaderStage_t *pStage){struct {int shader_type;} def{99};\n'
    source += shader_source[start:end] + '\nreturn def.shader_type;}\n'
    start = shader_source.index('\tCom_Memset( lastStage, 0, sizeof( lastStage ) );')
    end = shader_source.index('\n#endif', start)
    source += 'static void Reuse(){int i,n;shaderStage_t *lastStage[NUM_TEXTURE_BUNDLES];\n'
    source += shader_source[start:end] + '\n}\n'
    source += r'''
int main(){
    image_t image{};
    for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKE3_IHV,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,
            COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP})
    for(int clouds:{0,1,2})for(int lightning:{0,1}){
        ri.gameId=profile;shader.sky.ihvClouds=clouds;shader.sky.ihvLightning=lightning;
        const bool ihv=(profile==COM_GAME_QUAKE3||profile==COM_GAME_QUAKE3_IHV)&&(clouds||lightning);
        assert(CollapseMultitexture(0,stages,stages+1,2)==!ihv);
        shader.numUnfoggedPasses=2;
        for(int s=0;s<2;++s){
            stages[s]={};stages[s].active=qtrue;stages[s].tessFlags=TESS_RGBA0|TESS_ST0;
            stages[s].bundle[0].image[0]=&image;
            assert(Pipeline(stages+s)==(ihv?TYPE_SIGNLE_TEXTURE:99));
        }
        Reuse();assert(bool(stages[1].tessFlags&TESS_RGBA0)==ihv);
        assert(!(stages[1].tessFlags&TESS_ST0));
    }
}
'''
    return source


class Q3IHVSkyTests(unittest.TestCase):
    def compile_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='q3-ihv-sky-', dir=ROOT / '.tmp') as temp:
            cpp = Path(temp) / 'probe.cpp'
            exe = Path(temp) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(source)
            result = subprocess.run([compiler, *flags, '-std=c++17', '-O2', '-ffast-math',
                                     '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                    cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_reference_colors_and_renderer_lifecycle(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                self.compile_run(fixture(renderer))

    def test_vulkan_pipelines_keep_per_stage_colors(self):
        for renderer in RENDERERS[1:]:
            with self.subTest(renderer=renderer):
                self.compile_run(pipeline_fixture(renderer))


if __name__ == '__main__':
    unittest.main()
