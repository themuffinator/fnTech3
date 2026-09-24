"""Execute the shared RoQ decoder against bounded synthetic movie streams."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]

HOST = r'''
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_types.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
struct ScopedFileHandle {
 int handle=0; int get()const{return handle;} void reset(){handle=0;}
};
static std::vector<byte> stream;
static size_t position=0;
static int shortRead=-1, reads=0, audio=0, diagnostics=0;
static int FileRead(int handle,void *out,size_t bytes){
 assert(handle==1);++reads;
 size_t count=std::min(bytes,stream.size()-position);
 if(shortRead>=0)count=std::min(count,static_cast<size_t>(shortRead));
 if(count)memcpy(out,stream.data()+position,count);
 position+=count;return static_cast<int>(count);
}
static int OpenFileRead(const char *,ScopedFileHandle &file,qboolean){
 file.handle=1;position=0;return static_cast<int>(stream.size());
}
static int CL_ScaledMilliseconds(){return 100;}
void QDECL Com_Printf(const char *,...){++diagnostics;}
void QDECL Com_DPrintf(const char *,...){++diagnostics;}
static void S_Update(int){}
static int s_rawend=0,s_soundtime=0;
static cvar_t volume{};static cvar_t *s_volume=&volume;
static void S_RawSamples(int count,int,int,int,byte *,float){assert(count>=0);++audio;}
static glconfig_t config{};
static struct{const glconfig_t *GetConfig(){return &config;}}re;
'''

CHECKS = r'''
static void header(std::vector<byte> &out,unsigned id,unsigned size,unsigned flags=0){
 out.push_back(id&255);out.push_back(id>>8);
 for(int i=0;i<4;++i)out.push_back((size>>(8*i))&255);
 out.push_back(flags&255);out.push_back(flags>>8);
}
static void chunk(std::vector<byte> &out,unsigned id,const std::vector<byte> &data,unsigned flags=0){
 header(out,id,static_cast<unsigned>(data.size()),flags);out.insert(out.end(),data.begin(),data.end());
}
static std::vector<byte> info(unsigned width=16,unsigned height=16){
 return {byte(width),byte(width>>8),byte(height),byte(height>>8),8,0,4,0};
}
static void begin(unsigned id,const std::vector<byte> &data,unsigned flags=0){
 stream.clear();header(stream,0x1084,0xffffffff,30);chunk(stream,id,data,flags);
 // A following header makes short bodies distinct from a valid terminal chunk.
 header(stream,ROQ_QUAD_HANG,0);
 cin={};cinTable={};currentHandle=cin.currentHandle=0;
 auto &movie=cinTable[0];movie.iFile.handle=1;movie.ROQSize=static_cast<long>(stream.size());
 movie.status=FMV_PLAY;movie.fileName[0]='x';
 position=0;shortRead=-1;reads=audio=diagnostics=0;
 config.maxTextureSize=4096;initRoQ();
 assert(FileRead(1,cin.file.data(),16)==16);assert(RoQ_init());
}
static void rejected(){
 assert(cinTable[0].status==FMV_EOF);
 assert(!cinTable[0].looping && !cinTable[0].holdAtEnd);
 assert(diagnostics>0);
}
int main(){
 // A short read must never turn stale lookahead bytes into quad information.
 begin(ROQ_QUAD_INFO,info());shortRead=7;
 cinTable[0].looping=true;cinTable[0].holdAtEnd=true;
 RoQInterrupt();rejected();assert(cinTable[0].numQuads==-1);
 for(unsigned bytes=1;bytes<8;++bytes){
  begin(ROQ_QUAD_INFO,std::vector<byte>(bytes));RoQInterrupt();rejected();
  assert(cinTable[0].numQuads==-1 && cinTable[0].screenDelta==0);
 }
 for(unsigned dimension:{0u,1u,8u,15u,17u,511u,513u,65535u}){
  for(int axis=0;axis<2;++axis){
   begin(ROQ_QUAD_INFO,info(axis?16:dimension,axis?dimension:16));
   RoQInterrupt();rejected();assert(cinTable[0].screenDelta==0);
  }
 }
 for(unsigned width:{16u,256u,512u})for(unsigned height:{16u,256u,512u}){
  begin(ROQ_QUAD_INFO,info(width,height));RoQInterrupt();
  assert(cinTable[0].status==FMV_PLAY && cinTable[0].numQuads==0);
  assert(cinTable[0].screenDelta==static_cast<long>(width*height*4));
  assert(cinTable[0].onQuad==static_cast<long>(width*height/64*5));
 }
 // Header high bytes must not truncate a 32-bit size to an accepted 24-bit one.
 begin(ROQ_QUAD_INFO,info());cin.file[13]=1;assert(!RoQ_init());
 begin(ROQ_QUAD_INFO,info());cin.file[10]=0;cin.file[11]=0;cin.file[12]=2;assert(!RoQ_init());
 // Codebook flags encode six bytes per small entry, four per large entry.
 for(unsigned flags:{0u,0x0100u,0x0101u,0x00ffu,0xffffu}){
  unsigned small=flags>>8;if(!small)small=256;
  unsigned large=flags?flags&255:256;unsigned size=small*6+large*4;
  begin(ROQ_CODEBOOK,std::vector<byte>(size-1),flags);
  vq2.fill(0x1234);RoQInterrupt();rejected();assert(vq2[0]==0x1234);
  begin(ROQ_CODEBOOK,std::vector<byte>(size),flags);RoQInterrupt();assert(cinTable[0].status==FMV_PLAY);
 }
 // Retail zero-count codebooks can update only the 256 small entries, or all
 // 256 large entries as well. No-update must preserve the prior large table.
 for(unsigned small:{1u,255u,256u}){
  const unsigned flags=small==256?0:small<<8;
  begin(ROQ_CODEBOOK,std::vector<byte>(small*6),flags);vq4.fill(0x1234);vq8.fill(0x5678);
  RoQInterrupt();assert(cinTable[0].status==FMV_PLAY&&vq4[0]==0x1234&&vq8[0]==0x5678);
  begin(ROQ_CODEBOOK,std::vector<byte>(small*6+1024),flags);RoQInterrupt();assert(cinTable[0].status==FMV_PLAY);
  begin(ROQ_CODEBOOK,std::vector<byte>(small*6+1),flags);RoQInterrupt();rejected();
 }
 // Odd stereo bodies must not consume a byte outside their payload, even muted.
 for(bool silent:{false,true}){
  begin(ZA_SOUND_STEREO,{1,2,3});cinTable[0].silent=silent;RoQInterrupt();rejected();assert(audio==0);
 }
 begin(ROQ_QUAD_VQ,{0,0});RoQInterrupt();rejected(); // Missing dimensions.
 // A complete last VQ frame is decoded once, and held until the next tick.
 begin(ROQ_QUAD_INFO,info());stream.resize(24);chunk(stream,ROQ_QUAD_VQ,{0,0});
 cinTable[0].ROQSize=static_cast<long>(stream.size());
 RoQInterrupt();RoQInterrupt();assert(cinTable[0].numQuads==1&&cinTable[0].buf&&cinTable[0].dirty);
 assert(cinTable[0].status==FMV_PLAY);RoQInterrupt();assert(cinTable[0].status==FMV_EOF);
 // EOF hold and loop controls still work on complete streams.
 for(bool hold:{false,true}){
  begin(ROQ_QUAD_INFO,info());stream.resize(24);chunk(stream,ROQ_QUAD_VQ,{0,0});
  cinTable[0].ROQSize=static_cast<long>(stream.size());
  cinTable[0].holdAtEnd=hold;cinTable[0].looping=!hold;
  RoQInterrupt();RoQInterrupt();RoQInterrupt();
  assert(cinTable[0].status==(hold?FMV_IDLE:FMV_LOOPED));
 }
 // Truncated final headers/bodies reject rather than reusing prior buffer data.
 for(int trailing=1;trailing<8;++trailing){
  begin(ROQ_QUAD_INFO,info());stream.resize(24+trailing);
  cinTable[0].ROQSize=static_cast<long>(stream.size());RoQInterrupt();rejected();
 }
 begin(ROQ_QUAD_INFO,info());stream.resize(20);cinTable[0].ROQSize=20;
 RoQInterrupt();rejected();assert(cinTable[0].screenDelta==0);
 // Every truncated VQ prefix is rejected before publishing a frame.
 // Four 8x8 codebook copies, then a motion block whose source leaves the buffer.
 for(const auto &data:std::vector<std::vector<byte>>{{},{0},{0,0xaa},{0,0xaa,0},{0,0xaa,0,0},{0,0xaa,0,0,0},{0,0x40,255}}){
  begin(ROQ_QUAD_INFO,info());stream.resize(24);chunk(stream,ROQ_QUAD_VQ,data);
  cinTable[0].ROQSize=static_cast<long>(stream.size());RoQInterrupt();
  cin.linbuf.fill(0x56);RoQInterrupt();rejected();
  assert(cinTable[0].numQuads==0&&!cinTable[0].buf);
  assert(std::all_of(cin.linbuf.begin(),cin.linbuf.end(),[](byte v){return v==0x56;}));
 }
 // Valid split 4x4 and 2x2 payloads, including code-word rollover.
 for(const auto &data:std::vector<std::vector<byte>>{{0,0xaa,0,0,0,0},{0,0xc0},{0,0xff,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}){
  begin(ROQ_QUAD_INFO,info());stream.resize(24);chunk(stream,ROQ_QUAD_VQ,data);
  cinTable[0].ROQSize=static_cast<long>(stream.size());RoQInterrupt();RoQInterrupt();
  assert(cinTable[0].status==FMV_PLAY&&cinTable[0].numQuads==1);
 }
 // Packed chunks cannot borrow bytes from the outer stream or exceed their count.
 std::vector<byte> packet;chunk(packet,ROQ_QUAD_INFO,info());chunk(packet,ROQ_QUAD_VQ,{0,0});
 begin(ROQ_PACKET,packet,2);RoQInterrupt();assert(cinTable[0].numQuads==1&&cinTable[0].status==FMV_PLAY);
 for(unsigned count:{0u,1u,3u,65535u}){begin(ROQ_PACKET,packet,count);RoQInterrupt();rejected();}
 packet.resize(packet.size()-1);begin(ROQ_PACKET,packet,2);RoQInterrupt();rejected();
 // All 65536 legal payload bytes fit; ignored JPEG chunks preserve reference behavior.
 begin(ROQ_QUAD_JPEG,std::vector<byte>(65536));RoQInterrupt();assert(cinTable[0].status==FMV_PLAY);
 // Reopening a truncated movie must not reuse the old 16-byte header or loop forever.
 begin(ROQ_QUAD_INFO,info());stream.resize(15);RoQReset();rejected();
 begin(ROQ_QUAD_INFO,info());shortRead=15;RoQReset();rejected();
 // A looping stream with setup chunks but no frames must terminate too.
 begin(ROQ_QUAD_INFO,info());stream.resize(24);cinTable[0].ROQSize=24;cinTable[0].looping=true;
 RoQInterrupt();RoQInterrupt();rejected();
}
'''


class RoQBoundsTests(unittest.TestCase):
    def test_real_decoder_rejects_malformed_streams_before_buffer_access(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/cl_cin.cpp').read_text()
        constants = source[source.index('namespace {'):source.index('} // namespace') + len('} // namespace')]
        declarations = source[source.index('static std::array<long, 256> ROQ_YY_tab;'):source.index('extern "C" {\nextern int')]
        decode = source[source.index('static void RllSetupTable('):source.index('static void RoQShutdown( void )')]
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32', '-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='roq-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(HOST + constants + '\n' + declarations + '\nstatic qboolean RoQ_init();\n' + decode + CHECKS)
            build = subprocess.run([compiler, *target, '-std=c++17', '-O1', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
