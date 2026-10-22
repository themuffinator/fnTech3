"""Execute the shared CSM light policy: retain ambient, lamps and unlit art."""
import unittest

from entity_sun_shadow_tests import SUN_PRELUDE, compile_and_run, read
from csm_receiver_tests import enum, function


class AmbientTests(unittest.TestCase):
    def test_authored_ambient_and_malformed_keys(self):
        source = SUN_PRELUDE + r'''
#define VectorClear(v) ((v)[0] = (v)[1] = (v)[2] = 0.0f)
#define VectorSet(v,x,y,z) ((v)[0]=(x), (v)[1]=(y), (v)[2]=(z))
''' + read('code/renderercommon/tr_sky_sun_units.h') + read('code/renderercommon/tr_csm_ambient.h') + r'''
static void Expect(int game, const char *text, float r, float g, float b) {
    vec3_t out = {999,999,999};
    R_CSMReadAmbient(text, game, out);
    assert(out[0] == r && out[1] == g && out[2] == b);
}
int main() {
    Expect(COM_GAME_RTCW_SP, "{ \"_color\" \"1 1 1\" \"ambient\" \"25\" }", 25,25,25);
    Expect(COM_GAME_QUAKE3, "{ \"_color\" \"1 0.5 0.25\" \"ambient\" \"20\" }", 20,10,5);
    Expect(COM_GAME_QUAKE3, "{ \"ambient\" \"20\" }", 0,0,0);
    Expect(COM_GAME_QUAKELIVE, "{ \"_ambient\" \"12\" }", 12,12,12);
    Expect(COM_GAME_QUAKE3, "{ \"ambient\" \"20\" \"_ambient\" \"5\" }", 5,5,5);
    Expect(COM_GAME_QUAKE3, "{ \"_color\" \"1 1 1\" \"ambient\" \"20\" } { \"ambient\" \"200\" }", 20,20,20);
    Expect(COM_GAME_MOHAA, "{ \"ambientlight\" \"4 3 2\" } { \"ambientlight\" \"99 99 99\" }", 4,3,2);
    Expect(COM_GAME_ALICE, "{ \"ambientlight\" \"4 3 2\" } { \"ambientlight\" \"9 8 7\" }", 9,8,7);
    Expect(COM_GAME_FAKK2, "{ \"ambientlight\" \"4 3 2\" }", 4,3,2);
    Expect(COM_GAME_QUAKE3, nullptr, 0,0,0);
    Expect(COM_GAME_QUAKE3, "", 0,0,0);
    Expect(COM_GAME_QUAKE3, "{ \"ambient\"", 0,0,0);
    Expect(COM_GAME_QUAKELIVE, "{ \"_ambient\" \"nan\" }", 0,0,0);
    Expect(COM_GAME_QUAKELIVE, "{ \"_ambient\" \"1e99\" }", 0,0,0);
    Expect(COM_GAME_QUAKELIVE, "{ \"_ambient\" \"-3\" }", 0,0,0);
    Expect(COM_GAME_MOHAA, "{ \"ambientlight\" \"nan inf -5\" }", 0,0,0);
    return 0;
}
'''
        compile_and_run(self, source)


MODEL_PRELUDE = r'''
#include <cassert>
#include <cmath>
#include <cstring>
typedef float vec3_t[3];
typedef unsigned char byte;
#define ID_INLINE inline
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define VectorClear(v) ((v)[0] = (v)[1] = (v)[2] = 0.0f)
#define VectorSet(v,x,y,z) ((v)[0]=(x), (v)[1]=(y), (v)[2]=(z))
#define DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define VectorLength(v) std::sqrt(DotProduct(v,v))
static float Com_Clamp(float lo,float hi,float v) { return v < lo ? lo : v > hi ? hi : v; }
static float VectorNormalize2(const float *v,float *o) {
    float d=VectorLength(v); for(int i=0;i<3;++i) o[i]=d>0?v[i]/d:0; return d;
}
struct trRefEntity_t { vec3_t csmSunLight, lightDir, ambientLight, directedLight; };
static struct { struct { bool valid,unitColour; vec3_t directionToSun,color,light; } worldSun; int overbrightBits; } tr;
struct cvar_t { int integer; };
static cvar_t enabled={1},overbright={0};
static cvar_t *r_csmShadows=&enabled,*r_mapOverBrightBits=&overbright;
'''


class ModelTests(unittest.TestCase):
    def test_sun_budget_preserves_ambient_and_dynamic_lights(self):
        compile_and_run(self, MODEL_PRELUDE + read('code/renderercommon/tr_csm_model_light.h') + r'''
int main() {
    trRefEntity_t e{};
    tr.worldSun.valid=true;
    VectorSet(tr.worldSun.directionToSun,0,0,1);
    VectorSet(tr.worldSun.light,80,40,20);
    VectorSet(e.lightDir,0,0,1);
    VectorSet(e.ambientLight,50,50,50);
    VectorSet(e.directedLight,100,100,100);
    const vec3_t normal={0,0,1},back={0,0,-1},side={1,0,0};
    byte out[4];
    R_CSMCaptureEntitySun(&e);
    assert(e.csmSunLight[0]==80 && e.csmSunLight[1]==40 && e.csmSunLight[2]==20);
    R_CSMModelShadowColor(&e,normal,normal,out);
    assert(out[0]==136 && out[1]==68 && out[2]==34 && out[3]==255);
    // A dynamic lamp increases total light after the sun was captured.
    VectorSet(e.directedLight,200,200,200);
    R_CSMModelShadowColor(&e,normal,normal,out);
    assert(out[0]==81 && out[1]==40 && out[2]==20);
    // Saturation still leaves at least all of the 200 ambient bytes.
    VectorSet(e.ambientLight,200,200,200);
    R_CSMModelShadowColor(&e,normal,normal,out);
    assert(out[0]<=55 && out[1]<=55 && out[2]<=55);
    R_CSMModelShadowColor(&e,back,normal,out);
    assert(out[0]==0 && out[1]==0 && out[2]==0);
    R_CSMModelShadowColor(&e,side,normal,out);
    assert(out[0]==0 && out[1]==0 && out[2]==0);
    // A grid pointing away from the sun supplies no sun budget.
    VectorSet(e.lightDir,0,0,-1);
    R_CSMCaptureEntitySun(&e);
    assert(e.csmSunLight[0]==0 && e.csmSunLight[1]==0 && e.csmSunLight[2]==0);
    VectorSet(e.lightDir,0,0,1);
    VectorSet(e.directedLight,10,5,2);
    R_CSMCaptureEntitySun(&e);
    assert(e.csmSunLight[0]==10 && e.csmSunLight[1]==5 && e.csmSunLight[2]==2);
    enabled.integer=0;
    R_CSMCaptureEntitySun(&e);
    assert(e.csmSunLight[0]==0);
    enabled.integer=1;tr.worldSun.valid=false;
    R_CSMCaptureEntitySun(&e);
    assert(e.csmSunLight[0]==0);
    // No division by zero on an unlit entity or zero normal.
    std::memset(&e,0,sizeof(e));
    R_CSMModelShadowColor(&e,e.lightDir,normal,out);
    assert(out[0]==0 && out[1]==0 && out[2]==0);
    return 0;
}
''')


class CasterTests(unittest.TestCase):
    def test_cutout_materials_cache_policy_and_split_bounds(self):
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                vulkan = renderer != 'renderer'
                prelude = MODEL_PRELUDE[:MODEL_PRELUDE.index('struct trRefEntity_t')]
                prelude += 'typedef int qboolean; enum {qfalse,qtrue};\n'
                prelude += '#define VectorSubtract(a,b,o) ((o)[0]=(a)[0]-(b)[0],(o)[1]=(a)[1]-(b)[1],(o)[2]=(a)[2]-(b)[2])\n'
                prelude += '#define NUM_TEXTURE_BUNDLES 2\n#define GLS_ATEST_BITS 7\n#define GLS_ALICE_ATEST_BITS 8\n'
                prelude += enum(read(f'code/{renderer}/tr_local.h'), 'alphaGen_t')
                prelude += enum(read(f'code/{renderer}/tr_local.h'), 'texCoordGen_t')
                prelude += '#define USE_VULKAN\n' if vulkan else ''
                prelude += r'''
struct textureBundle_t { int numImageAnimations,numTexMods; alphaGen_t alphaGen; bool isVideoMap; texCoordGen_t tcGen; };
struct shaderStage_t { unsigned stateBits; alphaGen_t alphaGen; textureBundle_t bundle[2]; };
struct shader_t { shader_t *remappedShader; int numDeforms,numUnfoggedPasses; shaderStage_t *stages[8]; };
struct orientationr_t { vec3_t origin,axis[3]; };
static shader_t opaque;
static struct { shader_t *defaultShader; } tr={&opaque};
'''
                compile_and_run(self, prelude + read('code/renderercommon/tr_csm_casters.h') + r'''
int main() {
    shaderStage_t stage{}; shader_t s{};
    s.numUnfoggedPasses=1; s.stages[0]=&stage;
    stage.alphaGen=AGEN_IDENTITY;
    for(int i=0;i<2;++i) stage.bundle[i].alphaGen=AGEN_IDENTITY;
    assert(R_CSMCasterShader(&s)==&opaque);
    assert(R_CSMCasterMaterialStatic(&s));
    for(unsigned cutout=1;cutout<=8;cutout*=8) {
        stage.stateBits=cutout;
        assert(R_CSMCasterShader(&s)==&s);
        assert(R_CSMCasterMaterialStatic(&s));
        stage.bundle[0].numImageAnimations=2;
        assert(!R_CSMCasterMaterialStatic(&s));
        stage.bundle[0].numImageAnimations=0;
        stage.bundle[0].numTexMods=1;
        assert(!R_CSMCasterMaterialStatic(&s));
        stage.bundle[0].numTexMods=0;
        stage.bundle[0].isVideoMap=true;
        assert(!R_CSMCasterMaterialStatic(&s));
        stage.bundle[0].isVideoMap=false;
        stage.bundle[0].tcGen=TCGEN_ENVIRONMENT_MAPPED;
        assert(!R_CSMCasterMaterialStatic(&s));
        stage.bundle[0].tcGen=TCGEN_TEXTURE;
        stage.alphaGen=stage.bundle[0].alphaGen=AGEN_WAVEFORM;
        assert(!R_CSMCasterMaterialStatic(&s));
        stage.alphaGen=stage.bundle[0].alphaGen=AGEN_IDENTITY;
    }
    stage.stateBits=0;s.numDeforms=1;
    assert(R_CSMCasterShader(&s)==&s && !R_CSMCasterMaterialStatic(&s));
    s.numDeforms=0;
    shader_t remap=s;remap.stages[0]=&stage;stage.stateBits=1;s.remappedShader=&remap;
    assert(R_CSMCasterShader(&s)==&remap);
    // A moving/scaled/rotated entity conservatively intersects the split.
    orientationr_t object{},view{};
    for(int i=0;i<3;++i) {object.axis[i][i]=1; view.axis[i][i]=1;}
    vec3_t mins={-10,-20,-30},maxs={10,20,30};
    object.origin[0]=100;
    assert(!R_CSMBoundsOutsideSplit(mins,maxs,&object,&view,90,110));
    assert(!R_CSMBoundsOutsideSplit(mins,maxs,&object,&view,110,200));
    assert(R_CSMBoundsOutsideSplit(mins,maxs,&object,&view,112,200));
    assert(R_CSMBoundsOutsideSplit(mins,maxs,&object,&view,0,88));
    object.axis[0][0]=0;object.axis[0][1]=2;object.axis[1][0]=-2;object.axis[1][1]=0;
    assert(!R_CSMBoundsOutsideSplit(mins,maxs,&object,&view,139,200));
    assert(R_CSMBoundsOutsideSplit(mins,maxs,&object,&view,142,200));
    return 0;
}
''')

    def test_all_backends_keep_cutout_stages_and_bound_receivers(self):
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                backend = read(f'code/{renderer}/tr_backend.c')
                caster = function(backend, 'static int RB_RenderCSMShadowCascade(')
                self.assertIn('casterShader = R_CSMCasterShader( shader );', caster)
                self.assertIn('surfaceActive && casterShader != batchShader', caster)
                self.assertIn('RB_BeginSurface( casterShader, 0 );', caster)
                receiver = function(backend, 'static void RB_CSMShadowReceiverPass(')
                self.assertIn('R_CSMBoundsOutsideSplit( mins, maxs', receiver)
                self.assertIn('receiverShader != batchShader', receiver)
                self.assertIn('RB_BeginSurface( receiverShader, 0 );', receiver)
                self.assertIn('if ( !receiverShader->numDeforms ) receiverShader = tr.whiteShader;', receiver)
                shade = read(f'code/{renderer}/tr_shade.c')
                self.assertIn('tess.csmCasterPass && !( pStage->stateBits & GLS_DEPTHMASK_TRUE )', shade)
                self.assertIn('if ( tess.csmCasterPass ) RB_StageIteratorGeneric();', shade)
                self.assertIn('if ( tess.csmShadowPass ) {\n\t\tRB_DeformTessGeometry();', shade)
                self.assertIn('!casterShader->numDeforms &&', caster)
                self.assertIn('RB_DlightShadowCacheHashPtr( hash, R_CSMCasterShader( shader ) )', backend)


class LegacyShadowTests(unittest.TestCase):
    def test_only_published_world_shadow_maps_disable_legacy_shadows(self):
        compile_and_run(self, r'''
#include <cassert>
#include <cstring>
#include <cstdarg>
#define ID_INLINE inline
#define PRINT_ALL 0
struct publication_t { bool published; };
struct shadowManager_t {
    bool planned, noWorldModel;
    publication_t csmAtlasPublication, pointAtlasPublication, spotAtlasPublication;
};
struct cvar_t { int integer; };
static cvar_t shadows={3};
static cvar_t *r_shadows=&shadows;
static int writes,notices;
static void Set(const char *name,const char *value) {
    assert(!std::strcmp(name,"cg_shadows") && !std::strcmp(value,"0"));
    ++writes; shadows.integer=0;
}
static void Print(int,const char *,...) { ++notices; }
static struct { void (*Cvar_Set)(const char *,const char *); void (*Printf)(int,const char *,...); } ri={Set,Print};
''' + read('code/renderercommon/tr_shadow_legacy.h') + r'''
int main() {
    shadowManager_t m{};
    R_DisableLegacyShadowsForShadowMaps(nullptr);
    R_DisableLegacyShadowsForShadowMaps(&m);
    m.planned=true;
    // Disabled/no-sun/unsupported/allocation-failed passes publish no atlas.
    R_DisableLegacyShadowsForShadowMaps(&m);
    assert(writes==0 && shadows.integer==3);
    publication_t *maps[]={&m.csmAtlasPublication,&m.pointAtlasPublication,&m.spotAtlasPublication};
    for(auto atlas:maps) {
        atlas->published=true;
        m.noWorldModel=true;
        R_DisableLegacyShadowsForShadowMaps(&m);
        m.noWorldModel=false; m.planned=false;
        R_DisableLegacyShadowsForShadowMaps(&m);
        assert(shadows.integer==3);
        m.planned=true;
        for(int legacy=1;legacy<=3;++legacy) {
            shadows.integer=legacy;
            int before=writes;
            R_DisableLegacyShadowsForShadowMaps(&m);
            assert(shadows.integer==0 && writes==before+1);
            R_DisableLegacyShadowsForShadowMaps(&m);
            assert(writes==before+1); // No cvar churn on every frame/cache hit.
        }
        atlas->published=false;
        R_DisableLegacyShadowsForShadowMaps(&m);
        assert(shadows.integer==0); // No invented restore value when maps stop.
        shadows.integer=3;
    }
    r_shadows=nullptr; m.csmAtlasPublication.published=true;
    R_DisableLegacyShadowsForShadowMaps(&m);
    assert(writes==9 && notices==9);
    return 0;
}
''')

    def test_each_backend_checks_after_atlas_passes(self):
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                source = read(f'code/{renderer}/tr_backend.c')
                scheduler = function(source, 'static void RB_RunScheduledShadowManagerPreMainPasses(')
                self.assertIn('#include "../renderercommon/tr_shadow_legacy.h"', source)
                self.assertGreater(scheduler.index('R_DisableLegacyShadowsForShadowMaps'),
                                   scheduler.index('RB_RunScheduledShadowManagerPass'))


if __name__ == '__main__':
    unittest.main()
