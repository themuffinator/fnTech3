"""Outcast's measured MP UI renderer PODs, native/QVM transfers and Fn dispatch."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/compat/jk2_render_adapter.hpp"
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
struct Drop {};
static char lastError[512];
void QDECL Com_Error(errorParm_t,const char *format,...){
 va_list args;va_start(args,format);std::vsnprintf(lastError,sizeof(lastError),format,args);va_end(args);throw Drop{};
}
static bool outcast=true;
int Com_GameIsJK2MP(){return outcast;}
static struct { struct { int vidHeight; } glconfig; } cls{{480}};
static vm_t *uivm;
static int calls,adjusted,tagResult=1;
static bool invalidOutput=false;
static refEntity_t entity;
static refdef_t view;
static std::array<polyVert_t,3> polygon;
static std::array<float,7> light;
static std::array<char,64> name;
static qhandle_t Register(const char *value){++calls;std::strcpy(name.data(),value);return 123;}
static void Clear(){++calls;}
static void AddEntity(const refEntity_t *value,qboolean effects){++calls;assert(!effects);entity=*value;}
static void Render(const refdef_t *value){++calls;view=*value;}
static void Polygon(qhandle_t shader,int count,const polyVert_t *vertices,int polys){
 ++calls;assert(shader==7&&count==3&&polys==1);std::copy(vertices,vertices+3,polygon.begin());
}
static void Light(const vec3_t origin,float radius,float r,float g,float b){
 ++calls;std::copy(origin,origin+3,light.begin());light[3]=radius;light[4]=r;light[5]=g;light[6]=b;
}
static float BadFloat(){std::uint32_t bits=0x7fc00001u;float value;std::memcpy(&value,&bits,4);return value;}
static void Bounds(qhandle_t model,vec3_t mins,vec3_t maxs){
 ++calls;assert(model==123);for(int n=0;n<3;++n){mins[n]=float(-n-1);maxs[n]=float(n+1);}
 if(invalidOutput)maxs[2]=BadFloat();
}
static int Tag(orientation_t *result,qhandle_t model,int first,int last,float fraction,const char *tag){
 ++calls;assert(model==123&&first==2&&last==5&&fraction==0.25f&&!std::strcmp(tag,"tag_logo"));
 *result={};for(int n=0;n<3;++n){result->origin[n]=tagResult?float(n+10):0.0f;result->axis[n][n]=1;}
 if(invalidOutput)result->axis[1][2]=BadFloat();return tagResult;
}
static void CL_UIAdjustVirtualRefdef(refdef_t *value){++adjusted;value->x+=3;value->width+=5;}
struct Renderer {
 decltype(&Register) RegisterModel,RegisterSkin;decltype(&Clear) ClearScene;
 decltype(&AddEntity) AddRefEntityToScene;decltype(&Polygon) AddPolyToScene;
 decltype(&Light) AddLightToScene;decltype(&Render) RenderScene;
 decltype(&Tag) LerpTag;decltype(&Bounds) ModelBounds;
} re{Register,Register,Clear,AddEntity,Polygon,Light,Render,Tag,Bounds};
'''

CHECKS = r'''
template<class F>void Reject(F attempt){bool caught=false;try{attempt();}catch(Drop){caught=true;}assert(caught);}
static intptr_t Bits(float value){std::int32_t bits;std::memcpy(&bits,&value,4);return bits;}
static intptr_t Invoke(int op,intptr_t a=0,intptr_t b=0,intptr_t c=0,intptr_t d=0,intptr_t e=0,intptr_t f=0){
 intptr_t args[]{op,a,b,c,d,e,f},result=-99;assert(CL_JK2UIRenderCall(args,result));return result;
}
static void Check(bool native){
 std::array<byte,4128> bytes;bytes.fill(0xa5);auto *data=bytes.data()+16;
 vm_t vm{};vm.dataBase=data;vm.dataMask=4095;vm.exactDataLength=4096;
 if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 uivm=&vm;
 auto at=[&](int offset)->intptr_t{return native?reinterpret_cast<intptr_t>(data+offset):offset;};
 std::strcpy(reinterpret_cast<char *>(data+1),"models/map_objects/bespin/jk2logo.md3");
 std::strcpy(reinterpret_cast<char *>(data+80),"tag_logo");
 assert(Invoke(18,at(1))==123&&!std::strcmp(name.data(),reinterpret_cast<char *>(data+1)));
 assert(Invoke(19,at(1))==123);assert(Invoke(21)==0);
 fntech3_jk2_mp_refEntity_t source{};source.hModel=123;source.renderfx=0xc0;
 source.axis[0][0]=source.axis[1][1]=source.axis[2][2]=1;
 source.origin[0]=42;source.oldorigin[1]=13;source.lightingOrigin[2]=17;
 source.frame=2;source.oldframe=1;source.backlerp=0.25f;source.customSkin=9;source.customShader=10;
 source.skinNum=3;source.shaderRGBA[0]=250;source.shaderRGBA[3]=127;source.shaderTime=3.5f;source.shaderTexCoord[1]=0.5f;
 auto submit=[&]{std::memcpy(data+129,&source,sizeof(source));return Invoke(22,at(129));};
 auto before=bytes;assert(submit()==0);std::memcpy(before.data()+16+129,&source,sizeof(source));assert(bytes==before);
 assert(entity.reType==RT_MODEL&&entity.hModel==123&&entity.origin[0]==42&&entity.oldorigin[1]==13&&entity.lightingOrigin[2]==17);
 assert(entity.renderfx==(RF_NOSHADOW|RF_LIGHTING_ORIGIN)&&entity.frame==2&&entity.oldframe==1&&entity.backlerp==0.25f);
 assert(entity.customSkin==9&&entity.customShader==10&&entity.skinNum==3&&entity.shader.rgba[0]==250&&entity.shader.rgba[3]==127);
 assert(entity.shaderTime.f==3.5f&&entity.shaderTexCoord[1]==0.5f&&!entity.jka.forceAlpha&&!entity.jka.rgbTint);
 const auto valid=source;const int beforeReject=calls;
 source.ghoul2=1;Reject(submit);source=valid;
 for(int type:{-1,1,2,4,7,11,12}){source.reType=type;Reject(submit);}source=valid;
 for(int flag:{0x10,0x20,0x1000,INT_MIN}){source.renderfx=flag;Reject(submit);}source=valid;
 source.axis[1][2]=BadFloat();Reject(submit);
 assert(std::strstr(lastError,"entity22:")&&std::strstr(lastError,"field=axis"));source=valid;
 source.backlerp=1.01f;Reject(submit);
 assert(std::strstr(lastError,"field=backlerp")&&std::strstr(lastError,"frame=2/1"));source=valid;
 assert(calls==beforeReject);
 // The renderer reads 0x400 as forced entity alpha whoever adds the entity;
 // 0x800 reaches nothing and is dropped (jk2_render_adapter.hpp).
 source.renderfx=0x400;assert(submit()==0&&entity.jka.forceAlpha&&entity.renderfx==0);
 source.renderfx=0x800;assert(submit()==0&&!entity.jka.forceAlpha&&entity.renderfx==0);
 source=valid;assert(submit()==0&&calls==beforeReject+3);
 // Different from Academy: Outcast puts the axis at36 and time at72.
 fntech3_jk2_mp_refdef_t scene{};scene.x=10;scene.y=11;scene.width=320;scene.height=240;
 scene.fov_x=37;scene.fov_y=34;scene.viewaxis[0][0]=scene.viewaxis[1][1]=scene.viewaxis[2][2]=1;
 scene.vieworg[2]=23;scene.time=1234567;scene.rdflags=1;scene.areamask[31]=0x81;scene.text[7][30]='z';
 auto render=[&]{std::memcpy(data+513,&scene,sizeof(scene));return Invoke(25,at(513));};
 const int adjustments=adjusted;assert(render()==0&&adjusted==adjustments+1);
 assert(view.x==13&&view.y==11&&view.width==325&&view.height==240&&view.fov_x==37&&view.fov_y==34);
 assert(view.vieworg[2]==23&&view.viewaxis[2][2]==1&&view.time==1234567&&view.areamask[31]==0x81&&view.text[7][30]=='z');
 assert(view.rdflags==(RDF_NOWORLDMODEL|RDF_NOFOVCORRECTION));
 const auto validScene=scene;const int oldCalls=calls;
 scene.rdflags=8;Reject(render);scene=validScene;scene.width=INT_MAX;Reject(render);scene=validScene;
 scene.fov_x=BadFloat();Reject(render);
 assert(std::strstr(lastError,"view25:")&&std::strstr(lastError,"field=fov_x")&&std::strstr(lastError,"rect=10,11 320x240"));
 scene=validScene;scene.viewaxis[2][2]=BadFloat();Reject(render);scene=validScene;
 assert(calls==oldCalls&&adjusted==adjustments+1);
 // Each shader-text slot is independently bounded, including the final row.
 // Reject before changing either the converted record or renderer state.
 for(int row=0;row<8;++row){
  scene=validScene;std::memset(scene.text[row],'x',sizeof(scene.text[row]));
  const auto sourceBefore=scene;refdef_t converted=view;const auto convertedBefore=converted;
  assert(fntech3::jk2::render::View(scene,converted)==fntech3::jk2::render::Status::Invalid);
  assert(!std::memcmp(&converted,&convertedBefore,sizeof(converted)));
  Reject(render);assert(!std::memcmp(&scene,&sourceBefore,sizeof(scene)));
  assert(std::strstr(lastError,"view25:")&&std::strstr(lastError,"field=text"));
  assert(calls==oldCalls&&adjusted==adjustments+1);
  scene.text[row][31]=0;
  assert(fntech3::jk2::render::View(scene,converted)==fntech3::jk2::render::Status::Ok);
  assert(!std::memcmp(converted.text[row],scene.text[row],32));
 }
 scene=validScene;
 // Retail logo view at640x480 is partly offscreen. Keep its signed origin.
 scene.x=-77;scene.y=84;scene.width=scene.height=248;scene.fov_x=scene.fov_y=45;
 assert(render()==0&&view.x==-74&&view.y==84&&view.width==253&&view.height==248);
 scene.y=-50;assert(render()==0&&view.y==-50);
 // Both the GL origin and its far edge must fit, after viewport adjustment.
 // Here glBottom fits but glTop=480-INT_MIN does not.
 scene.y=INT_MIN;const auto viewportCalls=calls;Reject(render);assert(calls==viewportCalls);
 assert(std::strstr(lastError,"invalid derived viewport"));
 scene=validScene;
 for(const refdef_t &invalid : [] {
  std::array<refdef_t,6> views{};
  for(auto &value:views){value.width=320;value.height=240;}
  views[0].y=INT_MIN;views[1].y=INT_MAX;views[2].x=INT_MAX;
  views[3].width=0;views[4].height=-1;
  views[5].y=INT_MIN+480;views[5].height=1;
  // Exact GL top INT_MAX+1 is independently rejected.
  return views;
 }()) Reject([&]{CL_JK2UIValidateViewport(invalid,480);});
 refdef_t limit{};limit.x=INT_MIN;limit.y=481-INT_MAX;limit.width=1;limit.height=1;
 CL_JK2UIValidateViewport(limit,480); // Both GL endpoints remain representable.
 fntech3_jk2_mp_polyVert_t vertices[3]{};
 for(int n=0;n<3;++n){vertices[n].xyz[0]=float(n);vertices[n].st[1]=0.5f;vertices[n].modulate[3]=byte(200+n);}
 std::memcpy(data+1025,vertices,sizeof(vertices));assert(Invoke(23,7,3,at(1025))==0);
 assert(polygon[2].xyz[0]==2&&polygon[1].st[1]==0.5f&&polygon[2].modulate.rgba[3]==202);
 vertices[2].xyz[1]=BadFloat();std::memcpy(data+1025,vertices,sizeof(vertices));
 int count=calls;Reject([&]{Invoke(23,7,3,at(1025));});
 for(int n:{-1,0,1,2,32769,INT_MAX})Reject([&]{Invoke(23,7,n,at(1025));});assert(calls==count);
 vec3_t origin{4,5,6};std::memcpy(data+1537,origin,sizeof(origin));
 assert(Invoke(24,at(1537),Bits(90),Bits(0.5f),Bits(0.75f),Bits(1))==0);
 assert((light==std::array<float,7>{4,5,6,90,0.5f,0.75f,1}));
 count=calls;Reject([&]{Invoke(24,at(1537),Bits(BadFloat()),0,0,0);});assert(calls==count);
 before=bytes;assert(Invoke(62,123,at(2201),at(2217))==0);
 vec3_t mins,maxs;std::memcpy(mins,data+2201,12);std::memcpy(maxs,data+2217,12);
 assert(mins[2]==-3&&maxs[2]==3);std::memcpy(before.data()+16+2201,mins,12);std::memcpy(before.data()+16+2217,maxs,12);assert(bytes==before);
 assert(Invoke(29,at(2301),123,2,5,Bits(0.25f),at(80))==0);
 fntech3_jk2_mp_orientation_t tag;std::memcpy(&tag,data+2301,sizeof(tag));assert(tag.origin[2]==12&&tag.axis[1][1]==1);
 tagResult=0;assert(Invoke(29,at(2301),123,2,5,Bits(0.25f),at(80))==0);
 std::memcpy(&tag,data+2301,sizeof(tag));assert(tag.origin[2]==0&&tag.axis[1][1]==1);tagResult=1;
 count=calls;Reject([&]{Invoke(29,at(2301),123,-1,5,Bits(0.25f),at(80));});
 Reject([&]{Invoke(29,at(2301),123,2,-1,Bits(0.25f),at(80));});assert(calls==count);
 before=bytes;invalidOutput=true;
 Reject([&]{Invoke(62,123,at(2201),at(2217));});assert(bytes==before);
 Reject([&]{Invoke(29,at(2301),123,2,5,Bits(0.25f),at(80));});assert(bytes==before);invalidOutput=false;
 count=calls;Reject([&]{Invoke(62,123,at(2201),0);});assert(bytes==before&&calls==count);
 std::memset(data+3001,'x',MAX_QPATH);Reject([&]{Invoke(18,at(3001));});assert(calls==count);
 if(!native){
  Reject([&]{Invoke(22,at(4000));});Reject([&]{Invoke(25,at(3900));});Reject([&]{Invoke(23,7,3,at(4070));});
  Reject([&]{Invoke(62,123,at(2201),at(4090));});Reject([&]{Invoke(29,at(4070),123,2,5,Bits(0.25f),at(80));});
  assert(calls==count);
  if(sizeof(intptr_t)>4)Reject([&]{Invoke(18,static_cast<intptr_t>(std::uint64_t(1)<<32)|1);});
 }
 Reject([&]{Invoke(18,0);});Reject([&]{Invoke(18,-1);});
 outcast=false;Reject([&]{Invoke(18,at(1));});outcast=true;assert(calls==count);
 uivm=nullptr;Reject([&]{Invoke(18,at(1));});
 intptr_t args[]{99},result=321;assert(!CL_JK2UIRenderCall(args,result)&&result==321);
}
int main(){Check(false);Check(true);}
'''


class JK2UIRenderTests(unittest.TestCase):
    def test_production_native_and_qvm_render_transfers(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        vm = (ROOT / 'code/qcommon/vm.c').read_text()
        ui = (ROOT / 'code/client/client_jk2_ui.hpp').read_text()
        source = PRELUDE + function(vm, 'void *VM_ExplicitArgPtr(') + function(vm, 'void VM_CheckBounds(')
        source += function(ui, 'static void *CL_JK2UISpan(')
        source += '#include "code/client/client_jk2_ui_render.hpp"\n' + CHECKS
        scratch = ROOT / '.tmp/tests/jk2-ui-render'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            directory = Path(temporary).resolve()
            cpp = directory / 'host.cpp'
            cpp.write_text(source)
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            for extra in ([], ['-O2', '-ffast-math']):
                executable = directory / ('host.exe' if os.name == 'nt' else 'host')
                built = subprocess.run([compiler, *target, '-std=c++17', *extra, '-Werror',
                                        '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT), str(cpp), '-o', str(executable)],
                                       cwd=directory, capture_output=True, text=True, timeout=90)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                ran = subprocess.run([str(executable)], cwd=directory, capture_output=True, text=True, timeout=30)
                self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
