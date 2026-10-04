"""Explicit map probes must not silently omit a requested case."""
from pathlib import Path
import importlib.util
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    'probe_retail_map_loads', ROOT / 'scripts' / 'probe_retail_map_loads.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class RetailMapSelectionTests(unittest.TestCase):
    def setUp(self):
        self.maps = [dict(profile='alice', label='alice:garden1'),
                     dict(profile='mohaa', label='mohaa:m1l1'),
                     dict(profile='mohaa', label='mohaa:dm/mohdm1'),
                     dict(profile='rtcw-sp', label='rtcw-sp:escape1'),
                     dict(profile='rtcw-mp', label='rtcw-mp:mp_beach')]

    def test_case_and_space_variants_resolve_to_canonical_labels(self):
        rows = probe.select_cases(self.maps, ' ALICE,MOHAA ',
                                  'alice:garden1, mohaa:DM/mohdm1 ')
        self.assertEqual([row['label'] for row in rows],
                         ['alice:garden1', 'mohaa:dm/mohdm1'])

    def test_partial_missing_selection_fails_instead_of_passing_subset(self):
        with self.assertRaisesRegex(ValueError, 'mohaa:missing'):
            probe.select_cases(self.maps, 'mohaa', 'mohaa:m1l1,mohaa:missing')
        with self.assertRaisesRegex(ValueError, 'alice:garden1'):
            probe.select_cases(self.maps, 'mohaa', 'alice:garden1')

    def test_unknown_profile_and_empty_inventory_fail(self):
        with self.assertRaisesRegex(ValueError, 'profiles have no installed maps: fakk2'):
            probe.select_cases(self.maps, 'alice,fakk2')
        with self.assertRaisesRegex(ValueError, 'no installed maps selected'):
            probe.select_cases([])

    def test_profile_selection_preserves_mode_and_inventory_order(self):
        self.assertEqual(probe.select_cases(self.maps, 'rtcw-sp'), [self.maps[3]])
        self.assertEqual(probe.select_cases(self.maps), self.maps)


if __name__ == '__main__':
    unittest.main()
