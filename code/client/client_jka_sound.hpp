/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_SOUND_HPP
#define FNTECH3_CLIENT_JKA_SOUND_HPP
#include "../qcommon/q_shared.h"
#include <climits>

/* Jedi Academy's multiplayer sound channels, numbered as the retail engine
 * reads them (jamp.exe e8062d39...): S_StartSound 0x46ae4d compares with 7,
 * the ambient channel, before scaling the listener's own sounds, 0x46ae8b..
 * 0x46ae98 mark a voice on 3, 4 and 12, and S_PickChannel 0x46a15b picks a
 * free channel for 10 as for 0. Their names follow OpenJK 1a6a643's
 * codemp/qcommon/q_shared.h soundChannel_t, which agrees with every one of
 * those numbers. Academy inserts channels among Quake III's, so a raw
 * Academy number must never reach the engine's channel rules: its 7 is
 * ambient, the engine's announcer. */
namespace fntech3::jka::sound {
enum : int {
    ChanAuto, ChanLocal, ChanWeapon, ChanVoice, ChanVoiceAtten, ChanItem, ChanBody,
    ChanAmbient, ChanLocalSound, ChanAnnouncer, ChanLessAtten, ChanMenu1,
    ChanVoiceGlobal, ChanMusic
};

/* Channels that mean what one of the engine's means keep the engine's number,
 * so Fn's rules for it apply (local channels heard at the listener, the
 * announcer never evicted, each channel's tone class). Academy's attenuated
 * and global voices, ambient, less-attenuated, menu and music channels, and
 * any other number, take numbers of their own above the engine's, as Elite
 * Force single player's do; a number that cannot be moved without overflow
 * is negative or past every engine channel already, and keeps its value. */
inline constexpr int OwnChannelBase = CHAN_ANNOUNCER + 1;
inline constexpr int EngineChannel(int channel) noexcept {
    switch (channel) {
    case ChanAuto: return CHAN_AUTO;
    case ChanLocal: return CHAN_LOCAL;
    case ChanWeapon: return CHAN_WEAPON;
    case ChanVoice: return CHAN_VOICE;
    case ChanItem: return CHAN_ITEM;
    case ChanBody: return CHAN_BODY;
    case ChanLocalSound: return CHAN_LOCAL_SOUND;
    case ChanAnnouncer: return CHAN_ANNOUNCER;
    default:
        return channel < 0 || channel > INT_MAX - OwnChannelBase ? channel : OwnChannelBase + channel;
    }
}

/* Retail plays the listener's own sounds below the ambient channel at 0.75
 * of full volume (0x46ae6d..0x46ae88: 0.75 at 0x54d4f0 times 255), the rest
 * at full volume. A local sound is always the listener's own, so this is the
 * level it hands Fn's mixer; no retail mixing replaces Fn's. */
inline constexpr float ListenerVolume(int channel) noexcept {
    return channel < ChanAmbient ? 0.75f : 1.0f;
}
} // namespace fntech3::jka::sound
#endif
