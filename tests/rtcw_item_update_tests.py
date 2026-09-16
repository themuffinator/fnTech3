"""Execute the inherited item updater through RTCW's no-argument boundary."""
from pathlib import Path
import unittest
import rtcw_routing_tests
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def item_source():
    source = rtcw_routing_tests.routing_source()
    goal = (ROOT / 'code/botlib/be_ai_goal.cpp').read_text()
    source += '\n#include "code/botlib/l_script.h"\n#include "code/botlib/l_precomp.h"\n#include "code/botlib/l_struct.h"\n'
    source += goal[goal.index('static constexpr int IFL_NOTFREE'):goal.index('struct maplocation_t')]
    source += goal[goal.index('struct levelitem_t'):goal.index('static const fielddef_t iteminfo_fields')]
    source += goal[goal.index('struct itemconfig_t'):goal.index('//goal state')]
    source += r'''
static levelitem_t *levelitems,*freelevelitems;
static int numlevelitems,g_gametype;
static itemconfig_t *itemconfig;
static int entityCalls;
int AAS_NextEntity(int){++entityCalls;assert(false);return 0;}
int AAS_EntityType(int){assert(false);return 0;}
int AAS_EntityModelindex(int){assert(false);return 0;}
void AAS_EntityInfo(int,aas_entityinfo_t*){assert(false);}
int AAS_BestReachableArea(float*,float*,float*,float*){assert(false);return 0;}
int AAS_AreaJumpPad(int){assert(false);return 0;}
'''
    for signature in ('static levelitem_t *AllocLevelItem(', 'static void FreeLevelItem(',
                      'static void AddLevelItemToList(', 'static void RemoveLevelItemFromList(',
                      'void BotUpdateEntityItems('):
        source += function(goal, signature) + '\n'
    return source


class RTCWItemUpdateTests(unittest.TestCase):
    compile_and_run = rtcw_routing_tests.RTCWRoutingTests.compile_and_run

    def test_empty_goal_configuration_and_expiration_through_native_qvm_import(self):
        source = item_source() + r'''
int main(){
 using namespace fntech3::rtcw::module;
 botlib_export_t exports{};exports.ai.BotUpdateEntityItems=BotUpdateEntityItems;
 for(const auto &p:profiles){profile=&p;if(!Com_GameIsRTCW())continue;
  assert(!(profile->bots.aiSystems&COM_BOT_AI_GOALS));
  for(bool native:{false,true}){initialize();botlib_export=&exports;vm_t vm{};
   if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
   auto invoke=[&]{intptr_t result=73;assert(SV_RTCWBotLibCall(&vm,Call::BOTLIB_AI_UPDATE_ENTITY_ITEMS,nullptr,result));assert(result==0);};
   invoke();assert(!itemconfig&&!levelitems&&!freelevelitems&&entityCalls==0);
   // The actual routine still expires existing temporary items before its
   // absent-configuration return, using the selected world's clock.
   levelitem_t items[3]{};items[0].timeout=0;items[1].timeout=2;items[2].timeout=3;
   for(auto &item:items)AddLevelItemToList(&item);
   AAS_SetCurrentWorld(0);aasworld.time=2;invoke();assert(levelitems==&items[2]&&!freelevelitems);
   AAS_SetCurrentWorld(1);aasworld.time=2.5f;invoke();
   assert(levelitems==&items[2]&&levelitems->next==&items[0]&&items[0].prev==&items[2]&&freelevelitems==&items[1]);
   aasworld.time=4;invoke();assert(levelitems==&items[0]&&!items[0].next&&freelevelitems==&items[2]&&freelevelitems->next==&items[1]);
   assert(!itemconfig&&entityCalls==0);levelitems=freelevelitems=nullptr;
   shutdown();botlib_export=nullptr;try{invoke();assert(false);}catch(Drop&){}
  }
 }
}
'''
        self.compile_and_run(source, rtcw_routing_tests.RTCWRoutingTests.extra(), ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
