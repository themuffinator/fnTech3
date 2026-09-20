"""Cinematic owners must release files before filesystem/static teardown."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_music_fixture import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include <array>
#include <cassert>
#include <memory>
#include <utility>
#include <vector>
constexpr int MAX_VIDEO_HANDLES=16;
using fileHandle_t=int;
constexpr int FS_INVALID_HANDLE=0;
static bool filesystemLive=true;
static bool files[32];static int closed;
static void FS_FCloseFile(int file){assert(filesystemLive&&file>0&&file<32&&files[file]);files[file]=false;++closed;}
static std::vector<int> order;
static void CL_ShutdownCGame(){order.push_back(1);}
static void CL_ShutdownUI(){order.push_back(2);}
static void Com_DPrintf(const char *,...){}
static int nextmaps;
static void CL_RunCinematicNextmap(){++nextmaps;}
static int paused, binkClosed;
static void Cvar_Set(const char *name,const char *value){assert(!strcmp(name,"cl_paused"));paused=atoi(value);}
'''

HOST = r'''
struct Movie{
 std::array<char,256>fileName{};ScopedFileHandle iFile;
 unsigned char *buf=nullptr;e_status status=FMV_IDLE;
 bool dirty=false,alterGameState=false;
 bool stvefInGame=false;
 struct Decoder {~Decoder(){++binkClosed;}};
 std::unique_ptr<Decoder> bink;
};
static std::array<Movie,MAX_VIDEO_HANDLES> cinTable;
static int currentHandle=-1,CL_handle=-1;
static char stvefPendingMovie[MAX_QPATH];
static struct{int currentHandle=-1;}cin;
static struct{int state=CA_ACTIVE;}cls;
static e_status CIN_StopCinematic(int);
'''

CHECKS = r'''
static void Open(int index,int handle,e_status status,bool frame,bool system=false,bool named=true){
 auto &movie=cinTable[index];files[handle]=true;movie.iFile.reset(handle);
 movie.status=status;movie.buf=frame?reinterpret_cast<unsigned char *>(1):nullptr;
 movie.fileName[0]=named?'x':0;movie.dirty=true;movie.alterGameState=system;
}
int main(){
 Open(0,1,FMV_PLAY,true);Open(1,2,FMV_PLAY,false);
 Open(2,3,FMV_PLAY,true,true); // A system movie after leaving cinematic state.
 Open(3,4,FMV_EOF,true);Open(4,5,FMV_IDLE,false,false,false);
 currentHandle=cin.currentHandle=CL_handle=2;
 CL_ShutdownVMs();assert((order==std::vector<int>{1,2}));
 assert(closed==5&&nextmaps==0&&currentHandle==-1&&cin.currentHandle==-1&&CL_handle==-1);
 for(const auto &movie:cinTable)assert(!movie.iFile&&!movie.fileName[0]&&!movie.buf&&movie.status==FMV_IDLE&&!movie.dirty);
 CIN_CloseAllVideos();assert(closed==5); // Idempotent after module cleanup.
 cls.state=CA_CINEMATIC;Open(0,6,FMV_PLAY,true,true);
 CIN_CloseAllVideos();assert(closed==6&&nextmaps==1&&cls.state==CA_DISCONNECTED);
 // Individual failures before the first frame and held/EOF movies release too.
 for(e_status status:{FMV_PLAY,FMV_EOF,FMV_IDLE}){
  const int before=closed;Open(0,7,status,false);CIN_StopCinematic(0);
  assert(closed==before+1&&!cinTable[0].iFile&&!cinTable[0].fileName[0]);
 }
 cls.state=CA_CINEMATIC;Open(0,8,FMV_EOF,false,true);currentHandle=0;
 RoQShutdown();assert(closed==10&&nextmaps==2&&cls.state==CA_DISCONNECTED);
 RoQShutdown();assert(closed==10&&nextmaps==2);
 // Bink closes even after a connection-state change, and in-game completion
 // releases its pause without disconnecting the running server/client.
 cls.state=CA_ACTIVE;Open(0,9,FMV_PLAY,true,true);cinTable[0].bink=std::make_unique<Movie::Decoder>();
 CIN_StopCinematic(0);assert(closed==11&&binkClosed==1&&nextmaps==2&&cls.state==CA_ACTIVE);
 Open(0,10,FMV_PLAY,true);cinTable[0].bink=std::make_unique<Movie::Decoder>();cinTable[0].stvefInGame=true;paused=1;CL_handle=0;
 CIN_StopCinematic(0);assert(closed==12&&binkClosed==2&&!paused&&CL_handle==-1&&cls.state==CA_ACTIVE);
 stvefPendingMovie[0]='x';CIN_CloseAllVideos();assert(stvefPendingMovie[0]=='x'&&closed==12&&binkClosed==2);
 filesystemLive=false; // Static owners must not call FS_FCloseFile after this.
 for(auto &movie:cinTable)movie.iFile.reset();
 assert(closed==12);
}
'''


class CinematicShutdownTests(unittest.TestCase):
    def test_complete_partial_and_aborted_movies_release_owned_files(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        ownership = (ROOT / 'code/client/client_cpp.h').read_text()
        start = ownership.index('inline void CloseFile(')
        ownership = ownership[start:ownership.index('inline int OpenFileRead(', start)]
        cinematic = (ROOT / 'code/client/cl_cin.cpp').read_text()
        bodies = '\n'.join(function(cinematic, signature) for signature in (
            'void CIN_CloseAllVideos(', 'static void RoQShutdown( void ) {', 'e_status CIN_StopCinematic('))
        bodies += function((ROOT / 'code/client/cl_main.cpp').read_text(), 'static void CL_ShutdownVMs(')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        with tempfile.TemporaryDirectory(prefix='cinematic-shutdown-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(PRELUDE + ownership + HOST + bodies + CHECKS, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
