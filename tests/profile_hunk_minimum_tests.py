"""Run the production hunk policy against old configs and larger user settings."""
from pathlib import Path
import unittest
from rtcw_music_tests import RTCWMusicTests

ROOT = Path(__file__).resolve().parents[1]


class ProfileHunkMinimumTests(unittest.TestCase):
    compile_run = RTCWMusicTests.compile_run

    def test_client_and_dedicated_profile_floors(self):
        text = (ROOT / 'code/qcommon/common.c').read_text()
        constants = text[text.index('#ifdef DEDICATED\n#define MIN_COMHUNKMEGS'):
                         text.index('#define DEF_COMZONEMEGS')]
        start = text.index('static void Com_InitHunkMemory( void ) {')
        policy = text[text.index('\tcv = Cvar_Get( "com_hunkMegs"', start):
                      text.index('\tCvar_SetDescription( cv,', start)]
        source = r'''
#include <cassert>
#include <cstdlib>
#include <initializer_list>
#define STRING(x) #x
#define XSTRING(x) STRING(x)
constexpr int CVAR_LATCH=1,CVAR_ARCHIVE=2,CV_INTEGER=1;
constexpr bool qtrue=true;
struct cvar_t {int integer=0;} value;
static int profile=0;
static bool Com_GameIsWolfET(){return profile==4;}
static bool Com_GameIsSTVEF(){return profile==5||profile==6;}
static bool Com_GameIsSTVEFSP(){return profile==5;}
static cvar_t *Cvar_Get(const char *,const char *,int){return &value;}
static cvar_t *Cvar_Set2(const char *,const char *s,bool){value.integer=std::atoi(s);return &value;}
static void Cvar_CheckRange(cvar_t *v,const char *s,const char *,int){if(v->integer<std::atoi(s))v->integer=std::atoi(s);}
static void Com_Printf(const char *,...){}
''' + constants + '\nstatic void apply(){cvar_t *cv;\n' + policy + r'''
}
int main(){
 for(profile=0;profile<16;++profile)for(int request:{1,36,47,56,64,127,128,159,160,161,191,192,193,256,512}){
  value.integer=request;apply();
  int floor=MIN_COMHUNKMEGS;
#ifndef DEDICATED
  if(profile==4||profile==6)floor=128;
  if(profile==5)floor=160;
#endif
  assert(value.integer==(request<floor?floor:request));
 }
}
'''
        self.compile_run(source)
        self.compile_run(source, ['-DDEDICATED'])

    def test_startup_freezes_profile_before_zone_and_registers_commands_afterward(self):
        text = (ROOT / 'code/qcommon/common.c').read_text()
        start = text.index('void Com_Init( char *commandLine ) {')
        setup = text[text.index('\tCom_InitSmallZoneMemory();', start):
                     text.index('\t// get the developer cvar', start)]
        source = r'''
#include <cassert>
static int selected, requested, phases;
static bool cvars, zone, commands;
static void Com_InitSmallZoneMemory(){assert(!phases);++phases;}
static void Cvar_Init(){assert(phases==1);cvars=true;++phases;}
static void Com_ParseCommandLine(char *){assert(cvars&&!selected);++phases;}
static void Cbuf_Init(){assert(cvars&&!selected);++phases;}
static void Com_StartupVariable(const char *){assert(cvars&&!selected);++phases;}
static void Com_InitGameProfile(){assert(phases==5&&!zone&&!commands);selected=requested;++phases;}
static void Com_InitZoneMemory(){assert(selected==requested&&phases==6);zone=true;++phases;}
static void Cmd_Init(){assert(zone&&selected==requested&&phases==7);commands=true;++phases;}
static void Com_InitGameCommands(){assert(commands&&zone&&selected==requested&&phases==8);++phases;}
static void setup(char *commandLine){
''' + setup + r'''
}
int main(){
 for(requested=1;requested<=16;++requested){
  phases=selected=0;cvars=zone=commands=false;
  char commandLine[]="probe";setup(commandLine);
  assert(phases==9&&selected==requested&&zone&&commands);
 }
}
'''
        self.compile_run(source)
        self.compile_run(source, ['-DDEDICATED'])

    def test_sp_initial_zone_precedes_large_renderer_allocations(self):
        text = (ROOT / 'code/qcommon/common.c').read_text()
        constants = text[text.index('#ifdef DEDICATED\n#define MIN_COMHUNKMEGS'):
                         text.index('#define DEF_COMZONEMEGS')]
        start = text.index('static void Com_InitZoneMemory( void ) {')
        policy = text[text.index('\tcv = Cvar_Get( "com_zoneMegs"', start):
                      text.index('\tCvar_SetDescription( cv,', start)]
        source = r'''
#include <cassert>
#include <cstdlib>
#include <initializer_list>
#define STRING(x) #x
#define XSTRING(x) STRING(x)
constexpr int CVAR_LATCH=1,CVAR_ARCHIVE=2,CV_INTEGER=1;
constexpr bool qtrue=true;
struct cvar_t {int integer=0;} value;
static int profile;
static bool Com_GameIsSTVEFSP(){return profile==5;}
static cvar_t *Cvar_Get(const char *,const char *,int){return &value;}
static cvar_t *Cvar_Set2(const char *,const char *s,bool){value.integer=std::atoi(s);return &value;}
static void Cvar_CheckRange(cvar_t *v,const char *s,const char *,int){if(v->integer<std::atoi(s))v->integer=std::atoi(s);}
static void Com_Printf(const char *,...){}
''' + constants + '\nstatic void apply(){cvar_t *cv;\n' + policy + r'''
}
int main(){
 for(profile=0;profile<16;++profile)for(int request:{0,1,16,32,64,95,96,97,128,256}){
  value.integer=request;apply();int floor=1;
#ifndef DEDICATED
  if(profile==5)floor=96;
#endif
  assert(value.integer==(request<floor?floor:request));
 }
}
'''
        self.compile_run(source)
        self.compile_run(source, ['-DDEDICATED'])


if __name__ == '__main__':
    unittest.main()
