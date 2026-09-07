"""Run each renderer's light RGB parser with the engine tokenizer."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
def extract(s,signature):
    start=s.index(signature)
    return s[start:s.index('\n}',start)+2]
class ShaderLightRGBTests(unittest.TestCase):
    def test_renderer_color_syntax_and_recovery(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('C++ compiler unavailable')
        common=(ROOT/'code/qcommon/q_shared.c').read_text()
        prelude=r'''
#include <cassert>
#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <algorithm>
using qboolean=int;
using vec3_t=float[3];
constexpr int qfalse=0,qtrue=1,PRINT_WARNING=1;
#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))
#define MAX(a,b) std::max(a,b)
#define VectorScale(a,s,b) do { for(int j=0;j<3;++j)(b)[j]=(a)[j]*(s); } while(0)
float Com_Clamp(float lo,float hi,float v) {return std::max(lo,std::min(hi,v));}
char com_token[1024];int com_lines,com_tokenline,warnings;
struct {const char *name="test";float surfaceLightColor[3];bool surfaceLightColorValid;} shader;
void warn(int,const char*,...) {++warnings;}
struct {decltype(&warn) Printf=warn;} ri;
'''+extract(common,'static const char *SkipWhitespace(')+'\n'+extract(common,'const char *COM_ParseExt(')
        main=r'''
int main() {
 for(const char *s : {"1 .5 0 next", "( 1 .5 0 ) next", "2 1 -1 next", "( 2 1 -1 ) next"}) {
  shader.surfaceLightColorValid=false;warnings=0;const char *p=s;
  ParseQ3MapLightRGB(&p);
  assert(shader.surfaceLightColorValid && !warnings);
  assert(shader.surfaceLightColor[0]==1 && shader.surfaceLightColor[1]==.5 && shader.surfaceLightColor[2]==0);
  assert(!strcmp(COM_ParseExt(&p,qtrue),"next"));
 }
 for(const char *s : {"", "1 2", "1 2\nnext", "1 } 0", "nan 0 0", "inf 1 1", "1e999 1 1", "1junk 2 3", "( 1 2 3 next", "( 1 2 )", "1 2 // missing\n next"}) {
  shader.surfaceLightColorValid=true;
  for(float &v:shader.surfaceLightColor)v=.25;
  warnings=0;const char *p=s;ParseQ3MapLightRGB(&p);
  assert(warnings==1 && p==s && shader.surfaceLightColorValid);
  for(float v:shader.surfaceLightColor)assert(v==.25);
 }
 shader.surfaceLightColorValid=false;
 const char *p="0 0 0 next";ParseQ3MapLightRGB(&p);
 assert(!shader.surfaceLightColorValid && !strcmp(COM_ParseExt(&p,qtrue),"next"));
}
'''
        with tempfile.TemporaryDirectory() as d:
            for folder in ('renderer','renderervk','rendererrtx'):
                with self.subTest(renderer=folder):
                    source=(ROOT/'code'/folder/'tr_shader.c').read_text()
                    cpp=Path(d)/'test.cpp';exe=Path(d)/'test.exe'
                    cpp.write_text(prelude+'\n'+extract(source,'static void ParseQ3MapLightRGB(')+'\n'+main)
                    subprocess.run([cc,*compiler_target_args(cc),'-std=c++17',str(cpp),'-o',str(exe)],check=True,capture_output=True,text=True)
                    subprocess.run([str(exe)],check=True,capture_output=True,text=True)
if __name__=='__main__':unittest.main()
