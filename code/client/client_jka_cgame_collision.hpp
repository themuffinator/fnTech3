/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_COLLISION_HPP
#define FNTECH3_CLIENT_JKA_CGAME_COLLISION_HPP
// Include after client_jka_cgame_ghoul2.hpp: its store, resolver and skins.
#include "../ghoul2/ghoul2_instance_collision.hpp"
#include <array>
#include <string>

namespace {
/* Each mesh at each level of detail a collision test has used, read once from
 * the client game's store's files; a new store starts it over. */
fntech3::ghoul2::collision::GeometryCache jkaCgameCollisionGeometry;

/* Client-game imports 299 and 300 (jamp.exe 1.01 0x4139ce and 0x413a36):
 * the game's own 606 and 607 with the client's vertex heap (0x12a4fa0), so
 * the same routines answer -- G2API_CollisionDetect (0x4e0170) and its
 * cached twin (0x4dfe00) -- and server_jka_trace.hpp's SV_JKACollide tells
 * the rest. The client's models carry surface overrides (import 321 and a
 * skin's visibility), which the walk honours as the renderer does. */
static bool CL_JKACgameCollisionCall(intptr_t *args, fntech3::jka::module::Call call, intptr_t &result) {
    using fntech3::jka::module::Call;
    if (call == Call::CG_G2_GETSURFACENAME) {
        // 343 (0x4140ec) is the game's 629 (server_jka_game.hpp): the name of
        // the surface a record names, copied whole into the game's buffer.
        CL_JKACgameGhoul2Native();
        result = 0;
        const auto *instance = CL_JKACgameGhoul2Resolve(static_cast<std::uint32_t>(args[1]));
        if (!instance) Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 surface name was handed no instance");
        const int modelIndex = static_cast<int>(args[3]);
        if (modelIndex < 0 || modelIndex >= instance->Count())
            Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 surface name: model %d is outside its instance", modelIndex);
        std::string name;
        if (const auto *model = instance->At(modelIndex)) {
            const int surface = static_cast<int>(args[2]);
            if (surface < 0 || surface >= model->mesh.numSurfaces)
                Com_Printf("Ghoul2 surface name: %s has no surface %d\n", model->name.c_str(), surface);
            name = fntech3::ghoul2::SurfaceNameAt(*model, surface);
        }
        std::memcpy(CL_JKACgameSpan(args[4], name.size() + 1), name.c_str(), name.size() + 1);
        return true;
    }
    if (call != Call::CG_G2_COLLISIONDETECT && call != Call::CG_G2_COLLISIONDETECTCACHE) return false;
    namespace collision = fntech3::ghoul2::collision;
    CL_JKACgameGhoul2Native();
    result = 0;
    void *records = CL_JKACgameSpan(args[1], sizeof(collision::Records));
    const auto identity = static_cast<std::uint32_t>(args[2]);
    // Retail's setup (0x4e0c50) reads through the instance: a null one faults.
    if (!identity) Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 collision was handed no instance");
    const auto *instance = CL_JKACgameGhoul2Resolve(identity);
    std::array<std::array<float,3>,5> vectors{};
    const int vectorArgs[5] = {3, 4, 7, 8, 9}; // angles, position, start, end, scale
    for (int n = 0; n < 5; ++n) {
        std::memcpy(vectors[n].data(), CL_JKACgameSpan(args[vectorArgs[n]], sizeof(vectors[n])), sizeof(vectors[n]));
        for (float component : vectors[n])
            if (!fntech3::ghoul2::pose::Finite(component))
                Com_Error(ERR_DROP, "Invalid Jedi Academy cgame Ghoul2 collision vector");
    }
    const float radius = _vmf(args[12]);
    if (!fntech3::ghoul2::pose::Finite(radius))
        Com_Error(ERR_DROP, "Invalid Jedi Academy cgame Ghoul2 collision radius");
    // A setup that finds no model returns at once, the records untouched.
    if (!instance->HoldsAny()) return true;
    const auto overrides = [identity](int index) -> const collision::SurfaceOverrides * {
        if (!jkaCgameSkins) return nullptr;
        const auto found = jkaCgameSkins->bindings.find(std::make_pair(identity, index));
        return found == jkaCgameSkins->bindings.end() || found->second.overrides.empty()
            ? nullptr : &found->second.overrides;
    };
    const int entityNum = static_cast<int>(args[6]);
    collision::Prepared prepared;
    if (!collision::Prepare(CL_JKACgameGhoul2Store(), *instance, static_cast<int>(args[5]),
        static_cast<int>(args[11]), jkaCgameCollisionGeometry, overrides, prepared)) {
        const auto *model = instance->At(prepared.failedModel);
        if (prepared.readiness == collision::Readiness::Geometry)
            Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 collision: %s: %s", model ? model->name.c_str() : "?",
                fntech3::ghoul2::geometry::ErrorName(prepared.error));
        if (prepared.readiness == collision::Readiness::NoMesh)
            Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 collision: model %d of entity %d has no mesh to trace",
                prepared.failedModel, entityNum);
        Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 collision: model %d of entity %d is %s",
            prepared.failedModel, entityNum, fntech3::ghoul2::PosingName(prepared.posing));
    }
    collision::Query query;
    query.entityNum = entityNum;
    query.radius = radius;
    query.angles = vectors[0];
    query.origin = vectors[1];
    query.start = vectors[2];
    query.end = vectors[3];
    query.scale = vectors[4];
    collision::Records kept;
    collision::ReadRecords(records, kept);
    collision::Error error = collision::Error::None;
    collision::Collide(prepared.models, query, kept, &error);
    if (error != collision::Error::None)
        Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 collision: entity %d has a mesh it cannot trace", entityNum);
    collision::WriteRecords(records, kept);
    return true;
}
} // namespace
#endif
