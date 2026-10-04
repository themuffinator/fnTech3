/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include "../code/compat/jka_font_format.hpp"

using qhandle_t = int;
struct vm_t { std::uintptr_t begin; std::size_t length; };
inline constexpr int ERR_DROP = 1;
#define S_COLOR_YELLOW ""
struct Drop : std::runtime_error { using std::runtime_error::runtime_error; };
void RaiseDrop(const char *message) { throw Drop(message); }
void (*volatile taskDropHandler)(const char *) = RaiseDrop;
void Com_Error(int, const char *message, ...) { taskDropHandler(message); }
void Com_Printf(const char *, ...) {}
void *VM_ExplicitArgPtr(vm_t *, intptr_t address) { return reinterpret_cast<void *>(address); }
void Bounds(vm_t *vm, intptr_t address, std::size_t size) {
    const auto pointer = static_cast<std::uintptr_t>(address);
    if (vm && (pointer < vm->begin || pointer-vm->begin > vm->length ||
        size > vm->length-(pointer-vm->begin))) throw Drop("bounds");
}
#define VM_CHECKBOUNDS(vm, address, size) Bounds(vm, address, size)
float ArgumentFloat(const intptr_t *args, unsigned index) {
    const auto bits = static_cast<std::uint32_t>(args[index]);
    float value; std::memcpy(&value, &bits, sizeof(value)); return value;
}
#define VMF(index) ArgumentFloat(args, index)
int Q_stricmp(const char *a, const char *b) {
    const auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c+32 : c; };
    for (; *a && lower(*a) == lower(*b); ++a, ++b) {}
    return lower(*a)-lower(*b);
}
int Sys_Milliseconds() { return 0; }
int ColorIndex(char c) { return (c-'0')&7; }
const float g_color_table[8][4]{{0,0,0,1}, {1,0,0,1}, {0,1,0,1}, {1,1,0,1},
    {0,0,1,1}, {0,1,1,1}, {1,0,1,1}, {1,1,1,1}};
std::vector<unsigned char> fixture;
int reads = 0, registrations = 0, colors = 0;
int FS_ReadFile(const char *path, void **bytes) {
    ++reads; *bytes = nullptr;
    if (std::strcmp(path, "fonts/test.fontdat")) return -1;
    auto *copy = new unsigned char[fixture.size()];
    std::memcpy(copy, fixture.data(), fixture.size()); *bytes = copy;
    return static_cast<int>(fixture.size());
}
void FS_FreeFile(void *bytes) { delete[] static_cast<unsigned char *>(bytes); }
struct Renderer {
    qhandle_t RegisterShaderNoMip(const char *) { ++registrations; return 77; }
    void SetColor(const float *) { ++colors; }
} re;

#include "../code/client/client_jka_font.hpp"

#define CHECK(test) do { if (!(test)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); return __LINE__; } } while (false)
void Word(unsigned at, unsigned value, unsigned width) {
    for (unsigned n = 0; n < width; ++n) fixture[at+n] = static_cast<unsigned char>(value >> (n*8));
}
void Float(intptr_t *args, unsigned index, float value) {
    std::uint32_t bits; std::memcpy(&bits, &value, sizeof(value)); args[index] = bits;
}
template<class Action> bool Refuses(Action action) {
    try { action(); } catch (const Drop &) { return true; }
    return false;
}
int main() {
    fixture.resize(fntech3::jka::font::FileBytes);
    Word(7168, 14, 2);
    const unsigned glyph = 'A'*fntech3::jka::font::GlyphBytes;
    Word(glyph, 8, 2); Word(glyph+2, 12, 2); Word(glyph+4, 10, 2);
    Word(glyph+6, 0xffff, 2); Word(glyph+8, 11, 4);
    Word(glyph+20, 0x3f000000, 4); Word(glyph+24, 0x3f800000, 4);
    CL_JKAFontReset();
    CHECK(CL_JKARegisterFont("test") == 1 && reads == 1 && registrations == 1);
    CHECK(CL_JKARegisterFont("FONTS/TEST") == 1 && reads == 1 && registrations == 2);
    CHECK(CL_JKARegisterFont("missing") == 0 && reads == 2);
    CHECK(Refuses([] { CL_JKARegisterFont("../test"); }));
    const char text[]{"A^2AA\nA"};
    vm_t vm{reinterpret_cast<std::uintptr_t>(text), sizeof(text)};
    CHECK(CL_JKAFontText(&vm, reinterpret_cast<intptr_t>(text)) == text);
    CHECK(Refuses([&] { CL_JKAFontText(&vm, reinterpret_cast<intptr_t>(text)-1); }));
    char unfinished[64]; std::memset(unfinished, 'A', sizeof(unfinished));
    vm_t bad{reinterpret_cast<std::uintptr_t>(unfinished), sizeof(unfinished)};
    CHECK(Refuses([&] { CL_JKAFontText(&bad, reinterpret_cast<intptr_t>(unfinished), sizeof(unfinished)); }));
    const auto *font = CL_JKAFindFont(1); CHECK(font && font->metrics.height == 14);
    CHECK(!CL_JKAFindFont(0) && !CL_JKAFindFont(2));
    struct Drawn { float x, y, width, height; int shader; };
    std::vector<Drawn> drawn;
    const auto draw = [&](float x, float y, float w, float h, const auto &record, int shader) {
        if (record.s2 != .5f || record.t2 != 1.f) throw Drop("wrong UV");
        drawn.push_back({x,y,w,h,shader});
    };
    CL_JKADrawFont(*font, text, 20, 30, nullptr, 0, -1, .5f, draw);
    CHECK(drawn.size() == 4);
    CHECK(drawn[0].x == 19.5f && drawn[0].y == 31.f && drawn[0].width == 4 && drawn[0].height == 6);
    CHECK(drawn[1].x == 24.5f && drawn[3].x == 19.5f && drawn[3].y == 38.f && drawn[0].shader == 77);
    drawn.clear();
    CL_JKADrawFont(*font, "AA", 0, 0, nullptr, 0, 5, .5f, draw);
    CHECK(drawn.size() == 1); // exact advance fits; next glyph is clipped
    CHECK(Refuses([&] { CL_JKADrawFont(*font, "A", 0, 0, nullptr, 0x01000000u, -1, 1, draw); }));
    CHECK(Refuses([&] { CL_JKADrawFont(*font, "A", 0, 0, nullptr, 0, -2, 1, draw); }));
    volatile std::uint32_t invalidEncoding = 0x7fc00000u;
    const std::uint32_t invalidBits = invalidEncoding;
    float invalid; std::memcpy(&invalid, &invalidBits, sizeof(invalid));
    CHECK(Refuses([&] { CL_JKAFontScale(invalid); }));
    const float malformedColor[]{1, invalid, 1, 1};
    CHECK(Refuses([&] { CL_JKADrawFont(*font, "A", 0, 0, malformedColor, 0, -1, 1, draw); }));
    intptr_t args[8]{58, reinterpret_cast<intptr_t>(text), 1};
    Float(args, 3, .5f); intptr_t result = -1;
    CHECK(CL_JKAFontCall(&vm, args, result, 57, draw) && result == 15);
    args[0] = 59; CHECK(CL_JKAFontCall(&vm, args, result, 57, draw) && result == 4);
    args[0] = 60; args[1] = 1; Float(args, 2, .5f);
    CHECK(CL_JKAFontCall(nullptr, args, result, 57, draw) && result == 7);
    args[0] = 56; CHECK(!CL_JKAFontCall(&vm, args, result, 57, draw));
    CL_JKAFontReset(); CHECK(!CL_JKAFindFont(1));
    return 0;
}
