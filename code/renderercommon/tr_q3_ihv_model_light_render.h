/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_MODEL_LIGHT_RENDER_H
#define FNTECH3_TR_Q3_IHV_MODEL_LIGHT_RENDER_H

static void R_SetupQ3IHVEntityLighting(const trRefdef_t *refdef, trRefEntity_t *ent) {
    float color[3] = {255, 255, 255};
    int i;
    if (!(refdef->rdflags & RDF_NOWORLDMODEL) && tr.world && tr.world->q3IHVModelLight) {
        q3IHVModelLightTrace_t trace;
        Q3IHV_LightTraceInit(ent->e.origin, &trace);
        if (Q3IHV_LightTrace(tr.world->q3IHVModelLight, &trace)) memcpy(color, trace.color, sizeof(color));
    }
    for (i = 0; i < refdef->num_dlights; ++i) {
        const dlight_t *light = refdef->dlights + i;
        Q3IHV_LightAddDynamic(ent->e.origin, light->origin, light->color, light->radius, color);
    }
    Q3IHV_LightFinish(ent->e.q3IHV.flags, color);
    memcpy(ent->ambientLight, color, sizeof(color));
    memcpy(ent->directedLight, color, sizeof(color));
    for (i = 0; i < 3; ++i) {
        const double value = color[i];
        ((byte *)&ent->ambientLightInt)[i] = (byte)Q3IHV_LightInteger(&value);
    }
    ((byte *)&ent->ambientLightInt)[3] = 255;
    Q3IHV_LightDirection(ent->e.axis, ent->lightDir);
#ifdef USE_PMLIGHT
    memcpy(ent->shadowLightDir, ent->lightDir, sizeof(ent->lightDir));
#endif
}
#endif
