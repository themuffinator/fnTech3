"""Retail movie fade/crop vectors and the client drawing boundary."""
from pathlib import Path
import unittest

from rtcw_music_fixture import function
import stvef_bink_tests

ROOT = Path(__file__).resolve().parents[1]
HEADER = r"""
#include <cmath>
#include "code/compat/stvef_cinematic_presentation.hpp"
"""


class STVEFCinematicPresentationTests(unittest.TestCase):
    def test_retail_fade_vectors_and_clock_wrap(self):
        stvef_bink_tests.STVEFBinkTests().execute(HEADER + r"""
int main(){
 // Independently observed stvoy.exe 1.20 CIN_Draw (0x403790), clock callback.
 const unsigned times[]={0,1,250,500,999,1000,1001};
 const float up[]={0,.00100000005f,.25f,.5f,.999000072f,1,1};
 const float down[]={1,.999000013f,.75f,.49999997f,.00099995255f,0,0};
 for(unsigned origin:{0u,1234u,0xfffffff0u}){
  Presentation p;p.Open(true,true,0);assert(p.Alpha()==0&&p.Mute()&&!p.Pause());
  p.Update(origin+5000);assert(p.Alpha()==0); // Start on first frame, not open.
  p.Frame(origin);assert(p.Pause());
  for(unsigned i=0;i<7;++i){
   p.Update(origin+times[i]);assert(std::fabs(p.Alpha()-up[i])<.0000002f);
   assert(p.Pause()==(times[i]<1000)&&p.Mute()==p.Pause()&&!p.Ending());
  }
  p.Frame(origin+2000);assert(!p.Pause()&&p.Alpha()==1);
  assert(p.End(origin+3000)&&p.Ending());
  for(unsigned i=0;i<7;++i){
   p.Update(origin+3000+times[i]);assert(std::fabs(p.Alpha()-down[i])<.0000002f);
   assert(p.Done()==(times[i]>=1000)&&p.Mute());
  }
  p.Reset();assert(p.Alpha()==1&&!p.Pause()&&!p.Ending()&&!p.Mute());
 }
 for(bool up:{true,false})for(bool down:{true,false}){
  Presentation p;p.Open(up,down,0);p.Frame(10);p.Update(1010);
  assert(p.Alpha()==1&&!p.Mute()&&p.End(2000)==down);
  p.Reset();p.Open(up,down,0);assert(!p.End(20)&&p.Done()); // No valid frame.
 }
}
""")

    def test_central_crop_native_image_odd_dimensions_and_bounded_fallbacks(self):
        stvef_bink_tests.STVEFBinkTests().execute(HEADER + r"""
int main(){
 for(auto size:std::vector<std::pair<int,int>>{{640,480},{512,384},{256,192},{257,193},{2048,2048},{255,191},{1,1}}){
  const auto [w,h]=size;
  std::vector<std::uint32_t>source(w*h);for(int i=0;i<w*h;++i)source[i]=i;
  for(float quality:{-1.f,0.f,1.f,1.9f,2.f,3.f,std::numeric_limits<float>::quiet_NaN()}){
   Presentation p;p.Open(false,false,quality);
   const auto image=p.Pixels(reinterpret_cast<const std::uint8_t*>(source.data()),source.size()*4,w,h,false);
   const bool cropped=quality>=2&&w>=256&&h>=192;
   if(!cropped){assert(image.pixels==reinterpret_cast<const std::uint8_t*>(source.data())&&image.width==w&&image.height==h&&image.heightScale==1);continue;}
   assert(image.width==256&&image.height==256&&image.heightScale==4.f/3);
   for(int y=0;y<256;++y)for(int x=0;x<256;++x){
    std::uint32_t value;memcpy(&value,image.pixels+(y*256+x)*4,4);
    const auto expected=y<192?source[(y+h/2-96)*w+x+w/2-128]:0;
    assert(value==expected);
   }
   std::uint32_t old;memcpy(&old,image.pixels,4);
   ++source[(h/2-96)*w+w/2-128];
   auto cached=p.Pixels(reinterpret_cast<const std::uint8_t*>(source.data()),source.size()*4,w,h,false);
   std::uint32_t value;memcpy(&value,cached.pixels,4);assert(value==old);
   cached=p.Pixels(reinterpret_cast<const std::uint8_t*>(source.data()),source.size()*4,w,h,true);
   memcpy(&value,cached.pixels,4);assert(value==old+1);
   --source[(h/2-96)*w+w/2-128];
   p.Reset();p.Open(false,false,0);
   assert(p.Pixels(reinterpret_cast<const std::uint8_t*>(source.data()),source.size()*4,w,h,true).pixels==
          reinterpret_cast<const std::uint8_t*>(source.data()));
  }
 }
 Presentation p;p.Open(false,false,2);std::uint8_t pixel[4]{};
 for(auto dims:std::vector<std::pair<int,int>>{{0,1},{1,0},{-1,1},{1,-1},{2049,1},{1,2049},{2147483647,2147483647}}){
  assert(!p.Pixels(pixel,4,dims.first,dims.second,true).pixels);
 }
 assert(!p.Pixels(nullptr,4,1,1,true).pixels&&!p.Pixels(pixel,3,1,1,true).pixels);
 assert(!p.Pixels(pixel,4,640,480,true).pixels);
}
""", sanitize=True)

    def test_draw_uploads_crop_once_and_restores_overlay_colour(self):
        host = r"""
using byte=unsigned char;using vec4_t=float[4];
static bool ToQboolean(bool value){return value;}
static struct Row {
 Presentation binkPresentation;byte *buf=nullptr;
 int CIN_WIDTH=640,CIN_HEIGHT=480;bool dirty=true;
}cinTable[1];
static struct {int whiteShader=7;}cls;
static std::string calls;
static struct {
 float x,y,w,h,alpha=0;int cols=0,rows=0;bool dirty=false;
 void DrawStretchRaw(float X,float Y,float W,float H,int C,int R,const byte *pixels,int handle,bool D){
  assert(pixels&&handle==0);calls+='R';x=X;y=Y;w=W;h=H;cols=C;rows=R;dirty=D;
 }
 void SetColor(const float *c){calls+=c?'C':'N';if(c){assert(c[0]==0&&c[1]==0&&c[2]==0);alpha=c[3];}}
 void DrawStretchPic(float X,float Y,float W,float H,float s,float t,float u,float v,int shader){
  calls+='P';assert(X==x&&Y==y&&W==w&&H==480&&s==0&&t==0&&u==1&&v==1&&shader==7);
 }
}re;
"""
        checks = r"""
int main(){
 auto &m=cinTable[0];std::vector<byte>pixels(640*480*4,255);m.buf=pixels.data();
 for(float quality:{0.f,1.f,2.f}){
  m.binkPresentation.Open(true,true,quality);m.binkPresentation.Frame(0);m.dirty=true;
  for(unsigned time:{0u,500u,1000u}){
   m.binkPresentation.Update(time);calls.clear();CIN_DrawBink(0,3,5,640,480);
   assert(calls==(time<1000?"RCPN":"R")&&re.dirty==(time==0)&&!m.dirty&&re.x==3&&re.y==5&&re.w==640);
   assert(re.cols==(quality>=2?256:640)&&re.rows==(quality>=2?256:480));
   assert(re.h==(quality>=2?640:480));
   // These sampled endpoints/midpoints are exact binary fractions. Do not
   // compare the stored draw colour with an unrounded x87 expression.
   if(time<1000)assert(re.alpha==(time==0?1.0f:0.5f));
  }
  m.binkPresentation.End(2000);m.binkPresentation.Update(2500);calls.clear();CIN_DrawBink(0,3,5,640,480);
  assert(calls=="RCPN"&&re.alpha==.5f&&!re.dirty);
 }
 m.buf=nullptr;m.dirty=true;calls.clear();CIN_DrawBink(0,0,0,640,480);assert(calls.empty()&&m.dirty);
}
"""
        draw = function((ROOT / 'code/client/client_stvef_sp_cinematic.hpp').read_text(),
                        'static void CIN_DrawBink(')
        stvef_bink_tests.STVEFBinkTests().execute(HEADER + host + draw + checks)

    def test_screen_and_shader_movies_select_presentation_at_open(self):
        host = r"""
#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
constexpr int MAX_OSPATH=256,CIN_silent=1,CIN_loop=2,CIN_hold=4,CIN_system=8,CIN_shader=16;
constexpr int FMV_PLAY=1,FMV_EOF=2,UI_SET_ACTIVE_MENU=1,UIMENU_NONE=0,CA_CINEMATIC=3;
static int quality=2,fadeUp=1,fadeDown=1;
static int CIN_HandleForVideo(){return 0;}
static void Q_strncpyz(char *to,const char *from,int length){std::snprintf(to,length,"%s",from);}
static void COM_StripExtension(const char *from,char *to,int size){Q_strncpyz(to,from,size);*strrchr(to,'.')=0;}
static void Com_sprintf(char *out,int size,const char *format,const char *stem){std::snprintf(out,size,format,stem);}
static int Q_stricmp(const char *a,const char *b){return strcmp(a,b);}
static int Cvar_VariableIntegerValue(const char *){return 0;}
static float Cvar_VariableValue(const char *name){
 if(!strcmp(name,"cl_VideoQuality"))return quality;
 if(!strcmp(name,"cl_VidFadeUp"))return fadeUp;
 assert(!strcmp(name,"cl_VidFadeDown"));return fadeDown;
}
static const char *Cvar_VariableString(const char *name){return !strcmp(name,"fs_basepath")?".":"ENGLISH";}
static void Com_Printf(const char *,...){}
static void Con_Close(){}
static int Sys_Milliseconds(){return 100;}
static void VM_Call(void *,int,int,int){}
namespace fntech3::stvef::bink {
struct Movie {FileInfo Info(){return {640,480,1,30,1,0,0};}bool Sound(){return false;}};
}
static std::unique_ptr<Movie> CIN_OpenBink(const char *,const char *,bool,bool){return std::make_unique<Movie>();}
static struct Row {
 std::unique_ptr<Movie>bink;Presentation binkPresentation;std::array<char,256>fileName{};
 unsigned char *buf=nullptr;int status,startTime,CIN_WIDTH,CIN_HEIGHT,drawX,drawY,xpos,ypos,width,height,playonwalls;
 bool looping,holdAtEnd,alterGameState,silent,shader,rtcwLetterBox,stvefInGame,dirty;
}cinTable[1];
static void *uivm=nullptr;
static struct {int state=0;}cls;
static int CIN_RunBink(int){return FMV_PLAY;}
"""
        checks = r"""
int main(){
 std::vector<std::uint8_t> pixels(640*480*4);
 for(bool shader:{false,true})for(int up:{0,1})for(int down:{0,1}){
  fadeUp=up;fadeDown=down;
  assert(CIN_PlayBink("video/test.bik",0,0,640,480,shader?CIN_shader:0)==0);
  auto &m=cinTable[0];auto &p=m.binkPresentation;
  assert(m.shader==shader&&m.status==FMV_PLAY&&m.startTime==100);
  p.Frame(100);assert(p.Pause()==(!shader&&up));
  auto image=p.Pixels(pixels.data(),pixels.size(),640,480,true);
  assert(image.width==(shader?640:256));
  p.Update(1100);assert(p.End(2000)==(!shader&&down));
 }
}
"""
        play = function((ROOT / 'code/client/client_stvef_sp_cinematic.hpp').read_text(),
                        'static int CIN_PlayBink(')
        stvef_bink_tests.STVEFBinkTests().execute(HEADER + host + play + checks)


if __name__ == '__main__':
    unittest.main()
