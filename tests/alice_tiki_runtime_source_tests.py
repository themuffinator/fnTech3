"""Retail rules in the Alice model-definition runtime (code/qcommon/alice_tiki.cpp).

These pin three of retail alice.exe's answers that no shipped asset exercises,
so no runtime probe can catch a regression in them:

* a vertex-animated (TAN) tag adds the model's setup origin before it is
  scaled (TIKI_Tag_Orientation 0x0045fada..0x0045fb30);
* Anim_Random answers -1 for an empty name before it looks the model up
  (0x0045d191, 0x0045d1a2..0x0045d1a7);
* Frame_Commands is true only when it copied a command (0x0045d780,
  0x0045d8d0), while InitCommands is true with none (0x0045cf1a).
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


class AliceTikiRuntimeSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / 'code/qcommon/alice_tiki.cpp').read_text(encoding='utf-8')

    def test_vertex_tag_adds_the_setup_origin_before_scaling(self):
        body = function(self.source, 'void TanTag( const Model &model,')
        self.assertIn('const double factor = static_cast<double>( scale ) * model.scale;', body)
        self.assertIn('const float placed = model.origin[k] + stored[k];', body)
        self.assertIn('origin[k] = static_cast<float>( factor * placed );', body)
        self.assertNotIn('stored[k] * model.scale * scale', body)

    def test_random_animation_refuses_an_empty_name_first(self):
        body = function(self.source, 'int AliceTiki_AnimRandom( int handle, const char *name ) {')
        refusal = body.index('if ( !name || !name[0] ) return -1;')
        self.assertLess(refusal, body.index('CheckModel( handle, "TIKI_Anim_Random" )'))

    def test_frame_commands_answer_whether_a_command_was_copied(self):
        frame = function(self.source, 'qboolean AliceTiki_FrameCommands( int handle,')
        self.assertIn('return out->num_cmds > 0 ? qtrue : qfalse;', frame)
        init = function(self.source, 'qboolean AliceTiki_InitCommands( int handle,')
        self.assertIn('return AliceTiki_FillCommands( client ? m->clientInit : m->serverInit, 0, true, out );',
                      init)


if __name__ == '__main__':
    unittest.main()
