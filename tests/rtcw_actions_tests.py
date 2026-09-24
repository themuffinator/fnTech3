"""Execute real elementary actions and all RTCW native/QVM action imports."""
from pathlib import Path
import unittest
import rtcw_botlib_tests as bootstrap

ROOT = Path(__file__).resolve().parents[1]


def action_source():
    source = bootstrap.PRELUDE[:bootstrap.PRELUDE.index('static int Export_BotLibSetup();')]
    source = source.replace('#include "code/botlib/be_interface.h"', '')
    catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text()
    source += catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')]
    source += r'''
#include <cstdarg>
#include <climits>
botlib_import_t botimport;
static botlib_export_t exports{};
botlib_export_t *botlib_export=&exports;
static int commandClient;
static std::string command;
void commandCallback(int client,const char *text){commandClient=client;command=text;}
const char *QDECL va(const char *format,...){
 static char text[4096];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);return text;
}
#include "code/botlib/be_ea.cpp"
botlib_globals_t botlibglobals;
#include "code/server/server_rtcw_actions.hpp"
using Call=fntech3::rtcw::module::Call;
using namespace fntech3::rtcw::module;
template<class F> void rejected(F f){try{f();assert(false);}catch(Drop&) {}}
static int floatBits(float value){int bits;std::memcpy(&bits,&value,4);return bits;}
static void setup(){
 botimport.BotClientCommand=commandCallback;
 auto &ea=exports.ea;
 ea.EA_Command=EA_Command;ea.EA_Say=EA_Say;ea.EA_SayTeam=EA_SayTeam;ea.EA_Action=EA_Action;
 ea.EA_Gesture=EA_Gesture;ea.EA_Talk=EA_Talk;ea.EA_Attack=EA_Attack;ea.EA_Use=EA_Use;ea.EA_Respawn=EA_Respawn;
 ea.EA_MoveUp=EA_MoveUp;ea.EA_MoveDown=EA_MoveDown;ea.EA_MoveForward=EA_MoveForward;ea.EA_MoveBack=EA_MoveBack;
 ea.EA_MoveLeft=EA_MoveLeft;ea.EA_MoveRight=EA_MoveRight;ea.EA_Crouch=EA_Crouch;ea.EA_SelectWeapon=EA_SelectWeapon;
 ea.EA_Jump=EA_Jump;ea.EA_DelayedJump=EA_DelayedJump;ea.EA_Move=EA_Move;ea.EA_View=EA_View;
 ea.EA_EndRegular=EA_EndRegular;ea.EA_GetInput=EA_GetInput;ea.EA_ResetInput=EA_ResetInput;
}
'''
    return source


CHECKS = r'''
static void checkBoundary(bool native){
 for(const auto &p:profiles){profile=&p;const bool rtcw=Com_GameIsRTCW();
  if(!p.bots.navigationWorlds){
   assert(p.id==COM_GAME_QUAKE3_IHV||p.id==COM_GAME_JKA_SP||p.id==COM_GAME_JK2_SP
    ||p.id==COM_GAME_ALICE);
   for(int count:{8,128,256}){botlibglobals.maxclients=count;
    assert(EA_Setup()==BLERR_LIBRARYNOTSETUP&&allocations.empty());}
   EA_Shutdown();assert(allocations.empty());continue;
  }
  botlibglobals.maxclients=p.maxClients;EA_Setup();assert(allocations.size()==1);
  std::array<byte,1400> memory;memory.fill(0xa5);
  vm_t vm{};vm.dataBase=memory.data();vm.exactDataLength=int(memory.size());
  if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
  auto address=[&](size_t offset)->intptr_t{return native?reinterpret_cast<intptr_t>(memory.data()+offset):intptr_t(offset);};
  auto call=[&](int ordinal,int client,std::initializer_list<intptr_t> extra={}){
   intptr_t args[5]={ordinal,client},result=77;int n=2;for(auto value:extra)args[n++]=value;
   bool handled=SV_RTCWActionCall(&vm,Lookup(p.id,Module::GAME,Direction::IMPORT,ordinal),args,result);
   assert(handled&&result==0);
  };
  const int client=p.maxClients-1;
  if(p.id==COM_GAME_STVEF_MP){
   // Holomatch keeps the 1.17 contract without RTCW's boundary: moving up and
   // down are a jump and a crouch, and the previous-jump marker is MOVELEFT.
   intptr_t result=73;assert(!SV_RTCWActionCall(nullptr,Call::BOTLIB_EA_VIEW,nullptr,result)&&result==73);
   EA_MoveUp(client);assert(botinputs[client].actionflags==ACTION_JUMP);EA_ResetInput(client);
   assert(botinputs[client].actionflags==ACTION_MOVELEFT);EA_ResetInput(client);
   assert(botinputs[client].actionflags==0);
   EA_MoveDown(client);assert(botinputs[client].actionflags==ACTION_CROUCH);EA_ResetInput(client);
   EA_Jump(client);EA_ResetInput(client);EA_Jump(client);
   assert(botinputs[client].actionflags==ACTION_MOVELEFT);
   EA_Shutdown();continue;
  }
  if(!rtcw){
   intptr_t result=73;assert(!SV_RTCWActionCall(nullptr,Call::BOTLIB_EA_VIEW,nullptr,result)&&result==73);
   EA_MoveUp(client);assert(botinputs[client].actionflags==ACTION_MOVEUP);EA_ResetInput(client);
   assert(botinputs[client].actionflags==0);EA_MoveDown(client);assert(botinputs[client].actionflags==ACTION_MOVEDOWN);
   EA_ResetInput(client);EA_Jump(client);assert(botinputs[client].actionflags==ACTION_JUMP);
   EA_ResetInput(client);assert(botinputs[client].actionflags==ACTION_JUMPEDLASTFRAME);
   EA_Jump(client);assert(!(botinputs[client].actionflags&ACTION_JUMP));EA_ResetInput(client);
   EA_MoveLeft(client);EA_Jump(client);assert(botinputs[client].actionflags==(ACTION_MOVELEFT|ACTION_JUMP));
   EA_Shutdown();continue;
  }
  auto get=[&](float time=.125f,size_t at=65){
   auto before=memory;call(426,client,{floatBits(time),address(at)});fntech3_rtcw_bot_input_t input;
   std::memcpy(&input,memory.data()+at,sizeof(input));
   std::memcpy(before.data()+at,&input,sizeof(input));assert(before==memory);return input;
  };
  auto reset=[&](const fntech3_rtcw_bot_input_t &input,size_t at=3){
   std::memcpy(memory.data()+at,&input,sizeof(input));auto before=memory;
   call(427,client,{address(at)});assert(before==memory);
  };
  // Every defined flag combination plus arbitrary reserved bits round trips.
  for(unsigned flags=0;flags<16384;++flags){
   fntech3_rtcw_bot_input_t input{.25f,{1,-2,3},431,{4,5,-6},int(flags),INT_MIN};
   unsigned raw=flags|((flags*0x9e3779b9u)&~0x3fffu);std::memcpy(&input.actionflags,&raw,4);
   reset(input);auto output=get(.25f);assert(!std::memcmp(&input,&output,sizeof(input)));
  }
  struct Action{int ordinal,flag;};
  for(auto a:{Action{406,2048},Action{409,1024},Action{410,1},Action{411,8192},Action{412,2},
      Action{413,4},Action{414,8},Action{415,512},Action{416,16},Action{417,8},Action{418,16},
      Action{419,32},Action{420,64},Action{421,128},Action{422,256}}){
   reset({});call(a.ordinal,client);assert(get().actionflags==a.flag);
  }
  // The original previous-jump bit is the left-movement bit (128).
  reset({});call(414,client);call(427,client,{0});assert(get().actionflags==128);
  call(414,client);call(415,client);assert(get().actionflags==128);
  call(417,client);assert(get().actionflags==136); // move up is unconditional
  call(427,client,{0});assert(get().actionflags==128);call(427,client,{0});assert(get().actionflags==0);
  call(421,client);call(414,client);assert(get().actionflags==128);
  reset({});call(408,client,{INT_MAX});assert(get().weapon==INT_MAX);
  float vector[3]={10,-20,30};std::memcpy(memory.data()+3,vector,12);
  call(424,client,{address(3)});auto view=get();assert(!std::memcmp(view.viewangles,vector,12));
  for(float speed:{-1000.f,-400.f,-.0f,0.f,5.f,400.f,1000.f}){
   std::memcpy(memory.data()+3,vector,12);call(423,client,{address(3),floatBits(speed)});auto input=get();
   assert(!std::memcmp(input.dir,vector,12)&&input.speed==std::clamp(speed,-400.f,400.f));
  }
  auto before=get();call(425,client,{floatBits(9.f)});auto after=get();assert(!std::memcmp(&before,&after,sizeof(before)));
  call(427,client,{0});after=get();assert(after.weapon==INT_MAX&&!std::memcmp(after.viewangles,vector,12));
  assert(!after.speed&&!after.dir[0]&&!after.dir[1]&&!after.dir[2]);
  const char *prefixes[]={"say ","say_team ","use ","drop ","invuse ","invdrop "};
  for(int ordinal=400;ordinal<=407;++ordinal){if(ordinal==406)continue;
   std::strcpy(reinterpret_cast<char*>(memory.data()+3),"item name; text");call(ordinal,client,{address(3)});
   assert(commandClient==client&&command==std::string(ordinal==407?"":prefixes[ordinal-400])+"item name; text");
  }
  // Exact-end, unaligned buffers are accepted; failed arguments publish nothing.
  fntech3_rtcw_bot_input_t initial{.5f,{1,2,3},17,{4,5,6},0x40000000,19};
  reset(initial,memory.size()-40);auto edge=get(.5f,memory.size()-40);assert(!std::memcmp(&initial,&edge,40));
  auto unchanged=[&](auto attempt){auto data=memory;auto state=botinputs[client];unsigned extra=EA_ExtraInputFlags(client);
   rejected(attempt);assert(memory==data&&!std::memcmp(&state,&botinputs[client],sizeof(state))&&extra==EA_ExtraInputFlags(client));};
  for(int bad:{-1,p.maxClients,INT_MAX,INT_MIN})unchanged([&]{call(424,bad,{0});});
  for(int bits:{int(0x7f800000u),int(0xff800000u),int(0x7fc00001u)}){
   unchanged([&]{call(426,client,{bits,address(65)});});
   unchanged([&]{call(425,client,{bits});});
   std::memcpy(memory.data()+3,vector,12);unchanged([&]{call(423,client,{address(3),bits});});
   for(int offset:{0,4,8,12,16,20,24,28}){
    std::memcpy(memory.data()+3,&initial,40);std::memcpy(memory.data()+3+offset,&bits,4);
    unchanged([&]{call(427,client,{address(3)});});
   }
  }
  unchanged([&]{call(426,client,{0,0});});unchanged([&]{call(424,client,{0});});
  if(!native){
   for(int offset:{-1,0,int(memory.size()-39),int(memory.size()),INT_MAX}){
    unchanged([&]{call(426,client,{0,offset});});if(offset)unchanged([&]{call(427,client,{offset});});
   }
   unchanged([&]{call(424,client,{int(memory.size()-11)});});
  }
  std::memset(memory.data()+3,'x',1024);unchanged([&]{call(407,client,{address(3)});});
  if(sizeof(intptr_t)>4){
   intptr_t wide=INT64_C(0x100000000),args[]={424,wide,0},result=77;
   unchanged([&]{SV_RTCWActionCall(&vm,Call::BOTLIB_EA_VIEW,args,result);});assert(result==77);
   unchanged([&]{call(408,client,{wide});});unchanged([&]{call(426,client,{wide,address(65)});});
  }
  botlib_export=nullptr;unchanged([&]{call(424,client,{0});});
  intptr_t result=73;assert(!SV_RTCWActionCall(nullptr,Call::G_PRINT,nullptr,result)&&result==73);botlib_export=&exports;
  EA_Shutdown();rejected([&]{call(424,client,{0});});EA_Shutdown();assert(allocations.empty());
 }
}
int main(){setup();checkBoundary(false);checkBoundary(true);assert(allocations.empty());}
'''


class RTCWActionsTests(unittest.TestCase):
    compile_and_run = bootstrap.RTCWBotLibTests.compile_and_run

    def test_elementary_actions_and_checked_module_boundary(self):
        self.compile_and_run(action_source() + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
