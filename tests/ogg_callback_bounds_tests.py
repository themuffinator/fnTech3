"""Execute production Vorbis filesystem callbacks with bounded file fixtures."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class OggCallbackBoundsTests(unittest.TestCase):
    def test_read_elements_and_seek_bounds(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/audio/codecs/snd_codec_ogg.cpp').read_text()
        callbacks = source[source.index('size_t S_OGG_Callback_read'):source.index('// fclose() replacement')]
        harness = r'''
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <limits>
#include <initializer_list>
using ogg_int64_t=long long;
struct snd_stream_t {int file=1,pos=0,length=16;};
constexpr int FS_SEEK_SET=0,FS_SEEK_CUR=1,FS_SEEK_END=2;
int cursor=0,readResult=3,readCalls=0,seekCalls=0,seekResult=0;
bool wrongPosition=false;
int FileRead(int,void*p,int bytes){
 ++readCalls;assert(bytes>=0&&bytes<=16);
 if(readResult<0)return readResult;
 int n=readResult<bytes?readResult:bytes;std::memset(p,0x24,n);cursor+=n;return n;
}
int FS_FTell(int){return cursor;}
int FS_Seek(int,long offset,int whence){
 ++seekCalls;if(seekResult<0)return seekResult;
 cursor=(whence==FS_SEEK_SET?0:whence==FS_SEEK_CUR?cursor:16)+offset;
 if(wrongPosition)++cursor;
 return seekResult;
}
''' + callbacks + r'''
int main(){
 char out[32];std::memset(out,0x5a,sizeof(out));snd_stream_t stream;
 assert(S_OGG_Callback_read(out,2,2,&stream)==1);
 assert(stream.pos==3&&cursor==3&&out[3]==0x5a);
 readResult=-1;int old=stream.pos;
 assert(S_OGG_Callback_read(out,1,4,&stream)==0&&stream.pos==old&&errno==EIO);
 int calls=readCalls;
 assert(S_OGG_Callback_read(out,(std::numeric_limits<size_t>::max)(),2,&stream)==0);
 assert(S_OGG_Callback_read(out,2,static_cast<size_t>(INT_MAX),&stream)==0);
 assert(readCalls==calls);
 assert(S_OGG_Callback_read(out,0,4,&stream)==0);
 assert(S_OGG_Callback_read(nullptr,1,4,&stream)==0);
 assert(S_OGG_Callback_read(out,1,4,nullptr)==0);
 stream.pos=cursor=15;readResult=16;
 assert(S_OGG_Callback_read(out,1,16,&stream)==1&&stream.pos==16&&cursor==16);
 assert(S_OGG_Callback_read(out,1,16,&stream)==0);
 for(int result:{0,7}){
  seekResult=result;stream.pos=cursor=3;
  assert(S_OGG_Callback_seek(&stream,5,SEEK_SET)==0&&cursor==5&&stream.pos==5);
  assert(S_OGG_Callback_seek(&stream,-2,SEEK_CUR)==0&&cursor==3&&stream.pos==3);
  assert(S_OGG_Callback_seek(&stream,-4,SEEK_END)==0&&cursor==12&&stream.pos==12);
 }
 for(int whence:{SEEK_SET,SEEK_CUR,SEEK_END})for(ogg_int64_t offset:{-9223372036854775807LL-1,9223372036854775807LL,-100LL,100LL}){
  stream.pos=cursor=8;int calls=seekCalls;
  assert(S_OGG_Callback_seek(&stream,offset,whence)==-1);
  assert(seekCalls==calls&&cursor==8&&stream.pos==8);
 }
 assert(S_OGG_Callback_seek(&stream,0,12345)==-1);
 assert(S_OGG_Callback_seek(nullptr,0,SEEK_SET)==-1);
 seekResult=-1;assert(S_OGG_Callback_seek(&stream,0,SEEK_SET)==-1&&stream.pos==8);
 seekResult=0;wrongPosition=true;
 assert(S_OGG_Callback_seek(&stream,0,SEEK_SET)==-1&&errno==EIO);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'callbacks.cpp'
            exe = Path(directory) / 'callbacks.exe'
            cpp.write_text(harness)
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)], capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
