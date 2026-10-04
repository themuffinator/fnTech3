/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_HANDSHAKE_HPP
#define FNTECH3_JKA_HANDSHAKE_HPP

#include "netchan_profile.h"
#include <charconv>
#include <cstring>

/* Retail jamp.exe answers a bare challengeResponse <challenge> and a bare
 * connectResponse. The pinned OpenJK server also echoes the client nonce.
 * fnTech3 adds its protocol and marker. None selects Quake III's fallback. */
namespace fntech3::jka::handshake {
inline bool Integer(const char *text, int &value) noexcept {
 if (!text || !*text) return false;
 const char *end=text+std::strlen(text);
 const auto parsed=std::from_chars(text,end,value);
 return parsed.ec==std::errc{} && parsed.ptr==end;
}
inline bool ParseChallengeReply(int count, const char *const *words,
 int nonce, bool expectedAddress, int &challenge) noexcept {
 if (count<2 || count>5 || !words || !words[0] ||
  std::strcmp(words[0],"challengeResponse")) return false;
 int parsed=0, echoed=0;
 if (!Integer(words[1],parsed)) return false;
 if (count==2 ? !expectedAddress : !Integer(words[2],echoed) || echoed!=nonce) return false;
 if (count>=4 && (!Integer(words[3],echoed) || echoed!=NETCHAN_JKA_MP_PROTOCOL_VERSION)) return false;
 if (count==5 && (!words[4] || std::strcmp(words[4],"FnQL"))) return false;
 challenge=parsed;
 return true;
}
inline bool AcceptConnectReply(int count, const char *const *words,
 int challenge) noexcept {
 if (count<1 || count>3 || !words || !words[0] ||
  std::strcmp(words[0],"connectResponse")) return false;
 int echoed=0;
 if (count>=2 && (!Integer(words[1],echoed) || echoed!=challenge)) return false;
 return count<3 || (Integer(words[2],echoed) && echoed==NETCHAN_JKA_MP_PROTOCOL_VERSION);
}
} // namespace fntech3::jka::handshake
#endif
