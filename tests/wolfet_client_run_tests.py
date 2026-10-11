"""What running a real ET client first required of the engine."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-client-run.md'


def source(name):
    return (ROOT/name).read_text(encoding='utf-8')


def menus():
    return {name: int(value) for name, value in re.findall(
        r'FNTECH3_WOLFET_MENU\((\w+), (-?\d+)\)', source('code/compat/wolfet_menus.def'))}


class WolfETClientRunTests(unittest.TestCase):
    def test_every_ui_and_cgame_export_et_declares_is_translated(self):
        calls = source('code/compat/wolfet_module_calls.def')
        vm = source('code/qcommon/vm.c')
        body = vm[vm.index('static qboolean VM_PrepareWolfETExport('):]
        body = body[:body.index('\n}\n')]
        for module in ('UI', 'CGAME'):
            for name in re.findall(r'MODULE_CALL\(%s, EXPORT, (\w+), \d+\)' % module, calls):
                self.assertIn('ET_EXPORT(%s,' % name, body, name)
        # The export path is ET's own; the RTCW one is never asked for ET.
        self.assertIn('Com_GameIsWolfET() ? VM_PrepareWolfETExport(', vm)

    def test_et_menus_are_its_own_numbering(self):
        table = menus()
        # ET's ordinals, from its bg_public.h: contiguous from zero, sixteen menus.
        self.assertEqual(sorted(table.values()), list(range(16)))
        self.assertEqual(table['HELP'], 7)
        self.assertEqual(table['WM_QUICKMESSAGE'], 8)
        self.assertEqual(table['INGAME_MESSAGEMODE'], 15)
        public = source('code/ui/ui_public.h')
        enum = public[public.index('UIMENU_NONE'):public.index('UIMENU_COUNT')]
        for name in table:
            self.assertIn('UIMENU_' + name + ',', enum, name)
        # A cgame popup names ET's number, which is turned back into the
        # engine's before the export translates it once more.
        cgame = source('code/client/cl_cgame.cpp')
        popup = cgame[cgame.index('case Call::CG_INGAME_POPUP:'):]
        popup = popup[:popup.index('case Call::CG_TRANSLATE_STRING:')]
        self.assertIn('FnTech3_WolfETMenuToInternal( args[1] )', popup)

    def test_et_cgame_learns_whether_it_is_replaying_a_demo(self):
        cgame = source('code/client/cl_cgame.cpp')
        self.assertIn('VM_Call( cgvm, 4, CG_INIT', cgame)
        self.assertIn('clc.demoplaying ? 1 : 0', cgame)

    def test_a_trace_reads_its_end_before_its_box(self):
        cgame = source('code/client/cl_cgame.cpp')
        signature = cgame[cgame.index('static void CL_WolfETCMTrace('):]
        signature = signature[:signature.index('{')]
        self.assertLess(signature.index('end'), signature.index('mins'))
        self.assertLess(signature.index('mins'), signature.index('maxs'))

    def test_et_fonts_are_found_by_et_names(self):
        font = source('code/renderercommon/tr_font.c')
        self.assertIn('"fonts/%s_%i.dat"', font)
        self.assertIn('ri.gameId == COM_GAME_WOLFET', font)

    def test_et_fog_is_the_multiplayer_fog_scaled_by_identity_light(self):
        scene = source('code/renderercommon/tr_rtcw_fog_scene.h')
        self.assertIn('red *= tr.identityLight;', scene)
        self.assertIn('ri.gameId != COM_GAME_RTCW_SP ? qtrue : qfalse', scene)
        self.assertIn('translated.rtcw.skyboxPortal = view->wolfet.skyboxPortal;', scene)

    def test_a_noise_alpha_wave_is_evaluated_in_every_renderer(self):
        for name in RENDERERS:
            calc = source('code/%s/tr_shade_calc.c' % name)
            alpha = calc[calc.index('void RB_CalcWaveAlpha('):]
            alpha = alpha[:alpha.index('\n}\n')]
            self.assertIn('GF_NOISE', alpha, name)
            self.assertIn('R_NoiseGet4f', alpha, name)

    def test_every_profile_has_an_icon_row(self):
        icon = source('code/platform/game_icon.hpp')
        self.assertEqual(icon.count('== COM_GAME_COUNT'), 3)
        self.assertIn('"et.exe"', icon)

    def test_an_initialisation_failure_keeps_its_message(self):
        common = source('code/qcommon/common.c')
        self.assertIn('"Error during initialization%s%s"', common)

    def test_profile_availability_is_independent_of_development_flags(self):
        profile = source('code/qcommon/game_profile.cpp')
        self.assertNotIn('RuntimeBlocked', profile)
        self.assertNotIn('FNTECH3_WOLFET_DEVELOPMENT', profile)

    def test_the_probe_keeps_its_discipline(self):
        probe = source('scripts/probe_wolfet_client.py')
        for claim in ("r_fullscreen='0'", 'screenshotPNG', 'WindowsPrivateProcess',
                      'installationUnchanged', 'remove_home'):
            self.assertIn(claim, probe)

    def test_the_probe_sees_the_client_become_active(self):
        # ET's log never names CA_ACTIVE. The client prints this line as it
        # goes active, and only once it has.
        cgame = source('code/client/cl_cgame.cpp')
        first = cgame[cgame.index('static void CL_FirstSnapshot('):]
        first = first[:first.index('\n}\n')]
        self.assertLess(first.index('cls.state = CA_ACTIVE;'), first.index('"CL_FirstSnapshot: '))
        probe = source('scripts/probe_wolfet_client.py')
        self.assertIn("report['activeObserved'] = 'CL_FirstSnapshot: ' in text", probe)

    def test_the_record_states_what_is_and_is_not_claimed(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('The ET client has now run', record)
        self.assertIn('No gameplay is claimed', record)


if __name__ == '__main__':
    unittest.main()
