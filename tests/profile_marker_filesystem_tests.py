"""Exercise bounded early archive identity independently of filesystem startup."""
from pathlib import Path
import shutil, struct, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]


def metadata_pak(profile, comment=b''):
    # Synthetic directory metadata only: deliberately no retail payloads.
    entries={
        'q3': [('default.cfg',1737,0x9a18b2fd),('maps/q3dm6.bsp',4408212,0x9fef9a4b)],
        'ql': [('default.cfg',1580,0xc6c9f233),('maps/campgrounds.bsp',2924532,0x0ddc0f1d)],
        'rtcw-sp': [('default.cfg',2237,0x844222c3),('maps/escape1.bsp',12366352,0x5f08b87c)],
        'rtcw-mp': [('default.cfg',1372,0x91dfaca0),('maps/mp_beach.bsp',7343512,0x77533d56)],
    }[profile]
    local=bytearray();directory=bytearray()
    for name,length,crc in entries:
        name=name.encode();offset=len(local)
        local+=struct.pack('<I5H3I2H',0x04034b50,20,0,0,0,0,crc,1,length,len(name),0)+name+b'x'
        directory+=struct.pack('<I6H3I5H2I',0x02014b50,20,20,0,0,0,0,crc,1,length,len(name),0,0,0,0,0,offset)+name
    return bytes(local+directory+struct.pack('<I4H2IH',0x06054b50,0,0,2,2,len(directory),len(local),len(comment))+comment)


class ProfileMarkerFilesystemTests(unittest.TestCase):
    def test_filesystem_entries_and_unrepresentable_paths(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        harness=r'''
#include "game_profile.h"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>
int main(int argc,char **argv) {
 assert(argc==3 || argc==4);
 const unsigned expected=std::strtoul(argv[2],nullptr,10);
 assert(Com_ProbeGameProfileMarkers(nullptr)==0 && Com_ProbeGameProfileMarkers("")==0);
 std::string candidate=argv[1];
 if(argc==4)candidate+="/caf\xc3\xa9";
 assert(Com_ProbeGameProfileMarkers(candidate.c_str())==expected);
 comGameMarkerInputs_t inputs={"auto",expected,COM_GAME_MARKER_QUAKELIVE,0};
 auto detected=Com_DetectGameProfileMarkers(&inputs);
 if(expected & COM_GAME_MARKER_UNRECOGNIZED_PAK) {
  assert(detected.status==COM_GAME_DETECTION_UNRECOGNIZED_PAK && !detected.profile);
  assert(detected.evidence==COM_GAME_EVIDENCE_BASE_PATH);
 }
 inputs.requestedProfile="q3";
 assert(Com_DetectGameProfileMarkers(&inputs).profile->id==COM_GAME_QUAKE3);
 const std::string invalid=std::string(argv[1])+"/\xff\xfe";
 assert(Com_ProbeGameProfileMarkers(invalid.c_str())==0);
 const std::string huge(40000,'x');
 assert(Com_ProbeGameProfileMarkers(huge.c_str())==0);
}
'''
        scratch=ROOT/'.tmp';scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='profile-markers-',dir=scratch) as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';fixture=Path(folder)/'fixture'
            base=fixture/'baseq3';base.mkdir(parents=True)
            cpp.write_text(harness)
            subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-I',str(ROOT/'code/qcommon'),str(cpp),str(ROOT/'code/qcommon/game_profile.cpp'),'-o',str(exe)],check=True,capture_output=True,text=True)
            def check(expected, unicode=False):
                run=subprocess.run([str(exe),str(fixture),str(expected)]+(["unicode"] if unicode else []),capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            check(0)
            q3=base/'pak0.pk3';ql=base/'pak00.pk3'
            q3.mkdir();check(0);q3.rmdir()
            for payload in (b'',b'x',b'PK\x03\x04',metadata_pak('q3')[:-1]):
                q3.write_bytes(payload);check(4)
            q3.write_bytes(metadata_pak('q3'));check(1)
            ql.write_bytes(metadata_pak('ql'));check(3)
            q3.unlink();check(2)
            # Valid-looking metadata under the other profile's filename is not evidence.
            ql.write_bytes(metadata_pak('q3'));check(4);ql.unlink()
            for profile,wrong_name in (('ql',q3),('q3',ql)):
                wrong_name.write_bytes(metadata_pak(profile));check(4);wrong_name.unlink()
            q3.write_bytes(metadata_pak('q3',b'comment'));check(1)
            q3.write_bytes(metadata_pak('q3',b'x'*65535));check(1)
            original=metadata_pak('q3');eocd=len(original)-22;central=original.index(b'PK\x01\x02')
            mutations=[(eocd+4,b'\x01\x00'),(eocd+10,b'\xff\xff'),
                       (eocd+12,struct.pack('<I',9*1024*1024)),
                       (eocd+16,struct.pack('<I',0xfffffff0)),
                       (central,b'bad!'),(central+28,b'\xff\xff'),
                       (central+8,b'\x01\x00'),(central+10,b'\x63\x00'),
                       (central+42,struct.pack('<I',0xfffffff0)),
                       (central+16,b'\x00\x00\x00\x00')]
            for offset,value in mutations:
                malformed=bytearray(original);malformed[offset:offset+len(value)]=value
                q3.write_bytes(malformed);check(4)
            unicode=fixture/'caf\u00e9';(unicode/'baseq3').mkdir(parents=True)
            (unicode/'baseq3/pak0.pk3').write_bytes(metadata_pak('q3'));check(1,True)
            q3.unlink()
            main=fixture/'main';main.mkdir()
            sp=main/'pak0.pk3';mp=main/'mp_pak0.pk3'
            sp.write_bytes(metadata_pak('rtcw-sp'));check(8)
            mp.write_bytes(metadata_pak('rtcw-mp'));check(24)
            q3.write_bytes(metadata_pak('q3'));check(25);q3.unlink()
            sp.unlink();check(16)
            for payload in (b'',b'PK\x03\x04',metadata_pak('rtcw-mp')[:-1],metadata_pak('rtcw-sp')):
                mp.write_bytes(payload);check(4)
            mp.unlink()
            sp.write_bytes(metadata_pak('rtcw-mp'));check(4)
            sp.write_bytes(metadata_pak('rtcw-sp',b'comment'));check(8)


if __name__=='__main__':unittest.main()
