"""Execute production Com_Error's recursive diagnostic and normal unwind."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#define DEDICATED
#define NORETURN
#define FORMAT_PRINTF(a,b)
#define QDECL
#define Q_longjmp longjmp
typedef int qboolean;
enum { qfalse, qtrue };
typedef enum { ERR_FATAL, ERR_DROP, ERR_DISCONNECT, ERR_SERVERDISCONNECT,
    ERR_NEED_CD, ERR_ENDGAME } errorParm_t;
static struct { uint32_t before;char text[64];uint32_t after; } message={0x12345678,"",0x87654321};
#define com_errorMessage message.text
static qboolean com_errorEntered;
static struct Cvar { int integer; } *com_buildScript;
static jmp_buf abortframe;
static int scenario, cancelled, codeWrites, nativeUnlinks, cbufInitializations;
static int forcedStarts, forcedEnds, serverStops, redirects, pureResets, shutdowns, sysErrors;
static char systemMessage[1024];
void Com_Error(errorParm_t,const char *,...);
void Sys_CancelRelaunch(void) { ++cancelled; }
void Cvar_SetIntegerValue(const char *name,int code) { assert(!strcmp(name,"com_errorCode") && code==ERR_DROP);++codeWrites; }
void Cvar_Set(const char *name,const char *value) { assert(!strcmp(name,"com_errorMessage") && !strcmp(value,"first error 12")); }
int Sys_Milliseconds(void) { return 1000; }
int Q_vsnprintf(char *out,size_t size,const char *format,va_list args) { return vsnprintf(out,size,format,args); }
int SV_ShouldErrorExit(errorParm_t code) { assert(code==ERR_DROP);return 0; }
int VM_UnlinkNativeExceptionRecords(void) { ++nativeUnlinks;return 0; }
void Com_DPrintf(const char *format,...) { (void)format; }
void Com_Printf(const char *format,...) { (void)format; }
void Cbuf_Init(void) { ++cbufInitializations; }
int Com_GameIsSTVEFSP(void) { return 0; }
int Com_GameIsRitual(void) { return 0; }
void SV_STVEFSPSaveAbort(void) { assert(0); }
void SV_AliceSaveAbort(void) { assert(0); }
void VM_Forced_Unload_Start(void) { ++forcedStarts; }
void VM_Forced_Unload_Done(void) { ++forcedEnds; }
const char *va(const char *format,...) {
    static char text[256];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);return text;
}
void SV_Shutdown(const char *text) {
    ++serverStops;assert(strstr(text,"first error 12"));
    if(scenario==1) Com_Error(ERR_DROP,"second cause %d %s",37,"nested");
    if(scenario==2) { char huge[300];memset(huge,'X',sizeof(huge)-1);huge[sizeof(huge)-1]=0;Com_Error(ERR_DROP,"%s",huge); }
    if(scenario==3) Com_Error(ERR_DROP,"aliased %s",com_errorMessage);
}
void Com_EndRedirect(void) { ++redirects; }
void FS_PureServerSetLoadedPaks(const char *first,const char *second) { assert(!first[0] && !second[0]);++pureResets; }
void Com_Shutdown(void) { ++shutdowns; }
void Sys_Error(const char *format,...) {
    va_list args;va_start(args,format);vsnprintf(systemMessage,sizeof(systemMessage),format,args);va_end(args);
    ++sysErrors;longjmp(abortframe,2);
}
'''

CHECKS = r'''
int main(int argc,char **argv) {
    int result;assert(argc==2);scenario=atoi(argv[1]);
    result=setjmp(abortframe);
    if(!result) Com_Error(ERR_DROP,"first error %d",12);
    assert(!strcmp(com_errorMessage,"first error 12"));
    assert(message.before==0x12345678 && message.after==0x87654321);
    assert(codeWrites==1 && nativeUnlinks==1 && cbufInitializations==1 && serverStops==1 && forcedStarts==1 && shutdowns==0);
    if(!scenario) {
        assert(result==1 && cancelled==1 && !com_errorEntered && !sysErrors);
        assert(forcedEnds==1 && redirects==1 && pureResets==1);
    } else {
        assert(result==2 && cancelled==2 && com_errorEntered && sysErrors==1);
        assert(!forcedEnds && !redirects && !pureResets);
        assert(strstr(systemMessage,"recursive error after: first error 12\nrecursive cause: "));
        if(scenario==1) assert(strstr(systemMessage,"recursive cause: second cause 37 nested"));
        if(scenario==2) {
            const char *cause=strstr(systemMessage,"recursive cause: ")+strlen("recursive cause: ");
            assert(strlen(cause)==sizeof(com_errorMessage)-1);
            for(size_t i=0;i<strlen(cause);++i) assert(cause[i]=='X');
        }
        if(scenario==3) assert(strstr(systemMessage,"recursive cause: aliased first error 12"));
    }
    return 0;
}
'''


class RecursiveErrorTests(unittest.TestCase):
    def test_first_error_is_preserved_and_second_is_bounded(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        source = (ROOT / 'code/qcommon/common.c').read_text(encoding='utf-8')
        start = source.index('void NORETURN FORMAT_PRINTF(2, 3) QDECL Com_Error(')
        body = source[start:source.index('\n}', start) + 2]
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='recursive-error-', dir=scratch) as folder:
            path = Path(folder)
            fixture = path / 'error.c'
            executable = path / ('error.exe' if os.name == 'nt' else 'error')
            fixture.write_text(PRELUDE + body + CHECKS, encoding='utf-8')
            build = subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                *compiler_target_args(compiler), str(fixture), '-o', str(executable)],
                text=True, capture_output=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            # Each process starts production's static recursion latch afresh.
            for scenario in range(4):
                with self.subTest(scenario=scenario):
                    result = subprocess.run([str(executable), str(scenario)],
                        text=True, capture_output=True, timeout=15)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
