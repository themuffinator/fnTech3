"""ET's spline camera paths: its own slots, its own ordering, its own state."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
CAMERA = ROOT/'code/client/client_wolfet_camera.hpp'
CGAME = ROOT/'code/client/cl_cgame.cpp'
CLIENT = ROOT/'code/client/client.h'
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-camera.md'


def _function(path, signature):
    lines = path.read_text(encoding='utf-8').split('\n')
    first = next(i for i, line in enumerate(lines) if line.startswith(signature))
    last = next(i for i in range(first + 1, len(lines)) if lines[i] == '}')
    return '\n'.join(lines[first:last + 1])


class WolfETCameraTests(unittest.TestCase):
    def test_all_four_camera_imports_are_handled(self):
        region = _function(CAMERA, 'inline bool CL_WolfETCameraCall')
        for call in ('CG_LOADCAMERA', 'CG_STARTCAMERA', 'CG_STOPCAMERA', 'CG_GETCAMERAINFO'):
            self.assertIn('Call::%s' % call, region, call)

    def test_the_two_profiles_keep_separate_slots(self):
        """A path loaded for one profile must not answer for the other."""
        text = CAMERA.read_text(encoding='utf-8')
        self.assertIn('static fntech3::rtcw::camera::Manager manager;', text)
        self.assertIn('cl.wolfetCameraMode', text)
        self.assertNotIn('CL_RTCWCameras()', text)
        self.assertNotIn('cl.rtcwCameraMode', text)
        self.assertIn('qboolean\twolfetCameraMode;', CLIENT.read_text(encoding='utf-8'))

    def test_et_uses_the_ordering_its_own_source_shows(self):
        """ET starts the position before building the timeline and feathers
        both ends, which is the evaluator's SinglePlayer ordering."""
        text = CAMERA.read_text(encoding='utf-8')
        self.assertIn('Mode::SinglePlayer', text)
        self.assertNotIn('Mode::MultiPlayer', text)
        # The name is the evaluator's; the record must say so.
        self.assertIn("That name is the evaluator's", text)

    def test_an_invalid_slot_examines_nothing(self):
        region = _function(CAMERA, 'inline bool CL_WolfETCameraCall')
        self.assertIn('if ( !Manager::ValidSlot( args[1] ) ) return true;', region)
        # Slot validity is checked before any filename, time or output pointer.
        self.assertLess(region.index('ValidSlot'), region.index('CL_LoadWolfETCamera'))

    def test_only_the_primary_slot_drives_the_input_policy(self):
        region = _function(CAMERA, 'inline bool CL_WolfETCameraCall')
        self.assertIn('if ( index == 0 ) cl.wolfetCameraMode = qfalse;', region)
        self.assertIn('if ( index == 0 ) cl.wolfetCameraMode = qtrue;', region)

    def test_overlapping_outputs_are_refused(self):
        """Overlapping buffers would make the result depend on write order."""
        region = _function(CAMERA, 'inline bool CL_WolfETCameraCall')
        self.assertIn('Overlapping ET camera outputs', region)
        # Roll belongs to the module and is never written.
        self.assertIn('const float pitchYaw[2]', region)

    def test_a_path_name_cannot_escape_the_search_path(self):
        region = _function(CAMERA, 'inline bool CL_LoadWolfETCamera')
        for guard in ("name[0] == '/'", "name[0] == '\\\\'", "':'", '".."'):
            self.assertIn(guard, region, guard)
        # A failed load must not leave an older path in the slot.
        self.assertLess(region.index('Clear( index )'), region.index('FS_FOpenFileRead'))

    def test_cameras_are_cleared_with_the_cgame(self):
        self.assertIn('CL_ClearWolfETCameras();', CGAME.read_text(encoding='utf-8'))

    def test_the_record_states_what_retail_ships(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('Retail ET ships no camera paths', record)
        self.assertIn('140 of the 147', record)


if __name__ == '__main__':
    unittest.main()
