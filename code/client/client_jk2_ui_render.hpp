/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JK2_UI_RENDER_HPP
#define FNTECH3_CLIENT_JK2_UI_RENDER_HPP
#include "../compat/jk2_render_adapter.hpp"
#include <array>
#include <cstdint>
#include <limits>

static void CL_JK2UIRenderName(intptr_t address, char (&name)[MAX_QPATH]) {
    for (std::size_t n = 0; n < sizeof(name); ++n) {
        if (std::uintptr_t(address) > (std::numeric_limits<std::uintptr_t>::max)() - n)
            Com_Error(ERR_DROP, "Invalid Jedi Outcast UI renderer name address");
        name[n] = *static_cast<const char *>(CL_JK2UISpan(std::uintptr_t(address) + n, 1));
        if (!name[n]) return;
    }
    Com_Error(ERR_DROP, "Jedi Outcast UI renderer name exceeds MAX_QPATH");
}
static int CL_JK2UIRenderInt(intptr_t value) {
    if (value < INT_MIN || value > INT_MAX) Com_Error(ERR_DROP, "Invalid Jedi Outcast UI renderer scalar");
    return static_cast<int>(value);
}
static float CL_JK2UIRenderFloat(intptr_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    float result; std::memcpy(&result, &bits, sizeof(result));
    if (!fntech3::jk2::render::Finite(result)) Com_Error(ERR_DROP, "Invalid Jedi Outcast UI renderer float");
    return result;
}

// Keep failure details bounded and stack-only: Com_Error may longjmp. These
// identify the failed field without publishing an invalid record to Fn.
static const char *CL_JK2UIEntityInvalidField(const fntech3_jk2_mp_refEntity_t &source) {
    using fntech3::jk2::render::Finite;
    if (source.nonNormalizedAxes != 0 && source.nonNormalizedAxes != 1) return "nonNormalizedAxes";
    if (!Finite(source.origin)) return "origin";
    if (!Finite(source.oldorigin)) return "oldorigin";
    if (!Finite(source.lightingOrigin)) return "lightingOrigin";
    if (!Finite(source.shaderTexCoord)) return "shaderTexCoord";
    if (!Finite(source.shadowPlane)) return "shadowPlane";
    if (!Finite(source.shaderTime)) return "shaderTime";
    if (!Finite(source.backlerp) || source.backlerp < 0 || source.backlerp > 1) return "backlerp";
    for (const auto &axis : source.axis) if (!Finite(axis)) return "axis";
    return "none";
}
static const char *CL_JK2UIViewInvalidField(const fntech3_jk2_mp_refdef_t &source) {
    using fntech3::jk2::render::Finite;
    if (source.width <= 0 || source.height <= 0) return "viewport size";
    if (source.x > INT_MAX - source.width || source.y > INT_MAX - source.height) return "viewport extent";
    if (!Finite(source.fov_x) || source.fov_x <= 0 || source.fov_x >= 180) return "fov_x";
    if (!Finite(source.fov_y) || source.fov_y <= 0 || source.fov_y >= 180) return "fov_y";
    if (!Finite(source.vieworg)) return "vieworg";
    for (const auto &axis : source.viewaxis) if (!Finite(axis)) return "viewaxis";
    for (const auto &text : source.text) if (!std::memchr(text, 0, sizeof(text))) return "text";
    return "none";
}
static void CL_JK2UIValidateViewport(const refdef_t &view, int framebufferHeight) {
    // Fn RE_RenderScene inverts top-origin y to GL coordinates, and the
    // backend also adds each viewport dimension. Check each signed result
    // after menu scaling, without clipping or altering authored placement.
    const std::int64_t right = std::int64_t(view.x) + view.width;
    const std::int64_t bottom = std::int64_t(view.y) + view.height;
    const std::int64_t glTop = std::int64_t(framebufferHeight) - view.y;
    const std::int64_t glBottom = std::int64_t(framebufferHeight) - bottom;
    if (view.width <= 0 || view.height <= 0 || framebufferHeight <= 0 ||
        right < INT_MIN || right > INT_MAX || bottom < INT_MIN || bottom > INT_MAX ||
        glTop < INT_MIN || glTop > INT_MAX || glBottom < INT_MIN || glBottom > INT_MAX)
        Com_Error(ERR_DROP, "Jedi Outcast UI view25 invalid derived viewport (rect=%d,%d %dx%d framebufferHeight=%d)",
            view.x, view.y, view.width, view.height, framebufferHeight);
}

/* Retail UI18..25 at0x40fe0e..0x40fedf select the renderer's model, skin,
 * scene, entity, polygon, light and view callbacks. UI29 at0x40ff61 stages
 * six tag arguments and discards its return value; UI62 at0x40ff2b writes
 * two model bounds. Records cross independently declared Outcast PODs.
 * Fn owns geometry, shaders, materials, lighting and the menu viewport. */
static bool CL_JK2UIRenderCall(intptr_t *args, intptr_t &result) {
    using namespace fntech3::jk2;
    switch (args[0]) {
    case 18: case 19: case 21: case 22: case 23: case 24: case 25: case 29: case 62: break;
    default: return false;
    }
    if (!Com_GameIsJK2MP() || !uivm) Com_Error(ERR_DROP, "Jedi Outcast UI renderer requires its selected module");
    result = 0;
    switch (args[0]) {
    case 18:
    case 19: {
        char name[MAX_QPATH]{};
        CL_JK2UIRenderName(args[1], name);
        result = args[0] == 18 ? re.RegisterModel(name) : re.RegisterSkin(name);
        return true;
    }
    case 21:
        re.ClearScene();
        return true;
    case 22: {
        fntech3_jk2_mp_refEntity_t source;
        std::memcpy(&source, CL_JK2UISpan(args[1], sizeof(source)), sizeof(source));
        refEntity_t entity{};
        const auto status = render::Entity(source, entity);
        if (status != render::Status::Ok)
            Com_Error(ERR_DROP, "Jedi Outcast UI entity22: %s (field=%s type=%d flags=0x%x axes=%d frame=%d/%d backlerp=%g)",
                render::Description(status), CL_JK2UIEntityInvalidField(source), source.reType,
                static_cast<unsigned>(source.renderfx), source.nonNormalizedAxes, source.frame, source.oldframe,
                static_cast<double>(source.backlerp));
        re.AddRefEntityToScene(&entity, qfalse);
        return true;
    }
    case 23: {
        const int shader = CL_JK2UIRenderInt(args[1]);
        // Existing Fn polygon queue capacity, not a retail ABI constant. Static
        // scratch avoids a large stack frame or heap owners across Com_Error.
        static std::array<polyVert_t, 32768> vertices;
        if (args[2] < 3 || std::uintptr_t(args[2]) > vertices.size())
            Com_Error(ERR_DROP, "Invalid Jedi Outcast UI polygon size");
        const auto count = static_cast<std::size_t>(args[2]);
        const auto *source = static_cast<const unsigned char *>(CL_JK2UISpan(args[3], count * sizeof(fntech3_jk2_mp_polyVert_t)));
        for (std::size_t n = 0; n < count; ++n)
            if (!render::PolyVertex(source + n * sizeof(fntech3_jk2_mp_polyVert_t), vertices[n]))
                Com_Error(ERR_DROP, "Invalid Jedi Outcast UI polygon vertex");
        re.AddPolyToScene(shader, static_cast<int>(count), vertices.data(), 1);
        return true;
    }
    case 24: {
        vec3_t origin;
        std::memcpy(origin, CL_JK2UISpan(args[1], sizeof(origin)), sizeof(origin));
        if (!render::Finite(origin)) Com_Error(ERR_DROP, "Invalid Jedi Outcast UI light origin");
        float light[4];
        for (unsigned n = 0; n < 4; ++n) light[n] = CL_JK2UIRenderFloat(args[n + 2]);
        re.AddLightToScene(origin, light[0], light[1], light[2], light[3]);
        return true;
    }
    case 25: {
        fntech3_jk2_mp_refdef_t source;
        std::memcpy(&source, CL_JK2UISpan(args[1], sizeof(source)), sizeof(source));
        refdef_t view{};
        const auto status = render::View(source, view);
        if (status != render::Status::Ok)
            Com_Error(ERR_DROP, "Jedi Outcast UI view25: %s (field=%s rect=%d,%d %dx%d fov=%g,%g flags=0x%x)",
                render::Description(status), CL_JK2UIViewInvalidField(source), source.x, source.y, source.width, source.height,
                static_cast<double>(source.fov_x), static_cast<double>(source.fov_y), static_cast<unsigned>(source.rdflags));
        view.rdflags |= RDF_NOFOVCORRECTION;
        CL_UIAdjustVirtualRefdef(&view);
        CL_JK2UIValidateViewport(view, cls.glconfig.vidHeight);
        re.RenderScene(&view);
        return true;
    }
    case 29: {
        void *destination = CL_JK2UISpan(args[1], sizeof(fntech3_jk2_mp_orientation_t));
        const int model = CL_JK2UIRenderInt(args[2]);
        const int first = CL_JK2UIRenderInt(args[3]), last = CL_JK2UIRenderInt(args[4]);
        if (first < 0 || last < 0) Com_Error(ERR_DROP, "Invalid Jedi Outcast UI tag frame");
        const float fraction = CL_JK2UIRenderFloat(args[5]);
        char name[MAX_QPATH]{}; CL_JK2UIRenderName(args[6], name);
        orientation_t internal{};
        re.LerpTag(&internal, model, first, last, fraction, name);
        if (!render::Finite(internal.origin)) Com_Error(ERR_DROP, "Invalid Jedi Outcast UI renderer tag output");
        for (const auto &axis : internal.axis)
            if (!render::Finite(axis)) Com_Error(ERR_DROP, "Invalid Jedi Outcast UI renderer tag output");
        fntech3_jk2_mp_orientation_t external{};
        render::Copy(external.origin, internal.origin); render::Copy(external.axis, internal.axis);
        std::memcpy(destination, &external, sizeof(external));
        return true;
    }
    case 62: {
        const int model = CL_JK2UIRenderInt(args[1]);
        void *minimum = CL_JK2UISpan(args[2], sizeof(vec3_t));
        void *maximum = CL_JK2UISpan(args[3], sizeof(vec3_t));
        vec3_t mins{}, maxs{};
        re.ModelBounds(model, mins, maxs);
        if (!render::Finite(mins) || !render::Finite(maxs)) Com_Error(ERR_DROP, "Invalid Jedi Outcast UI model bounds output");
        std::memcpy(minimum, mins, sizeof(mins)); std::memcpy(maximum, maxs, sizeof(maxs));
        return true;
    }
    }
    return false;
}
#endif
