/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_EFFECT_PLAY_HPP
#define FNTECH3_JKA_EFFECT_PLAY_HPP
#include "jka_effects.hpp"
#include <array>
#include <cstdlib>

/* Independently authored declaration adapter. Retail constructor
 * 0x40c650..0x40ca7e pins life50ms, count1, delay/motion/flags0 and color/
 * alpha/size/length endpoints1. Named fields and flag meanings have separate
 * pinned OpenJK format observations. Fn owns geometry quality/presentation.
 * Visual families take one shader and no motion. Lines also serve traced
 * endpoints and a wave size curve. Emitters, Lights, FxRunners and
 * Electricity are measured from retail jamp.exe 1.01's CreateEffect
 * (0x407c40), its primitive classes and the scheduler's count rule; see
 * docs/fntech3/2026-10-15-jka-spawn-effect.md. Shader lists, particle
 * motion, curves and delays follow retail's media pick (0x405d50), CParticle
 * (0x401730..0x40268e), FX_AddParticle (0x40f480) and the scheduler
 * (0x4073c6..0x40755d, 0x407810); see
 * docs/fntech3/2026-10-15-jka-particle-dynamics.md. No unsupported field or
 * primitive is discarded. */
namespace fntech3::jka::effectplay {
using effects::Kind;
struct Range { float first{},last{}; };
using Vector=std::array<float,3>;
using Axis=std::array<Vector,3>;
// Retail flag words: flags at template+0x98 (0x40d2ad..0x40d4b7), spawn
// flags at +0x9c (0x40d5ed..0x40d78e), each word matched without case.
namespace flag {
constexpr std::uint32_t UseModel=0x1000000u,UseBBox=0x4000000u,UsePhysics=0x2000000u,
    ExpensivePhysics=0x800000u,ImpactKills=0x40000000u,EmitFx=0x10000000u,UseAlpha=0x8000000u;
constexpr std::uint32_t Org2FromTrace=0x10u,Org2IsOffset=0x40u,EvenDistribution=0x2000u,ComponentInterpolation=0x4000u;
// Curve words (0x40cb10) OR into one nibble per curve: alpha bits 0..3, rgb
// 4..7, size 8..11 (0x402040, 0x402390, 0x401e50).
constexpr std::uint32_t CurveLinear=1u,CurveRandom=2u,CurveParm=0xcu,CurveNonlinear=4u,CurveWave=8u,CurveClamp=0xcu;
constexpr int AlphaShift=0,RgbShift=4,SizeShift=8;
}
constexpr int MaxCount=100;           // Fn bound; retail has none.
constexpr float MaxDelay=60000.0f;    // Fn bound on a spawn delay, ms.
constexpr float TraceDistance=16384.0f; // 0x5664cc
constexpr int EmitterTick=12;         // 0x403a00: emission walk step, ms.
constexpr int BoltSegments=16;        // Fn's procedural bolt resolution.
struct Spec {
    Kind kind{};
    int shader{};
    Range life{50,50},count{1,1};
    std::array<Range,3> origin{};
    std::array<Range,3> endpoint{};
    std::array<Range,3> velocity{};
    std::array<Range,4> color{{{1,1},{1,1},{1,1},{1,1}}};
    Range size{1,1},size2{1,1},length{1,1},rotation{};
    Range sizeEnd{1,1},sizeParm{};
    Range density{},variance{},chaos{};
    std::uint32_t flags{},spawnFlags{};
    std::vector<int> children;
    // Every handle of a visual primitive's list; one is picked per spawn.
    std::vector<int> shaders;
    Range delay{};
    // Particle dynamics: template +0x15c acceleration, +0x174 gravity,
    // +0x10c rotationDelta, curve ends and parms, and the curve nibbles.
    std::array<Range,3> acceleration{};
    std::array<Range,3> colorEnd{};
    Range gravity{},rotationDelta{},alphaEnd{},rgbParm{},alphaParm{};
    std::uint32_t curves{};
    bool traceEnd{},offsetEnd{},sizeWave{},sizeLinear{};
};
struct Spawn {
    Kind kind{};int shader{},life{50};
    Vector origin{};Axis axis{};
    Vector endpoint{};
    std::array<unsigned char,4> color{};
    std::array<float,3> light{};
    float size{},size2{},length{},rotation{};
    float sizeEnd{},sizeRate{};
    // Emitter state, retail CEmitter 0x403c30/0x4038f0/0x401a50.
    Vector velocity{},base{},baseVelocity{};
    float density{},variance{},chaos{};
    int child{},previousTime{};
    std::uint32_t flags{},seed{};
    // Particle state, retail CParticle: velocity above plus acceleration,
    // the decaying rotation step, both colour ends, alpha ends and the
    // drawn (unconverted) curve parms.
    Vector acceleration{};
    std::array<float,3> rgbStart{},rgbEnd{};
    float alphaStart{},alphaEnd{},rotationDelta{};
    float rgbParm{},alphaParm{},sizeParm{};
    std::uint32_t curves{};
    bool traceEnd{},sizeWave{},sizeLinear{};
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
inline bool Served(Kind kind) noexcept {
    return kind==Kind::Particle||kind==Kind::OrientedParticle||kind==Kind::Cylinder||kind==Kind::Line||
        kind==Kind::Electricity||kind==Kind::Emitter||kind==Kind::Light||kind==Kind::Runner;
}
inline bool Visual(Kind kind) noexcept {
    return kind==Kind::Particle||kind==Kind::OrientedParticle||kind==Kind::Cylinder||kind==Kind::Line||kind==Kind::Electricity;
}
// Every word retail names, so an unknown word is distinguished from a known
// one this subset does not serve. Retail reads at most seven words.
inline bool FlagWords(const std::string &text,bool spawn,std::uint32_t &bits) {
    static constexpr std::pair<const char *,std::uint32_t> flags[]{{"usemodel",0x1000000u},{"usebbox",0x4000000u},
        {"usephysics",0x2000000u},{"expensivephysics",0x800000u},{"ghoul2collision",0x2820000u},
        {"ghoul2decals",0x40000u},{"impactkills",0x40000000u},{"impactfx",0x80000000u},{"deathfx",0x20000000u},
        {"usealpha",0x8000000u},{"emitfx",0x10000000u},{"depthhack",0x100000u},{"relative",0x200000u},
        {"setshadertime",0x400000u},{"paperphysics",0x10000u},{"localizedflash",0x10000u},{"playerview",0x10000u}};
    static constexpr std::pair<const char *,std::uint32_t> spawns[]{{"org2fromtrace",0x10u},{"traceimpactfx",0x20u},
        {"org2isoffset",0x40u},{"cheaporgcalc",0x100u},{"cheaporg2calc",0x200u},{"absolutevel",0x400u},
        {"absoluteaccel",0x800u},{"orgonsphere",1u},{"orgoncylinder",4u},{"axisfromsphere",2u},
        {"randrotaroundfwd",0x1000u},{"evendistribution",0x2000u},{"rgbcomponentinterpolation",0x4000u},
        {"affectedbywind",0x10000u}};
    std::istringstream input(text);std::string word;bits=0;
    for(int n=0;n<7&&input>>word;++n){
        word=effects::Lower(word);bool known=false;
        if(spawn){for(const auto &entry:spawns)if(word==entry.first){bits|=entry.second;known=true;}}
        else for(const auto &entry:flags)if(word==entry.first){bits|=entry.second;known=true;}
        if(!known)return false;
    }
    return true;
}
// Curve flags (0x40cb10) read at most four words and OR them, so
// "linear nonlinear" is 5 and "nonlinear wave" is clamp.
inline bool CurveWords(const std::string &text,std::uint32_t &bits) {
    static constexpr std::pair<const char *,std::uint32_t> words[]{{"linear",flag::CurveLinear},
        {"nonlinear",flag::CurveNonlinear},{"wave",flag::CurveWave},{"random",flag::CurveRandom},{"clamp",flag::CurveClamp}};
    std::istringstream input(text);std::string word;bits=0;
    for(int n=0;n<4&&input>>word;++n){
        word=effects::Lower(word);bool known=false;
        for(const auto &entry:words)if(word==entry.first){bits|=entry.second;known=true;}
        if(!known)return false;
    }
    return true;
}
inline bool Compile(const effects::Primitive &primitive,Spec &output,Failure &failure) {
    Spec spec;spec.kind=primitive.kind;
    const char *const name=primitive.declaration.name.c_str();
    if(!Served(spec.kind))return Refuse(failure,name,"effects primitive family is not served yet");
    const bool visual=Visual(spec.kind),segment=spec.kind==Kind::Line||spec.kind==Kind::Electricity;
    const bool emitter=spec.kind==Kind::Emitter,light=spec.kind==Kind::Light,runner=spec.kind==Kind::Runner;
    const bool particle=spec.kind==Kind::Particle;
    if(visual){
        // A shader list: retail picks one handle per spawn (0x405d50 over
        // the template's list at +0x48).
        for(const auto &media:primitive.media){
            if(media.kind!=effects::MediaKind::Shader||media.handle<0)
                return Refuse(failure,name,"effects playback requires registered Fn shaders");
            spec.shaders.push_back(media.handle);
        }
        if(spec.shaders.empty())return Refuse(failure,name,"effects playback requires registered Fn shaders");
        spec.shader=spec.shaders.front();
    }else for(const auto &media:primitive.media){
        // Retail reads one list per class: Emitter emitFx (+0x78), FxRunner playFx (+0x88).
        const bool wanted=(emitter&&media.kind==effects::MediaKind::Emit)||(runner&&media.kind==effects::MediaKind::Play);
        if(!wanted||media.handle<0)return Refuse(failure,name,"effects media is not served for this primitive family");
        spec.children.push_back(media.handle);
    }
    for(const auto &pair:primitive.declaration.pairs){
        const auto key=effects::Lower(pair.name);
        if(key=="name"||key=="shader"||key=="shaders"||key=="emitfx"||key=="playfx"||key=="origin"||
            key=="count"||key=="life"||key=="delay")continue;
        if(visual&&key=="rotation")continue;
        // Retail parses cullrange and stores nothing (0x40eab2 continues the
        // key loop), so PlayEffect's range test (0x40721b) never applies.
        if(key=="cullrange")continue;
        if(particle&&(key=="velocity"||key=="vel"||key=="acceleration"||key=="accel"||key=="gravity"||key=="rotationdelta"))continue;
        if(segment&&key=="origin2")continue;
        if(spec.kind==Kind::Electricity&&key=="intensity")continue;
        if(emitter&&(key=="velocity"||key=="vel"||key=="density"||key=="variance"))continue;
        if(key=="flags"||key=="spawnflags"){
            const bool spawn=key=="spawnflags";std::uint32_t bits=0;
            if(pair.values.size()!=1||!FlagWords(pair.values[0],spawn,bits))
                return Refuse(failure,pair.name.c_str(),"unknown effects flag word");
            (spawn?spec.spawnFlags:spec.flags)|=bits;continue;
        }
        return Refuse(failure,pair.name.c_str(),"effects field is outside the served playback subset");
    }
    // Spawn flags: org2fromTrace, with org2isOffset, serve the shared Line and
    // Electricity endpoint path (0x408f39..0x4093e2); evenDistribution
    // (0x4073c6) and rgbComponentInterpolation (0x406de0) serve every family.
    // Flags per class below.
    if(spec.spawnFlags&~((segment?flag::Org2FromTrace|flag::Org2IsOffset:0u)|flag::EvenDistribution|flag::ComponentInterpolation))
        return Refuse(failure,name,"effects spawn controls are not served yet");
    if((spec.spawnFlags&flag::Org2IsOffset)&&!(spec.spawnFlags&flag::Org2FromTrace))
        return Refuse(failure,name,"effects untraced endpoint offsets are not served yet");
    spec.traceEnd=spec.spawnFlags&flag::Org2FromTrace;spec.offsetEnd=spec.spawnFlags&flag::Org2IsOffset;
    // Retail's "intensity" (and "bounce") parser ORs usePhysics into the
    // flags (0x40cdca); for Electricity that word means forked bolts.
    if(spec.kind==Kind::Electricity)for(const auto &number:primitive.numbers)
        if(effects::Lower(number.name)=="intensity")spec.flags|=flag::UsePhysics;
    const std::uint32_t allowed=spec.kind==Kind::Electricity?flag::UseModel|flag::UseBBox|flag::UsePhysics:
        emitter?flag::UsePhysics|flag::ExpensivePhysics|flag::ImpactKills|flag::EmitFx:particle?flag::UseAlpha:0u;
    if(spec.flags&~allowed)return Refuse(failure,name,"effects flags are not served yet");
    // Retail's cheap probe (cgame export 9) and bounce (0x401c34) are not served.
    if(emitter&&(spec.flags&flag::UsePhysics)&&(spec.flags&(flag::ExpensivePhysics|flag::ImpactKills))!=(flag::ExpensivePhysics|flag::ImpactKills))
        return Refuse(failure,name,"effects emitter physics needs expensivePhysics and impactKills");
    for(const auto &group:primitive.declaration.groups){
        const auto key=effects::Lower(group.name);
        const bool curve=key=="rgb"||key=="alpha"||key=="size"||key=="size2"||key=="length";
        if(!curve||emitter||runner||(light&&key!="rgb"&&key!="size"&&key!="alpha")||
            (spec.kind==Kind::Electricity&&key!="rgb"&&key!="size"&&key!="alpha"))
            return Refuse(failure,group.name.c_str(),"effects curve family is not served yet");
        if(!group.groups.empty())return Refuse(failure,group.name.c_str(),"nested effects playback controls are not served yet");
        for(const auto &pair:group.pairs){
            const auto field=effects::Lower(pair.name);
            // CParticle curves (0x401e50, 0x402040, 0x402390): parm and flags.
            if(particle&&(key=="rgb"||key=="alpha"||key=="size")&&(field=="parm"||field=="flags")){
                if(field=="parm")continue;
                std::uint32_t bits=0;
                if(pair.values.size()!=1||!CurveWords(pair.values[0],bits))
                    return Refuse(failure,pair.name.c_str(),"unknown effects curve flag word");
                if(bits&flag::CurveRandom)return Refuse(failure,pair.name.c_str(),"effects random curves are not served yet");
                spec.curves|=bits<<(key=="alpha"?flag::AlphaShift:key=="rgb"?flag::RgbShift:flag::SizeShift);continue;
            }
            if(spec.kind==Kind::Line&&key=="size"){
                if(field=="parm")continue;
                if(field=="flags"){
                    if(pair.values.size()!=1||effects::Lower(pair.values[0])!="wave")
                        return Refuse(failure,pair.name.c_str(),"effects line size controls are not served yet");
                    spec.sizeWave=true;continue;
                }
            }
            // UpdateSize 0x401e50/0x4040a0: curve flag "linear" (bit 0x100).
            if((light||spec.kind==Kind::Electricity)&&key=="size"&&field=="flags"){
                if(pair.values.size()!=1||effects::Lower(pair.values[0])!="linear")
                    return Refuse(failure,pair.name.c_str(),"effects size controls are not served yet");
                spec.sizeLinear=true;continue;
            }
            if(field!="start"&&field!="end")return Refuse(failure,pair.name.c_str(),"effects curve controls are not served yet");
        }
    }
    for(const auto &number:primitive.numbers){
        const auto key=effects::Lower(number.name);
        if(key=="origin"||key=="origin2"||key=="rgb/start"||key=="rgb/end"||key=="velocity"||key=="vel"||
            key=="acceleration"||key=="accel"){
            if(number.values.size()!=3&&number.values.size()!=6)return Refuse(failure,number.name.c_str(),"invalid effects vector range");
            const bool position=key=="origin"||key=="origin2",acceleration=key=="acceleration"||key=="accel";
            const bool motion=key=="velocity"||key=="vel"||acceleration;
            // Traced offsets reach beyond the world, as retail's spawn bolts do.
            const float bound=key=="origin2"&&spec.offsetEnd?65536.0f:16384.0f;
            for(int n=0;n<3;++n){
                const Range range{static_cast<float>(number.values[n]),static_cast<float>(number.values[number.values.size()==6?n+3:n])};
                if(!Bounded(range,position||motion?-bound:0.0f,position||motion?bound:1.0f))
                    return Refuse(failure,number.name.c_str(),"effects vector range exceeds the Fn bound");
                if(key=="origin")spec.origin[n]=range;
                else if(key=="origin2")spec.endpoint[n]=range;
                else if(acceleration)spec.acceleration[n]=range;
                else if(motion)spec.velocity[n]=range;
                else if(key=="rgb/start")spec.color[n]=range;
                else spec.colorEnd[n]=range;
            }
            continue;
        }
        if(number.values.size()!=1&&number.values.size()!=2)return Refuse(failure,number.name.c_str(),"invalid effects scalar range");
        const auto value=Scalar(number);
        if(key=="count"){
            if(!Bounded(value,0,MaxCount))return Refuse(failure,number.name.c_str(),"effects spawn count exceeds the Fn bound");
            spec.count=value;continue;
        }
        if(key=="delay"){
            if(!Bounded(value,-MaxDelay,MaxDelay))return Refuse(failure,number.name.c_str(),"effects spawn delay exceeds the Fn bound");
            spec.delay=value;continue;
        }
        if(key=="gravity"){
            if(!Bounded(value,-16384,16384))return Refuse(failure,number.name.c_str(),"effects gravity exceeds the Fn bound");
            spec.gravity=value;continue;
        }
        if(key=="rotationdelta"){
            if(!Bounded(value,-36000,36000))return Refuse(failure,number.name.c_str(),"effects rotation step exceeds the Fn bound");
            spec.rotationDelta=value;continue;
        }
        if(key=="life"){
            if(!Bounded(value,1,60000))return Refuse(failure,number.name.c_str(),"effects lifetime is outside the Fn bound");
            spec.life=value;continue;
        }
        if(key=="rotation"){
            if(!Bounded(value,-36000,36000))return Refuse(failure,number.name.c_str(),"effects rotation exceeds the Fn bound");
            spec.rotation=value;continue;
        }
        if(key=="density"||key=="variance"){
            if(!Bounded(value,0,16384))return Refuse(failure,number.name.c_str(),"effects emission spacing exceeds the Fn bound");
            (key=="density"?spec.density:spec.variance)=value;continue;
        }
        if(key=="intensity"){
            if(!Bounded(value,0,64))return Refuse(failure,number.name.c_str(),"effects electricity intensity exceeds the Fn bound");
            spec.chaos=value;continue;
        }
        if(key=="alpha/start"||key=="alpha/end"){
            if(!Bounded(value,0,1))return Refuse(failure,number.name.c_str(),"effects alpha exceeds the Fn bound");
            (key=="alpha/start"?spec.color[3]:spec.alphaEnd)=value;continue;
        }
        if(particle&&(key=="rgb/parm"||key=="alpha/parm"||key=="size/parm")){
            if(!Bounded(value,-100000,100000))return Refuse(failure,number.name.c_str(),"effects curve parm exceeds the Fn bound");
            (key=="rgb/parm"?spec.rgbParm:key=="alpha/parm"?spec.alphaParm:spec.sizeParm)=value;continue;
        }
        if(spec.kind==Kind::Line&&key=="size/parm"){
            if(!Bounded(value,-10000,10000))return Refuse(failure,number.name.c_str(),"effects line wave frequency exceeds the Fn bound");
            spec.sizeParm=value;continue;
        }
        if(key=="size/start"||key=="size/end"||key=="size2/start"||key=="size2/end"||key=="length/start"||key=="length/end"){
            if(!Bounded(value,0,16384))return Refuse(failure,number.name.c_str(),"effects dimensions exceed the Fn bound");
            if(key=="size/start")spec.size=value;
            if(key=="size/end")spec.sizeEnd=value;
            if(key=="size2/start")spec.size2=value;
            if(key=="length/start")spec.length=value;continue;
        }
        return Refuse(failure,number.name.c_str(),"effects numeric field is not served yet");
    }
    output=std::move(spec);return true;
}
inline bool Basis(const Vector &forward,Axis &axis) noexcept {
    if(!Finite(forward))return false;
    double squared=0;for(float value:forward)squared+=double(value)*value;
    // Three squared finite floats fit in double, even at FLT_MAX. The bitwise
    // Finite check above also remains valid when the compiler uses fast math.
    if(squared<1e-12)return false;
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
// An emitter's axis is rebuilt from angles (0x40a867 vectoangles of the play
// axis's forward, 0x403e90 AnglesToAxis): roll is zero whatever the caller's.
inline Vector DirectionAngles(const Vector &forward) noexcept {
    constexpr double degrees=180.0/3.14159265358979323846;
    double yaw=0,pitch;
    if(forward[0]==0&&forward[1]==0)pitch=forward[2]>0?90:270;
    else{
        yaw=std::atan2(double(forward[1]),double(forward[0]))*degrees;if(yaw<0)yaw+=360;
        const double flat=std::sqrt(double(forward[0])*forward[0]+double(forward[1])*forward[1]);
        pitch=std::atan2(double(forward[2]),flat)*degrees;if(pitch<0)pitch+=360;
    }
    return {static_cast<float>(-pitch),static_cast<float>(yaw),0};
}
inline Axis AnglesAxis(const Vector &angles) noexcept {
    constexpr double radians=3.14159265358979323846/180.0;
    const double sp=std::sin(angles[0]*radians),cp=std::cos(angles[0]*radians);
    const double sy=std::sin(angles[1]*radians),cy=std::cos(angles[1]*radians);
    const double sr=std::sin(angles[2]*radians),cr=std::cos(angles[2]*radians);
    Axis axis;
    axis[0]={float(cp*cy),float(cp*sy),float(-sp)};
    // Axis 1 is left: the negated right vector.
    axis[1]={float(sr*sp*cy-cr*sy),float(sr*sp*sy+cr*cy),float(sr*cp)};
    axis[2]={float(cr*sp*cy+sr*sy),float(cr*sp*sy-sr*cy),float(cr*cp)};
    return axis;
}
inline float Pick(Range value,std::uint32_t &random) noexcept {
    random=random*1664525u+1013904223u;
    const double fraction=double(random>>8)/16777216.0;
    return static_cast<float>(double(value.first)+(double(value.last)-value.first)*fraction);
}
inline int PickChild(const std::vector<int> &children,std::uint32_t &random) noexcept {
    if(children.empty())return 0;
    random=random*1664525u+1013904223u;
    return children[std::size_t((std::uint64_t(random>>8)*children.size())>>24)];
}
// Retail scheduler 0x4072b1..0x4073ba: a spread wider than one is scaled by
// fx_countScale, clamped to one; the value rounds half up; a minimum of at
// least one always spawns one.
inline int Count(const Spec &spec,float scale,std::uint32_t &random) noexcept {
    float value=Pick(spec.count,random);
    if(std::fabs(double(spec.count.last-spec.count.first))>1.0)value*=scale>1.0f?1.0f:scale;
    double rounded=std::floor(double(value)+0.5);
    if(!(rounded>=0))rounded=0;
    int count=rounded>MaxCount?MaxCount:static_cast<int>(rounded);
    if(spec.count.first>=1.0f&&count<1)count=1;
    return count;
}
// Late is how long after its due time a spawn is created (retail CreateEffect's
// lateTime: the negated delay below one, or the scheduler's lag).
inline bool Instantiate(const Spec &spec,const Vector &origin,const Axis &axis,std::uint32_t &random,Spawn &output,int late=0) noexcept {
    if(spec.shader<0||!Served(spec.kind)||!Bounded(spec.life,1,60000))return false;
    if(!Finite(origin))return false;for(const auto &row:axis)if(!Finite(row))return false;
    Spawn spawn;spawn.kind=spec.kind;spawn.shader=spec.shader;spawn.axis=axis;spawn.origin=origin;spawn.flags=spec.flags;
    if(spec.shaders.size()>1)spawn.shader=PickChild(spec.shaders,random);
    // Retail truncates the sampled lifetime (_ftol 0x539584).
    spawn.life=static_cast<int>(Pick(spec.life,random));if(spawn.life<1)spawn.life=1;
    for(int n=0;n<3;++n){const float offset=Pick(spec.origin[n],random);for(int component=0;component<3;++component)spawn.origin[component]+=axis[n][component]*offset;}
    // GetRGB (0x406de0): rgbComponentInterpolation draws one fraction for
    // every colour range, so colours lie on the line between the ends.
    const bool blend=spec.spawnFlags&flag::ComponentInterpolation;
    const float fraction=blend?Pick({0,1},random):0.0f;
    const auto colour=[&](Range range){
        return blend?static_cast<float>(double(range.first)+(double(range.last)-range.first)*fraction):Pick(range,random);};
    for(int n=0;n<4;++n){const float value=n<3?colour(spec.color[n]):Pick(spec.color[n],random);if(!Finite(value)||value<0||value>1)return false;
        if(n<3)spawn.light[n]=spawn.rgbStart[n]=value;else spawn.alphaStart=value;
        spawn.color[n]=static_cast<unsigned char>(value*255.0f);}
    // All served controls have flags0. Retail0x4025e1..0x402630 treats its
    // alpha as additive RGB intensity; explicit alpha-channel controls are
    // outside this subset. Fn quantizes sampled colors at its byte boundary.
    // A Light (0x403f50) passes its RGB alone; it has no alpha.
    const unsigned intensity=spawn.color[3];
    for(int n=0;n<3;++n)spawn.color[n]=static_cast<unsigned char>((unsigned(spawn.color[n])*intensity)>>8);
    spawn.color[3]=255;
    spawn.size=Pick(spec.size,random);spawn.size2=Pick(spec.size2,random);spawn.length=Pick(spec.length,random);spawn.rotation=Pick(spec.rotation,random);
    if(spec.kind==Kind::Line||spec.kind==Kind::Electricity){
        spawn.traceEnd=spec.traceEnd;spawn.sizeWave=spec.sizeWave;spawn.endpoint=origin;
        if(spec.traceEnd){
            for(int n=0;n<3;++n)spawn.endpoint[n]=spawn.origin[n]+axis[0][n]*TraceDistance;
            // 0x408f96..0x409317 draws each offset range once per component.
            if(spec.offsetEnd)for(int row=0;row<3;++row)for(int n=0;n<3;++n)
                spawn.endpoint[n]+=Pick(spec.endpoint[row],random)*axis[row][n];
        }else for(int n=0;n<3;++n){
            const float offset=Pick(spec.endpoint[n],random);
            for(int component=0;component<3;++component)spawn.endpoint[component]+=axis[n][component]*offset;
        }
        if(spec.sizeWave){
            spawn.sizeEnd=Pick(spec.sizeEnd,random);
            // Retail0x40fa60 uses this binary32 constant (0x5664d8),
            // based on 3.14159 rather than a higher-precision pi.
            spawn.sizeRate=Pick(spec.sizeParm,random)*0.0031415901612490416f;
            if(!Finite(spawn.sizeEnd)||!Finite(spawn.sizeRate))return false;
        }
        if(!Finite(spawn.endpoint))return false;
        for(float value:spawn.endpoint)if(std::fabs(value)>10000000.0f)return false;
    }
    if(spec.kind==Kind::Electricity||spec.kind==Kind::Light){
        spawn.sizeLinear=spec.sizeLinear;spawn.sizeEnd=Pick(spec.sizeEnd,random);
        spawn.chaos=Pick(spec.chaos,random);
        random=random*1664525u+1013904223u;spawn.seed=random;
    }
    if(spec.kind==Kind::Emitter){
        // 0x408739..0x4089f0: one draw per velocity range and component.
        for(int row=0;row<3;++row)for(int n=0;n<3;++n)spawn.velocity[n]+=Pick(spec.velocity[row],random)*axis[row][n];
        spawn.axis=AnglesAxis(DirectionAngles(axis[0]));
        for(const auto &row:spawn.axis)if(!Finite(row))return false;
        spawn.variance=Pick(spec.variance,random);spawn.density=Pick(spec.density,random);
        if(!Finite(spawn.velocity))return false;
    }
    if(spec.kind==Kind::Particle){
        // CreateEffect 0x40988e and FX_AddParticle 0x40f480: both colour
        // ends, alpha and size ends, the parms, the rotation step, then
        // velocity, acceleration (each range once per component) and gravity
        // on world Z (0x408e30).
        for(int n=0;n<3;++n){spawn.rgbEnd[n]=colour(spec.colorEnd[n]);if(!Finite(spawn.rgbEnd[n])||spawn.rgbEnd[n]<0||spawn.rgbEnd[n]>1)return false;}
        spawn.alphaEnd=Pick(spec.alphaEnd,random);spawn.sizeEnd=Pick(spec.sizeEnd,random);
        spawn.rgbParm=Pick(spec.rgbParm,random);spawn.alphaParm=Pick(spec.alphaParm,random);spawn.sizeParm=Pick(spec.sizeParm,random);
        spawn.rotationDelta=Pick(spec.rotationDelta,random);spawn.curves=spec.curves;
        for(int row=0;row<3;++row)for(int n=0;n<3;++n)spawn.velocity[n]+=Pick(spec.velocity[row],random)*axis[row][n];
        for(int row=0;row<3;++row)for(int n=0;n<3;++n)spawn.acceleration[n]+=Pick(spec.acceleration[row],random)*axis[row][n];
        spawn.acceleration[2]+=Pick(spec.gravity,random);
        if(!Finite(spawn.velocity)||!Finite(spawn.acceleration)||!Finite(spawn.alphaEnd)||!Finite(spawn.sizeEnd)||
            !Finite(spawn.rgbParm)||!Finite(spawn.alphaParm)||!Finite(spawn.sizeParm)||!Finite(spawn.rotationDelta))return false;
    }
    if(late>0&&(spec.kind==Kind::Particle||spec.kind==Kind::Emitter)){
        // 0x408e84..0x408f29: a late spawn takes the lag's acceleration and
        // moves by (t + t*t/2) times its velocity.
        volatile float seconds=static_cast<float>(late)*0.001f;
        volatile float half=seconds*seconds*0.5f;
        for(int n=0;n<3;++n)spawn.velocity[n]+=seconds*spawn.acceleration[n];
        const float step=half+seconds;
        for(int n=0;n<3;++n)spawn.origin[n]+=step*spawn.velocity[n];
        if(!Finite(spawn.velocity))return false;
    }
    if(spec.kind==Kind::Emitter||spec.kind==Kind::Runner)spawn.child=PickChild(spec.children,random);
    if(!Finite(spawn.origin))return false;
    for(float value:spawn.origin)if(std::fabs(value)>10000000.0f)return false;
    if(!Finite(spawn.size)||!Finite(spawn.size2)||!Finite(spawn.length)||!Finite(spawn.rotation)||
        !Finite(spawn.sizeEnd)||!Finite(spawn.chaos)||!Finite(spawn.density)||!Finite(spawn.variance))return false;
    output=spawn;return true;
}
inline float LineRadius(const Spawn &spawn,int age) noexcept {
    if(!spawn.sizeWave)return spawn.size;
    // Retail0x401f45 rounds the phase to binary32 before its double cos.
    volatile float phase=static_cast<float>(age)*spawn.sizeRate;
    const float bias=static_cast<float>(std::cos(static_cast<double>(phase)));
    return spawn.size*bias+spawn.sizeEnd*(1.0f-bias);
}
// UpdateSize 0x401e50 (particles) and 0x4040a0 (lights): linear runs from
// start to end over the lifetime, with binary32 operands.
inline float CurveRadius(const Spawn &spawn,int age) noexcept {
    if(spawn.sizeWave)return LineRadius(spawn,age);
    if(!spawn.sizeLinear)return spawn.size;
    volatile float elapsed=static_cast<float>(age),span=static_cast<float>(spawn.life);
    volatile float bias=1.0f-elapsed/span;
    return spawn.size*bias+spawn.sizeEnd*(1.0f-bias);
}
// CParticle curves (UpdateSize 0x401e50, UpdateRGB 0x402040, UpdateAlpha
// 0x402390) share one law over a nibble of flags, in binary32: linear runs
// from start to end; nonlinear holds until its parm time then runs to the
// end; clamp runs from its parm time back to the start; wave modulates by
// cos(age * parm); linear blends evenly with nonlinear or clamp. The parm is
// converted at creation (0x40f642..0x40f76e): a wave's by 0.0031415901, any
// other's to the absolute time life * parm% after the start.
inline float CurveParm(std::uint32_t nibble,float parm,int start,int life) noexcept {
    if((nibble&flag::CurveParm)==flag::CurveWave)return parm*0.0031415901612490416f;
    if(!(nibble&flag::CurveParm))return parm;
    volatile float span=static_cast<float>(life),origin=static_cast<float>(start);
    volatile float offset=static_cast<float>(double(span)*parm*double(0.01f));
    return offset+origin;
}
inline float CurvePercent(std::uint32_t nibble,float parm,int now,int start,int life) noexcept {
    volatile float first=1.0f,second=1.0f;
    const int end=start+life;
    if(nibble&flag::CurveLinear){
        volatile float elapsed=static_cast<float>(now-start),span=static_cast<float>(end-start);
        first=1.0f-elapsed/span;
    }
    const std::uint32_t mode=nibble&flag::CurveParm;
    if(mode==flag::CurveWave){
        volatile float phase=static_cast<float>(now-start)*parm;
        first=first*static_cast<float>(std::cos(static_cast<double>(phase)));
    }else if(mode){
        volatile float time=static_cast<float>(now);
        if(mode==flag::CurveNonlinear){
            if(time>parm){volatile float finish=static_cast<float>(end);second=1.0f-(time-parm)/(finish-parm);}
        }else{
            volatile float begin=static_cast<float>(start);
            second=time<parm?(parm-time)/(parm-begin):0.0f;
        }
        first=(nibble&flag::CurveLinear)?(second+first)*0.5f:float(second);
    }
    return first;
}
struct Look { float radius{};std::array<unsigned char,4> rgba{}; };
// One frame of a particle's size, colour and alpha. UpdateRGB rounds each
// clamped component times 255 to the nearest byte (fistp) and sets alpha
// 255; UpdateAlpha clamps, truncates (_ftol 0x539584) and either writes the
// alpha byte (useAlpha) or scales RGB by alpha/256.
inline bool ParticleLook(const Spawn &spawn,int now,int start,Look &look) noexcept {
    const auto percent=[&](int shift,float parm){
        const std::uint32_t nibble=(spawn.curves>>shift)&0xfu;
        return CurvePercent(nibble,CurveParm(nibble,parm,start,spawn.life),now,start,spawn.life);};
    const float size=percent(flag::SizeShift,spawn.sizeParm);
    look.radius=size*spawn.size+(1.0f-size)*spawn.sizeEnd;
    const float rgb=percent(flag::RgbShift,spawn.rgbParm);
    for(int n=0;n<3;++n){
        volatile float value=rgb*spawn.rgbStart[n]+(1.0f-rgb)*spawn.rgbEnd[n];
        const float clamped=value<0.0f?0.0f:value>1.0f?1.0f:float(value);
        const double scaled=std::nearbyint(double(clamped*255.0f));
        if(!(scaled>=0&&scaled<=255))return false;
        look.rgba[n]=static_cast<unsigned char>(scaled);
    }
    look.rgba[3]=255;
    const float bias=percent(flag::AlphaShift,spawn.alphaParm);
    volatile float alpha=bias*spawn.alphaStart+(1.0f-bias)*spawn.alphaEnd;
    float level=alpha<0.0f?0.0f:alpha>1.0f?1.0f:float(alpha);
    level*=255.0f;if(!(level>=0))level=0;if(level>255)level=255;
    const unsigned byte=static_cast<unsigned>(level);
    if(spawn.flags&flag::UseAlpha)look.rgba[3]=static_cast<unsigned char>(byte);
    else for(int n=0;n<3;++n)look.rgba[n]=static_cast<unsigned char>((unsigned(look.rgba[n])*byte)>>8);
    return Finite(look.radius);
}
// UpdateRotation 0x402640: the angle steps by frameTime * step / 100 and
// the step decays by frameTime * 0.0007 each update.
inline bool ParticleRotate(Spawn &spawn,int frameTime) noexcept {
    volatile float elapsed=static_cast<float>(frameTime);
    spawn.rotation=static_cast<float>(double(elapsed)*spawn.rotationDelta*double(0.01f)+spawn.rotation);
    spawn.rotationDelta=static_cast<float>((1.0-double(elapsed)*double(0.0007f))*spawn.rotationDelta);
    return Finite(spawn.rotation)&&Finite(spawn.rotationDelta);
}
// UpdateOrigin 0x401a50 without physics: velocity takes this frame's
// acceleration first, then the particle moves by it.
inline bool ParticleNext(Spawn &spawn,float seconds) noexcept {
    for(int n=0;n<3;++n)spawn.velocity[n]=seconds*spawn.acceleration[n]+spawn.velocity[n];
    for(int n=0;n<3;++n)spawn.origin[n]=seconds*spawn.velocity[n]+spawn.origin[n];
    if(!Finite(spawn.origin)||!Finite(spawn.velocity))return false;
    for(float value:spawn.origin)if(std::fabs(value)>10000000.0f)return false;
    return true;
}
// Retail scheduler (0x4073c6..0x4074bb): evenDistribution spaces the count
// over |_ftol(max - min)|, else each spawn draws its delay; both truncate.
inline int Delay(const Spec &spec,int index,int count,std::uint32_t &random) noexcept {
    if(spec.spawnFlags&flag::EvenDistribution){
        const int spread=std::abs(static_cast<int>(spec.delay.last-spec.delay.first));
        volatile float factor=static_cast<float>(spread)/static_cast<float>(count);
        return static_cast<int>(static_cast<float>(index)*factor);
    }
    if(spec.delay.first==spec.delay.last)return static_cast<int>(spec.delay.first);
    return static_cast<int>(Pick(spec.delay,random));
}
// Emitter motion without its physics test (0x401a50: no acceleration control
// is served, so velocity is constant). The caller owns the trace.
inline bool EmitterNext(const Spawn &spawn,float seconds,Vector &next) noexcept {
    for(int n=0;n<3;++n)next[n]=spawn.origin[n]+seconds*spawn.velocity[n];
    if(!Finite(next))return false;
    for(float value:next)if(std::fabs(value)>10000000.0f)return false;
    return true;
}
// Retail CEmitter draw 0x4039a4..0x403c0c. It walks 12ms ticks from the last
// emission, predicting from this frame's start position and velocity
// (including retail's velocity in its half-square term), and emits whenever
// the prediction has moved the squared step since that position.
template<class Emit>
inline bool EmitterWalk(Spawn &spawn,int now,std::uint32_t &random,Emit &&emit) {
    if(spawn.previousTime>now)return true;
    const auto step=[&]{const float spacing=Pick({-spawn.variance,spawn.variance},random)+spawn.density;
        return static_cast<float>(spacing*spacing);};
    float squared=step();int elapsed=0;
    for(std::int64_t time=spawn.previousTime;time<=now;time+=EmitterTick){
        elapsed+=EmitterTick;
        volatile float whole=static_cast<float>(elapsed);
        const float seconds=whole*0.001f,half=seconds*seconds*0.5f;
        const Vector velocity=spawn.baseVelocity;
        Vector position;double distance=0;
        for(int n=0;n<3;++n){
            position[n]=static_cast<float>(double(half)*velocity[n]+double(seconds)*velocity[n]+spawn.base[n]);
            const double delta=double(spawn.base[n])-position[n];distance+=delta*delta;
        }
        if(!Finite(position))return false;
        if(distance<squared)continue;
        squared=step();
        if(!emit(position))return false;
        spawn.base=position;spawn.baseVelocity=velocity;elapsed=0;spawn.previousTime=static_cast<int>(time);
    }
    return true;
}
// Fn's own bolt for Electricity. Retail hands RT_ELECTRICITY to its renderer
// (0x402c10), whose flags come from usePhysics (forks), useModel (taper) and
// useBBox (grow) at 0x402bd5..0x402c05; "intensity" sets usePhysics itself
// (0x40cdca). Fn builds seeded jagged strips in its polygon path instead:
// intensity scales the jag, a grown bolt reaches its end at half life, a
// tapered one narrows to nothing, and a forked one adds two shorter
// branches that grow out of it.
using Bolt=std::array<Vector,BoltSegments+1>;
constexpr int BoltBranches=2,BranchSegments=8;
inline bool Jagged(const Vector &start,const Vector &end,float chaos,std::uint32_t seed,int segments,Bolt &points) noexcept {
    Vector direction;double squared=0;
    for(int n=0;n<3;++n){direction[n]=end[n]-start[n];squared+=double(direction[n])*direction[n];}
    Axis basis;if(segments<1||segments>BoltSegments||squared<1e-6||!Basis(direction,basis))return false;
    const float length=static_cast<float>(std::sqrt(squared));
    const float amplitude=std::min(length/segments,96.0f)*chaos*0.25f;
    for(int point=0;point<=segments;++point){
        const float along=float(point)/segments;
        const bool tip=point==0||point==segments;
        const float a=tip?0.0f:Pick({-1,1},seed)*amplitude,b=tip?0.0f:Pick({-1,1},seed)*amplitude;
        for(int n=0;n<3;++n)points[point][n]=start[n]+direction[n]*along+basis[1][n]*a+basis[2][n]*b;
        if(!Finite(points[point]))return false;
    }
    return true;
}
// Grown length in segments: all of them, or up to half life's share.
inline float BoltReach(const Spawn &spawn,int age) noexcept {
    if(!(spawn.flags&flag::UseBBox))return BoltSegments;
    return std::min(1.0f,2.0f*float(age)/float(spawn.life))*BoltSegments;
}
// Cut a strip at a fractional segment count; returns whole segments to draw.
inline int Truncate(Bolt &points,int segments,float reach) noexcept {
    if(reach>=float(segments))return segments;
    if(!(reach>0))return 0;
    const int whole=static_cast<int>(reach);const float part=reach-float(whole);
    if(part<=0)return whole;
    for(int n=0;n<3;++n)points[whole+1][n]=points[whole][n]+(points[whole+1][n]-points[whole][n])*part;
    return whole+1;
}
inline int BoltPoints(const Spawn &spawn,int age,Bolt &points) noexcept {
    if(!Jagged(spawn.origin,spawn.endpoint,spawn.chaos,spawn.seed,BoltSegments,points))return 0;
    return Truncate(points,BoltSegments,BoltReach(spawn,age));
}
// A branch leaves the main bolt at segment 5 or 10, about 35 degrees off it,
// for 0.4 of the remaining length; it grows once the bolt has passed it.
inline int BranchPoints(const Spawn &spawn,int age,int branch,Bolt &points,int &from) noexcept {
    from=0;
    if(!(spawn.flags&flag::UsePhysics)||branch<0||branch>=BoltBranches)return 0;
    Bolt main;if(!Jagged(spawn.origin,spawn.endpoint,spawn.chaos,spawn.seed,BoltSegments,main))return 0;
    from=5+5*branch;
    const float reach=BoltReach(spawn,age);
    if(reach<=float(from))return 0;
    Vector direction;double squared=0;
    for(int n=0;n<3;++n){direction[n]=spawn.endpoint[n]-main[from][n];squared+=double(direction[n])*direction[n];}
    Axis basis;if(squared<1e-6||!Basis(direction,basis))return 0;
    std::uint32_t seed=spawn.seed^(0x9e3779b9u*std::uint32_t(branch+1));
    const float turn=Pick({0,6.2831853f},seed);
    const float length=static_cast<float>(std::sqrt(squared))*0.4f;
    Vector end;
    for(int n=0;n<3;++n)end[n]=main[from][n]+length*(basis[0][n]*0.819152f+
        (basis[1][n]*std::cos(turn)+basis[2][n]*std::sin(turn))*0.573576f);
    if(!Jagged(main[from],end,spawn.chaos,seed,BranchSegments,points))return 0;
    return Truncate(points,BranchSegments,(reach-float(from))/float(BoltSegments-from)*BranchSegments);
}
inline float BoltWidth(const Spawn &spawn,float radius,int point) noexcept {
    return spawn.flags&flag::UseModel?radius*(1.0f-float(point)/BoltSegments):radius;
}
// Branches start at 0.7 of the bolt's width there and always narrow away.
inline float BranchWidth(const Spawn &spawn,float radius,int from,int point) noexcept {
    return BoltWidth(spawn,radius,from)*0.7f*(1.0f-float(point)/BranchSegments);
}
// Fn's polygon path supplies the material and clipping. Construct a stable
// camera-facing span, including a camera exactly on the line's axis.
inline bool LineSide(const Vector &start,const Vector &end,const Vector &eye,Vector &side) noexcept {
    if(!Finite(start)||!Finite(end)||!Finite(eye))return false;
    std::array<double,3> direction{},toward{},cross{};
    double length=0;
    for(int n=0;n<3;++n){direction[n]=double(end[n])-start[n];toward[n]=double(eye[n])-start[n];length+=direction[n]*direction[n];}
    if(length<1e-12)return false;
    const auto perpendicular=[&]{
        cross={toward[1]*direction[2]-toward[2]*direction[1],toward[2]*direction[0]-toward[0]*direction[2],toward[0]*direction[1]-toward[1]*direction[0]};
        return cross[0]*cross[0]+cross[1]*cross[1]+cross[2]*cross[2];
    };
    double squared=perpendicular();
    if(squared<1e-12){
        int least=0;for(int n=1;n<3;++n)if(std::fabs(direction[n])<std::fabs(direction[least]))least=n;
        toward={};toward[least]=1;squared=perpendicular();
    }
    const double magnitude=std::sqrt(squared);
    for(int n=0;n<3;++n)side[n]=static_cast<float>(cross[n]/magnitude);
    return Finite(side);
}
} // namespace fntech3::jka::effectplay
#endif
