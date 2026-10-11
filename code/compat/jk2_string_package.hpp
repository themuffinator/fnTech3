/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JK2_STRING_PACKAGE_HPP
#define FNTECH3_JK2_STRING_PACKAGE_HPP
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <utility>

/* Independent StripEd reader, observed in the installed JK2 assets1.pk3
 * packages and the pinned OpenJK strip.cpp format contract. JK2's .sp files
 * are distinct from Academy's StringEd .str files. No external implementation
 * text is incorporated. Text uses the authored byte codepage and font atlas.
 */
namespace fntech3::jk2::strings {
inline constexpr std::size_t MaximumFile = 2 * 1024 * 1024, MaximumLine = 8191;
struct Entry {
    std::array<std::string, 10> text;
    unsigned present = 0;
};
struct Package {
    unsigned id = 0;
    std::string reference;
    std::map<std::string, Entry> entries;
};
inline std::string Upper(std::string_view value) {
    std::string result(value);
    for (auto &c : result) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    return result;
}
inline std::string_view Trim(std::string_view value) noexcept {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.remove_suffix(1);
    return value;
}
inline bool Component(std::string_view value) noexcept {
    if (value.empty() || value.size() > 63) return false;
    for (const unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}
inline bool Number(std::string_view value, unsigned maximum, unsigned &result) noexcept {
    if (value.empty()) return false;
    unsigned parsed = 0;
    for (const unsigned char c : value) {
        if (c < '0' || c > '9' || parsed > maximum / 10) return false;
        parsed = parsed * 10 + c - '0';
        if (parsed > maximum) return false;
    }
    result = parsed;
    return true;
}
inline std::string Decode(std::string_view value, unsigned language) {
    std::string result;
    for (std::size_t n = 0; n < value.size(); ++n) {
        if (value[n] == '\\' && n + 1 < value.size() && value[n + 1] == 'n') {
            result += '\n'; ++n;
        } else {
            auto byte = static_cast<unsigned char>(value[n]);
            // jk2mp.exe 1.02a 0x437440, called for languages 1-3.
            // In particular its ellipsis becomes one period, not Academy's three.
            if (language <= 3) {
                if (byte == 0x91 || byte == 0x92) byte = '\'';
                else if (byte == 0x93 || byte == 0x94) byte = '"';
                else if (byte == 0x0b || byte == 0x85) byte = '.';
                else if (byte == 0x96 || byte == 0x97) byte = '-';
                else if (byte == '\t') byte = ' ';
            }
            result += static_cast<char>(byte);
        }
    }
    return result;
}
inline bool Read(std::string_view data, Package &destination) {
    if (data.empty() || data.size() > MaximumFile || data.find('\0') != std::string_view::npos) return false;
    Package next;
    Entry entry;
    std::array<bool, 256> indices{};
    std::string reference;
    unsigned count = 0, parsed = 0, index = 0;
    bool version = false, id = false, haveReference = false, haveCount = false;
    enum class Phase { Header, Open, Entry, Between } phase = Phase::Header;
    while (!data.empty()) {
        const auto end = data.find('\n');
        auto line = data.substr(0, end);
        data = end == std::string_view::npos ? std::string_view{} : data.substr(end + 1);
        if (line.size() > MaximumLine) return false;
        line = Trim(line);
        if (line.empty()) continue;
        const auto space = line.find_first_of(" \t");
        const auto key = Upper(line.substr(0, space));
        auto value = space == std::string_view::npos ? std::string_view{} : Trim(line.substr(space + 1));
        if (!value.empty() && value.front() == '"') {
            if (value.size() < 2 || value.back() != '"') return false;
            value = value.substr(1, value.size() - 2); // authored strings can contain internal quotes
        }
        if (phase == Phase::Open) {
            if (key != "{" || !value.empty()) return false;
            phase = Phase::Entry;
            continue;
        }
        if (phase == Phase::Entry) {
            if (key == "}" && value.empty()) {
                if (reference.empty() || !(entry.present & 1) ||
                    next.reference.size() + 1 + reference.size() > 63 ||
                    !next.entries.emplace(reference, std::move(entry)).second) return false;
                phase = Phase::Between; ++parsed;
            } else if (key == "REFERENCE") {
                if (!reference.empty() || value.empty() || value.size() > 63) return false;
                reference = Upper(value); // one installed reference contains '/' (LAN/CABLE)
            } else if (key.compare(0, 13, "TEXT_LANGUAGE") == 0) {
                unsigned language = 0;
                if (!Number(std::string_view(key).substr(13), 10, language) || !language ||
                    (entry.present & (1u << (language - 1)))) return false;
                entry.text[language - 1] = Decode(value, language);
                entry.present |= 1u << (language - 1);
            } else if (key != "NOTES" && key != "FLAGS") return false;
            continue;
        }
        if (key == "INDEX") {
            if (!version || !id || !haveReference || !haveCount || parsed >= count ||
                !Number(value, 255, index) || indices[index]) return false;
            indices[index] = true; entry = {}; reference.clear(); phase = Phase::Open;
        } else if (phase != Phase::Header) return false;
        else if (key == "VERSION") {
            if (version || value != "1") return false;
            version = true;
        } else if (!version) return false;
        else if (key == "ID") {
            if (id || !Number(value, 255, next.id)) return false;
            id = true;
        } else if (key == "REFERENCE") {
            if (haveReference || !Component(value)) return false;
            next.reference = Upper(value); haveReference = true;
        } else if (key == "COUNT") {
            if (haveCount || !Number(value, 256, count)) return false;
            haveCount = true;
        } else if (key != "CONFIG" && key != "DESCRIPTION") return false;
    }
    if (!version || !id || !haveReference || !haveCount || parsed != count ||
        phase == Phase::Entry || phase == Phase::Open) return false;
    destination = std::move(next);
    return true;
}
inline std::string_view Lookup(const Package &package, std::string_view reference, unsigned language) {
    if (language >= 10) return {};
    const auto key = Upper(reference);
    const auto prefix = package.reference + "_";
    if (key.compare(0, prefix.size(), prefix)) return {};
    const auto found = package.entries.find(key.substr(prefix.size()));
    if (found == package.entries.end()) return {};
    const auto &entry = found->second;
    // Missing translations fall back to English; an authored empty translation does not.
    return entry.text[(entry.present & (1u << language)) ? language : 0];
}
inline bool Copy(std::string_view text, char *destination, std::size_t capacity) noexcept {
    if (!destination || !capacity) return false;
    const auto count = std::min(text.size(), capacity - 1);
    if (count) std::memcpy(destination, text.data(), count);
    destination[count] = '\0';
    return true;
}
} // namespace fntech3::jk2::strings
#endif
