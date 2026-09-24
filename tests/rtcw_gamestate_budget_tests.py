"""The pure-server gamestate budget follows RTCW's own message contract."""
from pathlib import Path
import re
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
        # Elite Force Holomatch measures its own raw-bit wire the same way, and
        # so does Alice, whose baselines start from its own null entity.
        self.assertIn(
            'const bool ownWire = Com_GameIsRTCW() || Com_GameIsSTVEFMP() || Com_GameIsAlice();',
            self.body)
        self.assertIn('MSG_NullEntityStateForWireProfile( &nullstate, wire );', self.body)
        self.assertIn('const netchanWireProfile_t wire = ownWire', self.body)
        self.assertIn('Netchan_SelectWireProfile( Com_GameProfile()->preferredProtocol, 0 )', self.body)
        self.assertIn('const int limit = ownWire ? Netchan_MessageLimit( wire ) : MAX_MSGLEN;', self.body)
        self.assertIn('MSG_Init( &msg, msgBuffer.data(), limit );', self.body)
        self.assertIn('return limit - len;', self.body)
        # Every configstring and baseline is measured with the selected wire,
        # from that wire's null entity.
        self.assertEqual(self.body.count('wire );'), 5)
        self.assertEqual(self.body.count('NETCHAN_WIRE_QL_RETAIL'), 1)

    def test_quake_profiles_keep_their_conservative_estimate(self):
        self.assertIn(': NETCHAN_WIRE_QL_RETAIL;', self.body)
        netchan = (ROOT / 'code/qcommon/netchan_profile.h').read_text(encoding='utf-8')
        self.assertIn('case NETCHAN_WIRE_RTCW_MP:\n\tcase NETCHAN_WIRE_WOLFET: return 32768;', netchan)

    def test_engine_storage_covers_every_registered_wire_profile(self):
        # Netchan_Process refuses a channel whose message limit does not fit
        # the assembly buffer, and it refuses it silently, by dropping the
        # packet. So MAX_GAME_MSGLEN is not a number of its own: it has to
        # track the largest limit any registered profile declares. Alice's
        # 48 KiB is the largest today; the rest are 16 or 32.
        netchan = (ROOT / 'code/qcommon/netchan_profile.h').read_text(encoding='utf-8')
        body = netchan[netchan.index('int Netchan_MessageLimit('):]
        body = body[:body.index('\n}')]
        limits = [int(value) for value in re.findall(r'return (\d+);', body)]
        self.assertTrue(limits, 'Netchan_MessageLimit declares no limits')
        qcommon = (ROOT / 'code/qcommon/qcommon.h').read_text(encoding='utf-8')
        storage = int(re.search(r'#define MAX_GAME_MSGLEN (\d+)', qcommon).group(1))
        self.assertGreaterEqual(storage, max(limits))


if __name__ == '__main__':
    unittest.main()
