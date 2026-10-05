"""Run the production Enemy Territory download-redirect handling.

An ET server may answer a download request with block -1: a URL, the file's
size and flags (retail et.exe 2.60b CL_ParseDownload). The client
acknowledges it, fetches the file over HTTP while it stays connected and
reports "wwwdl done", or declines with "wwwdl fail" so the server sends the
file in blocks. These tests compile the client's redirect reader and its
transfer completion out of cl_parse.cpp and cl_curl.cpp.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if not depth:
                return source[start:i + 1]
    raise AssertionError(signature)


HARNESS = r'''
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#define USE_CLIENT_CURL 1
using qboolean = int;
constexpr qboolean qtrue = 1, qfalse = 0;
constexpr int MAX_STRING_CHARS = 1024, MAX_OSPATH = 256, ERR_DROP = 1;
struct msg_t { std::vector<unsigned char> data; int readcount = 0; int cursize = 0; };
static struct {
    char downloadName[MAX_OSPATH];
    char downloadTempName[MAX_OSPATH + 4];
    char downloadURL[MAX_OSPATH];
    int wolfetWebDownload;
    qboolean downloadRestart;
} clc;
static std::vector<std::string> sent, events;
static bool curlAvailable = true;
static std::string error;
static int downloadSize = -1;
struct ErrorThrown {};
static void Com_Error(int, const char *format, ...) {
    char buffer[2048]; va_list args; va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
    error = buffer; throw ErrorThrown{};
}
static void Com_Printf(const char *, ...) {}
static void Com_DPrintf(const char *, ...) {}
static void Q_strncpyz(char *dest, const char *src, int size) {
    std::strncpy(dest, src, size - 1); dest[size - 1] = 0;
}
static int ReadByte(msg_t *msg) {
    if (msg->readcount >= msg->cursize) { ++msg->readcount; return -1; }
    return msg->data[msg->readcount++];
}
static const char *MSG_ReadString(msg_t *msg) {
    static char text[MAX_STRING_CHARS]; int length = 0;
    for (;;) { const int c = ReadByte(msg); if (c <= 0) break; if (length < MAX_STRING_CHARS - 1) text[length++] = char(c); }
    text[length] = 0; return text;
}
static int MSG_ReadLong(msg_t *msg) {
    int value = 0; for (int i = 0; i < 4; ++i) value |= (ReadByte(msg) & 0xff) << (8 * i); return value;
}
static void CL_AddReliableCommand(const char *text, qboolean) { sent.push_back(text); }
static qboolean CL_cURL_Init() { return curlAvailable; }
static void CL_WolfETBeginWebDownload(const char *name, const char *url) {
    events.push_back(std::string("begin ") + name + " " + url);
    clc.wolfetWebDownload = 1;
}
static void Cvar_SetIntegerValue(const char *, int value) { downloadSize = value; }
static void Cvar_Set(const char *, const char *) {}
static void CL_cURL_Cleanup() { events.push_back("cleanup"); }
static void FS_SV_Rename(const char *from, const char *to) { events.push_back(std::string("rename ") + from + " " + to); }
static void FS_SV_HomeRemove(const char *name) { events.push_back(std::string("remove ") + name); }
static void CL_NextDownload() { events.push_back("next"); }

REDIRECT
FINISH

static msg_t Message(const char *url, int size, int flags, int truncate = 0) {
    msg_t m;
    for (const char *c = url; *c; ++c) m.data.push_back((unsigned char)*c);
    m.data.push_back(0);
    for (int v : {size, flags}) for (int i = 0; i < 4; ++i) m.data.push_back((unsigned char)(v >> (8 * i)));
    m.cursize = int(m.data.size()) - truncate;
    return m;
}
static void Reset() {
    clc = {}; sent.clear(); events.clear(); error.clear(); downloadSize = -1; curlAvailable = true;
    Q_strncpyz(clc.downloadName, "fntech3probe/zz.pk3", sizeof(clc.downloadName));
    Q_strncpyz(clc.downloadTempName, "fntech3probe/zz.pk3.tmp", sizeof(clc.downloadTempName));
    Q_strncpyz(clc.downloadURL, "http://127.0.0.1:1/fntech3probe/zz.pk3", sizeof(clc.downloadURL));
}
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main() {
    // A redirect is acknowledged and fetched under the requested name.
    Reset();
    { msg_t m = Message("http://127.0.0.1:1/fntech3probe/zz.pk3", 32916, 0); CL_WolfETDownloadRedirect(&m);
      CHECK(m.readcount == m.cursize); }
    CHECK(sent == std::vector<std::string>{"wwwdl ack"});
    CHECK(events == std::vector<std::string>{"begin fntech3probe/zz.pk3 http://127.0.0.1:1/fntech3probe/zz.pk3"});
    CHECK(downloadSize == 32916 && clc.wolfetWebDownload == 1);
    // Its repeats are read and set aside.
    sent.clear(); events.clear();
    { msg_t m = Message("http://127.0.0.1:1/fntech3probe/zz.pk3", 32916, 0); CL_WolfETDownloadRedirect(&m);
      CHECK(m.readcount == m.cursize); }
    CHECK(sent.empty() && events.empty());
    // A server allowing the client to leave is still answered while connected.
    Reset();
    { msg_t m = Message("http://x/y.pk3", 10, 1); CL_WolfETDownloadRedirect(&m); }
    CHECK(sent == std::vector<std::string>{"wwwdl ack"} && clc.wolfetWebDownload == 1);
    // Without a web transport, or for an empty file, the client declines.
    for (int size : {0, 10}) {
        Reset(); curlAvailable = size == 0;
        msg_t m = Message("http://x/y.pk3", size, 0); CL_WolfETDownloadRedirect(&m);
        CHECK((sent == std::vector<std::string>{"wwwdl ack", "wwwdl fail"}));
        CHECK(events.empty() && clc.wolfetWebDownload == 2);
        CHECK(!std::strcmp(clc.downloadName, "fntech3probe/zz.pk3"));
    }
    // A plain fallback page ends the connection with its address.
    Reset();
    try { msg_t m = Message("https://example.invalid/files", 0, 2); CL_WolfETDownloadRedirect(&m); CHECK(false); }
    catch (ErrorThrown &) {}
    CHECK(error.find("https://example.invalid/files") != std::string::npos && sent.empty());
    // A truncated redirect is refused before anything is sent.
    for (int cut : {1, 4, 9}) {
        Reset();
        try { msg_t m = Message("http://x/y.pk3", 10, 0, cut); CL_WolfETDownloadRedirect(&m); CHECK(false); }
        catch (ErrorThrown &) {}
        CHECK(error.find("Truncated") != std::string::npos && sent.empty() && events.empty());
    }
    // A fetched file joins the downloads and the server hears it is done.
    Reset(); clc.wolfetWebDownload = 1;
    CL_WolfETFinishWebDownload(true);
    CHECK(sent == std::vector<std::string>{"wwwdl done"});
    CHECK((events == std::vector<std::string>{"rename fntech3probe/zz.pk3.tmp fntech3probe/zz.pk3", "cleanup", "next"}));
    CHECK(clc.downloadRestart && clc.wolfetWebDownload == 0);
    // A failed one is removed, declined, and its name kept for the repeats.
    Reset(); clc.wolfetWebDownload = 1;
    CL_WolfETFinishWebDownload(false);
    CHECK(sent == std::vector<std::string>{"wwwdl fail"});
    CHECK((events == std::vector<std::string>{"cleanup", "remove fntech3probe/zz.pk3.tmp"}));
    CHECK(!clc.downloadRestart && clc.wolfetWebDownload == 2);
    CHECK(!std::strcmp(clc.downloadName, "fntech3probe/zz.pk3"));
    std::printf("ok\n");
    return 0;
}
'''


class WolfETWebDownloadClientTests(unittest.TestCase):
    def test_redirect_answers_and_completion(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        parse = (ROOT / 'code/client/cl_parse.cpp').read_text(encoding='utf-8')
        curl = (ROOT / 'code/client/cl_curl.cpp').read_text(encoding='utf-8')
        redirect = function(parse, 'static void CL_WolfETDownloadRedirect( msg_t *msg ) {')
        finish = function(curl, 'static void CL_WolfETFinishWebDownload( bool fetched )')
        source = HARNESS.replace('REDIRECT', redirect).replace('FINISH', finish)
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='wolfet-web-', dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(source, encoding='utf-8')
            sanitizer = [] if os.name == 'nt' else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O1', '-Wall',
                                    '-D_CRT_SECURE_NO_WARNINGS', *sanitizer, str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_the_parser_and_gamestate_route_through_the_redirect(self):
        parse = (ROOT / 'code/client/cl_parse.cpp').read_text(encoding='utf-8')
        download = function(parse, 'static void CL_ParseDownload( msg_t *msg ) {')
        self.assertIn('if ( Com_GameIsWolfET() && block == 0xffff ) {\n\t\tCL_WolfETDownloadRedirect( msg );', download)
        self.assertNotIn('supports ET UDP downloads only', parse)
        gamestate = function(parse, 'static void CL_ParseGamestate( msg_t *msg ) {')
        self.assertLess(gamestate.index('clc.wolfetWebDownload = 0;'), gamestate.index('CL_ClearState();'))
        curl = (ROOT / 'code/client/cl_curl.cpp').read_text(encoding='utf-8')
        perform = function(curl, 'void CL_cURL_PerformDownload( void )')
        self.assertEqual(perform.count('CL_WolfETFinishWebDownload('), 2)
        main = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        self.assertIn('Cvar_Get( "cl_wwwDownload", "1", CVAR_USERINFO | CVAR_ARCHIVE )', main)


if __name__ == '__main__':
    unittest.main()
