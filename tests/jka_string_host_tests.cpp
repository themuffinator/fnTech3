/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
using qboolean = int;
enum { qfalse, qtrue, ERR_DROP = 1, CVAR_ARCHIVE = 1, CVAR_NORESTART = 1024 };
struct vm_t { std::uintptr_t start; std::size_t size; };
struct Drop : std::runtime_error { using std::runtime_error::runtime_error; };
void RaiseDrop(const char *format) { throw Drop(format); }
void (*volatile taskDropHandler)(const char *) = RaiseDrop;
void Com_Error(int, const char *format, ...) { taskDropHandler(format); }
void *VM_ExplicitArgPtr(vm_t *, intptr_t address) { return reinterpret_cast<void *>(address); }
void Bounds(vm_t *vm, intptr_t address, std::size_t count) {
    const auto offset = static_cast<std::uintptr_t>(address)-vm->start;
    if (offset > vm->size || count > vm->size-offset) throw Drop("bounds");
}
#define VM_CHECKBOUNDS(vm, address, count) Bounds(vm, address, count)
std::string taskSelectedLanguage = "english";
int reads = 0, lists = 0;
void Cvar_Get(const char *name, const char *value, int flags) {
    if (std::strcmp(name, "se_language") || std::strcmp(value, "english") || flags != 1025) throw Drop("cvar");
}
void Cvar_VariableStringBuffer(const char *, char *output, int size) {
    std::snprintf(output, size, "%s", taskSelectedLanguage.c_str());
}
char **FS_ListFiles(const char *directory, const char *extension, int *count) {
    ++lists;
    const std::vector<std::string> entries = !std::strcmp(extension, "/") ? std::vector<std::string>{"french/", "../bad/"}
        : !std::strcmp(directory, "strings/french") ? std::vector<std::string>{"menu.str"}
        : std::vector<std::string>{"English/Menu.str", "english/menu.str", "French/menu.str", "../bad.str", "invalid/a/b.str"};
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
    const auto data = malformed ? std::string("VERSION 9\nENDMARKER\n")
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
    return 0;
}
