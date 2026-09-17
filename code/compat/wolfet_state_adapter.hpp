/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_STATE_ADAPTER_HPP
#define FNTECH3_WOLFET_STATE_ADAPTER_HPP
#include "wolfet_game_abi.h"
#include "rtcw_state_adapter.hpp"
namespace fntech3::wolfet::state {
namespace detail {
using rtcw::state::detail::Field;
inline constexpr trType_t trajectories[]{
    TR_STATIONARY, TR_INTERPOLATE, TR_LINEAR, TR_LINEAR_STOP,
    TR_RTCW_LINEAR_STOP_BACK, TR_SINE, TR_GRAVITY, TR_RTCW_GRAVITY_LOW,
    TR_RTCW_GRAVITY_FLOAT, TR_RTCW_GRAVITY_PAUSED, TR_RTCW_ACCELERATE,
    TR_RTCW_DECELERATE, TR_WOLFET_SPLINE, TR_WOLFET_LINEAR_PATH
};
template<bool Out, class E, class I> bool Trajectory(E &external, I &internal) {
    if constexpr (Out) {
        uint32_t raw; std::memcpy(&raw, &internal.trType, 4);
        unsigned index = 0;
        while (index < 14 && raw != static_cast<uint32_t>(trajectories[index])) ++index;
        if (index == 14) return false;
        external.trType = index;
    } else {
        if (external.trType < 0 || external.trType >= 14) return false;
        internal.trType = trajectories[external.trType];
    }
    Field<Out>(external.trTime, internal.trTime);
    Field<Out>(external.trDuration, internal.trDuration);
    Field<Out>(external.trBase, internal.trBase);
    Field<Out>(external.trDelta, internal.trDelta);
    return true;
}
template<bool Out, class E, class I> bool Trace(E &external, I &internal) {
    uint32_t allsolid, startsolid;
    if constexpr (Out) {
        std::memcpy(&allsolid, &internal.allsolid, 4);
        std::memcpy(&startsolid, &internal.startsolid, 4);
    } else { allsolid = external.allsolid; startsolid = external.startsolid; }
    if (allsolid > 1 || startsolid > 1) return false;
    Field<Out>(external.allsolid, internal.allsolid);
    Field<Out>(external.startsolid, internal.startsolid);
    Field<Out>(external.fraction, internal.fraction);
    Field<Out>(external.endpos, internal.endpos);
    Field<Out>(external.plane.normal, internal.plane.normal);
    Field<Out>(external.plane.dist, internal.plane.dist);
    Field<Out>(external.plane.type, internal.plane.type);
    Field<Out>(external.plane.signbits, internal.plane.signbits);
    Field<Out>(external.plane.pad, internal.plane.pad);
    Field<Out>(external.surfaceFlags, internal.surfaceFlags);
    Field<Out>(external.contents, internal.contents);
    Field<Out>(external.entityNum, internal.entityNum);
    return true;
}
template<bool Out, class E, class I> bool Entity(E &external, I &internal) {
    Field<Out>(external.number, internal.number);
    Field<Out>(external.eType, internal.eType);
    Field<Out>(external.eFlags, internal.eFlags);
    if (!Trajectory<Out>(external.pos, internal.pos)) return false;
    if (!Trajectory<Out>(external.apos, internal.apos)) return false;
    Field<Out>(external.time, internal.time);
    Field<Out>(external.time2, internal.time2);
    Field<Out>(external.origin, internal.origin);
    Field<Out>(external.origin2, internal.origin2);
    Field<Out>(external.angles, internal.angles);
    Field<Out>(external.angles2, internal.angles2);
    Field<Out>(external.otherEntityNum, internal.otherEntityNum);
    Field<Out>(external.otherEntityNum2, internal.otherEntityNum2);
    Field<Out>(external.groundEntityNum, internal.groundEntityNum);
    Field<Out>(external.constantLight, internal.constantLight);
    Field<Out>(external.dl_intensity, internal.rtcw.dl_intensity);
    Field<Out>(external.loopSound, internal.loopSound);
    Field<Out>(external.modelindex, internal.modelindex);
    Field<Out>(external.modelindex2, internal.modelindex2);
    Field<Out>(external.clientNum, internal.clientNum);
    Field<Out>(external.frame, internal.frame);
    Field<Out>(external.solid, internal.solid);
    Field<Out>(external.event, internal.event);
    Field<Out>(external.eventParm, internal.eventParm);
    Field<Out>(external.eventSequence, internal.rtcw.eventSequence);
    Field<Out>(external.events, internal.rtcw.events);
    Field<Out>(external.eventParms, internal.rtcw.eventParms);
    Field<Out>(external.powerups, internal.powerups);
    Field<Out>(external.weapon, internal.weapon);
    Field<Out>(external.legsAnim, internal.legsAnim);
    Field<Out>(external.torsoAnim, internal.torsoAnim);
    Field<Out>(external.density, internal.rtcw.density);
    Field<Out>(external.dmgFlags, internal.rtcw.dmgFlags);
    Field<Out>(external.onFireStart, internal.rtcw.onFireStart);
    Field<Out>(external.onFireEnd, internal.rtcw.onFireEnd);
    Field<Out>(external.nextWeapon, internal.wolfet.nextWeapon);
    Field<Out>(external.teamNum, internal.rtcw.teamNum);
    Field<Out>(external.effect1Time, internal.rtcw.effect1Time);
    Field<Out>(external.effect2Time, internal.rtcw.effect2Time);
    Field<Out>(external.effect3Time, internal.rtcw.effect3Time);
    Field<Out>(external.aiState, internal.rtcw.aiState);
    Field<Out>(external.animMovetype, internal.rtcw.animMovetype);
    return true;
}
template<bool Out, class E, class I> bool Player(E &external, I &internal) {
    Field<Out>(external.commandTime, internal.commandTime);
    Field<Out>(external.pm_type, internal.pm_type);
    Field<Out>(external.bobCycle, internal.bobCycle);
    Field<Out>(external.pm_flags, internal.pm_flags);
    Field<Out>(external.pm_time, internal.pm_time);
    Field<Out>(external.origin, internal.origin);
    Field<Out>(external.velocity, internal.velocity);
    Field<Out>(external.weaponTime, internal.weaponTime);
    Field<Out>(external.weaponDelay, internal.rtcw.weaponDelay);
    Field<Out>(external.grenadeTimeLeft, internal.rtcw.grenadeTimeLeft);
    Field<Out>(external.gravity, internal.gravity);
    Field<Out>(external.leanf, internal.rtcw.leanf);
    Field<Out>(external.speed, internal.speed);
    Field<Out>(external.delta_angles, internal.delta_angles);
    Field<Out>(external.groundEntityNum, internal.groundEntityNum);
    Field<Out>(external.legsTimer, internal.legsTimer);
    Field<Out>(external.legsAnim, internal.legsAnim);
    Field<Out>(external.torsoTimer, internal.torsoTimer);
    Field<Out>(external.torsoAnim, internal.torsoAnim);
    Field<Out>(external.movementDir, internal.movementDir);
    Field<Out>(external.eFlags, internal.eFlags);
    Field<Out>(external.eventSequence, internal.eventSequence);
    for (unsigned n = 0; n < 4; ++n) {
        if (n < 2) Field<Out>(external.events[n], internal.events[n]);
        else Field<Out>(external.events[n], internal.rtcw.events[n - 2]);
    }
    for (unsigned n = 0; n < 4; ++n) {
        if (n < 2) Field<Out>(external.eventParms[n], internal.eventParms[n]);
        else Field<Out>(external.eventParms[n], internal.rtcw.eventParms[n - 2]);
    }
    Field<Out>(external.oldEventSequence, internal.rtcw.oldEventSequence);
    Field<Out>(external.externalEvent, internal.externalEvent);
    Field<Out>(external.externalEventParm, internal.externalEventParm);
    Field<Out>(external.externalEventTime, internal.externalEventTime);
    Field<Out>(external.clientNum, internal.clientNum);
    Field<Out>(external.weapon, internal.weapon);
    Field<Out>(external.weaponstate, internal.weaponstate);
    Field<Out>(external.item, internal.rtcw.item);
    Field<Out>(external.viewangles, internal.viewangles);
    Field<Out>(external.viewheight, internal.viewheight);
    Field<Out>(external.damageEvent, internal.damageEvent);
    Field<Out>(external.damageYaw, internal.damageYaw);
    Field<Out>(external.damagePitch, internal.damagePitch);
    Field<Out>(external.damageCount, internal.damageCount);
    Field<Out>(external.stats, internal.stats);
    Field<Out>(external.persistant, internal.persistant);
    Field<Out>(external.powerups, internal.powerups);
    for (unsigned n = 0; n < 64; ++n) {
        if (n < 16) Field<Out>(external.ammo[n], internal.ammo[n]);
        else Field<Out>(external.ammo[n], internal.rtcw.ammo[n - 16]);
    }
    Field<Out>(external.ammoclip, internal.rtcw.ammoclip);
    Field<Out>(external.holdable, internal.rtcw.holdable);
    Field<Out>(external.holding, internal.rtcw.holding);
    Field<Out>(external.weapons, internal.rtcw.weapons);
    Field<Out>(external.mins, internal.rtcw.mins);
    Field<Out>(external.maxs, internal.rtcw.maxs);
    Field<Out>(external.crouchMaxZ, internal.rtcw.crouchMaxZ);
    Field<Out>(external.crouchViewHeight, internal.rtcw.crouchViewHeight);
    Field<Out>(external.standViewHeight, internal.rtcw.standViewHeight);
    Field<Out>(external.deadViewHeight, internal.rtcw.deadViewHeight);
    Field<Out>(external.runSpeedScale, internal.rtcw.runSpeedScale);
    Field<Out>(external.sprintSpeedScale, internal.rtcw.sprintSpeedScale);
    Field<Out>(external.crouchSpeedScale, internal.rtcw.crouchSpeedScale);
    Field<Out>(external.viewlocked, internal.rtcw.viewlocked);
    Field<Out>(external.viewlocked_entNum, internal.rtcw.viewlocked_entNum);
    Field<Out>(external.friction, internal.rtcw.friction);
    Field<Out>(external.nextWeapon, internal.wolfet.nextWeapon);
    Field<Out>(external.teamNum, internal.rtcw.teamNum);
    Field<Out>(external.onFireStart, internal.rtcw.onFireStart);
    Field<Out>(external.serverCursorHint, internal.rtcw.serverCursorHint);
    Field<Out>(external.serverCursorHintVal, internal.rtcw.serverCursorHintVal);
    if (!Trace<Out>(external.serverCursorHintTrace, internal.rtcw.serverCursorHintTrace)) return false;
    Field<Out>(external.ping, internal.ping);
    Field<Out>(external.pmove_framecount, internal.pmove_framecount);
    Field<Out>(external.entityEventSequence, internal.entityEventSequence);
    Field<Out>(external.sprintExertTime, internal.rtcw.sprintExertTime);
    Field<Out>(external.classWeaponTime, internal.rtcw.classWeaponTime);
    Field<Out>(external.jumpTime, internal.jumpTime);
    Field<Out>(external.weapAnim, internal.rtcw.weapAnim);
    Field<Out>(external.releasedFire, internal.rtcw.releasedFire);
    Field<Out>(external.aimSpreadScaleFloat, internal.rtcw.aimSpreadScaleFloat);
    Field<Out>(external.aimSpreadScale, internal.rtcw.aimSpreadScale);
    Field<Out>(external.lastFireTime, internal.rtcw.lastFireTime);
    Field<Out>(external.quickGrenTime, internal.rtcw.quickGrenTime);
    Field<Out>(external.leanStopDebounceTime, internal.rtcw.leanStopDebounceTime);
    Field<Out>(external.weapHeat, internal.rtcw.weapHeat);
    Field<Out>(external.curWeapHeat, internal.rtcw.curWeapHeat);
    Field<Out>(external.identifyClient, internal.rtcw.identifyClient);
    Field<Out>(external.identifyClientHealth, internal.wolfet.identifyClientHealth);
    Field<Out>(external.aiState, internal.rtcw.aiState);
    return true;
}
template<bool Out, class E, class I> bool Command(E &external, I &internal) {
    Field<Out>(external.serverTime, internal.serverTime);
    Field<Out>(external.angles, internal.angles);
    Field<Out>(external.forwardmove, internal.forwardmove);
    Field<Out>(external.rightmove, internal.rightmove);
    Field<Out>(external.upmove, internal.upmove);
    Field<Out>(external.wbuttons, internal.rtcw.wbuttons);
    Field<Out>(external.flags, internal.wolfet.flags);
    Field<Out>(external.doubleTap, internal.wolfet.doubleTap);
    Field<Out>(external.identClient, internal.wolfet.identClient);
    if constexpr (Out) {
        if (internal.buttons < 0 || internal.buttons > 255 || internal.weapon > 255 || internal.wolfet.doubleTap > 7) return false;
        external.buttons = static_cast<uint8_t>(internal.buttons);
        external.weapon = static_cast<uint8_t>(internal.weapon);
    } else {
        if (external.doubleTap > 7) return false;
        internal.buttons = external.buttons; internal.weapon = external.weapon;
    }
    return true;
}
} // namespace detail
#define WOLFET_TRANSFER(kind, externalType, internalType) \
inline bool FromInternal(const internalType &source, externalType &destination) { \
    externalType temporary{}; if (!detail::kind<true>(temporary, source)) return false; \
    destination = temporary; return true; } \
inline bool ToInternal(const externalType &source, internalType &destination) { \
    internalType temporary{}; if (!detail::kind<false>(source, temporary)) return false; \
    destination = temporary; return true; }
WOLFET_TRANSFER(Entity, fntech3_wolfet_entityState_t, entityState_t)
WOLFET_TRANSFER(Player, fntech3_wolfet_playerState_t, playerState_t)
WOLFET_TRANSFER(Command, fntech3_wolfet_usercmd_t, usercmd_t)
WOLFET_TRANSFER(Trace, fntech3_wolfet_trace_t, trace_t)
#undef WOLFET_TRANSFER
} // namespace fntech3::wolfet::state
#endif
