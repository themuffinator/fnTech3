"""Run Academy's production guard and canonical renderer feature dispatch."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from render_record_adapter_tests import PRELUDE, function

ROOT = Path(__file__).resolve().parents[1]


def fixture():
    host = (ROOT / 'code/client/client_jka_cgame_host.hpp').read_text()
    shared = (ROOT / 'code/client/cl_cgame.cpp').read_text()
    prelude = PRELUDE.replace('#include "code/qcommon/qcommon.h"',
        '#include "code/qcommon/qcommon.h"\n#include "code/qcommon/vm_local.h"\n#include <limits>')
    prelude = prelude.replace('#include "code/cgame/cg_public.h"',
        '#include "code/cgame/cg_public.h"\n#include "code/client/client_jka_cgame.hpp"')
    prelude = prelude.replace('static vm_t *cgvm = reinterpret_cast<vm_t *>(1), *uivm = reinterpret_cast<vm_t *>(2);',
        'static vm_t nativeVM{}; static vm_t *cgvm=&nativeVM, *uivm=&nativeVM;')
    start = host.index('    using namespace fntech3::jka;', host.index('static bool CL_JKACgameCall('))
    end = host.index('    const auto call =', start)
    first = host.index('    case Call::CG_R_ADDREFENTITYTOSCENE:')
    # This scene-only renderer refuses an unexpected normal/additive light.
    # Their bounded transfer checks execute in jka_cgame_host_tests.
    prelude = prelude.replace(
        'static struct { decltype(&AddEntity) AddRefEntityToScene; decltype(&RenderScene) RenderScene; } re{AddEntity, RenderScene};',
        'static void UnexpectedLight(const vec3_t,float,float,float,float){throw 9;} '
        'static struct { decltype(&AddEntity) AddRefEntityToScene; decltype(&RenderScene) RenderScene; '
        'decltype(&UnexpectedLight) AddLightToScene; decltype(&UnexpectedLight) AddAdditiveLightToScene; '
        '} re{AddEntity, RenderScene, UnexpectedLight, UnexpectedLight};')
    # The tag query's actual three-backend bodies run in jka_cgame_tag_tests.
    last = host.index('    case Call::CG_R_LERPTAG:', first)
    cases = []
    for begin, finish in [('R_ADDREFENTITYTOSCENE', 'R_ADDPOLYTOSCENE'), ('R_RENDERSCENE', 'R_SETCOLOR')]:
        a = shared.index(f'\tcase CG_{begin}:')
        b = shared.index(f'\tcase CG_{finish}:', a)
        cases.append(shared[a:b])
    return prelude + function(host, 'static void *CL_JKACgameSpan(') + r'''
using Call=fntech3::jka::module::Call;
// This static-only backend has no owned model. The actual owned queue and
// scene flush are exercised by jka_cgame_render_tests with real Store data.
static void CL_JKACgameRenderQueue(const fntech3_jka_mp_refEntity_t &){Com_Error(ERR_DROP,"No owned model in static-render fixture");}
static bool JKA(Call call,intptr_t *args,intptr_t &operation,intptr_t &result) {
''' + host[start:end] + 'switch(call){\n' + host[first:last] + '\ndefault: throw 9;\n}\n}\n' + r'''
static intptr_t Cgame(intptr_t operation,intptr_t *args){switch(operation){
''' + '\n'.join(cases) + r'''
default:throw 9;}}
static intptr_t UI(intptr_t,intptr_t *){throw 9;}
template<class T> static void submit(Call call,const T &source){
 auto bytes=Unaligned(source);allowedAddress=reinterpret_cast<uintptr_t>(bytes.get()+1);allowedBytes=sizeof(T);
 auto copy=std::make_unique<byte[]>(sizeof(T));std::memcpy(copy.get(),bytes.get()+1,sizeof(T));
 intptr_t args[]{0,static_cast<intptr_t>(allowedAddress)},operation=-1,result=-1;
 assert(!JKA(call,args,operation,result)&&result==0);
 Cgame(operation,args);assert(bytes[0]==0xe5&&!std::memcmp(copy.get(),bytes.get()+1,sizeof(T)));
}
int main(){
 profile.id=COM_GAME_JKA_MP;nativeVM.entryPoint=reinterpret_cast<decltype(nativeVM.entryPoint)>(1);
 fntech3_jka_mp_refEntity_t entity{};entity.hModel=19;entity.shaderTime=1.25f;
 submit(Call::CG_R_ADDREFENTITYTOSCENE,entity);
 assert(entityCalls==1&&capturedEntity.hModel==19&&capturedEntity.shaderTime.f==1.25f&&!capturedIntegerTime&&checkedBytes==212);
 fntech3_jka_mp_refdef_t view{};view.width=640;view.height=480;view.fov_x=90;view.fov_y=75;
 submit(Call::CG_R_RENDERSCENE,view);
 assert(sceneCalls==1&&liquidCalls==1&&underwaterCalls==1&&flushCalls==1&&checkedBytes==384);
 capture.integer=hideWeapon.integer=1;submit(Call::CG_R_RENDERSCENE,view);
 assert(sceneCalls==2&&(capturedView.rdflags&RDF_NOFIRSTPERSON));
 hideHud=true;view.rdflags=1;cl_enemyHighlightNumPending=4;submit(Call::CG_R_RENDERSCENE,view);
 assert(sceneCalls==2&&!cl_enemyHighlightNumPending&&liquidCalls==2&&underwaterCalls==2&&flushCalls==2);
 hideHud=false;view.rdflags=0;
 entity.ghoul2=0x12345678;Reject([&]{submit(Call::CG_R_ADDREFENTITYTOSCENE,entity);});entity.ghoul2=0;
 entity.renderfx=0x10;Reject([&]{submit(Call::CG_R_ADDREFENTITYTOSCENE,entity);});entity.renderfx=0;
 view.rdflags=8;Reject([&]{submit(Call::CG_R_RENDERSCENE,view);});view.rdflags=0;
 boundsFailure=true;Reject([&]{submit(Call::CG_R_ADDREFENTITYTOSCENE,entity);});boundsFailure=false;
 nativeVM.entryPoint=nullptr;Reject([&]{submit(Call::CG_R_ADDREFENTITYTOSCENE,entity);});nativeVM.entryPoint=reinterpret_cast<decltype(nativeVM.entryPoint)>(1);
 nativeVM.dllExports=reinterpret_cast<void *>(1);Reject([&]{submit(Call::CG_R_RENDERSCENE,view);});nativeVM.dllExports=nullptr;
 auto bytes=Unaligned(entity);allowedAddress=reinterpret_cast<uintptr_t>(bytes.get()+1);allowedBytes=211;
 intptr_t args[]{0,static_cast<intptr_t>(allowedAddress)},operation=-1,result=-1;
 Reject([&]{JKA(Call::CG_R_ADDREFENTITYTOSCENE,args,operation,result);});
 args[1]=0;Reject([&]{JKA(Call::CG_R_ADDREFENTITYTOSCENE,args,operation,result);});
 args[1]=-1;Reject([&]{JKA(Call::CG_R_ADDREFENTITYTOSCENE,args,operation,result);});
}
'''


def ui_fixture():
    host = (ROOT / 'code/client/client_jka_ui_host.hpp').read_text()
    shared = (ROOT / 'code/client/cl_ui.cpp').read_text()
    prelude = PRELUDE.replace('#include "code/qcommon/qcommon.h"',
        '#include "code/qcommon/qcommon.h"\n#include "code/qcommon/vm_local.h"\n#include <limits>')
    prelude = prelude.replace('#include "code/ui/ui_public.h"',
        '#include "code/ui/ui_public.h"\n#include "code/client/client_jka_ui.hpp"')
    prelude = prelude.replace('static vm_t *cgvm = reinterpret_cast<vm_t *>(1), *uivm = reinterpret_cast<vm_t *>(2);',
        'static vm_t nativeVM{}; static vm_t *cgvm=&nativeVM, *uivm=&nativeVM;')
    prelude = prelude.replace('void QDECL Com_Error(errorParm_t, const char *, ...) { throw 7; }',
        'static char lastError[1024]; void QDECL Com_Error(errorParm_t, const char *message, ...) { '
        'va_list args; va_start(args,message); std::vsnprintf(lastError,sizeof(lastError),message,args); va_end(args); throw 7; }')
    prelude = prelude.replace('static void CL_UIAdjustRefdef(refdef_t *) {}',
        'static cvar_t menuAspect{};static cvar_t *cl_menuAspect=&menuAspect;'
        'static struct { struct {int vidWidth,vidHeight;} glconfig; float scale,biasX;} cls;')
    # The extracted UI dispatch also contains the point-light case; its
    # transfer/finite checks execute in jka_ui_render_tests. Keep this fixture's
    # scene-only renderer explicit so an unexpected light call fails loudly.
    prelude = prelude.replace(
        'static struct { decltype(&AddEntity) AddRefEntityToScene; decltype(&RenderScene) RenderScene; } re{AddEntity, RenderScene};',
        'static void UnexpectedLight(const vec3_t,float,float,float,float){throw 9;} '
        'static struct { decltype(&AddEntity) AddRefEntityToScene; decltype(&RenderScene) RenderScene; '
        'decltype(&UnexpectedLight) AddLightToScene; } re{AddEntity, RenderScene, UnexpectedLight};')
    first = host.index('\tcase Call::UI_R_ADDREFENTITYTOSCENE:')
    last = host.index('\tcase Call::UI_KEY_GETBINDINGBUF:', first)
    cases = []
    for begin, finish in [('R_ADDREFENTITYTOSCENE', 'R_ADDPOLYTOSCENE'), ('R_RENDERSCENE', 'R_SETCOLOR')]:
        a = shared.index(f'\tcase UI_{begin}:')
        b = shared.index(f'\tcase UI_{finish}:', a)
        cases.append(shared[a:b])
    return prelude + function(shared, 'static void CL_UIAdjustStretchPic(') + '\n' + \
        function(shared, 'static void CL_UIAdjustRefdef(') + '\n' + \
        function(host, 'static void *CL_JKAUINativeSpan(') + r'''
using Call=fntech3::jka::module::Call;
static void CL_JKAUIRenderQueue(const fntech3_jka_mp_refEntity_t &){Com_Error(ERR_DROP,"Ghoul2 is checked by its dedicated production host");}
static void CL_JKAUIRenderFlush(int){}
static bool JKAUI(Call call,intptr_t *args,intptr_t &operation,intptr_t &result) {
 using namespace fntech3::jka;result=0;switch(call){
''' + host[first:last] + '\ndefault: throw 9;\n}\n}\n' + r'''
static intptr_t UI(intptr_t operation,intptr_t *args){switch(operation){
''' + '\n'.join(cases) + r'''
default:throw 9;}}
static intptr_t Cgame(intptr_t,intptr_t *){throw 9;}
template<class T> static void submitUI(Call call,const T &source){
 auto bytes=Unaligned(source);allowedAddress=reinterpret_cast<uintptr_t>(bytes.get()+1);allowedBytes=sizeof(T);
 auto copy=std::make_unique<byte[]>(sizeof(T));std::memcpy(copy.get(),bytes.get()+1,sizeof(T));
 intptr_t args[]{0,static_cast<intptr_t>(allowedAddress)},operation=-1,result=-1;
 assert(!JKAUI(call,args,operation,result)&&result==0);
 UI(operation,args);assert(bytes[0]==0xe5&&!std::memcmp(copy.get(),bytes.get()+1,sizeof(T)));
}
int main(){
 profile.id=COM_GAME_JKA_MP;nativeVM.entryPoint=reinterpret_cast<decltype(nativeVM.entryPoint)>(1);
 assert(fntech3::jka::ui::SharedImport(23)==UI_R_ADDREFENTITYTOSCENE);
 assert(fntech3::jka::ui::SharedImport(26)==UI_R_RENDERSCENE);
 fntech3_jka_mp_refEntity_t entity{};entity.hModel=19;entity.shaderTime=1.25f;
 submitUI(Call::UI_R_ADDREFENTITYTOSCENE,entity);
 assert(entityCalls==1&&capturedEntity.hModel==19&&capturedEntity.shaderTime.f==1.25f&&!capturedIntegerTime&&checkedBytes==212);
 cls.glconfig.vidWidth=1600;cls.glconfig.vidHeight=900;cls.scale=1.875f;cls.biasX=200;
 fntech3_jka_mp_refdef_t view{};view.x=400;view.y=200;view.width=300;view.height=200;view.fov_x=90;view.fov_y=75;view.rdflags=1;
 menuAspect.integer=1;submitUI(Call::UI_R_RENDERSCENE,view);
 assert(sceneCalls==1&&capturedView.x==400&&capturedView.y==200&&capturedView.width==300&&capturedView.height==200&&checkedBytes==384);
 assert(capturedView.fov_x==90&&capturedView.fov_y==75&&capturedView.rdflags==(RDF_NOWORLDMODEL|RDF_NOFOVCORRECTION));
 menuAspect.integer=0;submitUI(Call::UI_R_RENDERSCENE,view);
 assert(sceneCalls==2&&capturedView.x==267&&capturedView.y==200&&capturedView.width==400&&capturedView.height==200);
 assert(!liquidCalls&&!underwaterCalls&&!flushCalls); // UI follows its menu pipeline
 entity.ghoul2=0x12345678;Reject([&]{submitUI(Call::UI_R_ADDREFENTITYTOSCENE,entity);});entity.ghoul2=0;
 assert(std::strstr(lastError,"dedicated production host"));
 entity.renderfx=0x10;Reject([&]{submitUI(Call::UI_R_ADDREFENTITYTOSCENE,entity);});entity.renderfx=0;
 assert(std::strstr(lastError,"unsupported Raven renderer entity flags"));
 view.rdflags=8;Reject([&]{submitUI(Call::UI_R_RENDERSCENE,view);});view.rdflags=1;
 assert(std::strstr(lastError,"unsupported Raven renderer view flags"));
 boundsFailure=true;Reject([&]{submitUI(Call::UI_R_ADDREFENTITYTOSCENE,entity);});boundsFailure=false;
 nativeVM.entryPoint=nullptr;Reject([&]{submitUI(Call::UI_R_ADDREFENTITYTOSCENE,entity);});nativeVM.entryPoint=reinterpret_cast<decltype(nativeVM.entryPoint)>(1);
 nativeVM.dllExports=reinterpret_cast<void *>(1);Reject([&]{submitUI(Call::UI_R_RENDERSCENE,view);});nativeVM.dllExports=nullptr;
 assert(std::strstr(lastError,"requires its legacy native module"));
 auto bytes=Unaligned(entity);allowedAddress=reinterpret_cast<uintptr_t>(bytes.get()+1);allowedBytes=211;
 intptr_t args[]{0,static_cast<intptr_t>(allowedAddress)},operation=-1,result=-1;
 Reject([&]{JKAUI(Call::UI_R_ADDREFENTITYTOSCENE,args,operation,result);});
 args[1]=0;Reject([&]{JKAUI(Call::UI_R_ADDREFENTITYTOSCENE,args,operation,result);});
 args[1]=-1;Reject([&]{JKAUI(Call::UI_R_ADDREFENTITYTOSCENE,args,operation,result);});
 assert(std::strstr(lastError,"Invalid Jedi Academy UI record address"));
}
'''


class JKARenderHostTests(unittest.TestCase):
    def test_actual_native_guard_and_canonical_scene_features(self):
        self.compile_and_run(fixture())

    def test_actual_ui_record_guard_and_menu_viewport_features(self):
        self.compile_and_run(ui_fixture())

    def compile_and_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        parent = ROOT / '.tmp/tests/jka-render-host'; parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary).resolve()
            cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-ffast-math', '-Werror',
                '-I', str(ROOT), str(cpp), '-o', str(exe)], cwd=folder, text=True,
                capture_output=True, timeout=90)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], cwd=folder, text=True, capture_output=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertTrue(folder.is_relative_to(parent.resolve()))
            self.assertFalse(any(p.is_symlink() or p.stat().st_file_attributes & 0x400
                for p in folder.rglob('*')) if os.name == 'nt' else any(p.is_symlink() for p in folder.rglob('*')))


if __name__ == '__main__':
    unittest.main()
