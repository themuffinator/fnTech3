"""The renderer's own byte reader for ET's skeletal models."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-renderer-models.md'

PROBE = r'''
#include "code/renderercommon/tr_wolfet_model_format.h"
#include <cassert>
#include <cstring>
#include <vector>

static std::vector<unsigned char> buffer;
static void put(size_t at, int32_t value) {
 if (at + 4 > buffer.size()) buffer.resize(at + 4, 0);
 std::memcpy(buffer.data() + at, &value, 4);
}
static int mesh() { fnWolfETMeshView_t v; return FnWolfET_Mesh(buffer.data(), buffer.size(), &v); }
static int skeleton() { fnWolfETSkeletonView_t v; return FnWolfET_Skeleton(buffer.data(), buffer.size(), &v); }

// The smallest well formed file of each kind: a header and nothing else.
static void emptyMesh() {
 buffer.assign(FN_WOLFET_MDM_HEADER_BYTES, 0);
 put(0, (int32_t)FN_WOLFET_MDM_IDENT);
 put(4, FN_WOLFET_MDM_VERSION);
 put(84, FN_WOLFET_MDM_HEADER_BYTES);
 put(92, FN_WOLFET_MDM_HEADER_BYTES);
 put(96, FN_WOLFET_MDM_HEADER_BYTES);
}
static void oneBoneSkeleton() {
 buffer.assign(FN_WOLFET_MDX_HEADER_BYTES + FN_WOLFET_MDX_BONE_BYTES, 0);
 put(0, (int32_t)FN_WOLFET_MDX_IDENT);
 put(4, FN_WOLFET_MDX_VERSION);
 put(72, 0);                                  // no frames
 put(76, 1);                                  // one bone
 put(80, FN_WOLFET_MDX_HEADER_BYTES);
 put(84, FN_WOLFET_MDX_HEADER_BYTES);
 put(88, -1);                                 // no torso parent
 put(92, (int32_t)buffer.size());
 put(FN_WOLFET_MDX_HEADER_BYTES + 64, -1);    // that bone has no parent
}

int main() {
 // The identifiers are the bytes the retail files actually begin with.
 assert(FN_WOLFET_MDM_IDENT == 0x574d444du && FN_WOLFET_MDX_IDENT == 0x5758444du);

 emptyMesh(); assert(mesh());
 oneBoneSkeleton(); assert(skeleton());

 // Null and every truncation short of a full header are refused.
 { fnWolfETMeshView_t v; assert(!FnWolfET_Mesh(nullptr, 64, &v)); }
 { fnWolfETSkeletonView_t v; assert(!FnWolfET_Skeleton(nullptr, 64, &v)); }
 for (size_t bytes = 0; bytes < FN_WOLFET_MDM_HEADER_BYTES; ++bytes) {
  emptyMesh(); buffer.resize(bytes); assert(!mesh());
 }
 for (size_t bytes = 0; bytes < FN_WOLFET_MDX_HEADER_BYTES; ++bytes) {
  oneBoneSkeleton(); buffer.resize(bytes); assert(!skeleton());
 }

 // Each kind refuses the other's identifier and every other version.
 emptyMesh(); put(0, (int32_t)FN_WOLFET_MDX_IDENT); assert(!mesh());
 oneBoneSkeleton(); put(0, (int32_t)FN_WOLFET_MDM_IDENT); assert(!skeleton());
 for (int version : {-1, 0, 1, 2, 4, 99}) { emptyMesh(); put(4, version); assert(!mesh()); }
 for (int version : {-1, 0, 1, 3, 99}) { oneBoneSkeleton(); put(4, version); assert(!skeleton()); }

 // A name that never terminates inside its own field is refused.
 emptyMesh(); std::memset(buffer.data() + 8, 'a', 64); assert(!mesh());
 oneBoneSkeleton(); std::memset(buffer.data() + 8, 'a', 64); assert(!skeleton());
 oneBoneSkeleton(); std::memset(buffer.data() + FN_WOLFET_MDX_HEADER_BYTES, 'b', 64); assert(!skeleton());

 // Negative and over-limit counts, and offsets past the end.
 for (int count : {-1, FN_WOLFET_MAX_SURFACES + 1}) { emptyMesh(); put(80, count); assert(!mesh()); }
 for (int count : {-1, FN_WOLFET_MAX_TAGS + 1}) { emptyMesh(); put(88, count); assert(!mesh()); }
 emptyMesh(); put(84, -1); assert(!mesh());
 emptyMesh(); put(80, 1); assert(!mesh());   // a surface that is not there
 emptyMesh(); put(88, 1); assert(!mesh());   // a tag that is not there
 for (int count : {-1, FN_WOLFET_MAX_BONES + 1, 0}) { oneBoneSkeleton(); put(76, count); assert(!skeleton()); }
 oneBoneSkeleton(); put(72, -1); assert(!skeleton());
 oneBoneSkeleton(); put(80, -1); assert(!skeleton());
 oneBoneSkeleton(); put(72, 100); assert(!skeleton());  // frames that are not there

 // The torso parent names a bone or nothing at all.
 oneBoneSkeleton(); put(88, 1); assert(!skeleton());
 oneBoneSkeleton(); put(88, -2); assert(!skeleton());
 oneBoneSkeleton(); put(88, 0); assert(skeleton());

 // A bone's parent must precede it, so no file can describe a cycle.
 oneBoneSkeleton(); put(FN_WOLFET_MDX_HEADER_BYTES + 64, 0); assert(!skeleton());
 oneBoneSkeleton(); put(FN_WOLFET_MDX_HEADER_BYTES + 64, 1); assert(!skeleton());
 oneBoneSkeleton(); put(FN_WOLFET_MDX_HEADER_BYTES + 64, -2); assert(!skeleton());

 // A frame carries one compressed pose per bone; the stride follows the count.
 {
  const size_t stride = FN_WOLFET_MDX_FRAME_BYTES + FN_WOLFET_MDX_BONE_FRAME_BYTES;
  oneBoneSkeleton();
  buffer.resize(FN_WOLFET_MDX_HEADER_BYTES + stride + FN_WOLFET_MDX_BONE_BYTES, 0);
  put(72, 1);
  put(80, FN_WOLFET_MDX_HEADER_BYTES);
  put(84, (int32_t)(FN_WOLFET_MDX_HEADER_BYTES + stride));
  put(FN_WOLFET_MDX_HEADER_BYTES + (int32_t)stride + 64, -1);
  fnWolfETSkeletonView_t view;
  assert(FnWolfET_Skeleton(buffer.data(), buffer.size(), &view));
  assert(view.frameStride == stride);
  fnWolfETFrame_t frame;
  fnWolfETBonePose_t pose;
  fnWolfETBone_t bone;
  assert(FnWolfET_Frame(&view, 0, &frame));
  assert(!FnWolfET_Frame(&view, 1, &frame));
  assert(FnWolfET_BonePose(&view, 0, 0, &pose));
  assert(!FnWolfET_BonePose(&view, 0, 1, &pose));
  assert(!FnWolfET_BonePose(&view, 1, 0, &pose));
  assert(FnWolfET_Bone(&view, 0, &bone, nullptr));
  assert(!FnWolfET_Bone(&view, 1, &bone, nullptr));
 }

 // A count that would wrap its stride must not become a small byte span.
 oneBoneSkeleton(); put(72, 0x40000000); assert(!skeleton());
 emptyMesh(); put(88, FN_WOLFET_MAX_TAGS); assert(!mesh());
 return 0;
}
'''


class WolfETRendererModelTests(unittest.TestCase):
    def test_reader(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/wolfet-renderer-models'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'models.cpp'
            executable = folder/('models.exe' if os.name == 'nt' else 'models')
            source.write_text(PROBE, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                '-I', str(ROOT), str(source), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True,
                                 text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_records_that_carry_their_own_data_are_chained(self):
        """Surfaces, tags and vertices are variable length; indexing any of them
        by a fixed stride would read the wrong record."""
        text = (ROOT/'code/renderercommon/tr_wolfet_model_format.h').read_text(encoding='utf-8')
        for walker in ('FnWolfET_MeshSurface', 'FnWolfET_MeshTag', 'FnWolfET_MeshVertex'):
            body = text[text.index('int %s(' % walker):]
            body = body[:body.index('\n}\n')]
            self.assertIn('offset +=', body, walker)
            self.assertNotIn('* FN_WOLFET_MDM_TAG_BYTES', body, walker)

    def test_the_probe_compares_both_readers(self):
        probe = (ROOT/'scripts/probe_wolfet_models.py').read_text(encoding='utf-8')
        self.assertIn('readers disagree', probe)
        self.assertIn('renderer reader refused', probe)
        self.assertIn('renderer=agrees', probe)

    def test_the_record_states_what_the_assets_show(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('all 21 files parse', record)
        # The two facts the shipped assets contradict the header comment on.
        self.assertIn('chained, not strided', record)
        self.assertIn('A vertex weight names a global bone', record)


if __name__ == '__main__':
    unittest.main()
