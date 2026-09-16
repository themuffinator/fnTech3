"""Execute AAS8 file loading, two-world map replacement and failure cleanup."""
from pathlib import Path
import struct
import unittest
import rtcw_aas_tests
import rtcw_navigation_tests
import rtcw_botlib_tests
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


def quake_aas(data):
    header = bytearray(data[:124])
    for i in range(8, 124):
        header[i] ^= ((i - 8) * 119) & 255
    lumps = []
    for i in range(14):
        offset, size = struct.unpack_from('<2I', header, 12 + 8 * i)
        lump = data[offset:offset + size]
        if i == 8:
            lump = b''.join(lump[p:p+28] for p in range(0, len(lump), 32))
        lumps.append(lump)
    struct.pack_into('<I', header, 4, 5)
    offset = 124
    for i, lump in enumerate(lumps):
        struct.pack_into('<2I', header, 12 + 8 * i, offset, len(lump))
        offset += len(lump)
    for i in range(8, 124):
        header[i] ^= ((i - 8) * 119) & 255
    return bytes(header) + b''.join(lumps)


def loader_source():
    source = rtcw_navigation_tests.navigation_source()
    source = source.replace(function(source, 'void AAS_DumpAASData(){'), 'void AAS_DumpAASData();')
    source = source.replace('void AAS_InvalidateEntities(){}', function(
        (ROOT / 'code/botlib/be_aas_entity.cpp').read_text(), 'void AAS_InvalidateEntities('))
    source += r'''
#include <map>
#include <cstdarg>
static int bspLoads, settingsLoads, legacyReachability, bspError;
int AAS_LoadBSPFile(){++bspLoads;return bspError;}
void AAS_InitSettings(){++settingsLoads;}
void AAS_InitReachability(){assert(!Com_GameIsRTCW());++legacyReachability;}
void QDECL AAS_Error(const char*,...){}
int QDECL Com_sprintf(char *out,int capacity,const char *fmt,...){
 va_list args;va_start(args,fmt);int n=vsnprintf(out,capacity,fmt,args);va_end(args);assert(n<capacity);return n;
}
static std::map<std::string,std::vector<byte>> files;
static std::vector<std::string> opened;
static std::string currentFile;
static bool fileOpen;
static int fileOffset, shortRead=-1, fileSizeOverride=-1;
static int openFile(const char *name,fileHandle_t *handle,fsMode_t mode){
 assert(mode==FS_READ&&!fileOpen);opened.push_back(name);currentFile=name;fileOffset=0;
 auto it=files.find(name);if(it==files.end()){*handle=0;return -1;}
 *handle=1;fileOpen=true;return fileSizeOverride<0?(int)it->second.size():fileSizeOverride;
}
static int readFile(void *out,int length,fileHandle_t handle){
 assert(fileOpen&&handle==1);const auto &data=files.at(currentFile);
 int size=std::min(length,(int)data.size()-fileOffset);if(shortRead>=0)size=std::min(size,shortRead);
 std::memcpy(out,data.data()+fileOffset,size);fileOffset+=size;return size;
}
static void closeFile(fileHandle_t handle){assert(handle==1&&fileOpen);fileOpen=false;}
static int seekFile(fileHandle_t handle,long offset,fsOrigin_t origin){
 assert(handle==1&&fileOpen);fileOffset=origin==FS_SEEK_CUR?fileOffset+(int)offset:(int)offset;return 0;
}
'''
    text = (ROOT / 'code/botlib/be_aas_file.cpp').read_text()
    source += text[text.index('static void AAS_SwapAASData('):text.index('static int AAS_WriteAASLump_offset;')]
    # The production include is relative to be_aas_file.cpp.
    source = source.replace('#include "rtcw_aas_loader.hpp"', '#include "code/botlib/rtcw_aas_loader.hpp"')
    source += function((ROOT / 'code/botlib/be_aas_entity.cpp').read_text(), 'void AAS_ResetEntityLinks(')
    main = (ROOT / 'code/botlib/be_aas_main.cpp').read_text()
    for signature in ('static int AAS_LoadFiles(', 'static bool AAS_ValidRTCWMapName(',
                      'static int AAS_LoadRTCWMap(', 'int AAS_LoadMap('):
        source += function(main, signature)
    source += r'''
static void initialize(){
 botimport.Print=print;botimport.FS_FOpenFile=openFile;botimport.FS_Read=readFile;
 botimport.FS_FCloseFile=closeFile;botimport.FS_Seek=seekFile;
 LibVarSet("sv_mapChecksum","-889275714");LibVarSet("max_aaslinks","8");
 assert(AAS_Setup()==0);
}
static void shutdown(){
 AAS_Shutdown();LibVarDeAllocAll();assert(allocations.empty()&&!fileOpen);
 files.clear();opened.clear();bspLoads=settingsLoads=legacyReachability=bspError=0;
 shortRead=fileSizeOverride=-1;
}
'''
    return source


CHECKS = r'''
static void checkDecoded(){
 assert(aasworld.loaded&&!aasworld.initialized&&!aasworld.savefile);
 assert(!aasworld.rtcwLoadFile&&!aasworld.rtcwLoadBytes);
 assert(aasworld.numareas==3&&aasworld.numareasettings==3&&aasworld.numnodes==2);
 assert(aasworld.rtcwGroundSteepness[1]==.25f&&aasworld.rtcwGroundSteepness[2]==.5f);
 assert(aasworld.areasettings[1].cluster==1&&aasworld.areasettings[2].firstreachablearea==1);
 assert(aasworld.reachability[0].areanum==2&&aasworld.reachability[1].areanum==1);
 assert(aasworld.reachability[1].traveltime==10&&aasworld.reachability[1].start[0]==.25f);
 assert(aasworld.nodes[1].children[0]==-1&&aasworld.nodes[1].children[1]==-2);
 assert(aasworld.planes[1].normal[0]==-1&&aasworld.bboxes[0].mins[2]==-24);
}
static void checkMaps(){
 for(const auto &p:profiles){profile=&p;initialize();
  if(!Com_GameIsRTCW()){
   files["maps/test.aas"]={std::begin(quake),std::end(quake)};
   assert(AAS_LoadMap("test")==0&&aasworld.loaded&&legacyReachability==1&&bspLoads==1);
   assert(opened==std::vector<std::string>{"maps/test.aas"}&&!aasworld.rtcwGroundSteepness);
   shutdown();continue;
  }
  for(int present:{3,1,2,0,3}){
   files.clear();opened.clear();
   if(present&1)files["maps/test_b0.aas"]={std::begin(initial),std::end(initial)};
   if(present&2)files["maps/test_b1.aas"]={std::begin(initial),std::end(initial)};
   const int before=bspLoads;
   assert(AAS_LoadMap("test")== (present?0:BLERR_CANNOTOPENAASFILE));
   assert(bspLoads==before+1&&aas_currentWorld==&aasWorlds[1]&&legacyReachability==0);
   assert((opened==std::vector<std::string>{"maps/test_b0.aas","maps/test_b1.aas"}));
   float *sidecar=nullptr;
   for(int world=0;world<2;++world){AAS_SetCurrentWorld(world);
    if(!(present&(1<<world))){assert(!aasworld.loaded&&!aasworld.rtcwGroundSteepness&&!aasworld.linkheap);continue;}
    checkDecoded();assert(aasworld.rtcwGroundSteepness!=sidecar);sidecar=aasworld.rtcwGroundSteepness;
    assert(aasworld.linkheapsize==8&&aasworld.freelinks&&aasworld.arealinkedentities);
    assert(aasworld.numreachabilityareas==5);
   }
  }
  auto *owned=aasworld.areas;const int before=bspLoads;auto n=opened.size();
  for(const char *bad:{"","../test","/test","test/..","test/.","test//a","test\\a","C:test","test/","test\n"})
   assert(AAS_LoadMap(bad)==BLERR_CANNOTOPENAASFILE);
  assert(AAS_LoadMap(std::string(52,'a').c_str())==BLERR_CANNOTOPENAASFILE);
  assert(AAS_LoadMap(nullptr)==0&&aasworld.areas==owned&&bspLoads==before&&opened.size()==n);
  assert(AAS_ValidRTCWMapName("folder/test")&&AAS_ValidRTCWMapName(std::string(51,'a').c_str()));
  // The entity table belongs only to world zero and survives a map reload.
  assert(aasWorlds[0].entities&&!aasWorlds[1].entities);
  aasWorlds[0].entities[1].areas=aasWorlds[0].linkheap;
  assert(AAS_LoadMap("test")==0&&!aasWorlds[0].entities[1].areas);
  for(int badWorld=0;badWorld<2;++badWorld){
   files["maps/test_b0.aas"]={std::begin(initial),std::end(initial)};
   files["maps/test_b1.aas"]={std::begin(initial),std::end(initial)};
   files["maps/test_b"+std::to_string(badWorld)+".aas"][4]=99;
   assert(AAS_LoadMap("test")==0&&!aasWorlds[badWorld].loaded&&aasWorlds[1-badWorld].loaded);
  }
  bspError=37;opened.clear();assert(AAS_LoadMap("test")==37&&opened.empty());
  assert(!aasWorlds[0].loaded&&!aasWorlds[1].loaded);shutdown();
 }
}
static void checkFailures(){
 profile=&profiles[2];assert(Com_GameIsRTCW());
 for(int fail=0;fail<20;++fail){initialize();files["maps/test.aas"]={std::begin(initial),std::end(initial)};
  failAllocation=fail;bool failed=false;
  try{assert(AAS_LoadAASFile("maps/test.aas")==0);checkDecoded();}catch(Drop&){failed=true;}
  failAllocation=-1;shutdown();if(!failed){assert(fail>10);break;}
 }
 initialize();files["maps/test.aas"]={std::begin(initial),std::end(initial)};
 const auto valid=files.at("maps/test.aas");
 for(int i=0;i<(int)valid.size();++i){files["maps/test.aas"]={valid.begin(),valid.begin()+i};
  assert(AAS_LoadAASFile("maps/test.aas")!=0&&!fileOpen&&!aasworld.loaded&&!aasworld.rtcwLoadBytes);
 }
 for(int offset:{0,4,8}){auto bad=valid;bad[offset]^=127;files["maps/test.aas"]=bad;
  assert(AAS_LoadAASFile("maps/test.aas")!=0&&!fileOpen&&!aasworld.loaded);
 }
 files["maps/test.aas"]=valid;shortRead=13;assert(AAS_LoadAASFile("maps/test.aas")==BLERR_CANNOTREADAASLUMP);
 shortRead=-1;fileSizeOverride=INT_MAX;assert(AAS_LoadAASFile("maps/test.aas")==BLERR_CANNOTREADAASLUMP);fileSizeOverride=-1;
 for(const char *sum:{"","0","nonsense","4294967296","-2147483649","9999999999999999999999"}){
  LibVarSet("sv_mapChecksum",sum);assert(AAS_LoadAASFile("maps/test.aas")==BLERR_WRONGAASFILEVERSION);
 }
 LibVarSet("sv_mapChecksum","3405691582");assert(AAS_LoadAASFile("maps/test.aas")==0);checkDecoded();
 assert(AAS_LoadAASFile("missing.aas")==BLERR_CANNOTOPENAASFILE&&!aasworld.loaded&&!aasworld.rtcwGroundSteepness);
 shutdown();
 // Allocation errors at any stage of either hull are recoverable on shutdown.
 int successes=0;
 for(int fail=0;fail<48;++fail){initialize();
  files["maps/test_b0.aas"]={std::begin(initial),std::end(initial)};
  files["maps/test_b1.aas"]={std::begin(initial),std::end(initial)};
  failAllocation=fail;
  try{assert(AAS_LoadMap("test")==0);++successes;}catch(Drop&){}
  failAllocation=-1;shutdown();if(successes)break;
 }
 assert(successes==1);
}
int main(){checkMaps();checkFailures();}
'''


class RTCWAASLoadTests(unittest.TestCase):
    compile_and_run = rtcw_botlib_tests.RTCWBotLibTests.compile_and_run

    def test_loader_replacement_partial_worlds_and_failure_ownership(self):
        aas = rtcw_aas_tests.synthetic_aas()
        source = loader_source()
        for name, data in [('initial', aas), ('quake', quake_aas(aas))]:
            source += '\nstatic const byte ' + name + '[]={' + ','.join(map(str, data)) + '};\n'
        self.compile_and_run(source + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')])

    def test_link_pool_small_and_invalid_capacities(self):
        source = rtcw_navigation_tests.navigation_source() + r'''
int main(){
 profile=&profiles[0];botimport.Print=print;
 for(const char *value:{"0","1","2","-1","nonsense","nan","inf","2147483648","9999999999999999999999999999999999999999999"}){
  LibVarSet("max_aaslinks",value);AAS_InitAASLinkHeap();
  const int expected=!strcmp(value,"1")?1:!strcmp(value,"2")?2:6144;
  assert(aasworld.linkheapsize==expected&&aasworld.numaaslinks==expected);
  for(int i=0;i<expected;++i){auto *link=AAS_AllocAASLink();assert(link==&aasworld.linkheap[i]);}
  assert(!aasworld.freelinks&&aasworld.numaaslinks==0);
  AAS_InitAASLinkHeap();assert(aasworld.numaaslinks==expected);
  AAS_FreeAASLinkHeap();assert(!aasworld.freelinks&&!aasworld.numaaslinks);
  LibVarDeAllocAll();assert(allocations.empty());
 }
 for(float value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),
                  -std::numeric_limits<float>::infinity(),2147483648.0f,-1.0f}){
  auto *variable=LibVar("max_aaslinks","8");variable->value=value;AAS_InitAASLinkHeap();
  assert(aasworld.linkheapsize==6144);AAS_FreeAASLinkHeap();LibVarDeAllocAll();assert(allocations.empty());
 }
 for(float value:{1.0f,1.999f,2.75f,31.125f,127.9f,6144.5f,65535.875f,1048576.0f}){
  auto *variable=LibVar("max_aaslinks","8");variable->value=value;AAS_InitAASLinkHeap();
  assert(aasworld.linkheapsize==static_cast<int>(value));
  AAS_FreeAASLinkHeap();LibVarDeAllocAll();assert(allocations.empty());
 }
}
'''
        self.compile_and_run(source,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')], ['-O2', '-ffast-math'])


if __name__ == '__main__':
    unittest.main()
