/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_STRING_PACKAGE_HPP
#define FNTECH3_JKA_STRING_PACKAGE_HPP
#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>

/* Independently written StringEd data reader. Schema/lookup observations:
 * pinned OpenJK stringed_ingame.cpp, and 215 installed .str records in JKA
 * assets0/assets2.pk3. No implementation text is incorporated from OpenJK.
 * Physical records end at LF; legacy strings can contain vertical tabs,
 * internal quotes, byte codepage text and literal backslash-n sequences. */
namespace fntech3::jka::strings {
using Table = std::map<std::string, std::string>;
inline constexpr std::size_t MaximumFile = 4*1024*1024, MaximumLine = 16384;

inline std::string Upper(std::string_view value) {
    std::string result(value);
    for (auto &c : result) if (c >= 'a' && c <= 'z') c -= 'a'-'A';
    return result;
}
inline std::string_view Trim(std::string_view value) noexcept {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.remove_suffix(1);
    return value;
}
inline std::string_view Value(std::string_view value) noexcept {
    value = Trim(value);
    if (!value.empty() && value.front() == '"') value.remove_prefix(1);
    if (!value.empty() && value.back() == '"') value.remove_suffix(1);
    return value;
}
inline std::string Text(std::string_view value, bool western) {
    std::string result;
    for (std::size_t n = 0; n < value.size(); ++n) {
        if (value[n] == '\\' && n+1 < value.size() && value[n+1] == 'n') {
            result += '\n'; ++n; continue;
        }
        const auto byte = static_cast<unsigned char>(value[n]);
        if (western) {
            if (byte == 0x85) { result += "..."; continue; }
            if (byte == 0x91 || byte == 0x92) { result += '\''; continue; }
            if (byte == 0x93 || byte == 0x94) { result += '"'; continue; }
            if (byte == 0x96 || byte == 0x97) { result += '-'; continue; }
            if (byte == 0x0b) { result += '.'; continue; }
            if (byte == '\t') { result += ' '; continue; }
        }
        result += value[n];
    }
    if (western) for (std::size_t n = 1; n < result.size(); ++n)
        if (result[n-1] == '?' && result[n] == '.') result[n] = ' ';
    return result;
}
inline bool Western(std::string_view language) {
    const auto name = Upper(language);
    return name == "ENGLISH" || name == "FRENCH" || name == "GERMAN" || name == "SPANISH" ||
        name == "ITALIAN" || name == "POLISH" || name == "RUSSIAN";
}

inline bool Read(std::string_view package, std::string_view language,
    std::string_view data, Table &destination) {
    if (package.empty() || package.size() > 64 || language.empty() || language.size() > 64 ||
        data.empty() || data.size() > MaximumFile || data.find('\0') != std::string_view::npos) return false;
    const auto title = Upper(package), selected = Upper(language);
    Table next = destination;
    std::string reference, english;
    bool version = false, ended = false;
    while (!data.empty()) {
        const auto newline = data.find('\n');
        auto line = data.substr(0, newline);
        data = newline == std::string_view::npos ? std::string_view{} : data.substr(newline+1);
        if (line.size() >= MaximumLine) return false;
        bool quoted = false;
        for (std::size_t n = 0; n < line.size(); ++n) {
            if (line[n] == '"') quoted = !quoted;
            if (!quoted && line[n] == '/' && n+1 < line.size() && line[n+1] == '/') {
                line = line.substr(0, n); break;
            }
        }
        line = Trim(line);
        if (line.empty()) continue;
        if (ended) return false;
        const auto split = line.find_first_of(" \t\r");
        const auto keyword = Upper(line.substr(0, split));
        const auto rawValue = Trim(split == std::string_view::npos ? std::string_view{} : line.substr(split));
        const auto value = Value(rawValue);
        if (keyword == "VERSION") {
            if (version || value != "1") return false;
            version = true;
        } else if (keyword == "CONFIG" || keyword == "FILENOTES" || keyword == "NOTES" || keyword == "FLAGS") {
            // Authoring metadata is outside the module's text lookup contract.
        } else if (keyword == "REFERENCE") {
            if (!version || value.empty() || value.size() > 128) return false;
            reference = Upper(value); english.clear();
        } else if (keyword == "ENDMARKER") {
            ended = true;
        } else if (keyword.rfind("LANG_", 0) == 0) {
            if (reference.empty() || !version) return false;
            if (!rawValue.empty() && rawValue.front() == '"' &&
                (rawValue.size() < 2 || rawValue.back() != '"')) return false;
            const auto rowLanguage = keyword.substr(5);
            if (rowLanguage != "ENGLISH" && rowLanguage != selected) return false;
            const auto text = Text(value, Western(rowLanguage));
            const auto key = title+"_"+reference;
            if (rowLanguage == "ENGLISH") { english = text; next[key] = text; }
            else next[key] = Upper(text) == "#SAME" ? english : text;
        } else return false;
    }
    if (!version || !ended || next.size() > 65536) return false;
    destination.swap(next);
    return true;
}

inline std::string_view Lookup(const Table &table, std::string_view key) {
    if (key.size() > 255) return {};
    const auto found = table.find(Upper(key));
    return found == table.end() ? std::string_view{} : std::string_view(found->second);
}
inline bool Copy(std::string_view value, char *output, std::size_t length) noexcept {
    if (!output || !length) return false;
    const auto count = std::min(value.size(), length-1);
    std::copy_n(value.begin(), count, output); output[count] = '\0';
    return true;
}
} // namespace fntech3::jka::strings
#endif
