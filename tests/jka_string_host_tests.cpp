/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include <cstdint>
#include <csetjmp>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
static std::size_t liveAllocations = 0;
void *operator new(std::size_t size) {
    if (void *memory = std::malloc(size ? size : 1)) { ++liveAllocations; return memory; }
    throw std::bad_alloc();
}
void operator delete(void *memory) noexcept {
    if (memory) { --liveAllocations; std::free(memory); }
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void *memory) noexcept { ::operator delete(memory); }
void operator delete(void *memory, std::size_t) noexcept { ::operator delete(memory); }
void operator delete[](void *memory, std::size_t) noexcept { ::operator delete(memory); }
using qboolean = int;
enum { qfalse, qtrue, ERR_DROP = 1, CVAR_ARCHIVE = 1, CVAR_NORESTART = 1024 };
struct vm_t { std::uintptr_t start; std::size_t size; bool overflowProbe = false; int checks = 0; };
struct Drop : std::runtime_error { using std::runtime_error::runtime_error; };
void RaiseDrop(const char *format) { throw Drop(format); }
void (*volatile taskDropHandler)(const char *) = RaiseDrop;
void Com_Error(int, const char *format, ...) { taskDropHandler(format); }
static char overflowByte = 'x';
void *VM_ExplicitArgPtr(vm_t *vm, intptr_t address) {
    return vm->overflowProbe ? &overflowByte : reinterpret_cast<void *>(address);
}
void Bounds(vm_t *vm, intptr_t address, std::size_t count) {
    if (vm->overflowProbe) {
        ++vm->checks;
        if (address != (std::numeric_limits<intptr_t>::max)() || count != 1) throw Drop("overflow");
        return;
    }
    const auto offset = static_cast<std::uintptr_t>(address)-vm->start;
    if (offset > vm->size || count > vm->size-offset) throw Drop("bounds");
}
#define VM_CHECKBOUNDS(vm, address, count) Bounds(vm, address, count)
std::string taskSelectedLanguage = "english";
int reads = 0, lists = 0;
int discoveryMode = 0;
bool malformedSecond = false;
void Cvar_Get(const char *name, const char *value, int flags) {
    if (std::strcmp(name, "se_language") || std::strcmp(value, "english") || flags != 1025) throw Drop("cvar");
}
void Cvar_VariableStringBuffer(const char *, char *output, int size) {
    std::snprintf(output, size, "%s", taskSelectedLanguage.c_str());
}
char **FS_ListFiles(const char *directory, const char *extension, int *count) {
    ++lists;
    std::vector<std::string> entries = !std::strcmp(extension, "/") ? std::vector<std::string>{"french/", "../bad/"}
        : !std::strcmp(directory, "strings/french") ? std::vector<std::string>{"menu.str"}
        : std::vector<std::string>{"English/Menu.str", "english/menu.str", "French/menu.str", "../bad.str", "invalid/a/b.str"};
    if (discoveryMode) {
        entries.clear();
        if (!std::strcmp(extension, ".str")) {
            if (discoveryMode == 1) entries = {"french/a_long_package_name.str"};
            if (discoveryMode == 3)
                for (int n = 0; n < 4097; ++n) entries.push_back("english/package_" + std::to_string(n) + ".str");
            if (discoveryMode == 4) entries = {"english/menu.str", "unsupported_language_with_a_long_name/menu.str"};
        }
    } else if (malformedSecond && !std::strcmp(directory, "strings") && !std::strcmp(extension, ".str")) {
        entries.push_back("english/zzbroken.str");
    }
    auto **result = new char *[entries.size()+1];
    for (std::size_t n = 0; n < entries.size(); ++n) {
        result[n] = new char[entries[n].size()+1];
        std::memcpy(result[n], entries[n].c_str(), entries[n].size()+1);
    }
    result[entries.size()] = nullptr; *count = static_cast<int>(entries.size()); return result;
}
void FS_FreeFileList(char **files) {
    for (unsigned n = 0; files[n]; ++n) delete[] files[n];
    delete[] files;
}
bool malformed = false;
int FS_ReadFile(const char *path, void **output) {
    ++reads;
    const std::string english = "VERSION 1\nREFERENCE GREETING\nLANG_ENGLISH \"Hello\"\nENDMARKER\n";
    const std::string french = "VERSION 1\nREFERENCE GREETING\nLANG_ENGLISH \"Hello\"\nLANG_FRENCH \"Bonjour\"\nENDMARKER\n";
    const auto data = malformed || (malformedSecond && !std::strcmp(path, "strings/english/zzbroken.str"))
        ? std::string("VERSION 9\nENDMARKER\n")
        : !std::strcmp(path, "strings/english/menu.str") ? english : french;
    auto *bytes = new char[data.size()]; std::memcpy(bytes, data.data(), data.size()); *output = bytes;
    return static_cast<int>(data.size());
}
void FS_FreeFile(void *bytes) { delete[] static_cast<char *>(bytes); }
#include "../code/client/client_jka_strings.hpp"

#define CHECK(test) do { if (!(test)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); return __LINE__; } } while (false)
template<class Action> bool Refuses(Action action) {
    try { action(); } catch (const Drop &) { return true; } return false;
}
static std::jmp_buf failureJump;
void LongjmpDrop(const char *) { std::longjmp(failureJump, 1); }
#ifdef _MSC_VER
// Deliberately model the engine ABI, with no owning object in this frame.
#pragma warning(push)
#pragma warning(disable: 4611)
#endif
template<class Action> bool LongjmpRefuses(Action action) {
    static_assert(std::is_trivially_destructible_v<Action>);
    const auto before = liveAllocations;
    taskDropHandler = LongjmpDrop;
    if (!setjmp(failureJump)) {
        action();
        taskDropHandler = RaiseDrop;
        return false;
    }
    taskDropHandler = RaiseDrop;
    return liveAllocations == before;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
int main() {
    CL_JKAStringReset(); CHECK(CL_JKAStringCount() == 2 && lists == 3);
    CHECK(CL_JKAStringCount() == 2 && lists == 3);
    struct Memory { unsigned guard; char output[128]; char reference[256]; unsigned tail; } memory{};
    std::memset(&memory, 0xa5, sizeof(memory));
    const char key[]{"menu_greeting"}; std::memcpy(memory.reference, key, sizeof(key));
    vm_t vm{reinterpret_cast<std::uintptr_t>(&memory), sizeof(memory)};
    const auto output = reinterpret_cast<intptr_t>(memory.output), reference = reinterpret_cast<intptr_t>(memory.reference);
    CL_JKAStringLanguage(&vm, 0, output); CHECK(!std::strcmp(memory.output, "english"));
    CL_JKAStringLanguage(&vm, 1, output); CHECK(!std::strcmp(memory.output, "french"));
    CL_JKAStringLanguage(&vm, -1, output); CHECK(memory.output[0] == 0);
    CHECK(CL_JKAStringText(&vm, reference, output, 4, false) == qtrue && !std::strcmp(memory.output, "Hel"));
    CHECK(reads == 1);
    taskSelectedLanguage = "FRENCH";
    CHECK(CL_JKAStringText(&vm, reference, output, 128, true) == qtrue && !std::strcmp(memory.output, "Bonjour"));
    CHECK(reads == 2);
    taskSelectedLanguage = "english";
    CHECK(CL_JKAStringText(&vm, reference, output, 128, false) == qtrue && !std::strcmp(memory.output, "Hello"));
    CHECK(reads == 2);
    CHECK(CL_JKAStringIsAsian() == qfalse && CL_JKAStringUsesSpaces() == qtrue);
    memory.reference[0] = static_cast<char>(0xff);
    CHECK(CL_JKAStringReadChar(&vm, reference, output, output+4) == 255);
    std::int32_t advance = 0, punctuation = 1;
    std::memcpy(&advance, memory.output, sizeof(advance));
    std::memcpy(&punctuation, memory.output+4, sizeof(punctuation));
    CHECK(advance == 1 && punctuation == 0);
    CHECK(Refuses([&] { CL_JKAStringReadChar(&vm, reference, 0, 0); }));
    CHECK(Refuses([&] { CL_JKAStringReadChar(&vm, reference, output, reinterpret_cast<intptr_t>(&memory.tail)+1); }));
    memory.reference[0] = 0;
    CHECK(CL_JKAStringText(&vm, reference, output, 128, false) == qtrue && memory.output[0] == 0);
    CHECK(CL_JKAStringText(&vm, reference, output, 128, true) == qfalse && !std::strcmp(memory.output, "??"));
    CHECK(memory.guard == 0xa5a5a5a5u && memory.tail == 0xa5a5a5a5u);
    CHECK(Refuses([&] { CL_JKAStringText(&vm, reference, output, 0, false); }));
    CHECK(Refuses([&] { CL_JKAStringText(&vm, reference, output, 65537, false); }));
    CHECK(Refuses([&] { CL_JKAStringLanguage(&vm, 0, reinterpret_cast<intptr_t>(&memory.tail)); }));
    taskSelectedLanguage = "../english"; CHECK(Refuses([&] { CL_JKAStringText(&vm, reference, output, 128, false); }));
    taskSelectedLanguage = "missing"; CHECK(Refuses([&] { CL_JKAStringText(&vm, reference, output, 128, false); }));
    taskSelectedLanguage = "english"; malformed = true; CL_JKAStringReset();
    CHECK(Refuses([&] { CL_JKAStringText(&vm, reference, output, 128, false); }));
    CHECK(fntech3::jka::hoststrings::Storage().cache.empty());
    // These actual longjmps must release every temporary C++ allocation,
    // including discovery lists and tables parsed before a later bad file.
    for (int mode : {1, 2, 3}) {
        discoveryMode = mode; CL_JKAStringReset();
        CHECK(LongjmpRefuses([] { CL_JKAStringCount(); }));
        const auto &state = fntech3::jka::hoststrings::Storage();
        CHECK(!state.discovered && state.paths.empty() && state.languages.empty() && state.cache.empty());
    }
    discoveryMode = 0; malformed = false; malformedSecond = true;
    CL_JKAStringReset(); CHECK(CL_JKAStringCount() == 2);
    taskSelectedLanguage = "english";
    CHECK(LongjmpRefuses([&] { CL_JKAStringText(&vm, reference, output, 128, false); }));
    CHECK(fntech3::jka::hoststrings::Storage().cache.empty());
    taskSelectedLanguage = "invalid_language_with_a_long_name/";
    CHECK(LongjmpRefuses([] { CL_JKAStringIsAsian(); }));
    taskSelectedLanguage = "missing_language_with_a_long_name";
    CHECK(LongjmpRefuses([] { CL_JKAStringUsesSpaces(); }));
    discoveryMode = 4; CL_JKAStringReset(); CHECK(CL_JKAStringCount() == 2);
    taskSelectedLanguage = "unsupported_language_with_a_long_name";
    CHECK(LongjmpRefuses([] { CL_JKAStringIsAsian(); }));
    // A synthetic mapping at INTPTR_MAX permits byte zero only. Byte one's
    // range must be rejected before signed address arithmetic or VM access.
    vm.overflowProbe = true;
    CHECK(Refuses([&] { fntech3::jka::hoststrings::Text(&vm, (std::numeric_limits<intptr_t>::max)()); }));
    CHECK(vm.checks == 1);
    CL_JKAStringReset();
    return 0;
}
