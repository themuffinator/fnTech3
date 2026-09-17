/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_MODULE_ABI_H
#define FNTECH3_WOLFET_MODULE_ABI_H
#include "wolfet_game_abi.h"
enum {
#define FNTECH3_WOLFET_MODULE_CALL(module, direction, name, number) FNTECH3_WOLFET_##name = number,
#include "wolfet_module_calls.def"
#undef FNTECH3_WOLFET_MODULE_CALL
};
#ifdef __cplusplus
#include <array>
#include <cstdint>
namespace fntech3::wolfet::module {
enum class Module : unsigned { UI, CGAME, GAME };
enum class Direction : unsigned { IMPORT, EXPORT };
enum class Call : uint16_t {
    Invalid,
#define FNTECH3_WOLFET_MODULE_CALL(module, direction, name, number) name,
#include "wolfet_module_calls.def"
#undef FNTECH3_WOLFET_MODULE_CALL
    Count
};
struct Descriptor { Module module; Direction direction; Call call; int ordinal; const char *name; };
inline constexpr Descriptor descriptors[]{
#define FNTECH3_WOLFET_MODULE_CALL(module, direction, name, number) {Module::module, Direction::direction, Call::name, number, #name},
#include "wolfet_module_calls.def"
#undef FNTECH3_WOLFET_MODULE_CALL
};
struct Tables {
    std::array<std::array<std::array<Call,1024>,2>,3> entries{};
    bool valid = true;
    constexpr Tables() {
        for (auto d : descriptors) {
            if (d.ordinal < 0 || d.ordinal >= 1024) { valid = false; continue; }
            auto &slot = entries[static_cast<unsigned>(d.module)][static_cast<unsigned>(d.direction)][d.ordinal];
            if (slot != Call::Invalid) valid = false;
            slot = d.call;
        }
    }
};
inline constexpr Tables tables;
static_assert(tables.valid, "ET ordinals must be unique and bounded within each module/direction");
inline constexpr Call Lookup(Module module, Direction direction, intptr_t ordinal) {
    return static_cast<unsigned>(module) < 3 && static_cast<unsigned>(direction) < 2 && ordinal >= 0 && ordinal < 1024
        ? tables.entries[static_cast<unsigned>(module)][static_cast<unsigned>(direction)][ordinal] : Call::Invalid;
}
inline constexpr const Descriptor *Describe(Call call) {
    const auto index = static_cast<unsigned>(call);
    return index && index < static_cast<unsigned>(Call::Count) ? &descriptors[index-1] : nullptr;
}
} // namespace fntech3::wolfet::module
#endif
#endif
