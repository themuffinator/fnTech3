/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_EFFECT_PLAY_HPP
#define FNTECH3_JKA_EFFECT_PLAY_HPP
#include "jka_effects.hpp"
#include <array>

/* Independently authored declaration adapter. Retail constructor
 * 0x40c650..0x40ca7e pins life50ms, count1, delay/motion/flags0 and color/
 * alpha/size/length endpoints1. Named fields and flag meanings have separate
 * pinned OpenJK format observations. Fn owns geometry quality/presentation.
 * This first subset is one shader, one immediate spawn, no motion and
 * constant curve controls. No unsupported field or primitive is discarded. */
namespace fntech3::jka::effectplay {
using effects::Kind;
struct Range { float first{},last{}; };
using Vector=std::array<float,3>;
using Axis=std::array<Vector,3>;
struct Spec {
    Kind kind{};
    int shader{},life{50};
    std::array<Range,3> origin{};
    std::array<Range,4> color{{{1,1},{1,1},{1,1},{1,1}}};
    Range size{1,1},size2{1,1},length{1,1},rotation{};
};
struct Spawn {
    Kind kind{};int shader{},life{50};
    Vector origin{};Axis axis{};
    std::array<unsigned char,4> color{};
    float size{},size2{},length{},rotation{};
};
struct Failure { const char *reason{"unsupported effects playback declaration"};const char *field{""}; };
inline bool Finite(float value) noexcept {
    std::uint32_t bits;std::memcpy(&bits,&value,4);volatile std::uint32_t word=bits;
    return (word&0x7f800000u)!=0x7f800000u;
}
inline bool Finite(const Vector &vector) noexcept {
    for(float value:vector)if(!Finite(value))return false;return true;
}
inline bool Refuse(Failure &failure,const char *field,const char *reason) noexcept {
    failure={reason,field};return false;
}
inline Range Scalar(const effects::Numeric &value) {
    return {static_cast<float>(value.values[0]),static_cast<float>(value.values[value.values.size()==2?1:0])};
}
inline bool Bounded(Range value,float low,float high) noexcept {
    return Finite(value.first)&&Finite(value.last)&&value.first>=low&&value.first<=high&&value.last>=low&&value.last<=high;
}
inline bool Compile(const effects::Primitive &primitive,Spec &output,Failure &failure) {
    Spec spec;spec.kind=primitive.kind;
    if(spec.kind!=Kind::Particle&&spec.kind!=Kind::OrientedParticle&&spec.kind!=Kind::Cylinder)
        return Refuse(failure,primitive.declaration.name.c_str(),"effects primitive family is not served yet");
    if(primitive.media.size()!=1||primitive.media[0].kind!=effects::MediaKind::Shader||primitive.media[0].handle<0)
        return Refuse(failure,primitive.declaration.name.c_str(),"effects playback requires one registered Fn shader");
    spec.shader=primitive.media[0].handle;
    for(const auto &pair:primitive.declaration.pairs){
        const auto key=effects::Lower(pair.name);
        if(key!="name"&&key!="shader"&&key!="shaders"&&key!="origin"&&key!="count"&&key!="life"&&key!="delay"&&key!="rotation")
            return Refuse(failure,pair.name.c_str(),"effects field is outside the no-motion single-spawn subset");
    }
    for(const auto &group:primitive.declaration.groups){
        const auto key=effects::Lower(group.name);
        if(key!="rgb"&&key!="alpha"&&key!="size"&&key!="size2"&&key!="length")
            return Refuse(failure,group.name.c_str(),"effects curve family is not served yet");
        if(!group.groups.empty())return Refuse(failure,group.name.c_str(),"nested effects playback controls are not served yet");
        for(const auto &pair:group.pairs){
            const auto name=effects::Lower(pair.name);
            if(name!="start"&&name!="end")return Refuse(failure,pair.name.c_str(),"effects curve controls are not served yet");
        }
    }
    for(const auto &number:primitive.numbers){
        const auto key=effects::Lower(number.name);
        if(key=="origin"||key=="rgb/start"||key=="rgb/end"){
            if(number.values.size()!=3&&number.values.size()!=6)return Refuse(failure,number.name.c_str(),"invalid effects vector range");
            for(int n=0;n<3;++n){
                const Range range{static_cast<float>(number.values[n]),static_cast<float>(number.values[number.values.size()==6?n+3:n])};
                if(!Bounded(range,key=="origin"?-16384.0f:0.0f,key=="origin"?16384.0f:1.0f))
                    return Refuse(failure,number.name.c_str(),"effects vector range exceeds the Fn bound");
                if(key=="origin")spec.origin[n]=range;else if(key=="rgb/start")spec.color[n]=range;
            }
            continue;
        }
        if(number.values.size()!=1&&number.values.size()!=2)return Refuse(failure,number.name.c_str(),"invalid effects scalar range");
        const auto value=Scalar(number);
        if(key=="count"||key=="delay"||key=="life"){
            if(!Finite(value.first)||value.first!=value.last||
                (key=="count"&&value.first!=1)||(key=="delay"&&value.first!=0)||
                (key=="life"&&(value.first<1||value.first>60000||value.first!=std::trunc(value.first))))
                return Refuse(failure,number.name.c_str(),"effects spawn count, delay or lifetime is not served yet");
            if(key=="life")spec.life=static_cast<int>(value.first);continue;
        }
        if(key=="rotation"){
            if(!Bounded(value,-36000,36000))return Refuse(failure,number.name.c_str(),"effects rotation exceeds the Fn bound");
            spec.rotation=value;continue;
        }
        if(key=="alpha/start"||key=="alpha/end"){
            if(!Bounded(value,0,1))return Refuse(failure,number.name.c_str(),"effects alpha exceeds the Fn bound");
            if(key=="alpha/start")spec.color[3]=value;continue;
        }
        if(key=="size/start"||key=="size/end"||key=="size2/start"||key=="size2/end"||key=="length/start"||key=="length/end"){
            if(!Bounded(value,0,16384))return Refuse(failure,number.name.c_str(),"effects dimensions exceed the Fn bound");
            if(key=="size/start")spec.size=value;
            if(key=="size2/start")spec.size2=value;
            if(key=="length/start")spec.length=value;continue;
        }
        return Refuse(failure,number.name.c_str(),"effects numeric field is not served yet");
    }
    output=spec;return true;
}
inline bool Basis(const Vector &forward,Axis &axis) noexcept {
    if(!Finite(forward))return false;
    double squared=0;for(float value:forward)squared+=double(value)*value;
    if(squared<1e-12||!std::isfinite(squared))return false;
    const double length=std::sqrt(squared);
    for(int n=0;n<3;++n)axis[0][n]=static_cast<float>(forward[n]/length);
    int least=0;for(int n=1;n<3;++n)if(std::fabs(axis[0][n])<std::fabs(axis[0][least]))least=n;
    axis[1]={};axis[1][least]=1;const float dot=axis[0][least];
    squared=0;for(int n=0;n<3;++n){axis[1][n]-=dot*axis[0][n];squared+=double(axis[1][n])*axis[1][n];}
    const double side=std::sqrt(squared);if(side<1e-6)return false;
    for(float &value:axis[1])value=static_cast<float>(value/side);
    axis[2]={axis[0][1]*axis[1][2]-axis[0][2]*axis[1][1],axis[0][2]*axis[1][0]-axis[0][0]*axis[1][2],axis[0][0]*axis[1][1]-axis[0][1]*axis[1][0]};
    return Finite(axis[0])&&Finite(axis[1])&&Finite(axis[2]);
}
inline float Pick(Range value,std::uint32_t &random) noexcept {
    random=random*1664525u+1013904223u;
    const double fraction=double(random>>8)/16777216.0;
    return static_cast<float>(double(value.first)+(double(value.last)-value.first)*fraction);
}
inline bool Instantiate(const Spec &spec,const Vector &origin,const Axis &axis,std::uint32_t &random,Spawn &output) noexcept {
    if(spec.shader<0||spec.life<1||spec.life>60000||
        (spec.kind!=Kind::Particle&&spec.kind!=Kind::OrientedParticle&&spec.kind!=Kind::Cylinder))return false;
    if(!Finite(origin))return false;for(const auto &row:axis)if(!Finite(row))return false;
    Spawn spawn;spawn.kind=spec.kind;spawn.shader=spec.shader;spawn.life=spec.life;spawn.axis=axis;spawn.origin=origin;
    for(int n=0;n<3;++n){const float offset=Pick(spec.origin[n],random);for(int component=0;component<3;++component)spawn.origin[component]+=axis[n][component]*offset;}
    for(int n=0;n<4;++n){const float value=Pick(spec.color[n],random);if(!Finite(value)||value<0||value>1)return false;
        spawn.color[n]=static_cast<unsigned char>(value*255.0f);}
    // All served controls have flags0. Retail0x4025e1..0x402630 treats its
    // alpha as additive RGB intensity; explicit alpha-channel controls are
    // outside this subset. Fn quantizes sampled colors at its byte boundary.
    const unsigned intensity=spawn.color[3];
    for(int n=0;n<3;++n)spawn.color[n]=static_cast<unsigned char>((unsigned(spawn.color[n])*intensity)>>8);
    spawn.color[3]=255;
    spawn.size=Pick(spec.size,random);spawn.size2=Pick(spec.size2,random);spawn.length=Pick(spec.length,random);spawn.rotation=Pick(spec.rotation,random);
    if(!Finite(spawn.origin))return false;
    for(float value:spawn.origin)if(std::fabs(value)>10000000.0f)return false;
    if(!Finite(spawn.size)||!Finite(spawn.size2)||!Finite(spawn.length)||!Finite(spawn.rotation))return false;
    output=spawn;return true;
}
} // namespace fntech3::jka::effectplay
#endif
