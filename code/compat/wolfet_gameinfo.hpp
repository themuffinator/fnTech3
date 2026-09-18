/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_GAMEINFO_HPP
#define FNTECH3_WOLFET_GAMEINFO_HPP
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace fntech3::wolfet::gameinfo {
// Enemy Territory's engine reads its game's rules for single-player and
// cooperative play from gameinfo.dat rather than knowing them itself: which
// game types count as single player, whether the single-player map commands
// are enabled at all, and which game types those commands and the game
// default to. Retail's file lists game type 0 alone as single player, leaves
// the single-player commands disabled, and defaults to the campaign.
// Independently written reader for that file's keywords. A missing file, like
// ET's, means no game type is single player and nothing is enabled.
struct GameInfo {
    bool spEnabled = false;
    uint32_t spGameTypes = 0;
    int defaultSPGameType = 0;
    uint32_t coopGameTypes = 0;
    int defaultCoopGameType = 0;
    int defaultGameType = 0;
    int usesProfiles = 0;
};

struct Token { std::string text; bool newLine = false; };

// A token and whether a line break came before it; // and /* */ are comments.
inline bool Next(const char *&cursor, const char *end, Token &token) {
    token.text.clear();
    token.newLine = false;
    for (;;) {
        while (cursor < end && static_cast<unsigned char>(*cursor) <= ' ') {
            if (*cursor == '\n') token.newLine = true;
            ++cursor;
        }
        if (cursor + 1 < end && cursor[0] == '/' && cursor[1] == '/') {
            while (cursor < end && *cursor != '\n') ++cursor;
            continue;
        }
        if (cursor + 1 < end && cursor[0] == '/' && cursor[1] == '*') {
            cursor += 2;
            while (cursor + 1 < end && !(cursor[0] == '*' && cursor[1] == '/')) {
                if (*cursor == '\n') token.newLine = true;
                ++cursor;
            }
            cursor = cursor + 1 < end ? cursor + 2 : end;
            continue;
        }
        break;
    }
    if (cursor >= end) return false;
    if (*cursor == '"') {
        ++cursor;
        while (cursor < end && *cursor != '"' && *cursor != '\n') token.text.push_back(*cursor++);
        if (cursor < end && *cursor == '"') ++cursor;
        return true;
    }
    while (cursor < end && static_cast<unsigned char>(*cursor) > ' ') token.text.push_back(*cursor++);
    return true;
}

inline bool SameWord(const std::string &left, const char *right) {
    size_t i = 0;
    for (; i < left.size() && right[i]; ++i) {
        const char a = left[i] >= 'A' && left[i] <= 'Z' ? char(left[i] - 'A' + 'a') : left[i];
        const char b = right[i] >= 'A' && right[i] <= 'Z' ? char(right[i] - 'A' + 'a') : right[i];
        if (a != b) return false;
    }
    return i == left.size() && !right[i];
}

inline uint32_t Bit(const std::string &value) {
    const long type = std::strtol(value.c_str(), nullptr, 10);
    return type >= 0 && type < 32 ? uint32_t(1) << type : 0;
}

inline GameInfo Parse(const char *text, size_t length) {
    GameInfo info;
    if (!text) return info;
    const char *cursor = text, *end = text + length;
    Token token;
    bool pending = Next(cursor, end, token);
    while (pending) {
        const std::string keyword = token.text;
        pending = Next(cursor, end, token);
        // A list or a value runs to the end of its keyword's line.
        const auto sameLine = [&]() { return pending && !token.newLine; };
        const auto number = [&](int &target) {
            if (sameLine()) {
                target = int(std::strtol(token.text.c_str(), nullptr, 10));
                pending = Next(cursor, end, token);
            }
        };
        if (SameWord(keyword, "spEnabled")) {
            info.spEnabled = true;
        } else if (SameWord(keyword, "spGameTypes") || SameWord(keyword, "coopGameTypes")) {
            uint32_t &types = SameWord(keyword, "spGameTypes") ? info.spGameTypes : info.coopGameTypes;
            while (sameLine()) {
                types |= Bit(token.text);
                pending = Next(cursor, end, token);
            }
        } else if (SameWord(keyword, "defaultSPGameType")) {
            number(info.defaultSPGameType);
        } else if (SameWord(keyword, "defaultCoopGameType")) {
            number(info.defaultCoopGameType);
        } else if (SameWord(keyword, "defaultGameType")) {
            number(info.defaultGameType);
        } else if (SameWord(keyword, "usesProfiles")) {
            number(info.usesProfiles);
        }
    }
    return info;
}

inline bool IsSinglePlayer(const GameInfo &info, int gameType) {
    return gameType >= 0 && gameType < 32 && (info.spGameTypes & (uint32_t(1) << gameType)) != 0;
}

inline bool IsCoop(const GameInfo &info, int gameType) {
    return gameType >= 0 && gameType < 32 && (info.coopGameTypes & (uint32_t(1) << gameType)) != 0;
}
}
#endif
