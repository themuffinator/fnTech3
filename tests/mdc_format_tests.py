"""Exercise the MDC byte boundary in C and C++, including hostile file ranges."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

FIXTURE = r'''
#include "code/renderercommon/tr_mdc_format.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

enum { FRAMES = 113, NAMES = FRAMES + 3 * 56, TAGS = NAMES + 64,
       SURFACE = TAGS + 3 * 12, TRI = 125, SHADER = TRI + 12,
       ST = SHADER + 68, BASE = ST + 24, COMP = BASE + 48,
       BASEMAP = COMP + 12, COMPMAP = BASEMAP + 6,
       SURFBYTES = COMPMAP + 6, BYTES = SURFACE + SURFBYTES };
static unsigned char original[BYTES];
static void Put32(unsigned char *p, uint32_t value) {
    int i; for (i = 0; i < 4; ++i) p[i] = (unsigned char)(value >> (8 * i));
}
static void Put16(unsigned char *p, int value) {
    p[0] = (unsigned char)value; p[1] = (unsigned char)((unsigned)value >> 8);
}
static void PutFloat(unsigned char *p, float value) {
    uint32_t bits; memcpy(&bits, &value, 4); Put32(p, bits);
}
static void Make(void) {
    unsigned char *s = original + SURFACE; int i, j;
    Put32(original, UINT32_C(0x43504449)); Put32(original + 4, 2);
    memcpy(original + 8, "model", 6);
    Put32(original + 76, 3); Put32(original + 80, 1); Put32(original + 84, 1);
    Put32(original + 92, FRAMES); Put32(original + 96, NAMES);
    Put32(original + 100, TAGS); Put32(original + 104, SURFACE); Put32(original + 108, BYTES);
    for (i = 0; i < 3; ++i) {
        unsigned char *f = original + FRAMES + i * 56;
        for (j = 0; j < 3; ++j) {
            PutFloat(f + j * 4, -10.0f - i); PutFloat(f + 12 + j * 4, 20.0f + i);
            PutFloat(f + 24 + j * 4, (float)(i + j));
            Put16(original + TAGS + i * 12 + j * 2, (i - 1) * 64 + j);
            Put16(original + TAGS + i * 12 + 6 + j * 2, (i - 1) * 16350 + j);
        }
        PutFloat(f + 36, 32.0f + i);
    }
    memcpy(original + NAMES, "tag_test", 9); Put32(s, 7); memcpy(s + 4, "surface_1", 10);
    Put32(s + 72, 1); Put32(s + 76, 2); Put32(s + 80, 1);
    Put32(s + 84, 3); Put32(s + 88, 1); Put32(s + 92, TRI);
    Put32(s + 96, SHADER); Put32(s + 100, ST); Put32(s + 104, BASE);
    Put32(s + 108, COMP); Put32(s + 112, BASEMAP); Put32(s + 116, COMPMAP); Put32(s + 120, SURFBYTES);
    Put32(s + TRI, 2); Put32(s + TRI + 4, 0); Put32(s + TRI + 8, 1);
    memcpy(s + SHADER, "textures/test", 14);
    for (i = 0; i < 3; ++i) {
        PutFloat(s + ST + i * 8, i * 0.25f); PutFloat(s + ST + i * 8 + 4, i * -0.5f);
        for (j = 0; j < 3; ++j) {
            Put16(s + BASE + i * 8 + j * 2, -32768 + i * 64 + j);
            Put16(s + BASE + 24 + i * 8 + j * 2, 32767 - i * 64 - j);
        }
        Put16(s + BASE + i * 8 + 6, i * 1000);
        Put16(s + BASE + 24 + i * 8 + 6, 0xffff - i * 1000);
        Put32(s + COMP + i * 4, UINT32_C(0xfffe7f00) - ((uint32_t)i << 24));
    }
    Put16(s + BASEMAP, 0); Put16(s + BASEMAP + 2, 1); Put16(s + BASEMAP + 4, 0);
    Put16(s + COMPMAP, -1); Put16(s + COMPMAP + 2, -1); Put16(s + COMPMAP + 4, 0);
}
static int Exercise(const unsigned char *p, size_t bytes) {
    fnMdcView_t view; fnMdcSurfaceView_t s; fnMdcFrame_t f; fnMdcVertex_t v;
    float origin[3], angles[3]; uint32_t i, j, k, offset;
    if (!FnMdc_Open(p, bytes, &view)) return 0;
    for (i = 0; i < view.numFrames; ++i) {
        assert(FnMdc_Frame(&view, i, &f));
        for (j = 0; j < view.numTags; ++j) assert(FnMdc_Tag(&view, i, j, origin, angles));
    }
    offset = view.surfaces;
    for (i = 0; i < view.numSurfaces; ++i) {
        assert(FnMdc_Surface(&view, offset, &s)); assert(FnMdc_ValidateSurface(&s));
        for (j = 0; j < s.numFrames; ++j)
            for (k = 0; k < s.numVerts; ++k) assert(FnMdc_Vertex(&s, j, k, &v));
        offset += (uint32_t)s.bytes;
    }
    return 1;
}
static void RejectWord(size_t offset, uint32_t value) {
    unsigned char p[BYTES]; fnMdcView_t view, before;
    memcpy(p, original, BYTES); Put32(p + offset, value);
    memset(&view, 0xa5, sizeof(view)); memcpy(&before, &view, sizeof(view));
    if (FnMdc_Open(p, BYTES, &view)) { fprintf(stderr, "Unexpected acceptance: %u = %u\n", (unsigned)offset, value); abort(); }
    assert(!memcmp(&view, &before, sizeof(view)));
}
int main(void) {
    fnMdcView_t view, before; fnMdcSurfaceView_t surface;
    fnMdcFrame_t frame; fnMdcVertex_t v, saved; float xyz[3], angles[3];
    size_t i; unsigned char p[BYTES + 7];
    static const int rootCounts[] = {76, 80, 84, 88};
    static const int rootOffsets[] = {92, 96, 100, 104, 108};
    static const int surfCounts[] = {72, 76, 80, 84, 88};
    static const int surfOffsets[] = {92, 96, 100, 104, 108, 112, 116, 120};
    Make(); assert(FnMdc_Open(original, BYTES, &view));
    assert(view.numFrames == 3 && view.numTags == 1 && view.numSurfaces == 1);
    assert(FnMdc_Surface(&view, SURFACE, &surface));
    assert(FnMdc_Frame(&view, 2, &frame)); assert(frame.radius == 34 && frame.bounds[0][0] == -12);
    assert(FnMdc_Vertex(&surface, 0, 0, &v)); assert(v.xyz[0] == -512 && !v.compressedNormal && !v.normal);
    assert(FnMdc_Vertex(&surface, 1, 0, &v)); assert(v.xyz[0] == 32767.0f / 64 && v.normal == 65535);
    assert(FnMdc_Vertex(&surface, 2, 0, &v));
    assert(v.xyz[0] == -512.0f + (float)(-127 * 0.05) && v.xyz[1] == -32767.0f / 64);
    assert(v.xyz[2] == -32766.0f / 64 + (float)(127 * 0.05) && v.normal == 255 && v.compressedNormal);
    assert(FnMdc_Tag(&view, 0, 0, xyz, angles)); assert(xyz[0] == -1 && angles[0] == -180);
    assert(FnMdc_Tag(&view, 2, 0, xyz, xyz)); assert(xyz[0] == 180);
    assert(!FnMdc_Tag(&view, 3, 0, xyz, angles) && !FnMdc_Tag(&view, 0, 1, xyz, angles));
    assert(!FnMdc_Tag(NULL, 0, 0, xyz, angles) && !FnMdc_Tag(&view, 0, 0, NULL, angles));
    memcpy(&saved, &v, sizeof(v));
    assert(!FnMdc_Vertex(&surface, 3, 0, &v) && !FnMdc_Vertex(&surface, 0, 3, &v));
    assert(!memcmp(&saved, &v, sizeof(v))); assert(!FnMdc_Vertex(NULL, 0, 0, &v));
    assert(!FnMdc_Frame(&view, 3, &frame) && !FnMdc_Surface(&view, BYTES - 123, &surface));
    assert(!FnMdc_Open(NULL, BYTES, &view) && !FnMdc_Open(original, BYTES, NULL));
    assert(!FnMdc_Range(100, 1, 1, 0, 0));
    for (i = 0; i < BYTES; ++i) {
        unsigned char *shortFile = (unsigned char *)malloc(i ? i : 1);
        memcpy(shortFile, original, i); memcpy(&before, &view, sizeof(view));
        assert(!FnMdc_Open(shortFile, i, &view)); assert(!memcmp(&before, &view, sizeof(view)));
        free(shortFile);
    }
    for (i = 0; i < 8; ++i) { memcpy(p + i, original, BYTES); assert(Exercise(p + i, BYTES)); }
    memcpy(p, original, BYTES); memset(p + BYTES, 0xff, 7); assert(Exercise(p, sizeof(p)));
    RejectWord(0, UINT32_C(0x33504449)); RejectWord(4, 1); RejectWord(76, 0);
    for (i = 0; i < sizeof(rootCounts) / sizeof(rootCounts[0]); ++i) {
        RejectWord(rootCounts[i], UINT32_MAX);
        if (rootCounts[i] != 88) RejectWord(rootCounts[i], INT_MAX); // numSkins is informational.
    }
    for (i = 0; i < sizeof(rootOffsets) / sizeof(rootOffsets[0]); ++i) {
        RejectWord(rootOffsets[i], 0); RejectWord(rootOffsets[i], BYTES + 1); RejectWord(rootOffsets[i], UINT32_MAX);
    }
    for (i = 0; i < sizeof(surfCounts) / sizeof(surfCounts[0]); ++i) {
        RejectWord(SURFACE + surfCounts[i], UINT32_MAX); RejectWord(SURFACE + surfCounts[i], INT_MAX);
    }
    for (i = 0; i < sizeof(surfOffsets) / sizeof(surfOffsets[0]); ++i) {
        RejectWord(SURFACE + surfOffsets[i], 0); RejectWord(SURFACE + surfOffsets[i], SURFBYTES + 1);
        RejectWord(SURFACE + surfOffsets[i], UINT32_MAX);
    }
    RejectWord(SURFACE + 76, 0); RejectWord(SURFACE + TRI, 3); RejectWord(SURFACE + TRI + 8, UINT32_MAX);
    RejectWord(SURFACE + BASEMAP, 2); RejectWord(SURFACE + BASEMAP, UINT32_MAX);
    RejectWord(SURFACE + COMPMAP, UINT32_C(0xfffffffe)); RejectWord(SURFACE + COMPMAP, 1);
    RejectWord(FRAMES, UINT32_C(0x7fc00000)); RejectWord(FRAMES + 12, UINT32_C(0xff800000));
    RejectWord(FRAMES + 24, UINT32_C(0x7f800000)); RejectWord(FRAMES + 36, UINT32_C(0xbf800000));
    RejectWord(SURFACE + ST, UINT32_C(0x7fc00000)); RejectWord(SURFACE + ST + 4, UINT32_C(0x7f800000));
    for (i = 0; i < 4; ++i) {
        static const int names[] = {8, NAMES, SURFACE + 4, SURFACE + SHADER};
        memcpy(p, original, BYTES); memset(p + names[i], 'a', 64); assert(!FnMdc_Open(p, BYTES, &view));
    }
    // A zero-surface model, unused shader list, and empty surface are representable.
    memcpy(p, original, BYTES); Put32(p + 84, 0); assert(Exercise(p, BYTES));
    memcpy(p, original, BYTES); Put32(p + 80, 0); Put32(p + 96, 0); Put32(p + 100, 0); assert(Exercise(p, BYTES));
    memcpy(p, original, BYTES); Put32(p + SURFACE + 80, 0); Put32(p + SURFACE + 96, 0); assert(Exercise(p, BYTES));
    memcpy(p, original, BYTES); Put32(p + SURFACE + 84, 0); Put32(p + SURFACE + 88, 0); assert(Exercise(p, BYTES));
    // Mutate every byte through zero/ones. Accepted variants must remain readable.
    for (i = 0; i < BYTES; ++i) {
        memcpy(p, original, BYTES); p[i] = 0; Exercise(p, BYTES);
        memcpy(p, original, BYTES); p[i] = 255; Exercise(p, BYTES);
    }
    return 0;
}
'''

MATH_FIXTURE = FIXTURE[:FIXTURE.index('int main(void)')] + r'''
#include "code/renderercommon/tr_mdc_decode.h"
#include <float.h>
int main(void) {
    fnMdcNormals_t table;
    fnMdcView_t view; fnMdcSurfaceView_t surface; fnMdcVertex_t vertex;
    uint64_t hash = UINT64_C(14695981039346656037);
    unsigned i, j, k; float normal[3], xyz[3], saved[3];
    FnMdc_InitNormals(&table);
    for (i = 0; i < 256; ++i) {
        vertex.normal = (uint16_t)i; vertex.compressedNormal = 1;
        FnMdc_DecodeNormal(&table, &vertex, normal);
        for (j = 0; j < 3; ++j) {
            uint32_t bits; memcpy(&bits, &normal[j], 4);
            for (k = 0; k < 4; ++k) { hash ^= (bits >> (k * 8)) & 255; hash *= UINT64_C(1099511628211); }
        }
    }
    // Encoding-data fingerprint independently checked against all four source dictionaries.
    assert(hash == UINT64_C(0xbd75001e7347a792));
    vertex.compressedNormal = 0;
    for (i = 0; i < 65536; ++i) {
        float length = 0;
        vertex.normal = (uint16_t)i; FnMdc_DecodeNormal(&table, &vertex, normal);
        for (j = 0; j < 3; ++j) { assert(isfinite(normal[j])); length += normal[j] * normal[j]; }
        assert(length > 0.99f && length < 1.01f);
    }
    Make(); assert(FnMdc_Open(original, BYTES, &view)); assert(FnMdc_Surface(&view, SURFACE, &surface));
    for (i = 0; i < 3; ++i) {
        assert(FnMdc_Vertex(&surface, i, 0, &vertex));
        assert(FnMdc_AnimatedVertex(&surface, &table, 9999, i, 0, 0, xyz, normal));
        assert(!memcmp(xyz, vertex.xyz, sizeof(xyz)));
        assert(FnMdc_AnimatedVertex(&surface, &table, i, 0, 1, 0, xyz, normal));
        assert(!memcmp(xyz, vertex.xyz, sizeof(xyz)));
        for (j = 0; j < 3; ++j) {
            for (k = 0; k < 7; ++k) {
                const float back = (int)(k - 1) * 0.25f;
                assert(FnMdc_AnimatedVertex(&surface, &table, i, j, back, 0, xyz, normal));
                assert(isfinite(xyz[0]) && isfinite(normal[0]));
            }
        }
    }
    memcpy(saved, xyz, sizeof(saved));
    assert(!FnMdc_AnimatedVertex(&surface, &table, 9999, 0, 0.5f, 0, xyz, normal));
    assert(!FnMdc_AnimatedVertex(&surface, &table, 0, 9999, 0, 0, xyz, normal));
    assert(!FnMdc_AnimatedVertex(&surface, &table, 0, 0, 0, 9999, xyz, normal));
    assert(!FnMdc_AnimatedVertex(&surface, &table, 0, 0, NAN, 0, xyz, normal));
    assert(!FnMdc_AnimatedVertex(&surface, &table, 0, 0, INFINITY, 0, xyz, normal));
    assert(!FnMdc_AnimatedVertex(&surface, &table, 0, 1, FLT_MAX, 0, xyz, normal));
    assert(!FnMdc_AnimatedVertex(&surface, NULL, 0, 0, 0, 0, xyz, normal));
    assert(!memcmp(saved, xyz, sizeof(xyz)));
    assert(FnMdc_AnimatedVertex(&surface, &table, 0, 0, 0, 0, xyz, xyz));
    assert(FnMdc_Vertex(&surface, 0, 0, &vertex)); FnMdc_DecodeNormal(&table, &vertex, normal);
    assert(!memcmp(xyz, normal, sizeof(xyz)));
    // Inverted finite exporter bounds are used by real geometry and tag-only files.
    PutFloat(original + FRAMES, 99999); PutFloat(original + FRAMES + 12, -99999);
    assert(FnMdc_Open(original, BYTES, &view));
    return 0;
}
'''


class MdcFormatTests(unittest.TestCase):
    def run_fixture(self, body):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C11 compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        for language, standard in (('c', 'c11'), ('cpp', 'c++17')):
            with self.subTest(language=language), tempfile.TemporaryDirectory(prefix='mdc-format-', dir=ROOT / '.tmp') as temp:
                folder = Path(temp)
                source = folder / ('probe.' + language)
                executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                source.write_text(body, encoding='utf-8')
                build = subprocess.run([compiler, *target, '-std=' + standard, '-I', str(ROOT),
                    str(source), '-o', str(executable), *(['-lm'] if os.name != 'nt' else [])],
                    cwd=ROOT, capture_output=True, text=True, timeout=60)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=20, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_unaligned_transactional_reader_and_hostile_ranges(self):
        self.run_fixture(FIXTURE)

    def test_normal_dictionary_animation_and_transactional_failures(self):
        self.run_fixture(MATH_FIXTURE)


if __name__ == '__main__':
    unittest.main()
