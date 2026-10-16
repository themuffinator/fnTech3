/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jk2_string_package.hpp"
#include <cassert>
#include <csetjmp>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>
using namespace fntech3::jk2::strings;

// Count live C++ allocations so an actual engine-style longjmp cannot hide
// leaked parser/name/path ownership behind the fixture's usual exceptions.
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
static std::jmp_buf failureJump;
static bool useLongjmp = false;

enum { ERR_DROP, CVAR_ARCHIVE = 1, CVAR_NORESTART = 1024 };
using qboolean = int;
constexpr int qfalse = 0, qtrue = 1;
struct cvar_t { int integer; } testLanguage{};
struct vm_t { std::array<char, 1024> bytes{}; bool overflowProbe = false; int checks = 0; };
[[noreturn]] void Com_Error(int, const char *, ...) {
    if (useLongjmp) std::longjmp(failureJump, 1);
    throw std::runtime_error("rejected");
}
#define S_COLOR_YELLOW "^3"
static int printed = 0;
void Com_Printf(const char *, ...) { ++printed; }
void VM_CHECKBOUNDS(vm_t *vm, intptr_t at, std::size_t size) {
    if (vm->overflowProbe) {
        ++vm->checks;
        if (at != (std::numeric_limits<intptr_t>::max)() || size != 1) Com_Error(ERR_DROP, "overflow");
        return;
    }
    if (at < 0 || std::size_t(at) > vm->bytes.size() || size > vm->bytes.size() - std::size_t(at))
        Com_Error(ERR_DROP, "bounds");
}
void *VM_ExplicitArgPtr(vm_t *vm, intptr_t at) {
    if (vm->overflowProbe) return vm->bytes.data();
    return at ? vm->bytes.data() + at : nullptr;
}
cvar_t *Cvar_Get(const char *name, const char *value, int flags) {
    assert(std::string(name) == "sp_language" && std::string(value) == "0");
    assert(flags == (CVAR_ARCHIVE | CVAR_NORESTART));
    return &testLanguage;
}
std::map<std::string, std::string> files;
int loads = 0, releases = 0;
int FS_ReadFile(const char *path, void **data) {
    ++loads;
    const auto found = files.find(path);
    if (found == files.end()) { *data = nullptr; return -1; }
    *data = const_cast<char *>(found->second.data());
    return int(found->second.size());
}
void FS_FreeFile(void *) { ++releases; }
#ifdef _MSC_VER
// The throwing fixture is stricter than the engine's longjmp error API.
#pragma warning(push)
#pragma warning(disable: 4702)
#endif
#include "../code/client/client_jk2_strings.hpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

template<typename F> void Rejected(F attempt) {
    bool rejected = false;
    try { attempt(); } catch (const std::runtime_error &) { rejected = true; }
    assert(rejected);
}
#ifdef _MSC_VER
// Deliberately exercise the engine's longjmp ABI; this frame owns only PODs.
#pragma warning(push)
#pragma warning(disable: 4611)
#endif
static void LongjmpRejected(vm_t *vm) {
    const auto before = liveAllocations;
    useLongjmp = true;
    if (!setjmp(failureJump)) {
        CL_JK2StringRegister(vm, 1, 4);
        assert(false);
    }
    useLongjmp = false;
    assert(liveAllocations == before);
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
static const std::string fixture =
    "VERSION 1\r\nCONFIG source-file\r\nID 250\r\nREFERENCE MENUS0\r\n"
    "DESCRIPTION \"synthetic only\"\r\nCOUNT 2\r\nINDEX 0\r\n{\r\n"
    "REFERENCE TEST\r\nTEXT_LANGUAGE1 \"One\\n\"quoted\" line\"\r\n"
    "TEXT_LANGUAGE2 \"French\"\r\nTEXT_LANGUAGE3 \"\"\r\n}\r\n"
    "INDEX 255\r\n{\r\nREFERENCE LAN/CABLE\r\nFLAGS SP_FLAG2\r\n"
    "NOTES \"unused\"\r\nTEXT_LANGUAGE1 \"Cable\"\r\n}\r\n";

int main(int argc, char **) {
    if (argc > 1) {
        const std::string data((std::istreambuf_iterator<char>(std::cin)), {});
        Package package;
        if (!Read(data, package)) return 1;
        std::cout << package.reference << " " << package.id << " " << package.entries.size() << '\n';
        return 0;
    }
    Package package;
    assert(Read(fixture, package));
    assert(package.id == 250 && package.entries.size() == 2);
    assert(Lookup(package, "menus0_test", 0) == "One\n\"quoted\" line");
    assert(Lookup(package, "MENUS0_TEST", 1) == "French");
    assert(Lookup(package, "MENUS0_TEST", 2).empty());
    assert(Lookup(package, "MENUS0_TEST", 7) == "One\n\"quoted\" line");
    assert(Lookup(package, "MENUS0_LAN/CABLE", 0) == "Cable");
    assert(Lookup(package, "MENUS0_TEST", 10).empty());
    assert(Lookup(package, "MENUS0_MISSING", 0).empty());
    const std::string punctuation = "\x91\x92\x93\x94\x85\x0b\x96\x97\t?.";
    assert(Decode(punctuation, 1) == "''\"\"..-- ?.");
    assert(Decode(punctuation, 2) == Decode(punctuation, 1));
    assert(Decode(punctuation, 3) == Decode(punctuation, 1));
    assert(Decode(punctuation, 5) == punctuation);
    auto malformed = [&](std::string value) {
        const auto before = Lookup(package, "MENUS0_TEST", 0);
        assert(!Read(value, package));
        assert(package.id == 250 && Lookup(package, "MENUS0_TEST", 0) == before);
    };
    malformed(""); malformed(fixture + '\0'); malformed(fixture.substr(0, fixture.size() - 4));
    malformed(std::string(MaximumFile + 1, 'a'));
    malformed("VERSION 1\nCONFIG " + std::string(MaximumLine, 'x') + '\n');
    auto replace = [&](const char *a, const char *b) {
        auto value = fixture; value.replace(value.find(a), std::strlen(a), b); malformed(value);
    };
    replace("VERSION 1", "VERSION 2"); replace("ID 250", "ID 256");
    replace("ID 250", "ID -1"); replace("COUNT 2", "COUNT 257");
    replace("COUNT 2", "COUNT 1"); replace("INDEX 255", "INDEX 0");
    replace("INDEX 255", "INDEX 256"); replace("TEXT_LANGUAGE1", "TEXT_LANGUAGE11");
    replace("REFERENCE LAN/CABLE", "REFERENCE TEST");
    replace("TEXT_LANGUAGE2", "TEXT_LANGUAGE1");
    replace("REFERENCE MENUS0", "REFERENCE ../MENUS0");
    replace("TEXT_LANGUAGE2 \"French\"", "TEXT_LANGUAGE2 \"French");
    char output[8] = {'x','x','x','x','x','x','x','x'};
    assert(Copy("abcdef", output + 1, 4));
    assert(output[0] == 'x' && std::string(output + 1) == "abc" && output[5] == 'x');
    assert(Copy("abcdef", output, 1) && !output[0]);
    assert(!Copy("abcdef", output, 0) && !Copy("abcdef", nullptr, 8));

    vm_t vm;
    const auto set = [&](int at, const char *text) { std::memcpy(vm.bytes.data() + at, text, std::strlen(text) + 1); };
    set(1, "menus0"); set(80, "MENUS0_TEST");
    files["strip/menus0.sp"] = fixture;
    assert(CL_JK2StringRegister(&vm, 1, 4) == qtrue);
    assert(loads == 1 && releases == 1);
    assert(CL_JK2StringRegister(&vm, 1, 1) == qtrue && loads == 1);
    testLanguage.integer = 1;
    assert(CL_JK2StringText(&vm, 80, 200, 8, false) == qtrue);
    assert(std::string(vm.bytes.data() + 200) == "French");
    testLanguage.integer = 2;
    assert(CL_JK2StringText(&vm, 80, 200, 8, true) == qfalse);
    assert(std::string(vm.bytes.data() + 200) == "??");
    assert(CL_JK2StringText(&vm, 80, 200, 8, false) == qtrue && !vm.bytes[200]);
    testLanguage.integer = 0;
    CL_JK2StringReset(4);
    assert(CL_JK2StringText(&vm, 80, 200, 8, true) == qtrue);
    CL_JK2StringReset(1);
    assert(CL_JK2StringText(&vm, 80, 200, 8, true) == qfalse);
    assert(fntech3::jk2::hoststrings::Storage().empty());
    // A full shutdown reloads changed installation/mod data.
    assert(CL_JK2StringRegister(&vm, 1, 4) == qtrue && loads == 2);
    set(1, "missing"); assert(CL_JK2StringRegister(&vm, 1, 4) == qfalse);
    set(1, "../menus0"); Rejected([&] { CL_JK2StringRegister(&vm, 1, 4); });
    set(1, "broken"); files["strip/broken.sp"] = "VERSION 0";
    Rejected([&] { CL_JK2StringRegister(&vm, 1, 4); });
    assert(releases == 3);
    set(1, "conflict"); files["strip/conflict.sp"] = fixture;
    Rejected([&] { CL_JK2StringRegister(&vm, 1, 4); });
    for (int invalid : {-1, 4, 5, 8, 9, 10}) {
        testLanguage.integer = invalid;
        Rejected([&] { CL_JK2StringText(&vm, 80, 200, 8, false); });
    }
    testLanguage.integer = 0;
    const auto untouched = vm.bytes;
    for (int capacity : {-1, 0, 65537}) Rejected([&] { CL_JK2StringText(&vm, 80, 200, capacity, false); });
    Rejected([&] { CL_JK2StringText(&vm, 80, 1020, 8, false); });
    Rejected([&] { CL_JK2StringText(&vm, 0, 200, 8, false); });
    Rejected([&] { CL_JK2StringText(&vm, 80, 0, 8, false); });
    assert(vm.bytes == untouched);
    std::fill(vm.bytes.begin() + 960, vm.bytes.end(), 'a');
    Rejected([&] { CL_JK2StringText(&vm, 960, 200, 8, false); });
    // All explicit failures unwind the registration worker before Com_Error.
    // Long names force allocations even on libraries with short-string storage.
    set(1, "invalid_package_name_with_a_slash/"); LongjmpRejected(&vm);
    set(1, "broken_package_with_a_long_name");
    files["strip/broken_package_with_a_long_name.sp"] = fixture + "UNKNOWN\n";
    const int beforeBrokenRelease = releases;
    LongjmpRejected(&vm); assert(releases == beforeBrokenRelease + 1);
    set(1, "conflicting_package_with_a_long_name");
    files["strip/conflicting_package_with_a_long_name.sp"] = fixture;
    LongjmpRejected(&vm);
    auto distinct = fixture;
    distinct.replace(distinct.find("ID 250"), 6, "ID 249");
    distinct.replace(distinct.find("REFERENCE MENUS0"), 16, "REFERENCE DIFFERENT");
    files["strip/package_exceeding_memory_budget.sp"] = distinct;
    set(1, "package_exceeding_memory_budget");
    auto &registered = fntech3::jk2::hoststrings::Storage();
    registered.begin()->second.bytes = 16 * 1024 * 1024;
    LongjmpRejected(&vm);
    assert(registered.size() == 1);
    for (int n = 1; n < 256; ++n)
        registered.emplace("capacity_fixture_" + std::to_string(n),
            fntech3::jk2::hoststrings::Registered{Package{}, 4, 0});
    set(1, "package_exceeding_registry_capacity"); LongjmpRejected(&vm);
    assert(registered.size() == 256);
    CL_JK2StringReset(4);
    // CL_Init's four packages (jk2mp.exe 0x40c5c0) carry their own owner, so
    // neither module's reset drops them; a missing one is reported, not fatal.
    auto ingame = fixture;
    ingame.replace(ingame.find("ID 250"), 6, "ID 248");
    ingame.replace(ingame.find("REFERENCE MENUS0"), 16, "REFERENCE INGAMETEXT");
    files["strip/mp_ingame.sp"] = ingame;
    printed = 0;
    CL_JK2StringRegisterEngine();
    assert(printed == 3 && registered.size() == 1 && registered.count("MP_INGAME"));
    set(80, "INGAMETEXT_TEST");
    CL_JK2StringReset(1); CL_JK2StringReset(4);
    assert(CL_JK2StringText(&vm, 80, 200, 8, true) == qtrue && std::string(vm.bytes.data() + 200).size() == 7);
    CL_JK2StringRegisterEngine(); assert(registered.size() == 1);
    CL_JK2StringReset(jk2StringEngineOwner); assert(registered.empty());
    vm.overflowProbe = true; vm.bytes[0] = 'x';
    Rejected([&] { fntech3::jk2::hoststrings::Text(&vm, (std::numeric_limits<intptr_t>::max)()); });
    assert(vm.checks == 1);
    std::cout << "Jedi Outcast StripEd parser, testLanguage fallback, transfers and lifetime hold\n";
}
