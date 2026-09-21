"""Spawn/teleport view-angle anchor gates.

A client's `cl.viewangles` is a free-running accumulator: it only describes the
player's actual view relative to `ps.delta_angles`, which the game re-anchors on
every spawn from the usercmd the engine hands back through `trap_GetUsercmd()`.
That call is served from `client_t::lastUsercmd`, so any engine path that spawns
a client against a cleared or pre-gamestate command throws the view onto the raw
accumulator, and pmove clamps whatever falls outside +/-90 degrees of pitch. The
visible result is a player who spawns or teleports staring straight up.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def function_body(source: str, name: str) -> str:
    signature = re.compile(
        rf"^[^\n;]*\b{re.escape(name)}\s*\([^;{{]*\)\s*\{{", re.MULTILINE | re.DOTALL
    )
    match = signature.search(source)
    if match is None:
        return ""
    open_brace = match.end() - 1
    tokens = re.compile(
        r'''//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[{}]''',
        re.DOTALL,
    )
    depth = 0
    for token in tokens.finditer(source, open_brace):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[match.start() : token.end()]
    return ""


class SpawnViewAngleAnchorTests(unittest.TestCase):
    def test_function_extraction_ignores_comments_and_string_braces(self) -> None:
        source = r'''static void target() {
// { a disabled block
/* } another comment */
const char *value = "a } string with an escaped \" quote";
char brace = '{';
if (value) { /* nested */ }
}
static void following() { }
'''
        body = function_body(source, "target")
        self.assertTrue(body.endswith("\n}"))
        self.assertIn("if (value)", body)
        self.assertNotIn("following", body)
        self.assertEqual(function_body("static void missing() {", "missing"), "")

    def test_entering_the_world_takes_the_command_that_brought_the_client_in(self) -> None:
        self.assertIn(
            "void SV_ClientEnterWorld( client_t *client, const usercmd_t *cmd );",
            read("code/server/server.h"),
        )

    def test_anchor_is_in_place_before_the_game_spawns_the_client(self) -> None:
        body = function_body(read("code/server/sv_client.cpp"), "SV_ClientEnterWorld")

        self.assertIn("if ( cmd != nullptr ) {", body)
        self.assertIn("client->lastUsercmd = *cmd;", body)
        # GAME_CLIENT_BEGIN is what spawns the client, and the spawn reads the
        # anchor back out through trap_GetUsercmd().
        self.assertLess(
            body.index("client->lastUsercmd = *cmd;"),
            body.index("VM_Call( gvm, 1, GAME_CLIENT_BEGIN, clientNum );"),
        )

    def test_adopting_the_anchor_preserves_timestamp_acceptance_gates(self) -> None:
        """Timestamp profiles keep their gate; EF SP acknowledges ordinals and
        passes the unmodified entering command to its game module."""
        body = function_body(read("code/server/sv_client.cpp"), "SV_ClientEnterWorld")

        self.assertIn("const int acceptFrom = client->lastUsercmd.serverTime;", body)
        self.assertIn(
            "if ( !Com_GameIsSTVEFSP() && fnql::net::IsNewerCounter( acceptFrom, cmd->serverTime ) ) {",
            body,
        )
        self.assertIn("client->lastUsercmd.serverTime = acceptFrom;", body)

    def test_first_move_packet_supplies_the_anchor(self) -> None:
        body = function_body(read("code/server/sv_client.cpp"), "SV_UserMove")

        self.assertIn("SV_ClientEnterWorld( cl, &cmds[0] );", body)
        # The packet is decoded first, so the anchor is a command this client
        # really sent rather than whatever the gate reset left behind.
        self.assertLess(
            body.index("MSG_ReadDeltaUsercmdKeyForWireProfile("),
            body.index("SV_ClientEnterWorld( cl, &cmds[0] );"),
        )
        self.assertIn("cl->netchan.wireProfile", body)
        self.assertLess(
            body.index("SV_ClientEnterWorld( cl, &cmds[0] );"),
            body.index("SV_ClientThink( cl, &cmds[ i ] );"),
        )

    def test_acceptance_gate_resets_do_not_clear_the_anchor(self) -> None:
        """Both resets exist only to drop commands from before a gamestate or a
        map_restart. Clearing the whole command cleared the angles with it, and
        the game respawns players inside exactly that window."""
        cases = (
            (
                "SV_SendClientGameState",
                function_body(read("code/server/sv_client.cpp"), "SV_SendClientGameState"),
                "client->lastUsercmd.serverTime =",
                "fnql::net::CounterSubtract( sv.time, 1u );",
            ),
            (
                "SV_MapRestart_f",
                function_body(read("code/server/sv_ccmds.cpp"), "SV_MapRestart_f"),
                "client.lastUsercmd.serverTime =",
                "fnql::net::CounterSubtract( sv.time, 1u );",
            ),
        )
        for name, body, assignment, gate in cases:
            with self.subTest(function=name):
                self.assertIn(assignment, body)
                self.assertIn(gate, body)
                if name == "SV_MapRestart_f":
                    # The retained Q3 restart contract clears the old command;
                    # the QL spawn fix applies only to the QL profile branch.
                    ql_branch = body[body.index("if ( Com_GameIsQuakeLive() )",
                        body.index("for ( client_t &client : SV_Clients() )")):]
                    ql_branch, q3_branch = ql_branch.split("} else {", 1)
                    self.assertIn("client.lastUsercmd = {};", q3_branch)
                    body = ql_branch
                self.assertNotIn("lastUsercmd = {}", body)


class SpawnViewAngleUnaffectedPathTests(unittest.TestCase):
    def test_restart_and_bot_entries_keep_the_anchor_they_already_had(self) -> None:
        for source in ("code/server/sv_ccmds.cpp", "code/server/sv_init.cpp"):
            with self.subTest(source=source):
                self.assertIn(
                    "SV_ClientEnterWorld( &slot.client, nullptr );", read(source)
                )

    def test_map_restart_spawns_before_it_moves_the_gate(self) -> None:
        body = function_body(read("code/server/sv_ccmds.cpp"), "SV_MapRestart_f")
        self.assertLess(
            body.index("SV_ClientEnterWorld( &slot.client, nullptr );"),
            body.index("client.lastUsercmd.serverTime ="),
        )

    def test_every_executed_command_still_becomes_the_anchor(self) -> None:
        body = function_body(read("code/server/sv_client.cpp"), "SV_ClientThink")
        self.assertIn("cl->lastUsercmd = *cmd;", body)
        self.assertLess(
            body.index("cl->lastUsercmd = *cmd;"),
            body.index("VM_Call( gvm, 1, GAME_CLIENT_THINK, SV_ClientIndex( cl ) );"),
        )

    def test_command_acceptance_gate_retains_order_across_signed_wrap(self) -> None:
        body = function_body(read("code/server/sv_client.cpp"), "SV_UserMove")
        self.assertIn(
            "fnql::net::IsNewerCounter(", body
        )
        self.assertIn(
            "cmds[i].serverTime, cmds[cmdCount - 1].serverTime", body
        )
        self.assertIn(
            "cmds[i].serverTime, cl->lastUsercmd.serverTime", body
        )

    def test_trap_getusercmd_is_still_served_from_the_anchor(self) -> None:
        body = function_body(read("code/server/sv_game.cpp"), "SV_GetUsercmd")
        self.assertIn("*cmd = SV_ClientForIndex( clientNum ).lastUsercmd;", body)


if __name__ == "__main__":
    unittest.main()
