"""Execute RTCW decal geometry and its owned native/QVM memory boundary."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]

GEOMETRY_HOST = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/surfaceflags.h"
#include "code/qcommon/game_profile.h"
#include "code/renderercommon/tr_rtcw_marks.h"
#include <cassert>
#include <cstring>
#include <climits>
#include <cstddef>
#include <initializer_list>
#define MAX_VERTS_ON_POLY 64
#define MAX_GRID_SIZE 129
#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2
#define Com_Memcpy memcpy
#define Com_Memset memset
enum surfaceType_t { SF_BAD, SF_FACE, SF_GRID, SF_TRIANGLES };
struct srfSurfaceFace_t { surfaceType_t surfaceType; cplane_t plane; int numIndices, numPoints, ofsIndices; float points[8][8]; int indices[12]; };
struct drawVert_t { vec3_t xyz,normal; };
struct srfGridMesh_t { surfaceType_t surfaceType; int width,height; drawVert_t verts[16]; };
struct shader_t { int surfaceFlags,contentFlags; };
struct msurface_t { shader_t *shader; surfaceType_t *data; };
struct mnode_t { int contents,visframe; cplane_t *plane; mnode_t *children[2]; int nummarksurfaces; msurface_t **firstmarksurface; };
struct world_t { mnode_t *nodes; int numnodes; };
static struct { world_t *world; int visCount; } tr;
static void Error(int,const char *,...) { throw 7; }
static struct { comGameId_t gameId; decltype(&Error) Error; } ri{COM_GAME_RTCW_SP,Error};
'''

GEOMETRY_CHECKS = r'''
#include "code/renderercommon/tr_rtcw_marks_surface.h"
template<class F> void Reject(F action){bool caught=false;try{action();}catch(int){caught=true;}assert(caught);}
int main(){
 const vec3_t corners[4]={{-2,-2,0},{2,-2,0},{2,2,0},{-2,2,0}}, projection={0,0,4};
 fnRTCWMarkPoint_t points[40]; markFragment_t fragments[8];
 auto clear=[&]{memset(points,0xa5,sizeof(points));memset(fragments,0xa5,sizeof(fragments));};
 srfSurfaceFace_t face{}; face.surfaceType=SF_FACE; face.plane.normal[2]=1;face.plane.type=2;
 face.numPoints=3;face.numIndices=3;face.ofsIndices=offsetof(srfSurfaceFace_t,indices);
 face.points[0][0]=-10;face.points[0][1]=-10;face.points[1][0]=10;face.points[1][1]=-10;face.points[2][1]=10;
 for(int i=0;i<3;++i)face.indices[i]=i;
 shader_t shader{};msurface_t surface{&shader,&face.surfaceType};msurface_t *marks[]={&surface,&surface};
 mnode_t node{};node.visframe=10;node.nummarksurfaces=2;node.firstmarksurface=marks;
 world_t world{&node,1};tr.world=&world;tr.visCount=10;
 auto invoke=[&](int cap=8,int size=40,int rotation=0){return R_MarkRTCWFragments(rotation,corners,projection,size,points,cap,fragments);};
 for(auto mode:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  ri.gameId=mode;
  for(int cap:{-8,8})for(int angle:{-450,-90,0,37,90,359,720}){
   clear();assert(invoke(cap,40,angle)==1);assert(fragments[0].firstPoint==0);
   const int count=abs(fragments[0].numPoints);assert(count==4);assert((fragments[0].numPoints<0)==(cap>0));
   for(int i=0;i<count;++i){assert(points[i].xyz[2]==0);assert(fabsf(points[i].xyz[0])<=2.83f&&fabsf(points[i].xyz[1])<=2.83f);
    if(cap>0){assert(points[i].st[0]>=-0.001f&&points[i].st[0]<=1.001f);assert(points[i].st[1]>=-0.001f&&points[i].st[1]<=1.001f);}
    else{unsigned char saved[8];memset(saved,0xa5,8);assert(!memcmp(points[i].st,saved,8));}
   }
   for(unsigned char value:reinterpret_cast<unsigned char(&)[sizeof(fnRTCWMarkPoint_t)]>(points[count]))assert(value==0xa5);
   clear();assert(invoke(cap,3,angle)==0);for(unsigned char value:reinterpret_cast<unsigned char(&)[sizeof(points)]>(points))assert(value==0xa5);
  }
  clear();assert(invoke(1)==1&&fragments[0].numPoints==-4);
  for(int flag:{SURF_NOIMPACT,SURF_NOMARKS}){shader.surfaceFlags=flag;assert(invoke()==0);}shader.surfaceFlags=0;
  shader.contentFlags=CONTENTS_FOG;assert(invoke()==0);shader.contentFlags=0;
  node.contents=CONTENTS_TRANSLUCENT;assert(invoke()==0);node.contents=0;
  node.visframe=7;assert(invoke()==0);node.visframe=8;assert(invoke()==1);node.visframe=10;
  face.plane.normal[2]=-1;assert(invoke()==0);face.plane.normal[2]=1;
  face.surfaceType=SF_TRIANGLES;assert(invoke()==0);face.surfaceType=SF_FACE;
  face.plane.dist=3;assert(invoke()==0);face.plane.dist=0;
  face.indices[1]=3;Reject([&]{invoke();});face.indices[1]=1;
  face.numIndices=2;Reject([&]{invoke();});face.numIndices=3;
  const vec3_t zero={};assert(R_MarkRTCWFragments(0,corners,zero,40,points,8,fragments)==0);
  vec3_t invalid={NAN,0,1};Reject([&]{R_MarkRTCWFragments(0,corners,invalid,40,points,8,fragments);});
  invalid[0]=1e30f;Reject([&]{R_MarkRTCWFragments(0,corners,invalid,40,points,8,fragments);});
  vec3_t degenerate[4]{};assert(R_MarkRTCWFragments(0,degenerate,projection,40,points,8,fragments)==0);
 }
 ri.gameId=COM_GAME_RTCW_SP;
 // Both curve triangles keep XYZ-only mapping, including signed output capacity.
 srfGridMesh_t grid{};grid.surfaceType=SF_GRID;grid.width=grid.height=2;
 for(int i=0;i<4;++i){grid.verts[i].xyz[0]=(i%2)*2-1;grid.verts[i].xyz[1]=1-(i/2)*2;}
 surface.data=&grid.surfaceType;
 for(int cap:{-8,8}){clear();assert(invoke(cap)==2);assert(fragments[0].numPoints==3&&fragments[1].numPoints==3&&fragments[1].firstPoint==3);}
 // The original shared plane-count quirk depends on a preceding accepted face.
 for(int i=0;i<4;++i)grid.verts[i].xyz[2]=3;
 assert(invoke()==0);
 msurface_t faceSurface{&shader,&face.surfaceType};msurface_t *mixed[]={&faceSurface,&surface};node.firstmarksurface=mixed;
 assert(invoke()==3);assert(fragments[0].numPoints==-4&&fragments[1].numPoints==3);
 node.firstmarksurface=marks;surface.data=&face.surfaceType;
 // Ordered BSP traversal and duplicate leaves don't duplicate geometry.
 cplane_t split{};split.normal[0]=1;split.type=0;
 mnode_t nodes[3]{};nodes[0].contents=CONTENTS_NODE;nodes[0].visframe=10;nodes[0].plane=&split;
 nodes[0].children[0]=&nodes[1];nodes[0].children[1]=&nodes[2];nodes[1]=nodes[2]=node;
 world={nodes,3};assert(invoke()==1);
 nodes[0].children[1]=nodes;Reject([&]{invoke();});world={&node,1};
 Reject([&]{invoke(INT_MIN);});Reject([&]{invoke(8,-1);});
 assert(R_MarkRTCWFragments(0,nullptr,nullptr,0,nullptr,8,nullptr)==0);
 tr.world=nullptr;assert(invoke()==0);tr.world=&world;world.nodes=nullptr;assert(invoke()==0);world.nodes=&node;
 for(auto mode:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){ri.gameId=mode;Reject([&]{invoke();});}
}
'''

IMPORT_HOST = r'''
#include "code/qcommon/q_shared.h"
#include "code/compat/rtcw_game_abi.h"
#include "code/renderercommon/tr_rtcw_marks.h"
#include <cassert>
#include <array>
#include <cstring>
#include <vector>
#include <utility>
#include <memory>
#include <climits>
struct vm_t { unsigned char *base; size_t size; };
static std::vector<std::pair<uintptr_t,size_t>> allowed;
static int calls, allocations, fault;
static const void *originalCorners;
void QDECL Com_Error(errorParm_t,const char *,...){throw 7;}
static void Bounds(vm_t *vm,intptr_t address,size_t size){
 if(vm->base){if(address<=0||static_cast<size_t>(address)>vm->size||size>vm->size-static_cast<size_t>(address))throw 8;return;}
 for(const auto &range:allowed)if(static_cast<uintptr_t>(address)>=range.first&&size<=range.second&&static_cast<uintptr_t>(address)-range.first<=range.second-size)return;
 throw 8;
}
#define VM_CHECKBOUNDS Bounds
static void *VM_ExplicitArgPtr(vm_t *vm,intptr_t address){return vm->base?vm->base+address:reinterpret_cast<void *>(address);}
static void *Hunk_AllocateTempMemory(int bytes){if(fault==1)throw 9;++allocations;return new unsigned char[bytes];}
static void Hunk_FreeTempMemory(void *memory){--allocations;delete[] static_cast<unsigned char *>(memory);}
static int Backend(int rotation,const vec3_t *corners,const vec3_t projection,int maxPoints,fnRTCWMarkPoint_t *points,int maxFragments,markFragment_t *fragments){
 ++calls;assert(rotation==-37&&maxPoints==8&&abs(maxFragments)==3);assert(corners!=originalCorners);
 assert(corners[0][0]==11&&projection[2]==5);
 for(int i=0;i<3;++i){for(int j=0;j<3;++j)points[i].xyz[j]=float(i*3+j);points[i].st[0]=0.25f;points[i].st[1]=0.75f;}
 fragments[0]={0,maxFragments<0?3:-3};
 if(fault==2)throw 9;
 if(fault==3)return 4;
 if(fault==4)fragments[0].firstPoint=6;
 if(fault==5)fragments[0].numPoints=INT_MIN;
 if(fault==6)points[1].xyz[2]=NAN;
 if(fault==7)points[1].st[0]=NAN;
 return 1;
}
static struct{decltype(&Backend) MarkRTCWFragments;}re{Backend};
#include "code/client/client_rtcw_marks.hpp"
template<class F>void Reject(F action){bool rejected=false;try{action();}catch(int){rejected=true;}assert(rejected&&allocations==0);}
int main(){
 static_assert(sizeof(fntech3_rtcw_markPoint_t)==20&&sizeof(fntech3_rtcw_markFragment_t)==8);
 for(bool qvm:{false,true}){
  std::array<unsigned char,1024> storage;vm_t vm{qvm?storage.data():nullptr,storage.size()};
  allowed={{reinterpret_cast<uintptr_t>(storage.data()),storage.size()}};
  auto address=[&](int offset){return qvm?intptr_t(offset):reinterpret_cast<intptr_t>(storage.data()+offset);};
  auto reset=[&]{storage.fill(0xa5);const vec3_t corners[4]={{11,12,13},{14,15,16},{17,18,19},{20,21,22}},projection={0,0,5};
   memcpy(storage.data()+1,corners,sizeof(corners));memcpy(storage.data()+51,projection,12);};
  reset();originalCorners=storage.data()+1;
  std::array<intptr_t,8> args{30,-37,address(1),address(51),8,address(101),3,address(301)};
  for(int cap:{-3,3}){
   reset();args[6]=cap;assert(CL_RTCWMarkFragments(&vm,args.data())==1&&allocations==0);
   fntech3_rtcw_markFragment_t fragment;memcpy(&fragment,storage.data()+301,8);assert(fragment.firstPoint==0&&fragment.numPoints==(cap<0?3:-3));
   for(int i=0;i<3;++i){fntech3_rtcw_markPoint_t point;memcpy(&point,storage.data()+101+20*i,20);for(int j=0;j<3;++j)assert(point.xyz[j]==i*3+j);
    if(cap>0)assert(point.st[0]==0.25f&&point.st[1]==0.75f);else for(int j=12;j<20;++j)assert(storage[101+20*i+j]==0xa5);}
   assert(storage[100]==0xa5&&storage[161]==0xa5&&storage[300]==0xa5&&storage[309]==0xa5);
  }
  args[6]=3;
  for(fault=1;fault<=7;++fault){reset();const auto saved=storage;Reject([&]{CL_RTCWMarkFragments(&vm,args.data());});assert(storage==saved);}fault=0;
  const auto good=args;
  for(int index:{2,3,5,7}){args=good;args[index]=0;Reject([&]{CL_RTCWMarkFragments(&vm,args.data());});args[index]=address(1020);Reject([&]{CL_RTCWMarkFragments(&vm,args.data());});}
  for(int index:{1,4,6})if(sizeof(intptr_t)>4){args=good;args[index]=static_cast<intptr_t>(INT64_C(0x100000000));Reject([&]{CL_RTCWMarkFragments(&vm,args.data());});}
  for(int cap:{INT_MIN,-1,INT_MAX}){args=good;args[4]=cap;Reject([&]{CL_RTCWMarkFragments(&vm,args.data());});}
  args=good;args[6]=INT_MIN;Reject([&]{CL_RTCWMarkFragments(&vm,args.data());});
  args=good;args[7]=args[5]+20;Reject([&]{CL_RTCWMarkFragments(&vm,args.data());});
  args=good;args[2]=-1;Reject([&]{CL_RTCWMarkFragments(&vm,args.data());});
  args=good;reset();float nan=NAN;memcpy(storage.data()+51,&nan,4);Reject([&]{CL_RTCWMarkFragments(&vm,args.data());});
  args=good;args[4]=0;args[2]=args[3]=args[5]=args[7]=0;const int before=calls;assert(CL_RTCWMarkFragments(&vm,args.data())==0&&calls==before);
 }
}
'''


class RTCWMarksTests(unittest.TestCase):
    def compile_run(self, body):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            flags = ['-m32']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-marks-', dir=ROOT / '.tmp') as tmp:
            source = Path(tmp) / 'probe.cpp'; exe = Path(tmp) / 'probe.exe'
            source.write_text(body)
            built = subprocess.run([compiler, *flags, '-std=c++17', '-I', str(ROOT), str(source), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_world_face_curve_clipping_and_texture_coordinates(self):
        math = (ROOT / 'code/qcommon/q_math.c').read_text()
        signatures = ('float Q_fabs(', 'vec_t VectorNormalize(', 'void ProjectPointOnPlane(', 'void PerpendicularVector(',
                      'void MatrixMultiply(', 'void RotatePointAroundVector(', 'void ClearBounds(',
                      'void AddPointToBounds(', 'int BoxOnPlaneSide(')
        helpers = '\n'.join(function(math, signature) for signature in signatures)
        clipper = function((ROOT / 'code/renderer/tr_marks.c').read_text(), 'static void R_ChopPolyBehindPlane(')
        self.compile_run(GEOMETRY_HOST + helpers + clipper + GEOMETRY_CHECKS)

    def test_native_qvm_spans_and_atomic_publication(self):
        self.compile_run(IMPORT_HOST)


if __name__ == '__main__':
    unittest.main()
