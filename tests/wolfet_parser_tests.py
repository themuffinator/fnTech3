"""Execute parser location/unread and latched-cvar boundaries with sentinels."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    begin = source.index(signature)
    return source[begin:source.index('\n}', begin) + 2]


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include <cassert>
#include <cstring>
#include <string>
constexpr int MAX_SOURCEFILES = 3;
struct token_t {
    char string[1024]; int type,subtype,intvalue; float floatvalue;
    int line,linescrossed;
};
struct source_t { token_t token; } source;
source_t *sourceFiles[MAX_SOURCEFILES] = {nullptr,&source,nullptr};
static token_t next{};
static int available = 1, reads = 0, unread = 0;
int PC_ReadToken(source_t *s,token_t *t) {
    ++reads;if(!available)return 0;*t=next;s->token=next;return 1;
}
void PC_UnreadLastToken(source_t *s) {assert(s==&source);++unread;}
void StripDoubleQuotes(char *s) {
    const size_t n=strlen(s);assert(n>=2&&s[0]=='"'&&s[n-1]=='"');
    memmove(s,s+1,n-2);s[n-2]=0;
}
static cvar_t cvar{};
static cvar_t *Cvar_FindVar(const char *s){return strcmp(s,"known")?nullptr:&cvar;}
void Q_strncpyz(char *dst,const char *src,int size){
    assert(size>0);strncpy(dst,src,size-1);dst[size-1]=0;
}
'''

CHECKS = r'''
int main() {
    strcpy(next.string,"\"token\"");next.type=TT_STRING;next.subtype=9;
    next.intvalue=23;next.floatvalue=2.5f;next.line=37;next.linescrossed=4;
    struct {uint32_t before;pc_token_t token;uint32_t after;} out{};
    out.before=0x12345678;out.after=0x87654321;
    int line=-1,crossed=-1;
    assert(PC_ReadTokenWithLocation(1,&out.token,&line,&crossed));
    assert(line==37&&crossed==4&&!strcmp(out.token.string,"token"));
    assert(out.token.type==1&&out.token.subtype==9&&out.token.intvalue==23&&out.token.floatvalue==2.5f);
    auto saved=out;
    assert(!PC_ReadTokenWithLocation(-1,&out.token,&line,&crossed));
    assert(!PC_ReadTokenWithLocation(0,&out.token,&line,&crossed));
    assert(!PC_ReadTokenWithLocation(2,&out.token,&line,&crossed));
    assert(!PC_ReadTokenWithLocation(3,&out.token,&line,&crossed));
    assert(!PC_ReadTokenWithLocation(1,nullptr,&line,&crossed));
    assert(!PC_ReadTokenWithLocation(1,&out.token,nullptr,&crossed));
    assert(!PC_ReadTokenWithLocation(1,&out.token,&line,nullptr));
    assert(reads==1&&!memcmp(&saved,&out,sizeof(out)));
    available=0;
    assert(!PC_ReadTokenWithLocation(1,&out.token,&line,&crossed));
    assert(line==37&&crossed==4&&!memcmp(&saved,&out,sizeof(out)));
    assert(!PC_UnreadTokenHandle(-1)&&!PC_UnreadTokenHandle(0)&&!PC_UnreadTokenHandle(2)&&!PC_UnreadTokenHandle(3));
    assert(PC_UnreadTokenHandle(1)&&unread==1);
    char value[]="current",pending[]="next",buffer[12]{};
    cvar.string=value;cvar.latchedString=pending;
    Cvar_LatchedVariableStringBufferSafe("known",buffer,12,0);assert(!strcmp(buffer,"next"));
    cvar.latchedString=nullptr;
    Cvar_LatchedVariableStringBufferSafe("known",buffer,12,0);assert(!strcmp(buffer,"current"));
    memset(buffer,'X',sizeof(buffer));
    Cvar_LatchedVariableStringBufferSafe("known",buffer,4,0);assert(!strcmp(buffer,"cur")&&buffer[4]=='X');
    cvar.flags=0x80;
    Cvar_LatchedVariableStringBufferSafe("known",buffer,12,0x80);assert(!buffer[0]);
    buffer[0]='X';Cvar_LatchedVariableStringBufferSafe("missing",buffer,12,0);assert(!buffer[0]);
    buffer[0]='X';Cvar_LatchedVariableStringBufferSafe("known",buffer,0,0);assert(buffer[0]=='X');
    Cvar_LatchedVariableStringBufferSafe("known",nullptr,12,0);
}
'''


class WolfETParserTests(unittest.TestCase):
    def test_parser_and_latched_cvar_boundaries(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        parser = (ROOT/'code/botlib/l_precomp.cpp').read_text(encoding='utf-8')
        cvar = (ROOT/'code/qcommon/cvar.c').read_text(encoding='utf-8')
        bodies = '\n'.join(function(parser, signature) for signature in (
            'int PC_ReadTokenHandle(', 'int PC_ReadTokenWithLocation(', 'int PC_UnreadTokenHandle('))
        bodies += '\n' + function(cvar, 'void Cvar_LatchedVariableStringBufferSafe(')
        scratch = ROOT/'.tmp/wolfet'
        scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source, executable = folder/'probe.cpp', folder/('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(PRELUDE+bodies+CHECKS,encoding='utf-8')
            target = compiler_target_args(compiler)
            if not target:
                target = ['-DFNTECH3_Q3_ONLY'] if os.name == 'nt' else ['-m32']
            built = subprocess.run([compiler,*target,'-std=c++17',
                '-D_CRT_SECURE_NO_WARNINGS','-I',str(ROOT),str(source),'-o',str(executable)],
                cwd=ROOT,capture_output=True,text=True,timeout=90)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run = subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=20)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)


if __name__ == '__main__':
    unittest.main()
