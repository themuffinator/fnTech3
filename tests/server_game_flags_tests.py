"""RTCW/ET/Elite Force server flags retain their distinct meanings at the native boundary."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class ServerGameFlagsTests(unittest.TestCase):
    def test_profile_flags_and_unknown_module_bits(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("requires a C++17 compiler")
        source = (ROOT / "code/server/sv_game.cpp").read_text(encoding="utf-8")
        start = source.index("struct ServerFlagMapping")
        end = source.index("\nvoid InvalidateServerGameData()", start)
        fixture = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/game_profile.h"
#include "code/game/g_public.h"
#include "code/compat/alice_module_abi.h"
#include "code/compat/rtcw_game_abi.h"
#include "code/compat/stvef_game_abi.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
''' + source[start:end] + r'''
static int signedBits(uint32_t value) {
    int result; std::memcpy(&result, &value, sizeof(result)); return result;
}
int main() {
    constexpr uint32_t known = 0x3ffd;
    for (auto profile : {COM_GAME_RTCW_SP, COM_GAME_RTCW_MP}) {
        assert(ServerFlagsToInternal(profile, 0x200) == SVF_CAPSULE);
        assert(ServerFlagsToInternal(profile, 0x800) == SVF_SINGLECLIENT);
        assert(ServerFlagsToInternal(profile, 0x1000) == SVF_NOSERVERINFO);
        assert(ServerFlagsToInternal(profile, 0x2000) == SVF_NOTSINGLECLIENT);
        assert(ServerFlagsToInternal(profile, 0x100) == SVF_RTCW_NOFOOTSTEPS);
        assert(ServerFlagsToInternal(profile, 0x400) == SVF_RTCW_VISDUMMY_MULTIPLE);
        assert(ServerFlagsToInternal(profile, 4) == SVF_RTCW_VISDUMMY);
        assert(ServerFlagsToInternal(profile, 16) == SVF_RTCW_CASTAI);
        assert(ServerFlagsToInternal(profile, 0xe9) == 0xe9);
        assert(ServerFlagsToInternal(profile, signedBits(~known)) == 0);
        for (uint32_t subset = 0; subset <= known; ++subset) {
            const int canonical = ServerFlagsToInternal(profile, subset);
            assert(!(canonical & (SVF_CLIENTMASK | SVF_SELF_PORTAL2)));
            for (uint32_t previous : {0u, 0x80000002u, 0xffffffffu}) {
                const uint32_t encoded = ServerFlagsFromInternal(profile, canonical, signedBits(previous));
                assert((encoded & known) == (subset & known));
                assert((encoded & ~known) == (previous & ~known));
                assert(ServerFlagsToInternal(profile, signedBits(encoded)) == canonical);
            }
        }
        const uint32_t before = 0x8000200au; // private bits, NOTSINGLECLIENT, BOT
        const int canonical = ServerFlagsToInternal(profile, signedBits(before)) & ~SVF_BOT;
        assert((uint32_t)ServerFlagsFromInternal(profile, canonical, signedBits(before)) == (before & ~8u));
        for (const auto &bit : rtcwServerFlags) {
            assert(bit.external && !(bit.external & (bit.external - 1)));
            assert(bit.internal && !(bit.internal & (bit.internal - 1)));
            for (const auto &other : rtcwServerFlags) {
                if (&bit != &other) assert(!(bit.external & other.external) && !(bit.internal & other.internal));
            }
        }
    }
    for (auto profile : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE}) {
        for (unsigned bit = 0; bit < 32; ++bit) {
            int value = signedBits(1u << bit);
            assert(ServerFlagsToInternal(profile, value) == value);
            assert(ServerFlagsFromInternal(profile, value, ~value) == value);
        }
    }
    // Holomatch: 0x2 is its shield box, not Quake III's client mask, and 0x10
    // (SVF_ELIMINATED) belongs to the module.
    constexpr uint32_t efKnown = 0xfebu;
    assert(ServerFlagsToInternal(COM_GAME_STVEF_MP, 0x2) == SVF_STVEF_SHIELD_BBOX);
    assert(ServerFlagsToInternal(COM_GAME_STVEF_MP, 0x10) == 0);
    assert(ServerFlagsToInternal(COM_GAME_STVEF_MP, 0x4) == 0);
    for (uint32_t subset = 0; subset <= 0xfff; ++subset) {
        const int canonical = ServerFlagsToInternal(COM_GAME_STVEF_MP, subset);
        assert(!(canonical & (SVF_CLIENTMASK | SVF_SELF_PORTAL2)));
        for (uint32_t previous : {0u, 0x80000014u, 0xffffffffu}) {
            const uint32_t encoded = ServerFlagsFromInternal(COM_GAME_STVEF_MP, canonical, signedBits(previous));
            assert((encoded & efKnown) == (subset & efKnown));
            assert((encoded & ~efKnown) == (previous & ~efKnown));
            assert(ServerFlagsToInternal(COM_GAME_STVEF_MP, signedBits(encoded)) == canonical);
        }
    }
    for (const auto &bit : stvefServerFlags) {
        assert(bit.external && !(bit.external & (bit.external - 1)));
        assert(bit.internal && !(bit.internal & (bit.internal - 1)));
    }
    // Alice: its bits are its own, and the two the engine tests but does not
    // map -- 0x10 (past the walk's model/sound requirement) and 0x4000 (a
    // second unconditional send) -- must drop on the way in and survive
    // untouched on the way out, so a round trip never writes a bit the
    // module did not set.
    constexpr uint32_t aliceKnown = 0xfu;
    assert(ServerFlagsToInternal(COM_GAME_ALICE, FNTECH3_ALICE_SVF_NOCLIENT) == SVF_NOCLIENT);
    assert(ServerFlagsToInternal(COM_GAME_ALICE, FNTECH3_ALICE_SVF_BOT) == SVF_BOT);
    assert(ServerFlagsToInternal(COM_GAME_ALICE, FNTECH3_ALICE_SVF_BROADCAST) == SVF_BROADCAST);
    assert(ServerFlagsToInternal(COM_GAME_ALICE, FNTECH3_ALICE_SVF_PORTAL) == SVF_PORTAL);
    assert(ServerFlagsToInternal(COM_GAME_ALICE, FNTECH3_ALICE_SVF_ALWAYS_RELEVANT) == 0);
    assert(ServerFlagsToInternal(COM_GAME_ALICE, FNTECH3_ALICE_SVF_BROADCAST_ALT) == 0);
    assert(ServerFlagsToInternal(COM_GAME_ALICE, FNTECH3_ALICE_SVF_ENGINE_VISITED) == 0);
    // Quake III's own bit positions must not survive this boundary.
    assert(ServerFlagsToInternal(COM_GAME_ALICE, SVF_BROADCAST) == 0);
    assert(ServerFlagsToInternal(COM_GAME_ALICE, SVF_PORTAL) == 0);
    for (uint32_t subset = 0; subset <= 0xffffu; ++subset) {
        const int canonical = ServerFlagsToInternal(COM_GAME_ALICE, subset);
        assert(!(canonical & (SVF_CLIENTMASK | SVF_SELF_PORTAL2)));
        for (uint32_t previous : {0u, 0x80004010u, 0xffffffffu}) {
            const uint32_t encoded = ServerFlagsFromInternal(COM_GAME_ALICE, canonical, signedBits(previous));
            assert((encoded & aliceKnown) == (subset & aliceKnown));
            assert((encoded & ~aliceKnown) == (previous & ~aliceKnown));
            assert(ServerFlagsToInternal(COM_GAME_ALICE, signedBits(encoded)) == canonical);
        }
    }
    for (const auto &bit : aliceServerFlags) {
        assert(bit.external && !(bit.external & (bit.external - 1)));
        assert(bit.internal && !(bit.internal & (bit.internal - 1)));
        for (const auto &other : aliceServerFlags) {
            if (&bit != &other) assert(!(bit.external & other.external) && !(bit.internal & other.internal));
        }
    }
    constexpr uint32_t etKnown = (known & ~0x90u) | 0x1c000u;
    assert(ServerFlagsToInternal(COM_GAME_WOLFET, 0x4000) == SVF_WOLFET_ORIGIN_ONLY);
    assert(ServerFlagsToInternal(COM_GAME_WOLFET, 0x8000) == SVF_WOLFET_SELF_PORTAL);
    assert(ServerFlagsToInternal(COM_GAME_WOLFET, 0x10000) == SVF_WOLFET_SELF_PORTAL_EXCLUSIVE);
    assert(ServerFlagsToInternal(COM_GAME_WOLFET, 0x90) == 0); // POW/BLANK are module-owned.
    for (uint32_t subset = 0; subset <= 0x1ffff; ++subset) {
        const int canonical = ServerFlagsToInternal(COM_GAME_WOLFET, subset);
        assert(!(canonical & (SVF_CLIENTMASK | SVF_SELF_PORTAL2 | SVF_RTCW_CASTAI | SVF_USE_CURRENT_ORIGIN)));
        for (uint32_t previous : {0u, 0x80000092u, 0xffffffffu}) {
            const uint32_t encoded = ServerFlagsFromInternal(COM_GAME_WOLFET, canonical, signedBits(previous));
            assert((encoded & etKnown) == (subset & etKnown));
            assert((encoded & ~etKnown) == (previous & ~etKnown));
            assert(ServerFlagsToInternal(COM_GAME_WOLFET, signedBits(encoded)) == canonical);
        }
    }
}
'''
        # Both canonical entity directions must use the tested translation.
        read = source[source.index("qboolean SV_ReadGameEntity("):source.index("qboolean SV_WriteGameEntity(")]
        write = source[source.index("qboolean SV_WriteGameEntity("):source.index("qboolean SV_ReadGameEntityByNum(")]
        self.assertIn("ServerFlagsToInternal( serverGameData.Profile(), value )", read)
        self.assertIn("ServerFlagsFromInternal(", write)
        self.assertIn("serverGameData.Profile(), source->r.svFlags, previousFlags", write)
        target = compiler_target_args(compiler)
        if os.name == "nt" and not target:
            target = ["-DFNTECH3_Q3_ONLY"]
        elif os.name != "nt":
            target = ["-m32"]
        (ROOT / ".tmp").mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="server-game-flags-", dir=ROOT / ".tmp") as temp:
            folder = Path(temp)
            cpp = folder / "probe.cpp"
            executable = folder / ("probe.exe" if os.name == "nt" else "probe")
            cpp.write_text(fixture, encoding="utf-8")
            build = subprocess.run([compiler, *target, "-std=c++17", "-I", str(ROOT),
                str(cpp), "-o", str(executable)], capture_output=True, text=True, timeout=60, cwd=ROOT)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15, cwd=ROOT)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
