"""Execute cinematic playback, command selection and renderer presentation paths."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]

HOST = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
static comGameId_t profile;
int Com_GameIsRTCWSP(){return profile==COM_GAME_RTCW_SP;}
int Com_GameIsRTCW(){return Com_GameIsRTCWSP()||profile==COM_GAME_RTCW_MP;}
void QDECL Com_Error(errorParm_t,const char *,...){throw 1;}
void QDECL Com_DPrintf(const char *,...){}
int QDECL Com_sprintf(char *out,int size,const char *format,...){
 va_list ap;va_start(ap,format);const int result=vsnprintf(out,size,format,ap);va_end(ap);return result;
}
void Q_strncpyz(char *out,const char *in,int size){assert(size>0);std::snprintf(out,size,"%s",in);}
int Q_stricmp(const char *a,const char *b){return strcmp(a,b);}
static qboolean ToQboolean(bool value){return value?qtrue:qfalse;}
enum { UI_SET_ACTIVE_MENU, UIMENU_NONE };
constexpr int MAX_VIDEO_HANDLES=16;
constexpr int DEFAULT_CIN_WIDTH=512,DEFAULT_CIN_HEIGHT=512;
static struct {int state=CA_DISCONNECTED;struct {int vidWidth=640,vidHeight=480;}glconfig;
 float scale=1,biasX=0,biasY=0;int whiteShader=7;} cls;
struct File {int value=0;int get(){return value;}void reset(){value=0;}};
struct Movie {
 std::array<char,256>fileName{};File iFile;int ROQSize=0;
 int xpos=0,ypos=0,width=0,height=0,CIN_WIDTH=0,CIN_HEIGHT=0,drawX=512,drawY=512,playonwalls=0;
 bool looping=false,holdAtEnd=false,dirty=false,alterGameState=false,silent=false,shader=false;
 bool rtcwLetterBox=false;byte *buf=nullptr;e_status status=FMV_IDLE;
};
static std::array<Movie,MAX_VIDEO_HANDLES> cinTable;
static struct {int currentHandle;std::array<byte,16>file;} cin;
static int currentHandle=-1,CL_handle=-1,s_rawend=0,s_soundtime=100;
static int nextmaps=0,openCalls=0,stopSounds=0,stopMovies=0,closeConsole=0,menuCalls=0;
static bool skipLogo=false,validHeader=true,validFrame=true,failedFirstFrame=false;
static int fileLength=32,readLength=16;
static const char *argument="movie.roq",*mode="";
static cvar_t inGameVideo{},cinematicAspect{};
static cvar_t *cl_inGameVideo=&inGameVideo,*cl_cinematicAspect=&cinematicAspect;
static void *uivm=nullptr;
static bool CL_ShouldSkipIdLogoCinematic(const char *){return skipLogo;}
static void CL_RunCinematicNextmap(){++nextmaps;}
static int OpenFileRead(const char *name,File &file,qboolean unique){
 assert(unique);++openCalls;file.value=1;return strstr(name,"missing")?-1:fileLength;
}
static int FileRead(int handle,byte *out,int length){assert(handle==1&&length==16);out[0]=validHeader?0x84:0;out[1]=0x10;return readLength;}
static void initRoQ(){}
static bool RoQ_init(){return validFrame;}
static void RoQShutdown(){cinTable[currentHandle].iFile.reset();cinTable[currentHandle].fileName[0]=0;}
static void Con_Close(){++closeConsole;}
static void VM_Call(void *,int count,int call,int menu){assert(count==1&&call==UI_SET_ACTIVE_MENU&&menu==UIMENU_NONE);++menuCalls;}
static const char *Cmd_Argv(int index){return index==1?argument:mode;}
static void S_StopAllSounds(){++stopSounds;}
static void SCR_StopCinematic(){++stopMovies;cls.state=CA_DISCONNECTED;}
static byte pixels[4]={1,2,3,255};
static void SCR_RunCinematic(){
 if(failedFirstFrame){cinTable[currentHandle].status=FMV_IDLE;CL_handle=currentHandle=-1;return;}
 cinTable[currentHandle].buf=pixels;
}
static void CIN_SetExtents(int,int,int,int,int);
static void CIN_SetLooping(int,qboolean);
static int resamples=0;
static void CIN_ResampleCinematic(int,int *out){++resamples;*out=0x7f;}
struct ScopedTempMemory {
 std::vector<int> data;
 static ScopedTempMemory Allocate(int bytes){return {std::vector<int>(bytes/4)};}
 template<class T>T *as(){return reinterpret_cast<T *>(data.data());}
};
vec4_t colorBlack={0,0,0,1};
struct Event {int kind;float x,y,w,h;int cols,rows,handle;qboolean dirty;};
static std::vector<Event> events;
static struct {
 void SetColor(const float *color){
  assert(!color||(!color[0]&&!color[1]&&!color[2]&&color[3]==1));
  events.push_back({color?1:4});
 }
 void DrawStretchPic(float x,float y,float w,float h,float s0,float t0,float s1,float t1,qhandle_t shader){
  assert(!s0&&!t0&&!s1&&!t1&&shader==cls.whiteShader);events.push_back({2,x,y,w,h});
 }
 void DrawStretchRaw(int x,int y,int w,int h,int cols,int rows,const byte *buffer,int handle,qboolean dirty){
  assert(buffer);events.push_back({3,(float)x,(float)y,(float)w,(float)h,cols,rows,handle,dirty});
 }
}re;
static void Near(float a,float b){assert(std::fabs(a-b)<.002f);}
'''

PLAY = r'''
int main(){
 for(auto id:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){
  profile=id;
  for(int bits=0;bits<128;++bits){
   cinTable={};currentHandle=-1;cls.state=CA_ACTIVE;openCalls=0;
   const auto h=CIN_PlayCinematic("movie.roq",17,29,123,97,bits);
   assert(h==0&&openCalls==1);auto &movie=cinTable[h];
   assert(movie.xpos==17&&movie.ypos==29&&movie.width==123&&movie.height==97);
   assert(movie.rtcwLetterBox==(id==COM_GAME_RTCW_SP&&bool(bits&32)));
   assert(movie.looping==bool(bits&2)&&movie.holdAtEnd==bool(bits&4));
   assert(movie.silent==bool(bits&8)&&movie.shader==bool(bits&16));
   assert(movie.alterGameState==bool(bits&1)&&movie.status==FMV_PLAY);
   // Non-system requests for an already playing name retain its flags/extents.
   assert(CIN_PlayCinematic("movie.roq",-1,-1,-1,-1,(bits^32)&~1)==h&&openCalls==1);
   assert(movie.xpos==17&&movie.rtcwLetterBox==(id==COM_GAME_RTCW_SP&&bool(bits&32)));
   movie.fileName[0]=0;movie.status=FMV_IDLE;
   assert(CIN_PlayCinematic("replacement.roq",1,2,3,4,0)==0&&!movie.rtcwLetterBox);
  }
  for(const char *option:{"","1","2","3","31","4"}){
   cinTable={};cls.state=CA_DISCONNECTED;mode=option;argument="movie.roq";
   CL_PlayCinematic_f();assert(CL_handle==0&&cinTable[0].buf);
   const bool box=id==COM_GAME_RTCW_SP&&option[0]=='3';
   const auto &movie=cinTable[0];assert(movie.rtcwLetterBox==box);
   assert(movie.xpos==0&&movie.ypos==(box?105:0)&&movie.width==640&&movie.height==(box?270:480));
   assert(movie.looping==(option[0]=='2')&&movie.holdAtEnd==(option[0]=='1'));
  }
  for(int count=0;count<16;++count){
   cinTable={};cls.state=CA_DISCONNECTED;fileLength=count;
   assert(CIN_PlayCinematic("short.roq",0,0,640,480,1)==-1);
   assert(!cinTable[0].iFile.value&&!cinTable[0].fileName[0]);
   fileLength=32;readLength=count;
   assert(CIN_PlayCinematic("short-read.roq",0,0,640,480,1)==-1);
   assert(!cinTable[0].iFile.value&&!cinTable[0].fileName[0]&&cls.state==CA_DISCONNECTED);
   readLength=16;
  }
  cinTable={};cls.state=CA_DISCONNECTED;failedFirstFrame=true;
  CL_PlayCinematic_f();assert(CL_handle==-1&&currentHandle==-1);failedFirstFrame=false;
 }
 profile=COM_GAME_RTCW_SP;cinTable={};cls.state=CA_DISCONNECTED;
 assert(CIN_PlayCinematic("missing.roq",0,0,640,480,33)==-1);
 assert(!cinTable[0].fileName[0]&&!cinTable[0].iFile.value&&cls.state==CA_DISCONNECTED);
 validHeader=false;assert(CIN_PlayCinematic("bad.roq",0,0,640,480,33)==-1);validHeader=true;
 validFrame=false;assert(CIN_PlayCinematic("bad.roq",0,0,640,480,33)==-1);validFrame=true;
 cinTable={};argument="missing.roq";nextmaps=0;CL_PlayCinematic_f();assert(CL_handle==-1&&nextmaps==1);argument="movie.roq";
 cinTable={};cls.state=CA_CINEMATIC;mode="3";stopMovies=0;CL_PlayCinematic_f();assert(stopMovies==1);
 skipLogo=true;openCalls=0;nextmaps=0;CL_PlayCinematic_f();assert(nextmaps==1&&!openCalls);
 skipLogo=false;cinTable={};uivm=&cin;menuCalls=0;
 assert(CIN_PlayCinematic("movie.roq",0,105,640,270,33)==0&&menuCalls==1);
}
'''

DRAW = r'''
int main(){
 const int sizes[][2]={{640,480},{1280,960},{1600,1200},{1920,1080},{641,479},{320,200}};
 for(auto id:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_QUAKE3,COM_GAME_QUAKELIVE})
 for(const auto &size:sizes)for(bool aspect:{false,true})for(bool ui:{false,true})
 for(bool box:{false,true})for(bool resample:{false,true}){
  profile=id;cls.glconfig.vidWidth=size[0];cls.glconfig.vidHeight=size[1];
  cls.scale=std::min(size[0]/640.f,size[1]/480.f);
  cls.biasX=(size[0]-640*cls.scale)*.5f;cls.biasY=(size[1]-480*cls.scale)*.5f;
  cinematicAspect.integer=aspect;cinTable={};
  int handle=CIN_PlayCinematic("movie.roq",0,105,640,270,box?32:0);assert(handle==0);
  auto &movie=cinTable[0];movie.buf=pixels;movie.CIN_WIDTH=resample?256:512;
  events.clear();resamples=0;
  if(ui)CIN_DrawCinematicUI(handle);else CIN_DrawCinematic(handle);
  const bool letterbox=id==COM_GAME_RTCW_SP&&box;
  assert(events.size()==(letterbox?5:1));
  if(letterbox){
   assert(events[0].kind==1&&events[1].kind==2&&events[2].kind==2&&events[3].kind==4);
   Near(events[1].x,0);Near(events[1].y,0);Near(events[1].w,(float)size[0]);
   Near(events[1].h,size[1]*.21875f);
   Near(events[2].x,0);Near(events[2].y,size[1]*.78125f-1);
   Near(events[2].w,(float)size[0]);Near(events[2].h,size[1]*.21875f+1);
  }
  const auto &raw=events.back();assert(raw.kind==3&&raw.handle==0&&raw.dirty);
  const bool uniform=ui&&aspect&&id!=COM_GAME_RTCW_SP&&id!=COM_GAME_RTCW_MP;
  Near(raw.x,uniform?(int)cls.biasX:0);Near(raw.y,uniform?(int)(105*cls.scale+cls.biasY):(int)(105*(size[1]/480.f)));
  Near(raw.w,uniform?(int)(640*cls.scale):size[0]);Near(raw.h,uniform?(int)(270*cls.scale):(int)(270*(size[1]/480.f)));
  assert(raw.cols==(resample?256:512)&&raw.rows==(resample?256:512));
  assert(resamples==resample&&!movie.dirty);
  events.clear();CIN_DrawCinematic(handle);assert(!events.back().dirty&&resamples==resample);
  CIN_SetExtents(handle,32,40,320,240);assert(movie.dirty);
  events.clear();CIN_DrawCinematic(handle);assert(events.back().dirty);
  if(letterbox)Near(events[1].w,size[0]*.5f); // Bars follow width, remain screen anchored.
  for(int invalid:{-1,MAX_VIDEO_HANDLES,INT_MAX,INT_MIN}){
   events.clear();CIN_DrawCinematic(invalid);CIN_DrawCinematicUI(invalid);assert(events.empty());
  }
  movie.buf=nullptr;events.clear();CIN_DrawCinematic(handle);assert(events.empty());
  movie.buf=pixels;movie.status=FMV_EOF;CIN_DrawCinematicUI(handle);assert(events.empty());
 }
}
'''

STARTUP_HOST = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_manager.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
static const comGameProfile_t *selected;
const comGameProfile_t *Com_GameProfile(){return selected;}
static cvar_t played{},skip{};
static cvar_t *com_introPlayed=&played,*com_skipIdLogo=&skip;
static std::map<std::string,std::string> writes;
static std::string queued;
static void Cvar_Set(const char *name,const char *value){
 writes[name]=value;if(!strcmp(name,"com_introplayed"))played.integer=atoi(value);
}
static void Cbuf_AddText(const char *text){queued+=text;}
const char *QDECL va(const char *format,...){
 static char buffer[1024];va_list args;va_start(args,format);
 vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);return buffer;
}
'''

STARTUP_CHECKS = r'''
int main(){
 played.name=const_cast<char *>("com_introplayed");
 for(const auto &profile:profiles)for(int seen:{0,1})for(int bypass:{-1,0,1}){
  selected=&profile;played.integer=seen;skip.integer=bypass;
  com_skipIdLogo=bypass<0?nullptr:&skip;writes.clear();queued.clear();
  Com_QueueIntroCinematics();
  if(profile.id==COM_GAME_WOLFET){
   assert(queued==(bypass==1?"":"cinematic etintro.roq\n"));
   assert(writes.empty()&&played.integer==seen);continue;
  }
  const bool sp=profile.id==COM_GAME_RTCW_SP,mp=profile.id==COM_GAME_RTCW_MP;
  const std::string intro=sp?"cinematic wolfintro.RoQ 3":mp?"cinematic wolfintro.RoQ":"cinematic intro.RoQ";
  const std::string logo=sp?"":mp?"cinematic gmlogo.RoQ":"cinematic idlogo.RoQ";
  const bool showLogo=!sp&&bypass!=1;
  assert(queued==(showLogo?logo+"\n":!seen?intro+"\n":""));
  assert(writes.count("nextmap")==(!seen&&showLogo));
  if(!seen&&showLogo)assert(writes["nextmap"]==intro);
  assert(writes.count("com_introplayed")==(!seen&&!sp));
  assert(played.integer==(sp?seen:1));
  writes.clear();queued.clear();Com_QueueIntroCinematics();
  assert(queued==(showLogo?logo+"\n":sp&&!seen?intro+"\n":""));
  assert(writes.empty()); // SP repeats; Q3/QL/MP only mark the first intro.
 }
}
'''

TIMING_HOST = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstdint>
constexpr int MAX_VIDEO_HANDLES=16;
static comGameId_t profile=COM_GAME_RTCW_SP;
int Com_GameIsRTCWSP(){return profile==COM_GAME_RTCW_SP;}
struct Movie {int startTime=0,lastTime=0,playonwalls=1;long tfps=0,numQuads=0,roqFPS=24;
 bool shader=false,alterGameState=false,looping=false;e_status status=FMV_PLAY;};
static std::array<Movie,MAX_VIDEO_HANDLES>cinTable;
static int currentHandle=0,now=0,interrupts=0,resets=0,shutdowns=0;
static struct {int currentHandle=0;}cin;
static struct {int state=CA_CINEMATIC;}cls;
static int CL_ScaledMilliseconds(){return now;}
static bool restartInside=false,finishInside=false;static int headerChunks=0;
static void RoQReset(){++resets;auto &m=cinTable[currentHandle];m.startTime=m.lastTime=now;m.numQuads=0;m.status=FMV_LOOPED;}
static void RoQInterrupt(){
 assert(++interrupts<1000);auto &m=cinTable[currentHandle];
 if(restartInside){restartInside=false;now+=5;m.startTime=m.lastTime=now;m.numQuads=0;}
 else if(finishInside){finishInside=false;m.status=FMV_EOF;}
 else if(headerChunks){--headerChunks;}
 else ++m.numQuads;
}
static void RoQShutdown(){++shutdowns;cinTable[currentHandle].status=FMV_IDLE;}
'''

TIMING_CHECKS = r'''
static void Clear(){cinTable={};cin.currentHandle=currentHandle=0;now=interrupts=resets=shutdowns=0;
 cls.state=CA_CINEMATIC;restartInside=finishInside=false;headerChunks=0;}
int main(){
 for(auto id:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){
  profile=id;Clear();now=1000;assert(CIN_RunCinematic(0)==FMV_PLAY);
  assert(cinTable[0].numQuads==(id==COM_GAME_RTCW_SP?24:30));
  Clear();auto &m=cinTable[0];m.startTime=1200;m.lastTime=1242;m.numQuads=1;now=16;
  assert(CIN_RunCinematic(0)==FMV_PLAY&&interrupts==0&&m.numQuads==1);
  assert(m.startTime==-26&&m.lastTime==16);now=66;CIN_RunCinematic(0);assert(m.numQuads==2);
  // A decoder that has already produced the target frame waits instead of consuming to EOF.
  Clear();cinTable[0].numQuads=1;CIN_RunCinematic(0);assert(!interrupts);
  Clear();m.shader=true;m.startTime=0;m.lastTime=100;m.numQuads=id==COM_GAME_RTCW_SP?2:3;
  now=1000;CIN_RunCinematic(0);assert(!interrupts&&m.startTime==900);
  now=1042;CIN_RunCinematic(0);assert(interrupts==1);
  Clear();m.startTime=m.lastTime=INT_MAX-40;now=INT_MIN+40;
  CIN_RunCinematic(0);assert(m.numQuads==(id==COM_GAME_RTCW_SP?1:2));
  Clear();m.numQuads=-1;CIN_RunCinematic(0);assert(interrupts==2&&m.numQuads==1);
  Clear();headerChunks=3;CIN_RunCinematic(0);assert(interrupts==4&&m.numQuads==1&&now==0);
  Clear();now=1000;restartInside=true;CIN_RunCinematic(0);assert(interrupts==2&&m.numQuads==1&&m.tfps==0&&m.startTime==now);
 }
 profile=COM_GAME_RTCW_SP;Clear();cinTable[0].roqFPS=65535;
 assert(CIN_FrameForTime(0,INT_MAX)==INT_MAX); // No signed multiplication overflow.
 assert(CIN_FrameForTime(0,-1)==0);
 Clear();now=1000;finishInside=true;assert(CIN_RunCinematic(0)==FMV_EOF&&shutdowns==1);
 Clear();now=1000;cinTable[0].looping=true;finishInside=true;CIN_RunCinematic(0);assert(resets==1&&!shutdowns);
 Clear();cin.currentHandle=1;assert(CIN_RunCinematic(0)==FMV_PLAY&&resets==1);
 for(int invalid:{-1,16,INT_MIN,INT_MAX}){Clear();assert(CIN_RunCinematic(invalid)==FMV_EOF&&!interrupts);}
 Clear();cinTable[0].status=FMV_EOF;assert(CIN_RunCinematic(0)==FMV_EOF&&!interrupts);
 Clear();cinTable[0].status=FMV_IDLE;assert(CIN_RunCinematic(0)==FMV_IDLE&&!interrupts);
 Clear();cinTable[0].playonwalls=-2;now=1000;assert(CIN_RunCinematic(0)==FMV_PLAY&&!interrupts);
 Clear();cinTable[0].alterGameState=true;cls.state=CA_DISCONNECTED;now=1000;
 assert(CIN_RunCinematic(0)==FMV_PLAY&&!interrupts);
}
'''


class RTCWCinematicPresentationTests(unittest.TestCase):
    def execute(self, checks, startup=False, timing=False):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        if timing:
            cinematic = (ROOT / 'code/client/cl_cin.cpp').read_text()
            body = TIMING_HOST + '\n'.join(function(cinematic, name) for name in (
                'static long CIN_FrameForTime(', 'e_status CIN_RunCinematic(')) + checks
        elif startup:
            catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text()
            catalog = catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('static_assert(')]
            body = STARTUP_HOST + catalog + function((ROOT / 'code/qcommon/common.c').read_text(),
                                                     'static void Com_QueueIntroCinematics(') + checks
        else:
            cinematic = (ROOT / 'code/client/cl_cin.cpp').read_text()
            constants = '\n'.join(line for line in cinematic.splitlines()
                                  if line.startswith('constexpr int RTCW_SP_CIN_'))
            screen = (ROOT / 'code/client/cl_scrn.cpp').read_text()
            bodies = '\n'.join(function(screen, name) for name in (
                'void SCR_AdjustFrom640(', 'void SCR_AdjustFrom640Uniform('))
            bodies += '\n'.join(function(cinematic, name) for name in (
                'static int CIN_HandleForVideo(', 'int CIN_PlayCinematic(',
                'void CIN_SetExtents(', 'static void CIN_SetLooping( int handle, qboolean loop ) {',
                'static void CIN_DrawCinematicInternal(', 'void CIN_DrawCinematic(',
                'void CIN_DrawCinematicUI(', 'void CL_PlayCinematic_f('))
            body = HOST + constants + '\n' + bodies + checks
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        with tempfile.TemporaryDirectory(prefix='rtcw-cinematic-presentation-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(body, encoding='utf-8')
            result = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT),
                                     str(cpp), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            result = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_profile_flags_commands_reuse_and_playback_failures(self):
        self.execute(PLAY)

    def test_bars_scaling_resampling_extents_and_invalid_draws(self):
        self.execute(DRAW)

    def test_catalog_drives_startup_movies_and_skip_replay_settings(self):
        self.execute(STARTUP_CHECKS, startup=True)

    def test_frame_rate_first_frame_clock_handoff_rewind_and_wrap(self):
        self.execute(TIMING_CHECKS, timing=True)


if __name__ == '__main__':
    unittest.main()
