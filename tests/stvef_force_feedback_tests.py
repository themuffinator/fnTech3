"""Exercise the shipping effect table, scheduler, native ABI and input lifecycle."""
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = '#include "tests/stvef_force_feedback_runtime.hpp"\n'


class STVEFForceFeedbackTests(unittest.TestCase):
    def execute(self, source):
        compiler = (shutil.which('clang++') if os.name == 'nt' else shutil.which('g++')) or shutil.which('clang++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-ff-', dir=ROOT / '.tmp') as folder:
            folder = Path(folder)
            cpp = folder / 'feedback.cpp'
            binary = folder / ('feedback.exe' if os.name == 'nt' else 'feedback')
            cpp.write_text(source, encoding='utf-8')
            sanitizer = [] if os.name == 'nt' else ['-fsanitize=undefined,float-cast-overflow', '-fno-sanitize-recover=all']
            built = subprocess.run([compiler, *compiler_target_args(compiler), *sanitizer,
                                    '-std=c++17', '-O1', '-I', str(ROOT), str(cpp), '-o', str(binary)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def fixture(self):
        return json.loads((ROOT / 'tests/fixtures/stvef_force_feedback_retail.json').read_text())

    def test_all_retail_effects_gains_guids_signed_offsets_and_envelopes(self):
        lines = [RUNTIME, 'int main(){ Native native; assert(native.Open(expectedPath.c_str(),instance,window,false,0));']
        for row in self.fixture()['rows']:
            gain = format(row['gain'], '.9g')
            if '.' not in gain and 'e' not in gain:
                gain += '.0'
            lines.append(f'recorded.clear(); assert(native.Build(0,{row["effect"]},{gain}f));')
            lines.append(f'assert(recorded.size()=={len(row["components"])});')
            for i, component in enumerate(row['components']):
                words = ','.join(f'{v}u' for v in component['args'])
                envelope = ','.join(f'{v}u' for v in component['envelope'])
                guid = ','.join(str(v) for v in bytes.fromhex(component['guidBytes']))
                lines.append(f'''{{ const unsigned char guid[]={{ {guid} }};
                  assert(recorded[{i}].kind=={component['kind']});
                  assert(!memcmp(&recorded[{i}].guid,guid,16));
                  assert((recorded[{i}].args==std::vector<std::uint32_t>{{{words}}}));
                  assert((recorded[{i}].envelope==std::vector<std::uint32_t>{{{envelope}}})); }}''')
            lines.append('native.Clear(0); assert(objects.size()==2);')
        for i, name in enumerate(self.fixture()['names']):
            lines.append(f'assert(!strcmp(Effects[{i}].name,"{name}"));')
        lines.append('''
          assert(Magnitude(10000,-1)==0 && Magnitude(10000,std::numeric_limits<float>::quiet_NaN())==0);
          assert(Magnitude(10000,std::numeric_limits<float>::infinity())==0);
          assert(Magnitude(10000,std::numeric_limits<float>::max())==2147483647u);
          assert(Magnitude(8300,.12345f)==1024 && Magnitude(10000,3)==30000);
          assert(Tension(-99)==0 && Tension(0)==0 && Tension(1)==1000 && Tension(2)==5000 && Tension(3)==10000 && Tension(99)==10000);
          native.Close(); native.Close(); assert(objects.empty() && !loaded && loads==unloads);
        }''')
        self.execute('\n'.join(lines))

    def test_scheduler_matches_native_sequence_including_partial_failures(self):
        lines = [RUNTIME, '''int main(){ Native native; Player<Native> player(native);
          assert(native.Open(expectedPath.c_str(),instance,window,false,0)); bool enabled=true;''']
        for row in self.fixture()['steps']:
            lines.append('{ starts=stops=creates=destroys=0;')
            lines.append(f'failStop={int(row["failure"]==1)}; failStart={int(row["failure"]==4)};')
            lines.append(f'failInitAt=initializations+{row["failure"]-1};' if row['failure'] in (2, 3) else 'failInitAt=0;')
            action, effect = row['action'], row['effect']
            lines.append({
                'play': f'if(enabled)player.Play({effect},1);',
                'ensure': f'if(enabled)player.Ensure({effect},1);',
                'stop': f'player.Stop({effect});',
                'stopall': 'player.StopAll();',
                'clear': 'player.Clear();',
                'disable': 'enabled=false;', 'enable': 'enabled=true;',
            }[action])
            for field, variable in [('starts', 'starts'), ('stops', 'stops'), ('created', 'creates'), ('destroyed', 'destroys')]:
                lines.append(f'assert({variable}=={row[field]});')
            for i in range(4):
                lines.append(f'assert(player.EffectInSlot({i})=={row["ids"][i]} && native.Playing({i})=={row["playing"][i]});')
            lines.append('}')
        lines.append('native.Close(); assert(objects.empty() && !loaded); }')
        self.execute('\n'.join(lines))

    def test_runtime_missing_exports_devices_spring_and_cleanup(self):
        self.execute(RUNTIME + r'''
int main(){
 Native native;
 assert(!native.Open(nullptr,instance,window,true,1) && !loads);
 assert(!native.Open(expectedPath.c_str(),nullptr,window,true,1) && !loads);
 assert(!native.Build(-1,0,1) && !native.Build(4,0,1) && !native.Start(0) && !native.Stop(0) && !native.Playing(0));
 failLoad=true; assert(!native.Open(expectedPath.c_str(),instance,window,true,1) && !native.Available()); failLoad=false;
 assert(native.Open(expectedPath.c_str(),instance,window,true,1) && !native.IsMouse() && native.HasSpring());
 assert(mouseInits==1 && joyInits==1 && springStarts==1 && lastSpring==1000);
 auto names=exports; assert(names.size()==21); native.Close();
 int attempts=mouseInits+joyInits;
 for(const auto &name:names){
  missing=name; assert(!native.Open(expectedPath.c_str(),instance,window,true,1));
  assert(!native.Available() && objects.empty() && !loaded && mouseInits+joyInits==attempts);
  native.Close();
 }
 missing.clear();joySuccess=false;
 assert(!native.Open(expectedPath.c_str(),instance,window,true,1) && !loaded && objects.empty());joySuccess=true;
 mouseSuccess=true;int before=joyInits;
 assert(native.Open(expectedPath.c_str(),instance,window,true,1) && native.IsMouse() && !native.HasSpring());
 assert(mouseServices==1 && joyInits==before && !native.Spring(1000));native.Close();
 mouseSuccess=false;springSuccess=false;
 assert(native.Open(expectedPath.c_str(),instance,window,false,1) && native.Available() && !native.HasSpring());
 assert(objects.size()==1 && native.Build(0,0,3));native.Close();springSuccess=true;
 failSpringStart=true;
 assert(native.Open(expectedPath.c_str(),instance,window,false,2));
 before=changes;assert(!native.Spring(5000) && changes==before);failSpringStart=false;
 assert(native.Spring(5000) && lastSpring==5000);
 before=springStarts;assert(native.Spring(7000) && lastSpring==7000 && springStarts==before);
 failChange=true;assert(!native.Spring(8000));failChange=false;
 failSpringStop=true;assert(!native.Spring(0));failSpringStop=false;assert(native.Spring(0));
 before=springStops;assert(native.Spring(0) && springStops==before);
 assert(native.Spring(100000) && lastSpring==10000);assert(native.Spring(-1));
 assert(!native.Build(-1,0,1) && !native.Build(4,0,1) && !native.Build(0,-1,1) && !native.Build(0,32,1));
 assert(!native.Playing(-1) && !native.Start(4) && !native.Stop(-1));native.Clear(4);native.Clear(-1);
 assert(native.Build(0,0,1));failStart=true;before=starts;
 assert(!native.Start(0) && starts==before+3 && !native.Playing(0));failStart=false;assert(native.Start(0));
 failStatus=true;assert(!native.Playing(0));failStatus=false;assert(native.Playing(0));
 failStop=true;before=stops;assert(!native.Stop(0) && stops==before+3 && native.Playing(0));
 native.Close();failStop=false;assert(objects.empty() && !loaded && !native.Available());
 assert(!native.Start(0) && !native.Spring(1) && !native.Build(0,0,1));native.Close();
 assert(loads==unloads+1 && creates==destroys);
}
''')

    def test_controller_opt_in_profile_focus_restart_commands_and_shutdown(self):
        self.execute(RUNTIME + r'''
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
using qboolean=int;
constexpr int qtrue=1,qfalse=0,CVAR_ARCHIVE=1,CV_INTEGER=1;
struct cvar_t { int integer=0; float value=0; bool modified=false; std::string text; };
static std::map<std::string,cvar_t> cvars;
static std::map<std::string,void(*)()> commands;
static std::vector<std::string> args;
static int profile, warnings;
static bool Com_GameIsSTVEFSP(){return profile==5;}
static void Cvar_Set(const char *name,const char *text){
 auto &v=cvars[name];v.text=text;v.value=strtof(text,nullptr);v.integer=std::isfinite(v.value)&&fabs(v.value)<1e8f?int(v.value):0;v.modified=true;
}
static cvar_t *Cvar_Get(const char *name,const char *text,int){if(!cvars.count(name))Cvar_Set(name,text);return &cvars[name];}
static const char *Cvar_VariableString(const char *name){return cvars[name].text.c_str();}
static void Cvar_CheckRange(cvar_t *,const char *,const char *,int){}
static void Cvar_SetDescription(cvar_t *,const char *){}
static void Cmd_AddCommand(const char *name,void(*fn)()){assert(!commands.count(name));commands[name]=fn;}
static void Cmd_RemoveCommand(const char *name){commands.erase(name);}
static int Cmd_Argc(){return int(args.size());}
static const char *Cmd_Argv(int i){return args.at(i).c_str();}
static int Q_stricmp(const char *a,const char *b){return std::strcmp(a,b);}
static const char *va(const char *format,int value){static char text[64];snprintf(text,sizeof(text),format,value);return text;}
static void Com_Printf(const char *,...){++warnings;}
#include "code/client/client_stvef_sp_feedback.hpp"
static void Command(std::initializer_list<const char *> words){args.clear();for(auto w:words)args.push_back(w);commands.at(args[0])();}
int main(){
 for(profile=0;profile<7;++profile)if(profile!=5){
  CL_STVEFSPFeedbackInit(instance,window);CL_STVEFSPFeedbackFrame(1);CL_STVEFSPFeedbackPlay(0,0);
  CL_STVEFSPFeedbackStop(0);CL_STVEFSPFeedbackStopAll();CL_STVEFSPFeedbackShutdown();
  assert(!loads && cvars.empty() && commands.empty());
 }
 profile=5;Cvar_Set("fs_basepath","installation");
 expectedPath=std::filesystem::absolute("installation/FFC10.dll").lexically_normal().u8string();
 CL_STVEFSPFeedbackInit(instance,window);assert(commands.size()==5 && !loads);
 assert(cvars["use_ff"].integer==0 && cvars["js_ffmult"].value==3 && cvars["ff_defaultTension"].integer==1);
 CL_STVEFSPFeedbackFrame(1);CL_STVEFSPFeedbackPlay(0,0);assert(!loads);
 Command({"ff_usejoy"});CL_STVEFSPFeedbackFrame(1);assert(!loads);
 Cvar_Set("use_ff","1");CL_STVEFSPFeedbackFrame(0);CL_STVEFSPFeedbackPlay(0,0);assert(!loads);
 failLoad=true;CL_STVEFSPFeedbackFrame(1);int before=loads;
 for(int i=0;i<10;++i)CL_STVEFSPFeedbackFrame(1);assert(loads==before);failLoad=false;
 CL_STVEFSPFeedbackSuspend();CL_STVEFSPFeedbackFrame(1);assert(loaded && joyInits==1 && !mouseInits);
 CL_STVEFSPFeedbackPlay(0,0);assert(stvefFeedback.native.Playing(0));
 before=starts;int changed=changes;CL_STVEFSPFeedbackPlay(0,1);assert(starts==before && changes==changed);
 CL_STVEFSPFeedbackStopAll();assert(!stvefFeedback.native.Playing(0));
 CL_STVEFSPFeedbackPlay(0,1);assert(starts==before+3 && changes==changed);
 CL_STVEFSPFeedbackStop(0);CL_STVEFSPFeedbackPlay(8,1);assert(lastSpring==1000 && changes==changed+1);
 before=starts;
 for(const char *bad:{"", "garbage", "1.2", "32", "21474836480", "-1"})Command({"ff_play",bad});
 assert(starts==before);
 Command({"ff_play","name","Shotgun"});assert(starts==before+2);
 Command({"ff_tension","2"});assert(lastSpring==5000 && cvars["ff_defaultTension"].integer==2);
 Command({"ff_spring","7000"});assert(lastSpring==7000);
 CL_STVEFSPFeedbackPlay(31,0);assert(lastSpring==5000);
 CL_STVEFSPFeedbackFrame(0);assert(!loaded && objects.empty());before=starts;
 CL_STVEFSPFeedbackPlay(0,0);Command({"ff_play","0"});assert(starts==before && !loaded);
 CL_STVEFSPFeedbackFrame(1);assert(loaded && stvefFeedback.player.EffectInSlot(0)==Empty);
 Cvar_Set("use_ff","0");CL_STVEFSPFeedbackPlay(0,0);assert(starts==before);
 CL_STVEFSPFeedbackFrame(1);assert(!loaded && objects.empty());
 mouseSuccess=true;Cvar_Set("use_ff","1");Command({"ff_usemouse"});CL_STVEFSPFeedbackFrame(1);
 assert(loaded && stvefFeedback.native.IsMouse());CL_STVEFSPFeedbackShutdown();
 assert(!loaded && objects.empty() && commands.empty());
 window=reinterpret_cast<void *>(0x333);CL_STVEFSPFeedbackInit(instance,window);CL_STVEFSPFeedbackFrame(1);
 assert(loaded);CL_STVEFSPFeedbackShutdown();CL_STVEFSPFeedbackShutdown();assert(!loaded && objects.empty());
}
''')

    def test_unsupported_platform_never_loads_or_drives_a_device(self):
        self.execute(r'''
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#undef _WIN32
#undef _M_IX86
#undef __i386__
#include "code/client/stvef_force_feedback_native.hpp"
int main(){using namespace fntech3::stvef::feedback;Native native;Player<Native> player(native);
 assert(!native.Open("FFC10.dll",(void*)1,(void*)2,true,1));
 assert(std::string(native.Error()).find("Windows x86")!=std::string::npos);
 player.Play(0,1);player.Ensure(0,1);player.Stop(0);player.StopAll();player.Clear();
 assert(!native.Available() && !native.Build(0,0,1) && !native.Playing(0) && !native.Spring(1));
 native.Close();native.Close();
}
''')


if __name__ == '__main__':
    unittest.main()
