"""Reject incomplete synchronized-view evidence without starting a game."""
import copy
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SPViewsProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location(
            'stvef_sp_views_probe', ROOT / 'scripts/probe_stvef_sp_views.py')
        cls.probe = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.probe)

    def scene(self):
        retail = dict(engine='retail', renderer=None, exit=0, forced=False,
                      images=1, image='retail.tga', viewpos=[1, 2, 3, 90])
        fn = dict(retail, engine='fntech3', renderer='glx', image='fn.png',
                  againstRetail=dict(meanAbsolute=[0.1, 0.2, 0.3], over16=0.01))
        return [dict(scene='voy2', runs=[retail, copy.deepcopy(retail), fn])]

    def test_completed_comparison_is_accepted(self):
        self.assertEqual(self.probe.result_problems(self.scene(), True), [])

    def test_incomplete_or_incomparable_runs_are_rejected(self):
        for changed in ({'exit': 1}, {'forced': True}, {'images': 0, 'image': None},
                        {'images': 2}, {'viewpos': None}, {'viewpos': [1, 2, 4, 90]},
                        {'againstRetail': {'sizes': [[640, 480], [800, 600]]}}):
            with self.subTest(changed=changed):
                scenes = self.scene()
                scenes[0]['runs'][2].update(changed)
                self.assertTrue(self.probe.result_problems(scenes, True))

    def test_missing_reference_and_changed_installation_are_rejected(self):
        scenes = self.scene()
        scenes[0]['runs'][0]['viewpos'] = None
        self.assertTrue(self.probe.result_problems(scenes, True))
        self.assertEqual(self.probe.result_problems(self.scene(), False), ['installation changed'])


if __name__ == '__main__':
    unittest.main()
