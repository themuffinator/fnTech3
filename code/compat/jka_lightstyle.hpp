/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_LIGHTSTYLE_HPP
#define FNTECH3_JKA_LIGHTSTYLE_HPP
#include <array>
#include <cstdint>

/* Retail jamp.exe0x4a31d0/0x4a3200 own 64 four-byte colors, initialized
 * to0xffffffff. Import220's scalar is an x86 little-endian RGBA word.
 * jk2mp.exe's 0x480100/0x480130 (cgame imports 215/216) keep the same
 * table, so Outcast's client uses this store too. Every style, style zero
 * included, takes any word; the client forwards each to the renderer, which
 * recomposes the Raven world's styled light (tr_raven_bsp_load.h). */
namespace fntech3::jka::lightstyle {
inline constexpr int Count=64;
using Color=std::array<unsigned char,4>;
enum class Status {Ok,Index};
class Store {
    std::array<Color,Count> colors_{};
public:
    Store(){Reset();}
    void Reset(){for(auto &color:colors_)color.fill(255);}
    Status Set(int index,uint32_t word){
        if(index<0||index>=Count)return Status::Index;
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
