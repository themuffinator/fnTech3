"""MDS byte-boundary regressions: graph validation and variable-length records."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

FIXTURE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/renderercommon/tr_mds_format.h"
#include <assert.h>
#include <stdlib.h>
enum { FRAMES = 121, BONES = FRAMES + 2 * 88, TAGS = BONES + 3 * 80,
    SURFACE = TAGS + 72, VERTS = 177, TRIANGLES = VERTS + 176,
    COLLAPSE = TRIANGLES + 12, REFS = COLLAPSE + 12, SURFBYTES = REFS + 8,
    BYTES = SURFACE + SURFBYTES };
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
    unsigned char *surface = original + SURFACE;
    int frame, bone, vertex, offset = VERTS;
    memset(original, 0, sizeof(original));
    Put32(original, 0x5753444d); Put32(original + 4, 4); memcpy(original + 8, "model", 6);
    PutFloat(original + 72, 1); PutFloat(original + 76, -1);
    Put32(original + 80, 2); Put32(original + 84, 3); Put32(original + 88, FRAMES); Put32(original + 92, BONES);
    Put32(original + 96, 1); Put32(original + 100, 1); Put32(original + 104, SURFACE);
    Put32(original + 108, 1); Put32(original + 112, TAGS); Put32(original + 116, BYTES);
    for (frame = 0; frame < 2; ++frame) {
        unsigned char *f = original + FRAMES + frame * 88;
        int i;
        for (i = 0; i < 3; ++i) {
            PutFloat(f + i * 4, -64); PutFloat(f + 12 + i * 4, 64);
            PutFloat(f + 24 + i * 4, (float)i); PutFloat(f + 40 + i * 4, (float)(frame * 10 + i));
        }
        PutFloat(f + 36, 100);
        for (bone = 0; bone < 3; ++bone) for (i = 0; i < 6; ++i)
            Put16(f + 52 + bone * 12 + i * 2, frame ? -32768 + bone * 100 + i : 32767 - bone * 100 - i);
    }
    for (bone = 0; bone < 3; ++bone) {
        unsigned char *b = original + BONES + bone * 80;
        b[0] = (unsigned char)('a' + bone); Put32(b + 64, (uint32_t)(bone - 1));
        PutFloat(b + 68, bone * 0.5f); PutFloat(b + 72, bone * 10.0f); Put32(b + 76, bone == 2);
    }
    memcpy(original + TAGS, "tag_head", 9); PutFloat(original + TAGS + 64, 1); Put32(original + TAGS + 68, 2);
    Put32(surface, 8); memcpy(surface + 4, "surface", 8); memcpy(surface + 68, "textures/test", 14);
    Put32(surface + 132, 0xdeadbeef); Put32(surface + 136, 1); Put32(surface + 140, (uint32_t)-SURFACE);
    Put32(surface + 144, 3); Put32(surface + 148, VERTS); Put32(surface + 152, 1); Put32(surface + 156, TRIANGLES);
    Put32(surface + 160, COLLAPSE); Put32(surface + 164, 2); Put32(surface + 168, REFS); Put32(surface + 172, SURFBYTES);
    for (vertex = 0; vertex < 3; ++vertex) {
        unsigned char *v = surface + offset;
        int weight, count = vertex == 1 ? 2 : 1;
        PutFloat(v + vertex * 4, 1); PutFloat(v + 12, vertex * 0.25f); PutFloat(v + 16, vertex * -0.5f);
        Put32(v + 20, count);
        // Unused exporter words are opaque even when their float bits are NaN.
        Put32(v + 24, 0xdeadbeef); Put32(v + 28, 0x7fc00001);
        for (weight = 0; weight < count; ++weight) {
            unsigned char *w = v + 32 + weight * 20;
            Put32(w, weight || vertex == 2 ? 2 : 0);
            PutFloat(w + 4, count == 1 ? 1.0f : weight ? 0.75f : 0.25f);
            PutFloat(w + 8, vertex + 0.125f); PutFloat(w + 12, weight - 10.5f); PutFloat(w + 16, 1000);
        }
        offset += 32 + count * 20;
    }
    Put32(surface + TRIANGLES, 0); Put32(surface + TRIANGLES + 4, 2); Put32(surface + TRIANGLES + 8, 1);
    Put32(surface + COLLAPSE, 0); Put32(surface + COLLAPSE + 4, 0); Put32(surface + COLLAPSE + 8, 1);
    Put32(surface + REFS, 2); Put32(surface + REFS + 4, 0); // Global bone 2 exceeds reference count.
}
static int Exercise(const unsigned char *data, size_t bytes) {
    fnMdsView_t view, before;
    uint32_t i, offset;
    memset(&view, 0xa5, sizeof(view)); before = view;
    if (!FnMds_Open(data, bytes, &view)) { assert(!memcmp(&view, &before, sizeof(view))); return 0; }
    assert(view.bytes <= bytes && view.numBones && view.numBones <= 128);
    for (i = 0; i < view.numFrames; ++i) {
        fnMdsFrame_t frame; uint32_t j;
        assert(FnMds_Frame(&view, i, &frame));
        for (j = 0; j < view.numBones; ++j) {
            fnMdsCompressedBone_t bone; assert(FnMds_CompressedBone(&view, i, j, &bone));
        }
    }
    for (i = 0; i < view.numBones; ++i) { fnMdsBone_t bone; assert(FnMds_Bone(&view, i, &bone)); }
    for (i = 0; i < view.numTags; ++i) { fnMdsTag_t tag; assert(FnMds_Tag(&view, i, &tag)); }
    offset = view.surfaces;
    for (i = 0; i < view.numSurfaces; ++i) {
        fnMdsSurfaceView_t surface; uint32_t j, vertexOffset;
        assert(FnMds_Surface(&view, offset, &surface) && FnMds_ValidateSurface(&surface));
        vertexOffset = surface.vertices;
        for (j = 0; j < surface.numVerts; ++j) {
            fnMdsVertexView_t vertex; uint32_t k;
            assert(FnMds_Vertex(&surface, vertexOffset, &vertex) && vertex.next > vertexOffset);
            for (k = 0; k < vertex.numWeights; ++k) { fnMdsWeight_t w; assert(FnMds_Weight(&vertex, k, &w)); }
            vertexOffset = vertex.next;
        }
        offset += (uint32_t)surface.bytes;
    }
    return 1;
}
static void Bad32(unsigned offset, uint32_t value) {
    unsigned char bad[BYTES]; memcpy(bad, original, BYTES); Put32(bad + offset, value); assert(!Exercise(bad, BYTES));
}
int main(void) {
    unsigned i, j;
    fnMdsView_t view; fnMdsSurfaceView_t surface; fnMdsFrame_t frame;
    fnMdsBone_t bone; fnMdsCompressedBone_t compressed; fnMdsTag_t tag;
    fnMdsVertexView_t vertex; fnMdsWeight_t weight;
    Make(); assert(Exercise(original, BYTES));
    for (i = 0; i < BYTES; ++i) {
        unsigned char *shortInput = (unsigned char *)malloc(i + 1); assert(shortInput);
        memcpy(shortInput + 1, original, i); assert(!Exercise(shortInput + 1, i)); free(shortInput);
    }
    for (i = 0; i < BYTES; ++i) for (j = 0; j < 2; ++j) {
        unsigned char mutated[BYTES + 2]; memcpy(mutated + 1, original, BYTES);
        mutated[i + 1] = j ? 255 : 0; Exercise(mutated + 1, BYTES);
    }
    assert(FnMds_Open(original, BYTES, &view));
    assert(FnMds_Frame(&view, 1, &frame) && frame.parentOffset[0] == 10 && frame.radius == 100);
    assert(FnMds_Bone(&view, 2, &bone) && bone.parent == 1 && bone.torsoWeight == 1 && bone.flags == 1);
    assert(FnMds_CompressedBone(&view, 1, 2, &compressed));
    assert(compressed.angles[0] == -32568 && compressed.angles[2] == -32566 && compressed.offsetAngles[0] == -32564);
    assert(FnMds_Tag(&view, 0, &tag) && tag.bone == 2 && tag.torsoWeight == 1);
    assert(FnMds_Surface(&view, SURFACE, &surface));
    assert(FnMds_Vertex(&surface, VERTS + 52, &vertex) && vertex.numWeights == 2 && vertex.next == VERTS + 124);
    assert(vertex.st[0] == 0.25f && vertex.st[1] == -0.5f && vertex.normal[1] == 1);
    assert(FnMds_Weight(&vertex, 1, &weight) && weight.bone == 2 && weight.weight == 0.75f && weight.offset[1] == -9.5f);
    {
        const fnMdsFrame_t saved = frame; const fnMdsVertexView_t savedVertex = vertex;
        assert(!FnMds_Frame(&view, 2, &frame) && !memcmp(&frame, &saved, sizeof(frame)));
        assert(!FnMds_Vertex(&surface, UINT32_MAX, &vertex) && !memcmp(&vertex, &savedVertex, sizeof(vertex)));
    }
    assert(!FnMds_Open(NULL, BYTES, &view)); assert(!FnMds_Open(original, BYTES, NULL));
    assert(!FnMds_Frame(NULL, 0, &frame)); assert(!FnMds_Frame(&view, 0, NULL));
    assert(!FnMds_Bone(NULL, 0, &bone)); assert(!FnMds_Bone(&view, 3, &bone)); assert(!FnMds_Bone(&view, 0, NULL));
    assert(!FnMds_CompressedBone(NULL, 0, 0, &compressed)); assert(!FnMds_CompressedBone(&view, 2, 0, &compressed));
    assert(!FnMds_CompressedBone(&view, 0, 3, &compressed)); assert(!FnMds_CompressedBone(&view, 0, 0, NULL));
    assert(!FnMds_Tag(NULL, 0, &tag)); assert(!FnMds_Tag(&view, 1, &tag)); assert(!FnMds_Tag(&view, 0, NULL));
    assert(!FnMds_Surface(NULL, SURFACE, &surface)); assert(!FnMds_Surface(&view, BYTES - 1, &surface));
    assert(!FnMds_Surface(&view, SURFACE, NULL)); assert(!FnMds_Vertex(NULL, VERTS, &vertex));
    assert(!FnMds_Vertex(&surface, VERTS, NULL)); assert(!FnMds_Weight(NULL, 0, &weight));
    assert(!FnMds_Weight(&vertex, vertex.numWeights, &weight)); assert(!FnMds_Weight(&vertex, 0, NULL));
    assert(!FnMds_Range(SIZE_MAX, 0, SIZE_MAX, 20, 0)); assert(!FnMds_Range(100, 101, 0, 1, 0));
    assert(!FnMds_Range(100, 0, 1, 0, 0)); assert(FnMds_Range(100, 100, 0, 1, 120));
    {
        unsigned char bits[4]; Put32(bits, 0x80000000); assert(FnMds_I32(bits) == INT32_MIN);
        Put32(bits, UINT32_MAX); assert(FnMds_I32(bits) == -1);
    }
    Bad32(4, 3); Bad32(80, 0); Bad32(80, UINT32_MAX); Bad32(84, 0); Bad32(84, 129); Bad32(96, 3);
    Bad32(0, 0);
    Bad32(88, UINT32_MAX); Bad32(92, BYTES); Bad32(112, BYTES - 1); Bad32(104, BYTES - 1);
    Bad32(100, INT_MAX); Bad32(108, INT_MAX); Bad32(116, INT_MAX); Bad32(72, 0x7fc00000);
    Bad32(BONES + 64, 0); Bad32(BONES + 64, 2); Bad32(BONES + 64, 0xfffffffe); // Self/long cycle/bad parent.
    Bad32(BONES + 72, 0xbf800000); Bad32(TAGS + 68, 3);
    Bad32(SURFACE + 140, 0); Bad32(SURFACE + 136, 4); Bad32(SURFACE + 144, UINT32_MAX);
    Bad32(SURFACE + 152, INT_MAX); Bad32(SURFACE + 164, 129); Bad32(SURFACE + 172, 175);
    Bad32(SURFACE + VERTS + 20, 0); Bad32(SURFACE + VERTS + 20, UINT32_MAX);
    Bad32(SURFACE + VERTS + 32, 1); Bad32(SURFACE + VERTS + 32, 3);
    Bad32(SURFACE + REFS, 3); Bad32(SURFACE + COLLAPSE, 1); Bad32(SURFACE + COLLAPSE + 8, 2);
    Bad32(SURFACE + TRIANGLES, 3); Bad32(FRAMES + 36, 0xbf800000);
    {
        const unsigned offsets[] = {FRAMES, FRAMES + 12, FRAMES + 24, FRAMES + 36, FRAMES + 40,
            BONES + 68, BONES + 72, TAGS + 64, SURFACE + VERTS, SURFACE + VERTS + 12,
            SURFACE + VERTS + 36, SURFACE + VERTS + 40};
        for (i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
            Bad32(offsets[i], 0x7fc00000); Bad32(offsets[i], 0x7f800000); Bad32(offsets[i], 0xff800000);
        }
    }
    {
        const unsigned names[] = {8, BONES, TAGS, SURFACE + 4, SURFACE + 68};
        for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
            unsigned char bad[BYTES]; memcpy(bad, original, BYTES); memset(bad + names[i], 'x', 64);
            assert(!Exercise(bad, BYTES));
        }
    }
    {
        enum { MAX_BONES = 128, LARGE_FRAMES = 120 + MAX_BONES * 80,
            LARGE_BYTES = LARGE_FRAMES + 52 + MAX_BONES * 12 };
        unsigned char *large = (unsigned char *)calloc(1, LARGE_BYTES); assert(large);
        Put32(large, 0x5753444d); Put32(large + 4, 4); Put32(large + 80, 1); Put32(large + 84, MAX_BONES);
        Put32(large + 88, LARGE_FRAMES); Put32(large + 92, 120); Put32(large + 96, MAX_BONES - 1); Put32(large + 116, LARGE_BYTES);
        for (i = 0; i < MAX_BONES; ++i) Put32(large + 120 + i * 80 + 64, i ? i - 1 : UINT32_MAX);
        assert(Exercise(large, LARGE_BYTES)); // Maximum-depth skeleton without surfaces or tags.
        Put32(large + 120 + 64, MAX_BONES - 1); assert(!Exercise(large, LARGE_BYTES)); // Full-length cycle.
        free(large);
    }
    {
        unsigned char unusual[BYTES + 1];
        memcpy(unusual, original, BYTES);
        Put32(unusual + BONES + 80 + 64, UINT32_MAX); assert(Exercise(unusual, BYTES)); // Multiple roots.
        Put32(unusual + BONES + 64, 2); assert(Exercise(unusual, BYTES)); // Valid forward ancestry 0 -> 2 -> 1.
        memcpy(unusual, original, BYTES); Put32(unusual + FRAMES, 0x447a0000); assert(Exercise(unusual, BYTES)); // Finite inverted bounds.
        memcpy(unusual, original, BYTES); PutFloat(unusual + SURFACE + VERTS + 36, -0.5f); assert(Exercise(unusual, BYTES)); // No weight renormalization.
        memcpy(unusual, original, BYTES); Put32(unusual + 100, 0); Put32(unusual + 104, 0); assert(Exercise(unusual, BYTES)); // Tag-only model.
        memcpy(unusual, original, BYTES); Put32(unusual + 108, 0); Put32(unusual + 112, 0); assert(Exercise(unusual, BYTES));
        memcpy(unusual, original, BYTES); Put32(unusual + SURFACE + 136, 0); Put32(unusual + SURFACE + 144, 0);
        Put32(unusual + SURFACE + 152, 0); Put32(unusual + SURFACE + 164, 0); assert(Exercise(unusual, BYTES));
        memcpy(unusual, original, BYTES); unusual[BYTES] = 0xf5; assert(Exercise(unusual, BYTES + 1)); // Trailing input.
    }
    return 0;
}
'''


class MdsFormatTests(unittest.TestCase):
    def test_c_and_cpp_bounded_reader(self):
        for language in ('c', 'c++'):
            compiler = shutil.which('clang' if language == 'c' else 'clang++')
            if not compiler:
                self.skipTest('Clang C/C++ compiler unavailable')
            target = compiler_target_args(compiler)
            if os.name != 'nt':
                target = ['-m32']
            with self.subTest(language=language), tempfile.TemporaryDirectory(prefix='mds-format-', dir=ROOT / '.tmp') as temp:
                folder = Path(temp)
                source = folder / ('probe.c' if language == 'c' else 'probe.cpp')
                executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
                source.write_text(FIXTURE)
                build = subprocess.run([compiler, *target, '-std=' + ('c11' if language == 'c' else 'c++17'),
                    '-I', str(ROOT), str(source), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
