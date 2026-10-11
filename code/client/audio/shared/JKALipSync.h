/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_JKA_LIPSYNC_H
#define FNTECH3_AUDIO_JKA_LIPSYNC_H

#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <vector>

// Independently measured in jamp.exe 1.01: legacy 0x46bb00, PCM
// preprocessing 0x472b80, MPEG preprocessing 0x46d560 and AL update 0x46d620.
// These are facial animation values, never a replacement for Fn audio gains.
namespace fntech3::audio::jka::lipsync {
constexpr int Rate = 22050;
constexpr int Entities = 1024;
inline float TaggedPeak(const unsigned char *data,int length) {
    if(!data||length<128)return 128.0f;
    const auto *tag=data+length-128;
    if(std::memcmp(tag,"TAG",3)||std::memcmp(tag+97,"#MAXVOL",7))return 128.0f;
    char number[25]{};std::memcpy(number,tag+104,24);
    return static_cast<float>(std::strtod(number,nullptr));
}
inline bool StreamedMpeg(const char *name,int encodedBytes,int frames,int rate,int overhead) {
    constexpr char prefix[]="sound/chars/";
    if(!name||encodedBytes<=0||frames<=0||rate<=0)return false;
    for(int i=0;prefix[i];++i) {
        const char c=name[i];
        if((c>='A'&&c<='Z'?c+('a'-'A'):c)!=prefix[i])return false;
    }
    // The cache decision uses native-rate mono decoded bytes; only the
    // subsequent playback/lip-sync buffer is resampled (0x473800..0x473832).
    const auto unpacked=std::int64_t(frames)*2;
    return std::int64_t(encodedBytes)+overhead<unpacked+2304;
}
struct Thresholds { float closed=0.5f, narrow=4.0f, wide=7.0f, widest=8.0f; };
inline bool Finite(float value) {
    std::uint32_t bits; std::memcpy(&bits,&value,sizeof(bits));
    volatile std::uint32_t encoded=bits;
    return (encoded & 0x7f800000u) != 0x7f800000u;
}
inline bool Valid(const Thresholds &t) {
    return Finite(t.closed)&&Finite(t.narrow)&&Finite(t.wide)&&Finite(t.widest);
}
inline int Level(int energy,float peak,const Thresholds &t) {
    // Retail's x87 multiplies the two binary32 operands before comparing.
    if (double(t.closed)*double(peak)>energy) return -1;
    if (double(t.narrow)*double(peak)>energy) return 1;
    if (double(t.wide)*double(peak)>energy) return 2;
    return double(t.widest)*double(peak)>energy ? 3 : 4;
}
inline int EnergySample(short value) { const int byte=int(value)>>8; return byte*byte; }
template<class SampleAt>float Peak(SampleAt at,int length) {
    int maximum=0;
    for(int i=0;i<length;++i) {
        const int value=at(i), magnitude=(value<0?-value:value)>>8;
        if(magnitude>maximum) maximum=magnitude;
    }
    return float(maximum);
}
template<class SampleAt>int Legacy(SampleAt at,int offset,int length,bool mpeg,float peak,const Thresholds &t) {
    if(offset<0||offset>length) return 0;
    int sum=0,count=0;
    for(int i=0;i<10&&std::int64_t(offset)+i*100<=length;++i) {
        // Retail's compressed sliding window steps 100 bytes, not samples.
        sum+=EnergySample(at(offset+i*(mpeg?50:100)));++count;
    }
    return count ? Level(sum/count,peak,t) : 0;
}
class Mouths {
    std::array<int,Entities> levels_{},remembered_{};
    std::int64_t next_=0;
public:
    void Clear(){levels_.fill(0);}
    void Reset(){Clear();remembered_.fill(0);next_=0;}
    void Write(int entity,int value){if(entity>=0&&entity<Entities)levels_[entity]=value;}
    int Read(int entity)const{return entity>=0&&entity<Entities?levels_[entity]:0;}
    template<class Measure>int MeasureLegacy(int entity,int start,int position,int clock,Measure measure) {
        if(entity<0||entity>=Entities)return 0;
        if(start==position||next_<clock) {
            const int value=measure();
            if(value)remembered_[entity]=value;
            return value;
        }
        return remembered_[entity];
    }
    void Mixed(int clock){if(next_<clock)next_=std::int64_t(clock)+800;}
};
class Envelope {
    std::vector<std::int8_t> levels_;
    int frames_=0,block_=1000;
public:
    void Clear(){std::vector<std::int8_t>().swap(levels_);frames_=0;}
    bool Ready()const{return frames_>0&&!levels_.empty();}
    bool Build(const short *pcm,int frames,int channels,int rate,bool mpeg,float peak,const Thresholds &t) {
        Clear();
        if(!pcm||frames<=0||channels!=1||rate<=0||rate>192000||(mpeg&&!Finite(peak))||!Valid(t))return false;
        // Freeze the same binary32 intermediates and 8-bit fractional cursor
        // as retail's analysis resampler (0x471a97..0x471b33).
        volatile float scale=static_cast<float>(rate)/static_cast<float>(Rate);
        volatile float outputCount=static_cast<float>(frames)/scale;
        if(!(outputCount>0)||double(outputCount)>std::numeric_limits<int>::max())return false;
        const int step=static_cast<int>(scale*256.0f);
        frames_=static_cast<int>(outputCount);block_=mpeg?576:1000;
        const auto sample=[&](int at)->short {
            const auto source=(std::int64_t(at)*step)>>8;
            return at>=0&&at<frames_&&source<frames ? pcm[source] : 0;
        };
        if(!mpeg)peak=Peak(sample,frames_);
        levels_.reserve((std::size_t(frames_)+block_-1)/block_);
        for(int begin=0;begin<frames_;) {
            const int remaining=frames_-begin;
            int sum=0,counted=0;
            for(int offset=0;offset<block_&&(mpeg||offset<remaining);offset+=100) {
                sum+=EnergySample(offset<remaining?sample(begin+offset):0);++counted;
            }
            // PCM's final partial bin uses one fewer sample as its divisor;
            // a bin containing only its first sample measures zero (0x472c72).
            const int divisor=mpeg?6:(remaining>900?10:counted-1);
            levels_.push_back(static_cast<std::int8_t>(Level(divisor>0?sum/divisor:0,peak,t)));
            if(remaining<=block_)break;
            begin+=block_;
        }
        return true;
    }
    int AtSample(std::uint64_t sample)const {
        return sample<std::uint64_t(frames_) ? levels_[std::size_t(sample/block_)] : 0;
    }
    int AtMilliseconds(std::uint32_t elapsed)const { return AtSample(std::uint64_t(elapsed)*Rate/1000); }
};
} // namespace fntech3::audio::jka::lipsync
#endif
