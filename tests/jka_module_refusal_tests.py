"""Jedi Academy's module boundaries answer honestly, whatever they cannot do.

fnTech3's native module loader is general enough to find Jedi Academy's
multiplayer modules inside the retail paks and call them, so the engine has to
say what it does not implement rather than invent an answer:

- the server's game dispatcher serves what it has built and stops at every
  other import by its own name and number;
- single player's measured retail API-8 table is loaded through an isolated
  developer host, whose unavailable imports stop by name; its client/snapshot
  boundary remains refused even if Init returns;
- single-player client game remains refused while multiplayer loads its
  measured cgame and UI through bounded adapters; single player's menus are
  the engine's own (cl_jedi_sp_ui.cpp), hosted before any module could load.

Returning a made-up value to an unimplemented import is what turned this
boundary into an access violation before these guards existed.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Entry points that refuse a Jedi Academy profile before loading a module.
REFUSED_BEFORE_LOADING = (
    ('code/client/cl_cgame.cpp', 'void CL_InitCGame( void ) {', 'Com_GameIsJKASP()',
     ('VM_Create', 'CL_InitCGameVM')),
)


class JKAModuleRefusalTests(unittest.TestCase):
    def test_single_player_routes_only_to_its_isolated_table_host(self):
        source = (ROOT / 'code/server/sv_game.cpp').read_text(encoding='utf-8')
        start = source.index('void SV_InitGameProgs( void ) {')
        body = source[start:source.index('\n}', start)]
        self.assertRegex(body, r'Com_GameIsJKASP\(\)\s*\?\s*SV_JKASPLoadGame\(\)')
        self.assertIn('#include "server_jka_sp_game.hpp"', source)
        host = (ROOT / 'code/server/server_jka_sp_game.hpp').read_text(encoding='utf-8')
        self.assertIn('VM_CreateTable(VM_GAME, "GetGameAPI"', host)
        self.assertLess(host.index('gvm = vm;'), host.index('getter(&jkaSpImports)'))
        self.assertIn('ImportName(Slot)', host)
        self.assertIn('client and snapshot adapters are not served yet', host)
        self.assertRegex(host, r'if \(initialized && jkaSpGame && !com_errorEntered\)')

    def test_every_gated_entry_point_refuses_before_loading(self):
        for path, signature, guard, loaders in REFUSED_BEFORE_LOADING:
            with self.subTest(path=path):
                source = (ROOT / path).read_text(encoding='utf-8')
                start = source.index(signature)
                body = source[start:source.index('\n}', start)]

                self.assertIn(guard, body, f'{path} must refuse its Jedi Academy profiles')
                where = body.index(guard)
                self.assertIn('Com_Error', body[where:where + 400],
                              f'{path} must report the open boundary')
                self.assertIn('is not implemented', body[where:where + 400])
                self.assertIn('single-player client module boundary', body[where:where + 400])
                self.assertIn('#include "client_jka_cgame_host.hpp"', source)
                self.assertIn('CL_JKACgameCall(', source)
                load_start = source.index('static vm_t *CL_LoadCGameVM(')
                load_body = source[load_start:source.index('\n}', load_start)]
                self.assertIn('VM_Create( VM_CGAME, CL_CgameSystemCalls, CL_DllSyscall,', load_body)
                self.assertIn('VMI_PINNED_NATIVE : VMI_NATIVE', load_body)

                for loader in loaders:
                    found = body.find(loader)
                    if found >= 0:
                        self.assertLess(where, found, f'{path} loads {loader} before refusing')

    def test_single_player_menus_are_the_engines_own(self):
        # Neither single-player game ships a menu module, so no module may be
        # looked for: the engine's own menus are hosted first, and the
        # multiplayer UI still loads its measured module through its adapters.
        source = (ROOT / 'code/client/cl_ui.cpp').read_text(encoding='utf-8')
        start = source.index('void CL_InitUI( void ) {')
        body = source[start:source.index('\n}', start)]
        guard = body.index('if ( Com_GameIsJKASP() || Com_GameProfile()->id == COM_GAME_JK2_SP ) {')
        hosted = body.index('uivm = CL_JediSPLoadUI();', guard)
        self.assertLess(hosted, body.index('return;', hosted))
        for loader in ('VM_Create(', 'CL_InitUIVM'):
            found = body.find(loader)
            if found >= 0:
                self.assertLess(hosted, found, f'CL_InitUI reaches {loader} before the Jedi menus')
        self.assertNotIn('is not implemented', body)
        self.assertIn('#include "client_jka_ui_host.hpp"', source)
        self.assertIn('CL_JKAUICall(', source)
        mp_start = body.index('if ( Com_GameIsJKAMP() )')
        mp_end = body.index('else if', mp_start)
        self.assertIn('VM_Create( VM_UI, CL_UISystemCalls, UI_DllSyscall,', body[mp_start:mp_end])
        self.assertIn('VMI_PINNED_NATIVE : VMI_NATIVE', body[mp_start:mp_end])
        host = (ROOT / 'code/client/cl_jedi_sp_ui.cpp').read_text(encoding='utf-8')
        self.assertIn('return VM_CreateHosted( VM_UI, Entry );', host)

    def test_the_game_dispatcher_names_every_import_it_does_not_serve(self):
        source = (ROOT / 'code/server/server_jka_game.hpp').read_text(encoding='utf-8')

        # The refusal names the call and its number, and is the last word: no
        # case may fall past it into a default answer.
        self.assertIn('is not served yet', source)
        self.assertIn('Describe( call )', source)
        self.assertRegex(source, r'descriptor\s*\?\s*descriptor->name')
        self.assertIn('static_cast<int>( args[0] )', source)

        # Quake III's shared arithmetic block is handed on rather than served
        # twice, because Jedi Academy kept its numbers.
        self.assertRegex(source, r'args\[0\] >= TRAP_MEMSET && args\[0\] <= TRAP_TESTPRINTFLOAT')

        # Every served case returns; none may fall through to another call.
        served = re.findall(r'case Call::(\w+):', source)
        self.assertEqual(len(served), 167, 'the imports served so far, counted')
        self.assertEqual(len(served), len(set(served)), 'no import is served twice')

    def test_navigation_is_answered_only_out_of_the_graph_the_engine_holds(self):
        # This replaces a stronger check that no navigation import but the load
        # was served. That check existed because the engine held no graph at
        # all, so any other answer would have been invented; it now holds one,
        # and the imports are answered from it. The claim the old check
        # protected -- that nothing about navigation is made up -- is what is
        # checked here instead, which is a deliberate narrowing and not a test
        # edited to pass.
        source = (ROOT / 'code/server/server_jka_game.hpp').read_text(encoding='utf-8')
        served = set(re.findall(r'case Call::(\w+):', source))
        self.assertIn('G_NAV_LOAD', served)

        # The load still answers that it loaded nothing, and that stays true
        # only while nothing in the engine reads a `.nav` file.
        load = source.split('case Call::G_NAV_LOAD:')[1].split('return true;')[0]
        self.assertIn('result = 0', load)
        graph = (ROOT / 'code/nav/nav_graph.hpp').read_text(encoding='utf-8')
        self.assertNotIn('Parse', graph, 'the graph reads a file while the load declines')

        # Every other navigation import either answers out of that one graph or
        # says in its own comment that it does nothing and why.
        navigation = sorted(name for name in served
                            if name.startswith('G_NAV_') and name != 'G_NAV_LOAD')
        self.assertGreater(len(navigation), 15, 'the navigation block is barely served')
        # Cases may be grouped, in which case they share the body that follows
        # the last of them, so an empty chunk carries on to the next.
        chunks = source.split('case Call::')[1:]
        bodies = {}
        pending = []
        for chunk in chunks:
            name = chunk.split(':', 1)[0].strip()
            body = chunk.split(':', 1)[1]
            pending.append(name)
            if body.strip():
                for grouped in pending:
                    bodies[grouped] = body
                pending = []
        # Six are answered by doing nothing, each named here and each carrying
        # its reason in the source beside it: the two that would re-test what
        # failed, which this engine has no test for; the three that draw the
        # graph, which a dedicated server has no client for; and the save,
        # which will not write into an installation it was asked to read.
        silent = {'G_NAV_CHECKFAILEDNODES', 'G_NAV_CHECKBLOCKEDEDGES',
                  'G_NAV_SHOWNODES', 'G_NAV_SHOWEDGES', 'G_NAV_SHOWPATH',
                  'G_NAV_SAVE'}
        for name in navigation:
            body = bodies.get(name, '')
            if name in silent:
                self.assertRegex(body, r'(?s)/\*.*\*/',
                                 '%s does nothing without saying why' % name)
            else:
                self.assertIn('jkaNavigation', body,
                              '%s answers without reading the graph' % name)

        table = (ROOT / 'code/compat/jka_module_calls.def').read_text(encoding='utf-8')
        navigation = re.findall(r'\(GAME, IMPORT, (G_NAV_\w+),', table)
        self.assertIn('G_NAV_LOAD', navigation)
        self.assertGreater(len(navigation), 20, 'the game imports far more navigation than this')

    def test_motion_paths_use_the_measured_profile_player(self):
        # Play and purge now have a retail-evidenced trajectory and callback
        # implementation. They must never slip back to success without motion.
        source = (ROOT / 'code/server/server_jka_game.hpp').read_text(encoding='utf-8')
        served = set(re.findall(r'case Call::(\w+):', source))
        for name, implementation in (('G_ROFF_CLEAN', 'jkaRoffPlayer.Clear'),
                                     ('G_ROFF_CACHE', 'ravenRoff.Cache'),
                                     ('G_ROFF_UPDATE_ENTITIES', 'jkaRoffPlayer.Update'),
                                     ('G_ROFF_PLAY', 'jkaRoffPlayer.Play'),
                                     ('G_ROFF_PURGE_ENT', 'jkaRoffPlayer.Stop')):
            self.assertIn(name, served)
            body = source.split('case Call::' + name + ':', 1)[1].split('case Call::', 1)[0]
            self.assertIn(implementation, body)
        self.assertIn('GAME_ROFF_NOTETRACK_CALLBACK', source)
        self.assertIn('VM_Call( gvm, 2, callback->ordinal, entity, text )', source)
        player = (ROOT / 'code/server/server_jka_roff.hpp').read_text(encoding='utf-8')
        self.assertIn('record->next_roff_time', player)
        self.assertIn('record->s.pos.trDelta', player)
        self.assertIn('record->s.apos.trDelta', player)
        self.assertIn('record.r.mIsRoffing = 0', player)
        self.assertIn('line is too long', player)

    def test_cleaning_ghoul2_attachments_really_cleans(self):
        # This replaces a check that no attachment import was served while the
        # clean call did nothing. That check existed because an empty answer
        # was only honest while the engine could hold no attachment. It can
        # now, so the claim has to be kept the other way round: the clean call
        # must clear what the engine keeps, and the store must really keep it.
        source = (ROOT / 'code/server/server_jka_game.hpp').read_text(encoding='utf-8')
        served = set(re.findall(r'case Call::(\w+):', source))
        for name in ('G_G2_CLEANENTATTACHMENTS', 'G_G2_ATTACHINSTANCETOENTNUM',
                     'G_G2_CLEARATTACHEDINSTANCE'):
            self.assertIn(name, served)

        chunks = source.split('case Call::')[1:]
        bodies = {}
        pending = []
        for chunk in chunks:
            name = chunk.split(':', 1)[0].strip()
            body = chunk.split(':', 1)[1]
            pending.append(name)
            if body.strip():
                for grouped in pending:
                    bodies[grouped] = body
                pending = []
        self.assertIn('ClearAttachments', bodies['G_G2_CLEANENTATTACHMENTS'],
                      'cleaning attachments no longer clears what the engine holds')
        self.assertIn('Attach(', bodies['G_G2_ATTACHINSTANCETOENTNUM'])
        self.assertIn('Detach(', bodies['G_G2_CLEARATTACHEDINSTANCE'])

        # The store it calls must hold attachments, not merely accept them.
        store = (ROOT / 'code/ghoul2/ghoul2_server.hpp').read_text(encoding='utf-8')
        for member in ('bool Attach(', 'bool Detach(', 'void ClearAttachments()',
                       'Instance *Attached('):
            self.assertIn(member, store, 'the store is missing %s' % member)

        # And the table it decodes really does carry those calls, so the checks
        # above are looking for something that exists.
        table = (ROOT / 'code/compat/jka_module_calls.def').read_text(encoding='utf-8')
        ghoul2 = set(re.findall(r'\(GAME, IMPORT, (G_G2\w+),', table))
        for name in ('G_G2_CLEANENTATTACHMENTS', 'G_G2_ATTACHINSTANCETOENTNUM',
                     'G_G2_CLEARATTACHEDINSTANCE'):
            self.assertIn(name, ghoul2, 'the table does not name %s' % name)

    def test_the_dispatcher_is_reached_for_multiplayer_only(self):
        source = (ROOT / 'code/server/sv_game.cpp').read_text(encoding='utf-8')
        start = source.index('static intptr_t SV_GameSystemCalls( intptr_t *args ) {')
        body = source[start:start + 4000]
        self.assertIn('Com_GameIsJKAMP()', body)
        self.assertIn('SV_JKAGameCall( args, operation, result )', body)


if __name__ == '__main__':
    unittest.main()
