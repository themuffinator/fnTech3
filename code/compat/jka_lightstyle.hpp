/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_LIGHTSTYLE_HPP
#define FNTECH3_JKA_LIGHTSTYLE_HPP
#include <array>
#include <cstdint>

/* Retail jamp.exe0x4a31d0/0x4a3200 own 64 four-byte colors, initialized
 * to0xffffffff. Import220's scalar is an x86 little-endian RGBA word.
 * Fn currently projects only style-zero RBSP lighting; other style indices
 * cannot occur in a successfully loaded world. Retain their actual metadata,
 * but refuse colored style zero until the renderers can modulate radiance.
 * Default neutral lighting continues through Fn's existing feature paths. */
namespace fntech3::jka::lightstyle {
inline constexpr int Count=64;
using Color=std::array<unsigned char,4>;
enum class Status {Ok,Index,Modulation};
class Store {
    std::array<Color,Count> colors_{};
public:
    Store(){Reset();}
    void Reset(){for(auto &color:colors_)color.fill(255);}
    Status Set(int index,uint32_t word){
        if(index<0||index>=Count)return Status::Index;
        if(index==0&&(word&UINT32_C(0x00ffffff))!=UINT32_C(0x00ffffff))
            return Status::Modulation;
        const Color value{{static_cast<unsigned char>(word),static_cast<unsigned char>(word>>8),
            static_cast<unsigned char>(word>>16),static_cast<unsigned char>(word>>24)}};
        colors_[index]=value;
        return Status::Ok;
    }
    bool Get(int index,Color &out)const{
        if(index<0||index>=Count)return false;
        out=colors_[index];return true;
    }
};
} // namespace fntech3::jka::lightstyle
#endif
