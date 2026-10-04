/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_GHOUL2_DEFAULT_HPP
#define FNTECH3_GHOUL2_DEFAULT_HPP
#include "../compat/jka_model_format.hpp"
#include <cstdint>
#include <cstring>
#include <vector>

/* Independently authored engine representation of the internal *default
 * selector. Retail jamp.exe at 0x547b98 pins one bone/frame, its name, identity
 * rest/inverse/local transforms and scale 1. This is generated format data,
 * not a copied builtin blob or a filesystem asset. Fn owns padding/layout;
 * the normal bounded GLA reader and pose kernel consume it. */
namespace fntech3::ghoul2::builtin {
inline std::vector<unsigned char> DefaultSkeleton() {
    constexpr std::size_t bone=104,frames=276,pool=284,bytes=298;
    std::vector<unsigned char> out(bytes,0);
    const auto word=[&](std::size_t at,std::uint32_t value){
        for(int n=0;n<4;++n)out[at+n]=static_cast<unsigned char>(value>>(n*8));
    };
    const auto half=[&](std::size_t at,std::uint16_t value){
        out[at]=static_cast<unsigned char>(value);out[at+1]=static_cast<unsigned char>(value>>8);
    };
    word(0,jka::model::SkeletonIdent);word(4,jka::model::SkeletonVersion);
    constexpr char selector[]="*default",name[]="ModView internal default";
    std::memcpy(out.data()+8,selector,sizeof(selector));
    word(72,0x3f800000u);word(76,1);word(80,frames);word(84,1);
    word(88,pool);word(92,bone);word(96,bytes);word(100,bone-jka::model::SkeletonHeaderBytes);
    std::memcpy(out.data()+bone,name,sizeof(name));word(bone+68,UINT32_MAX);
    for(int matrix=0;matrix<2;++matrix)for(int axis=0;axis<3;++axis)
        word(bone+72+matrix*48+axis*20,0x3f800000u);
    // Identity quaternion and translation in the independently frozen codec.
    half(pool,3*16383);for(int n=1;n<4;++n)half(pool+n*2,2*16383);
    for(int n=4;n<7;++n)half(pool+n*2,512*64);
    return out;
}
} // namespace fntech3::ghoul2::builtin
#endif
