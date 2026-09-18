"""Hold ET's UI import coverage, its browser numbering and its refusals."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
DISPATCH = ROOT/'code/client/cl_ui.cpp'
RENDER = ROOT/'code/client/client_wolfet_render_compat.hpp'
AUDIO = ROOT/'code/client/client_wolfet_audio_compat.hpp'
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-ui-boundary.md'

# Six of these have no case in ET's own engine either, so refusing them is what
# retail ET does; the rest are services fnTech3 does not provide.
REFUSED_BY_ET_ITSELF = {
    'UI_FS_COPYFILE': 21, 'UI_CM_LOADMODEL': 38,
    'UI_LAN_GETLOCALSERVERCOUNT': 57, 'UI_LAN_GETLOCALSERVERADDRESSSTRING': 58,
    'UI_LAN_GETGLOBALSERVERCOUNT': 59, 'UI_LAN_GETGLOBALSERVERADDRESSSTRING': 60,
}
OPEN = dict(REFUSED_BY_ET_ITSELF, **{
    'UI_FS_DELETEFILE': 20,
    'UI_CL_TRANSLATE_STRING': 108, 'UI_OPENURL': 109,
})


def _imports():
    text = (ROOT/'code/compat/wolfet_module_calls.def').read_text(encoding='utf-8')
    return {name: int(number) for module, direction, name, number in re.findall(
        r'FNTECH3_WOLFET_MODULE_CALL\((\w+), (\w+), (\w+), (\d+)\)', text)
        if module == 'UI' and direction == 'IMPORT'}


def _function(path, signature):
    lines = path.read_text(encoding='utf-8').split('\n')
    first = next(i for i, line in enumerate(lines) if line.startswith(signature))
    last = next(i for i in range(first + 1, len(lines)) if lines[i] == '}')
    return '\n'.join(lines[first:last + 1])


def _regions():
    return '\n'.join((
        _function(DISPATCH, 'static intptr_t CL_WolfETUISharedCall'),
        _function(DISPATCH, 'static bool CL_WolfETUICall'),
        _function(RENDER, 'inline bool CL_WolfETRenderCall'),
        _function(AUDIO, 'inline bool CL_WolfETAudioCall'),
    ))


def _handled():
    text = _regions()
    return set(re.findall(r'case Call::(UI_\w+)', text)) | \
        set(re.findall(r'WOLFET_SHARED_CALL\((UI_\w+)\)', text))


class WolfETUIImportTests(unittest.TestCase):
    def test_every_import_is_handled_or_recorded_open(self):
        declared = _imports()
        self.assertEqual(len(declared), 121)
        handled = _handled() & set(declared)
        self.assertEqual(sorted(set(declared) - handled), sorted(OPEN))
        self.assertEqual(len(handled), 112)

    def test_open_ordinals_match_the_declared_ones(self):
        declared = _imports()
        for name, ordinal in OPEN.items():
            self.assertEqual(declared.get(name), ordinal, name)

    def test_an_unhandled_import_is_refused_by_name(self):
        region = _regions()
        self.assertIn('Unsupported %s UI import %ld (%s)', region)
        self.assertIn('Describe( call )', region)

    def test_browser_sources_are_translated(self):
        """ET numbers Global 1 and Favorites 2; this engine numbers them 2 and
        3 because it keeps a retired MPlayer slot at 1. Passing one through
        would show the module a different list than it asked for."""
        region = _function(DISPATCH, 'static intptr_t *CL_WolfETUIBrowserArguments')
        self.assertIn('{ AS_LOCAL, AS_GLOBAL, AS_FAVORITES }', region)
        self.assertIn('translated[1] = args[1] >= 0 && args[1] < 3 ? sources[args[1]] : -1;',
                      region)

    def test_et_keys_are_translated_rather_than_passed_through(self):
        region = _function(DISPATCH, 'static bool CL_WolfETUICall')
        for direct in ('Key_IsDown(', 'Key_GetBinding(', 'Key_SetBinding(',
                       'Key_KeynumToString(', 'Key_GetCatcher(', 'Key_SetCatcher('):
            self.assertNotIn(direct, region, direct)
        for translated in ('CL_WolfETKeyIsDown', 'CL_WolfETSetBinding',
                           'CL_WolfETKeyString', 'CL_WolfETGetKeyCatcher',
                           'CL_WolfETSetKeyCatcher', 'CL_WolfETKeysForBinding'):
            self.assertIn(translated, region, translated)

    def test_no_online_service_is_reported_as_available(self):
        """An absent update or anti-cheat service must not look present."""
        region = _function(DISPATCH, 'static bool CL_WolfETUICall')
        self.assertIn('unavailable in fnTech3', region)
        for invented in ('autoupdate', 'AUTOUPDATE_SUCCESS', 'pbEnabled'):
            self.assertNotIn(invented, region, invented)

    def test_the_record_names_what_et_itself_refuses(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('112 are handled', record)
        self.assertIn('The 9 that remain open', record)
        for ordinal in sorted(REFUSED_BY_ET_ITSELF.values()):
            self.assertRegex(record, r'\b%d\b' % ordinal,
                             'ordinal %d refused by ET itself is unrecorded' % ordinal)

    def test_the_et_branch_runs_before_the_shared_switch(self):
        text = DISPATCH.read_text(encoding='utf-8')
        dispatcher = text[text.index('static intptr_t CL_UISystemCalls('):]
        self.assertLess(dispatcher.index('CL_WolfETUICall( args, operation'),
                        dispatcher.index('switch ( operation ) {'))


if __name__ == '__main__':
    unittest.main()
