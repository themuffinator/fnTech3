"""Execute each renderer's MDR skinning and SP light-grid query with retail vectors."""
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

ANIMATION = r'''
#define main MdrLoaderFixtureMain
#include "tests/mdr_format_tests.c"
#undef main
#include <cstring>
struct probeEntity { struct { int frame, oldframe; float backlerp; } e; } entity;
static struct { probeEntity *currentEntity; } backEnd{&entity};
static struct {
    int numIndexes, numVertexes, surfType; bool needsNormal;
    unsigned indexes[SHADER_MAX_INDEXES];
    float xyz[SHADER_MAX_VERTEXES][4], normal[SHADER_MAX_VERTEXES][4];
    float texCoords[2][SHADER_MAX_VERTEXES][2];
} tess;
#define RB_CHECKOVERFLOW(v,i) assert(tess.numVertexes+(v)<=SHADER_MAX_VERTEXES && tess.numIndexes+(i)<=SHADER_MAX_INDEXES)
'''

ANIMATION_MAIN = r'''
int main() {
    for (int compressed = 0; compressed < 2; ++compressed) {
        Make(compressed); model_t model{};
        assert(R_LoadMDR(&model, original, length, "pose.mdr"));
        auto *header = static_cast<mdrHeader_t *>(model.modelData);
        auto *lod = reinterpret_cast<mdrLOD_t *>(reinterpret_cast<byte *>(header)+header->ofsLODs);
        auto *surface = reinterpret_cast<mdrSurface_t *>(reinterpret_cast<byte *>(lod)+lod->ofsSurfaces);
        for (int step = 0; step <= 4; ++step) {
            std::memset(&tess,0,sizeof(tess)); tess.numVertexes=5; tess.numIndexes=2; tess.needsNormal=true;
            entity.e.frame=1; entity.e.oldframe=0; entity.e.backlerp=step*0.25f;
            RB_MDRSurfaceAnim(surface);
            assert(tess.numVertexes==8 && tess.numIndexes==5);
            assert(tess.indexes[2]==5 && tess.indexes[3]==7 && tess.indexes[4]==6);
            const float translation = 10*(1-entity.e.backlerp)+0.75f;
            assert(tess.xyz[5][0]==translation+1);
            assert(tess.xyz[5][1]==translation+2+1.0f/64);
            assert(tess.xyz[5][2]==translation+3+2.0f/64);
            assert(tess.normal[5][0]==0 && tess.normal[5][1]==0 && tess.normal[5][2]==1);
            assert(tess.texCoords[0][7][0]==1 && tess.texCoords[0][7][1]==-0.5f);
        }
        // Equal frames suppress stale interpolation, including an invalid lerp.
        entity.e.frame=entity.e.oldframe=0; entity.e.backlerp=NAN;
        std::memset(&tess,0,sizeof(tess)); tess.needsNormal=true;
        RB_MDRSurfaceAnim(surface); assert(tess.xyz[0][0]==1.75f);
        free(allocation); allocation=nullptr;
    }
}
'''

LIGHT = r'''
#include "code/qcommon/q_shared.h"
#include <cassert>
#include <cstring>
#include <cmath>
#include "code/renderercommon/tr_stvef_light.h"
#include "code/renderercommon/tr_raven_bsp_format.h"
#define qboolean int
enum { RF_LIGHTING_ORIGIN = 0x80, RF_MINLIGHT = 1, RDF_NOWORLDMODEL = 1,
    FUNCTABLE_SIZE = 1024, FUNCTABLE_MASK = 1023, COM_GAME_STVEF_SP = 5, COM_GAME_STVEF_MP = 6,
    COM_GAME_QUAKE3_IHV = 7,
    DLIGHT_AT_RADIUS = 16, DLIGHT_MINIMUM_RADIUS = 16 };
struct trRefEntity_t {
    struct { int renderfx; vec3_t lightingOrigin, origin, axis[3];
        struct { float hilightIntensity; int entityNum; } rtcw;
        struct { int fullbright, darken; } stvef;
        struct { int active; } q3IHV;
    } e;
    vec3_t ambientLight, directedLight, lightDir;
    int ambientLightInt, lightingCalculated;
};
struct dlight_t { vec3_t origin, color; float radius; int rtcwShader; };
struct trRefdef_t { int rdflags, num_dlights; dlight_t *dlights; struct { int snooperView; } rtcw; };
struct world_t { vec3_t lightGridOrigin, lightGridInverseSize; int lightGridBounds[3]; byte *lightGridData; fnRavenBspView_t ravenBsp; } world;
static struct { world_t *world; float sinTable[1024]; int rtcwWorld;
    float identityLight; int identityLightByte; vec3_t sunDirection; } tr;
static struct { int gameId; } ri;
static cvar_t ambient{}, directed{}, debug{}, cel{};
static cvar_t *r_ambientScale=&ambient, *r_directedScale=&directed, *r_debugLight=&debug, *r_celShadingModelShadows=&cel;
static void FnRTCWWorldGridScale(const int *,float *,float *) {}
static bool R_UsesRTCWLighting() { return false; }
static void LogLight(const trRefEntity_t *) {}
#define myftol(x) ((int)(x))
static struct { trRefEntity_t *currentEntity; } backEnd;
static struct { int numVertexes; float normal[7][4]; } tess;
static bool R_CelShadingActive(const trRefEntity_t *) { return false; }
static void R_CelQuantizeModelLighting(const trRefEntity_t *, byte *) {}
// The IHV test lights its own entities (tests/q3_ihv_model_lighting_tests.py); none here is one.
template<class... T> static void R_SetupQ3IHVEntityLighting(T...) { assert(false); }
template<class... T> static void Q3IHV_LightDiffuse(T...) { assert(false); }
'''

LIGHT_MAIN = r'''
int main() {
    byte grid[64]; ambient.value=directed.value=tr.identityLight=1; tr.identityLightByte=255;
    ri.gameId=COM_GAME_STVEF_SP;
    vec3_t points[]={{0,0,0},{16,32,48},{32,32,32},{63,63,63},{-16,-8,-4}};
    // Direct calls to stvoy.exe 1.20 R_GetLighting (0x45bf10), with a
    // synthetic 2x2x2 grid and the engine's 1024-entry sine table.
    const float golden[10][9]={
        {16,23,30,37,44,51,0,0,1},
        {105.25f,112.25f,119.25f,126.25f,133.25f,140.25f,-.235367686f,.707170069f,-.666717768f},
        {89.5f,96.5f,103.5f,110.5f,117.5f,124.5f,-.226636425f,.942618072f,-.245167494f},
        {160.703125f,167.703125f,174.703125f,181.703125f,188.703125f,195.703125f,-.567071676f,.817836761f,-.0978407636f},
        {147.25f,154.25f,161.25f,168.25f,175.25f,182.25f,-.42076537f,.835822046f,-.352644295f},
        {16,23,30,37,44,51,0,0,1},
        {110.137932f,117.137932f,124.137932f,131.137924f,138.137924f,145.137924f,-.260386258f,.600101352f,-.756357908f},
        {94,101,108,115,122,129,-.296899527f,.875674844f,-.380846769f},
        {160.689178f,167.6875f,174.685822f,181.684143f,188.682465f,195.680771f,-.567186475f,.817747891f,-.0979178026f},
        {148.487122f,155.487122f,162.487122f,169.487122f,176.487122f,183.487122f,-.426338583f,.829564154f,-.360636562f}
    };
    // Native retail R_SetupEntityLighting (0x45b740), including ties to even.
    const unsigned packed[10]={0xff3e3730,0xff979089,0xff88807a,0xffcfc8c1,0xffc1bab3,
                               0xff3e3730,0xff9c958e,0xff8c857e,0xffcfc8c1,0xffc2bbb4};
    for(int i=0;i<1024;++i) tr.sinTable[i]=(float)sin(i*2.0*M_PI/1023);
    for(int i=0;i<3;++i) { world.lightGridInverseSize[i]=1.0f/64; world.lightGridBounds[i]=2; }
    for(int i=0;i<8;++i) {
        for(int j=0;j<6;++j) grid[i*8+j]=(byte)(16+i*21+j*7);
        grid[i*8+6]=(byte)(i*27); grid[i*8+7]=(byte)(i*31);
    }
    tr.world=&world; world.lightGridData=grid;
    for(int pass=0;pass<2;++pass) {
        if(pass) std::memset(grid+16,0,8);
        for(int point=0;point<5;++point) {
            float result[9]; assert(R_LightForPoint(points[point],result,result+3,result+6));
            for(int i=0;i<9;++i) assert(fabsf(result[i]-golden[pass*5+point][i])<(i<6 ? 0.0001f : 0.000001f));
            trRefEntity_t entity{}; entity.e.renderfx=RF_LIGHTING_ORIGIN;
            VectorCopy(points[point],entity.e.lightingOrigin); VectorSet(entity.e.origin,1000,1000,1000);
            R_SetupEntityLightingGrid(&entity);
            for(int i=0;i<3;++i) assert(entity.ambientLight[i]==result[i] && entity.lightDir[i]==result[i+6]);
            trRefdef_t scene{};
            for(int i=0;i<3;++i) entity.e.axis[i][i]=1;
            R_SetupEntityLighting(&scene,&entity);
            assert((unsigned)entity.ambientLightInt==packed[pass*5+point]);
            for(int i=0;i<3;++i) {
                assert(fabsf(entity.ambientLight[i]-(result[i]+32))<.0001f);
                assert(fabsf(entity.directedLight[i]-result[i+3])<.0001f);
                assert(fabsf(entity.lightDir[i]-result[i+6])<.000001f);
            }
        }
    }
    // Boundary samples stay within the allocated grid. Retail's old routine
    // reads beyond it at the high edge, so that undefined behavior is not copied.
    vec3_t edge={64,64,64}; float result[9]; assert(R_LightForPoint(edge,result,result+3,result+6));
    for(int i=0;i<6;++i) assert(result[i]==grid[56+i]);
    ambient.value=2; directed.value=.5f; assert(R_LightForPoint(points[0],result,result+3,result+6));
    assert(result[0]==32 && result[3]==18.5f);
    std::memset(grid,0,sizeof(grid)); assert(R_LightForPoint(points[2],result,result+3,result+6));
    for(float v:result) assert(v==0);
    for(float &v:result) v=123;
    world.lightGridData=nullptr; assert(!R_LightForPoint(points[0],result,result+3,result+6));
    tr.world=nullptr; assert(!R_LightForPoint(points[0],result,result+3,result+6));
    for(float v:result) assert(v==123);
    trRefEntity_t noWorld{}; trRefdef_t scene{};
    R_SetupEntityLighting(&scene,&noWorld); assert(noWorld.ambientLight[0]==182);
    // Native retail RB_CalcDiffuseColor (0x465f00) with an identity light direction.
    const byte diffuse[7][4]={{122,128,136,255},{122,128,136,255},{149,158,167,255},
        {177,187,198,255},{204,217,229,255},{232,246,255,255},{255,255,255,255}};
    const float incoming[7]={-1,0,.25f,.5f,.75f,1,2};
    trRefEntity_t entity{}; entity.lightDir[0]=1; backEnd.currentEntity=&entity; tess.numVertexes=7;
    for(int i=0;i<3;++i) { entity.ambientLight[i]=121.5f+i*7; entity.directedLight[i]=110.5f+i*7; }
    for(int i=0;i<7;++i) tess.normal[i][0]=incoming[i];
    for(int profile=0;profile<=6;++profile) {
        ri.gameId=profile; entity.ambientLightInt=profile>=5 ? 0xff88807a : 0xff878079;
        byte colors[28]; RB_CalcDiffuseColor_scalar(colors);
        if(profile>=5) assert(!std::memcmp(colors,diffuse,sizeof(colors)));
        else {
            assert(colors[0]==121 && colors[2]==135);
            assert(colors[12]==176 && colors[14]==197);
        }
    }
    assert(FnSTVEF_LightByte(NAN)==0 && FnSTVEF_LightByte(INFINITY)==0);
    assert(FnSTVEF_LightByte(-1)==0 && FnSTVEF_LightByte(1e20f)==255);
}
'''


class StvefMdrRenderTests(unittest.TestCase):
    def test_profile_lighting_defaults(self):
        names = ('overBrightBits', 'mapOverBrightBits', 'ambientScale', 'directedScale')
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                source = (ROOT / 'code' / renderer / 'tr_init.c').read_text()
                statements = [re.search(r'r_' + name + r' = ri\.Cvar_Get\(.*?;', source, re.S).group() for name in names]
                self.execute(renderer, r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <string>
#include <map>
#define RITUAL_ML_PROFILE( id ) COM_GAME_ID_IS_RITUAL( id )
static std::map<std::string,std::string> values;
static cvar_t value;
static cvar_t *Get(const char *name,const char *text,int) { values[name]=text; return &value; }
static struct { comGameId_t gameId; decltype(&Get) Cvar_Get; } ri{COM_GAME_QUAKE3,Get};
int main() {
    cvar_t *r_overBrightBits,*r_mapOverBrightBits,*r_ambientScale,*r_directedScale;
    for(int profile=COM_GAME_QUAKE3;profile<COM_GAME_COUNT;++profile) {
        ri.gameId=(comGameId_t)profile;
''' + '\n'.join(statements) + r'''
        const bool ritual=COM_GAME_ID_IS_RITUAL(profile);
        assert(values["r_overBrightBits"]==((profile==COM_GAME_STVEF_SP || ritual) ? "0" : "1"));
        assert(values["r_mapOverBrightBits"]==((profile==COM_GAME_STVEF_SP || profile==COM_GAME_ALICE) ? "0" :
            profile==COM_GAME_STVEF_MP ? "1" : "2"));
        assert(values["r_ambientScale"]==(profile==COM_GAME_QUAKELIVE ? "10" :
            (profile==COM_GAME_STVEF_SP || profile==COM_GAME_STVEF_MP || ritual) ? "0.5" : "0.6"));
        assert(values["r_directedScale"]=="1");
    }
}
''')

    def execute(self, renderer, source):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang C++ compiler unavailable')
        with tempfile.TemporaryDirectory(prefix='stvef-mdr-render-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp); probe = folder / 'probe.cpp'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            probe.write_text(source)
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                                     '-I', str(ROOT), str(probe), '-o', str(executable)],
                                    cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, renderer + result.stdout + result.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(result.returncode, 0, renderer + result.stdout + result.stderr)

    def test_skinning_in_each_renderer(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                source = (ROOT / 'code' / renderer / 'tr_animation.c').read_text()
                self.assertIn('FnMdr_SelectFrames(', source)
                self.execute(renderer, ANIMATION + '\n' + function(source, 'void RB_MDRSurfaceAnim(') + '\n' + ANIMATION_MAIN)

    def test_light_grid_matches_retail(self):
        math = (ROOT / 'code/qcommon/q_math.c').read_text()
        normalization = function(math, 'vec_t VectorNormalize2(') + '\n' + function(math, 'vec_t VectorNormalize(')
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                source = (ROOT / 'code' / renderer / 'tr_light.c').read_text()
                grid = function(source, 'static void R_SetupEntityLightingGrid(')
                query = function(source, 'int R_LightForPoint(')
                setup = function(source, 'void R_SetupEntityLighting(')
                diffuse = function((ROOT / 'code' / renderer / 'tr_shade_calc.c').read_text(), 'static void RB_CalcDiffuseColor_scalar(')
                self.execute(renderer, '\n'.join((LIGHT, normalization, grid, query, setup, diffuse, LIGHT_MAIN)))


if __name__ == '__main__':
    unittest.main()
