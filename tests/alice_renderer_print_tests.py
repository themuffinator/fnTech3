"""Alice's renderer messages reach the console as retail's CL_RefPrintf sends them."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AliceRendererPrintTests(unittest.TestCase):
    def setUp(self):
        source = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        start = source.index('QDECL CL_RefPrintf( printParm_t level, const char *fmt, ... ) {')
        self.body = source[start:source.index('\n}\n', start)]

    def test_alice_prints_warnings_only_at_developer_two(self):
        alice = self.body.index('if ( Com_GameIsRitual() ) {')
        switch = self.body.index('switch ( level ) {')
        self.assertLess(alice, switch)
        branch = self.body[alice:switch]
        self.assertIn('0x0042c2d0..0x0042c366', branch)
        self.assertIn('if ( level == PRINT_ALL ) {\n\t\t\tCom_Printf( "%s", msg.data() );', branch)
        self.assertIn('level != PRINT_WARNING || ( com_developer && com_developer->integer == 2 )',
                      branch)
        self.assertIn('Com_DPrintf( "%s", msg.data() );', branch)
        self.assertIn('return;', branch)

    def test_other_profiles_keep_their_levels(self):
        switch = self.body[self.body.index('switch ( level ) {'):]
        self.assertIn('default: Com_Printf( "%s", msg.data() ); break;', switch)
        self.assertIn('case PRINT_DEVELOPER: Com_DPrintf( "%s", msg.data() ); break;', switch)
        self.assertIn('case PRINT_WARNING: Com_Printf( S_COLOR_WARNING "%s", msg.data() ); break;', switch)
        self.assertIn('case PRINT_ERROR: Com_Printf( S_COLOR_ERROR "%s", msg.data() ); break;', switch)


if __name__ == '__main__':
    unittest.main()
