"""Execute whole-file loading against short reads and ZIP payload failures."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class FilesystemPayloadTests(unittest.TestCase):
    def test_failed_payload_is_not_published_or_journaled(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('C++ compiler unavailable')
        source=(ROOT/'code/qcommon/files.c').read_text()
        start=source.index('int FS_ReadFile( const char *qpath, void **buffer ) {')
        function=source[start:source.index('\n}',start)+2]
        shared=(ROOT/'code/qcommon/q_shared.c').read_text()
        def extract(text, signature):
            begin=text.index(signature)
            return text[begin:text.index('\n}',begin)+2]
        crc=extract(shared,'unsigned int crc32_buffer(')
        validator=extract(source,'static qboolean FS_ZipPayloadMatches(')
        profile=extract(source,'int FS_ReadProfileFile( const char *qpath, void **buffer ) {')
        harness=r'''
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <vector>
using byte=unsigned char;
using fileHandle_t=int;
using qboolean=bool;
constexpr bool qfalse=false,qtrue=true;
constexpr int FS_INVALID_HANDLE=0, ERR_FATAL=1, UNZ_OK=0;
#define S_COLOR_YELLOW ""
int fs_searchpaths=1,com_journalDataFile=0,fs_loadCount=0,fs_loadStack=0;
struct Cvar {int integer=0;} journal;
auto com_journal=&journal;
int readResult=4,closeResult=0,closes=0,frees=0,allocations=0;
long openLength=4;
std::vector<long> journalLengths;
void Com_Error(int,const char*){std::abort();}
void Com_DPrintf(const char*,...){}
void Com_Printf(const char*,...){}
int FS_ReadFileFromRootArchive(const char*,void**){return -1;}
long FS_FOpenFileRead(const char*,int*h,bool){*h=7;return openLength;}
int FS_FOpenProfileFileRead(const char*,int*h){*h=7;return static_cast<int>(openLength);}
byte* Hunk_AllocateTempMemory(int n){++allocations;return static_cast<byte*>(std::malloc(n));}
void Hunk_FreeTempMemory(void*p){++frees;std::free(p);}
int FS_Read(void*p,int n,int h){if(h==3){assert(n==sizeof(long));std::memcpy(p,&openLength,n);return n;}assert(h==7);if(readResult>0)std::memset(p,'x',readResult);return readResult;}
struct Handle {bool zipFile=true;struct {struct {int z=1;} file;} handleFiles;} fsh[8];
using unzFile=int;
struct unz_file_info {unsigned long uncompressed_size,crc;};
unsigned int crc32_buffer(const byte*,unsigned int);
int unzGetCurrentFileInfo(int,unz_file_info*info,void*,int,void*,int,void*,int){
 info->uncompressed_size=openLength;
 const byte payload[]={ 'x','x','x','x' };
 info->crc=crc32_buffer(payload,static_cast<unsigned>(openLength)) ^ (closeResult?1u:0u);
 return 0;
}
void FS_FCloseFile(int h){assert(h==7);++closes;}
void FS_Write(const void*p,int n,int){assert(n==sizeof(long));journalLengths.push_back(*static_cast<const long*>(p));}
void FS_Flush(int){}
'''+crc+'\n'+validator+'\n'+function+'\n'+profile+r'''
int main(){
 for(int recording=0;recording<2;++recording){
  com_journalDataFile=recording?3:0;journal.integer=recording;
  for(int fault=0;fault<3;++fault){
   readResult=fault==0?3:fault==1?-1:4;closeResult=fault==2?-105:0;
   closes=frees=allocations=fs_loadCount=fs_loadStack=0;journalLengths.clear();
   void *buffer=reinterpret_cast<void*>(1);
   assert(FS_ReadFile("fixture.cfg",&buffer)==-1 && buffer==nullptr);
   assert(closes==1 && frees==1 && allocations==1 && fs_loadCount==0 && fs_loadStack==0);
   assert(journalLengths.size()==static_cast<unsigned>(recording));
   if(recording)assert(journalLengths[0]==0);
  }
 }
 com_journalDataFile=0;readResult=4;closeResult=0;
 void* buffer=nullptr;assert(FS_ReadFile("fixture.bin",&buffer)==4);
 assert(buffer && !std::memcmp(buffer,"xxxx\0",5));
 assert(fs_loadCount==1 && fs_loadStack==1);Hunk_FreeTempMemory(buffer);
 // Empty entries must still be closed and CRC checked.
 openLength=readResult=0;closeResult=-105;
 assert(FS_ReadFile("empty.bin",&buffer)==-1 && !buffer);
 closeResult=0;assert(FS_ReadFile("empty.bin",&buffer)==0 && buffer);
 assert(*static_cast<char*>(buffer)==0);Hunk_FreeTempMemory(buffer);
 // Reject unrepresentable allocations without reading, allocating, or leaking.
 for(long bad : {-1L, static_cast<long>(INT_MIN), static_cast<long>(INT_MAX)}) {
  openLength=bad;int prior=allocations;
  for(int query=0;query<2;++query){
   closes=0;buffer=reinterpret_cast<void*>(1);
   assert(FS_ReadFile("huge.bin",query?nullptr:&buffer)==-1);
   assert(closes==1 && allocations==prior && (query || !buffer));
   closes=0;buffer=reinterpret_cast<void*>(1);
   assert(FS_ReadProfileFile("huge.bin",query?nullptr:&buffer)==-1);
   assert(closes==1 && allocations==prior && (query || !buffer));
   com_journalDataFile=3;journal.integer=2;closes=0;buffer=reinterpret_cast<void*>(1);
   assert(FS_ReadFile("huge.cfg",query?nullptr:&buffer)==-1);
   assert(closes==0 && allocations==prior && (query || !buffer));
   com_journalDataFile=0;
  }
 }
 openLength=INT_MAX-1;
 assert(FS_ReadFile("largest.bin",nullptr)==INT_MAX-1);
 assert(FS_ReadProfileFile("largest.bin",nullptr)==INT_MAX-1);
 openLength=0;
 // Length-only queries preserve their existing metadata-only behavior.
 int before=allocations;assert(FS_ReadFile("fixture.bin",nullptr)==0);
 assert(allocations==before);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp=Path(temp)/'probe.cpp';exe=Path(temp)/'probe.exe';cpp.write_text(harness)
            subprocess.run([cc,*compiler_target_args(cc),'-std=c++17',str(cpp),'-o',str(exe)],check=True,capture_output=True,text=True)
            subprocess.run([str(exe)],check=True,capture_output=True,text=True)
if __name__=='__main__':unittest.main()
