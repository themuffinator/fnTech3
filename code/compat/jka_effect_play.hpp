/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_EFFECT_PLAY_HPP
#define FNTECH3_JKA_EFFECT_PLAY_HPP
#include "jka_effects.hpp"
#include <array>

/* Independently authored declaration adapter. Retail constructor
 * 0x40c650..0x40ca7e pins life50ms, count1, delay/motion/flags0 and color/
 * alpha/size/length endpoints1. Named fields and flag meanings have separate
 * pinned OpenJK format observations. Fn owns geometry quality/presentation.
 * Visual families take one shader and no motion. Lines also serve traced
 * endpoints and a wave size curve. Emitters, Lights, FxRunners and
 * Electricity are measured from retail jamp.exe 1.01's CreateEffect
 * (0x407c40), its primitive classes and the scheduler's count rule; see
 * docs/fntech3/2026-10-15-jka-spawn-effect.md. No unsupported field or
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
    ExpensivePhysics=0x800000u,ImpactKills=0x40000000u,EmitFx=0x10000000u;
constexpr std::uint32_t Org2FromTrace=0x10u,Org2IsOffset=0x40u;
}
constexpr int MaxCount=100;           // Fn bound; retail has none.
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
inline bool Compile(const effects::Primitive &primitive,Spec &output,Failure &failure) {
    Spec spec;spec.kind=primitive.kind;
    const char *const name=primitive.declaration.name.c_str();
    if(!Served(spec.kind))return Refuse(failure,name,"effects primitive family is not served yet");
    const bool visual=Visual(spec.kind),segment=spec.kind==Kind::Line||spec.kind==Kind::Electricity;
    const bool emitter=spec.kind==Kind::Emitter,light=spec.kind==Kind::Light,runner=spec.kind==Kind::Runner;
    if(visual){
        if(primitive.media.size()!=1||primitive.media[0].kind!=effects::MediaKind::Shader||primitive.media[0].handle<0)
            return Refuse(failure,name,"effects playback requires one registered Fn shader");
        spec.shader=primitive.media[0].handle;
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
    // Electricity endpoint path (0x408f39..0x4093e2). Flags per class below.
    if(spec.spawnFlags&~(segment?flag::Org2FromTrace|flag::Org2IsOffset:0u))
        return Refuse(failure,name,"effects spawn controls are not served yet");
    if((spec.spawnFlags&flag::Org2IsOffset)&&!(spec.spawnFlags&flag::Org2FromTrace))
        return Refuse(failure,name,"effects untraced endpoint offsets are not served yet");
    spec.traceEnd=spec.spawnFlags&flag::Org2FromTrace;spec.offsetEnd=spec.spawnFlags&flag::Org2IsOffset;
    // Retail's "intensity" (and "bounce") parser ORs usePhysics into the
    // flags (0x40cdca); for Electricity that word means forked bolts.
    if(spec.kind==Kind::Electricity)for(const auto &number:primitive.numbers)
        if(effects::Lower(number.name)=="intensity")spec.flags|=flag::UsePhysics;
    const std::uint32_t allowed=spec.kind==Kind::Electricity?flag::UseModel|flag::UseBBox|flag::UsePhysics:
        emitter?flag::UsePhysics|flag::ExpensivePhysics|flag::ImpactKills|flag::EmitFx:0u;
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
        if(key=="origin"||key=="origin2"||key=="rgb/start"||key=="rgb/end"||key=="velocity"||key=="vel"){
            if(number.values.size()!=3&&number.values.size()!=6)return Refuse(failure,number.name.c_str(),"invalid effects vector range");
            const bool position=key=="origin"||key=="origin2",motion=key=="velocity"||key=="vel";
            // Traced offsets reach beyond the world, as retail's spawn bolts do.
            const float bound=key=="origin2"&&spec.offsetEnd?65536.0f:16384.0f;
            for(int n=0;n<3;++n){
                const Range range{static_cast<float>(number.values[n]),static_cast<float>(number.values[number.values.size()==6?n+3:n])};
                if(!Bounded(range,position||motion?-bound:0.0f,position||motion?bound:1.0f))
                    return Refuse(failure,number.name.c_str(),"effects vector range exceeds the Fn bound");
                if(key=="origin")spec.origin[n]=range;
                else if(key=="origin2")spec.endpoint[n]=range;
                else if(motion)spec.velocity[n]=range;
                else if(key=="rgb/start")spec.color[n]=range;
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
            if(!Finite(value.first)||value.first!=0||value.last!=0)
                return Refuse(failure,number.name.c_str(),"effects spawn delay is not served yet");
            continue;
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
            if(key=="alpha/start")spec.color[3]=value;continue;
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
inline bool Instantiate(const Spec &spec,const Vector &origin,const Axis &axis,std::uint32_t &random,Spawn &output) noexcept {
    if(spec.shader<0||!Served(spec.kind)||!Bounded(spec.life,1,60000))return false;
    if(!Finite(origin))return false;for(const auto &row:axis)if(!Finite(row))return false;
    Spawn spawn;spawn.kind=spec.kind;spawn.shader=spec.shader;spawn.axis=axis;spawn.origin=origin;spawn.flags=spec.flags;
    // Retail truncates the sampled lifetime (_ftol 0x539584).
    spawn.life=static_cast<int>(Pick(spec.life,random));if(spawn.life<1)spawn.life=1;
    for(int n=0;n<3;++n){const float offset=Pick(spec.origin[n],random);for(int component=0;component<3;++component)spawn.origin[component]+=axis[n][component]*offset;}
    for(int n=0;n<4;++n){const float value=Pick(spec.color[n],random);if(!Finite(value)||value<0||value>1)return false;
        if(n<3)spawn.light[n]=value;
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
