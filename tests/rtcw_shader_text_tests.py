"""Exercise each renderer's shader-file loader with the real engine tokenizer."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
static char com_token[MAX_TOKEN_CHARS],com_parsename[MAX_TOKEN_CHARS];
static int com_lines,com_tokenline;
int QDECL Com_sprintf(char *out,int size,const char *format,...) {
    va_list args;va_start(args,format);int n=std::vsnprintf(out,size,format,args);va_end(args);return n;
}
void Q_strncpyz(char *out,const char *in,int size) {assert(size>0);std::snprintf(out,size,"%s",in);}
static std::map<std::string,std::string> files;
static int allocations=0;
static int Read(const char *name,void **out) {
    auto found=files.find(name);*out=nullptr;if(found==files.end())return -1;
    const auto &text=found->second;*out=new char[text.size()+1];++allocations;
    std::memcpy(*out,text.c_str(),text.size()+1);return int(text.size());
}
static void Free(void *p){assert(p && allocations>0);delete[] static_cast<char *>(p);--allocations;}
static void QDECL Print(int,const char *,...){}
[[noreturn]] static void QDECL Error(int,const char *,...){throw 1;}
static struct {decltype(&Read) FS_ReadFile=Read;decltype(&Free) FS_FreeFile=Free;
    decltype(&Print) Printf=Print;decltype(&Error) Error=::Error;comGameId_t gameId=COM_GAME_QUAKE3;}ri;
'''


def tokenizer():
    common = (ROOT / 'code/qcommon/q_shared.c').read_text()
    source = '\n'.join(function(common, signature) for signature in (
        'void COM_BeginParseSession(', 'int COM_GetCurrentParseLine(',
        'static const char *SkipWhitespace(', 'const char *COM_ParseExt(',
        'int COM_Compress(', 'qboolean SkipBracedSection(', 'void SkipRestOfLine('))
    # The C tokenizer returns an int expression as a qboolean enum; C++ needs
    # an explicit enum value. No tokenization or brace-counting changes.
    return source.replace('return ( depth == 0 );', 'return depth == 0 ? qtrue : qfalse;')


CHECKS = r'''
static std::vector<std::string> Names(const char *text) {
    std::vector<std::string> names;
    while(text) {
        const auto *token=COM_ParseExt(&text,qtrue);if(!*token)break;
        names.emplace_back(token);assert(names.back()!="}" && names.back()!="{");
        token=COM_ParseExt(&text,qtrue);assert(!std::strcmp(token,"{"));
        assert(SkipBracedSection(&text,1));
    }
    return names;
}
static std::vector<std::string> Load(const std::string &input) {
    files["scripts/test.shader"]=input;char name[]="test.shader";char *names[]{name};char *buffers[]{nullptr};
    int length=loadShaderBuffers(names,1,buffers);
    assert(length>=0 && length<=int(input.size()));
    std::vector<std::string> result;
    if(buffers[0]) {assert(int(std::strlen(buffers[0]))==length);result=Names(buffers[0]);Free(buffers[0]);}
    assert(allocations==0);return result;
}
int main() {
    const std::string first="/* { ignored } */\nmenu/first\n{\n nopicmip\n { map image/a.tga }\n}\n";
    const std::string second="menu/second\n{\n { map \"image/with spaces.tga\" }\n}\n";
    for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
        ri.gameId=profile;
        assert(Load(first+second)==std::vector<std::string>({"menu/first","menu/second"}));
        assert(Load("").empty());
        assert(Load("// only a comment\n").empty());
        assert(Load(first+first)==std::vector<std::string>({"menu/first","menu/first"}));
        const bool rtcw=profile==COM_GAME_RTCW_SP || profile==COM_GAME_RTCW_MP;
        assert(Load(first+second+"}\n}\n")== (rtcw?std::vector<std::string>({"menu/first","menu/second"}):std::vector<std::string>{}));
        assert(Load(first+"unfinished { { map image/b.tga }")== (rtcw?std::vector<std::string>({"menu/first"}):std::vector<std::string>{}));
        if(rtcw) {
            assert(Load("stray\n}\n"+first+"{ { } }\n"+second)==std::vector<std::string>({"menu/first","menu/second"}));
            assert(Load("broken-name\n"+second)==std::vector<std::string>({"menu/second"}));
            assert(Load(first+"/* unterminated")==std::vector<std::string>({"menu/first"}));
            assert(Load("shader { /* missing close").empty());
            assert(Load("shader { \"unterminated").empty());
            assert(Load("{ } } bare").empty());
            // Retained text is byte-for-byte, including quoted values/comments.
            auto copy=first+second;const auto expected=copy;
            int size=R_RTCWNormalizeShaderText(copy.data(),"direct.shader");
            assert(copy.substr(0,size)==expected.substr(0,expected.find_last_of('}')+1));
            // Repeated malformed tails cannot contaminate the next file.
            for(int i=0;i<128;++i) {
                assert(Load(first+std::string(i,'}')+"\n{ ")==std::vector<std::string>({"menu/first"}));
                assert(Load(second)==std::vector<std::string>({"menu/second"}));
            }
        }
    }
}
'''


class RTCWShaderTextTests(unittest.TestCase):
    def test_profile_loader_keeps_complete_definitions(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler: self.skipTest('C++ compiler unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags: flags += ['-DFNTECH3_Q3_ONLY']
        base = PRELUDE + tokenizer() + '\n#include "code/renderercommon/tr_rtcw_shader_text.h"\n#include "code/renderercommon/tr_q3_ihv_shader_text.h"\n'
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-shaders-', dir=ROOT / '.tmp') as temporary:
            for renderer in ('renderer', 'renderervk', 'rendererrtx'):
                with self.subTest(renderer=renderer):
                    source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
                    cpp = Path(temporary) / 'test.cpp'
                    executable = Path(temporary) / ('test.exe' if os.name == 'nt' else 'test')
                    cpp.write_text(base + function(source, 'static int loadShaderBuffers(') + CHECKS)
                    build = subprocess.run([compiler, *flags, '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(executable)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__': unittest.main()
