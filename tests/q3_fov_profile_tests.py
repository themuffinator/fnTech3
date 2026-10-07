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
static int Corrected(void) {
    static const char *const corrected[] = {"quake3", "rtcw-sp", "rtcw-mp", "wolfet", "stvef-sp", "stvef-mp",
        "alice", "fakk2", "mohaa", "jka-sp", "jka-mp"};
    for (size_t i = 0; i < sizeof(corrected) / sizeof(corrected[0]); ++i)
        if (!strcmp(profile, corrected[i])) return 1;
    return 0;
}
static cvar_t aspectPreset = {0};
static int presetReads;
static cvar_t *Register(const char *name, const char *value, int flags) {
    if (!strcmp(name, "r_aspectRatio")) {
        /* The preset Quake Live's client publishes for its cgame; read, never owned. */
        assert(!strcmp(profile, "quakelive") && !strcmp(value, "0") && flags == 0);
        ++presetReads;
        return &aspectPreset;
    }
    assert(Corrected() || !strcmp(profile, "quakelive"));
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
    if (!strcmp(profile, "quakelive")) {
        /* Its cgame fits its own view to four shapes: 16:9 on a 16:9 screen is left alone. */
        aspectPreset.integer = 1;
        ExpectFov(view, 90.0f, 60.0f);
        assert(registrations == 1 && presetReads == 1 && profileReads == 1);
        /* At 21:9 its 16:9 field is widened; its vertical field is kept. */
        view.width = glConfig.vidWidth = 2560;
        glConfig.windowAspect = 2560.0f / 1080.0f;
        ExpectFov(view, 106.260205f, 60.0f);
        view.rdflags = RDF_NOWORLDMODEL;
        ExpectFov(view, 90.0f, 60.0f);   /* HUD and menu model scenes */
        view.rdflags = RDF_NOFOVCORRECTION;
        ExpectFov(view, 90.0f, 60.0f);
        view.rdflags = 0; enabled.integer = 0;
        ExpectFov(view, 90.0f, 60.0f);   /* retail's own field */
        enabled.integer = 1;
        /* 3:2 publishes the 4:3 preset, whose field is widened too. */
        aspectPreset.integer = 0;
        view.width = glConfig.vidWidth = 1440; view.height = glConfig.vidHeight = 960;
        glConfig.windowAspect = 1.5f;
        ExpectFov(view, 96.732921f, 60.0f);
        view.width = glConfig.vidWidth = 1024; view.height = glConfig.vidHeight = 768;
        glConfig.windowAspect = 4.0f / 3.0f;
        ExpectFov(view, 90.0f, 60.0f);
        aspectPreset.integer = 2;
        view.width = glConfig.vidWidth = 1920; view.height = glConfig.vidHeight = 1200;
        glConfig.windowAspect = 1.6f;
        ExpectFov(view, 90.0f, 60.0f);
        aspectPreset.integer = 3;
        view.width = glConfig.vidWidth = 1280; view.height = glConfig.vidHeight = 1024;
        glConfig.windowAspect = 1.25f;
        ExpectFov(view, 90.0f, 60.0f);
        view.width = glConfig.vidWidth = 1080; view.height = glConfig.vidHeight = 1920;
        glConfig.windowAspect = 1080.0f / 1920.0f;
        ExpectFov(view, 48.455491f, 60.0f); /* narrower than its narrowest preset */
        view.width = -1;
        ExpectFov(view, 90.0f, 60.0f);
        assert(registrations == 1 && presetReads == 1 && profileReads == 1);
        return 0;
    }
    if (!Corrected()) {
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
    /* A view the game cuts from a 4:3 screen, a letterbox, was authored so. */
    view.height = 576;
    ExpectFov(view, 90.0f, 60.0f);
    /* The same cut of a 16:9 screen keeps its 4:3 framing, widened. */
    view.width = glConfig.vidWidth = 1920;
    view.height = 810; glConfig.vidHeight = 1080;
    glConfig.windowAspect = 16.0f / 9.0f;
    ExpectFov(view, 106.260205f, 58.715507f);
    view.width = glConfig.vidWidth = 1024;
    view.height = glConfig.vidHeight = 768;
    glConfig.windowAspect = 4.0f / 3.0f;
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
            fixture.write_text(flags + "\n" + HARNESS, encoding="utf-8")
            for mode in (None, "USE_FBO", "USE_VULKAN"):
                # One executable per build: Windows can hold the last one run.
                name = "fov-" + (mode or "default").lower()
                executable = path / (name + ".exe" if os.name == "nt" else name)
                with self.subTest(mode=mode):
                    command = [compiler, *compiler_target_args(compiler), "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-I", str(ROOT), str(fixture), "-o", str(executable)]
                    if mode:
                        command.append("-D" + mode)
                    if os.name != "nt":
                        command.append("-lm")
                    build = subprocess.run(command, text=True, capture_output=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    for profile in ("quake3", "quakelive", "rtcw-sp", "rtcw-mp", "wolfet", "stvef-sp", "stvef-mp",
                                    "alice", "fakk2", "mohaa", "jka-sp", "jka-mp", "q3-ihv", "unknown"):
                        result = subprocess.run([str(executable), profile],
                            text=True, capture_output=True, timeout=15)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_all_renderers_apply_the_same_policy_and_q3_ui_opts_out(self) -> None:
        for backend in ("renderer", "renderervk", "rendererrtx"):
            source = (ROOT / "code" / backend / "tr_scene.c").read_text(encoding="utf-8")
            self.assertIn('#include "../renderercommon/tr_q3_fov.h"', source)
            self.assertIn("R_ApplyProfileFovCorrection( fd, &tr.refdef.fov_x, &tr.refdef.fov_y );", source)
            # Detail and distance rules scaled by the field keep the one the game gave.
            self.assertLess(source.index("tr.refdef.fovXAuthored = fd->fov_x;"),
                            source.index("R_ApplyProfileFovCorrection( fd, &tr.refdef.fov_x, &tr.refdef.fov_y );"))
            # MoHAA's first-person body stands behind the eye; its unposed bounds
            # must not cull its arms from a widened field.
            mohaa = (ROOT / "code" / backend / "tr_mohaa.c").read_text(encoding="utf-8")
            self.assertIn("if ( !( ent->e.renderfx & RF_DEPTHHACK ) && R_CullLocalBox( bounds ) == CULL_OUT ) {", mohaa)
            self.assertIn("tr.refdef.fovXAuthored > 0.0f ? tr.refdef.fovXAuthored : tr.viewParms.fovX", mohaa)
        foliage = (ROOT / "code/renderercommon/tr_wolfet_foliage_surface.h").read_text(encoding="utf-8")
        self.assertIn("backEnd.refdef.fovXAuthored > 0.0f ? backEnd.refdef.fovXAuthored : backEnd.viewParms.fovX",
                      foliage)
        ui = (ROOT / "code/client/cl_ui.cpp").read_text(encoding="utf-8")
        render = ui.split("case UI_R_RENDERSCENE:", 1)[1].split("case UI_R_SETCOLOR:", 1)[0]
        self.assertRegex(render, r"if \( Com_GameIsQuake3\(\) \|\| Com_GameIsSTVEF\(\) \|\| Com_GameIsJKA\(\) \)\s*\{\s*"
                                 r"refdef.rdflags \|= RDF_NOFOVCORRECTION;")
        # Elite Force single player's menus reach the renderer by their own table.
        sp_ui = (ROOT / "code/client/client_stvef_sp_ui.hpp").read_text(encoding="utf-8")
        scene = sp_ui.split("void CL_STVEFSPUIRenderScene(", 1)[1].split("\n}", 1)[0]
        self.assertLess(scene.index("refdef.rdflags |= RDF_NOFOVCORRECTION;"), scene.index("re.RenderScene( &refdef );"))


if __name__ == "__main__":
    unittest.main()
