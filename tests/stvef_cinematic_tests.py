"""Execute Elite Force movie lifecycle and key policy without controlling input."""
from pathlib import Path
import unittest

from rtcw_music_fixture import function
import stvef_bink_tests

ROOT = Path(__file__).resolve().parents[1]

HOST = r'''
#include <array>
#include <cstdio>
#include <memory>
#include <unordered_map>
#include "code/compat/stvef_cinematic_presentation.hpp"
using byte=unsigned char;
enum e_status {FMV_EOF,FMV_PLAY,FMV_IDLE};
using qboolean=int;
constexpr int qtrue=1,qfalse=0,MAX_QPATH=64,MAX_VIDEO_HANDLES=16,SCREEN_WIDTH=640,SCREEN_HEIGHT=480;
constexpr int CA_DISCONNECTED=1,CA_LOADING=2,CA_ACTIVE=3,CA_CINEMATIC=4,K_MOUSE1=178;
constexpr int UI_SET_ACTIVE_MENU=1,UIMENU_NONE=0;
namespace fntech3::stvef::bink {
class Movie {
public:
 enum class Result {Waiting,Frame,End};
 Result result=Result::Waiting;
 int steps=0,rewinds=0;bool rewindOK=true,paused=false;float volume=1;
 const char *error="";byte pixels[4]={1,2,3,4};
 Result Step(std::uint32_t){if(paused)return Result::Waiting;++steps;return result;}
 void Pause(bool value,std::uint32_t){paused=value;}
 void Volume(float value){volume=value;}
 bool Rewind(){++rewinds;if(!rewindOK)error="rewind failed";return rewindOK;}
 const char *Error(){return error;}
 byte *Pixels(){return pixels;}
};
}
struct Row {
 std::array<char,256>fileName{};
 std::unique_ptr<fntech3::stvef::bink::Movie>bink;
 fntech3::stvef::bink::Presentation binkPresentation;
 struct {void reset(){}}iFile;
 e_status status=FMV_IDLE;
 bool dirty=false,stvefInGame=false,alterGameState=false,silent=false,looping=false,holdAtEnd=false;
 byte *buf=nullptr;int startTime=0;
};
static std::array<Row,MAX_VIDEO_HANDLES>cinTable;
static struct {int currentHandle=-1;}cin;
static int currentHandle=-1,CL_handle=-1,now=100,catcher=0,playCalls=0,nextmaps=0,stoppedSounds=0;
static bool sp=true,failPlay=false;
static char stvefPendingMovie[MAX_QPATH];
static std::string played,argument="st_01";
static int argc=2;
static struct {int state=CA_DISCONNECTED;}cls;
static int gw_active=1,gw_minimized=0;
static void *uivm=reinterpret_cast<void *>(1);
static std::unordered_map<std::string,std::string>cvars;
static void Cvar_Set(const char *name,const char *value){cvars[name]=value;}
static int Cvar_VariableIntegerValue(const char *name){return atoi(cvars[name].c_str());}
static float Cvar_VariableValue(const char *name){return static_cast<float>(atof(cvars[name].c_str()));}
static bool Com_GameIsSTVEFSP(){return sp;}
// Alice's and Medal of Honor's movies take their own paths
// (tests/alice_cinematic_tests.py); no profile here is either.
static bool Com_GameIsRitual(){return false;}
static bool Com_GameIsMOHAA(){return false;}
static void S_AliceMP3Free(){}
static void CL_MOHAAMovieSoundUpdate(){}
namespace fntech3::alice::cinematic {struct Record{};static void Stop(bool,Record&){}}
static fntech3::alice::cinematic::Record aliceCinematic;
static int Sys_Milliseconds(){return now;}
static int Key_GetCatcher(){return catcher;}
static int Cmd_Argc(){return argc;}
static const char *Cmd_Argv(int index){assert(index==1);return argument.c_str();}
static void Com_Printf(const char *,...){}
static void Com_DPrintf(const char *,...){}
static void Q_strncpyz(char *out,const char *in,int size){std::snprintf(out,size,"%s",in);}
static void CL_RunCinematicNextmap(){++nextmaps;}
static void S_StopAllSounds(){++stoppedSounds;}
static void VM_Call(void *vm,int count,int call,int menu){assert(vm==uivm&&count==1&&call==UI_SET_ACTIVE_MENU&&menu==UIMENU_NONE);}
static e_status CIN_RunBink(int);
static void SCR_StopCinematic();
static e_status CIN_StopCinematic(int);
static e_status CIN_RunCinematic(int handle){return CIN_RunBink(handle);}
static int CIN_PlayCinematic(const char *name,int x,int y,int w,int h,int bits){
 assert(!x&&!y&&w==640&&h==480&&!bits);++playCalls;played=name;if(failPlay)return -1;
 auto &row=cinTable[0];row.bink=std::make_unique<fntech3::stvef::bink::Movie>();
 row.binkPresentation.Open(Cvar_VariableValue("cl_VidFadeUp")!=0,Cvar_VariableValue("cl_VidFadeDown")!=0,0);
 row.fileName[0]='x';row.status=FMV_PLAY;row.startTime=now;return 0;
}
'''

CHECKS = r'''
int main(){
 using Result=fntech3::stvef::bink::Movie::Result;
 sp=false;CL_PlayInGameCinematic_f();assert(!playCalls&&!stvefPendingMovie[0]);sp=true;
 argc=1;CL_PlayInGameCinematic_f();assert(!stvefPendingMovie[0]);argc=2;
 cls.state=CA_LOADING;CL_PlayInGameCinematic_f();assert(std::string(stvefPendingMovie)=="st_01"&&!playCalls);
 CIN_CloseAllVideos();assert(stvefPendingMovie[0]);SCR_RunCinematic();assert(!playCalls);
 cls.state=CA_ACTIVE;SCR_RunCinematic();assert(playCalls==1&&played=="st_01"&&!stvefPendingMovie[0]);
 auto &row=cinTable[0];assert(row.stvefInGame&&Cvar_VariableIntegerValue("cl_paused")==1&&cls.state==CA_ACTIVE);
 assert(cvars["timescale"]=="1"&&cvars["skippingCinematic"]=="0");
 cvars["s_volume"]="0.5";row.bink->result=Result::Frame;
 SCR_RunCinematic();assert(row.dirty&&row.buf==row.bink->Pixels()&&row.bink->volume==0.5f);
 row.silent=true;SCR_RunCinematic();assert(row.bink->volume==0);row.silent=false;
 gw_active=0;cvars["s_muteWhenUnfocused"]="1";SCR_RunCinematic();assert(row.bink->volume==0);
 gw_active=1;gw_minimized=1;cvars["s_muteWhenMinimized"]="1";SCR_RunCinematic();assert(row.bink->volume==0);gw_minimized=0;
 // Early skip requests are swallowed; menu/console catchers and non-SP
 // profiles retain their normal keys. These call policy only, never OS input.
 now=1299;assert(CL_STVEFSPCinematicKey('a')&&CL_handle==0);
 catcher=1;now=1300;assert(!CL_STVEFSPCinematicKey('a')&&CL_handle==0);catcher=0;
 sp=false;assert(!CL_STVEFSPCinematicKey('a'));sp=true;
 assert(!CL_STVEFSPCinematicKey(200));assert(CL_STVEFSPCinematicKey(K_MOUSE1));
 assert(CL_handle==-1&&!row.bink&&!Cvar_VariableIntegerValue("cl_paused")&&cls.state==CA_ACTIVE&&!nextmaps);
 // Unsigned elapsed time handles the millisecond clock wrapping.
 now=static_cast<int>(0xfffffff0u);CL_PlayInGameCinematic_f();now=0x20;
 assert(CL_STVEFSPCinematicKey(27)&&CL_handle==0);now=0x500;
 assert(CL_STVEFSPCinematicKey(27)&&CL_handle==-1);
 failPlay=true;CL_PlayInGameCinematic_f();assert(CL_handle==-1&&!Cvar_VariableIntegerValue("cl_paused"));failPlay=false;
 CL_PlayInGameCinematic_f();row.bink->result=Result::End;row.looping=true;
 SCR_RunCinematic();assert(row.bink&&row.bink->rewinds==1&&!row.buf&&CL_handle==0);
 row.looping=false;row.holdAtEnd=true;SCR_RunCinematic();assert(row.status==FMV_IDLE&&row.bink->volume==0&&CL_handle==0);
 int steps=row.bink->steps;SCR_RunCinematic();assert(row.bink->steps==steps);SCR_StopCinematic();
 CL_PlayInGameCinematic_f();row.holdAtEnd=false;row.bink->result=Result::End;SCR_RunCinematic();
 assert(CL_handle==-1&&!row.bink&&!Cvar_VariableIntegerValue("cl_paused")&&cls.state==CA_ACTIVE);
 CL_PlayInGameCinematic_f();row.bink->result=Result::End;row.looping=row.holdAtEnd=true;row.bink->rewindOK=false;
 SCR_RunCinematic();assert(CL_handle==-1&&!row.bink&&!Cvar_VariableIntegerValue("cl_paused"));row.looping=row.holdAtEnd=false;
 CL_PlayInGameCinematic_f();row.bink->result=Result::End;row.bink->error="decode failed";row.holdAtEnd=true;
 SCR_RunCinematic();assert(CL_handle==-1&&!row.bink);row.holdAtEnd=false;
 CL_PlayInGameCinematic_f();row.stvefInGame=false;row.alterGameState=true;cls.state=CA_CINEMATIC;
 row.bink->result=Result::End;SCR_RunCinematic();assert(cls.state==CA_DISCONNECTED&&nextmaps==1&&CL_handle==-1);
 cls.state=CA_LOADING;CL_PlayInGameCinematic_f();assert(stvefPendingMovie[0]);SCR_StopCinematic();assert(!stvefPendingMovie[0]);
 for(int handle:{-1,16,0x7fffffff}){CL_handle=handle;assert(!CL_STVEFSPCinematicKey('a'));SCR_StopCinematic();}
}
'''


class STVEFCinematicTests(unittest.TestCase):
    def test_vfs_reads_release_handles_on_missing_short_invalid_and_valid_files(self):
        host = r'''
#include <memory>
constexpr int qtrue=1;
static int fileLength,readLength,reads,closes,opens;
static std::vector<std::uint8_t>stream;
static bool fileLive=false,openFails=false;
struct ScopedFileHandle {
 int value=0;
 ~ScopedFileHandle(){reset();}
 explicit operator bool()const{return value!=0;}
 int get()const{return value;}
 void reset(){if(value){assert(fileLive);fileLive=false;++closes;value=0;}}
};
static int OpenFileRead(const char *path,ScopedFileHandle& file,int unique){
 assert(!fileLive&&unique);if(!strcmp(path,"missing"))return -1;
 fileLive=true;file.value=1;return fileLength;
}
static int FileRead(int handle,void *out,int size){
 assert(handle==1&&fileLive&&size==fileLength);++reads;
 memcpy(out,stream.data(),static_cast<size_t>(std::min(size,readLength)));return readLength;
}
static void Com_Printf(const char *,...){}
namespace fntech3::stvef::bink {
class Movie {
public:
 bool Open(std::vector<std::uint8_t> bytes,const char *runtime,bool german,bool sound){
  assert(!fileLive&&!strcmp(runtime,"installation/binkw32.dll")&&german&&sound);++opens;
  FileInfo info;return !openFails&&Inspect(bytes.data(),bytes.size(),german,info);
 }
 const char *Error(){return "invalid";}
};
}
'''
        checks = r'''
int main(){
 auto Open=[](const char *path){return CIN_OpenBink(path,"installation/binkw32.dll",true,true);};
 assert(!Open("missing")&&!reads&&!closes&&!opens);
 for(int size:{-1,0,43,static_cast<int>(MaxFileBytes+1)}){
  fileLength=size;int before=closes;assert(!Open("badsize")&&closes==before+1&&!fileLive&&!reads&&!opens);
 }
 stream=File(2);fileLength=stream.size();readLength=fileLength-1;
 int before=closes;assert(!Open("short")&&closes==before+1&&!fileLive&&reads==1&&!opens);
 readLength=fileLength;stream[0]=0;assert(!Open("malformed")&&!fileLive&&opens==1);
 stream=File(2);openFails=true;assert(!Open("runtime-failure")&&!fileLive&&opens==2);
 openFails=false;assert(Open("valid")&&!fileLive&&opens==3);
}
'''
        source = (ROOT / 'code/client/client_stvef_sp_cinematic.hpp').read_text()
        body = function(source, 'static std::unique_ptr<fntech3::stvef::bink::Movie> CIN_OpenBink(')
        stvef_bink_tests.STVEFBinkTests().execute(host + body + checks)

    def lifecycle(self, checks):
        source = (ROOT / 'code/client/cl_cin.cpp').read_text()
        adapter = (ROOT / 'code/client/client_stvef_sp_cinematic.hpp').read_text()
        body = HOST + '\n'.join(function(source, name) for name in (
            'void CIN_CloseAllVideos(', 'static void RoQShutdown( void ) {', 'e_status CIN_StopCinematic('))
        body += function(adapter, 'static e_status CIN_RunBink(')
        body += '\n'.join(function(source, name) for name in (
            'static void CL_PlaySTVEFSPInGameCinematic(', 'void CL_PlayInGameCinematic_f(',
            'qboolean CL_STVEFSPCinematicKey(', 'void SCR_RunCinematic(', 'void SCR_StopCinematic('))
        stvef_bink_tests.STVEFBinkTests().execute(body + checks)

    def test_movie_commands_pending_load_pause_completion_loop_hold_and_keys(self):
        self.lifecycle(CHECKS)

    def test_fades_freeze_decoder_mute_and_defer_completion_without_blocking_shutdown(self):
        self.lifecycle(r"""
int main(){
 using Result=fntech3::stvef::bink::Movie::Result;
 cls.state=CA_ACTIVE;cvars["cl_VidFadeUp"]="1";cvars["cl_VidFadeDown"]="1";cvars["s_volume"]="0.7";
 auto &row=cinTable[0];
 for(unsigned origin:{100u,0xfffffff0u}){
  now=static_cast<int>(origin);CL_PlayInGameCinematic_f();
  row.bink->result=Result::Frame;SCR_RunCinematic();
  assert(row.bink->paused&&row.bink->volume==0&&row.bink->steps==1&&row.binkPresentation.Alpha()==0);
  const auto *first=row.buf;
  for(unsigned elapsed:{1u,500u,999u}){
   now=static_cast<int>(origin+elapsed);SCR_RunCinematic();
   assert(row.buf==first&&row.bink->paused&&row.bink->steps==1&&row.bink->volume==0);
   assert(Cvar_VariableIntegerValue("cl_paused")==1&&CL_handle==0);
  }
  now=static_cast<int>(origin+1000);SCR_RunCinematic();
  assert(!row.bink->paused&&row.bink->steps==2&&row.bink->volume==0.7f&&row.binkPresentation.Alpha()==1);
  row.bink->result=Result::End;now=static_cast<int>(origin+2000);SCR_RunCinematic();
  assert(row.bink&&row.bink->volume==0&&row.binkPresentation.Alpha()==1&&row.buf==first);
  for(unsigned elapsed:{1u,500u,999u}){
   now=static_cast<int>(origin+2000+elapsed);SCR_RunCinematic();
   assert(row.bink->steps==3&&row.bink->volume==0&&Cvar_VariableIntegerValue("cl_paused")==1);
  }
  now=static_cast<int>(origin+3000);SCR_RunCinematic();
  assert(!row.bink&&CL_handle==-1&&!Cvar_VariableIntegerValue("cl_paused")&&cls.state==CA_ACTIVE);
 }
 // Forced stop releases the pause at every presentation phase.
 for(int phase=0;phase<3;++phase){
  now=100;CL_PlayInGameCinematic_f();row.bink->result=Result::Frame;SCR_RunCinematic();
  if(phase){now=1100;SCR_RunCinematic();}
  if(phase==2){row.bink->result=Result::End;SCR_RunCinematic();}
  SCR_StopCinematic();assert(!row.bink&&CL_handle==-1&&!Cvar_VariableIntegerValue("cl_paused"));
 }
 // Loop and hold extensions do not acquire a fade-out or a repeated fade-in.
 for(bool loop:{true,false}){
  now=100;CL_PlayInGameCinematic_f();row.bink->result=Result::Frame;SCR_RunCinematic();
  now=1100;SCR_RunCinematic();row.looping=loop;row.holdAtEnd=!loop;
  row.bink->result=Result::End;SCR_RunCinematic();
  assert(row.bink&&!row.binkPresentation.Ending()&&row.status==(loop?FMV_PLAY:FMV_IDLE));
  SCR_StopCinematic();row.looping=row.holdAtEnd=false;
 }
 // A failed decode never waits on a fade; neither does a movie with no frame.
 for(bool hasFrame:{true,false}){
  now=100;CL_PlayInGameCinematic_f();
  if(hasFrame){row.bink->result=Result::Frame;SCR_RunCinematic();now=1100;SCR_RunCinematic();row.bink->error="failed";}
  row.bink->result=Result::End;SCR_RunCinematic();assert(!row.bink&&CL_handle==-1);
 }
}
""")

    def test_renderer_movie_dimensions_remain_profile_specific(self):
        host = r'''
using byte=unsigned char;using qboolean=int;
constexpr int COM_GAME_STVEF_SP=6,ERR_DROP=1,PRINT_ALL=0;
static struct {int maxTextureSize=2048;}glConfig;
static struct {int integer=0;}speeds;
static auto *r_speeds=&speeds;
static struct {int gameId=0;int Milliseconds(){return 0;}
 void Error(int,const char *,...){throw 1;}void Printf(int,const char *,...){};}ri;
static struct Bundle{void *image[1];}bundle;
static struct Stage{Bundle bundle[1];}stage;
static struct Shader{Stage *stages[1]={&stage};int index=1;}shader;
static struct {bool registered=true;Shader *cinematicShader=&shader;void *scratchImage[16]{};}tr;
static int uploads,draws;
static void RE_UploadCinematic(int,int,int,int,const byte *,int,qboolean){++uploads;}
static void RE_StretchPic(int,int,int,int,float,float,float,float,int){++draws;}
'''
        checks = r'''
int main(){
 for(auto draw:{RE_StretchRaw_glx,RE_StretchRaw_vk,RE_StretchRaw_rtx}){
  for(int profile=0;profile<7;++profile){ri.gameId=profile;
   for(auto dims:std::vector<std::pair<int,int>>{{512,512},{512,384},{256,192},{1,1},{0,1},{1,0},{-1,512},{512,-1},{2049,1},{1,2049},{2147483647,1}}){
    uploads=draws=0;bool rejected=false;
    try{draw(0,0,640,480,dims.first,dims.second,nullptr,0,1);}catch(int){rejected=true;}
    const bool bounded=dims.first>0&&dims.second>0&&dims.first<=2048&&dims.second<=2048;
    const bool allowed=bounded&&(profile==6||((dims.first&(dims.first-1))==0&&(dims.second&(dims.second-1))==0));
    assert(rejected!=allowed&&uploads==int(allowed)&&draws==int(allowed));
   }
  }
  tr.registered=false;uploads=draws=0;draw(0,0,640,480,-1,-1,nullptr,0,1);assert(!uploads&&!draws);tr.registered=true;
 }
}
'''
        bodies = ''
        for folder, suffix in (('renderer', 'glx'), ('renderervk', 'vk'), ('rendererrtx', 'rtx')):
            text = (ROOT / 'code' / folder / 'tr_backend.c').read_text()
            bodies += function(text, 'void RE_StretchRaw(').replace('RE_StretchRaw(', f'RE_StretchRaw_{suffix}(')
        stvef_bink_tests.STVEFBinkTests().execute(host + bodies + checks)


if __name__ == '__main__':
    unittest.main()
