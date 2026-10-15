"""Shared menu viewport scaling rejects overflow before publishing coordinates."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = r'''
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
struct Drop {};
enum { ERR_DROP };
[[noreturn]] static void Com_Error(int, const char *) { throw Drop{}; }
struct refdef_t { int x,y,width,height; };
struct { struct { int vidWidth,vidHeight; } glconfig; float scale,biasX,biasY; } cls;
struct Cvar { int integer; } aspect;
static Cvar *cl_menuAspect=&aspect;
'''
CHECKS = r'''
template<class F> static void Reject(F action) {
 bool caught=false;try{action();}catch(const Drop&){caught=true;}assert(caught);
}
static float Encoded(uint32_t bits) {float value;std::memcpy(&value,&bits,4);return value;}
int main() {
 for(float f : {0.f,1.4f,1.5f,-1.5f,640.f,2160.f})
  assert(RoundToInt(f)==static_cast<int>(f+0.5f));
 for(float f : {Encoded(0x7f800000u),Encoded(0xff800000u),
     Encoded(0x7fc00001u),float(INT_MAX),float(INT_MIN)*2})
  Reject([&]{RoundToInt(f);});
 assert(RoundToInt(-2147483648.f)==INT_MIN);
 assert(RoundToInt(2147483520.f)==2147483520);
 for(int aspectMode : {0,1}) for(int screen : {640,1920,3440}) {
  aspect.integer=aspectMode;cls.glconfig={screen,480};cls.scale=1;cls.biasX=(screen-640)*0.5f;
  refdef_t view{int(cls.biasX)+10,20,320,200};
  CL_UIAdjustRefdef(&view);
  if(aspectMode) assert(view.x==int(cls.biasX)+10&&view.width==320);
  else assert(view.x==RoundToInt(10*(screen/640.f))&&view.width==RoundToInt(320*(screen/640.f)));
  assert(view.y==20&&view.height==200);
 }
 aspect.integer=0;cls.glconfig={640,480};cls.scale=1;cls.biasX=0;
 for(refdef_t view : {refdef_t{0,0,INT_MAX,480},refdef_t{1000000000,0,1200000000,480}}) {
  const auto before=view;Reject([&]{CL_UIAdjustRefdef(&view);});assert(!std::memcmp(&view,&before,sizeof(view)));
 }
 cls.glconfig={3440,1440};cls.scale=3;cls.biasX=760;
 refdef_t view{760,0,1800000000,480};const auto before=view;
 Reject([&]{CL_UIAdjustRefdef(&view);});assert(!std::memcmp(&view,&before,sizeof(view)));
 cls.scale=std::numeric_limits<float>::denorm_min();
 Reject([&]{CL_UIAdjustRefdef(&view);});assert(!std::memcmp(&view,&before,sizeof(view)));
 // Raven's menus draw in a virtual 640x480 screen: stretched over the
 // window as the originals did (0), or in the centred 4:3 area (1).
 // Fast-math may divide by a reciprocal, so compare within a thousandth.
 const auto near=[](float a,float b){return std::fabs(a-b)<0.001f;};
 for(int aspectMode : {0,1}) {
  aspect.integer=aspectMode;cls.glconfig={1680,720};cls.scale=1.5f;cls.biasX=360;cls.biasY=0;
  float x=10,y=20,w=100,h=40;CL_UIFromVirtual(&x,&y,&w,&h);
  if(aspectMode) assert(near(x,375)&&near(y,30)&&near(w,150)&&near(h,60));
  else assert(near(x,26.25f)&&near(y,30)&&near(w,262.5f)&&near(h,60));
 }
 aspect.integer=1;cls.glconfig={900,720};cls.scale=1.40625f;cls.biasX=0;cls.biasY=22.5f;
 float x=0,y=0,w=640,h=480;CL_UIFromVirtual(&x,&y,&w,&h);
 assert(near(x,0)&&near(y,22.5f)&&near(w,900)&&near(h,675)); // taller than 4:3: centred top to bottom
}
'''


class UIViewportTests(unittest.TestCase):
    def test_actual_shared_scaling(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        ui = (ROOT / 'code/client/cl_ui.cpp').read_text()
        source = PRELUDE + '#include <initializer_list>\n'
        for name in ('static int RoundToInt(', 'static void CL_UIAdjustStretchPic(', 'static void CL_UIAdjustRefdef(',
                     'static void CL_UIFromVirtual('):
            source += function(ui, name)
        source += CHECKS
        scratch = ROOT / '.tmp/tests/ui-viewport'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            directory = Path(temporary).resolve()
            cpp = directory / 'viewport.cpp'
            cpp.write_text(source)
            for extra in ([], ['-O2', '-ffast-math']):
                executable = directory / ('viewport.exe' if os.name == 'nt' else 'viewport')
                built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', *extra,
                    '-Werror', str(cpp), '-o', str(executable)], cwd=directory, capture_output=True, text=True, timeout=90)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                ran = subprocess.run([str(executable)], cwd=directory, capture_output=True, text=True, timeout=30)
                self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
