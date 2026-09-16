"""RTCW RCD12/15 byte layouts, visibility runs and map-index validation."""
import binascii
from pathlib import Path
import struct
import unittest
import rtcw_aas_tests
import rtcw_botlib_tests

ROOT = Path(__file__).resolve().parents[1]


def map_lumps(data):
    header = bytearray(data[:124])
    for i in range(8, 124):
        header[i] ^= ((i - 8) * 119) & 255
    return [data[o:o + n] for o, n in (struct.unpack_from('<2I', header, 12 + i * 8) for i in range(14))]


def synthetic_cache(aas, version=15):
    lumps = map_lumps(aas)
    crc = [binascii.crc_hqx(lumps[i], 65535) for i in (7, 13, 9)]

    def cache(times, reaches):
        n = len(times)
        data = bytearray(52 + 3 * n)
        struct.pack_into('<ifii4fi3I', data, 0, len(data), 7.5, 1, 1,
                         1.25, -2.5, 3.75, 1.0, 20451262, 0x12345678, 0x87654321, 0xdeadbeef)
        struct.pack_into('<' + 'H' * n, data, 48, *times)
        data[52 + n * 2:] = bytes(reaches)
        return data

    return (struct.pack('<9I', 0x4352454d, version, len(lumps[7]) // 48, len(lumps[13]) // 16,
                        *crc, 1, 1) + cache([0], [255]) + cache([1, 25], [0, 0])
            + struct.pack('<I', 0) + struct.pack('<I2B', 2, 0, 3)
            + struct.pack('<I6B', 6, 2, 1, 0, 1, 255, 1)
            + struct.pack('<9f', 0, 0, 0, 1.25, -2.5, 3.75, -10, 20, 30))


def fixture_source():
    aas = rtcw_aas_tests.synthetic_aas()
    cache = synthetic_cache(aas)
    return r'''
#include "code/compat/rtcw_routecache_format.hpp"
#include <cassert>
#include <climits>
#include <vector>
namespace rc = fntech3::rtcw::routecache;
namespace aa = fntech3::rtcw::aas;
static const unsigned char mapBytes[]={''' + ','.join(map(str, aas)) + r'''};
static const unsigned char initial[]={''' + ','.join(map(str, cache)) + r'''};
static void put(std::vector<unsigned char>&b,size_t offset,uint32_t value){
 for(unsigned i=0;i<4;++i)b[offset+i]=static_cast<unsigned char>(value>>(i*8));
}
'''


class RTCWRouteCacheTests(unittest.TestCase):
    compile_and_run = rtcw_botlib_tests.RTCWBotLibTests.compile_and_run

    def test_layout_visibility_bounds_and_atomic_rejection(self):
        self.compile_and_run(fixture_source() + r'''
int main(){
 assert(rc::CRC("123456789",9)==0x29b1&&rc::CRC(nullptr,0)==0xffff);
 aa::View map;assert(aa::Parse(mapBytes,sizeof(mapBytes),0xcafebabe,map)==aa::Error::None);
 const auto signature=rc::MapSignature(map);assert(signature.areas==3&&signature.clusters==2);
 std::vector<unsigned char> bytes(std::begin(initial),std::end(initial));rc::View view;
 const auto parse=[&](const auto &data,rc::Version v=rc::Version::SP){
  return rc::Parse(data.data(),data.size(),v,signature,view);
 };
 assert(parse(bytes)==rc::Error::None&&view.AreaCount()==3);
 assert(rc::ValidateTopology(view,rc::AAS8Topology(map))==rc::Error::None);
 size_t count=0;for(const auto cache:view.Caches()){
  assert(cache.IsPortal()==(count==0)&&cache.Count()==count+1&&cache.Bytes()==52+3*(count+1));
  assert(cache.Time()==7.5f&&cache.Cluster()==1&&cache.Area()==1);
  assert(cache.Origin(0)==1.25f&&cache.Origin(1)==-2.5f&&cache.Origin(2)==3.75f&&cache.Origin(3)==0);
  assert(cache.StartTravelTime()==1&&cache.TravelFlags()==20451262);
  assert(cache.TravelTime(0)==count&&cache.TravelTime(1)==(count?25:0));
  assert(cache.Reachability(0)==(count?0:255)&&cache.TravelTime(SIZE_MAX)==0&&cache.Reachability(SIZE_MAX)==0);++count;
 }assert(count==2);
 unsigned rowIndex=0;for(const auto row:view.Visibility()){
  unsigned char out[]={17,17,17,17,17};assert(rc::DecompressVisibility(row,3,out+1,3));
  assert(out[0]==17&&out[4]==17&&out[1]==(rowIndex==2)&&out[2]==0&&out[3]==(rowIndex==2));++rowIndex;
 }assert(rowIndex==3&&view.Waypoint(2,2)==30&&view.Waypoint(3,0)==0&&view.Waypoint(0,3)==0);
 rc::View empty;assert(empty.AreaCount()==0&&empty.Waypoint(0,0)==0);
 for(const auto c:empty.Caches()){(void)c;assert(false);}for(const auto v:empty.Visibility()){(void)v;assert(false);}
 auto old=view.Caches().data;
 const auto reject=[&](const auto &bad,rc::Error expected=rc::Error::None){
  const auto result=parse(bad);assert(result!=rc::Error::None);
  if(expected!=rc::Error::None)assert(result==expected);
  assert(view.Caches().data==old&&view.AreaCount()==3);
 };
 assert(rc::Parse(nullptr,bytes.size(),rc::Version::SP,signature,view)==rc::Error::Header);
 assert(rc::Parse(bytes.data(),rc::MaxFileBytes+1,rc::Version::SP,signature,view)==rc::Error::Header);
 for(size_t n=0;n<bytes.size();++n)reject(std::vector<unsigned char>(bytes.begin(),bytes.begin()+n));
 auto change=[&](size_t at,uint32_t value,rc::Error error){auto bad=bytes;put(bad,at,value);reject(bad,error);};
 change(0,0,rc::Error::Identity);change(4,12,rc::Error::Version);
 for(unsigned at:{8u,12u})change(at,UINT32_MAX,rc::Error::Map);
 for(unsigned at:{16u,20u,24u})change(at,UINT32_MAX,rc::Error::Checksum);
 for(unsigned at:{28u,32u})change(at,UINT32_MAX,rc::Error::Extent);
 change(36,51,rc::Error::Cache);change(36,54,rc::Error::Cache);change(36,52+3*65536,rc::Error::Cache);
 change(36,52+3*1000,rc::Error::Extent);
 for(unsigned at:{44u,48u}){change(at,0,rc::Error::Cache);change(at,UINT32_MAX,rc::Error::Cache);}
 for(unsigned at:{40u,52u,56u,60u,64u})for(uint32_t value:{0x7fc00000u,0x7f800000u,0xff800000u})
  change(at,value,rc::Error::Geometry);
 change(52,0x7f7fffffu,rc::Error::Geometry);change(64,0xbf800000u,rc::Error::Geometry);
 const size_t vis=36+55+58;
 change(vis,UINT32_MAX,rc::Error::Extent);
 change(vis+4,1,rc::Error::Visibility);
 for(unsigned count:{0u,2u,4u}){auto bad=bytes;bad[vis+9]=static_cast<unsigned char>(count);reject(bad,rc::Error::Visibility);}
 change(bytes.size()-4,0x7fc00000u,rc::Error::Geometry);
 auto extra=bytes;extra.push_back(0);reject(extra,rc::Error::Trailing);
 // On-disk pointer words and the four unused bytes after meaningful times
 // are padding, not addresses or extra travel entries.
 auto ignored=bytes;for(size_t at:{72u,76u,80u,86u})ignored[at]^=0xff;
 assert(parse(ignored)==rc::Error::None);assert(rc::ValidateTopology(view,rc::AAS8Topology(map))==rc::Error::None);
 auto mp=bytes;put(mp,4,12);assert(parse(mp,rc::Version::MP)==rc::Error::None);
 assert(parse(bytes,rc::Version::MP)==rc::Error::Version);
 assert(parse(bytes,static_cast<rc::Version>(99))==rc::Error::Version);
 auto unaligned=bytes;unaligned.insert(unaligned.begin(),0xa5);
 assert(rc::Parse(unaligned.data()+1,bytes.size(),rc::Version::SP,signature,view)==rc::Error::None);
 assert(rc::ValidateTopology(view,rc::AAS8Topology(map))==rc::Error::None);
 // Malformed RLE and undersized outputs leave caller memory untouched.
 for(const auto bad:std::vector<std::vector<unsigned char>>{{0},{0,0},{0,4},{0,2},{0,3,1,1}}){
  unsigned char out[]={9,9,9};assert(!rc::DecompressVisibility({bad.data(),bad.size()},3,out,3));
  assert(out[0]==9&&out[1]==9&&out[2]==9);
 }
 unsigned char out[]={9,9,9},row[]={7,3};
 assert(!rc::DecompressVisibility({row,2},3,out,2)&&out[0]==9);
 assert(!rc::DecompressVisibility({nullptr,2},3,out,3)&&out[0]==9);
 assert(!rc::DecompressVisibility({row,2},3,nullptr,3));
 assert(rc::DecompressVisibility({},0,nullptr,0));
 for(unsigned length:{1u,2u,254u,255u,256u,4096u,65535u}){
  std::vector<unsigned char> compressed,decoded(length+2,77);
  for(unsigned left=length;left;){const unsigned n=left>255?255:left;compressed.push_back(9);compressed.push_back(n);left-=n;}
  assert(rc::DecompressVisibility({compressed.data(),compressed.size()},length,decoded.data()+1,length));
  assert(decoded.front()==77&&decoded.back()==77);for(unsigned i=1;i<=length;++i)assert(decoded[i]==1);
 }
}
''', flags=['-O2', '-ffast-math'])

    def test_cache_counts_membership_reachability_and_portal_indices(self):
        self.compile_and_run(fixture_source() + r'''
struct Topology {
 std::vector<rc::Area> areas={{0,0,0},{1,0,1},{1,1,1}};
 std::vector<rc::Cluster> clusters={{0,0},{2,2}};
 std::vector<rc::Portal> portals={{0,{0,0},{0,0}}};
 size_t AreaCount()const{return areas.size();}size_t ClusterCount()const{return clusters.size();}
 size_t PortalCount()const{return portals.size();}
 rc::Area GetArea(size_t i)const{return areas.at(i);}rc::Cluster GetCluster(size_t i)const{return clusters.at(i);}
 rc::Portal GetPortal(size_t i)const{return portals.at(i);}
};
int main(){
 aa::View map;assert(aa::Parse(mapBytes,sizeof(mapBytes),0xcafebabe,map)==aa::Error::None);
 auto signature=rc::MapSignature(map);std::vector<unsigned char> bytes(std::begin(initial),std::end(initial));rc::View view;
 assert(rc::Parse(bytes.data(),bytes.size(),rc::Version::SP,signature,view)==rc::Error::None);
 Topology topology;assert(rc::ValidateTopology(view,topology)==rc::Error::None);
 auto reject=[&](Topology bad){assert(rc::ValidateTopology(view,bad)!=rc::Error::None);};
 {auto bad=topology;bad.areas.clear();reject(bad);}
 {auto bad=topology;bad.clusters.clear();reject(bad);}
 {auto bad=topology;bad.portals.clear();reject(bad);}
 for(int value:{-1,INT_MIN,INT_MAX}){auto bad=topology;bad.clusters[1].areas=value;reject(bad);}
 for(int value:{-1,1,3}){auto bad=topology;bad.clusters[1].reachable=value;reject(bad);}
 {auto bad=topology;bad.areas[2].local=0;reject(bad);}
 {auto bad=topology;bad.areas[2].local=2;reject(bad);}
 {auto bad=topology;bad.areas[2].cluster=INT_MIN;reject(bad);}
 {auto bad=topology;bad.areas[2].cluster=2;reject(bad);}
 {auto bad=topology;bad.areas[2].cluster=0;reject(bad);}
 {auto bad=topology;bad.areas[2].reachabilities=0;reject(bad);}
 {auto bad=topology;bad.areas[2].reachabilities=-1;reject(bad);}
 // A goal may have no outgoing edge; its self-distance does not use one.
 {auto valid=topology;valid.areas[1].reachabilities=0;assert(rc::ValidateTopology(view,valid)==rc::Error::None);}
 // Corrupted used offsets fail even though the container layout remains valid.
 bytes[36+55+52+4+1]=1;
 assert(rc::Parse(bytes.data(),bytes.size(),rc::Version::SP,signature,view)==rc::Error::None);
 assert(rc::ValidateTopology(view,topology)==rc::Error::Cache);
 bytes[36+55+52+4+1]=0;
 // Add a real portal on both sides and remove the synthetic portal-cache
 // record so the area-cache bytes and entry count stay the same.
 bytes.erase(bytes.begin()+36,bytes.begin()+36+55);put(bytes,28,0);put(bytes,12,3);signature.clusters=3;
 topology.areas[2].cluster=-1;topology.portals.push_back({2,{1,2},{1,0}});topology.clusters.push_back({1,1});
 assert(rc::Parse(bytes.data(),bytes.size(),rc::Version::SP,signature,view)==rc::Error::None);
 assert(rc::ValidateTopology(view,topology)==rc::Error::None);
 {auto bad=topology;bad.portals[1].area=1;reject(bad);}
 {auto bad=topology;bad.portals[1].clusters[1]=1;reject(bad);}
 {auto bad=topology;bad.portals[1].locals[1]=1;reject(bad);}
 {auto bad=topology;bad.portals[1].clusters[1]=INT_MIN;reject(bad);}
 {auto bad=topology;bad.areas[1].cluster=0;reject(bad);}
}
''', flags=['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
