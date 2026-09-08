"""Server download reads must fill advertised bytes or fail without sending EOF."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from compiler_support import compiler_target_args
ROOT = Path(__file__).resolve().parents[1]

class ServerDownloadReadTests(unittest.TestCase):
    def test_read_failure_growth_and_window(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler required')
        source = (ROOT/'code/server/sv_client.cpp').read_text()
        start = source.index('static void SV_CloseDownload( client_t *cl ) {')
        cleanup = source[start:source.index('\n}',start)+2]
        start = source.index('\t// Perform any reads that we need to')
        reader = source[start:source.index('\n\tif (cl->downloadClientBlock == cl->downloadCurrentBlock)',start)]
        commands = (ROOT/'code/server/sv_main.cpp').read_text()
        start = commands.index('\tconst bool downloadDisconnect =')
        gate = commands[start:commands.index('\n\tclient->reliableSequence =',start)]

        harness = r'''
#include <cassert>
#include <cstring>
#include <initializer_list>
constexpr int MAX_DOWNLOAD_WINDOW=48;
constexpr bool qtrue=true;
struct client_t {int download=1,downloadSize=0,downloadCount=0,downloadCurrentBlock=0,downloadClientBlock=0;bool downloadEOF=false;char downloadName[32]="test.pk3";unsigned char*downloadBlocks[48]{};int downloadBlockSize[48]{};};
int dropped,reads,available,forced=-2,lastRequest,closes;
template<class T> T* SV_ZMallocArray(int n){return new T[n];}
void SV_ZFree(unsigned char*&p){delete[]p;p=nullptr;}
void SV_CloseFileHandle(int&f){if(f)++closes;f=0;}
int FS_Read(void*p,int n,int){++reads;lastRequest=n;if(forced!=-2)return forced;int got=n<available?n:available;memset(p,42,got);available-=got;return got;}
''' + cleanup + r'''
void SV_DropClient(client_t*cl,const char*){++dropped;SV_CloseDownload(cl);}
int fill(client_t*cl,int downloadWindow,int downloadBlockBytes){int curindex;
''' + reader + r'''
 return 1;
}
void reset(){dropped=reads=closes=0;forced=-2;available=0;}
struct command_client {int state;bool downloading;};
constexpr int CS_FREE=0,CS_ZOMBIE=1,CS_CONNECTED=2,CS_PRIMED=3,CS_ACTIVE=4;
bool sent;
void commandGate(command_client*client,const char*cmd){
''' + gate + r'''
 sent=true;
}
int main(){
 for(int state:{CS_FREE,CS_ZOMBIE,CS_CONNECTED,CS_PRIMED,CS_ACTIVE}){
  for(bool downloading:{false,true}){
   command_client client{state,downloading};
   for(const char*cmd:{"disconnect \"failure\"","print message","cs 1 value","disconnectOther"}){
    sent=false;commandGate(&client,cmd);
    assert(sent==(state>=CS_PRIMED || (state==CS_CONNECTED && downloading && !strncmp(cmd,"disconnect ",11))));
   }
  }
 }
 for(int blockBytes:{1024,2048}){
  for(int failure:{-1,0,7}){
   reset();client_t c;c.downloadSize=blockBytes+10;forced=failure;
   fill(&c,8,blockBytes);
   assert(dropped==1 && closes==1 && reads==1);
   assert(!c.downloadEOF && c.downloadCount==0 && c.downloadCurrentBlock==0);
   assert(!c.download && !*c.downloadName);
   for(auto p:c.downloadBlocks)assert(!p);
  }
  reset();client_t c;c.downloadSize=blockBytes+10;available=c.downloadSize+100;
  fill(&c,8,blockBytes);
  assert(!dropped && reads==2 && lastRequest==10 && available==100);
  assert(c.downloadCount==c.downloadSize && c.downloadEOF && c.downloadCurrentBlock==3);
  assert(c.downloadBlockSize[0]==blockBytes && c.downloadBlockSize[1]==10 && !c.downloadBlockSize[2]);
  SV_CloseDownload(&c);
  reset();client_t limited;limited.downloadSize=blockBytes*2;available=limited.downloadSize;
  fill(&limited,1,blockBytes);assert(reads==1 && !limited.downloadEOF);
  fill(&limited,1,blockBytes);assert(reads==1);
  limited.downloadClientBlock=1;fill(&limited,1,blockBytes);
  assert(reads==2 && limited.downloadCount==limited.downloadSize && !limited.downloadEOF);
  limited.downloadClientBlock=2;fill(&limited,1,blockBytes);
  assert(reads==2 && limited.downloadEOF);
  SV_CloseDownload(&limited);
  reset();client_t partial;partial.downloadSize=blockBytes*2;available=blockBytes;
  fill(&partial,8,blockBytes);
  assert(dropped==1 && reads==2 && !partial.downloadEOF && partial.downloadCount==blockBytes);
  for(auto p:partial.downloadBlocks)assert(!p);
 }
}
'''
        with tempfile.TemporaryDirectory(prefix='fntech3-download-read-') as folder:
            cpp=Path(folder)/'probe.cpp'; exe=Path(folder)/'probe.exe'; cpp.write_text(harness)
            build=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True,timeout=60)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

if __name__=='__main__':unittest.main()
