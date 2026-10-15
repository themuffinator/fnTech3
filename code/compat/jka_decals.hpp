/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_DECALS_HPP
#define FNTECH3_JKA_DECALS_HPP
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

/* Academy's world decals, independently authored from retail jamp.exe 1.01.
 * The effects Decal case (0x40ac7e) and client-game import 205 both call the
 * renderer's AddDecalToScene (0x4ab6c0, the refexport slot the engine copies
 * to 0x8af084): it builds a square of the given radius about the origin,
 * turned by the orientation about the normal, presses it 20 units along the
 * negated direction onto the world through R_MarkFragments (0x4a6cf0), and
 * keeps each fragment in a pool of r_markcount records (0x4ab5b0). A record
 * pushed out of the pool is copied to a second pool and fades over a second
 * (0x4ab530); every world scene draws both (0x4abb20, called by RE_RenderScene
 * 0x4abf32 unless RDF_NOWORLDMODEL). Only the arithmetic and the pool live
 * here: Fn's own mark clipper cuts the fragments and Fn's polygon path draws
 * them. See docs/fntech3/2026-10-15-jka-decal.md. */
namespace fntech3::jka::decals {
using Vector=std::array<float,3>;
using Axis=std::array<Vector,3>;
constexpr int MaxPolys=500;       // 0x1f4 records of each type (0x4ab602).
constexpr int MaxVerts=10;        // A fragment's points beyond ten are dropped (0x4ab9a0).
constexpr int MaxFragments=128;   // 0x4ab860
constexpr int MaxPoints=384;      // 0x4ab871
constexpr int FadeTime=1000;      // 0x4ab530 and 0x4abbbc, ms.
constexpr float Projection=-20.0f; // 0x56655c times the caller's direction.
// r_markcount, registered by retail's R_Init (0x4a24fe) with CVAR_ARCHIVE.
constexpr const char *CountName="r_markcount",*CountDefault="100";
enum Type { Normal, Fade, Types };
struct Vertex { float xyz[3]{},st[2]{};std::uint8_t rgba[4]{}; };
// Retail's 0x118-byte record: time, fade time, shader, colour, its vertex
// count at +0x20 and its vertices at +0x28. A time of zero marks it free.
struct Decal {
    int time{},fadeTime{},shader{};
    float color[4]{};
    int numVerts{};
    Vertex verts[MaxVerts]{};
};
inline bool Finite(float value) noexcept {
    std::uint32_t bits;std::memcpy(&bits,&value,4);volatile std::uint32_t word=bits;
    return (word&0x7f800000u)!=0x7f800000u;
}
inline bool Finite(const Vector &value) noexcept { return Finite(value[0])&&Finite(value[1])&&Finite(value[2]); }
// Fn bound: retail indexes its 500 records with r_markcount unchecked, so a
// larger count would run past them. Fn draws at most 500 of each type.
inline int Limit(int count) noexcept { return count<0?0:count>MaxPolys?MaxPolys:count; }
// The square pressed onto the world (0x4ab70b..0x4ab8ef): the normal, a
// perpendicular turned by the orientation about it, and their cross product;
// corners at the origin -/+ radius along the second and third rows.
struct Square {
    Axis axis{};
    std::array<Vector,4> corners{};
    Vector projection{};
    float scale{};
};
inline Vector Cross(const Vector &a,const Vector &b) noexcept {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
// Fn's rotation about a unit axis (Rodrigues), turning by the right-hand rule.
inline Vector Turn(const Vector &axis,const Vector &point,float degrees) noexcept {
    const double radians=double(degrees)*3.14159265358979323846/180.0,c=std::cos(radians),s=std::sin(radians);
    const double dot=double(axis[0])*point[0]+double(axis[1])*point[1]+double(axis[2])*point[2];
    const Vector cross=Cross(axis,point);Vector result;
    for(int n=0;n<3;++n)result[n]=static_cast<float>(point[n]*c+cross[n]*s+axis[n]*dot*(1.0-c));
    return result;
}
// A unit vector perpendicular to a unit normal: the world axis the normal
// leans along least, with its share along the normal removed.
inline Vector Perpendicular(const Vector &normal) noexcept {
    int least=0;float smallest=1.0f;
    for(int n=0;n<3;++n)if(std::fabs(normal[n])<smallest){least=n;smallest=std::fabs(normal[n]);}
    Vector result{};result[least]=1;
    const float along=normal[least];
    double squared=0;
    for(int n=0;n<3;++n){result[n]-=along*normal[n];squared+=double(result[n])*result[n];}
    const double length=std::sqrt(squared);
    for(float &value:result)value=static_cast<float>(value/length);
    return result;
}
// False for a direction that has no length, or values Fn will not press: a
// radius at or below zero is a retail fatal error (0x4ab6f9), and Fn bounds
// it, the orientation and the origin as the effects adapter does.
inline bool Press(const Vector &origin,const Vector &direction,float orientation,float radius,Square &square) noexcept {
    if(!Finite(origin)||!Finite(direction)||!Finite(orientation)||!Finite(radius))return false;
    if(!(radius>0)||radius>16384.0f||std::fabs(orientation)>36000.0f)return false;
    for(float value:origin)if(std::fabs(value)>10000000.0f)return false;
    double squared=0;for(float value:direction)squared+=double(value)*value;
    if(!(squared>1e-12)||!(squared<1e24))return false;
    Square value;
    const double length=std::sqrt(squared);
    for(int n=0;n<3;++n)value.axis[0][n]=static_cast<float>(direction[n]/length);
    value.axis[1]=Perpendicular(value.axis[0]);
    value.axis[2]=Turn(value.axis[0],value.axis[1],orientation);
    value.axis[1]=Cross(value.axis[0],value.axis[2]);
    for(int n=0;n<3;++n){
        const float side=radius*value.axis[1][n],up=radius*value.axis[2][n];
        value.corners[0][n]=origin[n]-side-up;
        value.corners[1][n]=origin[n]+side-up;
        value.corners[2][n]=origin[n]+side+up;
        value.corners[3][n]=origin[n]-side+up;
        // The projection scales the caller's direction, not the normal.
        value.projection[n]=direction[n]*Projection;
    }
    // texCoordScale: the double 0.5 over the radius, kept as a float (0x4ab78e).
    value.scale=static_cast<float>(0.5/double(radius));
    for(const auto &corner:value.corners)if(!Finite(corner))return false;
    if(!Finite(value.projection)||!Finite(value.scale))return false;
    square=value;return true;
}
// Each fragment point keeps its place; its texture coordinates centre the
// square at one half (0x4ab9d0..0x4aba65).
inline bool Place(const Square &square,const Vector &origin,const Vector &point,const std::uint8_t (&rgba)[4],Vertex &vertex) noexcept {
    if(!Finite(point))return false;
    Vector delta;for(int n=0;n<3;++n){delta[n]=point[n]-origin[n];vertex.xyz[n]=point[n];}
    for(int row=0;row<2;++row){
        const auto &axis=square.axis[row+1];
        const double dot=double(delta[0])*axis[0]+double(delta[1])*axis[1]+double(delta[2])*axis[2];
        vertex.st[row]=static_cast<float>(dot*square.scale+0.5);
        if(!Finite(vertex.st[row]))return false;
    }
    std::memcpy(vertex.rgba,rgba,4);
    return true;
}
// Each component times 255, truncated (_ftol 0x539584 at 0x4ab92b..0x4ab975).
// Fn clamps to a byte where retail would wrap.
inline std::uint8_t Byte(float value) noexcept {
    const float scaled=value*255.0f;
    if(!(scaled>0))return 0;
    if(scaled>=255.0f)return 255;
    return static_cast<std::uint8_t>(static_cast<int>(scaled));
}
// A faded record's alpha t ms into its second (0x4abbc8..0x4abbf7):
// (1 - t / 1000) * 255, truncated. Fn clamps a time before its start.
inline std::uint8_t FadeAlpha(int elapsed) noexcept {
    volatile float time=static_cast<float>(elapsed);
    volatile float fade=(1.0f-time/1000.0f)*255.0f;
    const float value=fade;
    if(!(value>0))return 0;
    if(value>=255.0f)return 255;
    return static_cast<std::uint8_t>(static_cast<int>(value));
}
class Pool {
public:
    // RE_ClearDecals 0x4ab500: both types, heads and totals.
    void Clear() noexcept {
        for(auto &type:polys)for(auto &decal:type)decal=Decal{};
        heads={};totals={};
    }
    int Total(int type) const noexcept { return totals[type]; }
    int Head(int type) const noexcept { return heads[type]; }
    const Decal &At(int type,int index) const noexcept { return polys[type][index]; }
    // RE_FreeDecal 0x4ab530: a normal record is copied to the fade pool,
    // where its second starts now.
    void Free(int type,int index,int now,int limit) noexcept {
        if(!polys[type][index].time)return;
        if(type==Normal){
            Decal &fade=Allocate(Fade,now,limit);
            fade=polys[type][index];
            fade.time=now;fade.fadeTime=now+FadeTime;
        }
        polys[type][index].time=0;
        --totals[type];
    }
    // RE_AllocDecal 0x4ab5b0. It always succeeds: a pool over the count is
    // cleared whole, and a record still in use is pushed out, taking with it
    // the records after it made in the same scene as it.
    Decal &Allocate(int type,int now,int limit) noexcept {
        limit=Limit(limit);if(limit<1)limit=1;
        if(totals[type]>limit)Clear();
        if(heads[type]>=limit)heads[type]=0; // Fn bound; retail indexes on.
        Decal &record=polys[type][heads[type]];
        if(record.time){
            if(record.time!=now){
                const int group=record.time;int index=heads[type];
                for(int step=0;step<MaxPolys;++step){
                    if(++index>=limit)index=0;
                    if(index==heads[type]||polys[type][index].time!=group)break;
                    Free(type,index,now,limit);
                }
            }
            Free(type,heads[type],now,limit);
        }
        record=Decal{};record.time=now;
        ++totals[type];
        if(++heads[type]>=limit)heads[type]=0;
        return record;
    }
    // R_AddDecals 0x4abb20: a changed count clears every record; each type is
    // walked from its head, fading records drawn with the fading alpha until
    // their second is over and then freed.
    template<class Draw> void Visit(int now,int count,Draw &&draw) {
        const int limit=Limit(count);
        if(limit!=last){if(last!=-1)Clear();last=limit;}
        if(limit<=0)return;
        for(int type=Normal;type<Types;++type){
            int index=heads[type];if(index>=limit)index=0;
            for(int step=0;step<limit;++step){
                Decal &decal=polys[type][index];
                if(decal.time){
                    if(decal.fadeTime){
                        const std::int64_t elapsed=std::int64_t(now)-decal.time;
                        if(elapsed<FadeTime){
                            const std::uint8_t alpha=FadeAlpha(static_cast<int>(elapsed<INT32_MIN?INT32_MIN:elapsed));
                            for(int n=0;n<decal.numVerts;++n)decal.verts[n].rgba[3]=alpha;
                            draw(static_cast<const Decal &>(decal));
                        }else Free(type,index,now,limit);
                    }else draw(static_cast<const Decal &>(decal));
                }
                if(++index>=limit)index=0;
            }
        }
    }
private:
    std::array<std::array<Decal,MaxPolys>,Types> polys{};
    std::array<int,Types> heads{},totals{};
    int last{-1};
};
} // namespace fntech3::jka::decals
#endif
