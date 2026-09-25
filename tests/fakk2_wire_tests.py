"""Heavy Metal: F.A.K.K.2's wire as retail fakk2.exe 1.02 reads it.

The official 1.02 patch installs a plain fakk2.exe; the disc's is encrypted.
Played back by that executable, an fnTech3 demo was first refused
("CL_ParseGamestate: bad command byte"): its entity and player bodies were
Alice's. With F.A.K.K.2's own tables it reached the first snapshot and ran off
the end of it: Ritual's snapshots carry a byte fnTech3 did not send. These
checks hold both corrections, and the sprites and HUD the same comparison
showed, in the source; tests/fakk2_message_codec_tests.cpp exercises the
codec itself. With FNTECH3_FAKK2_EXE (or the patch extracted under
.tmp/fakk2-patch) the generated tables are also compared with the executable.
"""
from pathlib import Path
import os
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = Path(os.environ.get('FNTECH3_FAKK2_EXE', str(
    ROOT / '.tmp' / 'fakk2-patch' / 'files' / 'Program Executable Files' / 'fakk2.exe')))


def read(path):
    return (ROOT / path).read_text(encoding='utf-8', errors='replace')


def function(text, signature):
    start = text.index(signature)
    depth = 0
    for index in range(text.index('{', start), len(text)):
        if text[index] == '{':
            depth += 1
        elif text[index] == '}':
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise AssertionError(f'unterminated {signature}')


class Fakk2WireTests(unittest.TestCase):
    def test_msg_codes_fakk2s_bodies_at_all_four_sites(self):
        msg = read('code/qcommon/msg.cpp')
        self.assertIn('#include "../compat/fakk2_message_codec.hpp"', msg)
        self.assertEqual(msg.count('if ( msg_fakk2Bodies ) {'), 4)
        for call in ('MSG_WriteFAKK2Entity( io, base, sent, force != qfalse )',
                     'MSG_ReadFAKK2Entity( io, base, decoded, number )',
                     'fntech3::fakk2::message::WritePlayer( io, base, sent )',
                     'fntech3::fakk2::message::ReadPlayer( io, base, decoded )'):
            self.assertIn(call, msg)
        # The switch is set once, from the immutable profile, where it is chosen.
        common = function(read('code/qcommon/common.c'), 'static void Com_InitGameProfile( void ) {')
        activate = common.index('Com_ActivateGameProfile( detection.profile->id )')
        self.assertGreater(common.index('MSG_UseFAKK2Bodies( Com_GameIsFAKK2() ? qtrue : qfalse );'), activate)

    def test_ritual_snapshots_carry_the_time_residual(self):
        snapshot = read('code/server/sv_snapshot.cpp')
        residual = function(snapshot, 'static void SV_WriteRitualTimeResidual( const client_t *client, msg_t *msg ) {')
        self.assertIn('MSG_WriteByte( msg, std::clamp( sv.timeResidual, 0, 255 ) );', residual)
        self.assertIn('NETCHAN_WIRE_ALICE', residual)
        for writer in ('static void SV_WriteSnapshotToClient( const client_t *client, msg_t *msg ) {',
                       'void SV_WriteLegacyDownloadSnapshot( const client_t *client, msg_t *msg ) {'):
            body = function(snapshot, writer)
            time = body.index('sv.time')
            self.assertLess(time, body.index('SV_WriteRitualTimeResidual( client, msg );'))
            self.assertLess(body.index('SV_WriteRitualTimeResidual( client, msg );'),
                            body.index('MSG_WriteByte', body.index('SV_WriteRitualTimeResidual( client, msg );')))
        parse = function(read('code/client/cl_parse.cpp'), 'static void CL_ParseSnapshot( msg_t *msg ) {')
        server_time = parse.index('newSnap.serverTime = MSG_ReadLong( msg );')
        residual_read = parse.index('(void)MSG_ReadByte( msg );')
        self.assertLess(server_time, residual_read)
        self.assertLess(residual_read, parse.index('deltaNum = MSG_ReadByte( msg );'))

    def test_fakk2_draws_hidden_emitters_sprites(self):
        sprite = function(read('code/client/client_alice_cgame.hpp'),
                          'void QDECL CL_AliceAddRefSpriteToScene( const fntech3_alice_refEntity_t *e ) {')
        self.assertIn('( !Com_GameIsFAKK2() && ( e->renderfx & FNTECH3_ALICE_RF_DONTDRAW ) )', sprite)

    def test_the_weapon_hud_shows_with_empty_hands(self):
        hud = read('code/compat/fakk2_hud.hpp')
        select = function(hud, 'inline Selection Select( const int *stats, const int *activeItems ) {')
        self.assertIn('s.dualWeapon = ReadSlot( activeItems, ItemWeaponDual ) >= 0;', select)
        self.assertIn('s.weapon = !s.dualWeapon;', select)

    def test_protocol_one_rings_the_profiles_window(self):
        # Alice and F.A.K.K.2 share protocol 1 and its wire but not the window:
        # alice.exe rings 128 reliable commands and fakk2.exe 1.02 rings 64, so
        # a client on that wire rings the profile's count, as a bot does.
        server = function(read('code/server/server.h'), 'static inline int SV_ReliableCommandLimit(')
        self.assertIn('if ( client->netchan.remoteAddress.type == NA_BOT\n'
                      '\t\t|| client->netchan.wireProfile == NETCHAN_WIRE_ALICE ) {\n'
                      '\t\treturn Com_GameProfile()->bots.reliableCommands;', server)
        client = function(read('code/client/client.h'), 'static inline int CL_ReliableCommandLimit(')
        self.assertIn('return wire == NETCHAN_WIRE_ALICE ? Com_GameProfile()->bots.reliableCommands\n'
                      '\t\t: Netchan_ReliableCommandLimit( wire );', client)
        catalog = read('code/qcommon/game_profile.cpp')
        fakk2 = catalog[catalog.index('\tCOM_GAME_FAKK2,'):catalog.index('\tCOM_GAME_MOHAA,')]
        self.assertIn('{ "0", "1", "1024", 1024, 0, 0, 0, 0, 0, 64 },', fakk2)
        self.assertIn('128, 1200, 768, 384, COM_ENTITY_HULL_TRACE_SHAPE,', fakk2)

    def test_retail_bounds_the_profile_holds(self):
        if not EXE.is_file():
            self.skipTest('fakk2.exe 1.02 not present (set FNTECH3_FAKK2_EXE)')
        sys.path.insert(0, str(ROOT / 'scripts'))
        from generate_fakk2_wire_tables import Image
        image = Image(EXE)
        pinned = {
            0x004468dd: '83 fa 41',           # SV_AddServerCommand drops at the 65th
            0x004468f5: '83 e0 3f',           # and rings 64
            0x0041ffaf: '83 f9 40',           # CL_AddReliableCommand refuses past 64
            0x0041ffd2: '83 e0 3f',           # and rings 64
            0x0041cdaa: '8d 51 c0',           # a server command 64 back is cycled out
            0x0042575f: '81 fe 80 01 00 00',  # CM_LoadSubmodels admits 384
            0x00426702: '81 fe 7f 01 00 00',  # CM_ClipHandleToModel's box is 383
            0x00443bad: '68 00 c0 00 00',     # a 48 KB server message
        }
        for va, expected in pinned.items():
            size = len(bytes.fromhex(expected))
            self.assertEqual(image.data[va - image.base:va - image.base + size].hex(' '), expected, hex(va))

    def test_generated_tables_match_retail(self):
        if not EXE.is_file():
            self.skipTest('fakk2.exe 1.02 not present (set FNTECH3_FAKK2_EXE)')
        run = subprocess.run([sys.executable, str(ROOT / 'scripts' / 'generate_fakk2_wire_tables.py'),
                              '--exe', str(EXE), '--check'], capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
