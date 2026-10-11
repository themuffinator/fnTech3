"""Check RTCW UI/parser/font/renderer records using offline synthetic backends."""
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
    return source[start:source.index("\n}", start) + 2]


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/compat/rtcw_client_adapter.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstring>
#include <initializer_list>
void QDECL Com_Error(errorParm_t, const char *, ...) { throw 7; }
static uiClientState_t currentState;
static void GetClientState(uiClientState_t *state) { std::memcpy(state, &currentState, sizeof(*state)); }
static fontInfo_t currentFont;
static pc_token_t currentToken;
static int fontCalls, tokenCalls, tokenResult = 1;
static bool failFont, failToken;
static constexpr int MAX_SOURCEFILES = 64;
struct Script { int line; } sourceScript;
struct source_t { char filename[1024]; Script *scriptstack; } currentSource;
static source_t *sourceFiles[MAX_SOURCEFILES];
static int sourceCalls, sourceFailure;
int PC_SourceFileAndLine(int, char *, int *);
static int SourceFileAndLine(int handle, char *filename, int *line) {
    ++sourceCalls;
    const int result = PC_SourceFileAndLine(handle, filename, line);
    if (sourceFailure == 1) return 0;
    if (sourceFailure == 2) throw 13;
    if (sourceFailure == 3) std::memset(filename, 'x', 1024);
    return result;
}
static void RegisterFont(const char *name, int pointSize, fontInfo_t *font) {
    assert(!std::strcmp(name, "fonts/test.ttf") && pointSize == 24);
    ++fontCalls; *font = currentFont;
    if (failFont) throw 9;
}
static int ReadToken(int handle, pc_token_t *token) {
    assert(handle == 17); ++tokenCalls; *token = currentToken;
    if (failToken) throw 11;
    return tokenResult;
}
static struct { void (*RegisterFont)(const char *, int, fontInfo_t *); } re{RegisterFont};
static struct Botlib {
    int (*PC_ReadTokenHandle)(int, pc_token_t *);
    int (*PC_SourceFileAndLine)(int, char *, int *);
} botlib{ReadToken, SourceFileAndLine};
static Botlib *botlib_export = &botlib;
'''

CHECKS = r'''
using fntech3::rtcw::client::FromInternal;
template<class T> struct Guarded {
    uint32_t before = 0x11223344;
    T value;
    uint32_t after = 0xaabbccdd;
    void Check() const { assert(before == 0x11223344 && after == 0xaabbccdd); }
};
template<class E> static void Invalid(E &value, uint32_t bits) {
    static_assert(sizeof(E) == 4); std::memcpy(&value, &bits, 4);
}
template<std::size_t N> static void BoundedText(const char (&text)[N], char expected) {
    for (std::size_t i = 0; i + 1 < N; ++i) assert(text[i] == expected);
    assert(text[N - 1] == 0);
}
static void Renderer() {
    glconfig_t source{};
    std::memset(source.renderer_string, 'r', sizeof(source.renderer_string));
    std::memset(source.vendor_string, 'v', sizeof(source.vendor_string));
    std::memset(source.version_string, 'n', sizeof(source.version_string));
    std::memset(source.extensions_string, 'e', sizeof(source.extensions_string));
    source.maxTextureSize = 16384; source.numTextureUnits = 16;
    source.colorBits = 24; source.depthBits = 32; source.stencilBits = 8;
    source.driverType = GLDRV_VOODOO; source.hardwareType = GLHW_PERMEDIA2;
    source.deviceSupportsGamma = qtrue; source.textureCompression = TC_S3TC_ARB;
    source.textureEnvAddAvailable = qtrue;
    source.vidWidth = 2560; source.vidHeight = 1440; source.windowAspect = 16.0f / 9.0f;
    source.displayFrequency = 144; source.isFullscreen = qtrue; source.stereoEnabled = qtrue;
    source.smpActive = qtrue;
    Guarded<fntech3_rtcw_glconfig_t> record;
    std::memset(&record.value, 0xa5, sizeof(record.value));
    CL_CopyRTCWGlconfig(&record.value, &source); record.Check();
    BoundedText(record.value.renderer_string, 'r');
    BoundedText(record.value.vendor_string, 'v');
    BoundedText(record.value.version_string, 'n');
    BoundedText(record.value.extensions_string, 'e');
    assert(record.value.maxTextureSize == 16384 && record.value.maxActiveTextures == 16);
    assert(record.value.colorBits == 24 && record.value.depthBits == 32 && record.value.stencilBits == 8);
    assert(record.value.driverType == 2 && record.value.hardwareType == 4);
    assert(record.value.deviceSupportsGamma == 1 && record.value.textureCompression == 2);
    assert(record.value.textureEnvAddAvailable == 1 && record.value.isFullscreen == 1);
    assert(record.value.stereoEnabled == 1 && record.value.smpActive == 1);
    assert(record.value.vidWidth == 2560 && record.value.vidHeight == 1440);
    volatile float expectedAspect = 16.0f / 9.0f;
    assert(record.value.windowAspect == expectedAspect && record.value.displayFrequency == 144);
    assert(record.value.anisotropicAvailable == 0 && record.value.maxAnisotropy == 0);
    assert(record.value.textureFilterAnisotropicAvailable == 0 && record.value.NVFogAvailable == 0);
    assert(record.value.NVFogMode == 0 && record.value.ATIMaxTruformTess == 0);
    assert(record.value.ATINormalMode == 0 && record.value.ATIPointMode == 0);
    const auto saved = record;
    for (uint32_t bits : {2u, UINT_MAX}) {
        for (qboolean glconfig_t::*field : {&glconfig_t::deviceSupportsGamma, &glconfig_t::textureEnvAddAvailable,
                &glconfig_t::isFullscreen, &glconfig_t::stereoEnabled, &glconfig_t::smpActive}) {
            Invalid(source.*field, bits);
            assert(!FromInternal(source, record.value));
            assert(!std::memcmp(&record, &saved, sizeof(record)));
            source.*field = qtrue;
        }
    }
    Invalid(source.driverType, 3); assert(!FromInternal(source, record.value)); source.driverType = GLDRV_ICD;
    Invalid(source.hardwareType, 5); assert(!FromInternal(source, record.value)); source.hardwareType = GLHW_GENERIC;
    Invalid(source.textureCompression, 3); bool threw = false;
    try { CL_CopyRTCWGlconfig(&record.value, &source); } catch (int) { threw = true; }
    assert(threw && !std::memcmp(&record, &saved, sizeof(record)));
    source.textureCompression = TC_NONE;
    for (int which : {0, 1}) {
        threw = false;
        try { CL_CopyRTCWGlconfig(which ? nullptr : &record.value, which ? &source : nullptr); }
        catch (int) { threw = true; }
        assert(threw && !std::memcmp(&record, &saved, sizeof(record)));
    }
    source = {};
    CL_CopyRTCWGlconfig(&record.value, &source); record.Check();
    const fntech3_rtcw_glconfig_t zero{};
    assert(!std::memcmp(&record.value, &zero, sizeof(zero)));
}
static void ClientState() {
    currentState = {};
    currentState.connectPacketCount = INT_MAX; currentState.clientNum = 127;
    std::memset(currentState.servername, 's', sizeof(currentState.servername));
    std::memset(currentState.updateInfoString, 'u', sizeof(currentState.updateInfoString));
    std::memset(currentState.messageString, 'm', sizeof(currentState.messageString));
    Guarded<fntech3_rtcw_uiClientState_t> record;
    for (int state = 0; state <= 9; ++state) {
        currentState.connState = static_cast<connstate_t>(state);
        GetRTCWClientState(&record.value); record.Check();
        assert(record.value.connState == state && record.value.connectPacketCount == INT_MAX);
        assert(record.value.clientNum == 127);
        BoundedText(record.value.servername, 's'); BoundedText(record.value.updateInfoString, 'u');
        BoundedText(record.value.messageString, 'm');
    }
    const auto saved = record;
    for (uint32_t bits : {10u, UINT_MAX}) {
        Invalid(currentState.connState, bits); bool threw = false;
        try { GetRTCWClientState(&record.value); } catch (int) { threw = true; }
        assert(threw && !std::memcmp(&record, &saved, sizeof(record)));
    }
    currentState = {}; bool threw = false;
    try { GetRTCWClientState(nullptr); } catch (int) { threw = true; }
    assert(threw);
}
static void TokenAndFont() {
    Guarded<fntech3_rtcw_pc_token_t> token;
    currentToken.type = 3; currentToken.subtype = 0x850;
    currentToken.intvalue = INT_MIN; currentToken.floatvalue = -0.25f;
    std::memset(currentToken.string, 't', sizeof(currentToken.string));
    assert(CL_RTCWReadToken(17, &token.value) == 1); token.Check();
    assert(token.value.type == 3 && token.value.subtype == 0x850);
    assert(token.value.intvalue == INT_MIN && token.value.floatvalue == -0.25f);
    BoundedText(token.value.string, 't');
    const auto savedToken = token;
    for (int type = 1; type <= 5; ++type) {
        currentToken.type = type;
        assert(CL_RTCWReadToken(17, &token.value) == 1 && token.value.type == type);
        token = savedToken;
    }
    for (int type : {0, -1, 6, INT_MAX}) {
        currentToken.type = type; bool threw = false;
        try { CL_RTCWReadToken(17, &token.value); } catch (int) { threw = true; }
        assert(threw && !std::memcmp(&token, &savedToken, sizeof(token)));
    }
    currentToken = {}; tokenResult = 0;
    assert(CL_RTCWReadToken(17, &token.value) == 0);
    assert(!std::memcmp(&token, &savedToken, sizeof(token)));
    failToken = true; bool threw = false;
    try { CL_RTCWReadToken(17, &token.value); } catch (int) { threw = true; }
    assert(threw && !std::memcmp(&token, &savedToken, sizeof(token)));
    failToken = false;
    const int calls = tokenCalls; threw = false;
    try { CL_RTCWReadToken(17, nullptr); } catch (int) { threw = true; }
    assert(threw && tokenCalls == calls);

    for (int i = 0; i < 256; ++i) {
        auto &glyph = currentFont.glyphs[i];
        glyph.height = i + 1; glyph.top = -i; glyph.bottom = i - 3; glyph.pitch = 2048 - i;
        glyph.xSkip = i * 2; glyph.imageWidth = i + 4; glyph.imageHeight = i + 5;
        glyph.s = i * .125f; glyph.t = -i * .25f; glyph.s2 = i * .5f; glyph.t2 = i + .5f;
        glyph.glyph = i + 1234;
        std::memset(glyph.shaderName, 'g', sizeof(glyph.shaderName));
    }
    currentFont.glyphScale = 1.125f; std::memset(currentFont.name, 'f', sizeof(currentFont.name));
    Guarded<fntech3_rtcw_fontInfo_t> font;
    CL_RTCWRegisterFont("fonts/test.ttf", 24, &font.value); font.Check();
    assert(fontCalls == 1 && font.value.glyphScale == 1.125f); BoundedText(font.value.name, 'f');
    for (int i = 0; i < 256; ++i) {
        const auto &g = font.value.glyphs[i];
        assert(g.height == i + 1 && g.top == -i && g.bottom == i - 3 && g.pitch == 2048 - i);
        assert(g.xSkip == i * 2 && g.imageWidth == i + 4 && g.imageHeight == i + 5);
        assert(g.s == i * .125f && g.t == -i * .25f && g.s2 == i * .5f && g.t2 == i + .5f);
        assert(g.glyph == i + 1234); BoundedText(g.shaderName, 'g');
    }
    const auto savedFont = font;
    currentFont = {}; failFont = true; threw = false;
    try { CL_RTCWRegisterFont("fonts/test.ttf", 24, &font.value); } catch (int) { threw = true; }
    assert(threw && !std::memcmp(&font, &savedFont, sizeof(font)));
    threw = false;
    try { CL_RTCWRegisterFont("fonts/test.ttf", 24, nullptr); } catch (int) { threw = true; }
    assert(threw && fontCalls == 2);
    failFont = false;
    CL_RTCWRegisterFont("fonts/test.ttf", 24, &font.value); font.Check();
    const fntech3_rtcw_fontInfo_t zero{};
    assert(!std::memcmp(&font.value, &zero, sizeof(zero)));
}
static void ParserLocation() {
    sourceFiles[17] = &currentSource; sourceScript.line = 793;
    currentSource.scriptstack = &sourceScript;
    struct Record { char filename[128]; int line; };
    Guarded<Record> output;
    for (int length : {0, 1, 126, 127, 128, 1023}) {
        std::memset(currentSource.filename, 's', sizeof(currentSource.filename));
        currentSource.filename[length] = 0;
        std::memset(&output.value, 0x5a, sizeof(output.value));
        assert(CL_RTCWSourceFileAndLine(17, output.value.filename, &output.value.line) == 1);
        const int kept = std::min(length, 127); output.Check();
        for (int i = 0; i < kept; ++i) assert(output.value.filename[i] == 's');
        assert(output.value.filename[kept] == 0 && output.value.line == 793);
        for (int i = kept + 1; i < 128; ++i) assert(output.value.filename[i] == 0x5a);
    }
    currentSource.scriptstack = nullptr;
    assert(CL_RTCWSourceFileAndLine(17, output.value.filename, &output.value.line) == 1);
    assert(output.value.line == 0); output.Check();
    const auto saved = output;
    for (int handle : {-1, 0, 1, 16, 18, 64, INT_MAX, INT_MIN}) {
        assert(CL_RTCWSourceFileAndLine(handle, output.value.filename, &output.value.line) == 0);
        assert(!std::memcmp(&saved, &output, sizeof(saved)));
    }
    currentSource.filename[0] = 0; currentSource.scriptstack = &sourceScript;
    for (int failure : {1, 2, 3}) {
        bool threw = false; sourceFailure = failure;
        try { assert(CL_RTCWSourceFileAndLine(17, output.value.filename, &output.value.line) == 0); }
        catch (int) { threw = true; }
        assert(threw == (failure != 1));
        assert(!std::memcmp(&saved, &output, sizeof(saved)));
    }
    sourceFailure = 0;
    for (bool nullName : {false, true}) {
        const int before = sourceCalls; bool threw = false;
        try { CL_RTCWSourceFileAndLine(17, nullName ? nullptr : output.value.filename,
                                    nullName ? &output.value.line : nullptr); }
        catch (int) { threw = true; }
        assert(threw && sourceCalls == before && !std::memcmp(&saved, &output, sizeof(saved)));
    }
}
int main() { Renderer(); ClientState(); TokenAndFont(); ParserLocation(); }
'''


class RTCWClientPODTests(unittest.TestCase):
    def test_frozen_records_and_transactional_transfers(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++17 compiler unavailable")
        cgame = (ROOT / "code/client/cl_cgame.cpp").read_text(encoding="utf-8")
        ui = (ROOT / "code/client/cl_ui.cpp").read_text(encoding="utf-8")
        bodies = "\n".join(function(cgame, name) for name in (
            "void CL_CopyRTCWGlconfig(", "int CL_RTCWReadToken(", "void CL_RTCWRegisterFont(",
            "int CL_RTCWSourceFileAndLine("))
        bodies += "\n" + function(ui, "static void GetRTCWClientState(")
        parser = (ROOT / "code/botlib/l_precomp.cpp").read_text(encoding="utf-8")
        bodies += "\n#define BOTLIB\n" + function(parser, "int PC_SourceFileAndLine(") + "\n#undef BOTLIB\n"
        (ROOT / ".tmp").mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="rtcw-client-pods-", dir=ROOT / ".tmp") as temp:
            folder = Path(temp)
            fixture = folder / "probe.cpp"
            executable = folder / ("probe.exe" if os.name == "nt" else "probe")
            fixture.write_text(PRELUDE + bodies + CHECKS, encoding="utf-8")
            target = compiler_target_args(compiler)
            if os.name == "nt" and not target:
                target = ["-DFNTECH3_Q3_ONLY"]
            elif os.name != "nt":
                target = ["-m32"]
            build = subprocess.run([compiler, *target, "-std=c++17", "-I", str(ROOT),
                str(fixture), "-o", str(executable)], capture_output=True, text=True, timeout=60, cwd=ROOT)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15, cwd=ROOT)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
