/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_STRINGS_HPP
#define FNTECH3_CLIENT_JKA_STRINGS_HPP
#include "../compat/jka_string_package.hpp"
#include <set>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

/* Header shared by UI/cgame hosts. Assets are read through the selected
 * filesystem and its pak precedence/pure rules. No translation text ships
 * with the engine. This slice serves installed Western string packages;
 * Asian multi-page fonts and codepages remain named compatibility gaps. */
namespace fntech3::jka::hoststrings {
struct State {
    bool discovered = false;
    std::vector<std::string> paths, languages;
    std::map<std::string, strings::Table> cache;
};
inline State &Storage() { static State state; return state; }
inline std::string Lower(std::string value) {
    for (auto &c : value) if (c >= 'A' && c <= 'Z') c += 'a'-'A';
    return value;
}
inline bool Component(std::string_view value) noexcept {
    if (value.empty() || value.size() > 64) return false;
    for (const unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}
inline void Add(std::string_view path, std::set<std::string> &paths) {
    const auto slash = path.find('/');
    if (slash == std::string_view::npos || !Component(path.substr(0, slash)) || path.size() > 160 ||
        path.find('/', slash+1) != std::string_view::npos || path.size() < slash+6 ||
        strings::Upper(path.substr(path.size()-4)) != ".STR" ||
        !Component(path.substr(slash+1, path.size()-slash-5))) return;
    paths.insert(Lower(std::string(path)));
}
inline void Discover() {
    auto &state = Storage();
    if (state.discovered) return;
    Cvar_Get("se_language", "english", CVAR_ARCHIVE | CVAR_NORESTART);
    std::set<std::string> paths;
    int count = 0;
    char **files = FS_ListFiles("strings", ".str", &count);
    for (int n = 0; n < count; ++n) if (files[n]) Add(files[n], paths);
    FS_FreeFileList(files);
    /* The pak list can contain language paths directly; loose files require
     * one directory enumeration, so both sources share the same lookup. */
    files = FS_ListFiles("strings", "/", &count);
    for (int n = 0; n < count; ++n) {
        if (!files[n]) continue;
        auto directory = std::string_view(files[n]);
        while (!directory.empty() && directory.back() == '/') directory.remove_suffix(1);
        if (!Component(directory)) continue;
        const std::string prefix = "strings/"+std::string(directory);
        int nestedCount = 0;
        char **nested = FS_ListFiles(prefix.c_str(), ".str", &nestedCount);
        for (int i = 0; i < nestedCount; ++i) if (nested[i]) Add(std::string(directory)+"/"+nested[i], paths);
        FS_FreeFileList(nested);
    }
    FS_FreeFileList(files);
    if (paths.empty() || paths.size() > 4096) Com_Error(ERR_DROP, "Jedi Academy StringEd packages are missing or exceed the file limit");
    std::set<std::string> languages;
    state.paths.assign(paths.begin(), paths.end());
    for (const auto &path : paths) languages.insert(path.substr(0, path.find('/')));
    if (!languages.count("english")) Com_Error(ERR_DROP, "Jedi Academy StringEd requires its installed English packages");
    state.languages.push_back("english");
    for (const auto &language : languages) if (language != "english") state.languages.push_back(language);
    state.discovered = true;
}
inline std::string Selected() {
    Discover();
    char language[65]{};
    Cvar_VariableStringBuffer("se_language", language, sizeof(language));
    const auto selected = Lower(language);
    if (!Component(selected)) Com_Error(ERR_DROP, "Invalid Jedi Academy StringEd language name");
    const auto &languages = Storage().languages;
    if (std::find(languages.begin(), languages.end(), selected) == languages.end())
        Com_Error(ERR_DROP, "Selected Jedi Academy StringEd language is not installed: %s", selected.c_str());
    if (!strings::Western(selected) || selected == "russian" || selected == "polish")
        Com_Error(ERR_DROP, "Jedi Academy language-specific font and localization boundary is open: %s", selected.c_str());
    return selected;
}
inline const strings::Table &Table() {
    const auto selected = Selected();
    auto &state = Storage();
    const auto cached = state.cache.find(selected);
    if (cached != state.cache.end()) return cached->second;
    strings::Table next;
    for (const auto &path : state.paths) {
        const auto slash = path.find('/');
        if (path.substr(0, slash) != selected) continue;
        const auto package = path.substr(slash+1, path.size()-slash-5);
        void *data = nullptr;
        const int length = FS_ReadFile(("strings/"+path).c_str(), &data);
        const bool valid = length > 0 && data && strings::Read(package, selected,
            std::string_view(static_cast<const char *>(data), static_cast<std::size_t>(length)), next);
        if (data) FS_FreeFile(data);
        if (!valid) Com_Error(ERR_DROP, "Invalid Jedi Academy StringEd package: strings/%s", path.c_str());
    }
    return state.cache.emplace(selected, std::move(next)).first->second;
}
inline const char *Text(vm_t *vm, intptr_t address) {
    if (!address) Com_Error(ERR_DROP, "Missing Jedi Academy string reference");
    const auto *value = static_cast<const char *>(VM_ExplicitArgPtr(vm, address));
    for (unsigned n = 0; n <= 255; ++n) {
        VM_CHECKBOUNDS(vm, address+n, 1);
        if (!value[n]) return value;
    }
    Com_Error(ERR_DROP, "Unterminated Jedi Academy string reference");
    return "";
}
} // namespace fntech3::jka::hoststrings

inline void CL_JKAStringReset() { fntech3::jka::hoststrings::Storage() = {}; }
inline int CL_JKAStringCount() {
    fntech3::jka::hoststrings::Discover();
    return static_cast<int>(fntech3::jka::hoststrings::Storage().languages.size());
}
inline void CL_JKAStringLanguage(vm_t *vm, int index, intptr_t address) {
    fntech3::jka::hoststrings::Discover();
    VM_CHECKBOUNDS(vm, address, 128);
    if (!address) Com_Error(ERR_DROP, "Missing Jedi Academy language output");
    const auto &languages = fntech3::jka::hoststrings::Storage().languages;
    const auto value = index >= 0 && static_cast<std::size_t>(index) < languages.size()
        ? std::string_view(languages[index]) : std::string_view{};
    fntech3::jka::strings::Copy(value, static_cast<char *>(VM_ExplicitArgPtr(vm, address)), 128);
}
inline qboolean CL_JKAStringText(vm_t *vm, intptr_t reference, intptr_t output, int length, bool cgame) {
    if (!output || length <= 0 || length > 65536) Com_Error(ERR_DROP, "Invalid Jedi Academy string output capacity");
    VM_CHECKBOUNDS(vm, output, length);
    const auto *key = fntech3::jka::hoststrings::Text(vm, reference);
    const auto value = fntech3::jka::strings::Lookup(fntech3::jka::hoststrings::Table(), key);
    const auto result = cgame && value.empty() ? std::string_view("??") : value;
    fntech3::jka::strings::Copy(result, static_cast<char *>(VM_ExplicitArgPtr(vm, output)), static_cast<std::size_t>(length));
    return cgame && value.empty() ? qfalse : qtrue;
}
inline qboolean CL_JKAStringIsAsian() {
    fntech3::jka::hoststrings::Selected(); // refuses unsupported language data before answering
    return qfalse;
}
inline qboolean CL_JKAStringUsesSpaces() {
    fntech3::jka::hoststrings::Selected();
    return qtrue;
}
inline unsigned CL_JKAStringReadChar(vm_t *vm, intptr_t text, intptr_t advance, intptr_t trailingPunctuation) {
    fntech3::jka::hoststrings::Selected();
    if (!text || !advance) Com_Error(ERR_DROP, "Missing Jedi Academy character input or advance output");
    VM_CHECKBOUNDS(vm, text, 1); VM_CHECKBOUNDS(vm, advance, sizeof(std::int32_t));
    if (trailingPunctuation) VM_CHECKBOUNDS(vm, trailingPunctuation, sizeof(std::int32_t));
    const auto value = *static_cast<const unsigned char *>(VM_ExplicitArgPtr(vm, text));
    const std::int32_t count = 1, punctuation = 0;
    std::memcpy(VM_ExplicitArgPtr(vm, advance), &count, sizeof(count));
    if (trailingPunctuation) std::memcpy(VM_ExplicitArgPtr(vm, trailingPunctuation), &punctuation, sizeof(punctuation));
    return value;
}
#endif
