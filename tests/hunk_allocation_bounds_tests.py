"""Execute temporary-hunk allocation boundaries without large allocations."""
from pathlib import Path
import shutil,subprocess,tempfile,unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class HunkAllocationBoundsTests(unittest.TestCase):
    def test_overhead_and_bank_sum_cannot_wrap(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('C++ compiler unavailable')
        text=(ROOT/'code/qcommon/common.c').read_text();start=text.index('void *Hunk_AllocateTempMemory( int size ) {')
        function=text[start:text.index('\n}',start)+2]
        source=r'''
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <climits>
#include <cstring>
#define PAD(x,y) (((x)+((y)-1))&~((y)-1))
constexpr int ERR_DROP=1,HUNK_MAGIC=123;
struct hunkHeader_t {int magic,size;};
struct memblock_t {char bytes[64];};
struct Bank {int temp=0,permanent=0,tempHighwater=0;} hunk_low,high;
auto hunk_temp=&hunk_low;auto hunk_permanent=&high;
alignas(16) unsigned char storage[256];unsigned char *s_hunkData=storage;
int s_hunkTotal=sizeof(storage),swaps=0,zoneCalls=0;
[[noreturn]] void Com_Error(int code,const char*,int){assert(code==ERR_DROP);throw code;}
void Hunk_SwapBanks(){++swaps;}
void *Z_Malloc(int size){assert(size>=0 && size<128);++zoneCalls;return storage;}
'''+function+r'''
int main(){
 for(bool initialized:{false,true}){
  s_hunkData=initialized?storage:nullptr;
  for(int size:{-1,INT_MIN,INT_MAX,INT_MAX-1,INT_MAX-7}){
   int prior=zoneCalls;swaps=0;hunk_low.temp=0;
   try{Hunk_AllocateTempMemory(size);assert(false);}catch(int){}
   assert(swaps==0 && zoneCalls==prior && hunk_low.temp==0);
  }
 }
 s_hunkData=storage;s_hunkTotal=INT_MAX;hunk_low.temp=INT_MAX-10;high.permanent=20;
 try{Hunk_AllocateTempMemory(16);assert(false);}catch(int){}
 assert(hunk_low.temp==INT_MAX-10);
 high.permanent=INT_MAX-10;
 try{Hunk_AllocateTempMemory(64);assert(false);}catch(int){}
 assert(hunk_low.temp==INT_MAX-10);
 s_hunkTotal=sizeof(storage);hunk_low={};high={};
 for(bool low:{true,false}){
  hunk_low={};high={};hunk_temp=low?&hunk_low:&high;hunk_permanent=low?&high:&hunk_low;
  auto p=Hunk_AllocateTempMemory(15);auto hdr=static_cast<hunkHeader_t*>(p)-1;
  assert(hdr->magic==HUNK_MAGIC && hdr->size==24 && hunk_temp->temp==24);
  std::memset(p,0x5a,15);
 }
 s_hunkData=nullptr;assert(Hunk_AllocateTempMemory(15)==storage && zoneCalls==1);
}
'''
        source=source.replace('#include <cstring>','#include <cstring>\n#include <initializer_list>')
        with tempfile.TemporaryDirectory() as temp:
            cpp=Path(temp)/'probe.cpp';exe=Path(temp)/'probe.exe';cpp.write_text(source)
            subprocess.run([cc,*compiler_target_args(cc),'-std=c++17',str(cpp),'-o',str(exe)],check=True,capture_output=True,text=True)
            subprocess.run([str(exe)],check=True,capture_output=True,text=True)
if __name__=='__main__':unittest.main()
