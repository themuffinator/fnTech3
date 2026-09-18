"""The pure-server gamestate budget follows RTCW's own message contract."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RTCWGamestateBudgetTests(unittest.TestCase):
    def setUp(self):
        source = (ROOT / 'code/server/sv_client.cpp').read_text(encoding='utf-8')
        start = source.index('int SV_RemainingGameState( void )')
        self.body = source[start:source.index('\n}\n', start)]

    def test_rtcw_uses_its_own_wire_and_message_limit(self):
        # Original RTCW MP publishes sv_paks without any budget; fnTech3 keeps a
        # budget, but it must measure RTCW's encoding against RTCW's 32 KiB
        # message rather than Quake III's 16 KiB one (mp_sub fell to degraded
        # pure under the Q3 estimate).
        self.assertIn('const netchanWireProfile_t wire = Com_GameIsRTCW()', self.body)
        self.assertIn('Netchan_SelectWireProfile( Com_GameProfile()->preferredProtocol, 0 )', self.body)
        self.assertIn('const int limit = Com_GameIsRTCW() ? Netchan_MessageLimit( wire ) : MAX_MSGLEN;', self.body)
        self.assertIn('MSG_Init( &msg, msgBuffer.data(), limit );', self.body)
        self.assertIn('return limit - len;', self.body)
        # Every configstring and baseline is measured with the selected wire.
        self.assertEqual(self.body.count('wire );'), 4)
        self.assertEqual(self.body.count('NETCHAN_WIRE_QL_RETAIL'), 1)

    def test_quake_profiles_keep_their_conservative_estimate(self):
        self.assertIn(': NETCHAN_WIRE_QL_RETAIL;', self.body)
        netchan = (ROOT / 'code/qcommon/netchan_profile.h').read_text(encoding='utf-8')
        self.assertIn('case NETCHAN_WIRE_RTCW_MP:\n\tcase NETCHAN_WIRE_WOLFET: return 32768;', netchan)
        qcommon = (ROOT / 'code/qcommon/qcommon.h').read_text(encoding='utf-8')
        self.assertIn('#define MAX_GAME_MSGLEN 32768', qcommon)


if __name__ == '__main__':
    unittest.main()
