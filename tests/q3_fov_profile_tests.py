"""Renderer projection contract tests; no graphics, modules, or input needed."""

from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest


from compiler_support import compiler_target_args


ROOT = Path(__file__).resolve().parents[1]


HARNESS = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef int qboolean;
enum { qfalse, qtrue, CVAR_ARCHIVE_ND = 1, CV_INTEGER = 2 };
typedef struct { int integer; } cvar_t;
typedef struct { int width, height, rdflags; } refdef_t;
static struct { int vidWidth, vidHeight; float windowAspect; } glConfig;
#if defined(USE_FBO) || defined(USE_VULKAN)
static struct { int windowWidth, windowHeight; } gls;
static cvar_t fbo = {1}, scale = {1};
static cvar_t *r_fbo = &fbo, *r_renderScale = &scale;
#endif
static cvar_t enabled = {1};
static const char *profile;
static int registrations, profileReads;
static const char *Profile(const char *name) {
    assert(!strcmp(name, "com_gameProfile"));
    ++profileReads;
    return profile;
}
static cvar_t *Register(const char *name, const char *value, int flags) {
    assert(!strcmp(profile, "quake3") || !strcmp(profile, "rtcw-sp") || !strcmp(profile, "rtcw-mp"));
    assert(!strcmp(name, "r_fovCorrection") && !strcmp(value, "1"));
    assert(flags == CVAR_ARCHIVE_ND);
    ++registrations;
    return &enabled;
}
static void Range(cvar_t *cv, const char *lo, const char *hi, int kind) {
    assert(cv == &enabled && !strcmp(lo, "0") && !strcmp(hi, "1") && kind == CV_INTEGER);
}
static void Description(cvar_t *cv, const char *message) {
    assert(cv == &enabled && message && message[0]);
}
static struct {
    const char *(*Cvar_VariableString)(const char *);
    cvar_t *(*Cvar_Get)(const char *, const char *, int);
    void (*Cvar_CheckRange)(cvar_t *, const char *, const char *, int);
    void (*Cvar_SetDescription)(cvar_t *, const char *);
} ri = {Profile, Register, Range, Description};
#define Q_stricmp strcmp
#include "code/renderercommon/tr_q3_fov.h"

static void ExpectFov(refdef_t view, float expectedX, float expectedY) {
    float x = 90.0f, y = 60.0f;
    R_ApplyProfileFovCorrection(&view, &x, &y);
    if (fabsf(x - expectedX) >= 0.0002f || fabsf(y - expectedY) >= 0.0002f) {
        fprintf(stderr, "got %f %f expected %f %f flags %d\n", x, y, expectedX, expectedY, view.rdflags);
        assert(0);
    }
}
int main(int argc, char **argv) {
    refdef_t view = {1920, 1080, 0};
    float x, y;
    int flag;
    assert(argc == 2);
    profile = argv[1];
    glConfig.vidWidth = 1920; glConfig.vidHeight = 1080;
    glConfig.windowAspect = 16.0f / 9.0f;
#if defined(USE_FBO) || defined(USE_VULKAN)
    gls.windowWidth = 1920; gls.windowHeight = 1080;
#endif
    if (strcmp(profile, "quake3") && strcmp(profile, "rtcw-sp") && strcmp(profile, "rtcw-mp")) {
        for (flag = 0; flag < 16; ++flag) {
            view.rdflags = flag;
            ExpectFov(view, 90.0f, 60.0f);
        }
        assert(registrations == 0 && profileReads == 1);
        return 0;
    }
    ExpectFov(view, 106.260205f, 73.739795f);
    assert(registrations == 1 && profileReads == 1);
    view.rdflags = RDF_NOFOVCORRECTION;
    ExpectFov(view, 90.0f, 60.0f);
    view.rdflags = RDF_NOWORLDMODEL;
    ExpectFov(view, 90.0f, 60.0f); /* HUD and menu model projections. */
    view.rdflags = RDF_HYPERSPACE;
    ExpectFov(view, 106.260205f, 73.739795f); /* Bit 4 is distinct from Q3 bit 2. */
    view.rdflags = 0; enabled.integer = 0;
    ExpectFov(view, 90.0f, 60.0f);
    enabled.integer = 1;
    view.width = glConfig.vidWidth = 2560;
    glConfig.windowAspect = 2560.0f / 1080.0f;
    ExpectFov(view, 121.284493f, 73.739795f);
    view.width = glConfig.vidWidth = 1080;
    view.height = glConfig.vidHeight = 1920;
    glConfig.windowAspect = 1080.0f / 1920.0f;
    ExpectFov(view, 45.747330f, 73.739795f);
    view.width = glConfig.vidWidth = 1024;
    view.height = glConfig.vidHeight = 768;
    glConfig.windowAspect = 4.0f / 3.0f;
    ExpectFov(view, 90.0f, 60.0f);
    glConfig.windowAspect = 16.0f / 9.0f;
    ExpectFov(view, 106.260205f, 73.739795f); /* Stretched 4:3 render target. */
#if defined(USE_FBO) || defined(USE_VULKAN)
    scale.integer = 2;
    ExpectFov(view, 90.0f, 60.0f); /* Letterbox/pillarbox preserves render aspect. */
    scale.integer = 1;
#endif
    view.width = -1;
    ExpectFov(view, 90.0f, 60.0f);
    view.width = 1920; view.height = 0;
    ExpectFov(view, 90.0f, 60.0f);
    x = NAN; y = 60.0f;
    R_CorrectQ3ViewportFov(1920, 1080, 1920, 1080, 16.0f/9.0f, qfalse, &x, &y);
    assert(isnan(x) && y == 60.0f);
    x = 90.0f; y = INFINITY;
    R_CorrectQ3ViewportFov(1920, 1080, 1920, 1080, 16.0f/9.0f, qfalse, &x, &y);
    assert(x == 90.0f && isinf(y));
    x = 179.0f; y = 60.0f;
    R_CorrectQ3ViewportFov(1920, 1080, 1920, 1080, 16.0f/9.0f, qfalse, &x, &y);
    assert(x == 179.0f && y == 60.0f);
    R_CorrectQ3ViewportFov(1920, 1080, 1920, 1080, 16.0f/9.0f, qfalse, NULL, &y);
    assert(y == 60.0f);
    assert(registrations == 1 && profileReads == 1);
    return 0;
}
'''


class Q3FovProfileTests(unittest.TestCase):
    def test_legacy_projection_and_ql_isolation_in_each_backend_mode(self) -> None:
        compiler = next((path for name in ("clang", "gcc", "cc")
            if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C11 compiler (clang, gcc, or cc)")
        public = (ROOT / "code/renderercommon/tr_types.h").read_text(encoding="utf-8")
        flags = "\n".join(re.findall(r"^#define\s+RDF_\w+\s+\S+", public, re.M))
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="q3-fov-", dir=scratch) as folder:
            path = Path(folder)
            fixture = path / "fov.c"
            executable = path / ("fov.exe" if os.name == "nt" else "fov")
            fixture.write_text(flags + "\n" + HARNESS, encoding="utf-8")
            for mode in (None, "USE_FBO", "USE_VULKAN"):
                with self.subTest(mode=mode):
                    command = [compiler, *compiler_target_args(compiler), "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-I", str(ROOT), str(fixture), "-o", str(executable)]
                    if mode:
                        command.append("-D" + mode)
                    if os.name != "nt":
                        command.append("-lm")
                    build = subprocess.run(command, text=True, capture_output=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    for profile in ("quake3", "quakelive", "rtcw-sp", "rtcw-mp", "unknown"):
                        result = subprocess.run([str(executable), profile],
                            text=True, capture_output=True, timeout=15)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_all_renderers_apply_the_same_policy_and_q3_ui_opts_out(self) -> None:
        for backend in ("renderer", "renderervk", "rendererrtx"):
            source = (ROOT / "code" / backend / "tr_scene.c").read_text(encoding="utf-8")
            self.assertIn('#include "../renderercommon/tr_q3_fov.h"', source)
            self.assertIn("R_ApplyProfileFovCorrection( fd, &tr.refdef.fov_x, &tr.refdef.fov_y );", source)
        ui = (ROOT / "code/client/cl_ui.cpp").read_text(encoding="utf-8")
        render = ui.split("case UI_R_RENDERSCENE:", 1)[1].split("case UI_R_SETCOLOR:", 1)[0]
        self.assertRegex(render, r"if \( Com_GameIsQuake3\(\) \)\s*\{\s*refdef.rdflags \|= RDF_NOFOVCORRECTION;")


if __name__ == "__main__":
    unittest.main()
