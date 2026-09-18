"""ET's binary message channel, screen-space polygons and saved view."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
CGAME = ROOT/'code/client/cl_cgame.cpp'
NETCHAN = ROOT/'code/client/cl_net_chan.cpp'
PARSE = ROOT/'code/client/cl_parse.cpp'
SERVER = ROOT/'code/server/sv_client.cpp'
STATE = ROOT/'code/client/client_wolfet_state.hpp'
RENDER = ROOT/'code/client/client_wolfet_render_compat.hpp'
POLYCMD = ROOT/'code/renderercommon/tr_wolfet_2d_poly_cmd.h'
POLYBACK = ROOT/'code/renderercommon/tr_wolfet_2d_poly_backend.h'
VIEWPARMS = ROOT/'code/renderercommon/tr_wolfet_view_parms.h'
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-client-services.md'

RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def _function(path, signature):
    lines = path.read_text(encoding='utf-8').split('\n')
    first = next(i for i, line in enumerate(lines) if line.startswith(signature))
    last = next(i for i in range(first + 1, len(lines)) if lines[i] == '}')
    return '\n'.join(lines[first:last + 1])


class WolfETClientServiceTests(unittest.TestCase):
    def test_binary_message_travels_in_both_directions(self):
        """A channel wired in only one direction would look like it worked."""
        self.assertIn('CL_WolfETWriteBinaryMessage( msg );',
                      _function(NETCHAN, 'extern "C" void CL_Netchan_Transmit'))
        self.assertIn('SV_WolfETParseBinaryMessage( cl, msg );',
                      SERVER.read_text(encoding='utf-8'))
        self.assertIn('CL_WolfETParseBinaryMessage( msg );',
                      _function(PARSE, 'void CL_ParseServerMessage'))

    def test_an_opaque_payload_never_reaches_a_bytecode_module(self):
        """The payload is a pointer into the packet, which only a native module
        may be handed; a bytecode module would need its own copy."""
        for path, signature in ((PARSE, 'static void CL_WolfETParseBinaryMessage'),
                                (SERVER, 'static void SV_WolfETParseBinaryMessage')):
            region = _function(path, signature)
            self.assertIn('dllExports', region, signature)
            self.assertIn('MSG_Uncompressed', region, signature)
            self.assertIn('MaxBinaryMessage', region, signature)

    def test_an_over_long_payload_is_refused_not_truncated(self):
        region = _function(CGAME, 'static bool CL_WolfETCgameCall')
        self.assertIn('Invalid ET binary message length %ld', region)
        self.assertIn('args[2] > MaxBinaryMessage', region)

    def test_message_status_reports_the_three_states(self):
        region = _function(CGAME, 'static bool CL_WolfETCgameCall')
        self.assertIn('MessageEmpty', region)
        self.assertIn('MessageWaitingOverflow', region)
        self.assertIn('MessageWaiting', region)
        text = STATE.read_text(encoding='utf-8')
        # ET's own cg_public.h numbering; a different order would mislead the module.
        self.assertIn('MessageEmpty = 0, MessageWaiting = 1, MessageWaitingOverflow = 2', text)
        self.assertIn('MaxBinaryMessage = 32768', text)

    def test_an_overflowed_payload_is_kept_rather_than_dropped(self):
        region = _function(NETCHAN, 'static void CL_WolfETWriteBinaryMessage')
        self.assertIn('message.overflowed = true;', region)
        # The length survives an overflow so the module can retry.
        overflow = region[region.index('message.overflowed = true;'):]
        self.assertNotIn('message.length = 0;', overflow.split('return;')[0])

    def test_popups_name_a_menu_by_ordinal(self):
        """ET names a menu by number where the other Wolfenstein profile names
        it by string; its own client also refuses while replaying a demo."""
        region = _function(CGAME, 'static bool CL_WolfETCgameCall')
        self.assertIn('FnTech3_WolfETMenuToInternal( args[1] )', region)
        self.assertIn('VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, menu );', region)
        self.assertIn('cls.state == CA_ACTIVE && !clc.demoplaying && uivm', region)

    def test_two_dimensional_polygons_are_a_fan_with_their_own_colours(self):
        backend = POLYBACK.read_text(encoding='utf-8')
        # Every triangle shares the first point.
        self.assertIn('tess.indexes[tess.numIndexes + 0] = first;', backend)
        self.assertIn('tess.indexes[tess.numIndexes + 1] = first + i + 1;', backend)
        self.assertIn('tess.indexes[tess.numIndexes + 2] = first + i + 2;', backend)
        # Screen depth is zero and each point keeps its submitted colour.
        self.assertIn('tess.xyz[first + i][2] = 0;', backend)
        self.assertIn('tess.vertexColors[first + i] = verts[i].modulate;', backend)

    def test_a_polygon_batch_is_bounded_and_finite(self):
        command = POLYCMD.read_text(encoding='utf-8')
        self.assertIn('numVerts < 3 || numVerts > FNTECH3_WOLFET_MAX_2D_POLY_VERTS', command)
        self.assertIn('non-finite vertex', command)
        self.assertIn('ri.gameId != COM_GAME_WOLFET', command)
        # The points travel inside the command, so nothing outlives the frame.
        self.assertIn('stored = (polyVert_t *)( cmd + 1 );', command)
        self.assertIn('cmd->commandBytes = bytes;', command)

    def test_restoring_an_unsaved_view_is_refused(self):
        region = VIEWPARMS.read_text(encoding='utf-8')
        self.assertIn('if ( !wolfetViewParmsSaved ) return;', region)
        self.assertIn('ri.gameId != COM_GAME_WOLFET', region)

    def test_every_renderer_registers_the_new_services(self):
        for renderer in RENDERERS:
            init = (ROOT/('code/%s/tr_init.c' % renderer)).read_text(encoding='utf-8')
            for entry in ('AddWolfET2DPolys', 'SaveWolfETViewParms', 'RestoreWolfETViewParms'):
                self.assertIn('re.%s = RE_%s;' % (entry, entry), init, renderer)
            backend = (ROOT/('code/%s/tr_backend.c' % renderer)).read_text(encoding='utf-8')
            self.assertIn('RB_DrawWolfET2DPolys( data )', backend, renderer)

    def test_the_record_states_the_coverage(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('134 of the 147', record)
        self.assertIn('112 of the 121', record)


if __name__ == '__main__':
    unittest.main()
