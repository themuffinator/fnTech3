"""Hold ET's cgame import coverage, and its refusal path, to what is recorded."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
DISPATCH = ROOT/'code/client/cl_cgame.cpp'
RENDER = ROOT/'code/client/client_wolfet_render_compat.hpp'
AUDIO = ROOT/'code/client/client_wolfet_audio_compat.hpp'
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-client-boundary.md'

# Every cgame import fnTech3 does not answer, with the ordinal ET gives it.
# Adding one here is a decision to leave an import open and must be argued in
# the record; removing one means the dispatcher now answers it.
OPEN = {
    'CG_FS_DELETEFILE': 16,
    'CG_R_PROJECTDECAL': 34, 'CG_R_CLEARDECALS': 35,
    'CG_R_ADDPOLYBUFFERTOSCENE': 64,
    'CG_R_SETGLOBALFOG': 68, 'CG_R_SAVEVIEWPARMS': 70, 'CG_R_RESTOREVIEWPARMS': 71,
    'CG_LOADCAMERA': 113, 'CG_STARTCAMERA': 114, 'CG_STOPCAMERA': 115,
    'CG_GETCAMERAINFO': 116,
    'CG_INGAME_POPUP': 162, 'CG_INGAME_CLOSEPOPUP': 163,
    'CG_R_DRAW2DPOLYS': 165, 'CG_TRANSLATE_STRING': 170,
    'CG_SENDMESSAGE': 174, 'CG_MESSAGESTATUS': 175,
    'CG_R_LOADDYNAMICSHADER': 176, 'CG_R_RENDERTOTEXTURE': 177,
    'CG_R_GETTEXTUREID': 178, 'CG_R_FINISH': 179,
}


def _imports():
    text = (ROOT/'code/compat/wolfet_module_calls.def').read_text(encoding='utf-8')
    return {name: int(number) for module, direction, name, number in re.findall(
        r'FNTECH3_WOLFET_MODULE_CALL\((\w+), (\w+), (\w+), (\d+)\)', text)
        if module == 'CGAME' and direction == 'IMPORT'}


def _function(path, signature):
    """The named function's text, ending at its own closing brace."""
    lines = path.read_text(encoding='utf-8').split('\n')
    first = next(i for i, line in enumerate(lines) if line.startswith(signature))
    last = next(i for i in range(first + 1, len(lines)) if lines[i] == '}')
    return '\n'.join(lines[first:last + 1])


def _wolfet_regions():
    """Only the ET dispatch paths; another profile's switch names the same calls."""
    return '\n'.join((
        _function(DISPATCH, 'static intptr_t CL_WolfETCgameSharedCall'),
        _function(DISPATCH, 'static intptr_t CL_WolfETKeyCall'),
        _function(DISPATCH, 'static bool CL_WolfETCgameCall'),
        _function(RENDER, 'inline bool CL_WolfETRenderCall'),
        _function(AUDIO, 'inline bool CL_WolfETAudioCall'),
    ))


def _handled():
    text = _wolfet_regions()
    return set(re.findall(r'case Call::(CG_\w+)', text)) | \
        set(re.findall(r'WOLFET_SHARED_CALL\((CG_\w+)\)', text))


class WolfETClientImportTests(unittest.TestCase):
    def test_every_import_is_handled_or_recorded_open(self):
        declared = _imports()
        self.assertEqual(len(declared), 147)
        handled = _handled() & set(declared)
        self.assertEqual(sorted(set(declared) - handled), sorted(OPEN))
        self.assertEqual(len(handled), 126)

    def test_open_ordinals_match_the_declared_ones(self):
        declared = _imports()
        for name, ordinal in OPEN.items():
            self.assertEqual(declared.get(name), ordinal, name)

    def test_nothing_is_both_handled_and_open(self):
        self.assertFalse(_handled() & set(OPEN))

    def test_an_unhandled_import_is_refused_by_name(self):
        """Silence would let a module believe an import succeeded."""
        region = _wolfet_regions()
        self.assertIn('Unsupported %s cgame import %ld (%s)', region)
        self.assertIn('Describe( call )', region)

    def test_the_record_lists_every_open_ordinal(self):
        """The recorded table must cover exactly the imports left open."""
        record = RECORD.read_text(encoding='utf-8')
        listed = set()
        for row in re.findall(r'^\| ([0-9–, ]+) \|', record, re.M):
            for part in row.split(','):
                part = part.strip()
                if '–' in part:
                    first, last = (int(value) for value in part.split('–'))
                    listed.update(range(first, last + 1))
                elif part:
                    listed.add(int(part))
        # A recorded range may span an ordinal that is in fact handled, so only
        # the declared import ordinals inside it are claimed as open.
        self.assertEqual(sorted(listed & set(_imports().values())),
                         sorted(set(OPEN.values())))
        self.assertIn('126 are handled', record)
        self.assertIn('The 21 that remain open', record)

    def test_et_keys_are_translated_rather_than_passed_through(self):
        """ET numbers the mouse wheel four below this engine; passing one
        through unchanged would bind a joystick button instead."""
        region = _function(DISPATCH, 'static intptr_t CL_WolfETKeyCall')
        for direct in ('Key_IsDown(', 'Key_GetBinding(', 'Key_SetBinding(',
                       'Key_KeynumToString(', 'Key_GetCatcher(', 'Key_SetCatcher('):
            self.assertNotIn(direct, region, direct)
        for translated in ('CL_WolfETKeyIsDown', 'CL_WolfETSetBinding',
                           'CL_WolfETKeyString', 'CL_WolfETGetKeyCatcher',
                           'CL_WolfETSetKeyCatcher', 'CL_WolfETKeysForBinding'):
            self.assertIn(translated, region, translated)

    def test_the_et_branch_runs_before_the_other_profile_branch(self):
        text = DISPATCH.read_text(encoding='utf-8')
        dispatcher = text[text.index('static intptr_t CL_CgameSystemCalls('):]
        # ET must answer before the shared trap switch, so no ET ordinal can
        # reach a handler meant for another profile's numbering.
        self.assertLess(dispatcher.index('CL_WolfETCgameCall( args, operation'),
                        dispatcher.index('switch( operation ) {'))


if __name__ == '__main__':
    unittest.main()
