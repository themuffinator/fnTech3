"""Exercise Q3 version discovery using the production connection-state guard."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class Q3PointReleaseTests(unittest.TestCase):
    def test_version_reply_requires_endpoint_nonce_and_connecting_state(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        source = (ROOT / "code/client/cl_main.cpp").read_text(encoding="utf-8")
        start = source.index("static void CL_Q3ProtocolInfo(")
        function = source[start:source.index("\n}", start) + 2]
        harness = r'''
#include <cassert>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <string>
enum { CA_DISCONNECTED, CA_CONNECTING, CA_CHALLENGING, CA_CONNECTED, CA_ACTIVE };
struct netadr_t { int address, port; };
static bool q3 = true;
static struct { int state; } cls;
static struct { netadr_t serverAddress; int q3ProtocolChallenge, q3ServerProtocol, handshakeProtocol; } clc;
static bool Com_GameIsQuake3() { return q3; }
static int Com_GameProfile() { return 0; }
static bool Com_GameProfileAllowsProtocol(int, int p) { return p == 66 || p == 67 || p == 68 || p == 71; }
static bool NET_CompareAdr(const netadr_t *a, const netadr_t *b) {
    return a->address == b->address && a->port == b->port;
}
static const char *va(const char *format, int number) {
    static char text[32]; std::snprintf(text, sizeof(text), format, number); return text;
}
static const char *Info_ValueForKey(const char *info, const char *key) {
    static std::string value;
    const std::string text(info), pattern = std::string("\\") + key + "\\";
    const auto start = text.find(pattern);
    value = start == std::string::npos ? "" : text.substr(start + pattern.size(),
        text.find('\\', start + pattern.size()) - start - pattern.size());
    return value.c_str();
}
''' + function + r'''
int main() {
    const netadr_t peer{1, 27960};
    clc.serverAddress = peer;
    clc.q3ProtocolChallenge = -123;
    for (int state : { CA_CONNECTING, CA_CHALLENGING }) {
        cls.state = state;
        for (int protocol : {66, 67, 68}) {
            clc.handshakeProtocol = 66; clc.q3ServerProtocol = 0;
            const std::string info = "\\challenge\\-123\\protocol\\" + std::to_string(protocol);
            CL_Q3ProtocolInfo(&peer, info.c_str());
            assert(clc.q3ServerProtocol == protocol && clc.handshakeProtocol == protocol);
        }
    }
    for (int state : {CA_DISCONNECTED, CA_CONNECTED, CA_ACTIVE}) {
        cls.state = state; clc.q3ServerProtocol = 0;
        CL_Q3ProtocolInfo(&peer, "\\challenge\\-123\\protocol\\66");
        assert(clc.q3ServerProtocol == 0);
    }
    cls.state = CA_CHALLENGING; clc.handshakeProtocol = 68;
    for (const netadr_t bad : {netadr_t{2, 27960}, netadr_t{1, 27961}}) {
        CL_Q3ProtocolInfo(&bad, "\\challenge\\-123\\protocol\\66");
        assert(clc.q3ServerProtocol == 0 && clc.handshakeProtocol == 68);
    }
    for (const char *bad : {"\\challenge\\123\\protocol\\66", "\\protocol\\66",
        "\\challenge\\-123\\protocol\\66junk", "\\challenge\\-123\\protocol\\",
        "\\challenge\\-123\\protocol\\999999999999", "\\challenge\\-123\\protocol\\43",
        "\\challenge\\-123\\protocol\\40", "\\challenge\\-123\\protocol\\91"}) {
        CL_Q3ProtocolInfo(&peer, bad);
        assert(clc.q3ServerProtocol == 0 && clc.handshakeProtocol == 68);
    }
    q3 = false;
    CL_Q3ProtocolInfo(&peer, "\\challenge\\-123\\protocol\\66");
    assert(clc.q3ServerProtocol == 0 && clc.handshakeProtocol == 68);
}
'''
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="q3-point-release-", dir=scratch) as folder:
            cpp, exe = Path(folder) / "probe.cpp", Path(folder) / "probe.exe"
            cpp.write_text(harness, encoding="utf-8")
            build = subprocess.run([compiler, *compiler_target_args(compiler), "-std=c++17",
                                    "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)],
                                   capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            subprocess.run([str(exe)], check=True, capture_output=True, timeout=10)


if __name__ == "__main__":
    unittest.main()
