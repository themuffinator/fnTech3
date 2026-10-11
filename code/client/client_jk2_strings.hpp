/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JK2_STRINGS_HPP
#define FNTECH3_CLIENT_JK2_STRINGS_HPP
#include "../compat/jk2_string_package.hpp"
#include <cstdio>
#include <limits>

namespace fntech3::jk2::hoststrings {
struct Registered { strings::Package package; unsigned owners; std::size_t bytes; };
inline std::map<std::string, Registered> &Storage() {
    static std::map<std::string, Registered> packages;
    return packages;
}
inline const char *Text(vm_t *vm, intptr_t address) {
    if (!address) Com_Error(ERR_DROP, "Missing Jedi Outcast string reference");
    const auto *value = static_cast<const char *>(VM_ExplicitArgPtr(vm, address));
    for (unsigned n = 0; n <= 63; ++n) {
        if (address > (std::numeric_limits<intptr_t>::max)() - static_cast<intptr_t>(n))
            Com_Error(ERR_DROP, "Invalid Jedi Outcast string reference range");
        VM_CHECKBOUNDS(vm, address + static_cast<intptr_t>(n), 1);
        if (!value[n]) return value;
    }
    Com_Error(ERR_DROP, "Jedi Outcast string reference exceeds 63 bytes");
    return "";
}
inline unsigned Language() {
    const auto *language = Cvar_Get("sp_language", "0", CVAR_ARCHIVE | CVAR_NORESTART);
    if (language->integer < 0 || language->integer > 9)
        Com_Error(ERR_DROP, "Invalid Jedi Outcast sp_language index: %d", language->integer);
    // The existing atlas adapter implements only the single-byte lane.
    if (language->integer == 4 || language->integer == 5 || language->integer >= 8)
        Com_Error(ERR_DROP, "Jedi Outcast Asian language font boundary is open: sp_language %d", language->integer);
    return static_cast<unsigned>(language->integer);
}
// Explicit registration failures return through this owning scope before the
// host raises Com_Error, which leaves C++ frames with longjmp in the engine.
inline qboolean Register(const char *name, unsigned owner, char (&error)[160]) {
    const auto fail = [&](const char *message, const char *path = nullptr) {
        std::snprintf(error, sizeof(error), "%s%s%s", message, path ? ": " : "", path ? path : "");
        return qfalse;
    };
    const auto packageName = strings::Upper(name);
    if (!strings::Component(packageName)) return fail("Invalid Jedi Outcast string package name");
    auto &packages = Storage();
    const auto existing = packages.find(packageName);
    if (existing != packages.end()) {
        existing->second.owners |= owner;
        return qtrue;
    }
    if (packages.size() >= 256) return fail("Too many Jedi Outcast string packages");
    std::string path = "strip/" + packageName + ".sp";
    // Filesystem pak lookups are case-insensitive; use canonical lower case for loose files.
    for (auto &c : path) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    void *data = nullptr;
    const int length = FS_ReadFile(path.c_str(), &data);
    if (length < 0) { if (data) FS_FreeFile(data); return qfalse; }
    strings::Package parsed;
    const bool valid = data && strings::Read(std::string_view(static_cast<const char *>(data),
        static_cast<std::size_t>(length)), parsed);
    if (data) FS_FreeFile(data);
    if (!valid) return fail("Invalid Jedi Outcast string package", path.c_str());
    std::size_t total = static_cast<std::size_t>(length);
    for (const auto &entry : packages) {
        total += entry.second.bytes;
        if (entry.second.package.id == parsed.id || entry.second.package.reference == parsed.reference)
            return fail("Conflicting Jedi Outcast string package", path.c_str());
    }
    if (total > 16 * 1024 * 1024) return fail("Jedi Outcast string packages exceed the memory limit");
    packages.emplace(packageName, Registered{std::move(parsed), owner, static_cast<std::size_t>(length)});
    return qtrue;
}
} // namespace fntech3::jk2::hoststrings

inline void CL_JK2StringReset(unsigned owner) {
    auto &packages = fntech3::jk2::hoststrings::Storage();
    for (auto it = packages.begin(); it != packages.end();) {
        it->second.owners &= ~owner;
        if (!it->second.owners) it = packages.erase(it); else ++it;
    }
}
inline qboolean CL_JK2StringRegister(vm_t *vm, intptr_t name, unsigned owner) {
    namespace host = fntech3::jk2::hoststrings;
    host::Language();
    const char *packageName = host::Text(vm, name);
    char error[160]{};
    const auto result = host::Register(packageName, owner, error);
    if (error[0]) Com_Error(ERR_DROP, "%s", error);
    return result;
}
inline qboolean CL_JK2StringText(vm_t *vm, intptr_t reference, intptr_t output, int length, bool cgame) {
    using namespace fntech3::jk2;
    if (!output || length <= 0 || length > 65536) Com_Error(ERR_DROP, "Invalid Jedi Outcast string output capacity");
    VM_CHECKBOUNDS(vm, output, length);
    const auto *key = hoststrings::Text(vm, reference);
    const unsigned language = hoststrings::Language();
    std::string_view value;
    for (const auto &entry : hoststrings::Storage()) {
        value = strings::Lookup(entry.second.package, key, language);
        if (!value.empty()) break;
    }
    // Retail UI 201 always returns true; cgame 271 returns false with "??"
    // for missing or empty strings (jk2mp.exe 0x410656 / 0x402e5b).
    strings::Copy(cgame && value.empty() ? std::string_view("??") : value,
        static_cast<char *>(VM_ExplicitArgPtr(vm, output)), static_cast<std::size_t>(length));
    return cgame && value.empty() ? qfalse : qtrue;
}
#endif
