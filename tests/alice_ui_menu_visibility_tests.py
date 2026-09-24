"""The client asks Alice's engine UI, not its key catcher, whether a menu is up.

A menu that takes no keys -- the credits after the ending -- still occupies the
engine UI's stack; answering from the catcher let CL_Frame's disconnected
main-menu restore run over it, stopping its music and dropping the console.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AliceUIMenuVisibilityTests(unittest.TestCase):
    def setUp(self):
        source = (ROOT / 'code/client/cl_ui.cpp').read_text(encoding='utf-8')
        start = source.index('qboolean CL_UIMenusAreVisible( void ) {')
        self.body = source[start:source.index('\n}\n', start)]

    def test_alice_answers_from_its_menu_stack_before_the_catcher(self):
        alice = self.body.index('if ( Com_GameIsAlice() ) {')
        self.assertIn('return CL_AliceUIMenuUp();', self.body[alice:])
        self.assertLess(self.body.index('if ( !uivm ) {'), alice)
        self.assertLess(alice, self.body.index('if ( !uivm->dllExports ) {'))

    def test_other_profiles_keep_their_answers(self):
        self.assertIn('return ( Key_GetCatcher() & KEYCATCH_UI ) ? qtrue : qfalse;', self.body)
        self.assertIn('return VM_Call( uivm, 0, UI_MENUS_ANY_VISIBLE ) ? qtrue : qfalse;', self.body)

    def test_the_stack_query_is_the_engine_uis(self):
        ui = (ROOT / 'code/client/cl_alice_ui.cpp').read_text(encoding='utf-8')
        self.assertIn('return uiInitialized && !menuStack.empty();', ui)
        self.assertIn('qboolean CL_AliceUIMenuUp( void ) {\n\treturn CL_AliceUIMenuIsUp() ? qtrue : qfalse;', ui)


if __name__ == '__main__':
    unittest.main()
