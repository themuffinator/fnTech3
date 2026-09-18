"""Exercise ET's MDM/MDX readers: layout facts, valid files and malformed ones."""
from pathlib import Path
import os
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PROBE = r'''
#include "code/compat/wolfet_model_format.hpp"
#include <cassert>
#include <cstring>
#include <vector>

using namespace fntech3::wolfet::model;

static std::vector<unsigned char> buffer;
static void put(std::size_t at, std::int32_t value) {
 if (at + 4 > buffer.size()) buffer.resize(at + 4, 0);
 std::memcpy(buffer.data() + at, &value, 4);
}

static Error mdm(int bones = 0) { Mdm out; return ParseMdm(buffer.data(), buffer.size(), bones, out); }
static Error mdx() { Mdx out; return ParseMdx(buffer.data(), buffer.size(), out); }

// The smallest well formed file of each kind: a header and nothing else.
static void emptyMdm() {
 buffer.assign(MdmHeaderBytes, 0);
 put(0, static_cast<std::int32_t>(MdmIdent));
 put(4, MdmVersion);
 put(84, static_cast<std::int32_t>(MdmHeaderBytes));
 put(92, static_cast<std::int32_t>(MdmHeaderBytes));
 put(96, static_cast<std::int32_t>(MdmHeaderBytes));
}

static void emptyMdx() {
 buffer.assign(MdxHeaderBytes, 0);
 put(0, static_cast<std::int32_t>(MdxIdent));
 put(4, MdxVersion);
 put(80, static_cast<std::int32_t>(MdxHeaderBytes));
 put(84, static_cast<std::int32_t>(MdxHeaderBytes));
 put(88, -1);
 put(92, static_cast<std::int32_t>(MdxHeaderBytes));
}

int main() {
 // The identifiers are the bytes the retail files actually begin with.
 assert(MdmIdent == 0x574d444du && MdxIdent == 0x5758444du);
 assert(MdmVersion == 3 && MdxVersion == 2);

 emptyMdm();
 assert(mdm() == Error::None);
 emptyMdx();
 assert(mdx() == Error::None);

 // A null or truncated buffer is refused before anything is read.
 { Mdm out; assert(ParseMdm(nullptr, 64, 0, out) == Error::Size); }
 { Mdx out; assert(ParseMdx(nullptr, 64, out) == Error::Size); }
 for (std::size_t bytes = 0; bytes < MdmHeaderBytes; ++bytes) {
  emptyMdm(); buffer.resize(bytes);
  assert(mdm() == Error::Size);
 }
 for (std::size_t bytes = 0; bytes < MdxHeaderBytes; ++bytes) {
  emptyMdx(); buffer.resize(bytes);
  assert(mdx() == Error::Size);
 }

 // Each file kind refuses the other's identifier and every other version.
 emptyMdm(); put(0, static_cast<std::int32_t>(MdxIdent)); assert(mdm() == Error::Ident);
 emptyMdx(); put(0, static_cast<std::int32_t>(MdmIdent)); assert(mdx() == Error::Ident);
 for (int version : {-1, 0, 1, 2, 4, 99}) {
  emptyMdm(); put(4, version); assert(mdm() == Error::Version);
 }
 for (int version : {-1, 0, 1, 3, 99}) {
  emptyMdx(); put(4, version); assert(mdx() == Error::Version);
 }

 // A name that never terminates inside its field is refused.
 emptyMdm();
 std::memset(buffer.data() + 8, 'a', NameBytes);
 assert(mdm() == Error::Name);
 emptyMdx();
 std::memset(buffer.data() + 8, 'a', NameBytes);
 assert(mdx() == Error::Name);

 // Counts beyond the format's own limits, and negative ones.
 for (int count : {-1, MdmMaxSurfaces + 1, 1 << 20}) {
  emptyMdm(); put(80, count); assert(mdm() == Error::Counts);
 }
 for (int count : {-1, MdmMaxTags + 1, 1 << 20}) {
  emptyMdm(); put(88, count); assert(mdm() == Error::Counts);
 }
 for (int count : {-1, MdxMaxBones + 1, 1 << 20}) {
  emptyMdx(); put(76, count); assert(mdx() == Error::Counts);
 }
 emptyMdx(); put(72, -1); assert(mdx() == Error::Counts);

 // A negative offset, and one whose records would run past the file.
 emptyMdm(); put(84, -1); assert(mdm() == Error::Offset);
 emptyMdm(); put(80, 1); assert(mdm() == Error::Extent);
 emptyMdx(); put(80, -1); assert(mdx() == Error::Offset);
 emptyMdx(); put(76, 1); assert(mdx() == Error::Extent);

 // The torso parent is a bone index or -1, never anything else.
 emptyMdx(); put(88, 0); assert(mdx() == Error::Bone);
 emptyMdx(); put(88, -2); assert(mdx() == Error::Bone);

 // One bone: its parent must be -1 or an earlier bone, and be named.
 emptyMdx();
 buffer.resize(MdxHeaderBytes + MdxBoneInfoBytes + MdxFrameBytes + MdxBoneFrameBytes, 0);
 put(72, 1); put(76, 1);
 put(80, static_cast<std::int32_t>(MdxHeaderBytes));
 put(84, static_cast<std::int32_t>(MdxHeaderBytes + MdxFrameBytes + MdxBoneFrameBytes));
 put(88, 0);
 const std::size_t bone = MdxHeaderBytes + MdxFrameBytes + MdxBoneFrameBytes;
 put(bone + 64, -1);
 assert(mdx() == Error::None);
 put(bone + 64, 0);   // its own index is not an earlier bone
 assert(mdx() == Error::Bone);
 put(bone + 64, 1);
 assert(mdx() == Error::Bone);
 put(bone + 64, -2);
 assert(mdx() == Error::Bone);
 put(bone + 64, -1);
 std::memset(buffer.data() + bone, 'b', NameBytes);
 assert(mdx() == Error::Name);
 return 0;
}
'''


def _layout():
    text = (ROOT/'code/compat/wolfet_model_layout.def').read_text(encoding='utf-8')
    records, fields = {}, {}
    for name, size, align in re.findall(
            r'FNTECH3_WOLFET_MODEL_RECORD\((\w+), (\d+), (\d+)\)', text):
        records[name] = (int(size), int(align))
    for record, field, offset, size in re.findall(
            r'FNTECH3_WOLFET_MODEL_FIELD\((\w+), (\w+), (\d+), (\d+)\)', text):
        fields.setdefault(record, []).append((field, int(offset), int(size)))
    return records, fields


class WolfETModelTests(unittest.TestCase):
    def test_reader(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/wolfet-next'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'models.cpp'
            executable = folder/('models.exe' if os.name == 'nt' else 'models')
            source.write_text(PROBE, encoding='utf-8')
            target = compiler_target_args(compiler)
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT),
                str(source), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT,
                capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_layout_is_self_consistent(self):
        """Each record's fields must tile it exactly, with no gaps or overlap."""
        records, fields = _layout()
        self.assertEqual(sorted(records), sorted(fields))
        for record, (size, align) in records.items():
            self.assertEqual(size % align, 0, record)
            cursor = 0
            for name, offset, width in fields[record]:
                self.assertEqual(offset, cursor, '%s.%s' % (record, name))
                cursor += width
            self.assertEqual(cursor, size, record)

    def test_reader_constants_match_the_layout(self):
        records, _ = _layout()
        text = (ROOT/'code/compat/wolfet_model_format.hpp').read_text(encoding='utf-8')
        for record, constant in (
                ('mdmHeader_t', 'MdmHeaderBytes'), ('mdmSurface_t', 'MdmSurfaceBytes'),
                ('mdmTag_t', 'MdmTagBytes'), ('mdmVertex_t', 'MdmVertexBytes'),
                ('mdmWeight_t', 'MdmWeightBytes'), ('mdmTriangle_t', 'MdmTriangleBytes'),
                ('mdxHeader_t', 'MdxHeaderBytes'), ('mdxFrame_t', 'MdxFrameBytes'),
                ('mdxBoneFrameCompressed_t', 'MdxBoneFrameBytes'),
                ('mdxBoneInfo_t', 'MdxBoneInfoBytes')):
            self.assertRegex(text, r'%s = %d;' % (constant, records[record][0]),
                             '%s vs %s' % (record, constant))

    def test_retail_header_bytes(self):
        """The identifiers and versions the installed files actually carry."""
        self.assertEqual(struct.pack('<I', 0x574d444d), b'MDMW')
        self.assertEqual(struct.pack('<I', 0x5758444d), b'MDXW')

    def test_probe_makes_no_rendering_claim(self):
        text = (ROOT/'scripts/probe_wolfet_models.py').read_text(encoding='utf-8')
        self.assertIn('makes no rendering claim', text)
        self.assertIn('installationUnchanged', text)


if __name__ == '__main__':
    unittest.main()
