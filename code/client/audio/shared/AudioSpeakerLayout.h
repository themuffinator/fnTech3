/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 fnTech3 contributors
 * Internal decoded-PCM speaker routing. Never used for module RawSamples.
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace fntech3::audio {
inline int WholeFrameReadBytes(int capacity,int width,int channels) {
 if(capacity<=0 || (width!=1 && width!=2) || channels<=0 || channels>capacity/width)return 0;
 const int frameBytes=width*channels;
 return capacity/frameBytes*frameBytes;
}
inline unsigned DefaultSpeakerMask(int channels) {
 switch(channels) {
 case 1:return 0x4; case 2:return 0x3; case 4:return 0x33;
 case 6:return 0x3f; case 7:return 0x70f; case 8:return 0x63f;
 default:return 0;
 }
}
inline bool NeedsSpeakerDownmix(unsigned mask,int channels,bool surround=true) {
 return mask && (mask!=DefaultSpeakerMask(channels) || (!surround && channels>2));
}
// Bits/order come from Microsoft's WAVEFORMATEXTENSIBLE contract. Gains are
// engine mixing policy, matching existing center/LFE/surround stereo fallbacks.
// Reserved positions and channels without a mask bit contribute no output.
inline void SpeakerStereoGains(unsigned bit,float &left,float &right) {
 left=right=0;
 switch(bit) {
 case 0:left=1;break; case 1:right=1;break;
 case 2:left=right=0.7071f;break; case 3:left=right=0.25f;break;
 case 4:case 9:case 12:case 15:left=0.5f;break;
 case 5:case 10:case 14:case 17:right=0.5f;break;
 case 6:left=0.7071f;break; case 7:right=0.7071f;break;
 case 8:case 11:case 13:case 16:left=right=0.35f;break;
 default:break;
 }
}
inline std::vector<short> DownmixSpeakers(const unsigned char *data,int frames,int width,int channels,unsigned mask) {
 if(!data || frames<=0 || channels<=0 || !mask || (width!=1 && width!=2))return {};
 // Consumers store decoded byte sizes in signed int.
 if(frames>std::numeric_limits<int>::max()/4)return {};
 const auto count=static_cast<std::size_t>(frames);
 if(count>std::numeric_limits<std::size_t>::max()/2/sizeof(short) ||
    count>std::numeric_limits<std::size_t>::max()/static_cast<std::size_t>(channels)/width)return {};
 std::vector<short> out(count*2);
 for(int frame=0;frame<frames;++frame) {
  float left=0,right=0;int channel=0;
  for(unsigned bit=0;bit<32 && channel<channels;++bit) {
   if(!(mask & (std::uint32_t{1}<<bit)))continue;
   const auto index=static_cast<std::size_t>(frame)*channels+channel++;
   float sample;
   if(width==2){short value;std::memcpy(&value,data+index*2,2);sample=value;}
   else sample=(channels==1 ? static_cast<int>(data[index])-128 : static_cast<int>(static_cast<signed char>(data[index])))*256.0f;
   float l,r;SpeakerStereoGains(bit,l,r);left+=sample*l;right+=sample*r;
  }
  const auto clamp=[](float value){return static_cast<short>(value<-32768?-32768:value>32767?32767:value);};
  out[static_cast<std::size_t>(frame)*2]=clamp(left);
  out[static_cast<std::size_t>(frame)*2+1]=clamp(right);
 }
 return out;
}
} // namespace fntech3::audio
