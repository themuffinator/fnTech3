"""Execute the production SV_UserMove body with isolated packet/game hooks."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from spawn_view_angle_source_tests import function_body

from compiler_support import compiler_target_args


ROOT = Path(__file__).resolve().parents[1]


HARNESS = r'''
#include <array>
#include <cstdlib>
#include <vector>
#include <numeric>
#include "netchan_profile.h"
#include "netchan_safety.hpp"

constexpr int MAX_PACKET_USERCMDS = 32;
constexpr int MAX_RELIABLE_COMMANDS = 64;
constexpr int PACKET_BACKUP = 32;
constexpr int PACKET_MASK = PACKET_BACKUP - 1;
[[maybe_unused]] constexpr int ERR_DROP = 1;
enum { CS_ZOMBIE, CS_PRIMED, CS_ACTIVE };
struct usercmd_t { int serverTime = 0; };
struct msg_t {
    int count = 3;
    int failAt = -1;
    int decoded = 0;
    netchanWireProfile_t expectedProfile = NETCHAN_WIRE_LEGACY_Q3;
};
struct client_t {
    int state = CS_ACTIVE;
    int deltaMessage = 0;
    int messageAcknowledge = 1;
    int reliableAcknowledge = 0;
    struct {
        int outgoingSequence = 10;
        netchanWireProfile_t wireProfile = NETCHAN_WIRE_LEGACY_Q3;
    } netchan;
    const char *reliableCommands[MAX_RELIABLE_COMMANDS]{};
    struct { int messageAcked = 0; } frames[PACKET_BACKUP];
    bool gotCP = true;
    bool pureAuthentic = true;
    int gamestate_rate = 0;
    const char *name = "test-client";
    usercmd_t lastUsercmd{};
    int dropped = 0;
    int entered = 0;
    int thought = 0;
    int gamestates = 0;
};
struct { int checksumFeed = 1234; int pure = 0; } sv;

auto SV_Indices(int count) {
    std::vector<int> indices(count);
    std::iota(indices.begin(), indices.end(), 0);
    return indices;
}
int MSG_ReadByte(msg_t *msg) { return msg->count; }
int MSG_HashKeyForWireProfile(netchanWireProfile_t, const char *, int) { return 0; }
bool MSG_ReadDeltaUsercmdKeyForWireProfile(msg_t *msg, int,
    const usercmd_t *, usercmd_t *out, netchanWireProfile_t profile) {
    const int index = msg->decoded++;
    if (profile != msg->expectedProfile || index == msg->failAt) return false;
    out->serverTime = 100 + index;
    return true;
}
void Com_Printf(const char *, ...) {}
void Com_DPrintf(const char *, ...) {}
void Com_Error(int, const char *, ...) { std::abort(); }
int Sys_Milliseconds() { return 1000; }
bool SVC_RateLimit(int *, int, int) { return false; }
void SV_DropClient(client_t *client, const char *) {
    ++client->dropped;
    client->state = CS_ZOMBIE;
}
void SV_SendClientGameState(client_t *client) { ++client->gamestates; }
void SV_ClientEnterWorld(client_t *client, const usercmd_t *cmd) {
    ++client->entered;
    client->state = CS_ACTIVE;
    client->lastUsercmd = *cmd;
}
void SV_ClientThink(client_t *client, const usercmd_t *cmd) {
    ++client->thought;
    client->lastUsercmd = *cmd;
}

// The entire SV_UserMove implementation is inserted here from sv_client.cpp.
@SV_USER_MOVE@

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)
int main() {
    for (const auto profile : {NETCHAN_WIRE_LEGACY_Q3,
        NETCHAN_WIRE_IOQ3, NETCHAN_WIRE_QL_RETAIL}) {
        for (const int state : {CS_PRIMED, CS_ACTIVE}) {
            for (int failAt = 0; failAt < 3; ++failAt) {
                client_t sender{};
                sender.state = state;
                sender.netchan.wireProfile = profile;
                client_t peer{};
                peer.netchan.wireProfile = profile;
                msg_t malformed{};
                malformed.expectedProfile = profile;
                malformed.failAt = failAt;
                SV_UserMove(&sender, &malformed, true);
                CHECK(sender.dropped == 1);
                CHECK(sender.state == CS_ZOMBIE);
                CHECK(sender.entered == 0 && sender.thought == 0);
                CHECK(sender.gamestates == 0);
                CHECK(sender.frames[1].messageAcked == 0);
                CHECK(sender.lastUsercmd.serverTime == 0);
                CHECK(malformed.decoded == failAt + 1);
                CHECK(peer.state == CS_ACTIVE && peer.dropped == 0);

                // A separate connected client continues to submit commands
                // after the malformed sender has been disconnected.
                msg_t valid{};
                valid.expectedProfile = profile;
                SV_UserMove(&peer, &valid, true);
                CHECK(peer.dropped == 0 && peer.state == CS_ACTIVE);
                CHECK(peer.thought == 3);
                CHECK(peer.lastUsercmd.serverTime == 102);
                CHECK(peer.frames[1].messageAcked == 1000);
            }
        }
    }
    return 0;
}
'''


class ServerUserMoveTests(unittest.TestCase):
    def test_bad_commands_disconnect_only_the_sender_before_game_execution(self) -> None:
        compiler = shutil.which("clang++") or shutil.which("g++") or shutil.which("cl")
        if compiler is None:
            self.skipTest("A C++20 compiler is required for the SV_UserMove harness")
        source = (ROOT / "code/server/sv_client.cpp").read_text(encoding="utf-8")
        body = function_body(source, "SV_UserMove")
        self.assertTrue(body, "SV_UserMove definition was not found")
        temporary_root = ROOT / ".tmp"
        temporary_root.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="server-user-move-", dir=temporary_root) as folder:
            folder = Path(folder)
            harness = folder / "server_user_move.cpp"
            executable = folder / ("server_user_move.exe" if os.name == "nt" else "server_user_move")
            harness.write_text(HARNESS.replace("@SV_USER_MOVE@", body), encoding="utf-8")
            include = str(ROOT / "code/qcommon")
            if Path(compiler).stem.lower() == "cl":
                command = [compiler, *compiler_target_args(compiler), "/nologo", "/std:c++17", "/EHsc", "/W4", "/WX",
                           "/I" + include, str(harness), "/Fe:" + str(executable)]
            else:
                command = [compiler, *compiler_target_args(compiler), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                           "-I" + include, str(harness), "-o", str(executable)]
            built = subprocess.run(command, cwd=folder, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
