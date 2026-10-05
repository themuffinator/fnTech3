"""Pin authored fixture identity and conservative runtime evidence decisions."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('wolfet_download_probe', ROOT / 'scripts/probe_wolfet_download.py')
PROBE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PROBE)


class WolfETDownloadProbeTests(unittest.TestCase):
    def test_transfer_logs_identify_the_fixture(self):
        name = PROBE.MOD + '/' + PROBE.PAK
        begin = f'clientDownload: 0 : beginning "{name}"\n'
        self.assertEqual(PROBE.transfer_log_status(begin), (True, False))
        for completed in (f'clientDownload: 0 : file "{name}" completed\n',
                          'clientDownload: 0 : download complete\n'):
            self.assertEqual(PROBE.transfer_log_status(begin + completed), (True, True))
        self.assertEqual(PROBE.transfer_log_status(
            'clientDownload: 0 : file "etmain/unrelated.pk3" completed\n'), (False, False))

    def test_fixture_is_reproducible_and_contains_only_authored_data(self):
        scratch = ROOT / '.tmp' / 'wolfet-download-probe-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            first, second = Path(folder) / 'one.pk3', Path(folder) / 'two.pk3'
            a, b = PROBE.write_fixture(first), PROBE.write_fixture(second)
            self.assertEqual(a, b)
            self.assertEqual(a['bytes'], 32916)
            with zipfile.ZipFile(first) as archive:
                self.assertEqual(archive.namelist(), ['fntech3_probe_payload.dat'])
                self.assertEqual(len(archive.read(archive.namelist()[0])), 32768)
                self.assertEqual(archive.infolist()[0].date_time, (2026, 1, 1, 0, 0, 0))

    def test_transfer_and_pure_admission_evidence_are_both_required(self):
        valid = dict(clientExit=0, serverExit=0, clientForced=False, serverForced=False,
                     installationUnchanged=True, fixtureInitiallyAbsent=True, fixtureMatches=True,
                     temporaryPakRemains=False, downloadBegan=True, downloadCompleted=True,
                     serverBegin=True, unpureDrop=False, clientErrors=[], serverErrors=[])
        self.assertTrue(PROBE.passed(valid))
        for field in ('installationUnchanged', 'fixtureInitiallyAbsent', 'fixtureMatches',
                      'downloadBegan', 'downloadCompleted', 'serverBegin'):
            self.assertFalse(PROBE.passed(dict(valid, **{field: False})), field)
        for field in ('clientForced', 'serverForced', 'temporaryPakRemains', 'unpureDrop'):
            self.assertFalse(PROBE.passed(dict(valid, **{field: True})), field)
        for field in ('clientExit', 'serverExit'):
            self.assertFalse(PROBE.passed(dict(valid, **{field: 1})), field)

    def test_web_redirect_modes_need_their_own_evidence(self):
        valid = dict(clientExit=0, serverExit=0, clientForced=False, serverForced=False,
                     installationUnchanged=True, fixtureInitiallyAbsent=True, fixtureMatches=True,
                     temporaryPakRemains=False, downloadBegan=True, downloadCompleted=True,
                     serverBegin=True, unpureDrop=False, clientErrors=[], serverErrors=[])
        path = '/' + PROBE.MOD + '/' + PROBE.PAK
        fetched = dict(valid, mode='www', downloadBegan=False, downloadCompleted=False,
                       httpRequests=[dict(path=path, status=200)])
        self.assertTrue(PROBE.passed(fetched))
        self.assertFalse(PROBE.passed(dict(fetched, httpRequests=[])))
        self.assertFalse(PROBE.passed(dict(fetched, httpRequests=[dict(path=path, status=404)])))
        self.assertFalse(PROBE.passed(dict(fetched, fixtureMatches=False)))
        declined = dict(valid, mode='www-missing', httpRequests=[dict(path=path, status=404)])
        self.assertTrue(PROBE.passed(declined))
        self.assertFalse(PROBE.passed(dict(declined, downloadCompleted=False)))
        self.assertFalse(PROBE.passed(dict(declined, httpRequests=[dict(path=path, status=200)])))
        for field in ('clientErrors', 'serverErrors'):
            self.assertFalse(PROBE.passed(dict(valid, **{field: ['decoder failed']})), field)

    def test_redirect_rejection_requires_live_recovery_and_no_file(self):
        valid = dict(redirectRefused=True, clientLiveAfterRefusal=True, serverExit=0,
                     serverForced=False, installationUnchanged=True, fixtureInitiallyAbsent=True,
                     downloadedBytes=0, temporaryPakRemains=False, downloadCompleted=False,
                     serverBegin=False, clientErrors=['ERROR: ' + PROBE.REDIRECT_REFUSAL],
                     serverErrors=[])
        self.assertTrue(PROBE.redirect_refused(valid))
        for field in ('redirectRefused', 'clientLiveAfterRefusal', 'installationUnchanged',
                      'fixtureInitiallyAbsent'):
            self.assertFalse(PROBE.redirect_refused(dict(valid, **{field: False})), field)
        for field in ('serverForced', 'temporaryPakRemains', 'downloadCompleted', 'serverBegin'):
            self.assertFalse(PROBE.redirect_refused(dict(valid, **{field: True})), field)
        self.assertFalse(PROBE.redirect_refused(dict(valid, downloadedBytes=22)))
        self.assertFalse(PROBE.redirect_refused(dict(valid, clientErrors=['ERROR: corrupted packet'])))


if __name__ == '__main__':
    unittest.main()
