"""Exercise ET's movement prediction contract and its result marshalling."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def _source(name):
    return (ROOT/name).read_text(encoding='utf-8')


def _body(text, signature):
    begin = text.index(signature)
    return text[begin:text.index('\n}\n', begin) + 3]


class WolfETPredictionTests(unittest.TestCase):
    def setUp(self):
        self.prediction = _source('code/botlib/wolfet_prediction.hpp')
        self.body = _body(self.prediction, 'int AAS_WolfETPredictClientMovement(')
        self.server = _source('code/server/server_wolfet_botlib.hpp')

    def test_traces_the_world_not_a_client_box(self):
        """The result's trace is why ET needs its own loop at all."""
        self.assertIn('AAS_Trace(', self.body)
        self.assertNotIn('AAS_TraceClientBBox', self.body)
        self.assertIn('( CONTENTS_SOLID | CONTENTS_PLAYERCLIP ) & ~CONTENTS_BODY',
                      self.prediction)
        # Quake's prediction is untouched and still traces client boxes.
        quake = _source('code/botlib/be_aas_move.cpp')
        self.assertIn('AAS_TraceClientBBox(org, end, presencetype, entnum)', quake)

    def test_unsticks_the_start_or_refuses(self):
        self.assertIn('while ( trace.startsolid )', self.body)
        self.assertIn('org[2] += 8;', self.body)
        self.assertIn('MaxUnstick', self.body)
        self.assertIn('move->stopevent = SE_NONE;', self.body)
        self.assertIn('return qfalse;', self.body)

    def test_wolfet_only_stop_events(self):
        """ET's high stop bits mean different things from Quake's."""
        record = _source('code/botlib/wolfet_prediction_record.h')
        self.assertIn('#define SE_WOLFET_HITENT\t2048', record)
        self.assertIn('#define SE_WOLFET_STUCK\t\t4096', record)
        for event in ('SE_WOLFET_HITENT', 'SE_WOLFET_STUCK', 'SE_ENTERAREA',
                      'SE_HITGROUNDAREA', 'SE_HITGROUNDDAMAGE', 'SE_HITGROUND',
                      'SE_LEAVEGROUND', 'SE_GAP', 'SE_ENTERLAVA', 'SE_ENTERSLIME',
                      'SE_ENTERWATER'):
            self.assertIn(event, self.body, event)
        # ET has no jump pads, teleporters or cluster portals to stop on.
        for absent in ('SE_TOUCHJUMPPAD', 'SE_TOUCHTELEPORTER', 'SE_TOUCHCLUSTERPORTAL',
                       'SE_HITBOUNDINGBOX'):
            self.assertNotIn(absent, self.body, absent)

    def test_move_towards_a_point_mode(self):
        """A negative frame count treats the command vector as a destination."""
        self.assertIn('if ( cmdframes < 0 )', self.body)
        self.assertIn('VectorSubtract( cmdmove, org, wishdir );', self.body)
        self.assertIn('if ( !swimming ) frame_test_vel[2] = savevel[2];', self.body)

    def test_friction_pair_is_wolfet_order(self):
        """ET picks the opposite member of the pair from Quake."""
        self.assertIn('const float friction = swimming ? friction0 : waterfriction;', self.body)

    def test_slide_is_bounded(self):
        self.assertIn('MaxClips', self.body)
        self.assertIn('lplane', self.body)
        self.assertIn('DotProduct( lplane->normal, plane->normal ) > 0.99f', self.body)

    def test_result_marshalling_is_checked(self):
        body = _body(self.server, 'static bool SV_WolfETPredictionOutput(')
        self.assertIn('static_assert( sizeof( fntech3_wolfet_bsp_trace_t ) == 84', body)
        self.assertIn('static_assert( sizeof( fntech3_wolfet_aas_clientmove_t ) == 128', body)
        # Every scalar the module reads is checked before it is handed over.
        for field in ('source.endpos', 'source.velocity', 'source.trace.endpos',
                      'source.trace.plane.normal', 'source.trace.fraction',
                      'source.trace.plane.dist', 'source.trace.exp_dist',
                      'source.endcontents', 'source.time'):
            self.assertIn(field, body, field)
        # The surface name is copied with a terminator, never memcpy'd whole.
        self.assertIn("destination.trace.surface.name[length] = '\\0';", body)
        self.assertIn('return false;', body)

    def test_import_passes_every_argument(self):
        begin = self.server.index('case Call::BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT:')
        block = self.server[begin:self.server.index('\n\tcase Call::', begin + 10)]
        for index in range(1, 14):
            self.assertIn('args[%d]' % index, block, 'argument %d' % index)
        self.assertIn('SV_WolfETPredictionOutput( predicted, output )', block)
        self.assertIn('Com_Error( ERR_DROP, "Invalid WolfET movement prediction" )', block)


if __name__ == '__main__':
    unittest.main()
