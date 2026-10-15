/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_DECALS_HPP
#define FNTECH3_CLIENT_JKA_CGAME_DECALS_HPP
#include "../compat/jka_decals.hpp"
#include <array>
#include <cstring>
#include <memory>

/* Academy's world decals. Retail keeps them in its renderer (jamp.exe
 * 0x4ab5b0..0x4abc87); Fn keeps the same pool in the client, beside the
 * effects that make them, and presents them through its own systems: Fn's
 * mark clipper (re.MarkFragments) cuts each square against the world, and
 * Fn's polygon path draws the fragments with their shader's stages, fog and
 * lighting before every world scene, as retail adds them inside
 * RE_RenderScene (0x4abf32). The pool lives until the client game shuts down
 * or asks for it to be cleared (import 201), as retail's renderer pool lives
 * until the renderer restarts. */
namespace {
struct JKACgameDecals {
    fntech3::jka::decals::Pool pool;
    cvar_t *count{};
    // RE_RenderScene's refdef time (0x4abdfa): every decal made before the
    // next world scene takes the time of the last one.
    int sceneTime{};
    std::array<markFragment_t,fntech3::jka::decals::MaxFragments> fragments{};
    vec3_t points[fntech3::jka::decals::MaxPoints]{};
    polyVert_t verts[fntech3::jka::decals::MaxVerts]{};
};
std::unique_ptr<JKACgameDecals> jkaCgameDecals;
static void CL_JKACgameDecalsClear() { jkaCgameDecals.reset(); }
static JKACgameDecals &CL_JKACgameDecalsState() {
    if (!jkaCgameDecals) jkaCgameDecals=std::make_unique<JKACgameDecals>();
    auto &state=*jkaCgameDecals;
    if (!state.count) state.count=Cvar_Get(fntech3::jka::decals::CountName,fntech3::jka::decals::CountDefault,CVAR_ARCHIVE);
    return state;
}
// AddDecalToScene (0x4ab6c0) as CreateEffect's Decal case calls it (alpha
// fade on, never temporary): nothing while r_markcount is zero or less;
// otherwise each fragment Fn's clipper cuts becomes a kept record of at most
// ten points, coloured by the truncated bytes of the colour. Returns the
// number of fragments kept.
static int CL_JKACgameDecalAdd(qhandle_t shader,const fntech3::jka::decals::Vector &origin,
    const fntech3::jka::decals::Vector &direction,float orientation,const float (&color)[4],float radius)
{
    namespace decals=fntech3::jka::decals;
    auto &state=CL_JKACgameDecalsState();
    const int limit=decals::Limit(state.count->integer);
    if (limit<=0) return 0;
    decals::Square square;
    // Fn skips a square it cannot build (a direction without length); retail
    // would build it from not-a-number values and cut nothing.
    if (!decals::Press(origin,direction,orientation,radius,square)) return 0;
    if (!re.MarkFragments) Com_Error(ERR_DROP,"Academy decal mark service is unavailable");
    vec3_t corners[4],projection;
    for (int corner=0;corner<4;++corner) std::memcpy(corners[corner],square.corners[corner].data(),sizeof(vec3_t));
    std::memcpy(projection,square.projection.data(),sizeof(projection));
    const int count=re.MarkFragments(4,corners,projection,decals::MaxPoints,state.points[0],
        decals::MaxFragments,state.fragments.data());
    if (count<0 || count>decals::MaxFragments) Com_Error(ERR_DROP,"Invalid Academy decal fragment count");
    const std::uint8_t rgba[4]{decals::Byte(color[0]),decals::Byte(color[1]),decals::Byte(color[2]),decals::Byte(color[3])};
    for (int index=0;index<count;++index) {
        const auto &fragment=state.fragments[index];
        if (fragment.firstPoint<0 || fragment.numPoints<0 || fragment.firstPoint>decals::MaxPoints ||
            fragment.numPoints>decals::MaxPoints-fragment.firstPoint)
            Com_Error(ERR_DROP,"Invalid Academy decal fragment");
        const int points=fragment.numPoints>decals::MaxVerts?decals::MaxVerts:fragment.numPoints;
        decals::Vertex verts[decals::MaxVerts];
        for (int point=0;point<points;++point) {
            const float *xyz=state.points[fragment.firstPoint+point];
            if (!decals::Place(square,origin,{xyz[0],xyz[1],xyz[2]},rgba,verts[point]))
                Com_Error(ERR_DROP,"Invalid Academy decal fragment point");
        }
        auto &decal=state.pool.Allocate(decals::Normal,state.sceneTime,limit);
        decal.time=state.sceneTime;decal.shader=shader;decal.numVerts=points;
        std::memcpy(decal.color,color,sizeof(decal.color));
        std::memcpy(decal.verts,verts,sizeof(verts[0])*points);
    }
    return count;
}
// R_AddDecals (0x4abb20), for a world scene about to render at the given
// time: every kept record goes to Fn's polygon path.
static void CL_JKACgameDecalsDraw(int time) {
    namespace decals=fntech3::jka::decals;
    auto &state=CL_JKACgameDecalsState();
    state.sceneTime=time;
    if (!re.AddPolyToScene) Com_Error(ERR_DROP,"Academy decal polygon service is unavailable");
    state.pool.Visit(time,state.count->integer,[&](const decals::Decal &decal){
        if (decal.numVerts<3) return; // A fragment of fewer points has no area.
        for (int n=0;n<decal.numVerts;++n) {
            std::memcpy(state.verts[n].xyz,decal.verts[n].xyz,sizeof(state.verts[n].xyz));
            std::memcpy(state.verts[n].st,decal.verts[n].st,sizeof(state.verts[n].st));
            std::memcpy(state.verts[n].modulate.rgba,decal.verts[n].rgba,4);
        }
        re.AddPolyToScene(decal.shader,decal.numVerts,state.verts,1);
    });
}
} // namespace
#endif
