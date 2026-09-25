"""Q3Test client paths a retail module or file reaches, run from the source.

1. The menus of 1.06 to 1.08 run `vid_restart`, `snd_restart`, `map` and
   `demo` with EXEC_NOW from their own key handlers (ui1.06 0x4000db34), so
   the engine would unload the module under its own frames. fnTech3 keeps
   those for the next client frame, when no call into the module is on the
   stack: `VM_Free` refuses a module that `VM_Call` has not left, even once
   the module's outermost export has returned.
2. A client game or menu loads MD3 headers through `CM_LoadModel`; offsets
   read from a malformed file must never reach past it.

The production functions are compiled with stubs and run.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


UI_PRELUDE = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <cctype>
#define QDECL
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
enum { MAX_TOKEN_CHARS = 1024, ERR_FATAL = 0 };
enum cbufExec_t { EXEC_NOW, EXEC_INSERT, EXEC_APPEND };
static int Q_stricmp(const char *a, const char *b) {
	for (;; ++a, ++b) {
		const int x = std::tolower(static_cast<unsigned char>(*a)), y = std::tolower(static_cast<unsigned char>(*b));
		if (x != y || !x) return x - y;
	}
}
static void Com_Error(int, const char *) { std::exit(3); }
struct { int depth = 0; std::vector<std::string> deferred; } q3ui;
// VM_Call's view: how deep the engine is inside the module.
static int callLevel;
static std::vector<std::string> executed, buffered;
static std::vector<int> levels;
static void Cmd_ExecuteString(const char *text) { executed.push_back(text); levels.push_back(callLevel); }
static void Cbuf_ExecuteText(cbufExec_t, const char *text) { buffered.push_back(text); }
'''

UI_CHECKS = r'''
// VM_Call( uivm, ... ) reaching an export through CL_Q3TestUIEntry.
template<class F> static void Export(F body) { ++callLevel; Q3TestUICall(body); --callLevel; }
int main() {
	// The video menu's apply: its reload waits, a query runs at once.
	Export([] {
		Q3UI_ExecuteText(EXEC_NOW, "vid_restart\n");
		Q3UI_ExecuteText(EXEC_NOW, "localservers");
		return std::intptr_t(0);
	});
	CHECK(executed == std::vector<std::string>{"localservers"} && levels == std::vector<int>{1});
	CHECK(q3ui.depth == 0 && q3ui.deferred.size() == 1);
	// Nothing runs when the export returns, only at the next client frame.
	CL_Q3TestUIFrame();
	CHECK(executed.size() == 2 && executed[1] == "vid_restart\n" && levels[1] == 0 && q3ui.deferred.empty());
	// A nested export's reloads wait too, in order, whatever their case.
	executed.clear(); levels.clear();
	Export([] {
		Q3UI_ExecuteText(EXEC_NOW, "  SND_RESTART");
		Export([] { Q3UI_ExecuteText(EXEC_NOW, "map q3test1"); return std::intptr_t(0); });
		Q3UI_ExecuteText(EXEC_NOW, "demo q3demo1");
		Q3UI_ExecuteText(EXEC_NOW, "cvar_restart");
		return std::intptr_t(0);
	});
	CHECK(executed.empty());
	CL_Q3TestUIFrame();
	CHECK((executed == std::vector<std::string>{"  SND_RESTART", "map q3test1", "demo q3demo1", "cvar_restart"}));
	for (int level : levels) CHECK(level == 0);
	// Other commands keep their mode; appended ones go to the buffer.
	executed.clear(); levels.clear();
	Export([] {
		Q3UI_ExecuteText(EXEC_NOW, "mapname");
		Q3UI_ExecuteText(EXEC_APPEND, "vid_restart\n");
		return std::intptr_t(0);
	});
	CHECK(executed == std::vector<std::string>{"mapname"} && buffered == std::vector<std::string>{"vid_restart\n"});
	CHECK(q3ui.deferred.empty());
	// An error inside a call leaves its depth behind; the next frame clears it.
	q3ui.depth = 2;
	CL_Q3TestUIFrame();
	CHECK(q3ui.depth == 0);
	return 0;
}
'''

MD3_PRELUDE = r'''
#include "code/qcommon/q3test_wire.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cctype>
#define QDECL
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
namespace q3wire = fntech3::q3test::wire;
typedef int clipHandle_t;
enum { ERR_DROP = 1 };
static int Q_stricmp(const char *a, const char *b) {
	for (;; ++a, ++b) {
		const int x = std::tolower(static_cast<unsigned char>(*a)), y = std::tolower(static_cast<unsigned char>(*b));
		if (x != y || !x) return x - y;
	}
}
static void Com_Error(int, const char *, ...) { std::exit(3); }
static int printed;
static void Com_Printf(const char *, ...) { ++printed; }
static std::vector<std::uint8_t> file;
static bool present = true;
static int FS_ReadFile(const char *, void **buffer) {
	if (!present) { *buffer = nullptr; return -1; }
	// An exact-size heap copy, so a read past it is a read past the file.
	auto *copy = static_cast<std::uint8_t *>(std::malloc(file.size()));
	std::memcpy(copy, file.data(), file.size());
	*buffer = copy;
	return static_cast<int>(file.size());
}
static void FS_FreeFile(void *buffer) { std::free(buffer); }
constexpr int Q3TEST_MD3_HANDLE = 0x100;
struct Q3TestMd3 {
	std::string name;
	int numFrames = 0, numTags = 0;
	std::vector<float> bounds;
	std::vector<std::uint8_t> tags;
};
static std::vector<Q3TestMd3> q3md3s;
'''

MD3_CHECKS = r'''
// A version 15 header: frames, tags and their offsets at +76, +80, +92, +96.
static std::vector<std::uint8_t> Header(int frames, int tags, int ofsFrames, int ofsTags, std::size_t size) {
	std::vector<std::uint8_t> data(size, 0);
	std::memcpy(data.data(), "IDP3", 4);
	auto put = [&](int at, int value) { std::memcpy(data.data() + at, &value, 4); };
	put(4, 15); put(76, frames); put(80, tags); put(92, ofsFrames); put(96, ofsTags);
	return data;
}
int main() {
	// A well-formed file: two frames' bounds and one tag each.
	file = Header(2, 1, 108, 108 + 2 * 56, 108 + 2 * 56 + 2 * 112);
	const float mins[6] = {-1, -2, -3, 4, 5, 6};
	std::memcpy(file.data() + 108, mins, sizeof mins);
	CHECK(Q3CG_LoadModel("models/good.md3") == Q3TEST_MD3_HANDLE);
	CHECK(q3md3s.size() == 1 && q3md3s[0].numFrames == 2 && q3md3s[0].bounds[0] == -1 && q3md3s[0].bounds[5] == 6);
	CHECK(q3md3s[0].tags.size() == 2 * 112 && Q3CG_LoadModel("models/good.md3") == Q3TEST_MD3_HANDLE);
	// Offsets past the file, including ones whose sum with a table would wrap.
	const int hostile[][2] = {{0x7ffffff0, 108}, {108, 0x7ffffff0}, {0x7fffffff, 0x7fffffff},
		{-1, 108}, {108, -8}, {300, 108}, {108, 300}};
	int name = 0;
	for (const auto &offsets : hostile) {
		file = Header(2, 1, offsets[0], offsets[1], 332);
		const int before = printed;
		const std::string model = "models/bad" + std::to_string(name++) + ".md3";
		CHECK(Q3CG_LoadModel(model.c_str()) == Q3TEST_MD3_HANDLE && printed == before + 1 && q3md3s.size() == 1);
	}
	// Counts out of range, a wrong version, a short file, a missing one, another extension.
	for (const auto &bad : {Header(0, 1, 108, 108, 332), Header(1025, 0, 108, 108, 400000), Header(1, 17, 108, 164, 4000)}) {
		file = bad;
		CHECK(Q3CG_LoadModel(("models/count" + std::to_string(name++) + ".md3").c_str()) == Q3TEST_MD3_HANDLE);
	}
	file = Header(1, 0, 108, 164, 164);
	file[4] = 14;
	CHECK(Q3CG_LoadModel("models/old.md3") == Q3TEST_MD3_HANDLE);
	file.resize(100);
	CHECK(Q3CG_LoadModel("models/short.md3") == Q3TEST_MD3_HANDLE);
	present = false;
	CHECK(Q3CG_LoadModel("models/missing.md3") == Q3TEST_MD3_HANDLE && Q3CG_LoadModel("models/x.mdc") == Q3TEST_MD3_HANDLE);
	CHECK(q3md3s.size() == 1);
	return 0;
}
'''


class Q3TestClientTests(unittest.TestCase):
    def execute(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='q3test-client-', dir=scratch) as folder:
            folder = Path(folder)
            cpp, exe = folder / 'probe.cpp', folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_menu_reloads_wait_until_no_module_call_is_on_the_stack(self):
        ui = (ROOT / 'code/client/cl_q3test_ui.cpp').read_text(encoding='utf-8')
        pieces = [function(ui, signature) for signature in (
            'bool Q3TestUIReloads( const char *text ) {', 'void QDECL Q3UI_ExecuteText( int when, const char *text ) {',
            'template<class F> intptr_t Q3TestUICall( F &&call ) {', 'void CL_Q3TestUIFrame( void ) {')]
        self.execute(UI_PRELUDE + '\n'.join(pieces) + UI_CHECKS)
        # The frame hook runs first in every client frame, outside any module.
        main = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        frame = function(main, 'void CL_Frame( int msec, int realMsec ) {')
        self.assertLess(frame.index('CL_Q3TestUIFrame();'), frame.index('CL_UpdateRetailVideoBridgeOwnership();'))

    def test_md3_offsets_never_reach_past_the_file(self):
        cgame = (ROOT / 'code/client/cl_q3test_cgame.cpp').read_text(encoding='utf-8')
        self.execute(MD3_PRELUDE + function(cgame, 'clipHandle_t QDECL Q3CG_LoadModel( const char *name ) {')
                     + MD3_CHECKS)


if __name__ == '__main__':
    unittest.main()
