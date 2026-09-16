#!/usr/bin/env python3
"""Compare RTCW light submission and projected passes with four source trees.

Unchanged reference functions are compiled only in disposable licensed fixtures.
GPU calls are recorded, not executed. All projection fixtures use original
platform color conversion: Win32 x87 FISTP or the portable C truncation path.
iortcw's conversion callback is controlled accordingly; its own Win32 SSE
conversion truncates. Its optional greyscale/backface filters use defaults.
No installed assets or modules are needed or copied.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from model_shader_tests import function
from rtcw_light_tests import SCENE, BACKEND, entity_fixture


SCENE_CHECKS = r'''
int main() {
    const vec3_t origin={1,2,3}, color={0.25f,0.5f,1};
    unsigned cases=0, accepted=0;
    ri.gameId=REFERENCE_SP ? COM_GAME_RTCW_SP : COM_GAME_RTCW_MP;
    for(bool registered:{false,true}) for(int count:{0,1,31,32})
    for(int hardware:{GLHW_GENERIC,GLHW_RIVA128,GLHW_PERMEDIA2})
    for(int dynamicMode:{-1,0,1,2,3}) for(float scale:{-1.f,0.f,0.125f,1.f,2.f})
    for(float radius:{-1.f,0.f,0.01f,1.f,250.f,1000.f})
    for(uint32_t flags:{0u,1u,2u,10u,11u,64u,0x80000000u,0x8000000au,0x40000000u,0xc0000002u}) {
        tr.registered=reference::tr.registered=registered;
        r_numdlights=reference::r_numdlights=count;
        glConfig.hardwareType=hardware; dynamic.integer=dynamicMode; radiusScale.value=scale;
        data={}; reference::data={}; // Never exercise the original stale-slot bug as a desired behavior.
        RE_AddRTCWLightToScene(origin,radius,color[0],color[1],color[2],int(flags));
        reference::RE_AddLightToScene(origin,radius,color[0],color[1],color[2],int(flags));
        assert(r_numdlights==reference::r_numdlights);
        if(r_numdlights>count) {
            const auto &a=data.dlights[count]; const auto &b=reference::data.dlights[count];
            assert(a.radius==b.radius && a.rtcwOverdraw==b.overdraw && a.rtcwShader==b.dlshader);
            assert(!std::memcmp(a.origin,b.origin,sizeof(vec3_t)) && !std::memcmp(a.color,b.color,sizeof(vec3_t)));
            ++accepted;
        }
        ++cases;
    }
    std::printf("{\"cases\":%u,\"accepted\":%u}\n",cases,accepted);
}
'''


def scene_fixture(source, mode, variant):
    text = SCENE[:SCENE.index('int main()')]
    text += '\n#define REFERENCE_SP ' + str(int(mode == 'sp')) + r'''
namespace reference {
enum { MAX_DLIGHTS=32, REF_FORCE_DLIGHT=0x80000000u, REF_JUNIOR_DLIGHT=0x40000000u };
struct dlight_t { vec3_t origin,color; float radius; shader_t *dlshader; int overdraw; bool forced; };
static struct { dlight_t dlights[32]; } data;
static struct { bool registered; int smpFrame=0; } tr;
static int r_numdlights;
static cvar_t *r_dlightScale=&::radiusScale;
static qhandle_t RE_RegisterShader(const char *name) {
    assert(!std::strcmp(name,"negdlightshader") || !std::strcmp(name,"flamedlightshader"));
    return !std::strcmp(name,"negdlightshader") ? 1 : 2;
}
static shader_t *R_GetShaderByHandle(qhandle_t handle) { return handle==1 ? &::negative : &::flame; }
'''
    text += 'static decltype(data) *backEndData' + ('[1]={&data};\n' if variant == 'original' else '=&data;\n')
    text += function(source, 'void RE_AddLightToScene(') + '\n}\n' + SCENE_CHECKS
    return text


PROJECTION_CHECKS = r'''
static uint32_t randomState=0x71925631;
static float Random(float range) {
    randomState=randomState*1664525u+1013904223u;
    return (float(int(randomState>>8)-0x800000)/float(0x800000))*range;
}
int main() {
    shader_t surface{}; tess.shader=&surface;
    shaderStage_t stages[3]={{GLS_SRCBLEND_ZERO,{{1}}},{GLS_DSTBLEND_ONE,{{2}}},{GLS_SRCBLEND_DST_COLOR,{{3}}}};
    shader_t material{3,0,0,{stages,stages+1,stages+2,nullptr}};
    unsigned cases=0, vertices=0, drawCount=0;
    for(int sample=0;sample<20000;++sample) {
        tess.numVertexes=12; tess.numIndexes=12;
        for(int i=0;i<12;++i) tess.indexes[i]=i;
        for(int i=0;i<12;++i) for(int axis=0;axis<3;++axis) tess.xyz[i][axis]=Random(2048);
        backEnd.refdef.num_dlights=1+sample%3;
        tess.dlightBits=sample%8;
        const bool snooper=(sample%17)==0;
        backEnd.refdef.rdflags=snooper ? RDF_SNOOPERVIEW : 0;
        backEnd.refdef.rtcw.snooperView=snooper;
        for(int i=0;i<backEnd.refdef.num_dlights;++i) {
            auto &light=backEnd.refdef.dlights[i];
            for(int axis=0;axis<3;++axis) { light.transformed[axis]=Random(1000); light.color[axis]=Random(1.5f); }
            light.radius=100+std::fabs(Random(3000));
            light.rtcwShader=sample%5==0 ? &material : nullptr; light.rtcwOverdraw=sample%3;
            // Exact boundaries and half-radius falloff, plus outside clipping.
            if(sample%4==0) for(int j=0;j<12;++j) for(int axis=0;axis<3;++axis)
                tess.xyz[j][axis]=light.transformed[axis]+light.radius*float((j+axis)%9-4)*0.25f;
        }
        draws.clear(); backEnd.pc={}; RB_RTCWProjectLights();
        const auto actual=draws; const auto counters=backEnd.pc;
        draws.clear(); backEnd.pc={}; reference::Project();
        assert(actual.size()==draws.size());
        for(size_t i=0;i<draws.size();++i) {
            const auto &a=actual[i]; const auto &b=draws[i];
            assert(a.texture==b.texture && a.state==b.state && a.indexes==b.indexes);
            if(a.colors!=b.colors || a.coordinates!=b.coordinates) {
                for(size_t v=0;v<a.colors.size();++v) if(a.colors[v]!=b.colors[v])
                    std::fprintf(stderr,"sample %d draw %zu color %zu: %u / %u\n",sample,i,v,unsigned(a.colors[v]),unsigned(b.colors[v]));
                for(size_t v=0;v<a.coordinates.size();++v) if(a.coordinates[v]!=b.coordinates[v])
                    std::fprintf(stderr,"sample %d draw %zu coordinate %zu: %a / %a\n",sample,i,v,double(a.coordinates[v]),double(b.coordinates[v]));
                assert(false && "projection data differs");
            }
            vertices+=unsigned(a.colors.size()/4); ++drawCount;
        }
        assert(counters.c_totalIndexes==backEnd.pc.c_totalIndexes && counters.c_dlightIndexes==backEnd.pc.c_dlightIndexes
            && counters.c_dlightVertexes==backEnd.pc.c_dlightVertexes);
        ++cases;
    }
    std::printf("{\"cases\":%u,\"draws\":%u,\"vertexComparisons\":%u}\n",cases,drawCount,vertices);
}
'''


def projection_fixture(source, variant, conversion):
    text = '#include <cmath>\n' + BACKEND[:BACKEND.index('int main()')]
    text = text.replace('shader_t *rtcwShader; int rtcwOverdraw;',
                        'union { shader_t *rtcwShader; shader_t *dlshader; }; union { int rtcwOverdraw; int overdraw; };')
    text = text.replace('int num_dlights;', 'int num_dlights,rdflags;')
    text = text.replace('vec4_t xyz[SHADER_MAX_VERTEXES];', 'vec4_t xyz[SHADER_MAX_VERTEXES],normal[SHADER_MAX_VERTEXES];')
    text = text.replace('std::vector<glIndex_t> indexes; };',
                        'std::vector<glIndex_t> indexes; std::vector<byte> colors; std::vector<float> coordinates; };')
    text = text.replace('draws.push_back({texture,state,{indexes,indexes+count}});', r'''
    Draw draw{texture,state,{indexes,indexes+count}};
    assert(glColorType==GL_UNSIGNED_BYTE);
    for(int i=0;i<tess.numVertexes*4;++i) draw.colors.push_back(static_cast<const byte *>(glColors)[i]);
    for(int i=0;i<tess.numVertexes*2;++i) draw.coordinates.push_back(glCoordinates[i]);
    draws.push_back(std::move(draw));
''')
    text += r'''
namespace reference {
enum { RDF_SNOOPERVIEW=4, GL_TEXTURE_COORD_ARRAY=30, GL_COLOR_ARRAY=31 };
#define MAC_STATIC
static void qglEnableClientState(int) {}
static void R_FogOn() {}
static void R_FogOff() {}
static float Q_fabs(float value) { return std::fabs(value); }
''' + conversion + r'''
static struct { decltype(&myftol) ftol; } ri{myftol};
static cvar_t grey{}, backs{}; static cvar_t *r_greyscale=&grey, *r_dlightBacks=&backs;
'''
    name = 'ProjectDlightTexture' if variant == 'original' else 'ProjectDlightTexture_scalar'
    text += function(source, f'static void {name}(')
    text += f'\nstatic void Project() {{ backs.integer=1; {name}(); }}\n}}\n'
    text += 'using reference::RDF_SNOOPERVIEW;\n' + PROJECTION_CHECKS
    return text


def rounding_fixture(conversion):
    return '#include <cfenv>\n' + SCENE[:SCENE.index('int main()')] + '\nnamespace reference {\n' + conversion + r'''
}
int main() {
    const int previous=std::fegetround(); unsigned cases=0;
    for(int mode:{FE_TONEAREST,FE_TOWARDZERO,FE_UPWARD,FE_DOWNWARD}) {
        assert(!std::fesetround(mode));
        for(int i=-32768;i<=32768;++i) {
            const float value=float(i)*0.125f;
            assert(RTCW_LightByte(value)==static_cast<byte>(reference::myftol(value)));
            ++cases;
        }
    }
    assert(!std::fesetround(previous));
    std::printf("{\"cases\":%u}\n",cases);
}
'''


ENTITY_COMPARISONS=r'''
int main() {
    ri.gameId=REFERENCE_SP ? COM_GAME_RTCW_SP : COM_GAME_RTCW_MP;
    dlight_t lights[2]{}; trRefdef_t view{}; view.dlights=lights;
    unsigned cases=0;
    for(float identity:{0.125f,0.5f,1.f}) for(bool grid:{false,true})
    for(float highlight:{-0.5f,0.f,0.25f,0.75f,2.f}) for(int flags:{0,int(RF_MINLIGHT),int(RF_LIGHTING_ORIGIN),RF_MINLIGHT|RF_LIGHTING_ORIGIN})
    for(int entityNum:{-1,0,63,64,128}) for(bool snooper:{false,true})
    for(int count:{0,1,2}) for(bool special:{false,true}) {
        tr.identityLight=identity; tr.identityLightByte=int(identity*255);
        world.lightGridData=grid ? &world : nullptr;
        view.rdflags=(grid ? 0 : RDF_NOWORLDMODEL)|(snooper ? RDF_SNOOPERVIEW : 0);
        view.rtcw.snooperView=snooper; view.num_dlights=count;
        for(int i=0;i<2;++i) {
            auto &light=lights[i]; VectorSet(light.origin,i*100,5,10); VectorSet(light.color,0.25f,0.5f,1.f);
            light.radius=i ? 500 : 10; light.rtcwShader=special && i==0 ? &world : nullptr;
        }
        trRefEntity_t actual{};
        actual.e.renderfx=flags; actual.e.rtcw.hilightIntensity=actual.e.hilightIntensity=highlight;
        actual.e.rtcw.entityNum=actual.e.entityNum=entityNum;
        VectorSet(actual.e.origin,1,2,3); VectorSet(actual.e.lightingOrigin,20,30,40);
        actual.e.axis[0][1]=-1; actual.e.axis[1][0]=1; actual.e.axis[2][2]=1;
        auto expected=actual;
        R_SetupEntityLighting(&view,&actual); reference::R_SetupEntityLighting(&view,&expected);
        assert(actual.lightingCalculated==expected.lightingCalculated);
        assert(!std::memcmp(actual.ambientLight,expected.ambientLight,sizeof(vec3_t)));
        assert(!std::memcmp(actual.directedLight,expected.directedLight,sizeof(vec3_t)));
        assert(!std::memcmp(actual.lightDir,expected.lightDir,sizeof(vec3_t)));
        ++cases;
    }
    std::printf("{\"cases\":%u}\n",cases);
}
'''


def entity_comparisons(source, mode, renderer):
    text=entity_fixture(renderer)+'\n#define REFERENCE_SP '+str(int(mode=='sp'))+'\n'
    text+='\n#undef MAX_CLIENTS\n#define MAX_CLIENTS 64\nnamespace reference {\n'
    text+=function(source,'void R_SetupEntityLighting(')+'\n}\n'+ENTITY_COMPARISONS
    return text


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('sp-source','mp-source','iortcw-source','report'):
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    report_path=args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target=['--target='+args.target] if args.target else ([] if os.name=='nt' else ['-m32'])
    triple=subprocess.check_output([args.compiler,*target,'-dumpmachine'],cwd=ROOT,text=True).strip()
    native_x87=os.name=='nt' and triple.startswith(('i386','i686'))
    report=dict(scope=__doc__,dateUtc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        compiler=args.compiler,target=triple,sanitized=args.sanitize,
        implementationSha256={str(path.relative_to(ROOT)):sha(path) for path in
            [*(ROOT/'code/renderercommon').glob('tr_rtcw_light_*.h'),ROOT/'tests/rtcw_light_tests.py']},results=[])
    references=[('sp','original',args.sp_source/'src'),('mp','original',args.mp_source/'src')]
    references += [(mode,'iortcw',args.iortcw_source/mode.upper()/'code') for mode in ('sp','mp')]
    for mode,variant,reference in references:
        conversion_path=(args.sp_source if mode=='sp' else args.mp_source)/'src/renderer/tr_shade_calc.c'
        conversion_source=conversion_path.read_text(encoding='utf-8')
        conversion=function(conversion_source,'long myftol(') if native_x87 else 'static long myftol(float value) { return (long)value; }'
        checks=[('scene','tr_scene.c'),('projection','tr_shade.c')]
        checks += [('entity-'+renderer,'tr_light.c') for renderer in ('renderer','renderervk','rendererrtx')]
        if variant=='original': checks.append(('color-conversion','tr_shade_calc.c'))
        for kind,filename in checks:
            path=reference/'renderer'/filename; source=path.read_text(encoding='utf-8')
            fixture=(scene_fixture(source,mode,variant) if kind=='scene' else
                     entity_comparisons(source,mode,kind.removeprefix('entity-')) if kind.startswith('entity-') else
                     rounding_fixture(conversion) if kind=='color-conversion' else projection_fixture(source,variant,conversion))
            notice=source[:source.index('*/')+2]+'\n'
            with tempfile.TemporaryDirectory(prefix='light-reference-',dir=report_path.parent) as temporary:
                folder=Path(temporary); cpp=folder/'probe.cpp'; cpp.write_text(notice+'#include <cstdio>\n'+fixture,encoding='utf-8')
                executable=folder/('probe.exe' if os.name=='nt' else 'probe')
                command=[args.compiler,*target,'-std=c++17','-O1','-I',str(ROOT),str(cpp),'-o',str(executable)]
                scalar_sse=kind=='projection' and os.name!='nt' and triple.startswith(('i386','i686'))
                if scalar_sse:
                    # Compare stored binary32 arithmetic, avoiding x87 extended
                    # register lifetimes varying with reference control flow.
                    command+=['-msse2','-mfpmath=sse']
                if os.name=='nt' and triple.startswith('x86_64'): command+=['-DFNTECH3_Q3_ONLY']
                if args.sanitize: command+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
                build=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90)
                result=dict(mode=mode,reference=variant,kind=kind,source=str(path),sourceSha256=sha(path),
                    buildExitCode=build.returncode,buildOutput=build.stdout+build.stderr,passed=False)
                result['scalarSseForBinary32Comparison']=scalar_sse
                if kind in ('projection','color-conversion'): result.update(colorConversion='original Win32 x87' if native_x87 else 'original portable C',
                    conversionSource=str(conversion_path),conversionSourceSha256=sha(conversion_path))
                if kind.startswith('entity-'):
                    result.update(scope='Ambient/direct light vectors and direction, controlled lightgrid; packed byte shading excluded',
                        rendererSourceSha256=sha(ROOT/'code'/kind.removeprefix('entity-')/'tr_light.c'))
                if build.returncode==0:
                    environment=os.environ.copy()
                    if args.sanitize: environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
                    options={'creationflags':subprocess.CREATE_NO_WINDOW} if os.name=='nt' else {}
                    run=subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=60,env=environment,**options)
                    result.update(exitCode=run.returncode,diagnostics=run.stderr,observations=json.loads(run.stdout) if run.returncode==0 else None)
                    result['passed']=run.returncode==0 and result['observations']['cases']>0
                report['results'].append(result)
                report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
                print(variant,mode,kind,'PASS' if result['passed'] else 'FAIL',result.get('observations'),flush=True)
    return 0 if all(row['passed'] for row in report['results']) else 1


if __name__=='__main__':
    raise SystemExit(main())
