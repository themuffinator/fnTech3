"""Exercise load optimizations against independent byte/cursor/geometry oracles."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2]


class LoadPathTests(unittest.TestCase):
    def test_crc_seeks_memory_and_patch_bounds(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        shared = (ROOT / 'code/qcommon/q_shared.c').read_text()
        files = (ROOT / 'code/qcommon/files.c').read_text()
        source = r'''
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include "code/client/audio/shared/AudioMemoryStream.h"
#include "code/client/DecodedImageCache.h"
using byte=unsigned char;
using qboolean=bool;
constexpr bool qfalse=false,qtrue=true;
using vec3_t=float[3];
#include "code/renderercommon/tr_patch_bounds.h"
using fileHandle_t=int;
enum fsOrigin_t {FS_SEEK_SET,FS_SEEK_CUR,FS_SEEK_END};
constexpr int ERR_FATAL=1,PK3_SEEK_BUFFER_SIZE=65536,PACK_FORMAT_PAK=1;
bool fs_searchpaths=true;
struct Pak {int format=0;};
struct Handle {bool zipFile=true;int zipFileLen=1000000,zipFilePos=0;Pak*pak=nullptr;
 struct {struct {int z=1;}file;}handleFiles;
} fsh[2];
int cursor=0,resets=0,consumed=0;
int FS_FTell(int){return cursor;}
void unzSetCurrentFileInfoPosition(int,int){}
int unzOpenCurrentFile(int){cursor=0;++resets;return 0;}
int FS_Read(void*data,int count,int){
 assert(count>=0);if(count>1000000-cursor)count=1000000-cursor;
 for(int i=0;i<count;++i)static_cast<byte*>(data)[i]=static_cast<byte>(cursor+i);
 cursor+=count;consumed+=count;return count;
}
FILE*FS_FileForHandle(int){std::abort();}
void Com_Error(int,const char*){std::abort();}
''' + function(shared, 'unsigned int crc32_buffer(') + '\n' + function(files, 'int FS_Seek(') + r'''
uint32_t referenceCRC(const byte*data,unsigned length){
 uint32_t crc=~0u;
 for(unsigned i=0;i<length;++i){crc^=data[i];for(int b=0;b<8;++b)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);}
 return ~crc;
}
uint32_t randomState=0x92654ba1u;
uint32_t randomWord(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;}
float storedFloat(float value){volatile float stored=value;return stored;}
int main(){
 assert(crc32_buffer(nullptr,0)==0);
 assert(crc32_buffer(reinterpret_cast<const byte*>("123456789"),9)==0xcbf43926u);
 std::vector<byte> bytes(8192+16);for(auto&b:bytes)b=static_cast<byte>(randomWord());
 for(unsigned alignment=0;alignment<16;++alignment){
  for(unsigned length=0;length<130;++length)assert(crc32_buffer(bytes.data()+alignment,length)==referenceCRC(bytes.data()+alignment,length));
  for(unsigned length:{511u,512u,513u,4096u,8192u})assert(crc32_buffer(bytes.data()+alignment,length)==referenceCRC(bytes.data()+alignment,length));
 }
 // Monotonic absolute seeks consume each prefix once, including chunk boundaries.
 for(int target=10000;target<=900000;target+=10000){assert(FS_Seek(1,target,FS_SEEK_SET)==target);assert(cursor==target);}
 assert(resets==0 && consumed==900000);
 assert(FS_Seek(1,900000,FS_SEEK_SET)==900000 && consumed==900000);
 assert(FS_Seek(1,42,FS_SEEK_SET)==42 && resets==1 && cursor==42);
 byte value=0;assert(FS_Read(&value,1,1)==1 && value==42);
 assert(FS_Seek(1,7,FS_SEEK_CUR)==7 && cursor==50);
 assert(FS_Seek(1,-10,FS_SEEK_CUR)==-10 && cursor==40);
 assert(FS_Seek(1,-1,FS_SEEK_END)==-1 && cursor==999999);
 assert(FS_Seek(1,0,FS_SEEK_END)==0 && cursor==1000000);
 assert(FS_Seek(1,-1000001,FS_SEEK_END)==-1000001 && cursor==0);
 // fread-style complete elements, trailing bytes and 64-bit seek overflow.
 fntech3::audio::AudioMemoryStream stream{bytes.data(),17};
 byte output[32];std::memset(output,0x5a,sizeof(output));
 assert(stream.Read(output,2,20)==8 && stream.position==17 && output[17]==0x5a);
 assert(!std::memcmp(output,bytes.data(),17));
 assert(stream.Read(output,1,1)==0 && stream.Seek(-3,SEEK_END)==0);
 assert(stream.Read(output,1,9)==3 && !std::memcmp(output,bytes.data()+14,3));
 for(int origin:{SEEK_SET,SEEK_CUR,SEEK_END})for(int64_t offset:{INT64_MIN,INT64_MAX}){
  assert(stream.Seek(offset,origin)==-1 && stream.position==17);
 }
 assert(stream.Seek(0,SEEK_SET)==0 && stream.Read(output,SIZE_MAX,2)==0 && stream.position==0);
 assert(stream.Seek(18,SEEK_SET)==-1 && stream.Seek(0,-1)==-1);
 assert(stream.Read(nullptr,1,1)==0 && stream.Read(output,0,1)==0);
 // A rejected bounds pair must never contain vertices that match the legacy
 // subtraction/tolerance test. Include degenerate and nearly touching bounds.
 for(int i=0;i<100000;++i){
  vec3_t a[2],b[2];bool match=true;
  for(int axis=0;axis<3;++axis){
   float p=static_cast<int>(randomWord()%2000000)-1000000.0f;
   float q=storedFloat(p+(static_cast<int>(randomWord()%401)-200)*0.001f);
   a[0][axis]=p; a[1][axis]=p+static_cast<float>(randomWord()%16);
   b[0][axis]=q; b[1][axis]=q+static_cast<float>(randomWord()%16);
   // The vertex oracle uses the stored coordinates, not x87 intermediates
   // from constructing them that may retain precision beyond float.
   if(std::fabs(a[0][axis]-b[0][axis])>.1)match=false;
  }
  if(match)assert(R_PatchBoundsOverlap(a,b));
 }
 vec3_t a[2]={{0,0,0},{1,1,1}},b[2]={{10,0,0},{11,1,1}};
 assert(!R_PatchBoundsOverlap(a,b));
 b[0][0]=1.05f;assert(R_PatchBoundsOverlap(a,b));
 // Decoded cache contents are admitted only when the current encoded bytes
 // still match exactly. Exercise replacement, LRU eviction and tight budgets.
 byte encodedA[]={1,2,3,4},encodedB[]={5,6,7,8},rgba[16]{};
 fntech3::DecodedImageCache cache(40);
 cache.Store("a",encodedA,4,rgba,2,2);
 cache.Store("b",encodedB,4,rgba,2,2);
 assert(cache.Bytes()==40 && cache.Find("a",encodedA,4));
 cache.Store("c",encodedB,4,rgba,2,2);
 assert(!cache.Find("b",encodedB,4) && cache.Find("a",encodedA,4));
 assert(!cache.Find("a",encodedB,4) && cache.Bytes()==20);
 cache.Store("c",encodedA,4,rgba,2,2);assert(cache.Bytes()==20);
 assert(!cache.Find("c",encodedA,3) && cache.Bytes()==0);
 cache.Store("huge",encodedA,4,rgba,INT_MAX,INT_MAX);assert(cache.Bytes()==0);
 cache.Store("empty",encodedA,4,rgba,0,2);assert(cache.Bytes()==0);
 fntech3::DecodedImageCache disabled(0);
 disabled.Store("a",encodedA,4,rgba,2,2);assert(disabled.Bytes()==0);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'load.cpp'
            exe = Path(directory) / 'load.exe'
            cpp.write_text(source)
            compiled = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                                       '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                      capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
