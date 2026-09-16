"""Synthetic collision services shared by the RTCW client/server boundary tests."""

SERVICES = r'''
static trace_t collisionResult;
static int collisionCalls, collisionFailure;
static const float *collisionStart, *collisionEnd, *collisionMins, *collisionMaxs;
static const float *collisionOrigin, *collisionAngles;
static int collisionTarget, collisionMask;
static qboolean collisionCapsule;
static bool collisionTransformed;
static void CollisionBackend(trace_t *result, const float *start, const float *end,
        const float *mins, const float *maxs, int target, int mask, qboolean capsule) {
    assert(start == collisionStart && end == collisionEnd);
    assert(mins == collisionMins && maxs == collisionMaxs);
    assert(target == collisionTarget && mask == collisionMask && capsule == collisionCapsule);
    ++collisionCalls;
    *result = collisionResult;
    if (collisionFailure == 1) throw 17;
    if (collisionFailure == 2) {
        const uint32_t invalid = 2;
        std::memcpy(&result->allsolid, &invalid, sizeof(invalid));
    }
}
void CM_BoxTrace(trace_t *out, const vec3_t start, const vec3_t end,
        const vec3_t mins, const vec3_t maxs, clipHandle_t target, int mask, qboolean capsule) {
    assert(!collisionTransformed);
    CollisionBackend(out, start, end, mins, maxs, target, mask, capsule);
}
void CM_TransformedBoxTrace(trace_t *out, const vec3_t start, const vec3_t end,
        const vec3_t mins, const vec3_t maxs, clipHandle_t target, int mask,
        const vec3_t origin, const vec3_t angles, qboolean capsule) {
    assert(collisionTransformed && origin == collisionOrigin && angles == collisionAngles);
    CollisionBackend(out, start, end, mins, maxs, target, mask, capsule);
}
void SV_Trace(trace_t *out, const vec3_t start, const vec3_t mins, const vec3_t maxs,
        const vec3_t end, int target, int mask, qboolean capsule) {
    assert(!collisionTransformed);
    CollisionBackend(out, start, end, mins, maxs, target, mask, capsule);
}
'''


CHECKS = r'''
static void CheckRTCWCollisionImports(intptr_t (*dispatch)(intptr_t *), bool server) {
    using namespace fntech3::rtcw::module;
    const auto id = Com_GameProfile()->id;
    const Call clientCalls[] = {Call::CG_CM_BOXTRACE, Call::CG_CM_CAPSULETRACE,
        Call::CG_CM_TRANSFORMEDBOXTRACE, Call::CG_CM_TRANSFORMEDCAPSULETRACE};
    const Call gameCalls[] = {Call::G_TRACE, Call::G_TRACECAPSULE};
    for (unsigned callIndex = 0; callIndex < (server ? 2u : 4u); ++callIndex) {
        const Call call = server ? gameCalls[callIndex] : clientCalls[callIndex];
        collisionCapsule = callIndex % 2 ? qtrue : qfalse;
        collisionTransformed = !server && callIndex >= 2;
        collisionTarget = server ? 127 : collisionCapsule ? 510 : 511;
        collisionMask = int32_t(0xa5a50080u);
        float start[]{1,2,3}, end[]{4,5,6}, mins[]{-1,-2,-3}, maxs[]{7,8,9};
        float origin[]{20,30,40}, angles[]{45,90,120};
        struct Record { uint32_t before; fntech3_rtcw_trace_t value; uint32_t after; } output;
        collisionResult = {};
        collisionResult.allsolid = qtrue; collisionResult.startsolid = qfalse;
        collisionResult.fraction = 0.375f;
        collisionResult.endpos[0] = 20; collisionResult.endpos[1] = 21; collisionResult.endpos[2] = 22;
        collisionResult.plane.normal[1] = 1; collisionResult.plane.dist = -18;
        collisionResult.plane.type = 1; collisionResult.plane.signbits = 4;
        collisionResult.plane.pad[0] = 0xab; collisionResult.plane.pad[1] = 0xcd;
        collisionResult.surfaceFlags = int32_t(0x80000040u);
        collisionResult.contents = int32_t(0x92000080u); collisionResult.entityNum = 317;
        const fntech3_rtcw_trace_t expected{1,0,0.375f,{20,21,22},
            {{0,1,0},-18,1,4,{0xab,0xcd}},int32_t(0x80000040u),int32_t(0x92000080u),317};
        // A transformed trace has 10 argument words; the ordinary trace has
        // exactly 8. ASan catches reads of absent origin/angle words.
        for (unsigned optional = 0; optional < 4; ++optional) {
            collisionMins = optional & 1 ? nullptr : mins;
            collisionMaxs = optional & 2 ? nullptr : maxs;
            collisionStart = start; collisionEnd = end;
            collisionOrigin = origin; collisionAngles = angles;
            intptr_t plain[]{Ordinal(id, call), (intptr_t)&output.value, (intptr_t)start,
                (intptr_t)(server ? collisionMins : end), (intptr_t)(server ? collisionMaxs : collisionMins),
                (intptr_t)(server ? end : collisionMaxs), collisionTarget, collisionMask};
            intptr_t transformed[]{plain[0], plain[1], plain[2], plain[3], plain[4], plain[5],
                plain[6], plain[7], (intptr_t)origin, (intptr_t)angles};
            intptr_t *args = collisionTransformed ? transformed : plain;
            for (int failure = 0; failure <= 2; ++failure) {
                std::memset(&output, 0x5a, sizeof(output)); const auto unchanged = output;
                collisionFailure = failure; collisionCalls = 0; bool threw = false;
                try { assert(dispatch(args) == 0); } catch(int code) {
                    threw = true; assert(code == 7 || code == 17);
                }
                assert(collisionCalls == 1 && threw == (failure != 0));
                assert(output.before == 0x5a5a5a5au && output.after == 0x5a5a5a5au);
                if (failure) assert(!std::memcmp(&output, &unchanged, sizeof(output)));
                else assert(!std::memcmp(&output.value, &expected, sizeof(expected)));
            }
            collisionFailure = 0;
            for (int missing : {1, 2, server ? 5 : 3, 8, 9}) {
                if (missing >= 8 && !collisionTransformed) continue;
                const auto saved = args[missing]; args[missing] = 0;
                std::memset(&output, 0x5a, sizeof(output)); const auto unchanged = output;
                collisionCalls = 0; bool threw = false;
                try { dispatch(args); } catch(int code) { assert(code == 7); threw = true; }
                assert(threw && !collisionCalls && !std::memcmp(&output, &unchanged, sizeof(output)));
                args[missing] = saved;
            }
            // Input may live in the output record. Publication follows the
            // backend read, so aliasing cannot corrupt the query inputs.
            collisionStart = output.value.endpos; args[2] = (intptr_t)collisionStart;
            std::memcpy(output.value.endpos, start, sizeof(start)); collisionCalls = 0;
            assert(dispatch(args) == 0 && collisionCalls == 1);
            assert(!std::memcmp(&output.value, &expected, sizeof(expected)));
        }
    }
}
'''
