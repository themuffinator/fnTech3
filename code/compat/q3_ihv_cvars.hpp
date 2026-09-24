/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_CVARS_HPP
#define FNTECH3_Q3_IHV_CVARS_HPP
#include "../qcommon/q_shared.h"

namespace fntech3::q3ihv {
/* Original Cvar_Get/Cvar_Set2 and the supplied game registrations. Bit 2
 * has no observed engine policy and remains native metadata. Do not expose
 * modern engine-only flags through the 32-byte external record. */
struct CvarFlag { int native, engine; };
inline constexpr CvarFlag CvarFlags[] = {
    {1,CVAR_ARCHIVE}, {4,CVAR_USERINFO}, {8,CVAR_SERVERINFO},
    {16,CVAR_INIT}, {32,CVAR_LATCH}, {64,CVAR_CHEAT},
    {128,CVAR_ROM}, {256,CVAR_USER_CREATED}
};
inline int CvarFlagsToEngine(int flags) {
    int result=0;
    for (const auto &bit:CvarFlags) if(flags&bit.native)result|=bit.engine;
    return result;
}
inline int CvarFlagsToNative(int flags,int previous) {
    int result=previous;
    for (const auto &bit:CvarFlags) {
        result&=~bit.native;
        if(flags&bit.engine)result|=bit.native;
    }
    return result;
}
}
#endif
