"""Exercise ET's redirected download: message shape, state and profile scope."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def _source(name):
    return (ROOT/name).read_text(encoding='utf-8')


def _body(text, signature):
    begin = text.index(signature)
    return text[begin:text.index('\n}\n', begin)]


class WolfETWebDownloadTests(unittest.TestCase):
    def setUp(self):
        self.client = _source('code/server/sv_client.cpp')
        self.state = _source('code/server/server_wolfet_client.hpp')

    def test_redirect_message_shape(self):
        """A redirect is svc_download with a -1 block, URL, size and flags."""
        body = _body(self.client, 'static void SV_WolfETSendRedirect(')
        order = [body.index(token) for token in (
            'MSG_WriteByte( &msg, svc_download )',
            'MSG_WriteShort( &msg, -1 )',
            'MSG_WriteString( &msg, url )',
            'MSG_WriteLong( &msg, size )',
            'MSG_WriteLong( &msg, flags )')]
        self.assertEqual(order, sorted(order))
        # The reliable-command acknowledgement precedes the payload, as for
        # every other message this server sends a connecting client.
        self.assertLess(body.index('MSG_WriteLong( &msg, cl->lastClientCommand )'), order[0])
        self.assertIn('SV_UpdateServerCommandsToClient', body)

    def test_redirect_preconditions(self):
        body = _body(self.client, 'static bool SV_WolfETWriteDownloadRedirect(')
        self.assertIn('!Com_GameIsWolfET() || !sv_wwwDownload->integer', body)
        # A client that cannot fetch files, or whose last fetch failed, is only
        # offered the fallback URL; otherwise it takes the block transfer.
        self.assertIn('if ( !web->capable )', body)
        self.assertIn('if ( web->fallback )', body)
        self.assertIn('web->fallback = false;', body)
        # An empty base URL is an operator error, not a malformed redirect.
        self.assertIn('sv_wwwBaseURL', body)
        self.assertIn('sv_wwwDlDisconnected', body)
        # The fallback form carries no size and its own flag.
        self.assertIn('SV_WolfETSendRedirect( cl, buffer, sv_wwwFallbackURL->string, 0, 2 )', body)

    def test_block_transfer_is_tried_after_a_refused_redirect(self):
        """Returning false must fall through to the ordinary download."""
        body = _body(self.client, 'static int SV_WriteDownloadToClient( client_t *cl )')
        redirect = body.index('SV_WolfETWriteDownloadRedirect( cl, msgBuffer )')
        self.assertIn('return 0;', body[redirect:redirect + 120])
        self.assertLess(redirect, body.index('cl->downloadSize = FS_SV_FOpenFileRead'))
        # Official and unreferenced paks are still refused before any redirect.
        self.assertIn('!idPack && !unreferenced && SV_WolfETWriteDownloadRedirect', body)
        # A client already fetching the file itself receives no blocks.
        self.assertIn('web->transferring ) return 0;', body)

    def test_progress_subcommands(self):
        body = _body(self.client, 'static void SV_WolfETWebDownload_f( client_t *cl )')
        for subcommand in ('ack', 'bbl8r', 'done', 'fail', 'chkfail'):
            self.assertIn('"%s"' % subcommand, body)
        # Anything before the redirect, or after it that is not understood,
        # drops the client rather than being ignored.
        self.assertEqual(body.count('SV_DropClient'), 4)
        self.assertIn('!web || !web->redirected', body)
        self.assertIn('acking disconnected download mode', body)
        # A failed fetch retries the file the ordinary way.
        failure = body.index('"fail"')
        self.assertIn('web->fallback = true;', body[failure:])
        self.assertIn('SV_SendClientGameState( cl )', body[failure:])

    def test_command_is_unknown_to_other_profiles(self):
        """wwwdl must still reach the game module under Q3, QL and RTCW."""
        body = _body(self.client, 'static const ucmd_t *SV_FindClientCommand(')
        self.assertIn('!strcmp( ucmd.name, "wwwdl" ) && !Com_GameIsWolfET()', body)
        self.assertIn('return nullptr;', body)
        # The shared accessor is ET-only too, so no other profile keeps state.
        self.assertIn('!Com_GameIsWolfET() || !svs.clients',
                      _body(self.state, 'inline WebDownload *Download( int client )'))

    def test_state_is_per_file_and_engine_owned(self):
        self.assertIn('std::array<WebDownload, FNTECH3_WOLFET_MAX_CLIENTS> downloads{};', self.state)
        self.assertIn('state.downloads.fill( WebDownload{} );', self.state)
        begin = self.client.index('static void SV_BeginDownload_f')
        block = self.client[begin:self.client.index('\n}\n', begin)]
        for field in ('web->redirected = false;', 'web->transferring = false;',
                      'web->notify = fntech3::wolfet::server::NotifyAll;'):
            self.assertIn(field, block)
        # client_t gains nothing: the other four profiles are untouched.
        self.assertNotIn('bWWWDl', _source('code/server/server.h'))
        self.assertNotIn('downloadURL', _source('code/server/server.h'))

    def test_cvars_match_the_retail_names_and_defaults(self):
        init = _source('code/server/sv_init.cpp')
        for name, default in (('sv_wwwDownload', '"0"'), ('sv_wwwBaseURL', '""'),
                              ('sv_wwwDlDisconnected', '"0"'), ('sv_wwwFallbackURL', '""')):
            self.assertRegex(init, r'%s = Cvar_Get\( "%s", %s, CVAR_ARCHIVE \);'
                             % (name, name, re.escape(default)))

    def test_client_capability_comes_from_userinfo(self):
        begin = self.client.index('void SV_UserinfoChanged(')
        block = self.client[begin:begin + 2000]
        self.assertIn('"cl_wwwDownload"', block)
        self.assertIn('web->capable', block)


if __name__ == '__main__':
    unittest.main()
