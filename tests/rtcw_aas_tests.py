"""Exercise the RTCW AAS8 reader with synthetic geometry and hostile graphs."""
from pathlib import Path
import struct
import unittest
import rtcw_botlib_tests

ROOT = Path(__file__).resolve().parents[1]


def synthetic_aas():
    pack = struct.pack
    lumps = [
        pack('<2i6f', 2, 0, -16, -16, -24, 16, 16, 32),
        pack('<3f', 0, 0, 0),
        pack('<4fi4fi', 1, 0, 0, 0, 0, -1, 0, 0, 0, 0),
        bytes(8), b'', bytes(24), b'',
        bytes(48) + b''.join(pack('<3i9f', i, 0, 0, -1, -1, -1, 1, 1, 1, 0, 0, 0) for i in (1, 2)),
        bytes(32) + b''.join(pack('<7if', 0, 1, 2, 1, i, 1, i, (i + 1) / 4) for i in (0, 1)),
        b''.join(pack('<3i6fiH2x', area, 0, 0, .25, 0, 0, -.25, 0, 0, 2, 10) for area in (2, 1)),
        bytes(12) + pack('<3i', 0, -1, -2), bytes(20), b'', bytes(16) + pack('<4i', 2, 2, 0, 0)
    ]
    header = bytearray(pack('<3I', 0x53414145, 8, 0xcafebabe))
    offset = 124
    for lump in lumps:
        header += pack('<2I', offset, len(lump)); offset += len(lump)
    for i in range(8, 124):
        header[i] ^= ((i - 8) * 119) & 255
    return bytes(header) + b''.join(lumps)


class RTCWAASFormatTests(unittest.TestCase):
    compile_and_run = rtcw_botlib_tests.RTCWBotLibTests.compile_and_run

    def test_v8_decoding_references_geometry_and_atomic_rejection(self):
        encoded = ','.join(str(b) for b in synthetic_aas())
        source = r'''
#include "code/compat/rtcw_aas_format.hpp"
#include <cassert>
#include <climits>
#include <limits>
#include <vector>
using namespace fntech3::rtcw::aas;
static const unsigned char initial[]={''' + encoded + r'''};
static void put(std::vector<unsigned char>&b,size_t offset,uint32_t value){
 for(unsigned i=0;i<4;++i)b[offset+i]=static_cast<unsigned char>(value>>(i*8));
}
static void xorHeader(std::vector<unsigned char>&b){for(size_t i=8;i<HeaderBytes;++i)b[i]^=static_cast<unsigned char>((i-8)*119);}
int main(){
 std::vector<unsigned char> bytes(std::begin(initial),std::end(initial));View view;
 assert(Parse(bytes.data(),bytes.size(),0xcafebabe,view)==Error::None);
 assert(view.Count(Areas)==3&&view.Count(Settings)==3&&view.Count(Reachability)==2&&view.Checksum()==0xcafebabe);
 assert(F32(view.Record(Settings,1)+28)==.25f&&I32(view.Record(Nodes,1)+4)==-1);
 assert(!view.Record(Areas,3)&&!view.Record(static_cast<Lump>(999),0)&&!view.Count(static_cast<Lump>(-1)));
 View empty;assert(!empty.Record(Areas,0)&&!empty.Count(Areas));
 auto old=view.Record(Areas,1);
 auto reject=[&](const std::vector<unsigned char>&bad,Error expected=Error::None){
  Error error=Parse(bad.data(),bad.size(),0xcafebabe,view);assert(error!=Error::None);
  if(expected!=Error::None)assert(error==expected);
  assert(view.Record(Areas,1)==old&&view.Checksum()==0xcafebabe);
 };
 assert(Parse(nullptr,bytes.size(),0xcafebabe,view)==Error::Header);
 assert(Parse(bytes.data(),bytes.size(),0xdeadbeef,view)==Error::Checksum);
 for(size_t n=0;n<bytes.size();++n)reject(std::vector<unsigned char>(bytes.begin(),bytes.begin()+n));
 for(unsigned offset:{0u,4u}){auto bad=bytes;put(bad,offset,offset?5:0);reject(bad,offset?Error::Version:Error::Identity);}
 for(unsigned i=0;i<LumpCount;++i){
  if(!view.Count(static_cast<Lump>(i)))continue;
  for(uint32_t offset:{0u,123u,static_cast<uint32_t>(bytes.size()),UINT32_MAX}){
   auto bad=bytes;xorHeader(bad);put(bad,12+i*8,offset);xorHeader(bad);reject(bad);
  }
  auto bad=bytes;xorHeader(bad);put(bad,16+i*8,UINT32_MAX);xorHeader(bad);reject(bad);
  bad=bytes;xorHeader(bad);put(bad,16+i*8,U32(bad.data()+16+i*8)-1);xorHeader(bad);reject(bad,Error::Stride);
 }
 auto change=[&](Lump lump,size_t row,size_t word,uint32_t value,Error error){
  auto bad=bytes;size_t at=view.Record(lump,row)-bytes.data();put(bad,at+word*4,value);reject(bad,error);
 };
 for(uint32_t nonfinite:{0x7fc00000u,0x7f800000u,0xff800000u}){
  change(Vertices,0,0,nonfinite,Error::Geometry);change(Settings,1,7,nonfinite,Error::Geometry);
  change(Reachability,0,3,nonfinite,Error::Geometry);change(Planes,0,3,nonfinite,Error::Geometry);
 }
 change(Boxes,0,2,0x42800000u,Error::Geometry); // minimum exceeds maximum
 change(Vertices,0,0,0x7f7fffffu,Error::Geometry);
 change(Settings,1,7,0xbf800000u,Error::Geometry);
 change(Settings,1,7,0x40000000u,Error::Geometry);
 change(Edges,0,0,1,Error::Index);change(Areas,1,0,99,Error::Index);
 change(Areas,1,2,UINT32_MAX,Error::Index);change(Planes,0,4,6,Error::Geometry);
 change(Settings,1,3,UINT32_MAX,Error::Routing);change(Settings,1,3,0x80000000u,Error::Routing);
 change(Settings,1,4,2,Error::Routing);change(Settings,1,5,INT_MAX,Error::Routing);
 change(Reachability,0,0,3,Error::Routing);change(Clusters,1,1,3,Error::Routing);
 change(Nodes,1,0,2,Error::Index);change(Nodes,1,1,0x80000000u,Error::Index);
 change(Nodes,1,1,3,Error::Index);change(Nodes,1,1,1,Error::Tree);
 // Lumps may be nonsequential and input may be unaligned, but overlaps fail.
 auto shuffled=bytes;xorHeader(shuffled);std::vector<unsigned char> copy(shuffled.begin(),shuffled.begin()+124);
 for(int i=LumpCount-1;i>=0;--i){size_t length=U32(shuffled.data()+16+i*8),offset=U32(shuffled.data()+12+i*8);
  put(copy,12+i*8,static_cast<uint32_t>(copy.size()));copy.insert(copy.end(),bytes.begin()+offset,bytes.begin()+offset+length);}
 xorHeader(copy);std::vector<unsigned char> unaligned(1,0xa5);unaligned.insert(unaligned.end(),copy.begin(),copy.end());
 View other;assert(Parse(unaligned.data()+1,copy.size(),0xcafebabe,other)==Error::None&&other.Count(Areas)==3);
 auto overlap=bytes;xorHeader(overlap);put(overlap,12+Edges*8,U32(overlap.data()+12+Vertices*8));xorHeader(overlap);reject(overlap,Error::Overlap);
 // Shared children and cycles cannot cause exponential or unbounded walks.
 // A deep chain is valid: traversal stack occupancy differs from tree depth.
 for(int mode=0;mode<3;++mode){
  auto bad=bytes;xorHeader(bad);size_t offset=bad.size();
  int count=mode==2?4096:4;put(bad,12+Nodes*8,(uint32_t)offset);put(bad,16+Nodes*8,count*12);
  bad.resize(offset+count*12);for(int i=1;i<count;++i){put(bad,offset+i*12+4,i+1<count?i+1:0);}
  if(mode==0)put(bad,offset+12+8,2); // two parents/edges for node 2
  if(mode==1)put(bad,offset+3*12+4,2); // cycle
  xorHeader(bad);if(mode==2){View deep;assert(Parse(bad.data(),bad.size(),0xcafebabe,deep)==Error::None);}
  else reject(bad,Error::Tree);
 }
}
'''
        self.compile_and_run(source, flags=['-O2', '-ffast-math'])
