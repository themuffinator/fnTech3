/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 fnTech3 contributors
 * Vorbis stream order -> internal WAVE speaker-bit order. Facts from Xiph's
 * https://xiph.org/vorbis/doc/vorbisfile/ov_read.html; independent implementation.
 */
#pragma once
#include <cstring>
#include <string>

namespace fntech3::audio {
// Preserve the engine's existing filename-selected sound-field conventions.
// They describe encoded components, not ordinary Vorbis speaker positions.
inline bool VorbisHasEncodedFieldTag(const char *filename,int channels) {
 if(!filename || channels<2 || channels>4)return false;
 std::string name(filename);
 for(char &c:name)if(c>='A' && c<='Z')c=static_cast<char>(c-'A'+'a');
 const char *tags[]{"uhj","uhj2","uhj2ch","uhj2chn","uhj3","uhj3ch","uhj3chn",
  "uhj4","uhj4ch","uhj4chn","bformat2d","b-format2d","bformat-2d","b-format-2d",
  "ambi2d","ambisonic2d","bformat3d","b-format3d","bformat-3d","b-format-3d",
  "ambi3d","ambisonic3d","bformat","b-format","ambisonic"};
 const auto boundary=[](char c){return !c || c=='/' || c=='\\' || c=='.' || c=='_' || c=='-' || c==' ' || c=='+';};
 for(const char *tag:tags){
  const auto length=std::strlen(tag);
  for(auto at=name.find(tag);at!=std::string::npos;at=name.find(tag,at+1))
   if(boundary(at?name[at-1]:0) && boundary(at+length<name.size()?name[at+length]:0))return true;
 }
 return false;
}

inline unsigned VorbisSpeakerMask(int channels) {
 constexpr unsigned masks[]{0,0x4,0x3,0x7,0x33,0x37,0x3f,0x70f,0x63f};
 return channels>=1 && channels<=8 ? masks[channels] : 0;
}

inline void ReorderVorbisFrames(void *buffer,int bytes,int channels) {
 if(!buffer || bytes<=0 || channels<3 || channels>8 || channels==4)return;
 // Each destination speaker selects its original Vorbis channel. Copy bytes
 // so the decoder's requested native endianness and unaligned buffers survive.
 constexpr unsigned char source[9][8]{
  {},{0},{0,1},{0,2,1},{0,1,2,3},{0,2,1,3,4},
  {0,2,1,5,3,4},{0,2,1,6,5,3,4},{0,2,1,7,5,6,3,4}
 };
 auto *data=static_cast<unsigned char *>(buffer);
 const int frameBytes=channels*2;
 for(int frame=0;frame<bytes/frameBytes;++frame){
  unsigned char original[16];std::memcpy(original,data,frameBytes);
  for(int channel=0;channel<channels;++channel)
   std::memcpy(data+channel*2,original+source[channels][channel]*2,2);
  data+=frameBytes;
 }
}
} // namespace fntech3::audio
