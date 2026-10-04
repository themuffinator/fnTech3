"""Execute ET's AAS8 disk boundary and ownership on generated navigation."""
from pathlib import Path
import struct
import unittest
import rtcw_aas_load_tests
import rtcw_aas_tests
import rtcw_botlib_tests

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
static void decoded(){
 assert(aasworld.loaded&&aasworld.numareas==3&&aasworld.numareasettings==3);
 assert(aasworld.rtcwGroundSteepness&&aasworld.rtcwGroundSteepness[1]==.25f);
 assert(aasworld.rtcwGroundSteepness[2]==.5f&&aasworld.areasettings[2].cluster==1);
 assert(aasworld.areasettings[2].firstreachablearea==1);
 assert(aasworld.reachabilitysize==2&&aasworld.reachability[1].traveltime==10);
 assert(!fileOpen&&!aasworld.rtcwLoadBytes&&!aasworld.rtcwLoadFile);
}
int main(){
 profile=&profiles[4];assert(Com_GameIsWolfET());
 initialize();files["maps/test.aas"]={std::begin(valid),std::end(valid)};
 assert(AAS_LoadMap("test")==0);decoded();
 assert(legacyReachability==1&&bspLoads==1&&opened==std::vector<std::string>{"maps/test.aas"});
 const auto settings=aasworld.areasettings[2];const auto steepness=aasworld.rtcwGroundSteepness[2];
 const auto opens=opened.size();
 assert(!AAS_WriteAASFile("scratch.aas")&&opened.size()==opens);
 assert(aasworld.areasettings[2].cluster==settings.cluster&&aasworld.rtcwGroundSteepness[2]==steepness);
 assert(AAS_LoadMap(nullptr)==0&&opened.size()==opens);
 assert(AAS_LoadMap("test")==0);decoded();shutdown();
 // Every incomplete length fails and owns no file or input allocation.
 for(unsigned n=0;n<sizeof(valid);++n){
  initialize();files["maps/test.aas"]={valid,valid+n};
  assert(AAS_LoadAASFile("maps/test.aas")!=0&&!fileOpen&&!aasworld.loaded&&!aasworld.rtcwLoadBytes);
  shutdown();
 }
 // All allocation stages remain reclaimable after an engine drop.
 bool success=false;
 for(int n=0;n<48;++n){
  initialize();files["maps/test.aas"]={std::begin(valid),std::end(valid)};failAllocation=n;
  try{assert(AAS_LoadMap("test")==0);decoded();success=true;}catch(Drop&){}
  failAllocation=-1;shutdown();if(success)break;
 }
 assert(success);
 initialize();files["maps/test.aas"]={std::begin(valid),std::end(valid)};
 shortRead=13;assert(AAS_LoadAASFile("maps/test.aas")==BLERR_CANNOTREADAASLUMP);shortRead=-1;
 fileSizeOverride=INT_MAX;assert(AAS_LoadAASFile("maps/test.aas")==BLERR_CANNOTREADAASLUMP);fileSizeOverride=-1;
 auto original=files.at("maps/test.aas");
 for(unsigned offset:{0u,4u,8u}){
  auto bad=original;bad[offset]^=127;files["maps/test.aas"]=bad;
  assert(AAS_LoadAASFile("maps/test.aas")!=0&&!aasworld.loaded&&!fileOpen);
 }
 files["maps/test.aas"]={std::begin(quake),std::end(quake)};
 assert(AAS_LoadAASFile("maps/test.aas")==BLERR_WRONGAASFILEVERSION);
 // ET checks its no-checksum marker in the encoded header, before XOR.
 files["maps/test.aas"]=original;LibVarSet("sv_mapChecksum","1");
 assert(AAS_LoadAASFile("maps/test.aas")==BLERR_WRONGAASFILEVERSION);
 for(unsigned i=8;i<12;++i)files["maps/test.aas"][i]=255;
 assert(AAS_LoadAASFile("maps/test.aas")==0);decoded();assert(aasworld.bspchecksum==1);
 // The marker bypasses only checksum equality, never structural validation.
 for(const auto &bad:std::vector<std::vector<byte>>{
   {std::begin(badSteepness),std::end(badSteepness)},
   {std::begin(badStride),std::end(badStride)}}){
  files["maps/test.aas"]=bad;
  assert(AAS_LoadAASFile("maps/test.aas")==BLERR_CANNOTREADAASLUMP&&!aasworld.loaded&&!fileOpen);
 }
 files["maps/test.aas"]=original;for(unsigned i=8;i<12;++i)files["maps/test.aas"][i]=255;
 // The optional ET marker never weakens RTCW's checksum boundary.
 profile=&profiles[2];assert(Com_GameIsRTCW());
 assert(AAS_LoadAASFile("maps/test.aas")==BLERR_WRONGAASFILEVERSION);
 shutdown();
}
'''


class WolfETAASLoadTests(unittest.TestCase):
    compile_and_run = rtcw_botlib_tests.RTCWBotLibTests.compile_and_run

    def test_aas8_stride_checksum_versions_and_failure_ownership(self):
        source = rtcw_aas_load_tests.loader_source()
        text = (ROOT / 'code/botlib/be_aas_file.cpp').read_text()
        source += text[text.index('static int AAS_WriteAASLump_offset;'):]
        data = rtcw_aas_tests.synthetic_aas()
        header = bytearray(data[:124])
        for i in range(8, 124):
            header[i] ^= ((i - 8) * 119) & 255
        offset, size = struct.unpack_from('<II', header, 12 + 8 * 8)
        bad_steepness = bytearray(data)
        struct.pack_into('<I', bad_steepness, offset + 32 + 28, 0x7fc00000)
        struct.pack_into('<I', header, 16 + 8 * 8, size - 1)
        for i in range(8, 124):
            header[i] ^= ((i - 8) * 119) & 255
        bad_stride = header + data[124:]
        bad_steepness[8:12] = bad_stride[8:12] = b'\xff' * 4
        for name, payload in [('valid', data), ('quake', rtcw_aas_load_tests.quake_aas(data)),
                              ('badSteepness', bad_steepness), ('badStride', bad_stride)]:
            source += '\nstatic const byte ' + name + '[]={' + ','.join(map(str, payload)) + '};\n'
        self.compile_and_run(source + CHECKS,
            [str(ROOT / 'code/server/server_game_data.cpp'), '-x', 'c++',
             str(ROOT / 'code/compat/game_abi_adapter.c')])


if __name__ == '__main__':
    unittest.main()
