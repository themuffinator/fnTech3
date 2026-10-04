"""Measured cgame tag boundary with each actual Fn renderer's MD3 query."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from jka_cgame_ghoul2_tests import fixture as ownership_fixture
from rtcw_music_fixture import function

ROOT=Path(__file__).resolve().parents[1]


def fixture(backend):
    prefix=ownership_fixture().split('int main(){')[0]
    old='''struct FixtureRenderer {qhandle_t (*RegisterSkin)(const char *);};
static FixtureRenderer re{RegisterSkin};'''
    assert prefix.count(old)==1
    prefix=prefix.replace(old,r'''
#include "code/renderercommon/tr_types.h"
#include "code/qcommon/qfiles.h"
static int Tag(orientation_t *,qhandle_t,int,int,float,const char *);
struct FixtureRenderer {qhandle_t (*RegisterSkin)(const char *);decltype(&Tag) LerpTag;};
static FixtureRenderer re{RegisterSkin,Tag};
''')
    source=(ROOT/'code'/backend/'tr_model.c').read_text()
    host=(ROOT/'code/client/client_jka_cgame_host.hpp').read_text()
    start=host.index('    case Call::CG_R_LERPTAG:')
    end=host.index('    case Call::CG_LANGUAGE_ISASIAN:',start)
    return prefix+r'''
#include "code/compat/jka_render_adapter.hpp"
enum {MOD_MDS=1,MOD_MDC,MOD_MDR,MOD_IQM};
struct FixtureMDS{struct {unsigned numFrames;} view;};
struct model_t{md3Header_t *md3[1];int type;FixtureMDS *mds;void *mdc[1];void *modelData;};
static model_t tagModel,missingModel;
static model_t *R_GetModelByHandle(qhandle_t handle){return handle==7?&tagModel:&missingModel;}
static int R_MdsTag(FixtureMDS *,const refEntity_t *,int,const char *,int,orientation_t *){assert(false);return -1;}
static md3Tag_t *R_GetMDCTag(void *,int,const char *,md3Tag_t *){assert(false);return nullptr;}
static int R_IQMLerpTag(orientation_t *,void *,int,int,float,const char *){assert(false);return 0;}
void AxisClear(vec3_t *axes){std::memset(axes,0,9*sizeof(float));for(int n=0;n<3;++n)axes[n][n]=1;}
vec_t VectorNormalize(vec3_t vector){
 const float length=std::sqrt(vector[0]*vector[0]+vector[1]*vector[1]+vector[2]*vector[2]);
 if(length)for(int n=0;n<3;++n)vector[n]/=length;return length;
}
void Q_strncpyz(char *target,const char *source,int length){assert(length>0);std::strncpy(target,source,length-1);target[length-1]=0;}
'''+function(source,'static md3Tag_t *R_GetTag(')+'\n'+function(source,'static md3Tag_t *R_GetAnimTag(')+'\n'+function(source,'int R_LerpTag(')+r'''
static int queries,badResult;
static int Tag(orientation_t *out,qhandle_t handle,int from,int to,float fraction,const char *name){
 ++queries;const int found=R_LerpTag(out,handle,from,to,fraction,name);
 if(badResult==1){const std::uint32_t nan=0x7fc00001;std::memcpy(&out->axis[2][1],&nan,4);}
 if(badResult==2)return 2;return found;
}
static intptr_t Dispatch(intptr_t *args){
 using namespace fntech3::jka;using module::Call;intptr_t result=-1;
 const auto call=module::Lookup(COM_GAME_JKA_MP,module::Module::CGAME,module::Direction::IMPORT,args[0]);
 switch(call){
'''+host[start:end].replace('return true;','return result;')+r'''
 default:assert(false);return -1;}}
static bool Near(float a,float b){return std::fabs(a-b)<.00001f;}
int main(){
 vm_t vm{};vm.index=VM_CGAME;vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 if(sizeof(void*)!=4)return 0;
 static_assert(sizeof(fntech3_jka_mp_orientation_t)==48);
 static_assert(offsetof(fntech3_jka_mp_orientation_t,axis)==12);
 alignas(md3Header_t) std::array<byte,sizeof(md3Header_t)+2*sizeof(md3Tag_t)> bytes{};
 tagModel.md3[0]=reinterpret_cast<md3Header_t *>(bytes.data());
 auto &header=*tagModel.md3[0];header.numFrames=2;header.numTags=1;header.ofsTags=sizeof(header);
 auto *tags=reinterpret_cast<md3Tag_t *>(bytes.data()+sizeof(header));
 for(int n=0;n<2;++n){std::strcpy(tags[n].name,"tag_test");
  tags[n].origin[0]=n?12:4;tags[n].origin[1]=n?6:2;tags[n].origin[2]=n?-10:-2;
  for(int axis=0;axis<3;++axis)tags[n].axis[axis][axis]=n?4:2;
 }
 std::array<byte,50> output;output.fill(0xa5);
 intptr_t args[]{213,reinterpret_cast<intptr_t>(output.data()+1),7,0,1,0,reinterpret_cast<intptr_t>("tag_test")};
 auto read=[&]{fntech3_jka_mp_orientation_t value;std::memcpy(&value,output.data()+1,sizeof(value));return value;};
 for(float fraction:{.25f,-1.0f,2.0f}){
  std::uint32_t bits;std::memcpy(&bits,&fraction,4);args[5]=bits;assert(Dispatch(args)==1);
  auto value=read();assert(Near(value.origin[0],4+8*fraction)&&Near(value.origin[1],2+4*fraction));
  assert(Near(value.origin[2],-2-8*fraction));
  const float diagonal=(2+2*fraction)==0?0:1;
  for(int row=0;row<3;++row)for(int col=0;col<3;++col)assert(Near(value.axis[row][col],row==col?diagonal:0));
  assert(output.front()==0xa5&&output.back()==0xa5);
 }
 // High frame indices clamp to the final frame in the actual Fn query.
 args[3]=args[4]=INT_MAX;assert(Dispatch(args)==1&&Near(read().origin[0],12));args[3]=0;args[4]=1;
 for(int kind=0;kind<3;++kind){
  args[2]=kind?7:0;args[6]=reinterpret_cast<intptr_t>(kind==2?"TAG_TEST":"missing");
  assert(Dispatch(args)==0);auto value=read();
  for(float component:value.origin)assert(component==0);
  for(int row=0;row<3;++row)for(int col=0;col<3;++col)assert(value.axis[row][col]==(row==col?1:0));
 }
 args[2]=7;args[6]=reinterpret_cast<intptr_t>("tag_test");
 const auto valid=args[5];const auto published=output;const int calls=queries;
 for(int scalar:{2,3,4}){const auto saved=args[scalar];args[scalar]=-1;Reject([&]{Dispatch(args);});args[scalar]=saved;assert(output==published);}
 for(std::uint32_t bits:{0x7fc00001u,0x7f800000u,0xff800000u}){args[5]=bits;Reject([&]{Dispatch(args);});assert(output==published);}args[5]=valid;
 for(int bad:{1,6}){const auto saved=args[bad];args[bad]=0;Reject([&]{Dispatch(args);});args[bad]=saved;assert(output==published);}
 args[1]=static_cast<intptr_t>(UINTPTR_MAX-20);Reject([&]{Dispatch(args);});args[1]=reinterpret_cast<intptr_t>(output.data()+1);
 std::array<char,MAX_QPATH> unterminated;unterminated.fill('a');args[6]=reinterpret_cast<intptr_t>(unterminated.data());
 Reject([&]{Dispatch(args);});args[6]=reinterpret_cast<intptr_t>("tag_test");
 vm.index=VM_UI;Reject([&]{Dispatch(args);});vm.index=VM_CGAME;
 vm.entryPoint=nullptr;Reject([&]{Dispatch(args);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{Dispatch(args);});vm.dllExports=nullptr;
 const auto query=re.LerpTag;re.LerpTag=nullptr;Reject([&]{Dispatch(args);});re.LerpTag=query;
 assert(queries==calls&&output==published);
 for(int invalid:{1,2}){badResult=invalid;Reject([&]{Dispatch(args);});assert(output==published);}badResult=0;
 assert(Dispatch(args)==1&&output.front()==0xa5&&output.back()==0xa5);
}
'''


class JKACgameTagTests(unittest.TestCase):
    def test_measured_tag_boundary_and_three_fn_md3_queries(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        scratch=ROOT/'.tmp/tests/jka-cgame-tag';scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory=Path(folder)
            for backend in ('renderer','renderervk','rendererrtx'):
                source=directory/(backend+'.cpp');source.write_text(fixture(backend))
                for fast in (False,True):
                    with self.subTest(backend=backend,fast_math=fast):
                        executable=directory/(backend+('fast' if fast else 'plain')+('.exe' if os.name=='nt' else ''))
                        built=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-O2','-Werror',
                            '-D_CRT_SECURE_NO_WARNINGS',*(['-ffast-math'] if fast else []),'-I',str(ROOT),str(source),'-o',str(executable)],
                            cwd=ROOT,capture_output=True,text=True,timeout=120)
                        self.assertEqual(built.returncode,0,built.stdout+built.stderr)
                        run=subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=30)
                        self.assertEqual(run.returncode,0,run.stdout+run.stderr)


if __name__=='__main__':unittest.main()
