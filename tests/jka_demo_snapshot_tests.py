"""Run the production demo snapshot writer against the real message codec."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
extern "C" {
#include "q_shared.h"
#include "qcommon.h"
}
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#define CHECK(test) do { if (!(test)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#test); return __LINE__; } } while(false)
extern "C" void QDECL Com_Printf(const char *,...) {}
extern "C" void QDECL Com_Error(errorParm_t,const char *,...) { std::abort(); }
struct clSnapshot_t {
    int messageNum, serverTime, snapFlags, areabytes, numEntities, parseEntitiesNum;
    byte areamask[32];
    playerState_t ps, jkaVehiclePs;
};
struct Client { clSnapshot_t snap, snapshots[32]; entityState_t parseEntities[1024]; } cl;
struct Connection { int demoDeltaNum, reliableSequence, demoplaying, demoMessageSequence,
    serverMessageSequence, recordfile; } clc;
constexpr int MAX_PARSE_ENTITIES=1024;
netchanWireProfile_t active = NETCHAN_WIRE_JKA_MP;
netchanWireProfile_t CL_ActiveWireProfile() { return active; }
qboolean CL_LegacyFraming() { return qfalse; }
void CL_WriteServerCommands(msg_t *) {}
void CL_WriteDeltaPlayerstateForActiveProfile(msg_t *msg,const playerState_t *from,const playerState_t *to) {
    if (!MSG_WriteDeltaPlayerstateForWireProfile(msg,from,to,active)) std::abort();
}
void CL_EmitPacketEntities(clSnapshot_t *,clSnapshot_t *,msg_t *msg,entityState_t *) {
    MSG_WriteBits(msg,MAX_GENTITIES-1,GENTITYNUM_BITS);
}
std::vector<byte> saved;
void FileWrite(int,const void *bytes,int length) {
    const auto *data=static_cast<const byte *>(bytes); saved.insert(saved.end(),data,data+length);
}
'''

CHECKS = r'''
int Read(const clSnapshot_t &expected,const clSnapshot_t *base,int delta,bool vehicle) {
    CHECK(saved.size()>=8);
    int sequence,length; std::memcpy(&sequence,saved.data(),4); std::memcpy(&length,saved.data()+4,4);
    CHECK(sequence==22 && length==int(saved.size()-8));
    msg_t msg; MSG_Init(&msg,saved.data()+8,length); msg.cursize=length; MSG_BeginReading(&msg);
    CHECK(MSG_ReadLong(&msg)==77 && MSG_ReadByte(&msg)==svc_snapshot);
    CHECK(MSG_ReadLong(&msg)==expected.serverTime && MSG_ReadByte(&msg)==delta);
    CHECK(MSG_ReadByte(&msg)==expected.snapFlags && MSG_ReadByte(&msg)==0);
    playerState_t ps{};
    CHECK(MSG_ReadDeltaPlayerstateForWireProfile(&msg,base?&base->ps:nullptr,&ps,active));
    CHECK(ps.commandTime==expected.ps.commandTime);
    if(active==NETCHAN_WIRE_JKA_MP) CHECK(ps.raven.m_iVehicleNum==expected.ps.raven.m_iVehicleNum);
    if(vehicle) {
        playerState_t state{};
        MSG_ReadDeltaJKAVehiclePlayerstate(&msg,base&&base->ps.raven.m_iVehicleNum?&base->jkaVehiclePs:nullptr,&state);
        CHECK(state.commandTime==expected.jkaVehiclePs.commandTime);
        CHECK(state.origin[0]==expected.jkaVehiclePs.origin[0] && state.stats[0]==expected.jkaVehiclePs.stats[0]);
    }
    CHECK(MSG_ReadEntitynum(&msg)==MAX_GENTITIES-1);
    CHECK(MSG_ReadByte(&msg)==Netchan_ServerEOFOpcode(active) && msg.readcount<=msg.cursize);
    return 0;
}
int main() {
    clc.reliableSequence=77; clc.serverMessageSequence=22; clc.demoMessageSequence=3;
    clSnapshot_t first{}; first.serverTime=1000; first.snapFlags=2; first.ps.commandTime=1000;
    first.ps.raven.m_iVehicleNum=9; first.ps.eFlags=256;
    first.jkaVehiclePs.commandTime=900; first.jkaVehiclePs.origin[0]=9; first.jkaVehiclePs.stats[0]=55;
    cl.snapshots[0]=first; cl.snap.messageNum=0; clc.demoDeltaNum=0;
    saved.clear(); CL_WriteSnapshot();
    if(const auto result=Read(first,nullptr,0,true)) return result;
    auto second=first; second.serverTime=1050; second.ps.commandTime=1050;
    second.jkaVehiclePs.commandTime=950; second.jkaVehiclePs.origin[0]=10; second.jkaVehiclePs.stats[0]=54;
    cl.snapshots[0]=second; saved.clear(); CL_WriteSnapshot();
    if(const auto result=Read(second,&first,1,true)) return result;
    auto third=second; third.serverTime=1100; third.ps.raven.m_iVehicleNum=0; third.ps.eFlags=0;
    cl.snapshots[0]=third; saved.clear(); CL_WriteSnapshot();
    if(const auto result=Read(third,&second,1,false)) return result;
    // Re-entering a vehicle after a non-vehicle frame starts a fresh vehicle base.
    cl.snapshots[0]=second; saved.clear(); CL_WriteSnapshot();
    if(const auto result=Read(second,&third,1,true)) return result;
    for(const auto profile:{NETCHAN_WIRE_LEGACY_Q3,NETCHAN_WIRE_QL_RETAIL,NETCHAN_WIRE_RTCW_SP,
        NETCHAN_WIRE_RTCW_MP,NETCHAN_WIRE_WOLFET,NETCHAN_WIRE_IOEF,NETCHAN_WIRE_JK2_MP}) {
        active=profile; clc.demoDeltaNum=0; cl.snapshots[0]=third; saved.clear(); CL_WriteSnapshot();
        if(const auto result=Read(third,nullptr,0,false)) return result;
    }
    return 0;
}
'''


class JkaDemoSnapshotTests(unittest.TestCase):
    def test_vehicle_body_and_profile_eof(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("requires a C++17 compiler")
        target = compiler_target_args(compiler)
        if os.name == "nt" and "clang" in Path(compiler).name.lower() and not target:
            target = ["--target=i686-pc-windows-msvc"]
        source = (ROOT / "code/client/cl_main.cpp").read_text(encoding="utf-8")
        start = source.index("static void CL_WriteSnapshot( void )")
        writer = source[start:source.index("\n}", start)+2]
        scratch = ROOT / ".tmp/jka-compat"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="demo-snapshot-", dir=scratch) as folder:
            path = Path(folder)
            fixture, executable, huffman = path/"probe.cpp", path/"probe.exe", path/"huffman.obj"
            fixture.write_text(PRELUDE+writer+CHECKS, encoding="utf-8")
            for command in (
                [compiler, *target, "-x", "c", "-std=c11", "-DDEDICATED", "-I", str(ROOT/"code/qcommon"),
                 "-c", str(ROOT/"code/qcommon/huffman_static.c"), "-o", str(huffman)],
                [compiler, *target, "-std=c++17", "-DDEDICATED", "-I", str(ROOT/"code/qcommon"),
                 str(fixture), str(ROOT/"code/qcommon/msg.cpp"), str(huffman), "-o", str(executable)],
            ):
                result = subprocess.run(command, capture_output=True, text=True, timeout=60)
                self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)


if __name__ == "__main__":
    unittest.main()
